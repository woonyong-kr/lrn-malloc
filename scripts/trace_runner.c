#define _POSIX_C_SOURCE 200809L
#include "memlib.h"
#include "mm.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
void mm_dump(FILE *);
typedef struct {
  char kind;
  int id;
  size_t size;
} operation;
typedef struct {
  void *ptr;
  size_t size;
} allocation;
static double now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}
static void pattern(allocation b, int id) {
  for (size_t i = 0; i < b.size; i++)
    assert(((unsigned char *)b.ptr)[i] == (unsigned char)(id * 37 + i));
}
static void fill(allocation b, int id) {
  for (size_t i = 0; i < b.size; i++)
    ((unsigned char *)b.ptr)[i] = (unsigned char)(id * 37 + i);
}
static double replay(operation *ops, int count, int ids, int verify,
                     int verbose, size_t *peak, size_t *heap) {
  allocation *blocks = calloc((size_t)ids, sizeof *blocks);
  assert(blocks);
  mem_reset_brk();
  assert(mm_init() == 0);
  size_t live = 0;
  *peak = 0;
  double start = now();
  for (int i = 0; i < count; i++) {
    operation op = ops[i];
    assert(op.id >= 0 && op.id < ids);
    allocation old = blocks[op.id];
    if (op.kind == 'f') {
      assert(old.ptr);
      if (verify)
        pattern(old, op.id);
      mm_free(old.ptr);
      live -= old.size;
      blocks[op.id] = (allocation){0};
    } else {
      assert(op.kind == 'a' || op.kind == 'r');
      if (op.kind == 'a')
        assert(!old.ptr);
      if (verify && old.ptr)
        pattern(old, op.id);
      void *p =
          op.kind == 'a' ? mm_malloc(op.size) : mm_realloc(old.ptr, op.size);
      assert(p || op.size == 0);
      assert((uintptr_t)p % 8 == 0);
      if (verify && p) {
        assert((char *)p >= (char *)mem_heap_lo() &&
               (char *)p + op.size <= (char *)mem_heap_hi() + 1);
        for (int j = 0; j < ids; j++)
          if (j != op.id && blocks[j].ptr)
            assert((char *)p + op.size <= (char *)blocks[j].ptr ||
                   (char *)blocks[j].ptr + blocks[j].size <= (char *)p);
        if (old.ptr)
          pattern((allocation){p, old.size < op.size ? old.size : op.size},
                  op.id);
      }
      blocks[op.id] = (allocation){p, op.size};
      if (verify && p)
        fill(blocks[op.id], op.id);
      live = live - old.size + op.size;
      if (live > *peak)
        *peak = live;
    }
    if (verbose) {
      printf("%c id=%d request=%zu heap=%zu\n", op.kind, op.id, op.size,
             mem_heapsize());
      mm_dump(stdout);
    }
  }
  double seconds = now() - start;
  *heap = mem_heapsize();
  if (verify)
    for (int j = 0; j < ids; j++)
      if (blocks[j].ptr)
        pattern(blocks[j], j);
  free(blocks);
  return seconds;
}
int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: trace-runner file.rep [--verbose]\n");
    return 2;
  }
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    perror(argv[1]);
    return 2;
  }
  int suggested, ids, count, weight;
  if (fscanf(f, "%d%d%d%d", &suggested, &ids, &count, &weight) != 4 ||
      ids <= 0 || count <= 0) {
    fclose(f);
    return 2;
  }
  operation *ops = calloc((size_t)count, sizeof *ops);
  assert(ops);
  for (int i = 0; i < count; i++) {
    if (fscanf(f, " %c %d", &ops[i].kind, &ops[i].id) != 2)
      return 2;
    if (ops[i].kind != 'f' && fscanf(f, "%zu", &ops[i].size) != 1)
      return 2;
  }
  fclose(f);
  mem_init();
  size_t peak, heap;
  replay(ops, count, ids, 1, argc > 2, &peak, &heap);
  double elapsed = 0;
  int repeats = 0;
  do {
    elapsed += replay(ops, count, ids, 0, 0, &peak, &heap);
    repeats++;
  } while ((repeats < 20 || elapsed < 0.03) && repeats < 100000);
  printf("{\"trace\":\"%s\",\"operations\":%d,\"verified\":true,\"peak_payload_"
         "bytes\":%zu,\"heap_bytes\":%zu,\"utilization\":%.6f,\"ops_per_"
         "second\":%.0f,\"timing_repeats\":%d}\n",
         argv[1], count, peak, heap, heap ? (double)peak / heap : 0,
         count * repeats / elapsed, repeats);
  mem_deinit();
  free(ops);
  return 0;
}
