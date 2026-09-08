#include "memlib.h"
#include "mm.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
int main(void) {
  mem_init();
  assert(mm_init() == 0);
  assert(mm_malloc(0) == NULL);
  void *p = mm_malloc(64);
  assert(p);
  memset(p, 0x5a, 64);
  assert(mm_malloc(SIZE_MAX) == NULL);
  assert(mm_realloc(p, SIZE_MAX) == NULL);
  for (int i = 0; i < 64; i++)
    assert(((unsigned char *)p)[i] == 0x5a);
  mm_free(NULL);
  mm_free(p);
  p = mm_realloc(NULL, 33);
  assert(p && (uintptr_t)p % 8 == 0);
  assert(mm_realloc(p, 0) == NULL);
  mem_deinit();
  return 0;
}
