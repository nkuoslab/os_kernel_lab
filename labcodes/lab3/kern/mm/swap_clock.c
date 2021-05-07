#include <defs.h>
#include <list.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_clock.h>
#include <x86.h>

struct Page pra_page_head;

static int _clock_init_mm(struct mm_struct* mm) {
    pra_page_head.ref = 0;
    list_init(&(pra_page_head.pra_page_link));
    mm->sm_priv = &(pra_page_head.pra_page_link);
    // cprintf(" mm->sm_priv %x in clock_init_mm\n",mm->sm_priv);
    return 0;
}

static int _clock_map_swappable(struct mm_struct* mm,
                                uintptr_t addr,
                                struct Page* page,
                                int swap_in) {
    // 得到链表头节点
    list_entry_t* head = (list_entry_t*)mm->sm_priv;
    // 找到page结构中的链表节点
    list_entry_t* entry = &(page->pra_page_link);
    assert(entry != NULL && head != NULL);
    // 将其连入链表
    list_add_before(head, entry);
    return 0;
}

static int _clock_swap_out_victim(struct mm_struct* mm,
                                  struct Page** ptr_page,
                                  int in_tick) {
    list_entry_t* current = (list_entry_t*)mm->sm_priv;
    assert(current != NULL);
    assert(in_tick == 0);
    list_entry_t* record = current;
    struct Page* page = NULL;
    int i;
    for (i = 0; i < 2; ++i) {
        current = record;
        while (1) {
            page = le2page(current, pra_page_link);
            if (page->ref != 0) {
                pte_t* ptep = get_pte(mm->pgdir, page->pra_vaddr, 0);
                if ((*ptep & (PTE_A | PTE_D)) == 0) {
                    mm->sm_priv = list_next(current);
                    *ptr_page = le2page(current, pra_page_link);
                    list_del(current);
                    return 0;
                }
            }
            current = list_next(current);
            if (current == record) {
                break;
            }
        }
        current = record;
        while (1) {
            page = le2page(current, pra_page_link);
            if (page->ref != 0) {
                pte_t* ptep = get_pte(mm->pgdir, page->pra_vaddr, 0);
                if ((*ptep & PTE_A) == 0) {
                    mm->sm_priv = list_next(current);
                    *ptr_page = le2page(current, pra_page_link);
                    list_del(current);
                    return 0;
                } else {
                    *ptep &= ~PTE_A;
                }
            }
            current = list_next(current);
            if (current == record) {
                break;
            }
        }
    }
}

static int _clock_check_swap(void) {
    cprintf("write Virt Page c in clock_check_swap\n");
    *(unsigned char*)0x3000 = 0x0c;
    assert(pgfault_num == 4);
    cprintf("write Virt Page a in clock_check_swap\n");
    *(unsigned char*)0x1000 = 0x0a;
    assert(pgfault_num == 4);
    cprintf("write Virt Page d in clock_check_swap\n");
    *(unsigned char*)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    cprintf("write Virt Page b in clock_check_swap\n");
    *(unsigned char*)0x2000 = 0x0b;
    assert(pgfault_num == 4);
    cprintf("write Virt Page e in clock_check_swap\n");
    *(unsigned char*)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    cprintf("write Virt Page b in clock_check_swap\n");
    *(unsigned char*)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    cprintf("write Virt Page a in clock_check_swap\n");
    *(unsigned char*)0x1000 = 0x0a;
    assert(pgfault_num == 6);
    cprintf("write Virt Page b in clock_check_swap\n");
    *(unsigned char*)0x2000 = 0x0b;
    assert(pgfault_num == 6);
    cprintf("write Virt Page c in clock_check_swap\n");
    *(unsigned char*)0x3000 = 0x0c;
    assert(pgfault_num == 7);
    cprintf("write Virt Page d in clock_check_swap\n");
    *(unsigned char*)0x4000 = 0x0d;
    assert(pgfault_num == 8);
    cprintf("write Virt Page e in clock_check_swap\n");
    *(unsigned char*)0x5000 = 0x0e;
    assert(pgfault_num == 9);
    cprintf("write Virt Page a in clock_check_swap\n");
    assert(*(unsigned char*)0x1000 == 0x0a);
    *(unsigned char*)0x1000 = 0x0a;
    assert(pgfault_num == 9);
    cprintf("read Virt Page b in clock_check_swap\n");
    assert(*(unsigned char*)0x2000 == 0x0b);
    assert(pgfault_num == 10);
    cprintf("read Virt Page c in clock_check_swap\n");
    assert(*(unsigned char*)0x3000 == 0x0c);
    assert(pgfault_num == 11);
    cprintf("read Virt Page a in clock_check_swap\n");
    assert(*(unsigned char*)0x1000 == 0x0a);
    assert(pgfault_num == 12);
    cprintf("read Virt Page d in clock_check_swap\n");
    assert(*(unsigned char*)0x4000 == 0x0d);
    assert(pgfault_num == 13);
    cprintf("read Virt Page b in clock_check_swap\n");
    *(unsigned char*)0x1000 = 0x0a;
    assert(*(unsigned char*)0x3000 == 0x0c);
    assert(*(unsigned char*)0x4000 == 0x0d);
    assert(*(unsigned char*)0x5000 == 0x0e);
    assert(*(unsigned char*)0x2000 == 0x0b);
    assert(pgfault_num == 14);
    return 0;
}

static int _clock_init(void) {
    return 0;
}

static int _clock_set_unswappable(struct mm_struct* mm, uintptr_t addr) {
    return 0;
}

static int _clock_tick_event(struct mm_struct* mm) {
    return 0;
}

struct swap_manager swap_manager_clock = {
    .name = "clock swap manager",
    .init = &_clock_init,
    .init_mm = &_clock_init_mm,
    .tick_event = &_clock_tick_event,
    .map_swappable = &_clock_map_swappable,
    .set_unswappable = &_clock_set_unswappable,
    .swap_out_victim = &_clock_swap_out_victim,
    .check_swap = &_clock_check_swap,
};
