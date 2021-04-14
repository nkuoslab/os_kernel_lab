#include <buddy.h>
#include <list.h>
#include <pmm.h>
#include <string.h>
// #include <stdlib.h>

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

struct buddy {
    unsigned size;
    unsigned longest[8000];
} buddy1;

free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void buddy_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

void buddy_new(int size) {
    cprintf("buddy new\n");
    unsigned node_size;
    int i;

    if (size < 1 || !IS_POWER_OF_2(size))
        return NULL;

    buddy1.size = size;

    node_size = size * 2;

    for (i = 0; i < 2 * size - 1; ++i) {
        if (IS_POWER_OF_2(i + 1))
            node_size /= 2;
        buddy1.longest[i] = node_size;
    }
    // return buddy1;
}

static void buddy_init_memmap(struct Page* base, size_t n) {
    cprintf("buddy_init_memmap\n");
    assert(n > 0);
    struct Page* p = base;
    for (; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    n = 1 << ROUND_DOWN_LOG(n);
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    list_add_before(&free_list, &(base->page_link));
    buddy_new(1024);
    cprintf("buddy init end\n");
}

int buddy_alloc(int size) {
    unsigned index = 0;
    unsigned node_size;
    unsigned offset = 0;

    if (size <= 0)
        size = 1;
    else if (!IS_POWER_OF_2(size))
        size = 1 << (ROUND_DOWN_LOG(size) + 1);

    if (buddy1.longest[index] < size)
        return -1;

    for (node_size = buddy1.size; node_size != size; node_size /= 2) {
        unsigned left = buddy1.longest[LEFT_LEAF(index)];
        unsigned right = buddy1.longest[RIGHT_LEAF(index)];
        if (left > right) {
            if (right > size)
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

    buddy1.longest[index] = 0;
    offset = (index + 1) * node_size - buddy1.size;

    while (index) {
        index = PARENT(index);
        buddy1.longest[index] = MAX(buddy1.longest[LEFT_LEAF(index)],
                                    buddy1.longest[RIGHT_LEAF(index)]);
    }
    return offset;
}

static struct Page* buddy_alloc_pages(size_t n) {
    cprintf("buddy alloc n=%d\n", n);
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    int offset = buddy_alloc(n);
    cprintf("offset=%d\n", offset);
    // struct Page* page = le2page((list_entry_t *)&free_list, page_link) +
    // offset;
    struct Page* page = NULL;
    list_entry_t* le = &free_list;
    page = le2page(list_next(le), page_link) + offset;
    ClearPageProperty(page);
    nr_free -= n;
    return page;
    // cprintf("buddy alloc end\n");
}

void buddy_free(int offset) {
    unsigned node_size, index = 0;
    unsigned left_longest, right_longest;
    cprintf("%d\n", offset);

    assert(offset >= 0 && offset < buddy1.size);

    node_size = 1;
    index = offset + buddy1.size - 1;

    for (; buddy1.longest[index]; index = PARENT(index)) {
        node_size *= 2;
        if (index == 0)
            return;
    }

    buddy1.longest[index] = node_size;

    while (index) {
        index = PARENT(index);
        node_size *= 2;

        left_longest = buddy1.longest[LEFT_LEAF(index)];
        right_longest = buddy1.longest[RIGHT_LEAF(index)];

        if (left_longest + right_longest == node_size)
            buddy1.longest[index] = node_size;
        else
            buddy1.longest[index] = MAX(left_longest, right_longest);
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
    buddy_free(base - le2page((list_entry_t*)list_next(&free_list), page_link));
    nr_free += n;
}

static size_t buddy_nr_free_pages(void) {
    return nr_free;
}

static void buddy_check(void) {
    cprintf("buddy check\n");
    struct Page *p0, *A, *B, *C, *D;
    p0 = A = B = C = D = NULL;

    assert((p0 = alloc_page()) != NULL);
    assert((A = alloc_page()) != NULL);
    assert((B = alloc_page()) != NULL);

    assert(p0 != A && p0 != B && A != B);
    assert(page_ref(p0) == 0 && page_ref(A) == 0 && page_ref(B) == 0);
    cprintf("buddy check1\n");

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

    p0 = alloc_pages(1024);
    cprintf("p0 %p\n", p0);
    assert(p0 == A);
    //以下是根据链接中的样例测试编写的
    A = alloc_pages(70);
    B = alloc_pages(35);
    cprintf("A %p\n", A);
    cprintf("B %p\n", B);
    assert(A + 128 == B);  //检查是否相邻
    cprintf("A %p\n", A);
    cprintf("B %p\n", B);
    C = alloc_pages(80);
    assert(A + 256 == C);  //检查C有没有和A重叠
    cprintf("C %p\n", C);
    free_pages(A, 70);  //释放A
    cprintf("B %p\n", B);
    D = alloc_pages(60);
    cprintf("D %p\n", D);
    assert(B + 64 == D);  //检查B，D是否相邻
    free_pages(B, 35);
    cprintf("D %p\n", D);
    free_pages(D, 60);
    cprintf("C %p\n", C);
    free_pages(C, 80);
    free_pages(p0, 1000);  //全部释放
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