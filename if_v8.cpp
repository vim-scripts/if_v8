/*
 * v8 interface to Vim
 *
 * Last Change: 2008-12-14
 * Maintainer: Yukihiro Nakadaira <yukihiro.nakadaira@gmail.com>
 *
 * Require:
 *  linux or windows.
 *  Vim executable file with some exported symbol that if_v8 requires.
 *    On linux:
 *      Compile with gcc's -rdynamic option.
 *    On windows (msvc):
 *      Use vim_export.def and add linker flag "/DEF:vim_export.def".
 *      nmake -f Make_mvc.mak linkdebug=/DEF:vim_export.def
 *
 * Compile:
 *  g++ -I/path/to/v8/include -shared -o if_v8.so if_v8.cpp \
 *      -L/path/to/v8 -lv8 -lpthread
 *
 * Usage:
 *  let if_v8 = '/path/to/if_v8.so'
 *  let err = libcall(if_v8, 'init', if_v8)
 *  let err = libcall(if_v8, 'execute', 'vim.execute("echo \"hello, v8\"")')
 *  let err = libcall(if_v8, 'execute', 'vim.eval("&tw")')
 *  let err = libcall(if_v8, 'execute', 'load("foo.js")')
 *
 */
#ifdef WIN32
# include <windows.h>
#else
# include <dlfcn.h>
#endif
#include <string>
#include <cstdio>
#include <map>
#include <v8.h>

typedef std::map<void*, v8::Handle<v8::Value> > LookupMap;

typedef unsigned char char_u;
typedef unsigned short short_u;
typedef unsigned int int_u;
typedef unsigned long long_u;

/* Item for a hashtable.  "hi_key" can be one of three values:
 * NULL:	   Never been used
 * HI_KEY_REMOVED: Entry was removed
 * Otherwise:	   Used item, pointer to the actual key; this usually is
 *		   inside the item, subtract an offset to locate the item.
 *		   This reduces the size of hashitem by 1/3.
 */
typedef struct hashitem_S
{
    long_u	hi_hash;	/* cached hash number of hi_key */
    char_u	*hi_key;
} hashitem_T;

/* The address of "hash_removed" is used as a magic number for hi_key to
 * indicate a removed item. */
#define HI_KEY_REMOVED vim.hash_removed
#define HASHITEM_EMPTY(hi) ((hi)->hi_key == NULL || (hi)->hi_key == vim.hash_removed)

/* Initial size for a hashtable.  Our items are relatively small and growing
 * is expensive, thus use 16 as a start.  Must be a power of 2. */
#define HT_INIT_SIZE 16

typedef struct hashtable_S
{
    long_u	ht_mask;	/* mask used for hash value (nr of items in
				 * array is "ht_mask" + 1) */
    long_u	ht_used;	/* number of items used */
    long_u	ht_filled;	/* number of items used + removed */
    int		ht_locked;	/* counter for hash_lock() */
    int		ht_error;	/* when set growing failed, can't add more
				   items before growing works */
    hashitem_T	*ht_array;	/* points to the array, allocated when it's
				   not "ht_smallarray" */
    hashitem_T	ht_smallarray[HT_INIT_SIZE];   /* initial array */
} hashtab_T;

typedef long_u hash_T;		/* Type for hi_hash */


#if SIZEOF_INT <= 3		/* use long if int is smaller than 32 bits */
typedef long	varnumber_T;
#else
typedef int	varnumber_T;
#endif
typedef double	float_T;

typedef struct listvar_S list_T;
typedef struct dictvar_S dict_T;

/*
 * Structure to hold an internal variable without a name.
 */
typedef struct
{
    char	v_type;	    /* see below: VAR_NUMBER, VAR_STRING, etc. */
    char	v_lock;	    /* see below: VAR_LOCKED, VAR_FIXED */
    union
    {
	varnumber_T	v_number;	/* number value */
	float_T		v_float;	/* floating number value */
	char_u		*v_string;	/* string value (can be NULL!) */
	list_T		*v_list;	/* list value (can be NULL!) */
	dict_T		*v_dict;	/* dict value (can be NULL!) */
    }		vval;
} typval_T;

/* Values for "v_type". */
#define VAR_UNKNOWN 0
#define VAR_NUMBER  1	/* "v_number" is used */
#define VAR_STRING  2	/* "v_string" is used */
#define VAR_FUNC    3	/* "v_string" is function name */
#define VAR_LIST    4	/* "v_list" is used */
#define VAR_DICT    5	/* "v_dict" is used */
#define VAR_FLOAT   6	/* "v_float" is used */

/* Values for "v_lock". */
#define VAR_LOCKED  1	/* locked with lock(), can use unlock() */
#define VAR_FIXED   2	/* locked forever */

/*
 * Structure to hold an item of a list: an internal variable without a name.
 */
typedef struct listitem_S listitem_T;

struct listitem_S
{
    listitem_T	*li_next;	/* next item in list */
    listitem_T	*li_prev;	/* previous item in list */
    typval_T	li_tv;		/* type and value of the variable */
};

/*
 * Struct used by those that are using an item in a list.
 */
typedef struct listwatch_S listwatch_T;

struct listwatch_S
{
    listitem_T		*lw_item;	/* item being watched */
    listwatch_T		*lw_next;	/* next watcher */
};

/*
 * Structure to hold info about a list.
 */
struct listvar_S
{
    listitem_T	*lv_first;	/* first item, NULL if none */
    listitem_T	*lv_last;	/* last item, NULL if none */
    int		lv_refcount;	/* reference count */
    int		lv_len;		/* number of items */
    listwatch_T	*lv_watch;	/* first watcher, NULL if none */
    int		lv_idx;		/* cached index of an item */
    listitem_T	*lv_idx_item;	/* when not NULL item at index "lv_idx" */
    int		lv_copyID;	/* ID used by deepcopy() */
    list_T	*lv_copylist;	/* copied list used by deepcopy() */
    char	lv_lock;	/* zero, VAR_LOCKED, VAR_FIXED */
    list_T	*lv_used_next;	/* next list in used lists list */
    list_T	*lv_used_prev;	/* previous list in used lists list */
};

/*
 * Structure to hold an item of a Dictionary.
 * Also used for a variable.
 * The key is copied into "di_key" to avoid an extra alloc/free for it.
 */
struct dictitem_S
{
    typval_T	di_tv;		/* type and value of the variable */
    char_u	di_flags;	/* flags (only used for variable) */
    char_u	di_key[1];	/* key (actually longer!) */
};

typedef struct dictitem_S dictitem_T;

#define DI_FLAGS_RO	1 /* "di_flags" value: read-only variable */
#define DI_FLAGS_RO_SBX 2 /* "di_flags" value: read-only in the sandbox */
#define DI_FLAGS_FIX	4 /* "di_flags" value: fixed variable, not allocated */
#define DI_FLAGS_LOCK	8 /* "di_flags" value: locked variable */

/*
 * Structure to hold info about a Dictionary.
 */
struct dictvar_S
{
    int		dv_refcount;	/* reference count */
    hashtab_T	dv_hashtab;	/* hashtab that refers to the items */
    int		dv_copyID;	/* ID used by deepcopy() */
    dict_T	*dv_copydict;	/* copied dict used by deepcopy() */
    char	dv_lock;	/* zero, VAR_LOCKED, VAR_FIXED */
    dict_T	*dv_used_next;	/* next dict in used dicts list */
    dict_T	*dv_used_prev;	/* previous dict in used dicts list */
};

/*
 * In a hashtab item "hi_key" points to "di_key" in a dictitem.
 * This avoids adding a pointer to the hashtab item.
 * DI2HIKEY() converts a dictitem pointer to a hashitem key pointer.
 * HIKEY2DI() converts a hashitem key pointer to a dictitem pointer.
 * HI2DI() converts a hashitem pointer to a dictitem pointer.
 */
static dictitem_T dumdi;
#define DI2HIKEY(di) ((di)->di_key)
#define HIKEY2DI(p)  ((dictitem_T *)(p - (dumdi.di_key - (char_u *)&dumdi)))
#define HI2DI(hi)     HIKEY2DI((hi)->hi_key)

static struct vim {
  typval_T * (*eval_expr) (char_u *arg, char_u **nextcmd);
  int (*do_cmdline_cmd) (char_u *cmd);
  void (*free_tv) (typval_T *varp);
  char_u *hash_removed;
} vim;

static void *dll_handle = NULL;
static v8::Handle<v8::Context> context;

#ifdef WIN32
# define DLLEXPORT __declspec(dllexport)
#else
# define DLLEXPORT
#endif

/* API */
extern "C" {
DLLEXPORT const char *init(const char *dll_path);
DLLEXPORT const char *execute(const char *expr);
}

static const char *init_vim();
static const char *init_v8();
static v8::Handle<v8::Value> vim_eval(const v8::Arguments& args);
static v8::Handle<v8::Value> vim_execute(const v8::Arguments& args);
static v8::Handle<v8::Value> vim_to_v8(typval_T *tv, int depth, LookupMap *lookup_map);
static v8::Handle<v8::Value> Load(const v8::Arguments& args);
static v8::Handle<v8::String> ReadFile(const char* name);
static std::string ExecuteString(v8::Handle<v8::String> source, v8::Handle<v8::Value> name);

const char *
init(const char *dll_path)
{
  const char *err;
  if (dll_handle != NULL)
    return NULL;
#ifdef WIN32
  if ((dll_handle = (void*)LoadLibrary(dll_path)) == NULL)
    return "error: LoadLibrary()";
#else
  if ((dll_handle = dlopen(dll_path, RTLD_NOW)) == NULL)
    return dlerror();
#endif
  if ((err = init_vim()) != NULL)
    return err;
  if ((err = init_v8()) != NULL)
    return err;
  return NULL;
}

const char *
execute(const char *expr)
{
  static std::string err;
  if (vim.eval_expr == NULL)
    return "not initialized";
  v8::HandleScope handle_scope;
  v8::Context::Scope context_scope(context);
  err = ExecuteString(v8::String::New(expr), v8::Undefined());
  return err.c_str();
}

#ifdef WIN32
#define GETSYMBOL(name)                                                   \
  do {                                                                    \
    void **p = (void **)&vim.name;                                        \
    if ((*p = (void*)GetProcAddress((HMODULE)vim_handle, #name)) == NULL) \
      return "error: GetProcAddress()";                                   \
  } while (0)
#else
#define GETSYMBOL(name)                           \
  do {                                            \
    void **p = (void **)&vim.name;                \
    if ((*p = dlsym(vim_handle, #name)) == NULL)  \
      return dlerror();                           \
  } while (0)
#endif

static const char *
init_vim()
{
  void *vim_handle;
#ifdef WIN32
  if ((vim_handle = (void*)GetModuleHandle(NULL)) == NULL)
    return "error: GetModuleHandle()";
#else
  if ((vim_handle = dlopen(NULL, RTLD_NOW)) == NULL)
    return dlerror();
#endif
  GETSYMBOL(eval_expr);
  GETSYMBOL(do_cmdline_cmd);
  GETSYMBOL(free_tv);
  GETSYMBOL(hash_removed);
  return NULL;
}

static const char *
init_v8()
{
  v8::HandleScope handle_scope;
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
  global->Set(v8::String::New("load"), v8::FunctionTemplate::New(Load));
  v8::Handle<v8::ObjectTemplate> vimobj = v8::ObjectTemplate::New();
  vimobj->Set(v8::String::New("eval"), v8::FunctionTemplate::New(vim_eval));
  vimobj->Set(v8::String::New("execute"), v8::FunctionTemplate::New(vim_execute));
  global->Set(v8::String::New("vim"), vimobj);
  context = v8::Context::New(NULL, global);
  return NULL;
}

static v8::Handle<v8::Value>
vim_eval(const v8::Arguments& args)
{
  if (args.Length() != 1 || !args[0]->IsString())
    return v8::Undefined();
  v8::HandleScope handle_scope;
  v8::String::AsciiValue str(args[0]);
  typval_T *tv = vim.eval_expr((char_u *)*str, NULL);
  LookupMap lookup_map;
  v8::Handle<v8::Value> res = vim_to_v8(tv, 1, &lookup_map);
  lookup_map.clear();
  vim.free_tv(tv);
  return res;
}

static v8::Handle<v8::Value>
vim_execute(const v8::Arguments& args)
{
  if (args.Length() != 1 || !args[0]->IsString())
    return v8::Undefined();
  v8::HandleScope handle_scope;
  v8::String::AsciiValue str(args[0]);
  vim.do_cmdline_cmd((char_u *)*str);
  return v8::Undefined();
}

static v8::Handle<v8::Value>
vim_to_v8(typval_T *tv, int depth, LookupMap *lookup_map)
{
  if (tv == NULL || depth > 100)
    return v8::Undefined();
  LookupMap::iterator it = lookup_map->find(tv);
  if (it != lookup_map->end())
    return it->second;
  switch (tv->v_type) {
  case VAR_NUMBER:
    return v8::Integer::New(tv->vval.v_number);
  case VAR_FLOAT:
    return v8::Number::New(tv->vval.v_float);
  case VAR_STRING:
    return v8::String::New((char *)tv->vval.v_string);
  case VAR_FUNC:
    return v8::String::New("[function]");
  case VAR_LIST:
    {
      list_T *list = tv->vval.v_list;
      if (list == NULL)
        return v8::Undefined();
      int i = 0;
      v8::Handle<v8::Array> res = v8::Array::New(0);
      for (listitem_T *curr = list->lv_first; curr != NULL; curr = curr->li_next)
        res->Set(v8::Number::New(i++), vim_to_v8(&curr->li_tv, depth + 1, lookup_map));
      lookup_map->insert(LookupMap::value_type(tv, res));
      return res;
    }
  case VAR_DICT:
    {
      hashtab_T *ht = &tv->vval.v_dict->dv_hashtab;
      long_u todo = ht->ht_used;
      hashitem_T *hi;
      dictitem_T *di;
      v8::Handle<v8::Object> res = v8::Object::New();
      for (hi = ht->ht_array; todo > 0; ++hi) {
        if (!HASHITEM_EMPTY(hi)) {
          --todo;
          di = HI2DI(hi);
          res->Set(v8::String::New((char *)hi->hi_key), vim_to_v8(&di->di_tv, depth + 1, lookup_map));
        }
      }
      lookup_map->insert(LookupMap::value_type(tv, res));
      return res;
    }
  case VAR_UNKNOWN:
  default:
    return v8::Undefined();
  }
}

// The callback that is invoked by v8 whenever the JavaScript 'load'
// function is called.  Loads, compiles and executes its argument
// JavaScript file.
v8::Handle<v8::Value> Load(const v8::Arguments& args) {
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope;
    v8::String::AsciiValue file(args[i]);
    v8::Handle<v8::String> source = ReadFile(*file);
    if (source.IsEmpty()) {
      return v8::ThrowException(v8::String::New("Error loading file"));
    }
    ExecuteString(source, v8::String::New(*file));
  }
  return v8::Undefined();
}

// Reads a file into a v8 string.
v8::Handle<v8::String> ReadFile(const char* name) {
  FILE* file = fopen(name, "rb");
  if (file == NULL) return v8::Handle<v8::String>();

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
  v8::Handle<v8::String> result = v8::String::New(chars, size);
  delete[] chars;
  return result;
}

std::string ExecuteString(v8::Handle<v8::String> source, v8::Handle<v8::Value> name) {
  v8::HandleScope handle_scope;
  v8::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);
  if (script.IsEmpty()) {
    v8::String::AsciiValue error(try_catch.Exception());
    return std::string(*error);
  }
  v8::Handle<v8::Value> result = script->Run();
  if (result.IsEmpty()) {
    v8::String::AsciiValue error(try_catch.Exception());
    return std::string(*error);
  }
  return std::string("");
}
