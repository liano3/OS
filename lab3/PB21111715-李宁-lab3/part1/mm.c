/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


/*explicit free list start*/
#define WSIZE 8 // 一个字 8 Byte
#define DSIZE 16 // 双字 16 Byte
#define CHUNKSIZE (1 << 12) // 堆 2 扩容的最小单位, 4KB
#define MAX(x, y) ((x) > (y) ? (x) : (y))   // 取大

// 将 size、prev_alloc、alloc 三个参数打包成一个 word
#define PACK(size, prev_alloc, alloc) (((size) & ~(1<<1)) | ((prev_alloc << 1) & ~(1)) | (alloc))
// 把前面邻居块的分配信息(size、prev_alloc)打包为一个 word，用于赋值头/脚部
#define PACK_PREV_ALLOC(val, prev_alloc) (((val) & ~(1<<1)) | (prev_alloc << 1))
// 用于更改是否分配
#define PACK_ALLOC(val, alloc) ((val) | (alloc))

#define GET(p) (*(unsigned long *)(p)) // 读地址 p
#define PUT(p, val) (*(unsigned long *)(p) = (val)) // 写地址 p

#define GET_SIZE(p) (GET(p) & ~0x7) // 获取 size
#define GET_ALLOC(p) (GET(p) & 0x1) // 获取 alloc
#define GET_PREV_ALLOC(p) ((GET(p) & 0x2) >> 1) // 获取 prev_alloc

// 获取当前块的头部地址
#define HDRP(bp) ((char *)(bp)-WSIZE)
// 获取当前块的脚部地址
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
// 获取后面邻居块的地址
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
// 获取前面邻居块的地址
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp)-DSIZE))) 

#define GET_PRED(bp) (GET(bp))  // 获取前驱空闲块的地址
#define SET_PRED(bp, val) (PUT(bp, val)) // 写入前驱空闲块的地址

#define GET_SUCC(bp) (GET(bp + WSIZE)) // 获取后继空闲块的地址
#define SET_SUCC(bp, val) (PUT(bp + WSIZE, val)) // 写入后继空闲块的地址

#define MIN_BLK_SIZE (2 * DSIZE)    // 最小块，4 个字(头加脚加前驱加后继)

/* single word (8) or double word (16) alignment */
#define ALIGNMENT DSIZE // 对齐

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static char *heap_listp; // 堆空间指针
static char *free_listp; // 空闲链表指针

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
// static void *find_fit(size_t asize);
static void *find_fit_best(size_t asize);
static void *find_fit_first(size_t asize);
static void place(void *bp, size_t asize);
static void add_to_free_list(void *bp);
static void delete_from_free_list(void *bp);
double get_utilization();
void mm_check(const char *);

/*
    TODO:
        完成一个简单的分配器内存使用率统计
        user_malloc_size: 用户申请内存量
        heap_size: 分配器占用内存量
    HINTS:
        1. 在适当的地方修改上述两个变量，细节参考实验文档
        2. 在 get_utilization() 中计算使用率并返回
*/
size_t user_malloc_size = 0;
size_t heap_size = 0;
double get_utilization() {
    // 计算使用率
    // printf("%u %u\n", user_malloc_size, heap_size);
    return (long double)user_malloc_size / heap_size;
}
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    free_listp = NULL;
    // 请求 4 个字的堆空间，分别作为填充块、序言块头/脚部、结尾块
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    heap_size += 32;
    // 填充块(0)
    PUT(heap_listp, 0);
    // 序言块头部
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1, 1));
    // 序言块脚部
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1, 1));
    // 尾块
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1, 1));
    // 指向序言块
    heap_listp += (2 * WSIZE);
    // 初始分配 4KB
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    /* mm_check(__FUNCTION__);*/
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    /*printf("\n in malloc : size=%u", size);*/
    /*mm_check(__FUNCTION__);*/
    size_t newsize;
    size_t extend_size;
    char *bp;

    if (size == 0)
        return NULL;
    newsize = MAX(MIN_BLK_SIZE, ALIGN((size + WSIZE))); /*size+WSIZE(head_len)*/
    /* newsize = MAX(MIN_BLK_SIZE, (ALIGN(size) + DSIZE));*/
    if ((bp = find_fit_best(newsize)) != NULL)
    {
        place(bp, newsize);
        // printf("no bug there~");
        return bp;
    }
    /*no fit found.*/
    extend_size = MAX(newsize, CHUNKSIZE);
    if ((bp = extend_heap(extend_size / WSIZE)) == NULL)
    {
        printf("extend heap failed\n");
        return NULL;
    }
    place(bp, newsize);
    // printf("no bug there~");
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    // 用户内存减少
    user_malloc_size -= size;
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    void *head_next_bp = NULL;

    PUT(HDRP(bp), PACK(size, prev_alloc, 0));
    PUT(FTRP(bp), PACK(size, prev_alloc, 0));
    /*printf("%s, addr_start=%u, size_head=%u, size_foot=%u\n",*/
    /*    __FUNCTION__, HDRP(bp), (size_t)GET_SIZE(HDRP(bp)), (size_t)GET_SIZE(FTRP(bp)));*/

    /*notify next_block, i am free*/
    head_next_bp = HDRP(NEXT_BLKP(bp));
    PUT(head_next_bp, PACK_PREV_ALLOC(GET(head_next_bp), 0));

    // add_to_free_list(bp);
    // printf("no bug there~");
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    // 重新分配空间
    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t words)
{
    /*get heap_brk*/
    char *old_heap_brk = mem_sbrk(0);
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(old_heap_brk));

    /*printf("\nin extend_heap prev_alloc=%u\n", prev_alloc);*/
    char *bp;
    size_t size;
    // 保证对齐
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    // 堆空间增长
    heap_size += size;
    PUT(HDRP(bp), PACK(size, prev_alloc, 0)); /*last free block*/
    PUT(FTRP(bp), PACK(size, prev_alloc, 0));

    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0, 1)); /*break block*/
    return coalesce(bp);
}

// 合并空闲块
static void *coalesce(void *bp)
{
    /*add_to_free_list(bp);*/
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    /*
        TODO:
            将 bp 指向的空闲块 与 相邻块合并
            结合前一块及后一块的分配情况，共有 4 种可能性
            分别完成相应case下的 数据结构维护逻辑
    */
    if (prev_alloc && next_alloc) { /* 前后块均已分配 */
        add_to_free_list(bp);
    }
    else if (prev_alloc && !next_alloc) /*前块已分配，后块空闲*/
    {
        // 将后块移出空闲链表
        delete_from_free_list(NEXT_BLKP(bp));
        // 合并后块
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        size += next_size;
        PUT(HDRP(bp), PACK(size, 1, 0));
        PUT(FTRP(bp), PACK(size, 1, 0));
        add_to_free_list(bp);
    }
    else if (!prev_alloc && next_alloc) /*前块空闲，后块已分配*/
    {
        // 合并前块
        size_t prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        size += prev_size;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 1, 0));
        PUT(FTRP(PREV_BLKP(bp)), PACK(size, 1, 0));
        // 更改 bp 指针
        bp = PREV_BLKP(bp);
    }
    else /*前后都是空闲块*/
    {
        // 将后块移出空闲链表
        delete_from_free_list(NEXT_BLKP(bp));
        // 合并前后块
        size_t prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        size += (prev_size + next_size);
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 1, 0));
        PUT(FTRP(PREV_BLKP(bp)), PACK(size, 1, 0));
        // 更改 bp 指针
        bp = PREV_BLKP(bp);
    }
    // printf("no bug there~ bp = %u", bp);
    return bp;
}

static void *find_fit_first(size_t asize)
{
    /* 
        首次匹配算法
        TODO:
            遍历 freelist， 找到第一个合适的空闲块后返回
        
        HINT: asize 已经计算了块头部的大小
    */
    // 遍历空闲链表
    for (char *bp = free_listp; bp != NULL; bp = (char *)GET_SUCC(bp))
    {
        // 找到合适的空闲块
        if (asize <= GET_SIZE(HDRP(bp)))
        {
            // printf("find_fit_first: bp = %u, size = %u\n", bp, GET_SIZE(HDRP(bp)));
            return bp;
        }
    }
    // 没找到
    return NULL;
}

static void* find_fit_best(size_t asize) {
    /* 
        最佳配算法
        TODO:
            遍历 freelist， 找到最合适的空闲块，返回
        
        HINT: asize 已经计算了块头部的大小
    */
    // 最合适的空间地址及大小
    void *best_bp = NULL;
    size_t best_size = -1; // 0xffffffff
    // 遍历空闲链表
    for (char *bp = free_listp; bp != NULL; bp = (char *)GET_SUCC(bp))
    {
        // 找到合适的空闲块
        if (asize <= GET_SIZE(HDRP(bp)) && GET_SIZE(HDRP(bp)) < best_size)
        {
            best_bp = bp;
            best_size = GET_SIZE(HDRP(bp));
        }
    }
    return best_bp;
}

static void place(void *bp, size_t asize)
{
    /* 
        TODO:
        将一个空闲块转变为已分配的块

        HINTS:
            1. 若空闲块在分离出一个 asize 大小的使用块后，剩余空间不足空闲块的最小大小，
                则原先整个空闲块应该都分配出去
            2. 若剩余空间仍可作为一个空闲块，则原空闲块被分割为一个已分配块+一个新的空闲块
            3. 空闲块的最小大小已经 #define，或者根据自己的理解计算该值
    */
    // 计算剩余空间
    size_t remain_size = GET_SIZE(HDRP(bp)) - asize;
    // 剩余空间不足
    if (remain_size < MIN_BLK_SIZE)
    {
        // 整个空闲块分配出去
        PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 1, 1));
        PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 1, 1));
        delete_from_free_list(bp);
        // 告诉后邻居块
        char *head_next_bp = HDRP(NEXT_BLKP(bp));
        PUT(head_next_bp, PACK_PREV_ALLOC(GET(head_next_bp), 1));
    }
    else
    {
        // 分配出去的块
        PUT(HDRP(bp), PACK(asize, 1, 1));
        PUT(FTRP(bp), PACK(asize, 1, 1));
        delete_from_free_list(bp);
        // 剩余空闲块
        PUT(HDRP(NEXT_BLKP(bp)), PACK(remain_size, 1, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(remain_size, 1, 0));
        add_to_free_list(NEXT_BLKP(bp));
    }
    // 用户内存增长
    user_malloc_size += GET_SIZE(HDRP(bp));
}

// 头插法
static void add_to_free_list(void *bp)
{
    /*set pred & succ*/
    if (free_listp == NULL) /*free_list empty*/
    {
        SET_PRED(bp, 0);
        SET_SUCC(bp, 0);
        free_listp = bp;
    }
    else
    {
        SET_PRED(bp, 0);
        SET_SUCC(bp, (size_t)free_listp); /*size_t ???*/
        SET_PRED(free_listp, (size_t)bp);
        free_listp = bp;
    }
}

static void delete_from_free_list(void *bp)
{
    size_t prev_free_bp=0;
    size_t next_free_bp=0;
    if (free_listp == NULL)
        return;
    prev_free_bp = GET_PRED(bp);
    next_free_bp = GET_SUCC(bp);

    if (prev_free_bp == next_free_bp && prev_free_bp != 0)
    {
        /*mm_check(__FUNCTION__);*/
        /*printf("\nin delete from list: bp=%u, prev_free_bp=%u, next_free_bp=%u\n", (size_t)bp, prev_free_bp, next_free_bp);*/
    }
    if (prev_free_bp && next_free_bp) /*11*/
    {
        SET_SUCC(prev_free_bp, GET_SUCC(bp));
        SET_PRED(next_free_bp, GET_PRED(bp));
    }
    else if (prev_free_bp && !next_free_bp) /*10*/
    {
        SET_SUCC(prev_free_bp, 0);
    }
    else if (!prev_free_bp && next_free_bp) /*01*/
    {
        SET_PRED(next_free_bp, 0);
        free_listp = (void *)next_free_bp;
    }
    else /*00*/
    {
        free_listp = NULL;
    }
}

void mm_check(const char *function)
{
    /* printf("\n---cur func: %s :\n", function);
    char *bp = free_listp;
    int count_empty_block = 0;
    while (bp != NULL) //not end block;
    {
        count_empty_block++;
        printf("addr_start：%u, addr_end：%u, size_head:%u, size_foot:%u, PRED=%u, SUCC=%u \n", (size_t)bp - WSIZE,
                (size_t)FTRP(bp), GET_SIZE(HDRP(bp)), GET_SIZE(FTRP(bp)), GET_PRED(bp), GET_SUCC(bp));
        ;
        bp = (char *)GET_SUCC(bp);
    }
    printf("empty_block num: %d\n\n", count_empty_block);*/
}