#include <stdio.h>
#include <stdlib.h>

// also by using 'container_of(ptr, type, member)', we're able to put the vtable
// anywhere in the struct.

#define UNUSED(...) (void)(__VA_ARGS__)

// get/set
typedef int (*get_t) (void*);
typedef void (*set_t) (void*, int);
typedef void (*free_t)(void*);

// virtual table will be created for each class
typedef struct {
  get_t aget;
  set_t aset;
  free_t afree;
} vtable_t;

typedef struct {
  // so that all the member share the same vtable
  vtable_t *v;
} a_t;

typedef struct {
  vtable_t *v;
} b_t;

int a_get(void *fake) {
  printf(__FUNCTION__);
  UNUSED(fake);
  return 0;  
}

void a_set(void *fake1, int fake2) {
  printf(__FUNCTION__);
  UNUSED(fake1, fake2);
}

int b_get(void *fake) {
  printf(__FUNCTION__);
  UNUSED(fake);
  return 0;  
}

void b_set(void *fake1, int fake2) {
  printf(__FUNCTION__);
  UNUSED(fake1, fake2);
}

void a_free(void *p) {
  printf(__FUNCTION__);
  free(p);
}

vtable_t a_vtable = {&a_get, &a_set, &a_free};
vtable_t b_vtable = {&b_get, &b_set, &a_free};

vtable_t *create_a()
{
  a_t* ret =  (a_t*)malloc(sizeof(a_t));
  ret->v = &a_vtable;
  return (vtable_t*)ret;
}

vtable_t *create_b()
{
  b_t* ret =  (b_t*)malloc(sizeof(b_t));
  ret->v = &b_vtable;
  return (vtable_t*)ret;
}

void foo(vtable_t *v)
{
  v->aset(v, 1);
  v->aget(v);
  v->afree(v);
}
