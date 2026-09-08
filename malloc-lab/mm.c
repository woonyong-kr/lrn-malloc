#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include "mm.h"
#include "memlib.h"

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define ALIGN(size)       (((size) + (DSIZE - 1)) & ~0x7)
#define CHUNK_ALIGN(size) (((size) + (CHUNKSIZE - 1)) & ~(CHUNKSIZE - 1))

#define LEFT(bp)      (*(void **)(bp))
#define RIGHT(bp)     (*(void **)((char *)(bp) + 8))
#define SUB_MAX(bp)   (*(size_t *)((char *)(bp) + 16))
#define SAME_NEXT(bp) (*(void **)((char *)(bp) + 24))

#define SET_LEFT(bp, ptr)      (*(void **)(bp) = (ptr))
#define SET_RIGHT(bp, ptr)     (*(void **)((char *)(bp) + 8) = (ptr))
#define SET_SAME_NEXT(bp, ptr) (*(void **)((char *)(bp) + 24) = (ptr))

#define GET_BAL(bp)      ((int)((GET(HDRP(bp)) >> 1) & 0x3) - 1)
#define SET_BAL(bp, bal) PUT(HDRP(bp), (GET(HDRP(bp)) & ~0x6) | ((((bal) + 1) & 0x3) << 1))

#define MIN_BLOCK_SIZE 40

static char *heap_listp;
static void *tree_root;
static int delta_h;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void  place(void *bp, size_t asize);

static void  update_submax(void *bp);
static void *rotate_left(void *x);
static void *rotate_right(void *y);
static void *balance_after_left_shrink(void *root);
static void *balance_after_right_shrink(void *root);
static void *tree_insert(void *root, void *bp);
static void *tree_delete(void *root, void *bp);
static void *extract_min(void *root, void **min_out);
static void *tree_best_fit(void *root, size_t asize);

team_t team = {
    "ateam",
    "Harry Bovik",
    "bovik@cs.cmu.edu",
    "",
    ""};

static void update_submax(void *bp)
{
    size_t sm = GET_SIZE(HDRP(bp));
    void *l = LEFT(bp);
    void *r = RIGHT(bp);
    if (l && SUB_MAX(l) > sm)
        sm = SUB_MAX(l);
    if (r && SUB_MAX(r) > sm)
        sm = SUB_MAX(r);
    SUB_MAX(bp) = sm;
}

static void *rotate_left(void *x)
{
    void *y = RIGHT(x);
    SET_RIGHT(x, LEFT(y));
    SET_LEFT(y, x);
    update_submax(x);
    update_submax(y);
    return y;
}

static void *rotate_right(void *y)
{
    void *x = LEFT(y);
    SET_LEFT(y, RIGHT(x));
    SET_RIGHT(x, y);
    update_submax(y);
    update_submax(x);
    return x;
}

static void *balance_after_left_shrink(void *root)
{
    int bal = GET_BAL(root);

    if (bal < 0) {
        SET_BAL(root, 0);
    } else if (bal == 0) {
        SET_BAL(root, 1);
        delta_h = 0;
    } else {
        void *R = RIGHT(root);
        int rb = GET_BAL(R);

        if (rb < 0) {
            void *rl = LEFT(R);
            int rlb = GET_BAL(rl);
            SET_RIGHT(root, rotate_right(R));
            root = rotate_left(root);
            SET_BAL(LEFT(root),  (rlb > 0) ? -1 : 0);
            SET_BAL(RIGHT(root), (rlb < 0) ?  1 : 0);
            SET_BAL(root, 0);
        } else if (rb == 0) {
            root = rotate_left(root);
            SET_BAL(root, -1);
            SET_BAL(LEFT(root), 1);
            delta_h = 0;
        } else {
            root = rotate_left(root);
            SET_BAL(root, 0);
            SET_BAL(LEFT(root), 0);
        }
    }
    update_submax(root);
    return root;
}

static void *balance_after_right_shrink(void *root)
{
    int bal = GET_BAL(root);

    if (bal > 0) {
        SET_BAL(root, 0);
    } else if (bal == 0) {
        SET_BAL(root, -1);
        delta_h = 0;
    } else {
        void *L = LEFT(root);
        int lb = GET_BAL(L);

        if (lb > 0) {
            void *lr = RIGHT(L);
            int lrb = GET_BAL(lr);
            SET_LEFT(root, rotate_left(L));
            root = rotate_right(root);
            SET_BAL(LEFT(root),  (lrb > 0) ? -1 : 0);
            SET_BAL(RIGHT(root), (lrb < 0) ?  1 : 0);
            SET_BAL(root, 0);
        } else if (lb == 0) {
            root = rotate_right(root);
            SET_BAL(root, 1);
            SET_BAL(RIGHT(root), -1);
            delta_h = 0;
        } else {
            root = rotate_right(root);
            SET_BAL(root, 0);
            SET_BAL(RIGHT(root), 0);
        }
    }
    update_submax(root);
    return root;
}

static void *tree_insert(void *root, void *bp)
{
    if (!root) {
        SET_LEFT(bp, NULL);
        SET_RIGHT(bp, NULL);
        SET_SAME_NEXT(bp, NULL);
        SET_BAL(bp, 0);
        SUB_MAX(bp) = GET_SIZE(HDRP(bp));
        delta_h = 1;
        return bp;
    }

    size_t bsz = GET_SIZE(HDRP(bp));
    size_t rsz = GET_SIZE(HDRP(root));

    if (bsz == rsz) {
        SET_SAME_NEXT(bp, SAME_NEXT(root));
        SET_SAME_NEXT(root, bp);
        delta_h = 0;
        return root;
    }

    if (bsz < rsz) {
        SET_LEFT(root, tree_insert(LEFT(root), bp));
        if (delta_h) {
            int bal = GET_BAL(root);
            if (bal > 0) {
                SET_BAL(root, 0);
                delta_h = 0;
            } else if (bal == 0) {
                SET_BAL(root, -1);
            } else {
                void *L = LEFT(root);
                if (GET_BAL(L) > 0) {
                    void *lr = RIGHT(L);
                    int lrb = GET_BAL(lr);
                    SET_LEFT(root, rotate_left(L));
                    root = rotate_right(root);
                    SET_BAL(LEFT(root),  (lrb > 0) ? -1 : 0);
                    SET_BAL(RIGHT(root), (lrb < 0) ?  1 : 0);
                    SET_BAL(root, 0);
                } else {
                    root = rotate_right(root);
                    SET_BAL(root, 0);
                    SET_BAL(RIGHT(root), 0);
                }
                delta_h = 0;
            }
        }
    } else {
        SET_RIGHT(root, tree_insert(RIGHT(root), bp));
        if (delta_h) {
            int bal = GET_BAL(root);
            if (bal < 0) {
                SET_BAL(root, 0);
                delta_h = 0;
            } else if (bal == 0) {
                SET_BAL(root, 1);
            } else {
                void *R = RIGHT(root);
                if (GET_BAL(R) < 0) {
                    void *rl = LEFT(R);
                    int rlb = GET_BAL(rl);
                    SET_RIGHT(root, rotate_right(R));
                    root = rotate_left(root);
                    SET_BAL(LEFT(root),  (rlb > 0) ? -1 : 0);
                    SET_BAL(RIGHT(root), (rlb < 0) ?  1 : 0);
                    SET_BAL(root, 0);
                } else {
                    root = rotate_left(root);
                    SET_BAL(root, 0);
                    SET_BAL(LEFT(root), 0);
                }
                delta_h = 0;
            }
        }
    }

    update_submax(root);
    return root;
}

static void *extract_min(void *root, void **min_out)
{
    if (!LEFT(root)) {
        *min_out = root;
        delta_h = 1;
        return RIGHT(root);
    }

    SET_LEFT(root, extract_min(LEFT(root), min_out));
    if (delta_h)
        root = balance_after_left_shrink(root);
    else
        update_submax(root);
    return root;
}

static void *tree_delete(void *root, void *bp)
{
    if (!root)
        return NULL;

    size_t bsz = GET_SIZE(HDRP(bp));
    size_t rsz = GET_SIZE(HDRP(root));

    if (bsz < rsz) {
        SET_LEFT(root, tree_delete(LEFT(root), bp));
        if (delta_h)
            root = balance_after_left_shrink(root);
        else
            update_submax(root);
        return root;
    }

    if (bsz > rsz) {
        SET_RIGHT(root, tree_delete(RIGHT(root), bp));
        if (delta_h)
            root = balance_after_right_shrink(root);
        else
            update_submax(root);
        return root;
    }

    if (root != bp) {
        void *prev = root;
        void *curr = SAME_NEXT(root);
        while (curr && curr != bp) {
            prev = curr;
            curr = SAME_NEXT(curr);
        }
        if (curr == bp)
            SET_SAME_NEXT(prev, SAME_NEXT(bp));
        delta_h = 0;
        return root;
    }

    void *sn = SAME_NEXT(root);
    if (sn) {
        SET_LEFT(sn, LEFT(root));
        SET_RIGHT(sn, RIGHT(root));
        SET_BAL(sn, GET_BAL(root));
        update_submax(sn);
        delta_h = 0;
        return sn;
    }

    void *L = LEFT(root);
    void *R = RIGHT(root);

    if (!L && !R) {
        delta_h = 1;
        return NULL;
    }
    if (!L) {
        delta_h = 1;
        return R;
    }
    if (!R) {
        delta_h = 1;
        return L;
    }

    void *succ;
    void *new_right = extract_min(R, &succ);

    SET_LEFT(succ, L);
    SET_RIGHT(succ, new_right);
    SET_BAL(succ, GET_BAL(root));

    if (delta_h)
        succ = balance_after_right_shrink(succ);
    else
        update_submax(succ);

    return succ;
}

static void *tree_best_fit(void *root, size_t asize)
{
    if (!root || SUB_MAX(root) < asize)
        return NULL;

    size_t rsz = GET_SIZE(HDRP(root));

    if (rsz == asize)
        return root;

    if (rsz > asize) {
        void *left_fit = tree_best_fit(LEFT(root), asize);
        return left_fit ? left_fit : root;
    }

    return tree_best_fit(RIGHT(root), asize);
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
    } else if (prev_alloc && !next_alloc) {
        void *nb = NEXT_BLKP(bp);
        tree_root = tree_delete(tree_root, nb);
        size += GET_SIZE(HDRP(nb));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        void *pb = PREV_BLKP(bp);
        tree_root = tree_delete(tree_root, pb);
        size += GET_SIZE(HDRP(pb));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(pb), PACK(size, 0));
        bp = pb;
    } else {
        void *pb = PREV_BLKP(bp);
        void *nb = NEXT_BLKP(bp);
        tree_root = tree_delete(tree_root, pb);
        tree_root = tree_delete(tree_root, nb);
        size += GET_SIZE(HDRP(pb))
              + GET_SIZE(HDRP(nb));
        PUT(HDRP(pb), PACK(size, 0));
        PUT(FTRP(nb), PACK(size, 0));
        bp = pb;
    }

    tree_root = tree_insert(tree_root, bp);
    return bp;
}

static void *find_fit(size_t asize)
{
    return tree_best_fit(tree_root, asize);
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    tree_root = tree_delete(tree_root, bp);

    if ((csize - asize) >= MIN_BLOCK_SIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        void *remainder = NEXT_BLKP(bp);
        PUT(HDRP(remainder), PACK(csize - asize, 0));
        PUT(FTRP(remainder), PACK(csize - asize, 0));
        tree_root = tree_insert(tree_root, remainder);
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += 2 * WSIZE;

    tree_root = NULL;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    void *bp;

    if (size == 0 || size > INT_MAX - CHUNKSIZE - DSIZE)
        return NULL;

    asize = ALIGN(size + DSIZE);
    if (asize < MIN_BLOCK_SIZE)
        asize = MIN_BLOCK_SIZE;

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = CHUNK_ALIGN(asize);
    bp = extend_heap(extendsize / WSIZE);
    if (bp == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

void mm_free(void *ptr)
{
    if (ptr == NULL)
        return;
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    size_t asize;
    size_t next_alloc;
    size_t next_size;
    size_t combined;
    void *newptr;

    if (ptr == NULL)
        return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    if (size > INT_MAX - CHUNKSIZE - DSIZE)
        return NULL;

    oldsize = GET_SIZE(HDRP(ptr));

    asize = ALIGN(size + DSIZE);
    if (asize < MIN_BLOCK_SIZE)
        asize = MIN_BLOCK_SIZE;

    if (asize <= oldsize) {
        if ((oldsize - asize) >= MIN_BLOCK_SIZE) {
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));
            newptr = NEXT_BLKP(ptr);
            PUT(HDRP(newptr), PACK(oldsize - asize, 0));
            PUT(FTRP(newptr), PACK(oldsize - asize, 0));
            coalesce(newptr);
        }
        return ptr;
    }

    next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));

    if (!next_alloc && (oldsize + next_size) >= asize) {
        tree_root = tree_delete(tree_root, NEXT_BLKP(ptr));
        combined = oldsize + next_size;
        PUT(HDRP(ptr), PACK(combined, 1));
        PUT(FTRP(ptr), PACK(combined, 1));
        if ((combined - asize) >= MIN_BLOCK_SIZE) {
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));
            newptr = NEXT_BLKP(ptr);
            PUT(HDRP(newptr), PACK(combined - asize, 0));
            PUT(FTRP(newptr), PACK(combined - asize, 0));
            tree_root = tree_insert(tree_root, newptr);
        }
        return ptr;
    }

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    memcpy(newptr, ptr, oldsize - DSIZE);
    mm_free(ptr);
    return newptr;
}

void mm_dump(FILE *out)
{
    for (char *bp = NEXT_BLKP(heap_listp); GET_SIZE(HDRP(bp)); bp = NEXT_BLKP(bp))
        fprintf(out, "  offset=%zu block=%u %s\n", (size_t)(bp - (char *)mem_heap_lo()),
                GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)) ? "used" : "free");
}
