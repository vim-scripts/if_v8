/* vim:set foldmethod=marker: */

#include <stdio.h>
#include <string.h>

#ifdef WIN32
# include <windows.h>
# define DLLEXPORT __declspec(dllexport)
# define DLLIMPORT extern __declspec(dllimport)
# define DLOPEN(path) (void*)LoadLibrary(path)
#else
# include <dlfcn.h>
# define DLLEXPORT
# define DLLIMPORT extern
# define DLOPEN(path) dlopen(path, RTLD_NOW)
#endif

/* header {{{1 */
#define FEAT_FLOAT 1

#define FALSE 0
#define TRUE 1

#define STRLEN(s)	    strlen((char *)(s))
#define STRCPY(d, s)	    strcpy((char *)(d), (char *)(s))

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

/* functions {{{1 */
static const char *init_vim();
/* typval */
static typval_T *alloc_tv();
/* List */
static listitem_T *listitem_alloc();
static void listitem_free(listitem_T *item);
static listitem_T *list_find(list_T *l, long n);
static void list_append(list_T *l, listitem_T *item);
static void list_fix_watch(list_T *l, listitem_T *item);
static void list_remove(list_T *l, listitem_T *item, listitem_T *item2);
static long list_len(list_T *l);
/* Dictionary */
static dictitem_T *dictitem_alloc(char_u *key);
static void dictitem_free(dictitem_T *item);
static void dictitem_remove(dict_T *dict, dictitem_T *item);
static int dict_add(dict_T *d, dictitem_T *item);
/* XXX: recurse is not supported (always TRUE) */
#define dict_free(d, recurse) wrap_dict_free(d)
static void wrap_dict_free(dict_T *d);
static dictitem_T *dict_find(dict_T *d, char_u *key, int len);
/* Funcref */
static void func_ref(char_u *name);
/* not in vim */
static void tv_set_number(typval_T *tv, varnumber_T number);
#ifdef FEAT_FLOAT
static void tv_set_float(typval_T *tv, float_T v_float);
#endif
static void tv_set_string(typval_T *tv, char_u *string);
static void tv_set_list(typval_T *tv, list_T *list);
static void tv_set_dict(typval_T *tv, dict_T *dict);
static void tv_set_func(typval_T *tv, char_u *name);
static int dict_set_tv_nocopy(dict_T *dict, char_u *key, typval_T *tv);
static int list_append_tv_nocopy(list_T *list, typval_T *tv);

/* import {{{1 */

#ifdef __cplusplus
extern "C" {
#endif
/* variables */
DLLIMPORT char_u hash_removed;
DLLIMPORT int got_int;
/* functions */
DLLIMPORT typval_T *eval_expr(char_u *arg, char_u **nextcmd);
DLLIMPORT int do_cmdline_cmd(char_u *cmd);
DLLIMPORT void free_tv(typval_T *varp);
DLLIMPORT void clear_tv(typval_T *varp);
DLLIMPORT int emsg(char_u *s);
DLLIMPORT char_u *alloc(unsigned size);
DLLIMPORT char_u *alloc_clear(unsigned size);
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
DLLIMPORT void ui_breakcheck();
#ifdef __cplusplus
}
#endif

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

/* typval {{{1 */

/*
 * Allocate memory for a variable type-value, and make it empty (0 or NULL
 * value).
 */
    static typval_T *
alloc_tv()
{
    return (typval_T *)alloc_clear((unsigned)sizeof(typval_T));
}

/* List {{{1 */

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

/* Dictionary {{{1 */

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
 * XXX: MODIFIED
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
	; /* void */
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
    tv_set_dict(&tv, d);
    d->dv_refcount = 1;
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

/* Funcref {{{1 */

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
    dict_set_tv_nocopy(&vimvardict, (char_u*)"%v8_func1%", &tv);
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

/* not in vim {{{1 */

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
 * let dict[key] = tv without copy
 */
static int
dict_set_tv_nocopy(dict_T *dict, char_u *key, typval_T *tv)
{
    dictitem_T *di;

    di = dict_find(dict, key, -1);
    if (di == NULL)
    {
	di = dictitem_alloc(key);
	if (di == NULL)
	    return FALSE;
	if (!dict_add(dict, di))
	{
	    dictitem_free(di);
	    return FALSE;
	}
    }
    else
	clear_tv(&di->di_tv);
    di->di_tv = *tv;
    return TRUE;
}

static int
list_append_tv_nocopy(list_T *list, typval_T *tv)
{
    listitem_T *li;

    li = listitem_alloc();
    if (li == NULL)
	return FALSE;
    li->li_tv = *tv;
    list_append(list, li);
    return TRUE;
}
