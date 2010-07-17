/* vim:set foldmethod=marker:
 *
 * v8 interface to Vim
 *
 * Last Change: 2010-06-27
 * Maintainer: Yukihiro Nakadaira <yukihiro.nakadaira@gmail.com>
 */
#include <cstdio>
#include <sstream>
#include <string>
#include <map>
#include <v8.h>

#include "vimext.h"

/* API */
extern "C" {
DLLEXPORT const char *init(const char *args);
DLLEXPORT const char *execute(const char *expr);
}

using namespace v8;

typedef void* list_or_dict_ptr;
typedef std::map<list_or_dict_ptr, Handle<Value> > LookupMap;

static void *dll_handle = NULL;
static Handle<Context> context;
static Handle<FunctionTemplate> VimList;
static Handle<FunctionTemplate> VimDict;
static Handle<FunctionTemplate> VimFunc;

// ensure the following condition:
//   var x = new vim.Dict();
//   x.x = x;
//   x === x.x  => true
// the same List/Dictionary is instantiated by only one V8 object.
static LookupMap objcache;

// v:['%v8_weak%']
// keep reference to avoid garbage collect.
static dict_T *v_weak;

static const char *init_v8(std::string args);

static bool vim_to_v8(typval_T *vimobj, Handle<Value> *v8obj, int depth, LookupMap *lookup, bool wrap, std::string *err);
static bool v8_to_vim(Handle<Value> v8obj, typval_T *vimobj, int depth, LookupMap *lookup, std::string *err);
static LookupMap::iterator LookupFindValue(LookupMap* lookup, Handle<Value> v);

static void weak_ref(typval_T *tv);
static void weak_unref(typval_T *tv);

static Handle<String> ReadFile(const char* name);
static bool ExecuteString(Handle<String> source, Handle<Value> name, bool print_result, bool report_exceptions, std::string& err);
static void ReportException(TryCatch* try_catch);

// functions
static Handle<Value> vim_execute(const Arguments& args);
static Handle<Value> Load(const Arguments& args);

// VimList
static Handle<Value> MakeVimList(list_T *list);
static Handle<Value> VimListCreate(const Arguments& args);
static void VimListDestroy(Persistent<Value> object, void* parameter);
static Handle<Value> VimListGet(uint32_t index, const AccessorInfo& info);
static Handle<Value> VimListSet(uint32_t index, Local<Value> value, const AccessorInfo& info);
static Handle<Boolean> VimListQuery(uint32_t index, const AccessorInfo& info);
static Handle<Boolean> VimListDelete(uint32_t index, const AccessorInfo& info);
static Handle<Array> VimListEnumerate(const AccessorInfo& info);
static Handle<Value> VimListLength(Local<String> property, const AccessorInfo& info);

// VimDict
static Handle<Value> MakeVimDict(dict_T *dict);
static void VimDictDestroy(Persistent<Value> object, void* parameter);
static Handle<Value> VimDictCreate(const Arguments& args);
static Handle<Value> VimDictIdxGet(uint32_t index, const AccessorInfo& info);
static Handle<Value> VimDictIdxSet(uint32_t index, Local<Value> value, const AccessorInfo& info);
static Handle<Boolean> VimDictIdxQuery(uint32_t index, const AccessorInfo& info);
static Handle<Boolean> VimDictIdxDelete(uint32_t index, const AccessorInfo& info);
static Handle<Value> VimDictGet(Local<String> property, const AccessorInfo& info);
static Handle<Value> VimDictSet(Local<String> property, Local<Value> value, const AccessorInfo& info);
static Handle<Integer> VimDictQuery(Local<String> property, const AccessorInfo& info);
static Handle<Boolean> VimDictDelete(Local<String> property, const AccessorInfo& info);
static Handle<Array> VimDictEnumerate(const AccessorInfo& info);

// VimFunc
static Handle<Value> MakeVimFunc(const char *name, Handle<Object> obj);
static void VimFuncDestroy(Persistent<Value> object, void* parameter);
static Handle<Value> VimFuncCall(const Arguments& args);

struct Trace {
  std::string name_;
  Trace(std::string name) {
    name_ = name;
    printf("%s: enter\n", name_.c_str());
  }
  ~Trace() {
    printf("%s: leave\n", name_.c_str());
  }
};

//#define DEBUG
#if defined(DEBUG)
# define TRACE(name) Trace trace__(name)
#else
# define TRACE(name)
#endif

/* args = dll_path,v8_args */
const char *
init(const char *_args)
{
  TRACE("init");
  const char *err;
  std::string args(_args);
  size_t pos = args.find(",", 0);
  std::string dll_path = args.substr(0, pos);
  std::string v8_args = args.substr(pos + 1);
  if (dll_handle != NULL)
    return NULL;
  if ((dll_handle = DLOPEN(dll_path.c_str())) == NULL)
    return "error: cannot load library";
  if ((err = init_vim()) != NULL)
    return err;
  if ((err = init_v8(v8_args)) != NULL)
    return err;
  return NULL;
}

const char *
execute(const char *expr)
{
  TRACE("execute");
  HandleScope handle_scope;
  Context::Scope context_scope(context);
  std::string err;
  if (!ExecuteString(String::New(expr), String::New("(command-line)"), true, true, err))
    emsg((char_u*)err.c_str());
  return NULL;
}

static const char *
init_v8(std::string args)
{
  TRACE("init_v8");

  V8::SetFlagsFromString(args.c_str(), args.length());

  HandleScope handle_scope;

  v_weak = dict_alloc();
  if (v_weak == NULL)
    return "init_v8(): error dict_alloc()";
  typval_T tv;
  tv_set_dict(&tv, v_weak);
  dict_set_tv_nocopy(&vimvardict, (char_u*)"%v8_weak%", &tv);

  VimList = Persistent<FunctionTemplate>::New(FunctionTemplate::New(VimListCreate));
  VimList->SetClassName(String::New("VimList"));
  Handle<ObjectTemplate> VimListTemplate = VimList->InstanceTemplate();
  VimListTemplate->SetInternalFieldCount(1);
  VimListTemplate->SetIndexedPropertyHandler(VimListGet, VimListSet, VimListQuery, VimListDelete, VimListEnumerate);
  VimListTemplate->SetAccessor(String::New("length"), VimListLength, NULL, Handle<Value>(), DEFAULT, (PropertyAttribute)(DontEnum|DontDelete));

  VimDict = Persistent<FunctionTemplate>::New(FunctionTemplate::New(VimDictCreate));
  VimDict->SetClassName(String::New("VimDict"));
  Handle<ObjectTemplate> VimDictTemplate = VimDict->InstanceTemplate();
  VimDictTemplate->SetInternalFieldCount(1);
  VimDictTemplate->SetIndexedPropertyHandler(VimDictIdxGet, VimDictIdxSet, VimDictIdxQuery, VimDictIdxDelete);
  VimDictTemplate->SetNamedPropertyHandler(VimDictGet, VimDictSet, VimDictQuery, VimDictDelete, VimDictEnumerate);

  VimFunc = Persistent<FunctionTemplate>::New(FunctionTemplate::New());
  VimFunc->SetClassName(String::New("VimFunc"));
  Handle<ObjectTemplate> VimFuncTemplate = VimFunc->InstanceTemplate();
  // [0]=funcname  [1]=self or undef
  VimFuncTemplate->SetInternalFieldCount(2);
  VimFuncTemplate->SetCallAsFunctionHandler(VimFuncCall);

  Handle<ObjectTemplate> vim = ObjectTemplate::New();
  vim->Set("execute", FunctionTemplate::New(vim_execute));
  vim->Set("List", VimList);
  vim->Set("Dict", VimDict);
  vim->Set("Func", VimFunc);

  Handle<ObjectTemplate> global = ObjectTemplate::New();
  global->Set("load", FunctionTemplate::New(Load));
  global->Set("vim", vim);

  context = Context::New(NULL, global);

  {
    Context::Scope context_scope(context);
    Handle<Object> obj = Handle<Object>::Cast(context->Global()->Get(String::New("vim")));
    obj->Set(String::New("g"), MakeVimDict(&globvardict));
    obj->Set(String::New("v"), MakeVimDict(&vimvardict));
  }

  return NULL;
}

static bool
vim_to_v8(typval_T *vimobj, Handle<Value> *v8obj, int depth, LookupMap *lookup, bool wrap, std::string *err)
{
  TRACE("vim_to_v8");
  if (depth > 100) {
    *err = "vim_to_v8(): too deep";
    return false;
  }

  if (vimobj->v_type == VAR_NUMBER) {
    *v8obj = Integer::New(vimobj->vval.v_number);
    return true;
  }

#ifdef FEAT_FLOAT
  if (vimobj->v_type == VAR_FLOAT) {
    *v8obj = Number::New(vimobj->vval.v_float);
    return true;
  }
#endif

  if (vimobj->v_type == VAR_STRING) {
    if (vimobj->vval.v_string == NULL)
      *v8obj = String::New("");
    else
      *v8obj = String::New((char *)vimobj->vval.v_string);
    return true;
  }

  if (vimobj->v_type == VAR_LIST) {
    list_T *list = vimobj->vval.v_list;
    if (list == NULL) {
      *v8obj = Array::New(0);
      return true;
    }
    LookupMap::iterator it = lookup->find(list);
    if (it != lookup->end()) {
      *v8obj = it->second;
      return true;
    }
    if (wrap) {
      *v8obj = MakeVimList(list);
      return true;
    }
    // copy by value
    Handle<Array> o = Array::New(0);
    Handle<Value> v;
    int i = 0;
    lookup->insert(LookupMap::value_type(list, o));
    for (listitem_T *curr = list->lv_first; curr != NULL; curr = curr->li_next) {
      if (!vim_to_v8(&curr->li_tv, &v, depth + 1, lookup, wrap, err))
        return false;
      o->Set(Integer::New(i++), v);
    }
    *v8obj = o;
    return true;
  }

  if (vimobj->v_type == VAR_DICT) {
    dict_T *dict = vimobj->vval.v_dict;
    if (dict == NULL) {
      *v8obj = Object::New();
      return true;
    }
    LookupMap::iterator it = lookup->find(dict);
    if (it != lookup->end()) {
      *v8obj = it->second;
      return true;
    }
    if (wrap) {
      *v8obj = MakeVimDict(dict);
      return true;
    }
    // copy by value
    Handle<Object> o = Object::New();
    Handle<Value> v;
    hashtab_T *ht = &dict->dv_hashtab;
    long_u todo = ht->ht_used;
    hashitem_T *hi;
    dictitem_T *di;
    lookup->insert(LookupMap::value_type(dict, o));
    for (hi = ht->ht_array; todo > 0; ++hi) {
      if (!HASHITEM_EMPTY(hi)) {
        --todo;
        di = HI2DI(hi);
        if (!vim_to_v8(&di->di_tv, &v, depth + 1, lookup, wrap, err))
          return false;
        o->Set(String::New((char *)hi->hi_key), v);
      }
    }
    *v8obj = o;
    return true;
  }

  if (vimobj->v_type == VAR_FUNC) {
    if (vimobj->vval.v_string == NULL)
      *v8obj = Undefined();
    else
      *v8obj = MakeVimFunc((char *)vimobj->vval.v_string, VimFunc->InstanceTemplate()->NewInstance());
    return true;
  }

  *err = "vim_to_v8(): unknown type";
  return false;
}

static bool
v8_to_vim(Handle<Value> v8obj, typval_T *vimobj, int depth, LookupMap *lookup, std::string *err)
{
  TRACE("v8_to_vim");
  if (depth > 100) {
    *err = "v8_to_vim(): too deep";
    return false;
  }

  if (VimList->HasInstance(v8obj)) {
    Handle<Object> o = Handle<Object>::Cast(v8obj);
    Handle<External> external = Handle<External>::Cast(o->GetInternalField(0));
    tv_set_list(vimobj, static_cast<list_T*>(external->Value()));
    return true;
  }

  if (VimDict->HasInstance(v8obj)) {
    Handle<Object> o = Handle<Object>::Cast(v8obj);
    Handle<External> external = Handle<External>::Cast(o->GetInternalField(0));
    tv_set_dict(vimobj, static_cast<dict_T*>(external->Value()));
    return true;
  }

  if (VimFunc->HasInstance(v8obj)) {
    Handle<Object> o = Handle<Object>::Cast(v8obj);
    Handle<External> external = Handle<External>::Cast(o->GetInternalField(0));
    tv_set_func(vimobj, static_cast<char_u*>(external->Value()));
    return true;
  }

  if (v8obj->IsUndefined()) {
    tv_set_number(vimobj, 0);
    return true;
  }

  if (v8obj->IsNull()) {
    tv_set_number(vimobj, 0);
    return true;
  }

  if (v8obj->IsBoolean()) {
    tv_set_number(vimobj, v8obj->IsTrue() ? 1 : 0);
    return true;
  }

  if (v8obj->IsInt32()) {
    tv_set_number(vimobj, v8obj->Int32Value());
    return true;
  }

#ifdef FEAT_FLOAT
  if (v8obj->IsNumber()) {
    tv_set_float(vimobj, v8obj->NumberValue());
    return true;
  }
#endif

  if (v8obj->IsString()) {
    tv_set_string(vimobj, (char_u*)(*String::Utf8Value(v8obj)));
    return true;
  }

  if (v8obj->IsDate()) {
    tv_set_string(vimobj, (char_u*)(*String::Utf8Value(v8obj)));
    return true;
  }

  if (v8obj->IsArray()) {
    LookupMap::iterator it = LookupFindValue(lookup, v8obj);
    if (it != lookup->end()) {
      tv_set_list(vimobj, (list_T *)it->first);
      return true;
    }
    list_T *list = list_alloc();
    if (list == NULL) {
      *err = "v8_to_vim(): list_alloc(): out of memoty";
      return false;
    }
    Handle<Array> o = Handle<Array>::Cast(v8obj);
    uint32_t len = o->Length();
    lookup->insert(LookupMap::value_type(list, v8obj));
    for (uint32_t i = 0; i < len; ++i) {
      Handle<Value> v = o->Get(Integer::New(i));
      typval_T tv;
      if (!v8_to_vim(v, &tv, depth + 1, lookup, err)) {
        list_free(list, TRUE);
        return false;
      }
      if (!list_append_tv_nocopy(list, &tv)) {
        clear_tv(&tv);
        list_free(list, TRUE);
        *err = "v8_to_vim(): list_append_tv_nocopy() error";
        return false;
      }
    }
    tv_set_list(vimobj, list);
    return true;
  }

  if (v8obj->IsObject()) {
    LookupMap::iterator it = LookupFindValue(lookup, v8obj);
    if (it != lookup->end()) {
      tv_set_dict(vimobj, (dict_T *)it->first);
      return true;
    }
    dict_T *dict = dict_alloc();
    if (dict == NULL) {
      *err = "v8_to_vim(): dict_alloc(): out of memory";
      return false;
    }
    Handle<Object> o = Handle<Object>::Cast(v8obj);
    Handle<Array> keys = o->GetPropertyNames();
    uint32_t len = keys->Length();
    lookup->insert(LookupMap::value_type(dict, v8obj));
    for (uint32_t i = 0; i < len; ++i) {
      Handle<Value> key = keys->Get(Integer::New(i));
      Handle<Value> v = o->Get(key);
      String::Utf8Value keystr(key);
      typval_T tv;
      if (keystr.length() == 0) {
        dict_free(dict, TRUE);
        *err = "v8_to_vim(): Cannot use empty key for Dictionary";
        return false;
      }
      if (!v8_to_vim(v, &tv, depth + 1, lookup, err)) {
        dict_free(dict, TRUE);
        return false;
      }
      if (!dict_set_tv_nocopy(dict, (char_u*)*keystr, &tv)) {
        clear_tv(&tv);
        dict_free(dict, TRUE);
        *err = "v8_to_vim(): error dict_set_tv_nocopy()";
        return false;
      }
    }
    tv_set_dict(vimobj, dict);
    return true;
  }

  if (v8obj->IsFunction()) {
    *err = "v8_to_vim(): cannot convert function";
    return false;
  }

  if (v8obj->IsExternal()) {
    *err = "v8_to_vim(): cannot convert native object";
    return false;
  }

  *err = "v8_to_vim(): unknown type";
  return false;
}

static LookupMap::iterator
LookupFindValue(LookupMap* lookup, Handle<Value> v)
{
  TRACE("LookupFindValue");
  LookupMap::iterator it;
  for (it = lookup->begin(); it != lookup->end(); ++it) {
    if (it->second == v)
      return it;
  }
  return it;
}

static void
weak_ref(typval_T *tv)
{
  TRACE("weak_ref");
  char buf[64];
  vim_snprintf(buf, sizeof(buf), (char*)"%p", tv);
  dict_set_tv_nocopy(v_weak, (char_u*)buf, tv);
}

static void
weak_unref(typval_T *tv)
{
  TRACE("weak_unref");
  char buf[64];
  hashitem_T *hi;
  dictitem_T *di;
  vim_snprintf(buf, sizeof(buf), (char*)"%p", tv);
  hi = hash_find(&v_weak->dv_hashtab, (char_u*)buf);
  if (HASHITEM_EMPTY(hi)) {
    emsg((char_u*)"if_v8: weak_unref(): internal error");
    return;
  }
  // XXX: save di because hash_remove() clear it.
  di = HI2DI(hi);
  hash_remove(&v_weak->dv_hashtab, hi);
  vim_free(di);
}

// Reads a file into a v8 string.
static Handle<String>
ReadFile(const char* name)
{
  TRACE("ReadFile");
  FILE* file = fopen(name, "rb");
  if (file == NULL) return Handle<String>();

  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = fread(&chars[i], 1, size - i, file);
    i += read;
  }
  fclose(file);
  Handle<String> result = String::New(chars, size);
  delete[] chars;
  return result;
}

static bool
ExecuteString(Handle<String> source, Handle<Value> name, bool print_result, bool report_exceptions, std::string& err)
{
  TRACE("ExecuteString");
  HandleScope handle_scope;
  TryCatch try_catch;
  Handle<Script> script = Script::Compile(source, name);
  if (script.IsEmpty()) {
    err = *(String::Utf8Value(try_catch.Exception()));
    if (report_exceptions)
      ReportException(&try_catch);
    return false;
  }
  Handle<Value> result = script->Run();
  if (result.IsEmpty()) {
    err = *(String::Utf8Value(try_catch.Exception()));
    if (report_exceptions)
      ReportException(&try_catch);
    return false;
  }
  if (print_result && !result->IsUndefined()) {
    typval_T tv;
    tv_set_string(&tv, (char_u*)(*String::Utf8Value(result)));
    dict_set_tv_nocopy(&vimvardict, (char_u*)"%v8_print%", &tv);
  }
  return true;
}

static void
ReportException(TryCatch* try_catch)
{
  TRACE("ReportException");
  HandleScope handle_scope;
  String::Utf8Value exception(try_catch->Exception());
  Handle<Message> message = try_catch->Message();
  std::ostringstream strm;
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    strm << *exception << "\n";
  } else {
    // Print (filename):(line number): (message).
    String::Utf8Value filename(message->GetScriptResourceName());
    int linenum = message->GetLineNumber();
    strm << *filename << ":" << linenum << ": " << *exception << "\n";
    // Print line of source code.
    String::Utf8Value sourceline(message->GetSourceLine());
    strm << *sourceline << "\n";
    // Print wavy underline (GetUnderline is deprecated).
    int start = message->GetStartColumn();
    for (int i = 0; i < start; i++) {
      strm << " ";
    }
    int end = message->GetEndColumn();
    for (int i = start; i < end; i++) {
      strm << "^";
    }
    strm << "\n";
  }
  typval_T tv;
  tv_set_string(&tv, (char_u*)strm.str().c_str());
  dict_set_tv_nocopy(&vimvardict, (char_u*)"%v8_errmsg%", &tv);
}

static Handle<Value>
vim_execute(const Arguments& args)
{
  TRACE("vim_execute");
  HandleScope handle_scope;

  if (args.Length() != 1 || !args[0]->IsString())
    return ThrowException(String::New("usage: vim_execute(string cmd"));

  String::Utf8Value cmd(args[0]);
  if (!do_cmdline_cmd((char_u*)*cmd))
    return ThrowException(String::New("vim_execute(): error do_cmdline_cmd()"));
  return Undefined();
}

// The callback that is invoked by v8 whenever the JavaScript 'load'
// function is called.  Loads, compiles and executes its argument
// JavaScript file.
static Handle<Value>
Load(const Arguments& args)
{
  TRACE("Load");
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope;
    String::Utf8Value file(args[i]);
    Handle<String> source = ReadFile(*file);
    if (source.IsEmpty()) {
      return ThrowException(String::New("Error loading file"));
    }
    std::string err;
    if (!ExecuteString(source, String::New(*file), false, false, err)) {
      return ThrowException(String::New(err.c_str()));
    }
  }
  return Undefined();
}

static list_T *makelistptr = NULL;

static Handle<Value>
MakeVimList(list_T *list)
{
  TRACE("MakeVimList");

  makelistptr = list;
  Handle<Object> self = VimList->InstanceTemplate()->NewInstance();
  makelistptr = NULL;

  return self;
}

static void
VimListDestroy(Persistent<Value> object, void* parameter)
{
  TRACE("VimListDestroy");
  typval_T *tv = (typval_T*)parameter;

  objcache.erase(tv->vval.v_list);

  weak_unref(tv);
  free_tv(tv);
}

static Handle<Value>
VimListCreate(const Arguments& args)
{
  TRACE("VimListCreate");
  if (!args.IsConstructCall())
    return ThrowException(String::New("Cannot call constructor as function"));

  Handle<Object> self = args.Holder();

  list_T *list;
  if (makelistptr != NULL) {
    list = makelistptr;
  } else {
    list = list_alloc();
    if (list == NULL)
      return ThrowException(String::New("VimListCreate(): list_alloc(): out of memory"));
  }

  self->SetInternalField(0, External::New(list));

  // increment Vim's reference count
  typval_T *tv = alloc_tv();
  tv_set_list(tv, list);
  weak_ref(tv);

  // make weak reference
  Persistent<Object> p = Persistent<Object>::New(self);
  p.MakeWeak(tv, VimListDestroy);
  objcache.insert(LookupMap::value_type(list, p));

  return self;
}

static Handle<Value>
VimListGet(uint32_t index, const AccessorInfo& info)
{
  TRACE("VimListGet");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  listitem_T *li = list_find(list, index);
  if (li == NULL)
    return Undefined();
  std::string err;
  Handle<Value> v8obj;
  if (!vim_to_v8(&li->li_tv, &v8obj, 1, &objcache, true, &err))
    return ThrowException(String::New(err.c_str()));
  return v8obj;
}

static Handle<Value>
VimListSet(uint32_t index, Local<Value> value, const AccessorInfo& info)
{
  TRACE("VimListSet");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  listitem_T *li = list_find(list, index);
  if (li == NULL)
    return ThrowException(String::New("list index out of range"));
  LookupMap lookup;
  std::string err;
  typval_T vimobj;
  if (!v8_to_vim(value, &vimobj, 1, &lookup, &err))
    return ThrowException(String::New(err.c_str()));
  clear_tv(&li->li_tv);
  li->li_tv = vimobj;
  return value;
}

static Handle<Boolean>
VimListQuery(uint32_t index, const AccessorInfo& info)
{
  TRACE("VimListQuery");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  listitem_T *li = list_find(list, index);
  if (li == NULL)
    return False();
  return True();
}

static Handle<Boolean>
VimListDelete(uint32_t index, const AccessorInfo& info)
{
  TRACE("VimListDelete");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  listitem_T *li = list_find(list, index);
  if (li == NULL)
    return False();
  list_remove(list, li, li);
  listitem_free(li);
  return True();
}

static Handle<Array>
VimListEnumerate(const AccessorInfo& info)
{
  TRACE("VimListEnumerate");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  uint32_t len = list_len(list);
  Handle<Array> keys = Array::New(len);
  for (uint32_t i = 0; i < len; ++i) {
    keys->Set(Integer::New(i), Integer::New(i));
  }
  return keys;
}

static Handle<Value>
VimListLength(Local<String> property, const AccessorInfo& info)
{
  TRACE("VimListLength");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  uint32_t len = list_len(list);
  return Integer::New(len);
}

static dict_T *makedictptr = NULL;

static Handle<Value>
MakeVimDict(dict_T *dict)
{
  TRACE("MakeVimDict");

  makedictptr = dict;
  Handle<Object> self = VimDict->InstanceTemplate()->NewInstance();
  makedictptr = NULL;

  return self;
}

static void
VimDictDestroy(Persistent<Value> object, void* parameter)
{
  TRACE("VimDictDestroy");
  typval_T *tv = (typval_T*)parameter;

  objcache.erase(tv->vval.v_dict);

  weak_unref(tv);
  free_tv(tv);
}

static Handle<Value>
VimDictCreate(const Arguments& args)
{
  TRACE("VimDictCreate");
  if (!args.IsConstructCall())
    return ThrowException(String::New("Cannot call constructor as function"));

  Handle<Object> self = args.Holder();

  dict_T *dict;
  if (makedictptr != NULL) {
    dict = makedictptr;
  } else {
    dict = dict_alloc();
    if (dict == NULL)
      return ThrowException(String::New("VimDictCreate(): dict_alloc(): out of memory"));
  }

  self->SetInternalField(0, External::New(dict));

  // increment Vim's reference count
  typval_T *tv = alloc_tv();
  tv_set_dict(tv, dict);
  weak_ref(tv);

  // make weak reference
  Persistent<Object> p = Persistent<Object>::New(self);
  p.MakeWeak(tv, VimDictDestroy);
  objcache.insert(LookupMap::value_type(dict, p));

  return self;
}

static Handle<Value>
VimDictIdxGet(uint32_t index, const AccessorInfo& info)
{
  TRACE("VimDictIdxGet");
  return VimDictGet(Integer::New(index)->ToString(), info);
}

static Handle<Value>
VimDictIdxSet(uint32_t index, Local<Value> value, const AccessorInfo& info)
{
  TRACE("VimDictIdxSet");
  return VimDictSet(Integer::New(index)->ToString(), value, info);
}

static Handle<Boolean>
VimDictIdxQuery(uint32_t index, const AccessorInfo& info)
{
  TRACE("VimDictIdxQuery");
  if (VimDictQuery(Integer::New(index)->ToString(), info).IsEmpty())
      return False();
  return True();
}

static Handle<Boolean>
VimDictIdxDelete(uint32_t index, const AccessorInfo& info)
{
  TRACE("VimDictIdxDelete");
  return VimDictDelete(Integer::New(index)->ToString(), info);
}

static Handle<Value>
VimDictGet(Local<String> property, const AccessorInfo& info)
{
  TRACE("VimDictIdxGet");
  if (property->Length() == 0)
    return ThrowException(String::New("Cannot use empty key for Dictionary"));
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  dict_T *dict = static_cast<dict_T*>(external->Value());
  String::Utf8Value key(property);
  dictitem_T *di = dict_find(dict, (char_u*)*key, -1);
  if (di == NULL) {
    // fallback to prototype.  otherwise String(obj) don't work due to
    // lack of toString().
    Handle<Object> prototype = Handle<Object>::Cast(self->GetPrototype());
    return prototype->Get(property);
  }
  std::string err;
  Handle<Value> v8obj;
  if (!vim_to_v8(&di->di_tv, &v8obj, 1, &objcache, true, &err))
    return ThrowException(String::New(err.c_str()));
  // XXX: When obj.func(), args.Holder() and args.This() are VimFunc
  // insted of obj.  Use internal field for now.
  if (VimFunc->HasInstance(v8obj)) {
    Handle<Object> func = Handle<Object>::Cast(v8obj);
    func->SetInternalField(1, self);
  }
  return v8obj;
}

static Handle<Value>
VimDictSet(Local<String> property, Local<Value> value, const AccessorInfo& info)
{
  TRACE("VimDictSet");
  if (property->Length() == 0)
    return ThrowException(String::New("Cannot use empty key for Dictionary"));
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  dict_T *dict = static_cast<dict_T*>(external->Value());
  String::Utf8Value key(property);
  LookupMap lookup;
  std::string err;
  typval_T vimobj;
  if (!v8_to_vim(value, &vimobj, 1, &lookup, &err))
    return ThrowException(String::New(err.c_str()));
  if (!dict_set_tv_nocopy(dict, (char_u*)*key, &vimobj)) {
    clear_tv(&vimobj);
    return ThrowException(String::New("error dict_set_tv_nocopy()"));
  }
  return value;
}

static Handle<Integer>
VimDictQuery(Local<String> property, const AccessorInfo& info)
{
  TRACE("VimDictQuery");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  dict_T *dict = static_cast<dict_T*>(external->Value());
  String::Utf8Value key(property);
  dictitem_T *di = dict_find(dict, (char_u*)*key, -1);
  if (di == NULL)
    return Handle<Integer>();
  return Integer::New(None);
}

static Handle<Boolean>
VimDictDelete(Local<String> property, const AccessorInfo& info)
{
  TRACE("VimDictDelete");
  if (property->Length() == 0)
    return False();
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  dict_T *dict = static_cast<dict_T*>(external->Value());
  String::Utf8Value key(property);
  dictitem_T *di = dict_find(dict, (char_u*)*key, -1);
  if (di == NULL)
    return False();
  dictitem_remove(dict, di);
  return True();
}

static Handle<Array>
VimDictEnumerate(const AccessorInfo& info)
{
  TRACE("VimDictEnumerate");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  dict_T *dict = static_cast<dict_T*>(external->Value());
  hashtab_T *ht = &dict->dv_hashtab;
  long_u todo = ht->ht_used;
  hashitem_T *hi;
  int i = 0;
  Handle<Array> keys = Array::New(todo);
  for (hi = ht->ht_array; todo > 0; ++hi) {
    if (!HASHITEM_EMPTY(hi)) {
      --todo;
      keys->Set(Integer::New(i++), String::New((char *)hi->hi_key));
    }
  }
  return keys;
}

static Handle<Value>
MakeVimFunc(const char *name, Handle<Object> obj)
{
  TRACE("MakeVimFunc");
  typval_T *tv = alloc_tv();
  tv_set_func(tv, (char_u*)name);
  weak_ref(tv);

  Persistent<Object> self = Persistent<Object>::New(obj);
  self.MakeWeak(tv, VimFuncDestroy);
  self->SetInternalField(0, External::New(tv->vval.v_string));
  self->SetInternalField(1, Undefined());

  return self;
}

static void
VimFuncDestroy(Persistent<Value> object, void* parameter)
{
  TRACE("VimFuncDestroy");
  typval_T *tv = (typval_T*)parameter;

  weak_unref(tv);
  free_tv(tv);
}

static Handle<Value>
VimFuncCall(const Arguments& args)
{
  TRACE("VimFuncCall");
  if (args.IsConstructCall())
    return ThrowException(String::New("Cannot create VimFunc"));

  Handle<Object> self = args.Holder();

  Handle<Array> arr = Array::New(args.Length());
  for (int i = 0; i < args.Length(); ++i)
    arr->Set(Integer::New(i), args[i]);

  Handle<Value> obj = self->GetInternalField(1);

  Handle<Value> callargs[3] = {self, arr, obj};

  // return vim.call(name, args, obj)
  Handle<Object> vim = Handle<Object>::Cast(context->Global()->Get(String::New("vim")));
  Handle<Function> call = Handle<Function>::Cast(vim->Get(String::New("call")));
  return call->Call(vim, 3, callargs);
}

