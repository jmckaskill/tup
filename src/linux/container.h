/* vim: set ts=8 sw=8 sts=8 noet tw=78: */
#ifndef tup_container_h
#define tup_container_h

#include <stddef.h>

/* Macros pulled out of list.h for re-use with rbtree */
#ifdef offsetof
#undef offsetof
#endif

#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

/**
 * container_of - cast a member of a structure out to the containing structure
 *
 * @ptr:        the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) \
        ((type*)((char*) (ptr) - offsetof(type, member)))

#if !defined __cplusplus && __STDC_VERSION__ + 0 < 199901L
#define inline
#endif

#endif
