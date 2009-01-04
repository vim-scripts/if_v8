/* vim:set foldmethod=marker:
 *
 * v8 interface to Vim
 *
 * Last Change: 2009-01-04
 * Maintainer: Yukihiro Nakadaira <yukihiro.nakadaira@gmail.com>
 */
#ifdef WIN32
# include <windows.h>
#else
# include <dlfcn.h>
#endif
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <v8.h>

// Vim {{{
// header {{{2
#define FEAT_FLOAT 1

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
#define HI_KEY_REMOVED p_hash_removed
#define HASHITEM_EMPTY(hi) ((hi)->hi_key == NULL || (hi)->hi_key == p_hash_removed)

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
#ifdef FEAT_FLOAT
	float_T		v_float;	/* floating number value */
#endif
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

struct condstack;

// interface {{{2

// variables
static char_u *p_hash_removed;
// functions
static typval_T * (*eval_expr) (char_u *arg, char_u **nextcmd);
static char_u * (*eval_to_string) (char_u *arg, char_u **nextcmd, int convert);
static int (*do_cmdline_cmd) (char_u *cmd);
static void (*free_tv) (typval_T *varp);
static void (*clear_tv) (typval_T *varp);
static int (*emsg) (char_u *s);
static char_u * (*alloc) (unsigned size);
static void * (*vim_free) (void *x);
static char_u * (*vim_strsave) (char_u *string);
static char_u * (*vim_strnsave) (char_u *string, int len);
static void (*vim_strncpy) (char_u *to, char_u *from, size_t len);
static list_T * (*list_alloc)();
static void (*list_free) (list_T *l, int recurse);
static void * (*list_unref) (list_T *l);
static dict_T * (*dict_alloc)();
static int (*hash_add) (hashtab_T *ht, char_u *key);
static hashitem_T * (*hash_find) (hashtab_T *ht, char_u *key);
static void (*hash_remove) (hashtab_T *ht, hashitem_T *hi);

#define FALSE 0
#define TRUE 1

#define STRLEN(s)	    strlen((char *)(s))
#define STRCPY(d, s)	    strcpy((char *)(d), (char *)(s))
#define vim_memset(ptr, c, size)   memset((ptr), (c), (size))

// copied static functions {{{2

/*
 * Set the value of a variable to NULL without freeing items.
 */
    static void
init_tv(
    typval_T *varp
    )
{
    if (varp != NULL)
	vim_memset(varp, 0, sizeof(typval_T));
}

// List {{{2

/*
 * Allocate a list item.
 */
    static listitem_T *
listitem_alloc()
{
    return (listitem_T *)alloc(sizeof(listitem_T));
}

/*
 * Free a list item.  Also clears the value.  Does not notify watchers.
 */
    static void
listitem_free(
    listitem_T *item
    )
{
    clear_tv(&item->li_tv);
    vim_free(item);
}

/*
 * Locate item with index "n" in list "l" and return it.
 * A negative index is counted from the end; -1 is the last item.
 * Returns NULL when "n" is out of range.
 */
    static listitem_T *
list_find(
    list_T	*l,
    long	n
    )
{
    listitem_T	*item;
    long	idx;

    if (l == NULL)
	return NULL;

    /* Negative index is relative to the end. */
    if (n < 0)
	n = l->lv_len + n;

    /* Check for index out of range. */
    if (n < 0 || n >= l->lv_len)
	return NULL;

    /* When there is a cached index may start search from there. */
    if (l->lv_idx_item != NULL)
    {
	if (n < l->lv_idx / 2)
	{
	    /* closest to the start of the list */
	    item = l->lv_first;
	    idx = 0;
	}
	else if (n > (l->lv_idx + l->lv_len) / 2)
	{
	    /* closest to the end of the list */
	    item = l->lv_last;
	    idx = l->lv_len - 1;
	}
	else
	{
	    /* closest to the cached index */
	    item = l->lv_idx_item;
	    idx = l->lv_idx;
	}
    }
    else
    {
	if (n < l->lv_len / 2)
	{
	    /* closest to the start of the list */
	    item = l->lv_first;
	    idx = 0;
	}
	else
	{
	    /* closest to the end of the list */
	    item = l->lv_last;
	    idx = l->lv_len - 1;
	}
    }

    while (n > idx)
    {
	/* search forward */
	item = item->li_next;
	++idx;
    }
    while (n < idx)
    {
	/* search backward */
	item = item->li_prev;
	--idx;
    }

    /* cache the used index */
    l->lv_idx = idx;
    l->lv_idx_item = item;

    return item;
}


/*
 * Append item "item" to the end of list "l".
 */
    static void
list_append(
    list_T	*l,
    listitem_T	*item
    )
{
    if (l->lv_last == NULL)
    {
	/* empty list */
	l->lv_first = item;
	l->lv_last = item;
	item->li_prev = NULL;
    }
    else
    {
	l->lv_last->li_next = item;
	item->li_prev = l->lv_last;
	l->lv_last = item;
    }
    ++l->lv_len;
    item->li_next = NULL;
}

/*
 * Just before removing an item from a list: advance watchers to the next
 * item.
 */
    static void
list_fix_watch(
    list_T	*l,
    listitem_T	*item
    )
{
    listwatch_T	*lw;

    for (lw = l->lv_watch; lw != NULL; lw = lw->lw_next)
	if (lw->lw_item == item)
	    lw->lw_item = item->li_next;
}

/*
 * Remove items "item" to "item2" from list "l".
 * Does not free the listitem or the value!
 */
    static void
list_remove(
    list_T	*l,
    listitem_T	*item,
    listitem_T	*item2
    )
{
    listitem_T	*ip;

    /* notify watchers */
    for (ip = item; ip != NULL; ip = ip->li_next)
    {
	--l->lv_len;
	list_fix_watch(l, ip);
	if (ip == item2)
	    break;
    }

    if (item2->li_next == NULL)
	l->lv_last = item->li_prev;
    else
	item2->li_next->li_prev = item->li_prev;
    if (item->li_prev == NULL)
	l->lv_first = item2->li_next;
    else
	item->li_prev->li_next = item2->li_next;
    l->lv_idx_item = NULL;
}

/*
 * Get the number of items in a list.
 */
    static long
list_len(
    list_T	*l
    )
{
    if (l == NULL)
	return 0L;
    return l->lv_len;
}

// Dictionary {{{2

/*
 * Allocate a Dictionary item.
 * The "key" is copied to the new item.
 * Note that the value of the item "di_tv" still needs to be initialized!
 * Returns NULL when out of memory.
 */
    static dictitem_T *
dictitem_alloc(
    char_u	*key
    )
{
    dictitem_T *di;

    di = (dictitem_T *)alloc((unsigned)(sizeof(dictitem_T) + STRLEN(key)));
    if (di != NULL)
    {
	STRCPY(di->di_key, key);
	di->di_flags = 0;
    }
    return di;
}

/*
 * Free a dict item.  Also clears the value.
 */
    static void
dictitem_free(
    dictitem_T *item
    )
{
    clear_tv(&item->di_tv);
    vim_free(item);
}

/*
 * Remove item "item" from Dictionary "dict" and free it.
 * XXX: modified
 */
    static void
dictitem_remove(
    dict_T	*dict,
    dictitem_T	*item
    )
{
    hashitem_T	*hi;

    hi = hash_find(&dict->dv_hashtab, item->di_key);
    if (HASHITEM_EMPTY(hi))
#if 0
	EMSG2(_(e_intern2), "dictitem_remove()");
#else
	; // void
#endif
    else
	hash_remove(&dict->dv_hashtab, hi);
    dictitem_free(item);
}

/*
 * Add item "item" to Dictionary "d".
 * Returns FAIL when out of memory and when key already existed.
 */
    static int
dict_add(
    dict_T	*d,
    dictitem_T	*item
    )
{
    return hash_add(&d->dv_hashtab, item->di_key);
}

/*
 * Free a Dictionary, including all items it contains.
 * Ignores the reference count.
 * XXX: recurse is not supported (always TRUE)
 */
#define dict_free(d, recurse) wrap_dict_free(d)
    static void
wrap_dict_free(dict_T  *d)
{
    typval_T tv;
    init_tv(&tv);
    tv.v_type = VAR_DICT;
    tv.vval.v_dict = d;
    tv.vval.v_dict->dv_refcount = 1;
    clear_tv(&tv);
}

/*
 * Unreference a Dictionary: decrement the reference count and free it when it
 * becomes zero.
 */
    static void
dict_unref(
    dict_T *d
    )
{
    if (d != NULL && --d->dv_refcount <= 0)
	dict_free(d, TRUE);
}

/*
 * Find item "key[len]" in Dictionary "d".
 * If "len" is negative use strlen(key).
 * Returns NULL when not found.
 */
    static dictitem_T *
dict_find(
    dict_T	*d,
    char_u	*key,
    int		len
    )
{
#define AKEYLEN 200
    char_u	buf[AKEYLEN];
    char_u	*akey;
    char_u	*tofree = NULL;
    hashitem_T	*hi;

    if (len < 0)
	akey = key;
    else if (len >= AKEYLEN)
    {
	tofree = akey = vim_strnsave(key, len);
	if (akey == NULL)
	    return NULL;
    }
    else
    {
	/* Avoid a malloc/free by using buf[]. */
	vim_strncpy(buf, key, len);
	akey = buf;
    }

    hi = hash_find(&d->dv_hashtab, akey);
    vim_free(tofree);
    if (HASHITEM_EMPTY(hi))
	return NULL;
    return HI2DI(hi);
}

// utils {{{2

/*
 * let dict[key] = tv without incrementing reference count.
 */
static bool
dict_set(dict_T *dict, const char *key, typval_T *tv)
{
  dictitem_T *di = dict_find(dict, (char_u*)key, -1);
  if (di == NULL) {
    di = dictitem_alloc((char_u*)key);
    if (di == NULL) {
      return false;
    }
    if (!dict_add(dict, di)) {
      dictitem_free(di);
      return false;
    }
  } else {
    clear_tv(&di->di_tv);
  }
  di->di_tv = *tv;
  return true;
}

// }}}1

typedef void* list_or_dict_ptr;
typedef std::map<list_or_dict_ptr, v8::Handle<v8::Value> > LookupMap;

static void *dll_handle = NULL;
static v8::Handle<v8::Context> context;
static v8::Handle<v8::FunctionTemplate> VimList;
static v8::Handle<v8::ObjectTemplate> VimListTemplate;
static v8::Handle<v8::FunctionTemplate> VimDict;
static v8::Handle<v8::ObjectTemplate> VimDictTemplate;

// ensure the following condition:
//   var x = new vim.Dict();
//   x.x = x;
//   x === x.x  => true
// the same List/Dictionary is instantiated by only one V8 object.
static LookupMap objcache;

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
static v8::Handle<v8::Value> vim_execute(const v8::Arguments& args);
static bool vim_to_v8(typval_T *vimobj, v8::Handle<v8::Value> *v8obj, int depth, LookupMap *lookup, bool wrap, std::string *err);
static LookupMap::iterator LookupMapFindV8Value(LookupMap* lookup, v8::Handle<v8::Value> v);
static bool v8_to_vim(v8::Handle<v8::Value> v8obj, typval_T *vimobj, int depth, LookupMap *lookup, std::string *err);
static v8::Handle<v8::Value> VimListCreate(const v8::Arguments& args);
static void VimListDestroy(v8::Persistent<v8::Value> object, void* parameter);
static v8::Handle<v8::Value> VimListGet(uint32_t index, const v8::AccessorInfo& info);
static v8::Handle<v8::Value> VimListSet(uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info);
static v8::Handle<v8::Boolean> VimListDelete(uint32_t index, const v8::AccessorInfo& info);
static v8::Handle<v8::Array> VimListEnumerate(const v8::AccessorInfo& info);
static v8::Handle<v8::Value> VimListLength(v8::Local<v8::String> property, const v8::AccessorInfo& info);
static v8::Handle<v8::Value> VimDictCreate(const v8::Arguments& args);
static void VimDictDestroy(v8::Persistent<v8::Value> object, void* parameter);
static v8::Handle<v8::Value> VimDictGet(v8::Local<v8::String> property, const v8::AccessorInfo& info);
static v8::Handle<v8::Value> VimDictSet(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::AccessorInfo& info);
static v8::Handle<v8::Boolean> VimDictDelete(v8::Local<v8::String> property, const v8::AccessorInfo& info);
static v8::Handle<v8::Array> VimDictEnumerate(const v8::AccessorInfo& info);
static v8::Handle<v8::Value> Load(const v8::Arguments& args);
static v8::Handle<v8::String> ReadFile(const char* name);
static bool ExecuteString(v8::Handle<v8::String> source, v8::Handle<v8::Value> name, std::string* err);

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
  if (eval_expr == NULL)
    return "not initialized";
  v8::HandleScope handle_scope;
  v8::Context::Scope context_scope(context);
  std::string err;
  if (!ExecuteString(v8::String::New(expr), v8::Undefined(), &err))
    emsg((char_u*)err.c_str());
  return NULL;
}

#define GETSYMBOL(name) GETSYMBOL2(name, #name)

#ifdef WIN32
#define GETSYMBOL2(var, name)                                             \
  do {                                                                    \
    void **p = (void **)&var;                                             \
    if ((*p = (void*)GetProcAddress((HMODULE)vim_handle, name)) == NULL)  \
      return "error: GetProcAddress()";                                   \
  } while (0)
#else
#define GETSYMBOL2(var, name)                     \
  do {                                            \
    void **p = (void **)&var;                     \
    if ((*p = dlsym(vim_handle, name)) == NULL)   \
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
  // variables
  GETSYMBOL2(p_hash_removed, "hash_removed");
  // functions
  GETSYMBOL(eval_expr);
  GETSYMBOL(eval_to_string);
  GETSYMBOL(do_cmdline_cmd);
  GETSYMBOL(free_tv);
  GETSYMBOL(clear_tv);
  GETSYMBOL(emsg);
  GETSYMBOL(alloc);
  GETSYMBOL(vim_free);
  GETSYMBOL(vim_strsave);
  GETSYMBOL(vim_strnsave);
  GETSYMBOL(vim_strncpy);
  GETSYMBOL(list_alloc);
  GETSYMBOL(list_free);
  GETSYMBOL(list_unref);
  GETSYMBOL(dict_alloc);
  GETSYMBOL(hash_add);
  GETSYMBOL(hash_find);
  GETSYMBOL(hash_remove);
  return NULL;
}

static const char *
init_v8()
{
  v8::HandleScope handle_scope;

  VimList = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(VimListCreate));
  VimList->SetClassName(v8::String::New("VimList"));
  VimListTemplate = v8::Persistent<v8::ObjectTemplate>::New(VimList->InstanceTemplate());
  VimListTemplate->SetInternalFieldCount(1);
  VimListTemplate->SetIndexedPropertyHandler(VimListGet, VimListSet, NULL, VimListDelete, VimListEnumerate);
  VimListTemplate->SetAccessor(v8::String::New("length"), VimListLength, NULL, v8::Handle<v8::Value>(), v8::DEFAULT, (v8::PropertyAttribute)(v8::DontEnum|v8::DontDelete));

  VimDict = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(VimDictCreate));
  VimDict->SetClassName(v8::String::New("VimDict"));
  VimDictTemplate = v8::Persistent<v8::ObjectTemplate>::New(VimDict->InstanceTemplate());
  VimDictTemplate->SetInternalFieldCount(1);
  VimDictTemplate->SetNamedPropertyHandler(VimDictGet, VimDictSet, NULL, VimDictDelete, VimDictEnumerate);

  v8::Handle<v8::ObjectTemplate> internal = v8::ObjectTemplate::New();
  internal->Set(v8::String::New("load"), v8::FunctionTemplate::New(Load));
  internal->Set(v8::String::New("vim_execute"), v8::FunctionTemplate::New(vim_execute));
  internal->Set(v8::String::New("VimList"), VimList);
  internal->Set(v8::String::New("VimDict"), VimDict);

  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
  global->Set(v8::String::New("%internal%"), internal);

  context = v8::Context::New(NULL, global);

  return NULL;
}

static v8::Handle<v8::Value>
vim_execute(const v8::Arguments& args)
{
  v8::HandleScope handle_scope;

  if (args.Length() <= 1 || !args[0]->IsString())
    return v8::ThrowException(v8::String::New("usage: vim_execute(string cmd [, ...])"));

  v8::Handle<v8::Array> v8_args = v8::Array::New(args.Length());
  for (int i = 0; i < args.Length(); ++i )
    v8_args->Set(v8::Integer::New(i), args[i]);

  // :let g:['%v8_args%'] = args
  {
    LookupMap lookup;
    std::string err;
    typval_T vimobj;
    if (!v8_to_vim(v8_args, &vimobj, 1, &lookup, &err)) {
      return v8::ThrowException(v8::String::New(err.c_str()));
    }

    char expr[] = "g:";
    typval_T *g = eval_expr((char_u*)expr, NULL);
    if (g == NULL) {
      clear_tv(&vimobj);
      return v8::ThrowException(v8::String::New("vim_execute(): error eval_expr()"));
    }

    if (!dict_set(g->vval.v_dict, "%v8_args%", &vimobj)) {
      clear_tv(&vimobj);
      free_tv(g);
      return v8::ThrowException(v8::String::New("vim_execute(): error dict_set()"));
    }

    free_tv(g);

    // don't clear
    // clear_tv(&vimobj);
  }

  // execute
  {
    char exprbuf[] = "try | execute g:['%v8_args%'][0] | let g:['%v8_exception%'] = '' | catch | let g:['%v8_exception%'] = v:exception | endtry";
    if (!do_cmdline_cmd((char_u*)exprbuf)) {
      return v8::ThrowException(v8::String::New("vim_call(): error do_cmdline_cmd()"));
    }
  }

  // catch
  {
    char exprbuf[] = "g:['%v8_exception%']";
    char_u *e = eval_to_string((char_u*)exprbuf, NULL, 0);
    if (e == NULL) {
      return v8::ThrowException(v8::String::New("vim_call(): cannot get exception"));
    }

    if (e[0] != '\0') {
      std::string s((char*)e);
      vim_free(e);
      return v8::ThrowException(v8::String::New(s.c_str()));
    }

    vim_free(e);
  }

  // return g:['%v8_result']
  v8::Handle<v8::Value> v8_result;
  {
    char exprbuf[] = "g:['%v8_result%']";
    typval_T *tv = eval_expr((char_u*)exprbuf, NULL);
    if (tv == NULL) {
      return v8::ThrowException(v8::String::New("vim_call(): cannot get result"));
    }

    LookupMap lookup;
    std::string err;
    if (!vim_to_v8(tv, &v8_result, 1, &lookup, true, &err)) {
      free_tv(tv);
      return v8::ThrowException(v8::String::New(err.c_str()));
    }

    free_tv(tv);
  }

  return v8_result;
}

static bool
vim_to_v8(typval_T *vimobj, v8::Handle<v8::Value> *v8obj, int depth, LookupMap *lookup, bool wrap, std::string *err)
{
  if (vimobj == NULL || depth > 100) {
    *v8obj = v8::Undefined();
    return true;
  }

  if (vimobj->v_type == VAR_NUMBER) {
    *v8obj = v8::Integer::New(vimobj->vval.v_number);
    return true;
  }

#ifdef FEAT_FLOAT
  if (vimobj->v_type == VAR_FLOAT) {
    *v8obj = v8::Number::New(vimobj->vval.v_float);
    return true;
  }
#endif

  if (vimobj->v_type == VAR_STRING) {
    if (vimobj->vval.v_string == NULL)
      *v8obj = v8::String::New("");
    else
      *v8obj = v8::String::New((char *)vimobj->vval.v_string);
    return true;
  }

  if (vimobj->v_type == VAR_LIST) {
    list_T *list = vimobj->vval.v_list;
    if (list == NULL) {
      *v8obj = v8::Array::New(0);
      return true;
    }
    LookupMap::iterator it = lookup->find(list);
    if (it != lookup->end()) {
      *v8obj = it->second;
      return true;
    }
    it = objcache.find(list);
    if (it != objcache.end()) {
      *v8obj = it->second;
      return true;
    }
    if (wrap) {
      v8::Persistent<v8::Object> o = v8::Persistent<v8::Object>::New(VimListTemplate->NewInstance());
      o.MakeWeak(list, VimListDestroy);
      o->SetInternalField(0, v8::External::New(list));
      lookup->insert(LookupMap::value_type(list, o));
      objcache.insert(LookupMap::value_type(list, o));
      ++list->lv_refcount;
      *v8obj = o;
    } else {
      v8::Handle<v8::Array> o = v8::Array::New(0);
      v8::Handle<v8::Value> v;
      int i = 0;
      lookup->insert(LookupMap::value_type(list, o));
      for (listitem_T *curr = list->lv_first; curr != NULL; curr = curr->li_next) {
        if (!vim_to_v8(&curr->li_tv, &v, depth + 1, lookup, wrap, err))
          return false;
        o->Set(v8::Integer::New(i++), v);
      }
      *v8obj = o;
    }
    return true;
  }

  if (vimobj->v_type == VAR_DICT) {
    dict_T *dict = vimobj->vval.v_dict;
    if (dict == NULL) {
      *v8obj = v8::Object::New();
      return true;
    }
    LookupMap::iterator it = lookup->find(dict);
    if (it != lookup->end()) {
      *v8obj = it->second;
      return true;
    }
    it = objcache.find(dict);
    if (it != objcache.end()) {
      *v8obj = it->second;
      return true;
    }
    if (wrap) {
      v8::Persistent<v8::Object> o = v8::Persistent<v8::Object>::New(VimDictTemplate->NewInstance());
      o.MakeWeak(dict, VimDictDestroy);
      o->SetInternalField(0, v8::External::New(dict));
      lookup->insert(LookupMap::value_type(dict, o));
      objcache.insert(LookupMap::value_type(dict, o));
      ++dict->dv_refcount;
      *v8obj = o;
    } else {
      v8::Handle<v8::Object> o = v8::Object::New();
      v8::Handle<v8::Value> v;
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
          o->Set(v8::String::New((char *)hi->hi_key), v);
        }
      }
      *v8obj = o;
    }
    return true;
  }

  // TODO:
  if (vimobj->v_type == VAR_FUNC) {
    if (vimobj->vval.v_string == NULL)
      *v8obj = v8::String::New("");
    else
      *v8obj = v8::String::New((char *)vimobj->vval.v_string);
    return true;
  }

  if (vimobj->v_type == VAR_UNKNOWN) {
    *v8obj = v8::Undefined();
    return true;
  }

  *err = "vim_to_v8(): internal error: unknown type";
  return false;
}

static LookupMap::iterator
LookupMapFindV8Value(LookupMap* lookup, v8::Handle<v8::Value> v)
{
  LookupMap::iterator it;
  for (it = lookup->begin(); it != lookup->end(); ++it) {
    if (it->second == v)
      return it;
  }
  return it;
}

static bool
v8_to_vim(v8::Handle<v8::Value> v8obj, typval_T *vimobj, int depth, LookupMap *lookup, std::string *err)
{
  vimobj->v_lock = 0;

  if (depth > 100) {
    vimobj->v_type = VAR_NUMBER;
    vimobj->vval.v_number = 0;
    return true;
  }

  if (VimList->HasInstance(v8obj)) {
    v8::Handle<v8::Object> o = v8::Handle<v8::Object>::Cast(v8obj);
    v8::Handle<v8::External> external = v8::Handle<v8::External>::Cast(o->GetInternalField(0));
    list_T *list = static_cast<list_T*>(external->Value());
    vimobj->v_type = VAR_LIST;
    vimobj->vval.v_list = list;
    ++vimobj->vval.v_list->lv_refcount;
    return true;
  }

  if (VimDict->HasInstance(v8obj)) {
    v8::Handle<v8::Object> o = v8::Handle<v8::Object>::Cast(v8obj);
    v8::Handle<v8::External> external = v8::Handle<v8::External>::Cast(o->GetInternalField(0));
    dict_T *dict = static_cast<dict_T*>(external->Value());
    vimobj->v_type = VAR_DICT;
    vimobj->vval.v_dict = dict;
    ++vimobj->vval.v_dict->dv_refcount;
    return true;
  }

  if (v8obj->IsUndefined()) {
    vimobj->v_type = VAR_NUMBER;
    vimobj->vval.v_number = 0;
    return true;
  }

  if (v8obj->IsNull()) {
    vimobj->v_type = VAR_NUMBER;
    vimobj->vval.v_number = 0;
    return true;
  }

  if (v8obj->IsBoolean()) {
    vimobj->v_type = VAR_NUMBER;
    vimobj->vval.v_number = v8obj->IsTrue() ? 1 : 0;
    return true;
  }

  if (v8obj->IsInt32()) {
    vimobj->v_type = VAR_NUMBER;
    vimobj->vval.v_number = v8obj->Int32Value();
    return true;
  }

#ifdef FEAT_FLOAT
  if (v8obj->IsNumber()) {
    vimobj->v_type = VAR_FLOAT;
    vimobj->vval.v_float = v8obj->NumberValue();
    return true;
  }
#endif

  if (v8obj->IsString()) {
    v8::String::Utf8Value str(v8obj);
    vimobj->v_type = VAR_STRING;
    vimobj->vval.v_string = vim_strsave((char_u*)*str);
    return true;
  }

  if (v8obj->IsDate()) {
    v8::String::Utf8Value str(v8obj);
    vimobj->v_type = VAR_STRING;
    vimobj->vval.v_string = vim_strsave((char_u*)*str);
    return true;
  }

  if (v8obj->IsArray()
      // also convert array like object, such as arguments, to array?
      //|| (v8obj->IsObject() && v8::Handle<v8::Object>::Cast(v8obj)->Has(v8::String::New("length")))
      ) {
    LookupMap::iterator it = LookupMapFindV8Value(lookup, v8obj);
    if (it != lookup->end()) {
      vimobj->v_type = VAR_LIST;
      vimobj->vval.v_list = (list_T *)it->first;
      ++vimobj->vval.v_list->lv_refcount;
      return true;
    }
    list_T *list = list_alloc();
    if (list == NULL) {
      *err = "v8_to_vim(): list_alloc(): out of memoty";
      return false;
    }
    v8::Handle<v8::Object> o = v8::Handle<v8::Object>::Cast(v8obj);
    uint32_t len;
    if (o->IsArray()) {
      v8::Handle<v8::Array> o = v8::Handle<v8::Array>::Cast(v8obj);
      len = o->Length();
    } else {
      len = o->Get(v8::String::New("length"))->Int32Value();
    }
    lookup->insert(LookupMap::value_type(list, v8obj));
    for (uint32_t i = 0; i < len; ++i) {
      v8::Handle<v8::Value> v = o->Get(v8::Integer::New(i));
      listitem_T *li = listitem_alloc();
      if (li == NULL) {
        list_free(list, TRUE);
        *err = "v8_to_vim(): listitem_alloc(): out of memoty";
        return false;
      }
      init_tv(&li->li_tv);
      if (!v8_to_vim(v, &li->li_tv, depth + 1, lookup, err)) {
        listitem_free(li);
        list_free(list, TRUE);
        return false;
      }
      list_append(list, li);
    }
    vimobj->v_type = VAR_LIST;
    vimobj->vval.v_list = list;
    ++vimobj->vval.v_list->lv_refcount;
    return true;
  }

  if (v8obj->IsObject()) {
    LookupMap::iterator it = LookupMapFindV8Value(lookup, v8obj);
    if (it != lookup->end()) {
      vimobj->v_type = VAR_DICT;
      vimobj->vval.v_dict = (dict_T *)it->first;
      ++vimobj->vval.v_dict->dv_refcount;
      return true;
    }
    dict_T *dict = dict_alloc();
    if (dict == NULL) {
      *err = "v8_to_vim(): dict_alloc(): out of memory";
      return false;
    }
    v8::Handle<v8::Object> o = v8::Handle<v8::Object>::Cast(v8obj);
    v8::Handle<v8::Array> keys = o->GetPropertyNames();
    uint32_t len = keys->Length();
    lookup->insert(LookupMap::value_type(dict, v8obj));
    for (uint32_t i = 0; i < len; ++i) {
      v8::Handle<v8::Value> key = keys->Get(v8::Integer::New(i));
      v8::Handle<v8::Value> v = o->Get(key);
      v8::String::Utf8Value keystr(key);
      if (keystr.length() == 0) {
        dict_free(dict, TRUE);
        *err = "v8_to_vim(): Cannot use empty key for Dictionary";
        return false;
      }
      dictitem_T *di = dictitem_alloc((char_u*)*keystr);
      if (di == NULL) {
        dict_free(dict, TRUE);
        *err = "v8_to_vim(): dictitem_alloc(): out of memory";
        return false;
      }
      init_tv(&di->di_tv);
      if (!v8_to_vim(v, &di->di_tv, depth + 1, lookup, err)) {
        dictitem_free(di);
        dict_free(dict, TRUE);
        return false;
      }
      if (!dict_add(dict, di)) {
        dictitem_free(di);
        dict_free(dict, TRUE);
        *err = "v8_to_vim(): error dict_add()";
        return false;
      }
    }
    vimobj->v_type = VAR_DICT;
    vimobj->vval.v_dict = dict;
    ++vimobj->vval.v_dict->dv_refcount;
    return true;
  }

  if (v8obj->IsFunction()) {
    vimobj->v_type = VAR_STRING;
    vimobj->vval.v_string = vim_strsave((char_u*)"[function]");
    return true;
  }

  if (v8obj->IsExternal()) {
    vimobj->v_type = VAR_STRING;
    vimobj->vval.v_string = vim_strsave((char_u*)"[external]");
    return true;
  }

  *err = "v8_to_vim(): internal error: unknown type";
  return true;
}

static v8::Handle<v8::Value>
VimListCreate(const v8::Arguments& args)
{
  if (!args.IsConstructCall())
    return v8::ThrowException(v8::String::New("Cannot call constructor as function"));

  list_T *list = list_alloc();
  if (list == NULL)
    return v8::ThrowException(v8::String::New("VimListCreate(): list_alloc(): out of memory"));

  v8::Persistent<v8::Object> self = v8::Persistent<v8::Object>::New(args.Holder());
  self.MakeWeak(list, VimListDestroy);
  self->SetInternalField(0, v8::External::New(list));
  objcache.insert(LookupMap::value_type(list, self));
  return self;
}

static void
VimListDestroy(v8::Persistent<v8::Value> object, void* parameter)
{
  objcache.erase(parameter);
  list_unref(static_cast<list_T*>(parameter));
}

static v8::Handle<v8::Value>
VimListGet(uint32_t index, const v8::AccessorInfo& info)
{
  v8::Handle<v8::Object> self = info.Holder();
  v8::Handle<v8::External> external = v8::Handle<v8::External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  listitem_T *li = list_find(list, index);
  if (li == NULL)
    return v8::Undefined();
  LookupMap lookup;
  std::string err;
  v8::Handle<v8::Value> v8obj;
  if (!vim_to_v8(&li->li_tv, &v8obj, 1, &lookup, true, &err))
    return v8::ThrowException(v8::String::New(err.c_str()));
  return v8obj;
}

static v8::Handle<v8::Value>
VimListSet(uint32_t index, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
  v8::Handle<v8::Object> self = info.Holder();
  v8::Handle<v8::External> external = v8::Handle<v8::External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  listitem_T *li = list_find(list, index);
  if (li == NULL)
    return v8::ThrowException(v8::String::New("list index out of range"));
  LookupMap lookup;
  std::string err;
  typval_T vimobj;
  if (!v8_to_vim(value, &vimobj, 1, &lookup, &err))
    return v8::ThrowException(v8::String::New(err.c_str()));
  clear_tv(&li->li_tv);
  li->li_tv = vimobj;
  return value;
}

static v8::Handle<v8::Boolean>
VimListDelete(uint32_t index, const v8::AccessorInfo& info)
{
  v8::Handle<v8::Object> self = info.Holder();
  v8::Handle<v8::External> external = v8::Handle<v8::External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  listitem_T *li = list_find(list, index);
  if (li == NULL)
    return v8::False();
  list_remove(list, li, li);
  listitem_free(li);
  return v8::True();
}

static v8::Handle<v8::Array>
VimListEnumerate(const v8::AccessorInfo& info)
{
  v8::Handle<v8::Object> self = info.Holder();
  v8::Handle<v8::External> external = v8::Handle<v8::External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  uint32_t len = list_len(list);
  v8::Handle<v8::Array> keys = v8::Array::New(len);
  for (uint32_t i = 0; i < len; ++i) {
    keys->Set(v8::Integer::New(i), v8::Integer::New(i));
  }
  return keys;
}

static v8::Handle<v8::Value>
VimListLength(v8::Local<v8::String> property, const v8::AccessorInfo& info)
{
  v8::Handle<v8::Object> self = info.Holder();
  v8::Handle<v8::External> external = v8::Handle<v8::External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  uint32_t len = list_len(list);
  return v8::Integer::New(len);
}

static v8::Handle<v8::Value>
VimDictCreate(const v8::Arguments& args)
{
  if (!args.IsConstructCall())
    return v8::ThrowException(v8::String::New("Cannot call constructor as function"));

  dict_T *dict = dict_alloc();
  if (dict == NULL)
    return v8::ThrowException(v8::String::New("VimDictCreate(): dict_alloc(): out of memory"));

  v8::Persistent<v8::Object> self = v8::Persistent<v8::Object>::New(args.Holder());
  self.MakeWeak(dict, VimDictDestroy);
  self->SetInternalField(0, v8::External::New(dict));
  objcache.insert(LookupMap::value_type(dict, self));
  return self;
}

static void
VimDictDestroy(v8::Persistent<v8::Value> object, void* parameter)
{
  objcache.erase(parameter);
  dict_unref(static_cast<dict_T*>(parameter));
}

static v8::Handle<v8::Value>
VimDictGet(v8::Local<v8::String> property, const v8::AccessorInfo& info)
{
  if (property->Length() == 0)
    return v8::ThrowException(v8::String::New("Cannot use empty key for Dictionary"));
  v8::Handle<v8::Object> self = info.Holder();
  v8::Handle<v8::External> external = v8::Handle<v8::External>::Cast(self->GetInternalField(0));
  v8::String::Utf8Value key(property);
  dict_T *dict = static_cast<dict_T*>(external->Value());
  dictitem_T *di = dict_find(dict, (char_u*)*key, -1);
  if (di == NULL) {
    // fallback to prototype.  otherwise String(obj) don't work due to
    // lack of toString().
    v8::Handle<v8::Object> prototype = v8::Handle<v8::Object>::Cast(self->GetPrototype());
    return prototype->Get(property);
  }
  LookupMap lookup;
  std::string err;
  v8::Handle<v8::Value> v8obj;
  if (!vim_to_v8(&di->di_tv, &v8obj, 1, &lookup, true, &err))
    return v8::ThrowException(v8::String::New(err.c_str()));
  return v8obj;
}

static v8::Handle<v8::Value>
VimDictSet(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
  if (property->Length() == 0)
    return v8::ThrowException(v8::String::New("Cannot use empty key for Dictionary"));
  v8::Handle<v8::Object> self = info.Holder();
  v8::Handle<v8::External> external = v8::Handle<v8::External>::Cast(self->GetInternalField(0));
  v8::String::Utf8Value key(property);
  dict_T *dict = static_cast<dict_T*>(external->Value());
  LookupMap lookup;
  std::string err;
  typval_T vimobj;
  if (!v8_to_vim(value, &vimobj, 1, &lookup, &err))
    return v8::ThrowException(v8::String::New(err.c_str()));
  if (!dict_set(dict, *key, &vimobj))
    return v8::ThrowException(v8::String::New("error dict_set()"));
  return value;
}

static v8::Handle<v8::Boolean>
VimDictDelete(v8::Local<v8::String> property, const v8::AccessorInfo& info)
{
  if (property->Length() == 0)
    return v8::False();
  v8::Handle<v8::Object> self = info.Holder();
  v8::Handle<v8::External> external = v8::Handle<v8::External>::Cast(self->GetInternalField(0));
  v8::String::Utf8Value key(property);
  dict_T *dict = static_cast<dict_T*>(external->Value());
  dictitem_T *di = dict_find(dict, (char_u*)*key, -1);
  if (di == NULL)
    return v8::False();
  dictitem_remove(dict, di);
  return v8::True();
}

static v8::Handle<v8::Array>
VimDictEnumerate(const v8::AccessorInfo& info)
{
  v8::Handle<v8::Object> self = info.Holder();
  v8::Handle<v8::External> external = v8::Handle<v8::External>::Cast(self->GetInternalField(0));
  dict_T *dict = static_cast<dict_T*>(external->Value());
  hashtab_T *ht = &dict->dv_hashtab;
  long_u todo = ht->ht_used;
  hashitem_T *hi;
  int i = 0;
  v8::Handle<v8::Array> keys = v8::Array::New(todo);
  for (hi = ht->ht_array; todo > 0; ++hi) {
    if (!HASHITEM_EMPTY(hi)) {
      --todo;
      keys->Set(v8::Integer::New(i++), v8::String::New((char *)hi->hi_key));
    }
  }
  return keys;
}

// The callback that is invoked by v8 whenever the JavaScript 'load'
// function is called.  Loads, compiles and executes its argument
// JavaScript file.
v8::Handle<v8::Value> Load(const v8::Arguments& args) {
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope;
    v8::String::Utf8Value file(args[i]);
    v8::Handle<v8::String> source = ReadFile(*file);
    if (source.IsEmpty()) {
      return v8::ThrowException(v8::String::New("Error loading file"));
    }
    std::string err;
    if (!ExecuteString(source, v8::String::New(*file), &err)) {
      return v8::ThrowException(v8::String::New(err.c_str()));
    }
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

bool ExecuteString(v8::Handle<v8::String> source,
                   v8::Handle<v8::Value> name,
                   std::string* err) {
  v8::HandleScope handle_scope;
  v8::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);
  if (script.IsEmpty()) {
    if (err != NULL) {
      v8::String::Utf8Value error(try_catch.Exception());
      *err = *error;
    }
    return false;
  }
  v8::Handle<v8::Value> result = script->Run();
  if (result.IsEmpty()) {
    if (err != NULL) {
      v8::String::Utf8Value error(try_catch.Exception());
      *err = *error;
    }
    return false;
  }
  return true;
}
