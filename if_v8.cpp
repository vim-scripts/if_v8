/* vim:set foldmethod=marker:
 *
 * v8 interface to Vim
 *
 * Last Change: 2009-01-17
 * Maintainer: Yukihiro Nakadaira <yukihiro.nakadaira@gmail.com>
 */
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <map>
#include <v8.h>

#ifdef WIN32
# include <windows.h>
# define DLLEXPORT __declspec(dllexport)
# define DLLIMPORT extern __declspec(dllimport)
# define DLLOPEN(path) (void*)LoadLibrary(path)
#else
# include <dlfcn.h>
# define DLLEXPORT
# define DLLIMPORT extern
# define DLLOPEN(path) dlopen(path, RTLD_NOW)
#endif


// Vim {{{
// header {{{2
#define FEAT_FLOAT 1

#define FALSE 0
#define TRUE 1

#define STRLEN(s)	    strlen((char *)(s))
#define STRCPY(d, s)	    strcpy((char *)(d), (char *)(s))
#define vim_memset(ptr, c, size)   memset((ptr), (c), (size))

typedef unsigned char char_u;
typedef unsigned short short_u;
typedef unsigned int int_u;
/* Make sure long_u is big enough to hold a pointer.
 * On Win64, longs are 32 bits and pointers are 64 bits.
 * For printf() and scanf(), we need to take care of long_u specifically. */
#ifdef _WIN64
typedef unsigned __int64        long_u;
typedef		 __int64        long_i;
# define SCANF_HEX_LONG_U       "%Ix"
# define SCANF_DECIMAL_LONG_U   "%Iu"
# define PRINTF_HEX_LONG_U      "0x%Ix"
#else
  /* Microsoft-specific. The __w64 keyword should be specified on any typedefs
   * that change size between 32-bit and 64-bit platforms.  For any such type,
   * __w64 should appear only on the 32-bit definition of the typedef.
   * Define __w64 as an empty token for everything but MSVC 7.x or later.
   */
# if !defined(_MSC_VER)	|| (_MSC_VER < 1300)
#  define __w64
# endif
typedef unsigned long __w64	long_u;
typedef		 long __w64     long_i;
# define SCANF_HEX_LONG_U       "%lx"
# define SCANF_DECIMAL_LONG_U   "%lu"
# define PRINTF_HEX_LONG_U      "0x%lx"
#endif

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
#define HI_KEY_REMOVED &hash_removed
#define HASHITEM_EMPTY(hi) ((hi)->hi_key == NULL || (hi)->hi_key == &hash_removed)

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

// functions {{{2
static const char *init_vim();
// vim's static function
static void init_tv(typval_T *varp);
// List
static listitem_T *listitem_alloc();
static void listitem_free(listitem_T *item);
static listitem_T *list_find(list_T *l, long n);
static void list_append(list_T *l, listitem_T *item);
static void list_fix_watch(list_T *l, listitem_T *item);
static void list_remove(list_T *l, listitem_T *item, listitem_T *item2);
static long list_len(list_T *l);
// Dictionary
static dictitem_T *dictitem_alloc(char_u *key);
static void dictitem_free(dictitem_T *item);
static void dictitem_remove(dict_T *dict, dictitem_T *item);
static int dict_add(dict_T *d, dictitem_T *item);
// XXX: recurse is not supported (always TRUE)
#define dict_free(d, recurse) wrap_dict_free(d)
static void wrap_dict_free(dict_T *d);
static dictitem_T *dict_find(dict_T *d, char_u *key, int len);
// Funcref
static void func_ref(char_u *name);
// not in vim
static void tv_set_number(typval_T *tv, varnumber_T number);
#ifdef FEAT_FLOAT
static void tv_set_float(typval_T *tv, float_T v_float);
#endif
static void tv_set_string(typval_T *tv, char_u *string);
static void tv_set_list(typval_T *tv, list_T *list);
static void tv_set_dict(typval_T *tv, dict_T *dict);
static void tv_set_func(typval_T *tv, char_u *name);
static bool dict_set(dict_T *dict, char_u *key, typval_T *tv);

// import {{{2

extern "C" {
// variables
DLLIMPORT char_u hash_removed;
// functions
DLLIMPORT typval_T *eval_expr(char_u *arg, char_u **nextcmd);
DLLIMPORT int do_cmdline_cmd(char_u *cmd);
DLLIMPORT void free_tv(typval_T *varp);
DLLIMPORT void clear_tv(typval_T *varp);
DLLIMPORT int emsg(char_u *s);
DLLIMPORT char_u *alloc(unsigned size);
DLLIMPORT void vim_free(void *x);
DLLIMPORT char_u *vim_strsave(char_u *string);
DLLIMPORT char_u *vim_strnsave(char_u *string, int len);
DLLIMPORT void vim_strncpy(char_u *to, char_u *from, size_t len);
DLLIMPORT list_T *list_alloc();
DLLIMPORT void list_free(list_T *l, int recurse);
DLLIMPORT dict_T *dict_alloc();
DLLIMPORT int hash_add(hashtab_T *ht, char_u *key);
DLLIMPORT hashitem_T *hash_find(hashtab_T *ht, char_u *key);
DLLIMPORT void hash_remove(hashtab_T *ht, hashitem_T *hi);
DLLIMPORT int vim_snprintf(char *str, size_t str_m, char *fmt, ...);
}

static dict_T *p_globvardict;
static dict_T *p_vimvardict;

#define globvardict (*p_globvardict)
#define vimvardict (*p_vimvardict)

static const char *
init_vim()
{
  typval_T *tv;
  char exprbuf[3];

  strcpy(exprbuf, "g:");
  tv = eval_expr((char_u*)exprbuf, NULL);
  if (tv == NULL)
    return "error: cannot get g:";
  p_globvardict = tv->vval.v_dict;
  free_tv(tv);

  strcpy(exprbuf, "v:");
  tv = eval_expr((char_u*)exprbuf, NULL);
  if (tv == NULL)
    return "error: cannot get v:";
  p_vimvardict = tv->vval.v_dict;
  free_tv(tv);

  return NULL;
}

// {{{2

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
 */
    static void
wrap_dict_free(dict_T *d)
{
    typval_T tv;
    init_tv(&tv);
    tv.v_type = VAR_DICT;
    tv.vval.v_dict = d;
    tv.vval.v_dict->dv_refcount = 1;
    clear_tv(&tv);
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

// Funcref {{{2

/*
 * Count a reference to a Function.
 * XXX: WORKAROUND
 * XXX: wish no error
 */
    static void
func_ref(
    char_u	*name
    )
{
  typval_T tv;
  dictitem_T *di;
  hashitem_T *hi;
  char exprbuf[] = "let v:['%v8_func2%'] = v:['%v8_func1%']";
  tv.v_type = VAR_FUNC;
  tv.v_lock = 0;
  tv.vval.v_string = name;
  dict_set(&vimvardict, (char_u*)"%v8_func1%", &tv);
  do_cmdline_cmd((char_u*)exprbuf);
  hi = hash_find(&vimvardict.dv_hashtab, (char_u*)"%v8_func1%");
  di = HI2DI(hi);
  hash_remove(&vimvardict.dv_hashtab, hi);
  vim_free(di);
  hi = hash_find(&vimvardict.dv_hashtab, (char_u*)"%v8_func2%");
  di = HI2DI(hi);
  hash_remove(&vimvardict.dv_hashtab, hi);
  vim_free(di);
}

// not in vim {{{2

static void
tv_set_number(typval_T *tv, varnumber_T number)
{
  tv->v_type = VAR_NUMBER;
  tv->v_lock = 0;
  tv->vval.v_number = number;
}

#ifdef FEAT_FLOAT
static void
tv_set_float(typval_T *tv, float_T v_float)
{
  tv->v_type = VAR_FLOAT;
  tv->v_lock = 0;
  tv->vval.v_float = v_float;
}
#endif

static void
tv_set_string(typval_T *tv, char_u *string)
{
  tv->v_type = VAR_STRING;
  tv->v_lock = 0;
  tv->vval.v_string = vim_strsave(string);
}

static void
tv_set_list(typval_T *tv, list_T *list)
{
  tv->v_type = VAR_LIST;
  tv->v_lock = 0;
  tv->vval.v_list = list;
  ++list->lv_refcount;
}

static void
tv_set_dict(typval_T *tv, dict_T *dict)
{
  tv->v_type = VAR_DICT;
  tv->v_lock = 0;
  tv->vval.v_dict = dict;
  ++dict->dv_refcount;
}

static void
tv_set_func(typval_T *tv, char_u *name)
{
  tv->v_type = VAR_FUNC;
  tv->v_lock = 0;
  tv->vval.v_string = vim_strsave(name);
  func_ref(name);
}

/*
 * let dict[key] = tv without incrementing reference count.
 */
static bool
dict_set(dict_T *dict, char_u *key, typval_T *tv)
{
  dictitem_T *di = dict_find(dict, key, -1);
  if (di == NULL) {
    di = dictitem_alloc(key);
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

/* API */
extern "C" {
DLLEXPORT const char *init(const char *dll_path);
DLLEXPORT const char *execute(const char *expr);
}

static const char *init_v8();

static bool vim_to_v8(typval_T *vimobj, Handle<Value> *v8obj, int depth, LookupMap *lookup, bool wrap, std::string *err);
static bool v8_to_vim(Handle<Value> v8obj, typval_T *vimobj, int depth, LookupMap *lookup, std::string *err);
static LookupMap::iterator LookupMapFindV8Value(LookupMap* lookup, Handle<Value> v);

static void weak_ref(void *p, typval_T *tv);
static void weak_unref(void *p);

static Handle<String> ReadFile(const char* name);
static bool ExecuteString(Handle<String> source, Handle<Value> name, bool print_result, bool report_exceptions, std::string& err);
static void ReportException(TryCatch* try_catch);

// functions
static Handle<Value> vim_execute(const Arguments& args);
static Handle<Value> Load(const Arguments& args);

// VimList
static Handle<Value> MakeVimList(list_T *list, Handle<Object> obj);
static Handle<Value> VimListCreate(const Arguments& args);
static void VimListDestroy(Persistent<Value> object, void* parameter);
static Handle<Value> VimListGet(uint32_t index, const AccessorInfo& info);
static Handle<Value> VimListSet(uint32_t index, Local<Value> value, const AccessorInfo& info);
static Handle<Boolean> VimListQuery(uint32_t index, const AccessorInfo& info);
static Handle<Boolean> VimListDelete(uint32_t index, const AccessorInfo& info);
static Handle<Array> VimListEnumerate(const AccessorInfo& info);
static Handle<Value> VimListLength(Local<String> property, const AccessorInfo& info);

// VimDict
static Handle<Value> MakeVimDict(dict_T *dict, Handle<Object> obj);
static void VimDictDestroy(Persistent<Value> object, void* parameter);
static Handle<Value> VimDictCreate(const Arguments& args);
static Handle<Value> VimDictIdxGet(uint32_t index, const AccessorInfo& info);
static Handle<Value> VimDictIdxSet(uint32_t index, Local<Value> value, const AccessorInfo& info);
static Handle<Boolean> VimDictIdxQuery(uint32_t index, const AccessorInfo& info);
static Handle<Boolean> VimDictIdxDelete(uint32_t index, const AccessorInfo& info);
static Handle<Value> VimDictGet(Local<String> property, const AccessorInfo& info);
static Handle<Value> VimDictSet(Local<String> property, Local<Value> value, const AccessorInfo& info);
static Handle<Boolean> VimDictQuery(Local<String> property, const AccessorInfo& info);
static Handle<Boolean> VimDictDelete(Local<String> property, const AccessorInfo& info);
static Handle<Array> VimDictEnumerate(const AccessorInfo& info);

// VimFunc
static Handle<Value> MakeVimFunc(const char *name, Handle<Object> obj);
static void VimFuncDestroy(Persistent<Value> object, void* parameter);
static Handle<Value> VimFuncCall(const Arguments& args);

const char *
init(const char *dll_path)
{
  const char *err;
  if (dll_handle != NULL)
    return NULL;
  if ((dll_handle = DLLOPEN(dll_path)) == NULL)
    return "error: cannot load library";
  if ((err = init_vim()) != NULL)
    return err;
  if ((err = init_v8()) != NULL)
    return err;
  return NULL;
}

const char *
execute(const char *expr)
{
  HandleScope handle_scope;
  Context::Scope context_scope(context);
  std::string err;
  if (!ExecuteString(String::New(expr), String::New("(command-line)"), true, true, err))
    emsg((char_u*)err.c_str());
  return NULL;
}

static const char *
init_v8()
{
  HandleScope handle_scope;

  v_weak = dict_alloc();
  if (v_weak == NULL)
    return "init_v8(): error dict_alloc()";
  typval_T tv;
  tv_set_dict(&tv, v_weak);
  dict_set(&vimvardict, (char_u*)"%v8_weak%", &tv);

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
    obj->Set(String::New("g"), MakeVimDict(&globvardict, VimDict->InstanceTemplate()->NewInstance()));
    obj->Set(String::New("v"), MakeVimDict(&vimvardict, VimDict->InstanceTemplate()->NewInstance()));
  }

  return NULL;
}

static bool
vim_to_v8(typval_T *vimobj, Handle<Value> *v8obj, int depth, LookupMap *lookup, bool wrap, std::string *err)
{
  if (vimobj == NULL || depth > 100) {
    *v8obj = Undefined();
    return true;
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
      *v8obj = MakeVimList(list, VimList->InstanceTemplate()->NewInstance());
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
      *v8obj = MakeVimDict(dict, VimDict->InstanceTemplate()->NewInstance());
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

  if (vimobj->v_type == VAR_UNKNOWN) {
    *v8obj = Undefined();
    return true;
  }

  *err = "vim_to_v8(): internal error: unknown type";
  return false;
}

static bool
v8_to_vim(Handle<Value> v8obj, typval_T *vimobj, int depth, LookupMap *lookup, std::string *err)
{
  if (depth > 100) {
    tv_set_number(vimobj, 0);
    return true;
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

  if (v8obj->IsArray()
      // also convert array like object, such as arguments, to array?
      //|| (v8obj->IsObject() && Handle<Object>::Cast(v8obj)->Has(String::New("length")))
      ) {
    LookupMap::iterator it = LookupMapFindV8Value(lookup, v8obj);
    if (it != lookup->end()) {
      tv_set_list(vimobj, (list_T *)it->first);
      return true;
    }
    list_T *list = list_alloc();
    if (list == NULL) {
      *err = "v8_to_vim(): list_alloc(): out of memoty";
      return false;
    }
    Handle<Object> o = Handle<Object>::Cast(v8obj);
    uint32_t len;
    if (o->IsArray()) {
      Handle<Array> o = Handle<Array>::Cast(v8obj);
      len = o->Length();
    } else {
      len = o->Get(String::New("length"))->Int32Value();
    }
    lookup->insert(LookupMap::value_type(list, v8obj));
    for (uint32_t i = 0; i < len; ++i) {
      Handle<Value> v = o->Get(Integer::New(i));
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
    tv_set_list(vimobj, list);
    return true;
  }

  if (v8obj->IsObject()) {
    LookupMap::iterator it = LookupMapFindV8Value(lookup, v8obj);
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

  *err = "v8_to_vim(): internal error: unknown type";
  return true;
}

static LookupMap::iterator
LookupMapFindV8Value(LookupMap* lookup, Handle<Value> v)
{
  LookupMap::iterator it;
  for (it = lookup->begin(); it != lookup->end(); ++it) {
    if (it->second == v)
      return it;
  }
  return it;
}

static void
weak_ref(void *p, typval_T *tv)
{
  char buf[64];
  vim_snprintf(buf, sizeof(buf), (char*)"%p", p);
  dict_set(v_weak, (char_u*)buf, tv);
}

static void
weak_unref(void *p)
{
  char buf[64];
  dictitem_T *di;
  vim_snprintf(buf, sizeof(buf), (char*)"%p", p);
  di = dict_find(v_weak, (char_u*)buf, -1);
  if (di == NULL) {
    emsg((char_u*)"if_v8: weak_unref(): internal error");
    return;
  }
  dictitem_remove(v_weak, di);
}

// Reads a file into a v8 string.
static Handle<String>
ReadFile(const char* name)
{
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
    dict_set(&vimvardict, (char_u*)"%v8_print%", &tv);
  }
  return true;
}

static void
ReportException(TryCatch* try_catch)
{
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
  dict_set(&vimvardict, (char_u*)"%v8_errmsg%", &tv);
}

static Handle<Value>
vim_execute(const Arguments& args)
{
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

static Handle<Value>
MakeVimList(list_T *list, Handle<Object> obj)
{
  typval_T tv;
  tv_set_list(&tv, list);
  weak_ref((void*)list, &tv);

  Persistent<Object> self = Persistent<Object>::New(obj);
  self.MakeWeak(list, VimListDestroy);
  self->SetInternalField(0, External::New(list));
  objcache.insert(LookupMap::value_type(list, self));
  return self;
}

static void
VimListDestroy(Persistent<Value> object, void* parameter)
{
  objcache.erase(parameter);
  weak_unref(parameter);
}

static Handle<Value>
VimListCreate(const Arguments& args)
{
  if (!args.IsConstructCall())
    return ThrowException(String::New("Cannot call constructor as function"));

  list_T *list = list_alloc();
  if (list == NULL)
    return ThrowException(String::New("VimListCreate(): list_alloc(): out of memory"));
  return MakeVimList(list, args.Holder());
}

static Handle<Value>
VimListGet(uint32_t index, const AccessorInfo& info)
{
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
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  list_T *list = static_cast<list_T*>(external->Value());
  uint32_t len = list_len(list);
  return Integer::New(len);
}

static Handle<Value>
MakeVimDict(dict_T *dict, Handle<Object> obj)
{
  typval_T tv;
  tv_set_dict(&tv, dict);
  weak_ref((void*)dict, &tv);

  Persistent<Object> self = Persistent<Object>::New(obj);
  self.MakeWeak(dict, VimDictDestroy);
  self->SetInternalField(0, External::New(dict));
  objcache.insert(LookupMap::value_type(dict, self));
  return self;
}

static void
VimDictDestroy(Persistent<Value> object, void* parameter)
{
  objcache.erase(parameter);
  weak_unref(parameter);
}

static Handle<Value>
VimDictCreate(const Arguments& args)
{
  if (!args.IsConstructCall())
    return ThrowException(String::New("Cannot call constructor as function"));

  dict_T *dict = dict_alloc();
  if (dict == NULL)
    return ThrowException(String::New("VimDictCreate(): dict_alloc(): out of memory"));
  return MakeVimDict(dict, args.Holder());
}

static Handle<Value>
VimDictIdxGet(uint32_t index, const AccessorInfo& info)
{
  return VimDictGet(Integer::New(index)->ToString(), info);
}

static Handle<Value>
VimDictIdxSet(uint32_t index, Local<Value> value, const AccessorInfo& info)
{
  return VimDictSet(Integer::New(index)->ToString(), value, info);
}

static Handle<Boolean>
VimDictIdxQuery(uint32_t index, const AccessorInfo& info)
{
  return VimDictQuery(Integer::New(index)->ToString(), info);
}

static Handle<Boolean>
VimDictIdxDelete(uint32_t index, const AccessorInfo& info)
{
  return VimDictDelete(Integer::New(index)->ToString(), info);
}

static Handle<Value>
VimDictGet(Local<String> property, const AccessorInfo& info)
{
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
  if (!dict_set(dict, (char_u*)*key, &vimobj))
    return ThrowException(String::New("error dict_set()"));
  return value;
}

static Handle<Boolean>
VimDictQuery(Local<String> property, const AccessorInfo& info)
{
  Handle<Object> self = info.Holder();
  Handle<External> external = Handle<External>::Cast(self->GetInternalField(0));
  dict_T *dict = static_cast<dict_T*>(external->Value());
  String::Utf8Value key(property);
  dictitem_T *di = dict_find(dict, (char_u*)*key, -1);
  if (di == NULL)
    return False();
  return True();
}

static Handle<Boolean>
VimDictDelete(Local<String> property, const AccessorInfo& info)
{
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
  typval_T tv;
  tv_set_func(&tv, (char_u*)name);
  weak_ref((void*)tv.vval.v_string, &tv);

  Persistent<Object> self = Persistent<Object>::New(obj);
  self.MakeWeak((void*)tv.vval.v_string, VimFuncDestroy);
  self->SetInternalField(0, External::New(tv.vval.v_string));
  self->SetInternalField(1, Undefined());
  return self;
}

static void
VimFuncDestroy(Persistent<Value> object, void* parameter)
{
  weak_unref(parameter);
}

static Handle<Value>
VimFuncCall(const Arguments& args)
{
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

#if 0
  // XXX: Exception is lost when vim.call() throw exception and caller
  // script doesn't have try-catch block.
  // ExecuteString() -> caller (js) -> VimFuncCall() -> vim.call() (js)
  return call->Call(vim, 3, callargs);
#else
  // Cannot use ThrowException() in the TryCatch block.
  Handle<Value> res;
  Handle<Value> exception;
  {
    TryCatch try_catch;
    res = call->Call(vim, 3, callargs);
    exception = try_catch.Exception();
  }
  if (!exception.IsEmpty())
    return ThrowException(exception);
  return res;
#endif
}

