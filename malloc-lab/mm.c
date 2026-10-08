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

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static char *heap_listp;    

static void *free_listp = NULL; // 가용 리스트 헤드

typedef enum ecoalesce_cases {
    NONE,
    PREV,
    NEXT,
    BOTH
}coalesce_cases;

typedef enum e_expand_cases {    
    EXPAND_FAIL,
    EXPAND_NEXT,
    EXPAND_PREV,
    EXPAND_BOTH
}expand_cases;

static coalesce_cases coalesce_case = NONE;
static expand_cases expand_case = NONE;

// 요청 크기에 맞는 블록 주소 반환 
/* first fit*/
static void *find_fit(size_t asize)
{        
    // 가용 리스트 확인    
    char *cur_bp = free_listp;

    while (cur_bp != NULL)
    {
        if (GET_SIZE(HDRP(cur_bp)) >= asize) 
            return cur_bp;
            
        cur_bp = SUCC_BLKP(cur_bp);        
    }    
    return NULL; // 적당한 블록 없으면 NULL 반환
}

// /* best fit */
// static void *find_fit(size_t asize)
// {            
//     char *cur_bp = free_listp;
//     char *best_fit_bp = NULL;

//     size_t cur_size;
//     size_t min_size = (size_t)-1;    

//     while (cur_bp != NULL)
//     {
//         if ((cur_size = GET_SIZE(HDRP(cur_bp))) >= asize) {
//             if (cur_size == asize)
//                 return cur_bp;

//             if (cur_size <= min_size) {
//                 min_size = cur_size;
//                 best_fit_bp = cur_bp;
//             }
//         }
//         cur_bp = SUCC_BLKP(cur_bp);        
//     }    

//     if (best_fit_bp != NULL)
//         return best_fit_bp;
    
//     return NULL; // 적당한 블록 없으면 NULL 반환
// }


// 블록 할당
// pred, succ 연결 관계 변경
static void place(void *bp, size_t asize)
{
    size_t block_size = GET_SIZE(HDRP(bp));    

    // 분할 전 pred, succ저장
    void *pred = PRED_BLKP(bp);
    void *succ = SUCC_BLKP(bp);     

    // 할당 후 남는 사이즈가 최소 블록 크기보다 크면 분할    
    if (block_size - asize >= 3 * DSIZE)
    {           
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        // 할당하고 남은 가용 블록 포인터
        void *next_free = NEXT_BLKP(bp);
        
        PUT(HDRP(next_free), PACK(block_size - asize, 0));
        PUT(FTRP(next_free), PACK(block_size - asize, 0));
        
        // '분할 후 가용 블록'에 pred, succ 저장
        PRED_BLKP(next_free) = pred;     
        SUCC_BLKP(next_free) = succ;     

        // 이전 가용 블록의 succ 저장
        if (pred != NULL)
            SUCC_BLKP(pred) = next_free;
        else 
            free_listp = next_free;

        // 다음 가용 블록의 pred 저장
        if (succ != NULL)
            PRED_BLKP(succ) = next_free;       

        return;
    }

    // 분할 하지 않을 경우

    if (pred != NULL)
        SUCC_BLKP(pred) = succ;
    else 
        free_listp = succ;

    if (succ != NULL)
        PRED_BLKP(succ) = pred;

    PUT(HDRP(bp), PACK(block_size, 1));
    PUT(FTRP(bp), PACK(block_size, 1));
}

// bp의 연결 상태 변경
void free_block_connect(coalesce_cases cases, void *bp)
{
    if (bp == NULL)
        return;
    
    void *prev;
    void *next;
    void *succ;

    switch (cases)
    {
        case PREV: /* pred - prev - bp - succ => pred - prev - succ */             
            prev = PREV_BLKP(bp); // 이전 블록 포인터
            succ = SUCC_BLKP(bp); // 다음 가용 블록 포인터

            SUCC_BLKP(prev) = succ;

            if (succ != NULL)
                PRED_BLKP(succ) = prev; // 다음 가용 블록의 이전 가용 블록을 prev와 연결
            break;

        case NEXT: /* pred - bp - next - succ => pred - bp - succ */
            next = NEXT_BLKP(bp); // 다음 블록 포인터
            succ = SUCC_BLKP(next); // 다음 가용 블록 포인터

            SUCC_BLKP(bp) = succ;

            if (succ != NULL)
                PRED_BLKP(succ) = bp;  // 다음 가용블록의 이전 가용 블록을 bp와 연결
            break;

        case BOTH: /* pred - prev - bp - next - succ => pred - prev - succ*/
            prev = PREV_BLKP(bp); // 이전 블록 포인터
            next = NEXT_BLKP(bp); // 다음 블록 포인터
            succ = SUCC_BLKP(next); // 다음 가용 블록 포인터

            SUCC_BLKP(prev) = succ;

            if (succ != NULL)
                PRED_BLKP(succ) = prev;                
            break;

        default : 
            break;
    }
}

// 인접 블록 병합
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp))); // 이전 블록 할당 상태
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 이전 블록 할당 상태
    size_t size = GET_SIZE(HDRP(bp));
    void *pred, *succ; // 이전 가용 블록 포인터, 다음 가용 블록 포인터

    if (prev_alloc && next_alloc)   // case #1    
        return bp;    
     
    if (prev_alloc && !next_alloc) { // case #2   
        free_block_connect(NEXT, bp);        
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));        
    }
    else if (!prev_alloc && next_alloc) { // case #3        
        free_block_connect(PREV, bp);
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else { // case #4        
        free_block_connect(BOTH, bp);
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
                GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
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

    // 새 free block의 pred, succ 초기화
    PRED_BLKP(bp) = NULL;
    SUCC_BLKP(bp) = NULL;

    return bp;    
}

// bp를 free list에 주소 순서대로 삽입
static void insert_free_block(void *bp)
{
    void *cur = free_listp;
    void *prev = NULL;

    while (cur != NULL && cur < bp) 
    {
        prev = cur;
        cur = SUCC_BLKP(cur);
    }

    PRED_BLKP(bp) = prev;
    SUCC_BLKP(bp) = cur;

    if (prev != NULL)
        SUCC_BLKP(prev) = bp;
    else
        free_listp = bp;

    if (cur != NULL)
        PRED_BLKP(cur) = bp;
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    free_listp = NULL;

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);                            // 최초 패딩
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 블록 헤더 셋팅 heap_listp + (1*WSIZE) 연산 먼저 실행 후 unsgined int*로 캐스팅
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 블록 풋터 셋팅
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // 에필로그 헤더 셋팅

    heap_listp += (2 * WSIZE);

    free_listp = extend_heap(CHUNKSIZE / WSIZE);

    if (free_listp == NULL)
        return -1;        

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) 
        return NULL;

    if (size <= DSIZE)
        asize = 3 * DSIZE; // 최소 블록 크기 24바이트
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);

    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;

    insert_free_block(bp);

    bp = coalesce(bp);

    place(bp, asize);

    return bp;
}
// void *mm_malloc(size_t size)
// {
//     int newsize = ALIGN(size + SIZE_T_SIZE);
//     void *p = mem_sbrk(newsize);
//     if (p == (void *)-1)
//         return NULL;
//     else
//     {
//         *(size_t *)p = size;
//         return (void *)((char *)p + SIZE_T_SIZE);
//     }
// }

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if (ptr == NULL)
        return;

    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    insert_free_block(ptr);

    coalesce(ptr);
}

// 병합할 인접 free block을 가용 리스트에서 제거
static void remove_free_block(expand_cases cases, void *bp)
{    
    if (bp == NULL) 
        return;

    void *prev = PREV_BLKP(bp);
    void *next = NEXT_BLKP(bp);    

    /* pred(free) - next(free) - succ(free) -> pred(free) - succ(free) */
    if (cases == EXPAND_NEXT) {
        void *pred = PRED_BLKP(next);
        void *succ = SUCC_BLKP(next);   

        if (pred != NULL)
            SUCC_BLKP(pred) = succ;
        else 
            free_listp = succ;
        
        if (succ != NULL)
            PRED_BLKP(succ) = pred;

        return;
    }

    /* pred(free) - prev(free) - succ(free) -> pred(free) - succ(free) */
    if (cases == EXPAND_PREV) {
        void *pred = PRED_BLKP(prev);
        void *succ = SUCC_BLKP(prev);

        if (pred != NULL)
            SUCC_BLKP(pred) = succ;
        else 
            free_listp = succ;
        
        if (succ != NULL)
            PRED_BLKP(succ) = pred;
        
        return;
    }

    /* pred(free) - [prev(free) - next(free)] - succ(free) -> pred(free) - succ(free) */
    if (cases == EXPAND_BOTH) { 
        remove_free_block(EXPAND_PREV, bp);
        remove_free_block(EXPAND_NEXT, bp);
        
        return;
    }
}

// 블록 확장 함수
static expand_cases try_expand_block(void *bp, size_t asize)
{     
    int prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp))); // 이전 블록 할당 상태
    int next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 이전 블록 할당 상태

    size_t prev_size = GET_SIZE(HDRP(PREV_BLKP(bp))); // 이전 블록 크기     
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블록 크기
    size_t cur_size = GET_SIZE(HDRP(bp));
    size_t copySize = cur_size - DSIZE;
    
    if (prev_alloc && next_alloc) 
        return EXPAND_FAIL;

    // 뒤 인접 블록이 free
    if (!next_alloc && (cur_size + next_size) >= asize) {
        void *next = NEXT_BLKP(bp);
        void *pred = PRED_BLKP(next);
        void *succ = SUCC_BLKP(next);

        remove_free_block(EXPAND_NEXT, bp);        
        
        if ((cur_size + next_size) - asize >= MINSIZE) {
            size_t remain = cur_size + next_size - asize;

            PUT(HDRP(bp), PACK(asize, 1));
            PUT(FTRP(bp), PACK(asize, 1));

            void *remain_bp = NEXT_BLKP(bp);            

            PUT(HDRP(remain_bp), PACK(remain, 0));
            PUT(FTRP(remain_bp), PACK(remain, 0));

            PRED_BLKP(remain_bp) = pred;
            SUCC_BLKP(remain_bp) = succ;

            if (pred != NULL)
                SUCC_BLKP(pred) = remain_bp;            
            else
                free_listp = remain_bp;
            
            if (succ != NULL)
                PRED_BLKP(succ) = remain_bp;

            return EXPAND_NEXT;
        }
        
        cur_size += next_size;
        
        PUT(HDRP(bp), PACK(cur_size, 1));
        PUT(FTRP(bp), PACK(cur_size, 1)); 

        return EXPAND_NEXT;
    }

    //앞 인접 블록이 free
    if (!prev_alloc && (prev_size + cur_size) >= asize) {
        void *prev = PREV_BLKP(bp);
        void *pred = PRED_BLKP(prev);
        void *succ = SUCC_BLKP(prev);

        remove_free_block(EXPAND_PREV, bp);

        memmove(prev, bp, copySize);

        if ((prev_size + cur_size) - asize >= MINSIZE) {
            size_t remain = cur_size + prev_size - asize;                

            PUT(HDRP(prev), PACK(asize, 1));
            PUT(FTRP(prev), PACK(asize, 1));     

            void *remain_bp = NEXT_BLKP(prev);            

            PUT(HDRP(remain_bp), PACK(remain, 0));
            PUT(FTRP(remain_bp), PACK(remain, 0));

            PRED_BLKP(remain_bp) = pred;
            SUCC_BLKP(remain_bp) = succ;

            if (pred != NULL)
                SUCC_BLKP(pred) = remain_bp;
            else 
                free_listp = remain_bp;
            
            if (succ != NULL)
                PRED_BLKP(succ) = remain_bp;            

            return EXPAND_PREV;
        }

        cur_size += prev_size;        
        PUT(HDRP(prev), PACK(cur_size, 1));
        PUT(FTRP(prev), PACK(cur_size, 1)); 

        return EXPAND_PREV;
    }

    if (!prev_alloc && !next_alloc &&
        (prev_size + cur_size + next_size) >= asize) {

        // 분할 전 주소
        void *prev = PREV_BLKP(bp);
        void *next = NEXT_BLKP(bp);
        void *pred = PRED_BLKP(prev);
        void *succ = SUCC_BLKP(next);        

        remove_free_block(EXPAND_BOTH, bp);

        memmove(prev, bp, copySize);

        if ((prev_size + cur_size + next_size) - asize >= MINSIZE) {
            size_t remain = prev_size + cur_size + next_size - asize;            
        
            PUT(HDRP(prev), PACK(asize, 1));
            PUT(FTRP(prev), PACK(asize, 1));             

            void *remain_bp = NEXT_BLKP(prev);            

            PUT(HDRP(remain_bp), PACK(remain, 0));
            PUT(FTRP(remain_bp), PACK(remain, 0));

            PRED_BLKP(remain_bp) = pred;
            SUCC_BLKP(remain_bp) = succ;

            if (pred != NULL)
                SUCC_BLKP(pred) = remain_bp;
            else 
                free_listp = remain_bp;
            
            if (succ != NULL)
                PRED_BLKP(succ) = remain_bp;

            return EXPAND_BOTH;
        }

        cur_size += (prev_size + next_size); 
        PUT(HDRP(prev), PACK(cur_size, 1));
        PUT(FTRP(prev), PACK(cur_size, 1)); 

        return EXPAND_BOTH;
    }
    return EXPAND_FAIL;
}

static void split_alloc_block(void *bp, size_t asize)
{ 
    size_t pre_size = GET_SIZE(HDRP(bp));
    size_t remain_size = pre_size - asize;

    // 할당 블록 크기 변경
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));

    // 새 가용 블록 연결관계 설정
    void *new_free_ptr = NEXT_BLKP(bp);

    // 새 가용 블록 크기 설정
    PUT(HDRP(NEXT_BLKP(bp)), PACK(remain_size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(remain_size, 0));

    // 주소순 free list에 삽입
    insert_free_block(new_free_ptr);

    // 뒤쪽 블록이 free라면 병합
    coalesce(new_free_ptr);

    return;    
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *newptr;
    size_t preSize, copySize;     
    size_t asize;   

    if (ptr == NULL)
        return mm_malloc(size);

    preSize = GET_SIZE(HDRP(ptr));        
    copySize = preSize - DSIZE; // 기존 포인터의 Payload만 복사

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    if (size <= DSIZE)
        asize = 3 * DSIZE; // 최소 블록 크기 24바이트
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    // if (asize <= preSize) {    
    //     return ptr;
    // }

    if (asize <= preSize) {    
        if (preSize - asize >= MINSIZE) 
            split_alloc_block(ptr, asize);    

        return ptr;
    }

    void *prev  = PREV_BLKP(ptr);

    if (asize > preSize) {        
        switch (try_expand_block(ptr, asize))
        {      
            case EXPAND_FAIL: break;

            case EXPAND_NEXT: return ptr;

            case EXPAND_PREV: return prev;

            case EXPAND_BOTH: return prev;

            default: break;
        }
    }

    newptr = mm_malloc(size);

    if (newptr == NULL)
        return NULL;    

    if (size < copySize)
        copySize = size;

    memcpy(newptr, ptr, copySize);
    
    mm_free(ptr);

    return newptr;
}
// void *mm_realloc(void *ptr, size_t size)
// {
//     void *newptr;
//     size_t copySize;

//     if (ptr == NULL)
//         return mm_malloc(size);

//     if (size == 0) {
//         mm_free(ptr);
//         return NULL;
//     }

//     newptr = mm_malloc(size);

//     if (newptr == NULL)
//         return NULL;

//     copySize = GET_SIZE(HDRP(ptr)) - DSIZE; // 기존 포인터의 Payload만 복사

//     if (size < copySize)
//         copySize = size;

//     memcpy(newptr, ptr, copySize);
    
//     mm_free(ptr);

//     return newptr;
// }
