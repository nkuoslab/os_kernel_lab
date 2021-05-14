<center><h3>实验三 虚拟内存管理</h3></center>
<div align='right'>学号： 1911463 姓名: 时浩铭 </div>
<div align='right'>学号： 1911477 姓名: 王耀群 </div>
<div align='right'>学号： 1911547 姓名: 李文婕 </div>

### 中断的处理过程

`trap`函数（定义在`trap.c`中）是对中断进行处理的过程，所有的中断在经过中断入口函数`__alltraps`预处理后 (定义在` trapasm.S`中) ，都会跳转到这里。在处理过程中，根据不同的中断类型，进行相应的处理。在相应的处理过程结束以后，`trap`将会返回，被中断的程序会继续运行。

整个中断处理流程大致如下：

1. 产生中断后，`CPU` 跳转到相应的中断处理入口 (`vectors`)，并在栈中压入相应的` error_code`（是否存在与异常号相关） 以及 `trap_no`，然后跳转到` alltraps` 函数入口。

   在栈中保存当前被打断程序的` trapframe` 结构(参见过程`trapentry.S`)。设置` kernel `(内核) 的数据段寄存器，最后压入` esp`，作为 `trap `函数参数(`struct trapframe * tf`) 并跳转到中断处理函数` trap `处

2. `trap`函数中，根据不同的中断号作不同的处理。

3. 结束` trap `函数的执行后，通过` ret `指令返回到 `alltraps `执行过程。从栈中恢复所有寄存器的值。调整` esp `的值。跳过栈中的` trap_no` 与 `error_code`，使`esp`指向中断返回` eip`，通过` iret` 调用恢复 `cs`、`eflag`以及 `eip`，继续执行。

### `ucore`的虚拟内存管理框架

在有了分页机制后，程序员或`CPU`“看到”的地址已经不是实际的物理地址了，这已经有一层虚拟化，我们可简称为内存地址虚拟化。

通过内存地址虚拟化，可以使得软件在没有访问某虚拟内存地址时不分配具体的物理内存，而只有在实际访问某虚拟内存地址时，操作系统再动态地分配物理内存，建立虚拟内存到物理内存的页映射关系，这种技术称为按需分页（`demand paging`）。把不经常访问的数据所占的内存空间临时写到硬盘上，这样可以腾出更多的空闲内存空间给经常访问的数据；当CPU访问到不经常访问的数据时，再把这些数据从硬盘读入到内存中，这种技术称为页换入换出（`page swap in/out`）。这种内存管理技术给了程序员更大的内存“空间”，从而可以让更多的程序在内存中并发运行。

#### `vma_struct`

`ucore`中用`vma_struct`来描述合法的连续虚拟内存空间块，一个进程合法的虚拟地址空间段将以`vma`集合的方式表示。

按照`vma_struct`对应的虚拟地址空间的高低顺序(还是从低地址到高地址)可以组成一个双向循环链表，与`lab2`中的`free_list`类似。

```c
// the virtual continuous memory area(vma), [vm_start, vm_end), 
// addr belong to a vma means  vma.vm_start<= addr <vma.vm_end 
struct vma_struct {
    struct mm_struct *vm_mm; // the set of vma using the same PDT 
    uintptr_t vm_start;      // start addr of vma      
    uintptr_t vm_end;        // end addr of vma, not include the vm_end itself
    uint32_t vm_flags;       // flags of vma
    list_entry_t list_link;  // linear list link which sorted by start addr of vma
};

#define le2vma(le, member)                  \
    to_struct((le), struct vma_struct, member)

//flags中目前用到的属性 可读、可写、可执行
#define VM_READ                 0x00000001
#define VM_WRITE                0x00000002
#define VM_EXEC                 0x00000004
```

这里由于与`free_list`几乎完全一致，不再多说。几个变量意思很明显，其中第一个变量`mm_struct`是另一个结构，具体看下面。

#### `mm_struct`

`mm_struct`作为一个进程的内存管理器，统一管理一个进程的虚拟内存与物理内存。

```c
// the control struct for a set of vma using the same PDT
struct mm_struct {
    // 连续虚拟内存块链表头 (内部节点虚拟内存块的起始、截止地址必须全局有序，且不能出现重叠)
    list_entry_t mmap_list;        // linear list link which sorted by start addr of vma
    // 当前访问的mmap_list链表中的vma块(由于局部性原理，之前访问过的vma有更大可能会在后续继续访问，该缓存可以减少从mmap_list中进行遍历查找的次数，提高效率)
    struct vma_struct *mmap_cache; // current accessed vma, used for speed purpose
    // 当前mm_struct关联的页目录表
    pde_t *pgdir;                  // the PDT of these vma
    // 当前mm_struct->mmap_list中vma块的数量
    int map_count;                 // the count of these vma
    // 用于虚拟内存置换算法的属性，使用void*指针做到通用 (lab中默认的swap_fifo替换算法中，将其做为了一个先进先出链表队列)
    // sm_priv指向用来链接记录页访问情况的链表头，这建立了mm_struct和后续要讲到的swap_manager之间的联系
    void *sm_priv;                   // the private data for swap manager
};
```

他们之间的关系如下图：

![](https://yuerer.com/images/mm_vma.png)

然后再看一些相关的函数

#### `mm_create`

创建一个`mm_struct`并初始化。

```c
// vmm.c
// mm_create -  alloc a mm_struct & initialize it.
struct mm_struct* mm_create(void) {
    // 分配空间
    struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));

    if (mm != NULL) {
        // 初始化双向链表
        list_init(&(mm->mmap_list));
        // 初始化变量
        mm->mmap_cache = NULL;
        mm->pgdir = NULL;
        mm->map_count = 0;
        
        //交换分区的初始化
        if (swap_init_ok)
            swap_init_mm(mm);
        else
            mm->sm_priv = NULL;
    }
    return mm;
}
```

关于`kmalloc`，是调用了`pmm_manager`的`alloc_pages`来分配一些物理内存。

```c
// pmm.c
void* kmalloc(size_t n) {
    void* ptr = NULL;
    struct Page* base = NULL;
    assert(n > 0 && n < 1024 * 0124);
    int num_pages = (n + PGSIZE - 1) / PGSIZE;
    base = alloc_pages(num_pages);
    assert(base != NULL);
    ptr = page2kva(base);
    return ptr;
}
```

#### `vma_create`

创建一个`vma_struct`并初始化。代码很简单，不多说。

```c
// vmm.c
// vma_create - alloc a vma_struct & initialize it. (addr range:vm_start~vm_end)
struct vma_struct* vma_create(uintptr_t vm_start,
                              uintptr_t vm_end,
                              uint32_t vm_flags) {
    struct vma_struct* vma = kmalloc(sizeof(struct vma_struct));

    if (vma != NULL) {
        vma->vm_start = vm_start;
        vma->vm_end = vm_end;
        vma->vm_flags = vm_flags;
    }
    return vma;
}
```

#### `find_vma`

根据传入的虚拟地址，找到这个地址所在的`vma_struct`。

思路很简单，遍历循环链表即可。类似之前`free_list`的查找，区别在于我们有一个`mmap_cache`记录上次访问过后的`vma`，我们从这个`vma`开始查找。

```c
// vmm.c
// find_vma - find a vma  (vma->vm_start <= addr < vma_vm_end)
struct vma_struct* find_vma(struct mm_struct* mm, uintptr_t addr) {
    struct vma_struct* vma = NULL;
    if (mm != NULL) {
        vma = mm->mmap_cache;
        // 如果cache这个vma不满足再循环遍历去找
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr)) {
            bool found = 0;
            // 这里从链表头开始找
            list_entry_t *list = &(mm->mmap_list), *le = list;
            while ((le = list_next(le)) != list) {
                // le2vma 代码在前面 原理与le2page一样 不明白去看lab2的报告
                vma = le2vma(le, list_link);
                if (vma->vm_start <= addr && addr < vma->vm_end) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                vma = NULL;
            }
        }
        if (vma != NULL) {
            // 最后再更新cache
            mm->mmap_cache = vma;
        }
    }
    return vma;
}
```

#### `check_vma_overlap`

检查两个连续的`vma`有没有重合。

```c
// vmm.c
// check_vma_overlap - check if vma1 overlaps vma2 ?
static inline void check_vma_overlap(struct vma_struct* prev,
                                     struct vma_struct* next) {
    assert(prev->vm_start < prev->vm_end);
    assert(prev->vm_end <= next->vm_start);
    assert(next->vm_start < next->vm_end);
}
```

#### `insert_vma_struct`

向`mm_struct`里面插入一块`vma`，需要找到正确的位置，并检查是否有重合的区域。

```c
// vmm.c
// insert_vma_struct -insert vma in mm's list link
void insert_vma_struct(struct mm_struct* mm, struct vma_struct* vma) {
    assert(vma->vm_start < vma->vm_end);
    // 也是从头开始找 一个prev 一个next 是为了检测重合
    list_entry_t* list = &(mm->mmap_list);
    list_entry_t *le_prev = list, *le_next;

    list_entry_t* le = list;
    while ((le = list_next(le)) != list) {
        struct vma_struct* mmap_prev = le2vma(le, list_link);
        if (mmap_prev->vm_start > vma->vm_start) {
            break;
        }
        le_prev = le;
    }

    le_next = list_next(le_prev);

    /* check overlap */
    // 检查两次 prev和vma 以及 vma和next 因为最终顺序是 prev vma next 两两之间都不重合
    if (le_prev != list) {
        check_vma_overlap(le2vma(le_prev, list_link), vma);
    }
    if (le_next != list) {
        check_vma_overlap(vma, le2vma(le_next, list_link));
    }
	
    // 给vma设置其mm，并插入到之前找到的位置
    vma->vm_mm = mm;
    list_add_after(le_prev, &(vma->list_link));
	
    // 给mm中记录vma数量的变量count加一
    mm->map_count++;
}
```

#### `mm_destory`

释放`mm_struct`和里面所有的`vma_struct`。

```c
// vmm.c
// mm_destroy - free mm and mm internal fields
void mm_destroy(struct mm_struct* mm) {
    list_entry_t *list = &(mm->mmap_list), *le;
    // 从头部开始循环释放
    while ((le = list_next(list)) != list) {
        // 删除头部第一个vma
        // 注意循环条件之中 list_next(list) 一直再取第一个vma，因为每次都删除第一个vma
        list_del(le);
        // 释放所占的内存区域
        kfree(le2vma(le, list_link), sizeof(struct vma_struct));  // kfree vma
    }
    // vma都释放完了再释放mm
    kfree(mm, sizeof(struct mm_struct));  // kfree mm
    mm = NULL;
}
```

`kfree`与`kmalloc`类似，也是调用`pmm_manager`的`free_pages`，这里就不贴代码了。

#### `check_...`

还有几个`check`函数就不贴了。

### 缺页异常处理

当启动分页机制以后，如果一条指令或数据的虚拟地址所对应的物理页框不在内存中或者访问的类型有错误（比如写一个只读页或用户态程序访问内核态的数据等），就会发生页访问异常。产生页访问异常的原因主要有：

- 目标页帧不存在（页表项全为0，即该线性地址与物理地址尚未建立映射或者已经撤销)；
- 相应的物理页帧不在内存中（页表项非空，但`Present`标志位为0，比如在`swap`分区或磁盘文件上)，这在本次实验中会出现，我们将在下面介绍换页机制实现时进一步讲解如何处理；
- 不满足访问权限(此时页表项P标志=1，但低权限的程序试图访问高权限的地址空间，或者有程序试图写只读页面).

当出现上面情况之一，那么就会产生页面`page fault`异常。`CPU`会把产生异常的线性地址存储在`CR2`中，并且把表示页访问异常类型的值（简称页访问异常错误码，`errorCode`）保存在中断栈中。

`CR2`是页故障线性地址寄存器，保存最后一次出现页故障的全32位线性地址。

页访问异常错误码有32位。位0为1表示对应物理页存在；位1为1表示写异常（比如写了只读页)；位2为1表示访问权限异常（比如用户态程序访问内核空间的数据）

`CPU`在当前内核栈保存当前被打断的程序现场，即依次压入当前被打断程序使用的`EFLAGS`，`CS`，`EIP`，`errorCode`；由于页访问异常的中断号是`0xE`，`CPU`把异常中断号`0xE`对应的中断服务例程的地址（`vectors.S`中的标号`vector14`处）加载到`CS`和`EIP`寄存器中，开始执行中断服务例程。这时`ucore`开始处理异常中断，首先需要保存硬件没有保存的寄存器。在`vectors.S`中的标号`vector14`处先把中断号压入内核栈，然后再在`trapentry.S`中的标号`__alltraps`处把`DS`、`ES`和其他通用寄存器都压栈。自此，被打断的程序执行现场（`context`）被保存在内核栈中。接下来，在`trap.c`的`trap`函数开始了中断服务例程的处理流程，大致调用关系为：

`trap--> trap_dispatch-->pgfault_handler-->do_pgfault`

`ucore OS`会把这个值保存在`struct trapframe `中`tf_err`成员变量中。而中断服务例程会调用页访问异常处理函数`do_pgfault`进行具体处理。这里的页访问异常处理是实现按需分页、页换入换出机制的关键之处。

`ucore`中`do_pgfault`函数是完成页访问异常处理的主要函数，它根据从`CPU`的控制寄存器`CR2`中获取的页访问异常的虚拟地址以及根据`errorCode`的错误类型来查找此地址是否在某个`VMA`的地址范围内以及是否满足正确的读写权限，如果在此范围内并且权限也正确，这认为这是一次合法访问，但没有建立虚实对应关系。所以需要分配一个空闲的内存页，并修改页表完成虚地址到物理地址的映射，刷新`TLB`，然后调用`iret`，返回到产生页访问异常的指令处重新执行此指令。如果该虚地址不在某`VMA`范围内，则认为是一次非法访问。

### 练习1：给未被映射的地址映射上物理页

完成`do_pgfault`（`mm/vmm.c`）函数，给未被映射的地址映射上物理页。设置访问权限的时候需要参考页面所在` VMA `的权限，同时需要注意映射物理页时需要操作内存控制结构所指定的页表，而不是内核的页表。

就是实现一下缺页异常的处理的关键函数`do_pgfault`。

这里面需要根据不同的`error_code`来判断是怎样导致的缺页异常。是因为缺页引发的还是由于非法的地址空间的访问或越级访问等引发的。

如果是缺页引发的，就要判断页表项是否全为0还是只有P位为0。前者表示目标页帧不存在，后者表示页帧不在内存中。前者需要分配一个物理页，并建立虚实映射关系，后者需要把交换分区中的数据读出并覆盖对应物理页。

这个函数有不同的返回值以区分遇到的不同情况。

```c
/* kernel error codes -- keep in sync with list in lib/printfmt.c */
#define E_UNSPECIFIED       1   // Unspecified or unknown problem
#define E_BAD_PROC          2   // Process doesn't exist or otherwise
#define E_INVAL             3   // Invalid parameter
#define E_NO_MEM            4   // Request failed due to memory shortage
#define E_NO_FREE_PROC      5   // Attempt to create a new process beyond
#define E_FAULT             6   // Memory fault
```

先看几个用到的函数。

```c
// pmm.c
// 建立la与指定物理页之间的映射关系
// page_insert - build the map of phy addr of an Page with the linear addr la
// paramemters:
//  pgdir: the kernel virtual base address of PDT
//  page:  the Page which need to map
//  la:    the linear address need to map
//  perm:  the permission of this Page which is setted in related pte
// return value: always 0
// note: PT is changed, so the TLB need to be invalidate
int page_insert(pde_t* pgdir, struct Page* page, uintptr_t la, uint32_t perm) {
    // 找到la对应的pte
    pte_t* ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL) {
        return -E_NO_MEM;
    }
    page_ref_inc(page);
    // 如果原来有映射关系，先取消
    if (*ptep & PTE_P) {
        struct Page* p = pte2page(*ptep);
        if (p == page) {
            page_ref_dec(page);
        } else {
            page_remove_pte(pgdir, la, ptep);
        }
    }
    // 再重新设置
    *ptep = page2pa(page) | PTE_P | perm;
    // 标记TLB中的无效
    tlb_invalidate(pgdir, la);
    return 0;
}

// 在pdgir指向的页表，给la对应的页分配一个物理页进行虚实地址映射
// pgdir_alloc_page - call alloc_page & page_insert functions to
//                  - allocate a page size memory & setup an addr map
//                  - pa<->la with linear address la and the PDT pgdir
struct Page* pgdir_alloc_page(pde_t* pgdir, uintptr_t la, uint32_t perm) {
    // 分配一个新的物理页
    struct Page* page = alloc_page();
    if (page != NULL) {
        // 建立la对应二级页表项(位于pgdir页表中)与page物理页的映射关系
        if (page_insert(pgdir, page, la, perm) != 0) {
            // 正常不会运行到这里
            free_page(page);
            return NULL;
        }
        // 如果启用了swap交换分区
        if (swap_init_ok) {
            // 将新映射的这一个page物理页设置为可交换的，并纳入全局swap交换管理器中管理
            swap_map_swappable(check_mm_struct, la, page, 0);
            // 设置这一物理页关联的虚拟内存
            page->pra_vaddr = la;
            // 校验这个新分配出来的物理页page是否引用次数正好为1
            assert(page_ref(page) == 1);
            // cprintf("get No. %d  page: pra_vaddr %x, pra_link.prev %x,
            // pra_link_next %x in pgdir_alloc_page\n", (page-pages),
            // page->pra_vaddr,page->pra_page_link.prev,
            // page->pra_page_link.next);
        }
    }

    return page;
}

// swap.c
// 从磁盘换入到主存
// 这里先不用细看，练习2进一步介绍
int swap_in(struct mm_struct* mm, uintptr_t addr, struct Page** ptr_result) {
    // 先分配一个物理页
    struct Page* result = alloc_page();
    assert(result != NULL);
	
    // 得到地址对应的页表项
    pte_t* ptep = get_pte(mm->pgdir, addr, 0);
    // cprintf("SWAP: load ptep %x swap entry %d to vaddr 0x%08x, page %x, No
    // %d\n", ptep, (*ptep)>>8, addr, result, (result-pages));

    int r;
    // 将磁盘中的对应数据写入result中
    if ((r = swapfs_read((*ptep), result)) != 0) {
        assert(r != 0);
    }
    cprintf("swap_in: load disk swap entry %d with swap_page in vadr 0x%x\n",
            (*ptep) >> 8, addr);
    // 修改ptr_result使其指向对应的Page
    *ptr_result = result;
    return 0;
}
```

最后根据注释写出代码。

```c
int do_pgfault(struct mm_struct* mm, uint32_t error_code, uintptr_t addr) {
    int ret = -E_INVAL;
    // try to find a vma which include addr
    // 从mm中找到这个地址对应的vma
    struct vma_struct* vma = find_vma(mm, addr);

    // 缺页异常数+1
    pgfault_num++;
    // If the addr is in the range of a mm's vma?
    // 找不到则failed
    // 这里应该不会发生第二个||后面的，因为在find_vma里面找不到的返回就是NULL
    if (vma == NULL || vma->vm_start > addr) {
        cprintf("not valid addr %x, and  can not find it in vma\n", addr);
        goto failed;
    }
    // check the error_code
    // 下面根据error_code判断异常发生的原因
    // 第0位，物理页是否存在；第1位，写异常/读异常
    switch (error_code & 3) {
        default:
            /* error code flag : default is 3 ( W/R=1, P=1): write, present */
            // 两位都是1，说明物理页存在发生写异常，缺页异常
        case 2: /* error code flag : (W/R=1, P=0): write, not present */
            // 物理页不存在发生写异常
            if (!(vma->vm_flags & VM_WRITE)) {
                // 如果这块虚拟地址不可写就failed
                cprintf(
                    "do_pgfault failed: error code flag = write AND not "
                    "present, but the addr's vma cannot write\n");
                goto failed;
            }
            // 否则发生缺页异常
            break;
        case 1: /* error code flag : (W/R=0, P=1): read, present */
            // 物理页存在读异常 可能发生的是权限导致的异常，failed
            cprintf("do_pgfault failed: error code flag = read AND present\n");
            goto failed;
        case 0: /* error code flag : (W/R=0, P=0): read, not present */
            // 读异常不存在
            if (!(vma->vm_flags & (VM_READ | VM_EXEC))) {
                // 如果不可读或者不可执行failed
                cprintf(
                    "do_pgfault failed: error code flag = read AND not "
                    "present, but the addr's vma cannot read or exec\n");
                goto failed;
            }
    }
    // 缺页异常三种情况
    /* IF (write an existed addr ) OR
     *    (write an non_existed addr && addr is writable) OR
     *    (read  an non_existed addr && addr is readable)
     * THEN
     *    continue process
     */
    // 设置pte用的权限
    uint32_t perm = PTE_U;
    if (vma->vm_flags & VM_WRITE) {
        perm |= PTE_W;
    }
    // 地址按页向下取整，因为映射的是整个页的关系
    addr = ROUNDDOWN(addr, PGSIZE);

    ret = -E_NO_MEM;

    pte_t* ptep = NULL;
    ptep = get_pte(mm->pgdir, addr, 1);
    // 如果一个pte全是0，表示目标页帧不存在，需要分配一个物理页并建立虚实映射关系
    if (*ptep == 0) {
        // 为空说明分配失败了
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) {
            cprintf("print alloc page failed.\n");
            goto failed;
        }
    } else {
        // 如果不是全为0，说明可能是之前被交换到了swap磁盘中，需要换出来
        // 如果开启了
        if (swap_init_ok) {
            struct Page* page = NULL;
            if (swap_in(mm, addr, &page)) {
                // 如果返回值不为0说明出了问题 failed
                cprintf("swap in failed.\n");
                goto failed;
            }
            // 建立映射关系，更新PTE，因为换出去以后PTE存的是关于磁盘扇区的信息
            page_insert(mm->pgdir, page, addr, perm);
            // 当前page是为可交换的，将其加入全局虚拟内存交换管理器的管理
            swap_map_swappable(mm, addr, page, 1);
            // 设置 这一页的虚拟地址
            page->pra_vaddr = addr;
        } else {  // 没开启不应该执行到这里
            cprintf("no swap_init_ok but ptep is %x, failed\n", *ptep);
            goto failed;
        }
    }
    ret = 0;
failed:
    return ret;
}
```

写好后`make qemu`执行。

```
-------------------- BEGIN --------------------
PDE(0e0) c0000000-f8000000 38000000 urw
  |-- PTE(38000) c0000000-f8000000 38000000 -rw
PDE(001) fac00000-fb000000 00400000 -rw
  |-- PTE(000e0) faf00000-fafe0000 000e0000 urw
  |-- PTE(00001) fafeb000-fafec000 00001000 -rw
--------------------- END ---------------------
check_vma_struct() succeeded!
page fault at 0x00000100: K/W [no page found].
check_pgfault() succeeded!
```

此时会在下面的异常卡住，但是看到前面的`check_pgfault() succeeded!`说明正确。

#### `trap.c`

下面看一下`do_pgfault`是怎么被调用的。

```c
// trap.c

static void trap_dispatch(struct trapframe* tf) {
    char c;

    int ret;

    switch (tf->tf_trapno) {
        case T_PGFLT:  // page fault
            if ((ret = pgfault_handler(tf)) != 0) {
                print_trapframe(tf);
                panic("handle pgfault failed. %e\n", ret);
            }
            break;
```

在`trap_dispatch`中根据中断号选择缺页异常的处理。调用`pgfault_handler`进行处理。

```c
static int pgfault_handler(struct trapframe* tf) {
    extern struct mm_struct* check_mm_struct;
    print_pgfault(tf);
    if (check_mm_struct != NULL) {
        // rcr2() 通过汇编获得cr2寄存器的值
        return do_pgfault(check_mm_struct, tf->tf_err, rcr2());
    }
    panic("unhandled page fault.\n");
}
```

这里有外部变量`check_mm_struct`，就是出现缺页异常的程序的`mm_struct`，但是由于目前还有操作系统一个线程，所以我们只在`check`函数里虚拟一些情况做检查。

在`pgfault_handler`中，再调用`do_pgfault`，并根据返回值来判断是否成功。

#### 如果`ucore`的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？

第一个问题与`lab2`中类似，不再回答。（这个也差不多）

页访问异常会将产生页访问异常的线性地址存入` cr2 `寄存器中，并且给出错误码` error_code `，说明页访问异常的具体原因。

`uCore OS `会将其 存入` struct trapframe `中` tf_err `等到中断服务例程调用页访问异常处理函数(`do_pgfault()`) 时再判断具体原因 。

若不在某个`VMA`的地址范围内或不满足正确的读写权限则是非法访问。

若在此范围且权限也正确，则认为是合法访问，只是没有建立虚实对应关系，应分配一页，并修改页表，完成虚拟地址到物理地址的映射，刷新 `TLB` ，最后再调用` iret` 重新执行引发页访问异常的那条指令。

若是在外存中则将其换入内存，刷新` TLB `，然后退出中断服务例程，重新执行引发页访问异常的那条指令。

### 页换入换出机制

操作系统给用户态的应用程序提供了一个虚拟的“大容量”内存空间，而实际的物理内存空间又没有那么大。所以操作系统就就“瞒着”应用程序，只把应用程序中“常用”的数据和代码放在物理内存中，而不常用的数据和代码放在了硬盘这样的存储介质上。如果应用程序访问的是“常用”的数据和代码，那么操作系统已经放置在内存中了，不会出现什么问题。但当应用程序访问它认为应该在内存中的的数据或代码时，如果这些数据或代码不在内存中，则会产生页访问异常。这时，操作系统必须能够应对这种页访问异常，即尽快把应用程序当前需要的数据或代码放到内存中来，然后重新执行应用程序产生异常的访存指令。如果在把硬盘中对应的数据或代码调入内存前，操作系统发现物理内存已经没有空闲空间了，这时操作系统必须把它认为“不常用”的页换出到磁盘上去，以腾出内存空闲空间给应用程序所需的数据或代码。

在`lab3`中，我们实现`FIFO`替换机制。总是淘汰最先进入内存的页，即选择在内存中驻留时间最久的页予以淘汰。只需把一个应用程序在执行过程中已调入内存的页按先后次序链接成一个队列，队列头指向内存中驻留时间最久的页，队列尾指向最近被调入内存的页。这样需要淘汰页时，从队列头很容易查找到需要淘汰的页。

#### 哪些页可以被换出？

并非所有的物理页都可以交换出去的，只有映射到用户空间且被用户程序直接访问的页面才能被交换，而被内核直接使用的内核空间的页面不能被换出。

也就是说，用户所在的虚拟空间对应的物理页可以被换出，而内核占的空间不能被换出。

#### 一个虚拟的页如何与硬盘上的扇区建立对应关系？

在`ucore`中，没有单独的另外构建一张虚拟页与磁盘扇区的映射表，而是巧妙地重复利用了二级页表项。

当一个`PTE`用来描述一般意义上的物理页时，显然它应该维护各种权限和映射关系，以及应该有`PTE_P`标记；但当它用来描述一个被置换出去的物理页时，它被用来维护该物理页与` swap `磁盘上扇区的映射关系，并且该` PTE `不应该由` MMU `将它解释成物理页映射(即没有` PTE_P` 标记)，与此同时对应的权限则交由` mm_struct `来维护，当对位于该页的内存地址进行访问的时候，必然导致` page fault`，然后`ucore`能够根据` PTE `描述的` swap `项将相应的物理页重新建立起来，并根据虚存所描述的权限重新设置好` PTE `使得内存访问能够继续正常进行。

也就是说，当`PTE_P`为1时表明所映射的物理页存在，访问正常；为0时代表不存在，此时`pte`表示的是虚拟页与磁盘扇区的对应关系。在后者的情况下，`ucore`用`pte`的高24位数据表明这一页的起始扇区号。但是为了区分全0的`pte`与从`swap`分区的第一个扇区开始存的页对应的`pte`，`ucore`规定`swap`分区从第9个扇区开始用（即空一个页的大小）。简单说就是，表明虚拟页与磁盘扇区对应关系的`pte`的高24位不全为0。

所以，`pte`就有以下三种情况：

- 全为0，代表未建立对应物理页的映射
- P位为1，代表已建立对应物理页的映射
- P位为0，但高24位不为0。代表所映射的物理页存在，只是被交换到了磁盘交换区中。

对于表明虚拟页与磁盘扇区的对应关系的`pte`，我们称它为`swap_entry_t`。

```c
// swap.c
/* *
 * swap_entry_t
 * --------------------------------------------
 * |         offset        |   reserved   | 0 |
 * --------------------------------------------
 *           24 bits            7 bits    1 bit
 * */

// memlayout.h
typedef pte_t swap_entry_t; //the pte can also be a swap entry
```

一个页大小`4KB`，而一个磁盘扇区大小`0.5KB`，因此一个页需要8个连续扇区来存储。

在`ucore`中为了简化设计，规定`offset*8`即为起始扇区号。

`ucore`可以保存`262144/8=32768`个页，即`128MB`的内存空间。`swap `分区的大小是` swapfs_init `里面根据磁盘驱动的接口计算出来的，目前` ucore` 里面要求` swap `磁盘至少包含` 1000 `个` page`，并且至多能使用 `1<<24 `个`page`。

#### 何时进行换入和换出操作？

当所访问的页存储在交换分区时（即`PTE`中高24位不为0而最低位为0），会发生页的换入。页的换入由`swap_in`完成。

`ucore`采用消极换出策略，即当请求空闲页时没有可供分配的物理页时才会发生页的换出。即当调用`alloc_page`并没有空闲的物理页时会调用`swap_out`完成页的换出。

在lab3中，用`check_mm_struct`来表示`ucore`认为所有合法的虚拟空间集合。（目前由于无其他线程所以只是测试用，无实际意义）

看一下lab3中的`alloc_page`。

```c
// pmm.c
// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page* alloc_pages(size_t n) {
    struct Page* page = NULL;
    bool intr_flag;

    while (1) {
        // 下面第一和第三行是同步用的
        local_intr_save(intr_flag);
        { page = pmm_manager->alloc_pages(n); }
        local_intr_restore(intr_flag);

        if (page != NULL || n > 1 || swap_init_ok == 0)
            break;
		
        // 当开启了页换入换出机制并且没有通过alloc_pages拿到所需的物理内存则会调用swap_out进行页的换出
        extern struct mm_struct* check_mm_struct;
        // cprintf("page %x, call swap_out in alloc_pages %d\n",page, n);
        swap_out(check_mm_struct, n, 0);
    }
    // cprintf("n %d,get page %x, No %d in alloc_pages\n",n,page,(page-pages));
    return page;
}
```

#### 如何设计数据结构以支持页替换算法？

##### `Page`

`ucore`对`Page`结构进行扩展以支持页替换算法。

也是用一个双向循环链表来记录被换出的页，其中链表头表示访问时间最近的页，链表尾表示访问时间最远的页。

```c
// memlayout.h
/* *
 * struct Page - Page descriptor structures. Each Page describes one
 * physical page. In kern/mm/pmm.h, you can find lots of useful functions
 * that convert Page to other data types, such as phyical address.
 * */
struct Page {
    int ref;                        // page frame's reference counter
    uint32_t flags;                 // array of flags that describe the status of the page frame
    unsigned int property;          // the num of free block, used in first fit pm manager
    list_entry_t page_link;         // free list link
    // 链表节点
    list_entry_t pra_page_link;     // used for pra (page replace algorithm)
    // 这个物理页对应的虚拟页的起始地址
    uintptr_t pra_vaddr;            // used for pra (page replace algorithm)
};

```

链表的头部`pra_list_head`定义在`swap_fifo.c`中。

链表的初始化在`_fifo_init_mm`中，其中也会设置`mm_struct`的`sm_priv`，将其设置为链表头。

```c
// swap_fifo.c
static int _fifo_init_mm(struct mm_struct* mm) {
    list_init(&pra_list_head);
    mm->sm_priv = &pra_list_head;
    // cprintf(" mm->sm_priv %x in fifo_init_mm\n",mm->sm_priv);
    return 0;
}
```

##### `swap_manager`

除扩展`Page`之外，还定义了`swap_manager`，作为管理换入换出机制的框架。类似于之前的`pmm_manager`，也是一个函数指针集合。

```c
// swap.h
struct swap_manager
{
     const char *name;
     /* Global initialization for the swap manager */
     // 初始化全局虚拟内存交换管理器
     int (*init)            (void);
     /* Initialize the priv data inside mm_struct */
     // 初始化设置所关联的全局内存管理器
     int (*init_mm)         (struct mm_struct *mm);
     /* Called when tick interrupt occured  */
     // 当时钟中断时被调用，可用于主动的swap交换策略
     int (*tick_event)      (struct mm_struct *mm);
     /* Called when map a swappable page into the mm_struct */
     // 当映射一个可交换Page物理页加入mm_struct时被调用
     int (*map_swappable)   (struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in);
     /* When a page is marked as shared, this routine is called to
      * delete the addr entry from the swap manager */
     // 当一个页面被标记为共享页面，该函数例程会被调用。
     // 用于将addr对应的虚拟页，从swap_manager中移除，阻止其被调度置换到磁盘中
     int (*set_unswappable) (struct mm_struct *mm, uintptr_t addr);
     /* Try to swap out a page, return then victim */
     // 当试图换出一个物理页时，返回被选中的页面(被牺牲的页面)
     int (*swap_out_victim) (struct mm_struct *mm, struct Page **ptr_page, int in_tick);
     /* check the page relpacement algorithm */
     int (*check_swap)(void);     
};
```

其中两个比较重要的函数是`map_swappable`和`swap_out_victim`。前者用于记录页访问情况相关属性（更新记录换出页的链表），后者用于挑选出被换出的页。

#### 如何完成页的换入换出机制？

主要再看下两个函数`swap_in`和`swap_out`。

`swap_in` 把内存页从磁盘换入到主存，发生在缺页异常（所访问的内存也保存在磁盘中）时，即练习1种处理缺页异常的函数`do_pgfault`中。

```c
// swap.c
// 从磁盘换入到主存
int swap_in(struct mm_struct* mm, uintptr_t addr, struct Page** ptr_result) {
    // 先分配一个物理页
    struct Page* result = alloc_page();
    assert(result != NULL);
	
    // 得到地址对应的页表项
    // 此时页表项中存储的是swap_entry_t
    pte_t* ptep = get_pte(mm->pgdir, addr, 0);
    // cprintf("SWAP: load ptep %x swap entry %d to vaddr 0x%08x, page %x, No
    // %d\n", ptep, (*ptep)>>8, addr, result, (result-pages));

    int r;
    // 将磁盘中的对应数据写入result中
    if ((r = swapfs_read((*ptep), result)) != 0) {
        assert(r != 0);
    }
    cprintf("swap_in: load disk swap entry %d with swap_page in vadr 0x%x\n",
            (*ptep) >> 8, addr);
    // 修改ptr_result使其指向对应的Page
    *ptr_result = result;
    return 0;
}

// swapfs.c
// 根据传入的swap_entry_t读取对应的磁盘扇区并保存在Page对应的物理内存处
// 这里面可以看出offset与扇区号的对应关系，即offset*8=起始扇区号
int swapfs_read(swap_entry_t entry, struct Page *page) {
    return ide_read_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT, page2kva(page), PAGE_NSECT);
}

// fs.h
#define SECTSIZE            512
#define PAGE_NSECT          (PGSIZE / SECTSIZE)

// swap.h
// 返回swap_entry_t的高24位，同时做一些检查
/* *
 * swap_offset - takes a swap_entry (saved in pte), and returns
 * the corresponding offset in swap mem_map.
 * */
#define swap_offset(entry) ({                                       \
               size_t __offset = (entry >> 8);                        \
               if (!(__offset > 0 && __offset < max_swap_offset)) {    \
                    panic("invalid swap_entry_t = %08x.\n", entry);    \
               }                                                    \
               __offset;                                            \
          })

// ide.c
// ideno 要读的磁盘的编号
// secno 读的起始扇区号
// dst 读到的内存地址
// nsecs 读的扇区数目
int ide_read_secs(unsigned short ideno, uint32_t secno, void *dst, size_t nsecs)
```

`swap_out`把内存页从内存换出到磁盘中，发生在请求物理页但却没有空闲物理页时，即`alloc_pages`函数中。

```c
// swap.c
// n 需要换出的内存页的个数
// in_tick 用于主动换出策略
int swap_out(struct mm_struct* mm, int n, int in_tick) {
    int i;
    for (i = 0; i != n; ++i) {
        uintptr_t v;
        // struct Page **ptr_page=NULL;
        struct Page* page;
        // cprintf("i %d, SWAP: call swap_out_victim\n",i);
        // 找到被换出的物理页，让page指向这个物理页
        int r = sm->swap_out_victim(mm, &page, in_tick);
        if (r != 0) {
            cprintf("i %d, swap_out: call swap_out_victim failed\n", i);
            break;
        }
        // assert(!PageReserved(page));

        // cprintf("SWAP: choose victim page 0x%08x\n", page);
		// 找到这个page对应的虚拟地址并得到对应的pte
        v = page->pra_vaddr;
        pte_t* ptep = get_pte(mm->pgdir, v, 0);
        assert((*ptep & PTE_P) != 0);
		// 把被换出的物理页写到磁盘上
        // page->pra_vaddr 是虚拟地址 除以PGSIZE即右移12位得到高20位 是物理页的索引
        // +1是因为交换分区的前8个扇区不存储
        // 再左移8位构成一个swap_entry_t
        if (swapfs_write((page->pra_vaddr / PGSIZE + 1) << 8, page) != 0) {
            cprintf("SWAP: failed to save\n");
            // 如果写入磁盘失败，重新令其加入swap管理器
            sm->map_swappable(mm, v, page, 0);
            continue;
        } else {
            cprintf(
                "swap_out: i %d, store page in vaddr 0x%x to disk swap entry "
                "%d\n",
                i, v, page->pra_vaddr / PGSIZE + 1);
            // 再修改pte为swap_entry_t，保存扇区号
            *ptep = (page->pra_vaddr / PGSIZE + 1) << 8;
            // 释放page对应的物理页
            free_page(page);
        }
		// 标记TLB不可用
        tlb_invalidate(mm->pgdir, v);
    }
    return i;
}

// swapfs.c
// 把page对应的物理页写到指定的磁盘位置
int swapfs_write(swap_entry_t entry, struct Page *page) {
    return ide_write_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT, page2kva(page), PAGE_NSECT);
}

// ide_write_secs参数与读一致
```

### 练习2：补充完成基于FIFO的页面替换算法`swap_manager_info`

```c
// swap_fifo.c
struct swap_manager swap_manager_fifo = {
    .name = "fifo swap manager",
    .init = &_fifo_init,
    .init_mm = &_fifo_init_mm,
    .tick_event = &_fifo_tick_event,
    .map_swappable = &_fifo_map_swappable,
    .set_unswappable = &_fifo_set_unswappable,
    .swap_out_victim = &_fifo_swap_out_victim,
    .check_swap = &_fifo_check_swap,
};
```

其中只有几个函数有实际意义，我们需要补全`map_swappable`和`swap_out_victim`。

#### `_fifo_init_mm`

链表的初始化在`_fifo_init_mm`中，其中也会设置`mm_struct`的`sm_priv`，将其设置为链表头。

```c
// swap_fifo.c
static int _fifo_init_mm(struct mm_struct* mm) {
    list_init(&pra_list_head);
    mm->sm_priv = &pra_list_head;
    // cprintf(" mm->sm_priv %x in fifo_init_mm\n",mm->sm_priv);
    return 0;
}
```

#### `_fifo_map_swappable`

记录页的相关属性，以便于`swap_manager`的管理 。

```c
/*
 * (3)_fifo_map_swappable: According FIFO PRA, we should link the most recent
 * arrival page at the back of pra_list_head qeueue
 */
static int _fifo_map_swappable(struct mm_struct* mm,
                               uintptr_t addr,
                               struct Page* page,
                               int swap_in) {
    // 得到链表头节点
    list_entry_t* head = (list_entry_t*)mm->sm_priv;
    // 找到page结构中的链表节点
    list_entry_t* entry = &(page->pra_page_link);

    assert(entry != NULL && head != NULL);
    // record the page access situlation
    /*LAB3 EXERCISE 2: YOUR CODE*/
    //(1)link the most recent arrival page at the back of the pra_list_head
    //qeueue.
    // 将其连入链表头的后面
    list_add(head, entry);
    return 0;
}
```

#### `_fifo_swap_out_victim`

找到被换出的页。返回的应该是链表尾部的，因为链表尾部最先进入。

```c
/*
 *  (4)_fifo_swap_out_victim: According FIFO PRA, we should unlink the  earliest
 * arrival page in front of pra_list_head queue, then assign the value of
 * *ptr_page to the addr of this page.
 */
static int _fifo_swap_out_victim(struct mm_struct* mm,
                                 struct Page** ptr_page,
                                 int in_tick) {
    // 拿到链表头部
    list_entry_t* head = (list_entry_t*)mm->sm_priv;
    assert(head != NULL);
    assert(in_tick == 0);
    /* Select the victim */
    /*LAB3 EXERCISE 2: YOUR CODE*/
    //(1)  unlink the  earliest arrival page in front of pra_list_head queue
    // 获得链表尾部的页并将其移除出链表
    list_entry_t *le = head->prev;
    list_del(le);
    //(2)  assign the value of *ptr_page to the addr of this page
    // 得到对应的Page结构，并保存在ptr_page中
    struct Page* page = le2page(le, pra_page_link);
    *ptr_page = page;
    return 0;
}
```

#### `_fifo_check_swap`

检测`fifo`是否正确。

记4-8KB为a，8-12KB为b，12-16KB为c，16-20KB为d，20-24KB为e

具体过程与课件上的图差不多，少了最后两个

[![gEmoo6.png](https://z3.ax1x.com/2021/04/30/gEmoo6.png)](https://imgtu.com/i/gEmoo6)

```c
static int _fifo_check_swap(void) {
    cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char*)0x3000 = 0x0c;
    assert(pgfault_num == 4);
    cprintf("write Virt Page a in fifo_check_swap\n");
    *(unsigned char*)0x1000 = 0x0a;
    assert(pgfault_num == 4);
    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char*)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char*)0x2000 = 0x0b;
    assert(pgfault_num == 4);
    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char*)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char*)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    cprintf("write Virt Page a in fifo_check_swap\n");
    *(unsigned char*)0x1000 = 0x0a;
    assert(pgfault_num == 6);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char*)0x2000 = 0x0b;
    assert(pgfault_num == 7);
    cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char*)0x3000 = 0x0c;
    assert(pgfault_num == 8);
    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char*)0x4000 = 0x0d;
    assert(pgfault_num == 9);
    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char*)0x5000 = 0x0e;
    assert(pgfault_num == 10);
    cprintf("write Virt Page a in fifo_check_swap\n");
    assert(*(unsigned char*)0x1000 == 0x0a);
    *(unsigned char*)0x1000 = 0x0a;
    assert(pgfault_num == 11);
    return 0;
}
```

其余函数无实际意义，不再看了。

这样练习2就做完了，可以`make qemu`得到如下输出，说明完成正确。

```c
check_vma_struct() succeeded!
page fault at 0x00000100: K/W [no page found].
check_pgfault() succeeded!
check_vmm() succeeded.
ide 0:      10000(sectors), 'QEMU HARDDISK'.
ide 1:     262144(sectors), 'QEMU HARDDISK'.
SWAP: manager = fifo swap manager
BEGIN check_swap: count 1, total 31963
setup Page Table for vaddr 0X1000, so alloc a page
setup Page Table vaddr 0~4MB OVER!
set up init env for check_swap begin!
page fault at 0x00001000: K/W [no page found].
page fault at 0x00002000: K/W [no page found].
page fault at 0x00003000: K/W [no page found].
page fault at 0x00004000: K/W [no page found].
set up init env for check_swap over!
write Virt Page c in fifo_check_swap
write Virt Page a in fifo_check_swap
write Virt Page d in fifo_check_swap
write Virt Page b in fifo_check_swap
write Virt Page e in fifo_check_swap
page fault at 0x00005000: K/W [no page found].
swap_out: i 0, store page in vaddr 0x1000 to disk swap entry 2
write Virt Page b in fifo_check_swap
write Virt Page a in fifo_check_swap
page fault at 0x00001000: K/W [no page found].
swap_out: i 0, store page in vaddr 0x2000 to disk swap entry 3
swap_in: load disk swap entry 2 with swap_page in vadr 0x1000
write Virt Page b in fifo_check_swap
page fault at 0x00002000: K/W [no page found].
swap_out: i 0, store page in vaddr 0x3000 to disk swap entry 4
swap_in: load disk swap entry 3 with swap_page in vadr 0x2000
write Virt Page c in fifo_check_swap
page fault at 0x00003000: K/W [no page found].
swap_out: i 0, store page in vaddr 0x4000 to disk swap entry 5
swap_in: load disk swap entry 4 with swap_page in vadr 0x3000
write Virt Page d in fifo_check_swap
page fault at 0x00004000: K/W [no page found].
swap_out: i 0, store page in vaddr 0x5000 to disk swap entry 6
swap_in: load disk swap entry 5 with swap_page in vadr 0x4000
write Virt Page e in fifo_check_swap
page fault at 0x00005000: K/W [no page found].
swap_out: i 0, store page in vaddr 0x1000 to disk swap entry 2
swap_in: load disk swap entry 6 with swap_page in vadr 0x5000
write Virt Page a in fifo_check_swap
page fault at 0x00001000: K/R [no page found].
swap_out: i 0, store page in vaddr 0x2000 to disk swap entry 3
swap_in: load disk swap entry 2 with swap_page in vadr 0x1000
count is 0, total is 7
check_swap() succeeded!
++ setup timer interrupts
100 ticks
```

#### `extended clock`页替换算法

如果要在`ucore`上实现`extended clock`页替换算法"请给你的设计方案，现有的`swap_manager`框架是否足以支持在`ucore`中实现此算法？如果是，请给你的设计方案。如果不是，请给出你的新的扩展和基此扩展的设计方案。并需要回答如下问题

- 需要被换出的页的特征是什么？
- 在`ucore`中如何判断具有这样特征的页？
- 何时进行换入和换出操作？

每个页面需要两个标志位，使用位和修改位。被换出的页的使用位为0，并且优先换出没有被修改的页。

记这两位标志位（使用，修改），则有四种情况。

- （0，0）表示最近未被引用也未被修改，首先选择此页淘汰

- （0，1）最近未被使用，但被修改，其次选择

- （1，0）最近使用而未修改，再次选择

- （1，1）最近使用且修改，最后选择

当内存页被访问后，把对应页表项的`PTE_A`置1；当内存页被修改后，把对应页表项的`PTE_D`置1。

换入换出操作何时进行与之前一直。换入发生在保存在磁盘中的内存需要被访问时，换出发生在物理内存页满并且被替换算法选中后。

### `swap.c`

简单看下这个文件里的一些函数。

#### `swap_init`

初始化换入换出机制

```c
int swap_init(void) {
    // 初始化swap分区，并计算swap分区可以存放的最大的物理页数目，存放在max_swap_offset里
    swapfs_init();
	// 检查一下是否满足要求
    if (!(1024 <= max_swap_offset && max_swap_offset < MAX_SWAP_OFFSET_LIMIT)) {
        panic("bad max_swap_offset %08x.\n", max_swap_offset);
    }
	// 初始化fifo
    sm = &swap_manager_fifo;
    int r = sm->init();
	// r=0 表示正常
    if (r == 0) {
        // 这个变量表示swap初始化完毕，换入换出机制已开启
        swap_init_ok = 1;
        cprintf("SWAP: manager = %s\n", sm->name);
        // 检查换入换出机制实现
        check_swap();
    }

    return r;
}
```

#### `check_swap`

关键函数，检查页面置换算法。

```c
static void check_swap(void) {
    // backup mem env
    // 先做一些关于连续内存管理的检查
    int ret, count = 0, total = 0, i;
    list_entry_t* le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page* p = le2page(le, page_link);
        assert(PageProperty(p));
        count++, total += p->property;
    }
    assert(total == nr_free_pages());
    cprintf("BEGIN check_swap: count %d, total %d\n", count, total);

    // now we set the phy pages env
    // 申请一个mm变量用于检查
    struct mm_struct* mm = mm_create();
    assert(mm != NULL);

    extern struct mm_struct* check_mm_struct;
    assert(check_mm_struct == NULL);

    check_mm_struct = mm;
	
    // 把内核的页表给mm用以检查
    pde_t* pgdir = mm->pgdir = boot_pgdir;
    assert(pgdir[0] == 0);
	
    // 创建vma，内存范围是4KB-24KB
    // the valid vaddr for check is between 0~CHECK_VALID_VADDR-1
	// #define CHECK_VALID_VIR_PAGE_NUM 5
	// #define BEING_CHECK_VALID_VADDR 0X1000
	// #define CHECK_VALID_VADDR (CHECK_VALID_VIR_PAGE_NUM + 1) * 0x1000
    struct vma_struct* vma = vma_create(BEING_CHECK_VALID_VADDR,
                                        CHECK_VALID_VADDR, VM_WRITE | VM_READ);
    assert(vma != NULL);
	// 加入mm的链表中
    insert_vma_struct(mm, vma);

    // setup the temp Page Table vaddr 0~4MB
    cprintf("setup Page Table for vaddr 0X1000, so alloc a page\n");
    pte_t* temp_ptep = NULL;
    // 创建一个临时的页表 传入的la是0x1000 因此这个页表映射的是0-4MB的物理地址
    temp_ptep = get_pte(mm->pgdir, BEING_CHECK_VALID_VADDR, 1);
    assert(temp_ptep != NULL);
    cprintf("setup Page Table vaddr 0~4MB OVER!\n");
	
    // check_rp 是Page数组
    // 分配4个物理页
    // the max number of valid physical page for check
	// #define CHECK_VALID_PHY_PAGE_NUM 4
    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        check_rp[i] = alloc_page();
        assert(check_rp[i] != NULL);
        assert(!PageProperty(check_rp[i]));
    }
    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    // assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;
    // 再释放4个物理页
    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        free_pages(check_rp[i], 1);
    }
    assert(nr_free == CHECK_VALID_PHY_PAGE_NUM);

    cprintf("set up init env for check_swap begin!\n");
    // setup initial vir_page<->phy_page environment for page relpacement
    // algorithm

    pgfault_num = 0;
	// 先到下面看这个函数
    // 这个函数执行完后，4个物理页全部被分配，4KB-20KB，此时nr_free为0
    check_content_set();
    assert(nr_free == 0);
    // 初始化记录换入换出顺序的数组
    for (i = 0; i < MAX_SEQ_NO; i++)
        swap_out_seq_no[i] = swap_in_seq_no[i] = -1;
	
    // 检测这四个物理页对应的pte
    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        check_ptep[i] = 0;
        check_ptep[i] = get_pte(pgdir, (i + 1) * 0x1000, 0);
        // cprintf("i %d, check_ptep addr %x, value %x\n", i, check_ptep[i],
        // *check_ptep[i]);
        assert(check_ptep[i] != NULL);
        assert(pte2page(*check_ptep[i]) == check_rp[i]);
        assert((*check_ptep[i] & PTE_P));
    }
    cprintf("set up init env for check_swap over!\n");
    
    
    // now access the virt pages to test  page relpacement algorithm
    // 然后调用fifo的check，具体前面看
    ret = check_content_access();
    assert(ret == 0);
	
    // 没有问题就释放了恢复环境
    // restore kernel mem env
    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        free_pages(check_rp[i], 1);
    }

    // free_page(pte2page(*temp_ptep));

    mm_destroy(mm);

    nr_free = nr_free_store;
    free_list = free_list_store;

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page* p = le2page(le, page_link);
        count--, total -= p->property;
    }
    cprintf("count is %d, total is %d\n", count, total);
    // assert(count == 0);

    cprintf("check_swap() succeeded!\n");
}
```

#### `check_content_set`

这函数很简单，就是访问一些内存区域去发生缺页异常然后统计数目

```c
static inline void check_content_set(void) {
    // 第一次访问4-8，异常数+1
    *(unsigned char*)0x1000 = 0x0a;
    assert(pgfault_num == 1);
    // 第 次访问4-8，不发生缺页
    *(unsigned char*)0x1010 = 0x0a;
    assert(pgfault_num == 1);
    // 下面同样
    *(unsigned char*)0x2000 = 0x0b;
    assert(pgfault_num == 2);
    *(unsigned char*)0x2010 = 0x0b;
    assert(pgfault_num == 2);
    *(unsigned char*)0x3000 = 0x0c;
    assert(pgfault_num == 3);
    *(unsigned char*)0x3010 = 0x0c;
    assert(pgfault_num == 3);
    *(unsigned char*)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    *(unsigned char*)0x4010 = 0x0d;
    assert(pgfault_num == 4);
}
```

其余函数都是调用了`fifo`的函数，不再看了

### 挑战1：实现识别`dirty bit`的` extended clock`页替换算法

在时钟置换算法中，淘汰一个页面时只考虑了页面是否被访问过，但在实际情况中，还应考虑被淘汰的页面是否被修改过。因为淘汰修改过的页面还需要写回硬盘，使得其置换代价大于未修改过的页面，所以优先淘汰没有修改的页，减少磁盘操作次数。改进的时钟置换算法除了考虑页面的访问情况，还需考虑页面的修改情况。即该算法不但希望淘汰的页面是最近未使用的页，而且还希望被淘汰的页是在主存驻留期间其页面内容未被修改过的。这需要为每一页的对应页表项内容中增加一位引用位和一位修改位。

每个页面需要两个标志位，使用位和修改位。被换出的页的使用位为0，并且优先换出没有被修改的页。

记这两位标志位（使用，修改），则有四种情况。

- （0，0）表示最近未被引用也未被修改，首先选择此页淘汰

- （0，1）最近未被使用，但被修改，其次选择

- （1，0）最近使用而未修改，再次选择

- （1，1）最近使用且修改，最后选择

当内存页被访问后，把对应页表项的`PTE_A`置1；当内存页被修改后，把对应页表项的`PTE_D`置1。

-----

用一个环形链表串起来所有的页面，再用一个指针代替时钟即可。

在选择换出的页面时，需要对整个链表循环四次

- 第一次寻找（0,0）
- 第二次寻找（0,1），并把访问过的页面的访问位置0
- 如果上一步没有找到，重复第一步操作
- 如果上一步没有找到，重复第二步操作

实现具体看代码吧，比较简单。