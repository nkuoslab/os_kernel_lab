<center><h3>实验三 虚拟内存管理</h3></center>
<div align='right'>学号： 1911463 姓名: 时浩铭 </div>
<div align='right'>学号： 1911477 姓名: 王耀群 </div>
<div align='right'>学号： 1911547 姓名: 李文婕 </div>

### 中断的处理过程

`trap`函数（定义在`trap.c`中）是对中断进行处理的过程，所有的中断在经过中断入口函数`__alltraps`预处理后 (定义在` trapasm.S`中) ，都会跳转到这里。在处理过程中，根据不同的中断类型，进行相应的处理。在相应的处理过程结束以后，`trap`将会返回，被中断的程序会继续运行。

整个中断处理流程大致如下：

1. 产生中断后，`CPU` 跳转到相应的中断处理入口 (`vectors`)，并在桟中压入相应的` error_code`（是否存在与异常号相关） 以及 `trap_no`，然后跳转到` alltraps` 函数入口。

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
// find_vma - find a vma  (vma->vm_start <= addr <= vma_vm_end)
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

页访问异常错误码有32位。位0为1表示对应物理页不存在；位1为1表示写异常（比如写了只读页)；位2为1表示访问权限异常（比如用户态程序访问内核空间的数据）

`CPU`在当前内核栈保存当前被打断的程序现场，即依次压入当前被打断程序使用的`EFLAGS`，`CS`，`EIP`，`errorCode`；由于页访问异常的中断号是`0xE`，`CPU`把异常中断号`0xE`对应的中断服务例程的地址（`vectors.S`中的标号`vector14`处）加载到`CS`和`EIP`寄存器中，开始执行中断服务例程。这时`ucore`开始处理异常中断，首先需要保存硬件没有保存的寄存器。在`vectors.S`中的标号`vector14`处先把中断号压入内核栈，然后再在`trapentry.S`中的标号`__alltraps`处把`DS`、`ES`和其他通用寄存器都压栈。自此，被打断的程序执行现场（`context`）被保存在内核栈中。接下来，在`trap.c`的`trap`函数开始了中断服务例程的处理流程，大致调用关系为：

`trap--> trap_dispatch-->pgfault_handler-->do_pgfault`

`ucore OS`会把这个值保存在`struct trapframe `中`tf_err`成员变量中。而中断服务例程会调用页访问异常处理函数`do_pgfault`进行具体处理。这里的页访问异常处理是实现按需分页、页换入换出机制的关键之处。

`ucore`中`do_pgfault`函数是完成页访问异常处理的主要函数，它根据从`CPU`的控制寄存器`CR2`中获取的页访问异常的物理地址以及根据`errorCode`的错误类型来查找此地址是否在某个`VMA`的地址范围内以及是否满足正确的读写权限，如果在此范围内并且权限也正确，这认为这是一次合法访问，但没有建立虚实对应关系。所以需要分配一个空闲的内存页，并修改页表完成虚地址到物理地址的映射，刷新`TLB`，然后调用`iret`中断，返回到产生页访问异常的指令处重新执行此指令。如果该虚地址不在某`VMA`范围内，则认为是一次非法访问。

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
    // 设置写pte用的权限
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
            if (swap_in(mm, addr, page)) {
                // 如果返回值不为0说明出了问题 failed
                cprintf("swap in failed.\n");
                goto failed;
            }
            // 建立映射关系，更新PTE，因为换出去以后PTE存的是关于交换的信息
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
        case IRQ_OFFSET + IRQ_TIMER:
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

### 页换入换出机制

