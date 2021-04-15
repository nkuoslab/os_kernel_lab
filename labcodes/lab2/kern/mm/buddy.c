#include <buddy.h>
#include <list.h>
#include <pmm.h>
#include <string.h>

#define LEFT_LEAF(index) ((index)*2 + 1)
#define RIGHT_LEAF(index) ((index)*2 + 2)
#define PARENT(index) (((index) + 1) / 2 - 1)

#define IS_POWER_OF_2(x) (!((x) & ((x)-1)))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

uint8_t ROUND_DOWN_LOG(int size) {
    uint8_t n = 0;
    while (size >>= 1) {
        n++;
    }
    return n;
}

#define buddy ((unsigned*)buddy_addr)

// 单位都是页的个数
// total_size 管理的总内存大小
// manage_size 实际可以被分配的内存大小
// buddy_size buddy结构占据的内存大小
// free_size 类似以前的nr_free
unsigned total_size, manage_size, buddy_size, free_size;
// buddy_addr buddy结构的起始地址
unsigned buddy_addr;
// manage_page 实际可以被分配的物理内存的起始页
struct Page* manage_page;

static void buddy_init(void) {
    free_size = 0;
}

void buddy_new(int size) {
    unsigned node_size;
    int i;

    if (size < 1 || !IS_POWER_OF_2(size))
        return NULL;

    node_size = size * 2;

    for (i = 0; i < 2 * size - 1; ++i) {
        if (IS_POWER_OF_2(i + 1))
            node_size /= 2;
        buddy[i] = node_size;
    }
}

static void buddy_init_memmap(struct Page* base, size_t n) {
    assert(n > 0);
    struct Page* p = base;
    for (; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    total_size = n;
    buddy_addr = page2kva(base);
    n = 1 << ROUND_DOWN_LOG(n);
    manage_size = n;
    buddy_size = 2 * n / 1024;
    free_size += manage_size;
    base += buddy_size;
    manage_page = base;
    base->property = n;
    SetPageProperty(base);
    buddy_new(manage_size);
    cprintf("---------buddy init end-------\n");
    cprintf("total_size = %d\n", total_size);
    cprintf("buddy_size = %d\n", buddy_size);
    cprintf("manage_size = %d\n", manage_size);
    cprintf("free_size = %d\n", free_size);
    cprintf("buddy_addr = 0x%08x\n", buddy_addr);
    cprintf("manage_page_addr = 0x%08x\n", manage_page);
    cprintf("-------------------------------\n");
}

int buddy_alloc(int size) {
    unsigned index = 0;
    unsigned node_size;
    unsigned offset = 0;

    if (size <= 0)
        size = 1;
    else if (!IS_POWER_OF_2(size))
        size = 1 << (ROUND_DOWN_LOG(size) + 1);

    if (buddy[index] < size)
        return -1;


    for (node_size = manage_size; node_size != size; node_size /= 2) {
        unsigned left = buddy[LEFT_LEAF(index)];
        unsigned right = buddy[RIGHT_LEAF(index)];
        // if (size == 64) {
        //     cprintf("index:%d, left:%d, right=%d\n", index, left, right);
        // }
        if (left > right) {
            if (right >= size)
                index = RIGHT_LEAF(index);
            else
                index = LEFT_LEAF(index);
        } else {
            if (left >= size)
                index = LEFT_LEAF(index);
            else
                index = RIGHT_LEAF(index);
        }
    }

    buddy[index] = 0;
    offset = (index + 1) * node_size - manage_size;

    while (index) {
        index = PARENT(index);
        buddy[index] = MAX(buddy[LEFT_LEAF(index)], buddy[RIGHT_LEAF(index)]);
    }
    return offset;
}

static struct Page* buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > free_size) {
        return NULL;
    }
    int offset = buddy_alloc(n);
    // cprintf("offset=%d\n", offset);
    struct Page* page = NULL;
    page = manage_page + offset;
    ClearPageProperty(page);
    free_size -= n;
    return page;
}

void buddy_free(int offset) {
    unsigned node_size, index = 0;
    unsigned left_longest, right_longest;
    // cprintf("buddy free: %d\n", offset);

    assert(offset >= 0 && offset < manage_size);

    node_size = 1;
    index = offset + manage_size - 1;

    for (; buddy[index]; index = PARENT(index)) {
        node_size *= 2;
        if (index == 0)
            return;
    }

    buddy[index] = node_size;

    while (index) {
        index = PARENT(index);
        node_size *= 2;

        left_longest = buddy[LEFT_LEAF(index)];
        right_longest = buddy[RIGHT_LEAF(index)];

        if (left_longest + right_longest == node_size)
            buddy[index] = node_size;
        else
            buddy[index] = MAX(left_longest, right_longest);
    }
}

static void buddy_free_pages(struct Page* base, size_t n) {
    assert(n > 0);
    struct Page* p = base;
    // 清除标志位和ref
    for (; p != base + n; p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    buddy_free(base - manage_page);
    free_size += n;
}

static size_t buddy_nr_free_pages(void) {
    return free_size;
}

static void buddy_check(void) {
    struct Page *p0, *A, *B, *C, *D;
    p0 = A = B = C = D = NULL;

    assert((p0 = alloc_page()) != NULL);
    assert((A = alloc_page()) != NULL);
    assert((B = alloc_page()) != NULL);

    assert(p0 != A && p0 != B && A != B);
    assert(page_ref(p0) == 0 && page_ref(A) == 0 && page_ref(B) == 0);

    free_page(p0);
    free_page(A);
    free_page(B);

    A = alloc_pages(500);
    B = alloc_pages(500);
    cprintf("A %p\n", A);
    cprintf("B %p\n", B);
    free_pages(A, 250);
    free_pages(B, 500);
    free_pages(A + 250, 250);

    p0 = alloc_pages(8192);
    cprintf("p0 %p\n", p0);
    assert(p0 == A);
    // free_pages(p0, 1024);
    //以下是根据链接中的样例测试编写的
    A = alloc_pages(70);
    B = alloc_pages(35);
    cprintf("A %p\n", A);
    cprintf("B %p\n", B);
    assert(A + 128 == B);  //检查是否相邻
    C = alloc_pages(80);
    assert(A + 256 == C);  //检查C有没有和A重叠
    cprintf("C %p\n", C);
    free_pages(A, 70);  //释放A
    D = alloc_pages(60);
    cprintf("D %p\n", D);
    assert(B + 64 == D);  //检查B，D是否相邻
    free_pages(B, 35);
    cprintf("D %p\n", D);
    free_pages(D, 60);
    cprintf("C %p\n", C);
    free_pages(C, 80);
    free_pages(p0, 8192);  //全部释放
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};