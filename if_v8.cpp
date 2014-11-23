/* vim:set foldmethod=marker:
 *
 * v8 interface to Vim
 *
 * Last Change: 2014-11-02
 * Maintainer: Yukihiro Nakadaira <yukihiro.nakadaira@gmail.com>
 */
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <v8.h>
#include <libplatform/libplatform.h>

#include "vimext.h"

/* API */
extern "C" {
DLLEXPORT const char *init(const char *args);
DLLEXPORT const char *execute(const char *expr);
}

using namespace v8;

template<typename T, typename U>
class PairTable {
public:
  typedef std::vector<std::pair<T, U> > container_type;
  typedef typename container_type::value_type value_type;
  typedef typename container_type::iterator iterator;

  PairTable() {}
  iterator begin() { return _container.begin(); }
  iterator end() { return _container.end(); }

  iterator get(const T& key) {
    for (iterator it = _container.begin(); it != _container.end(); ++it) {
      if (it->first == key)
        return it;
    }
    return end();
  }

  void set(const T& key, const U& value) {
    iterator it = get(key);
    if (it != end())
      it->second = value;
    else
      _container.push_back(value_type(key, value));
  }

  void del(const T& key) {
    iterator it = get(key);
    if (it != end())
      _container.erase(it);
  }

private:
  container_type _container;
};

struct VimValue {
  VimValue(char_u *val) { v_type = VAR_FUNC; vval.v_string = val; }
  VimValue(list_T *val) { v_type = VAR_LIST; vval.v_list = val; }
  VimValue(dict_T *val) { v_type = VAR_DICT; vval.v_dict = val; }
  bool operator==(const VimValue& other) const {
    if (v_type != other.v_type)
      return false;
    else if (v_type == VAR_FUNC) {
      if (vval.v_string != NULL && other.vval.v_string != NULL)
        return strcmp((char*)vval.v_string, (char*)other.vval.v_string) == 0;
      return vval.v_string == other.vval.v_string;
    } else if (v_type == VAR_LIST)
      return vval.v_list == other.vval.v_list;
    else if (v_type == VAR_DICT)
      return vval.v_dict == other.vval.v_dict;
    return false;
  }
  char v_type;
  union {
    char_u *v_string;
    list_T *v_list;
    dict_T *v_dict;
  } vval;
};

typedef Persistent<Value, CopyablePersistentTraits<Value> > CopyableValuePersistent;

typedef PairTable<Handle<Value>, VimValue> V8ToVimLookup;
typedef PairTable<VimValue, CopyableValuePersistent> VimToV8Lookup;

static void *dll_handle = NULL;
static Isolate *isolate;
static Persistent<Context> p_context;
static Persistent<FunctionTemplate> p_VimList;
static Persistent<FunctionTemplate> p_VimDict;
static Persistent<FunctionTemplate> p_VimFunc;

// ensure the following condition:
//   var x = new vim.Dict();
//   x.x = x;
//   x === x.x  => true
// the same List/Dictionary is instantiated by only one V8 object.
static VimToV8Lookup objcache;

// register
static dict_T *v_reg;

// reg['%v8_weak%']
// keep reference to avoid garbage collect.
static dict_T *v_weak;

static const char *init_v8(std::string args);

static bool vim_to_v8(typval_T *vimobj, Handle<Value> *v8obj, int depth, VimToV8Lookup *lookup, std::string *err);
static bool v8_to_vim(Handle<Value> v8obj, typval_T *vimobj, int depth, V8ToVimLookup *lookup, std::string *err);

static void weak_ref(typval_T *tv);
static void weak_unref(typval_T *tv);

static Handle<String> ReadFile(const char* name);
static bool ExecuteString(Handle<String> source, Handle<Value> name, bool print_result, bool report_exceptions, std::string& err);
static void ReportException(TryCatch* try_catch);

// functions
static void vim_execute(const FunctionCallbackInfo<Value>& args);
static void Load(const FunctionCallbackInfo<Value>& args);

// VimList
static Handle<Value> MakeVimList(list_T *list);
static void VimListCreate(const FunctionCallbackInfo<Value>& args);
static void VimListDestroy(const WeakCallbackData<Value, typval_T>& data);
static void VimListGet(uint32_t index, const PropertyCallbackInfo<Value>& info);
static void VimListSet(uint32_t index, Local<Value> value, const PropertyCallbackInfo<Value>& info);
static void VimListQuery(uint32_t index, const PropertyCallbackInfo<Integer>& info);
static void VimListDelete(uint32_t index, const PropertyCallbackInfo<Boolean>& info);
static void VimListEnumerate(const PropertyCallbackInfo<Array>& info);
static void VimListLength(Local<String> property, const PropertyCallbackInfo<Value>& info);

// VimDict
static Handle<Value> MakeVimDict(dict_T *dict);
static void VimDictDestroy(const WeakCallbackData<Value, typval_T>& data);
static void VimDictCreate(const FunctionCallbackInfo<Value>& args);
static void VimDictIdxGet(uint32_t index, const PropertyCallbackInfo<Value>& info);
static void VimDictIdxSet(uint32_t index, Local<Value> value, const PropertyCallbackInfo<Value>& info);
static void VimDictIdxQuery(uint32_t index, const PropertyCallbackInfo<Integer>& info);
static void VimDictIdxDelete(uint32_t index, const PropertyCallbackInfo<Boolean>& info);
static void VimDictGet(Local<String> property, const PropertyCallbackInfo<Value>& info);
static void VimDictSet(Local<String> property, Local<Value> value, const PropertyCallbackInfo<Value>& info);
static void VimDictQuery(Local<String> property, const PropertyCallbackInfo<Integer>& info);
static void VimDictDelete(Local<String> property, const PropertyCallbackInfo<Boolean>& info);
static void VimDictEnumerate(const PropertyCallbackInfo<Array>& info);

// VimFunc
static Handle<Value> MakeVimFunc(const char *name);
static void VimFuncDestroy(const WeakCallbackData<Value, typval_T>& data);
static void VimFuncCall(const FunctionCallbackInfo<Value>& args);

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
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  Context::Scope context_scope(Local<Context>::New(isolate, p_context));
  std::string err;
  if (!ExecuteString(String::NewFromUtf8(isolate, expr), String::NewFromUtf8(isolate, "(command-line)"), true, true, err))
    emsg((char_u*)err.c_str());
  return NULL;
}

static const char *
init_v8(std::string args)
{
  TRACE("init_v8");

  V8::InitializeICU();
  Platform* platform = v8::platform::CreateDefaultPlatform();
  V8::InitializePlatform(platform);
  V8::Initialize();
  V8::SetFlagsFromString(args.c_str(), args.length());

  isolate = Isolate::New();

  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);

  char expr[] = "g:__if_v8";
  typval_T *ptv;
  ptv = eval_expr((char_u *)expr, NULL);
  if (ptv == NULL)
      return "init_v8(): cannot get register variable";
  v_reg = ptv->vval.v_dict;
  free_tv(ptv);

  v_weak = dict_alloc();
  if (v_weak == NULL)
    return "init_v8(): error dict_alloc()";
  typval_T tv;
  tv_set_dict(&tv, v_weak);
  dict_set_tv_nocopy(v_reg, (char_u*)"%v8_weak%", &tv);

  p_VimList.Reset(isolate, FunctionTemplate::New(isolate, VimListCreate));
  Local<FunctionTemplate> VimList = Local<FunctionTemplate>::New(isolate, p_VimList);
  VimList->SetClassName(String::NewFromUtf8(isolate, "VimList"));
  Handle<ObjectTemplate> VimListTemplate = VimList->InstanceTemplate();
  VimListTemplate->SetInternalFieldCount(1);
  VimListTemplate->SetIndexedPropertyHandler(VimListGet, VimListSet, VimListQuery, VimListDelete, VimListEnumerate);
  VimListTemplate->SetAccessor(String::NewFromUtf8(isolate, "length"), VimListLength, NULL, Handle<Value>(), DEFAULT, (PropertyAttribute)(DontEnum|DontDelete));

  p_VimDict.Reset(isolate, FunctionTemplate::New(isolate, VimDictCreate));
  Local<FunctionTemplate> VimDict = Local<FunctionTemplate>::New(isolate, p_VimDict);
  VimDict->SetClassName(String::NewFromUtf8(isolate, "VimDict"));
  Handle<ObjectTemplate> VimDictTemplate = VimDict->InstanceTemplate();
  VimDictTemplate->SetInternalFieldCount(1);
  VimDictTemplate->SetIndexedPropertyHandler(VimDictIdxGet, VimDictIdxSet, VimDictIdxQuery, VimDictIdxDelete);
  VimDictTemplate->SetNamedPropertyHandler(VimDictGet, VimDictSet, VimDictQuery, VimDictDelete, VimDictEnumerate);

  p_VimFunc.Reset(isolate, FunctionTemplate::New(isolate));
  Local<FunctionTemplate> VimFunc = Local<FunctionTemplate>::New(isolate, p_VimFunc);
  VimFunc->SetClassName(String::NewFromUtf8(isolate, "VimFunc"));
  Handle<ObjectTemplate> VimFuncTemplate = VimFunc->InstanceTemplate();
  // [0]=funcname  [1]=self or undef
  VimFuncTemplate->SetInternalFieldCount(2);
  VimFuncTemplate->SetCallAsFunctionHandler(VimFuncCall);

  Handle<ObjectTemplate> vim = ObjectTemplate::New();
  vim->Set(String::NewFromUtf8(isolate, "execute"), FunctionTemplate::New(isolate, vim_execute));
  vim->Set(String::NewFromUtf8(isolate, "List"), VimList);
  vim->Set(String::NewFromUtf8(isolate, "Dict"), VimDict);
  vim->Set(String::NewFromUtf8(isolate, "Func"), VimFunc);

  Handle<ObjectTemplate> global = ObjectTemplate::New();
  global->Set(String::NewFromUtf8(isolate, "load"), FunctionTemplate::New(isolate, Load));
  global->Set(String::NewFromUtf8(isolate, "vim"), vim);

  p_context.Reset(isolate, Context::New(isolate, NULL, global));

  {
    Local<Context> context = Local<Context>::New(isolate, p_context);
    Context::Scope context_scope(context);
    Handle<Object> obj = Handle<Object>::Cast(context->Global()->Get(String::NewFromUtf8(isolate, "vim")));
    obj->Set(String::NewFromUtf8(isolate, "g"), MakeVimDict(&globvardict));
    obj->Set(String::NewFromUtf8(isolate, "v"), MakeVimDict(&vimvardict));
  }

  return NULL;
}

static bool
vim_to_v8(typval_T *vimobj, Handle<Value> *v8obj, int depth, VimToV8Lookup *lookup, std::string *err)
{
  TRACE("vim_to_v8");
  if (depth > 100) {
    *err = "vim_to_v8(): too deep";
    return false;
  }

  if (vimobj->v_type == VAR_NUMBER) {
    *v8obj = Integer::New(isolate, vimobj->vval.v_number);
    return true;
  }

#ifdef FEAT_FLOAT
  if (vimobj->v_type == VAR_FLOAT) {
    *v8obj = Number::New(isolate, vimobj->vval.v_float);
    return true;
  }
#endif

  if (vimobj->v_type == VAR_STRING) {
    if (vimobj->vval.v_string == NULL)
      *v8obj = String::NewFromUtf8(isolate, "");
    else
      *v8obj = String::NewFromUtf8(isolate, (char *)vimobj->vval.v_string);
    return true;
  }

  if (vimobj->v_type == VAR_LIST) {
    list_T *list = vimobj->vval.v_list;
    if (list == NULL) {
      *v8obj = Array::New(0);
      return true;
    }
    VimToV8Lookup::iterator it = lookup->get(VimValue(list));
    if (it != lookup->end()) {
      *v8obj = Local<Value>::New(isolate, it->second);
      return true;
    }
#if 1
    *v8obj = MakeVimList(list);
    return true;
#else
    // copy by value
    Handle<Array> o = Array::New(0);
    Handle<Value> v;
    int i = 0;
    lookup->set(VimValue(list), o);
    for (listitem_T *curr = list->lv_first; curr != NULL; curr = curr->li_next) {
      if (!vim_to_v8(&curr->li_tv, &v, depth + 1, lookup, err))
        return false;
      o->Set(Integer::New(isolate, i++), v);
    }
    *v8obj = o;
    return true;
#endif
  }

  if (vimobj->v_type == VAR_DICT) {
    dict_T *dict = vimobj->vval.v_dict;
    if (dict == NULL) {
      *v8obj = Object::New(isolate);
      return true;
    }
    VimToV8Lookup::iterator it = lookup->get(VimValue(dict));
    if (it != lookup->end()) {
      *v8obj = Local<Value>::New(isolate, it->second);
      return true;
    }
#if 1
    *v8obj = MakeVimDict(dict);
    return true;
#else
    // copy by value
    Handle<Object> o = Object::New(isolate);
    Handle<Value> v;
    hashtab_T *ht = &dict->dv_hashtab;
    long_u todo = ht->ht_used;
    hashitem_T *hi;
    dictitem_T *di;
    lookup->set(VimValue(dict), o);
    for (hi = ht->ht_array; todo > 0; ++hi) {
      if (!HASHITEM_EMPTY(hi)) {
        --todo;
        di = HI2DI(hi);
        if (!vim_to_v8(&di->di_tv, &v, depth + 1, lookup, err))
          return false;
        o->Set(String::NewFromUtf8(isolate, (char *)hi->hi_key), v);
      }
    }
    *v8obj = o;
    return true;
#endif
  }

  if (vimobj->v_type == VAR_FUNC) {
    VimToV8Lookup::iterator it = lookup->get(VimValue(vimobj->vval.v_string));
    if (it != lookup->end()) {
      *v8obj = Local<Value>::New(isolate, it->second);
      return true;
    }
    *v8obj = MakeVimFunc((char *)vimobj->vval.v_string);
    return true;
  }

  *err = "vim_to_v8(): unknown type";
  return false;
}

static bool
v8_to_vim(Handle<Value> v8obj, typval_T *vimobj, int depth, V8ToVimLookup *lookup, std::string *err)
{
  TRACE("v8_to_vim");
  if (depth > 100) {
    *err = "v8_to_vim(): too deep";
    return false;
  }

  Local<FunctionTemplate> VimList = Local<FunctionTemplate>::New(isolate, p_VimList);
  Local<FunctionTemplate> VimDict = Local<FunctionTemplate>::New(isolate, p_VimDict);
  Local<FunctionTemplate> VimFunc = Local<FunctionTemplate>::New(isolate, p_VimFunc);

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
    V8ToVimLookup::iterator it = lookup->get(v8obj);
    if (it != lookup->end()) {
      tv_set_list(vimobj, it->second.vval.v_list);
      return true;
    }
    list_T *list = list_alloc();
    if (list == NULL) {
      *err = "v8_to_vim(): list_alloc(): out of memoty";
      return false;
    }
    Handle<Array> o = Handle<Array>::Cast(v8obj);
    uint32_t len = o->Length();
    lookup->set(v8obj, VimValue(list));
    for (uint32_t i = 0; i < len; ++i) {
      Handle<Value> v = o->Get(Integer::New(isolate, i));
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
    V8ToVimLookup::iterator it = lookup->get(v8obj);
    if (it != lookup->end()) {
      tv_set_dict(vimobj, it->second.vval.v_dict);
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
    lookup->set(v8obj, VimValue(dict));
    for (uint32_t i = 0; i < len; ++i) {
      Handle<Value> key = keys->Get(Integer::New(isolate, i));
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
  Handle<String> result = String::NewFromUtf8(isolate, chars, String::kNormalString, size);
  delete[] chars;
  return result;
}

static bool
ExecuteString(Handle<String> source, Handle<Value> name, bool print_result, bool report_exceptions, std::string& err)
{
  TRACE("ExecuteString");
  HandleScope handle_scope(isolate);
  TryCatch try_catch;
  Handle<Script> script = Script::Compile(source, name->ToString());
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
    dict_set_tv_nocopy(v_reg, (char_u*)"%v8_print%", &tv);
  }
  return true;
}

static void
ReportException(TryCatch* try_catch)
{
  TRACE("ReportException");
  HandleScope handle_scope(isolate);
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
  dict_set_tv_nocopy(v_reg, (char_u*)"%v8_errmsg%", &tv);
}

static void
vim_execute(const FunctionCallbackInfo<Value>& args)
{
  TRACE("vim_execute");
  HandleScope handle_scope(isolate);

  if (args.Length() != 1 || !args[0]->IsString()) {
    isolate->ThrowException(String::NewFromUtf8(isolate, "usage: vim_execute(string cmd"));
    return;
  }

  String::Utf8Value cmd(args[0]);
  if (!do_cmdline_cmd((char_u*)*cmd)) {
    isolate->ThrowException(String::NewFromUtf8(isolate, "vim_execute(): error do_cmdline_cmd()"));
    return;
  }
}

// The callback that is invoked by v8 whenever the JavaScript 'load'
// function is called.  Loads, compiles and executes its argument
// JavaScript file.
static void
Load(const FunctionCallbackInfo<Value>& args)
{
  TRACE("Load");
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope(isolate);
    String::Utf8Value file(args[i]);
    Handle<String> source = ReadFile(*file);
    if (source.IsEmpty()) {
      isolate->ThrowException(String::NewFromUtf8(isolate, "Error loading file"));
      return;
    }
    std::string err;
    if (!ExecuteString(source, String::NewFromUtf8(isolate, *file), false, false, err)) {
      isolate->ThrowException(String::NewFromUtf8(isolate, err.c_str()));
      return;
    }
  }
}

static list_T *makelistptr = NULL;

static Handle<Value>
MakeVimList(list_T *list)
{
  TRACE("MakeVimList");

  Local<FunctionTemplate> VimList = Local<FunctionTemplate>::New(isolate, p_VimList);

  makelistptr = list;
  Handle<Object> self = VimList->InstanceTemplate()->NewInstance();
  makelistptr = NULL;

  return self;
}

static void
VimListDestroy(const WeakCallbackData<Value, typval_T>& data)
{
  TRACE("VimListDestroy");
  typval_T *tv = data.GetParameter();

  objcache.del(VimValue(tv->vval.v_list));

  weak_unref(tv);
  free_tv(tv);
}

static void
VimListCreate(const FunctionCallbackInfo<Value>& args)
{
  TRACE("VimListCreate");
  if (!args.IsConstructCall()) {
    isolate->ThrowException(String::NewFromUtf8(isolate, "Cannot call constructor as function"));
    return;
  }

  Handle<Object> self = args.Holder();

  list_T *list;
  if (makelistptr != NULL) {
    list = makelistptr;
  } else {
    list = list_alloc();
    if (list == NULL) {
      isolate->ThrowException(String::NewFromUtf8(isolate, "VimListCreate(): list_alloc(): out of memory"));
      return;
    }
  }

  self->SetInternalField(0, External::New(isolate, list));

  // increment Vim's reference count
  typval_T *tv = alloc_tv();
  tv_set_list(tv, list);
  weak_ref(tv);

  // make weak reference
  CopyableValuePersistent p = CopyableValuePersistent();
  p.Reset(isolate, self);
  p.SetWeak(tv, VimListDestroy);

  objcache.set(VimValue(list), p);

  args.GetReturnValue().Set(self);
}

static void
VimListGet(uint32_t index, const PropertyCallbackInfo<Value>& info)
{
  TRACE("VimListGet");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  listitem_T *li = list_find(list, index);
  if (li == NULL)
    return;
  std::string err;
  Handle<Value> v8obj;
  if (!vim_to_v8(&li->li_tv, &v8obj, 1, &objcache, &err)) {
    isolate->ThrowException(String::NewFromUtf8(isolate, err.c_str()));
    return;
  }
  info.GetReturnValue().Set(v8obj);
}

static void
VimListSet(uint32_t index, Local<Value> value, const PropertyCallbackInfo<Value>& info)
{
  TRACE("VimListSet");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  listitem_T *li = list_find(list, index);
  if (li == NULL) {
    isolate->ThrowException(String::NewFromUtf8(isolate, "list index out of range"));
    return;
  }
  V8ToVimLookup lookup;
  std::string err;
  typval_T vimobj;
  if (!v8_to_vim(value, &vimobj, 1, &lookup, &err)) {
    isolate->ThrowException(String::NewFromUtf8(isolate, err.c_str()));
    return;
  }
  clear_tv(&li->li_tv);
  li->li_tv = vimobj;
  info.GetReturnValue().Set(value);
}

static void
VimListQuery(uint32_t index, const PropertyCallbackInfo<Integer>& info)
{
  TRACE("VimListQuery");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  listitem_T *li = list_find(list, index);
  if (li == NULL) {
    info.GetReturnValue().Set(Integer::New(isolate, DontEnum));
    return;
  }
  info.GetReturnValue().Set(Integer::New(isolate, None));
}

static void
VimListDelete(uint32_t index, const PropertyCallbackInfo<Boolean>& info)
{
  TRACE("VimListDelete");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  listitem_T *li = list_find(list, index);
  if (li == NULL) {
    info.GetReturnValue().Set(False(isolate));
    return;
  }
  list_remove(list, li, li);
  listitem_free(li);
  info.GetReturnValue().Set(True(isolate));
}

static void
VimListEnumerate(const PropertyCallbackInfo<Array>& info)
{
  TRACE("VimListEnumerate");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  uint32_t len = list_len(list);
  Handle<Array> keys = Array::New(isolate, len);
  for (uint32_t i = 0; i < len; ++i) {
    keys->Set(Integer::New(isolate, i), Integer::New(isolate, i));
  }
  info.GetReturnValue().Set(keys);
}

static void
VimListLength(Local<String> property, const PropertyCallbackInfo<Value>& info)
{
  TRACE("VimListLength");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  uint32_t len = list_len(list);
  info.GetReturnValue().Set(Integer::New(isolate, len));
}

static dict_T *makedictptr = NULL;

static Handle<Value>
MakeVimDict(dict_T *dict)
{
  TRACE("MakeVimDict");

  Local<FunctionTemplate> VimDict = Local<FunctionTemplate>::New(isolate, p_VimDict);

  makedictptr = dict;
  Handle<Object> self = VimDict->InstanceTemplate()->NewInstance();
  makedictptr = NULL;

  return self;
}

static void
VimDictDestroy(const WeakCallbackData<Value, typval_T>& data)
{
  TRACE("VimDictDestroy");
  typval_T *tv = data.GetParameter();

  objcache.del(VimValue(tv->vval.v_dict));

  weak_unref(tv);
  free_tv(tv);
}

static void
VimDictCreate(const FunctionCallbackInfo<Value>& args)
{
  TRACE("VimDictCreate");
  if (!args.IsConstructCall()) {
    isolate->ThrowException(String::NewFromUtf8(isolate, "Cannot call constructor as function"));
    return;
  }

  Handle<Object> self = args.Holder();

  dict_T *dict;
  if (makedictptr != NULL) {
    dict = makedictptr;
  } else {
    dict = dict_alloc();
    if (dict == NULL) {
      isolate->ThrowException(String::NewFromUtf8(isolate, "VimDictCreate(): dict_alloc(): out of memory"));
      return;
    }
  }

  self->SetInternalField(0, External::New(isolate, dict));

  // increment Vim's reference count
  typval_T *tv = alloc_tv();
  tv_set_dict(tv, dict);
  weak_ref(tv);

  // make weak reference
  CopyableValuePersistent p = CopyableValuePersistent();
  p.Reset(isolate, self);
  p.SetWeak(tv, VimDictDestroy);
  objcache.set(VimValue(dict), p);

  args.GetReturnValue().Set(self);
}

static void
VimDictIdxGet(uint32_t index, const PropertyCallbackInfo<Value>& info)
{
  TRACE("VimDictIdxGet");
  VimDictGet(Integer::New(isolate, index)->ToString(), info);
}

static void
VimDictIdxSet(uint32_t index, Local<Value> value, const PropertyCallbackInfo<Value>& info)
{
  TRACE("VimDictIdxSet");
  VimDictSet(Integer::New(isolate, index)->ToString(), value, info);
}

static void
VimDictIdxQuery(uint32_t index, const PropertyCallbackInfo<Integer>& info)
{
  TRACE("VimDictIdxQuery");
  VimDictQuery(Integer::New(isolate, index)->ToString(), info);
}

static void
VimDictIdxDelete(uint32_t index, const PropertyCallbackInfo<Boolean>& info)
{
  TRACE("VimDictIdxDelete");
  VimDictDelete(Integer::New(isolate, index)->ToString(), info);
}

static void
VimDictGet(Local<String> property, const PropertyCallbackInfo<Value>& info)
{
  TRACE("VimDictIdxGet");
  Local<FunctionTemplate> VimFunc = Local<FunctionTemplate>::New(isolate, p_VimFunc);
  if (property->Length() == 0) {
    isolate->ThrowException(String::NewFromUtf8(isolate, "Cannot use empty key for Dictionary"));
    return;
  }
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  dict_T *dict = static_cast<dict_T*>(external->Value());
  String::Utf8Value key(property);
  dictitem_T *di = dict_find(dict, (char_u*)*key, -1);
  if (di == NULL) {
    // fallback to prototype.  otherwise String(obj) don't work due to
    // lack of toString().
    Handle<Object> prototype = Handle<Object>::Cast(self->GetPrototype());
    info.GetReturnValue().Set(prototype->Get(property));
    return;
  }
  std::string err;
  Handle<Value> v8obj;
  if (!vim_to_v8(&di->di_tv, &v8obj, 1, &objcache, &err)) {
    isolate->ThrowException(String::NewFromUtf8(isolate, err.c_str()));
    return;
  }
  // XXX: When obj.func(), args.Holder() and args.This() are VimFunc
  // insted of obj.  Use internal field for now.
  if (VimFunc->HasInstance(v8obj)) {
    Handle<Object> func = Handle<Object>::Cast(v8obj);
    func->SetInternalField(1, self);
  }
  info.GetReturnValue().Set(v8obj);
}

static void
VimDictSet(Local<String> property, Local<Value> value, const PropertyCallbackInfo<Value>& info)
{
  TRACE("VimDictSet");
  if (property->Length() == 0) {
    isolate->ThrowException(String::NewFromUtf8(isolate, "Cannot use empty key for Dictionary"));
    return;
  }
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  dict_T *dict = static_cast<dict_T*>(external->Value());
  String::Utf8Value key(property);
  V8ToVimLookup lookup;
  std::string err;
  typval_T vimobj;
  if (!v8_to_vim(value, &vimobj, 1, &lookup, &err)) {
    isolate->ThrowException(String::NewFromUtf8(isolate, err.c_str()));
    return;
  }
  if (!dict_set_tv_nocopy(dict, (char_u*)*key, &vimobj)) {
    clear_tv(&vimobj);
    isolate->ThrowException(String::NewFromUtf8(isolate, "error dict_set_tv_nocopy()"));
    return;
  }
  info.GetReturnValue().Set(value);
}

static void
VimDictQuery(Local<String> property, const PropertyCallbackInfo<Integer>& info)
{
  TRACE("VimDictQuery");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  dict_T *dict = static_cast<dict_T*>(external->Value());
  String::Utf8Value key(property);
  dictitem_T *di = dict_find(dict, (char_u*)*key, -1);
  if (di == NULL) {
    info.GetReturnValue().Set(Integer::New(isolate, DontEnum));
    return;
  }
  info.GetReturnValue().Set(Integer::New(isolate, None));
}

static void
VimDictDelete(Local<String> property, const PropertyCallbackInfo<Boolean>& info)
{
  TRACE("VimDictDelete");
  if (property->Length() == 0) {
    info.GetReturnValue().Set(False(isolate));
    return;
  }
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  dict_T *dict = static_cast<dict_T*>(external->Value());
  String::Utf8Value key(property);
  dictitem_T *di = dict_find(dict, (char_u*)*key, -1);
  if (di == NULL) {
    info.GetReturnValue().Set(False(isolate));
    return;
  }
  dictitem_remove(dict, di);
  info.GetReturnValue().Set(True(isolate));
}

static void
VimDictEnumerate(const PropertyCallbackInfo<Array>& info)
{
  TRACE("VimDictEnumerate");
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  dict_T *dict = static_cast<dict_T*>(external->Value());
  hashtab_T *ht = &dict->dv_hashtab;
  long_u todo = ht->ht_used;
  hashitem_T *hi;
  int i = 0;
  Handle<Array> keys = Array::New(isolate, todo);
  for (hi = ht->ht_array; todo > 0; ++hi) {
    if (!HASHITEM_EMPTY(hi)) {
      --todo;
      keys->Set(Integer::New(isolate, i++), String::NewFromUtf8(isolate, (char *)hi->hi_key));
    }
  }
  info.GetReturnValue().Set(keys);
}

static Handle<Value>
MakeVimFunc(const char *name)
{
  TRACE("MakeVimFunc");

  if (name == NULL)
      return Undefined(isolate);

  Local<FunctionTemplate> VimFunc = Local<FunctionTemplate>::New(isolate, p_VimFunc);

  typval_T *tv = alloc_tv();
  tv_set_func(tv, (char_u*)name);
  weak_ref(tv);

  Handle<Object> self = VimFunc->InstanceTemplate()->NewInstance();
  self->SetInternalField(0, External::New(isolate, tv->vval.v_string));
  self->SetInternalField(1, Undefined(isolate));

  // make weak reference
  CopyableValuePersistent p = CopyableValuePersistent();
  p.Reset(isolate, self);
  p.SetWeak(tv, VimFuncDestroy);
  objcache.set(VimValue(tv->vval.v_string), p);

  return self;
}

static void
VimFuncDestroy(const WeakCallbackData<Value, typval_T>& data)
{
  TRACE("VimFuncDestroy");
  typval_T *tv = data.GetParameter();

  objcache.del(VimValue(tv->vval.v_string));

  weak_unref(tv);
  free_tv(tv);
}

static void
VimFuncCall(const FunctionCallbackInfo<Value>& args)
{
  TRACE("VimFuncCall");
  if (args.IsConstructCall()) {
    isolate->ThrowException(String::NewFromUtf8(isolate, "Cannot create VimFunc"));
    return;
  }

  Handle<Object> self = args.Holder();

  Handle<Array> arr = Array::New(isolate, args.Length());
  for (int i = 0; i < args.Length(); ++i)
    arr->Set(Integer::New(isolate, i), args[i]);

  Handle<Value> obj = self->GetInternalField(1);

  Handle<Value> callargs[3] = {self, arr, obj};

  // return vim.call(name, args, obj)
  Local<Context> context = Local<Context>::New(isolate, p_context);
  Handle<Object> vim = Handle<Object>::Cast(context->Global()->Get(String::NewFromUtf8(isolate, "vim")));
  Handle<Function> call = Handle<Function>::Cast(vim->Get(String::NewFromUtf8(isolate, "call")));
  args.GetReturnValue().Set(call->Call(vim, 3, callargs));
}

