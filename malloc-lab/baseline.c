/* First-fit explicit free list; single heap, eight-byte alignment. */
#include "memlib.h"
#include "mm.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct block {
  size_t capacity;
  struct block *next;
  struct block *free_next;
  size_t used;
} block;
static block *first, *last, *free_list;
team_t team = {"lrn-malloc first-fit", "", "", "", ""};
static void remove_free(block *b) {
  block **p = &free_list;
  while (*p && *p != b)
    p = &(*p)->free_next;
  if (*p)
    *p = b->free_next;
}
int mm_init(void) {
  first = last = free_list = NULL;
  return 0;
}
void *mm_malloc(size_t size) {
  if (!size || size > INT_MAX - sizeof(block) - 8)
    return NULL;
  size = (size + 7) & ~(size_t)7;
  block *b = free_list;
  while (b && b->capacity < size)
    b = b->free_next;
  if (!b) {
    size_t capacity = size > 4096 ? size : 4096;
    b = mem_sbrk((int)(sizeof(block) + capacity));
    if (b == (void *)-1)
      return NULL;
    *b = (block){capacity, NULL, NULL, 0};
    if (last)
      last->next = b;
    else
      first = b;
    last = b;
  } else
    remove_free(b);
  if (b->capacity >= size + sizeof(block) + 8) {
    block *rest = (block *)((char *)(b + 1) + size);
    *rest = (block){b->capacity - size - sizeof(block), b->next, free_list, 0};
    free_list = rest;
    b->next = rest;
    b->capacity = size;
    if (last == b)
      last = rest;
  }
  b->used = 1;
  b->free_next = NULL;
  return b + 1;
}
void mm_free(void *p) {
  if (!p)
    return;
  block *b = (block *)p - 1;
  b->used = 0;
  if (b->next && !b->next->used) {
    block *next = b->next;
    remove_free(next);
    b->capacity += sizeof(block) + next->capacity;
    b->next = next->next;
    if (last == next)
      last = b;
  }
  block *prev = NULL;
  for (block *q = first; q && q != b; q = q->next)
    prev = q;
  if (prev && !prev->used) {
    prev->capacity += sizeof(block) + b->capacity;
    prev->next = b->next;
    if (last == b)
      last = prev;
  } else {
    b->free_next = free_list;
    free_list = b;
  }
}
void *mm_realloc(void *p, size_t size) {
  if (!p)
    return mm_malloc(size);
  if (!size) {
    mm_free(p);
    return NULL;
  }
  block *b = (block *)p - 1;
  if (size <= b->capacity)
    return p;
  void *next = mm_malloc(size);
  if (!next)
    return NULL;
  memcpy(next, p, b->capacity);
  mm_free(p);
  return next;
}
void mm_dump(FILE *out) {
  for (block *b = first; b; b = b->next)
    fprintf(out, "  offset=%zu capacity=%zu %s\n",
            (size_t)((char *)(b + 1) - (char *)mem_heap_lo()), b->capacity,
            b->used ? "used" : "free");
}
