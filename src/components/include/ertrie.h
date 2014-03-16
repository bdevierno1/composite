/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef ERTRIE_H
#define ERTRIE_H

#ifndef LINUX_TEST
#include <cos_component.h>
#endif

/* 
 * TODO:
 * - separate the types for an opaque, and accessible internal node,
 *   and a leaf data-node.
 * - fix the set functions to actually do the memory store in the
 *   customized function
 * - change the accum variable to be void *, and be named load_info,
 *   and memctxt to be named store_info.
 */

/* Internal node in the trie */
struct ert_intern {
	struct ert_intern *next;
};
struct ert {
	struct ert_intern vect[0]; /* in-place array */
};

/* get the next level/value in the internal structure/value */
typedef struct ert_intern *(*ert_get_fn_t)(struct ert_intern *, unsigned long *accum, int isleaf);
/* check if the value in the internal structure is "null" */
typedef int  (*ert_isnull_fn_t)(struct ert_intern *, unsigned long *accum, int isleaf);
/* set values to their initial value (often "null") */
typedef void *(*ert_initval_fn_t)(struct ert_intern *, int isleaf);
/* set a value in an internal structure/value */
typedef void (*ert_set_fn_t)(struct ert_intern *e, void *val, unsigned long *accum, int isleaf);
/* allocate an internal or leaf structure */
typedef void *(*ert_alloc_fn_t)(void *data, int sz, int last_lvl);
/* if we you extending the leaf level, this is called to set the leaf entry */
typedef void (*ert_setleaf_fn_t)(struct ert_intern *entry, void *data);
typedef void *(*ert_getleaf_fn_t)(struct ert_intern *entry, unsigned long *accum);

/* 
 * Default implementations of the customization functions that assume
 * a normal tree with pointers for internal nodes, with the "null
 * node" being equal to NULL (i.e. you can't store NULL values in the
 * structure), and setting values in internal and leaf nodes being
 * done with straightforward stores.
 */
static struct ert_intern *
ert_defget(struct ert_intern *a, unsigned long *accum, int isleaf) { (void)accum; (void)isleaf; return a->next; }
static void *ert_defgetleaf(struct ert_intern *a, unsigned long *accum) { (void)accum;  return a->next; }
static void ert_defset(struct ert_intern *a, void *v, unsigned long *accum, int isleaf) { (void)isleaf; (void)accum; a->next = v; }
static void ert_defsetleaf(struct ert_intern *a, void *data) { a->next = data; } /* assume leaf items are ptrs */
static int 
ert_defisnull(struct ert_intern *a, unsigned long *accum, int isleaf) 
{ (void)accum; (void)isleaf; return a == NULL; }
static void *ert_definitfn(struct ert_intern *a, int isleaf) { (void)a; (void)isleaf; return NULL; }
static void *ert_definitval = NULL;

#define ERT_CONST_PARAMS                                             \
	u32_t depth, u32_t order, u32_t last_order, u32_t last_sz,   \
	void *initval, ert_initval_fn_t initfn, ert_get_fn_t getfn, ert_isnull_fn_t isnullfn, \
	ert_set_fn_t setfn, ert_alloc_fn_t allocfn, ert_setleaf_fn_t setleaffn, ert_getleaf_fn_t getleaffn
#define ERT_CONST_ARGS depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn
#define ERT_CONSTS_DEWARN  (void)depth; (void)order; (void)last_order; (void)last_sz; (void)initval; \
        (void)initfn; (void)getfn; (void)isnullfn; (void)setfn; (void)allocfn; (void)setleaffn; (void)getleaffn;

/* 
 * This macro is the key using the compiler to generate fast code.
 * This is generating function calls that are often inlined that are
 * being passed _constants_.  After function inlining, loop unrolling,
 * and constant propagation, the code generated should be very
 * specific to the set of parameters used.  Loops should be
 * eliminated, conditionals removed, and straight-line code produced.
 *
 * The informal goal of this is to ensure that the lookup code
 * generated is on the order of 10-20 instructions, depending on
 * depth.  In terms of (hot-cache) performance, we're shooting for
 * ~5*depth cycles (if L1 is 5 cycles to access).  For cold caches,
 * we're looking for ~500*depth cycles (if memory accesses are 500
 * cycles).  When there is parallel contention (writes), the cost
 * should be comparable to the latter case.
 *
 * Question: How can I replace the long argument lists here with a
 * macro?  I'm hitting some preprocessor limitation I didn't
 * anticipate here.
 */
#define ERT_CREATE(name, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn) \
struct name##_ert { struct ert t; };				        \
static struct name##_ert *name##_alloc(void *memctxt)                   \
{ return (struct name##_ert*)ert_alloc(memctxt, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn); } \
static inline void *name##_lkup(struct name##_ert *v, unsigned long id)	\
{ unsigned long a; return __ert_lookup((struct ert*)v, id, depth, &a, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn); } \
static inline void *name##_lkupa(struct name##_ert *v, unsigned long id, unsigned long *accum)  \
{ return __ert_lookup((struct ert*)v, id, depth, accum, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn); } \
static inline void *name##_lkupan(struct name##_ert *v, unsigned long id, int dlimit, unsigned long *accum) \
{ return __ert_lookup((struct ert*)v, id, dlimit, accum, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn); } \
static inline int name##_expandn(struct name##_ert *v, unsigned long id, u32_t dlimit, unsigned long *accum, void *memctxt, void *data) \
{ return __ert_expand((struct ert*)v, id, dlimit, accum, memctxt, data, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn); } \
static inline int name##_expand(struct name##_ert *v, unsigned long id, unsigned long *accum, void *memctxt, void *data) \
{ return __ert_expand((struct ert*)v, id, depth, accum, memctxt, data, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn); } \
static inline unsigned long name##_maxid(void)				\
{ return __ert_maxid(depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn); }


#define ERT_CREATE_DEF(name, depth, order, last_order, last_sz, allocfn)	\
ERT_CREATE(name, depth, order, last_order, last_sz, ert_definitval, ert_definitfn, ert_defget, ert_defisnull, ert_defset, allocfn, ert_defsetleaf, ert_defgetleaf)

static inline unsigned long
__ert_maxid(ERT_CONST_PARAMS)
{ 
	ERT_CONSTS_DEWARN;
	unsigned long off    = (unsigned long)(((order * (depth-1)) + last_order));
	unsigned long maxoff = (sizeof(int*)*8);
	return (off > maxoff) ? 1<<maxoff : 1<<off; 
}

/* 
 * Initialize a level in the ertrie.  lvl is either an internal level,
 * lvl > 1, or a leaf level in the tree in which case embedded leaf
 * structs require a different initialization.
 */
static inline void
__ert_init(struct ert_intern *vi, int isleaf, ERT_CONST_PARAMS)
{
	int i, base, sz;
	ERT_CONSTS_DEWARN;

	assert(vi);
	if (!isleaf) {
		base = 1<<order;
		sz   = sizeof(int*);
	} else {
		base = 1<<last_order;
		sz   = last_sz;
	}
	for (i = 0 ; i < base ; i++) {
		struct ert_intern *t = (void*)(((char*)vi) + (sz * i));
		t->next = initfn(t, isleaf);
	}
}

static struct ert *
ert_alloc(void *memctxt, ERT_CONST_PARAMS)
{
	struct ert *v;
	struct ert_intern e;
	unsigned long accum = 0;
	
	/* Make sure the id size can be represented on our system */
	assert(((order * (depth-1)) + last_order) < (sizeof(unsigned long)*8));
	assert(depth >= 1);
	if (depth > 1) v = allocfn(memctxt, (1<<order) * sizeof(int*), 0);
	else           v = allocfn(memctxt, (1<<last_order) * last_sz, 1);
	if (NULL == v) return NULL;
	__ert_init(v->vect, depth == 1, ERT_CONST_ARGS);

	setfn(&e, v, &accum, depth == 1);
	return (struct ert *)e.next;
}

static inline struct ert_intern *
__ert_walk(struct ert_intern *vi, unsigned long id, unsigned long *accum, u32_t lvl, ERT_CONST_PARAMS)
{
#define ERT_M(id, o) ((id) & ((1<<(o))-1)) // Mask out order number of bits
	ERT_CONSTS_DEWARN;

	vi = getfn(vi, accum, 0);
	if (lvl-1 == 0) {
		/* offset into the last level, leaf node */
		u32_t last_off = ERT_M(id, last_order) * last_sz;
		return (struct ert_intern *)(((char *)vi) + last_off);
	}
	/* calculate the offset in an internal node */
	return &vi[ERT_M((id >> ((order * (lvl-2)) + last_order)), order)];
}

/* 
 * This is the most optimized/most important function.  
 *
 * We rely on compiler optimizations -- including constant
 * propagation, loop unrolling, function inlining, dead-code
 * elimination, and function inlining from constant function pointers
 * -- to turn this into straight-line code.  It should be on the order
 * of 10-20 instructions without loops, only including error checking
 * branches that are not taken by the static branch detection
 * algorithms.
 *
 * dlimit is the depth we should look into the tree.  This can be 0
 * (return the highest-level of the tree) all the way to depth+1 which
 * actually treats the entries in the last level of the trie as a
 * pointer and returns its destination.  dlimit = depth+1 means that the
 * size of the last-level nodes should be the size of an integer.
 */
static inline struct ert_intern *
__ert_lookup(struct ert *v, unsigned long id, u32_t dlimit, unsigned long *accum, ERT_CONST_PARAMS) 
{
	struct ert_intern r, *n;
	u32_t i, limit;

	assert(v);
	assert(id < __ert_maxid(ERT_CONST_ARGS));
	assert(dlimit <= depth+1);

	/* simply gets the address of the vector */
	r.next = v->vect;
	n      = &r;
	limit  = dlimit < depth ? dlimit : depth;
	for (i = 0 ; i < limit ; i++) {
		if (unlikely(isnullfn(n->next, accum, 0))) return NULL;
		n = __ert_walk(n, id, accum, depth-i, ERT_CONST_ARGS);
	}
	if (dlimit == depth+1) n = getleaffn(n, accum);

	return n;
}

/* 
 * Expand the data-structure up to and including some depth limit
 * (dlimit).  This will call the initialization routines for that
 * level, and hook it into the overall trie.  If you want to control
 * the costs of memory allocation and initialization, then you should
 * use limit to ensure that multiple levels of the trie are not
 * expanded here, if desired.
 * 
 * limit == 1 does not make sense (i.e. ert is already allocated),
 * and limit = depth+1 means that we're trying to "expand" or set the
 * leaf data to something provided in the memctxt.
 */
static inline int
__ert_expand(struct ert *v, unsigned long id, u32_t dlimit, unsigned long *accum, void *memctxt, void *data, ERT_CONST_PARAMS)
{
	struct ert_intern r, *n, *new;
	u32_t i, limit;

	assert(v);
	assert(id < __ert_maxid(ERT_CONST_ARGS));
	assert(dlimit <= depth+1); /* cannot expand past leaf */

	r.next = v->vect;
	n      = &r;
	limit  = dlimit < depth ? dlimit : depth;
	for (i = 0 ; i < limit-1 ; i++) {
		n = __ert_walk(n, id, accum, depth-i, ERT_CONST_ARGS);
		if (!isnullfn(n->next, accum, 0)) continue;
		
		if (i+2 < depth) new = allocfn(memctxt, (1<<order) * sizeof(int*), 0);
		else             new = allocfn(memctxt, (1<<last_order) * last_sz, 1);
		if (unlikely(!new)) return -1;
		__ert_init(new, i+2 >= depth, ERT_CONST_ARGS);
		setfn(n, new, accum, 0);
	}
	if (dlimit == depth+1) {
		n = __ert_walk(n, id, accum, depth-i, ERT_CONST_ARGS);
		setleaffn(n, data);
	}
	return 0;
}

#endif /* ERTRIE_H */
