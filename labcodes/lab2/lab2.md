<center><h3>实验二 物理内存管理</h3></center>
<div align='right'>学号： 1911463 姓名: 时浩铭 </div>
<div align='right'>学号： 1911477 姓名: 王耀群 </div>
<div align='right'>学号： 1911547 姓名: 李文婕 </div>

## 写在前面

**`lab2`比起`lab1`更注重了代码的阅读，很多知识你只知道理论都是不够的，必须要结合着代码来看。**

**这次的报告我会尽量贴进去所有的代码，然后解释的细一些，看的时候也要努力去弄懂每一条代码。不用非得记住，理解后知道在哪找就行了。（这样如果再问到代码相关的话查报告也会快一些）**

**然后还有就是注意也不要光看报告，对应着文件去找一下这些函数、结构什么的在哪个文件，我也会标注一下的。（因为她上回问我来着）**

**阅读报告请一定带着问题阅读，因为我写的也可能不是很详细，有些问题我也会忽略。遇到问题可以大家讨论一下，这样记得也清除。**

**一定一定尽量搞懂每一行代码，尤其是看起来长的就奇怪的。（说的就是那条函数指针）（当然不要钻牛角尖，以理解大致意思为准，特别细节的肯定不用很清楚）**

---

## 前置工作&代码分析

### `bootasm.S`

在`lab 2`中，`bootloader`的工作有所增加，增加了对于物理内存资源的探测工作（即了解物理内存是如何分布的）。探测方法是通过`BIOS`中断调用来完成的，必须在实模式下进行。

BIOS 通过系统内存映射地址描述符（Address Range Descriptor）格式来表示系统物理内存布局，其具体表示如下：

```
Offset  Size    Description
00h    8字节   base address               #系统内存块基地址
08h    8字节   length in bytes            #系统内存大小
10h    4字节   type of address range      #内存类型
```

关于代码中的两种类型

```
ARM    01h    memory, available to OS
ARR    02h    reserved, not available (e.g. system ROM, memory-mapped device)
```

通过 BIOS 中断获取内存可调用参数为 e820h 的 INT 15h BIOS 中断。

参数如下：.

```
eax：e820h：INT 15的中断调用参数；
edx：534D4150h (即4个ASCII字符“SMAP”) ，这只是一个签名而已；
ebx：如果是第一次调用或内存区域扫描完毕，则为0。 如果不是，则存放上次调用之后的计数值；
ecx：保存地址范围描述符的内存大小,应该大于等于20字节；
es:di：指向保存地址范围描述符结构的缓冲区，BIOS把信息写入这个结构的起始地址。
```

返回值如下：

```
eflags的CF位：若INT 15中断执行成功，则不置位，否则置位；
eax：534D4150h ('SMAP') ；
es:di：指向保存地址范围描述符的缓冲区,此时缓冲区内的数据已由BIOS填写完毕
ebx：下一个地址范围描述符的计数地址
ecx：返回BIOS往ES:DI处写的地址范围描述符的字节大小
ah：失败时保存出错代码
```

调用中断后，BIOS 会填写好 ARD 的数据，并用`es:di`指向它，再从这里开始，每次读取 20 字节的 ARD，并保存在一个结构`e820map`中。

```c
// memlayout.h
// some constants for bios interrupt 15h AX = 0xE820
#define E820MAX             20      // number of entries in E820MAP
#define E820_ARM            1       // address range memory
#define E820_ARR            2       // address range reserved

struct e820map {
    int nr_map;						//内存段个数
    struct {
        uint64_t addr;              // 内存段开始
        uint64_t size;              // 内存段大小
        uint32_t type;              // 内存段类型
    } __attribute__((packed)) map[E820MAX];
};
```

看代码：

```assembly
probe_memory:
    movl $0, 0x8000				    # 对0x8000处的32位单元清零,即给位于0x8000处的
									# struct e820map的成员变量nr_map清零
    xorl %ebx, %ebx
    movw $0x8004, %di				# 设置BIOS返回的映射地址描述符的起始地址
start_probe:
    movl $0xE820, %eax				# 写参数
    movl $20, %ecx  			    # 设置地址范围描述符的大小为20字节，与e820map中map大小一致
    movl $SMAP, %edx
    int $0x15						# 调用0x15中断
    jnc cont						# 检测CF，没有置位说明中断执行成功
    movw $12345, 0x8000				# 探测有问题，结束探测
    jmp finish_probe
cont:
    addw $20, %di					# 设置下一个BIOS返回的映射地址描述符的起始地址
    incl 0x8000						# 递增struct e820map的成员变量nr_map
    cmpl $0, %ebx					# 如果INT0x15返回的ebx为零，表示探测结束，否则继续探测
    jnz start_probe
finish_probe:
```

正常执行完后，会在`0x8000`处保存结构`e820map`，后面由`ucore`来根据这个结构完成对物理内存的管理。

### `entry.S`

其次，`bootloader`不像`lab1`那样，直接调用`kern_init`函数，而是先调用位于`lab2/kern/init/entry.S`中的`kern_entry`函数。`kern_entry`函数的主要任务是为执行`kern_init`建立一个良好的 C 语言运行环境（设置堆栈），而且临时建立了一个段映射关系，为之后建立分页机制的过程做一个准备（细节在 3.5 小节有进一步阐述）。完成这些工作后，才调用`kern_init`函数。

_这里包括下面`pmm_init`的主要作用，所给的指导书内容与代码是不一致的（代码发生了改动而指导书并未做对应的改动）。这里`entry.S`不仅建立了内核栈而且设立了临时页表后又取消 0-4M 的对等映射，相当于以前在`pmm_init`中完成的一些功能，现在提前到这里去实现了。改动链接：https://github.com/chyyuu/os_kernel_lab/commit/07a95c0a5f30bebdf7ee459cfe996004b0773899#diff-819506afdc296bb942a41cc290d314ad92b16336cd228e92601dca0ab9b90b43_

```assembly
#define REALLOC(x) (x - KERNBASE)
# REALLOC是因为内核在构建时被设置在了高位(kernel.ld中设置了内核起始虚地址0xC0100000,使得虚地址整体增加了KERNBASE)
# 因此需要REALLOC来对内核全局变量进行重定位，在开启分页模式前保证程序访问的物理地址的正确性
# #define KERNBASE            0xC0000000 在memlayout.h中定义

kern_entry:									# 内核入口点
    # load pa of boot pgdir
    movl $REALLOC(__boot_pgdir), %eax		# 这里使用的是物理地址，因为页表映射还没开
    movl %eax, %cr3							# 将临时页表，存到CR3中。
    										# CR3中含有页目录表物理内存基地址，因此该寄存器也被称为页目录基址寄存器PDBR

    # enable paging							# 使能页表映射功能
    movl %cr0, %eax
    orl $(CR0_PE | CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP), %eax
    andl $~(CR0_TS | CR0_EM), %eax			# 取消的两位与协处理器相关，至于为什么先置位又取消，并不是很懂
    movl %eax, %cr0							# 此时CR0.PE(第0位)已经置1，保护模式开启;CR0.PG(第31位)也置1，页机制开启，页表地址为__boot_pgdir

    #此时内核仍然运行在0-4M的空间上，但是内核要运行的虚拟地址在高地址，因此更新eip

    # update eip
    # now, eip = 0x1.....
    leal next, %eax							# 把next的地址赋给eax，然后用jmp跳转
    # set eip = KERNBASE + 0x1.....
    jmp *%eax								# 因为eax里面存的是地址，所以要用*取值
```

下面看一下临时的页表`__boot_pgdir`。这是一个两级页表，`__boot_pgdir`是页目录表（一级页表），`__boot_pt1`是二级页表。

```
31                     22 21                       12 11                        0
+-------------------------------------------------------------------------------+
|        一级页号         |           二级页号          |           页内偏移        |
+-------------------------------------------------------------------------------+
```

每一个二级页表项大小为 4K，可以映射 4MB 的物理内存。

```assembly
# kernel builtin pgdir
# an initial page directory (Page Directory Table, PDT)
# These page directory table and page table can be reused!
.section .data.pgdir
.align PGSIZE
# 一级页表的起始地址必须是页对齐地址，低12位为0，因为它也存在一个页中
__boot_pgdir:
.globl __boot_pgdir
    # map va 0 ~ 4M to pa 0 ~ 4M (temporary)
    # 将页表的物理地址填入页目录表的第一项 PTE_P 有效位 PTE_U 用户 PTE_W 可写
    .long REALLOC(__boot_pt1) + (PTE_P | PTE_U | PTE_W)
    # space用于将指定范围大小内的空间全部设置为0(等价于P位为0，即不存在的、无效的页表项)
    # 填充页目录表直到其映射的物理地址为0xC0000000那项
    # 这里的计算想算出来这一段物理地址对应到页目录表上的区域/偏移，从而才能建立从KERNBASE开始的4M内存的映射
    # KERNBASE=0xC0000000, PGSHIFT=12, 右移12位再右移10位，从而得到一级页号（结合前面那个结构很好理解）
    # 再左移两位因为一个页表项32位，即4byte
    # 再减去当前地址减掉页目录表的地址，相当于减去页目录表第一项的地址
    .space (KERNBASE >> PGSHIFT >> 10 << 2) - (. - __boot_pgdir) # pad to PDE of KERNBASE
    # map va KERNBASE + (0 ~ 4M) to pa 0 ~ 4M
    # 第二个有效页表项，同样对应二级页表__boot_pt1, 从而实现0-4m 与 KERNBASE+0-4m都映射到物理内存0-4m
    .long REALLOC(__boot_pt1) + (PTE_P | PTE_U | PTE_W)
    .space PGSIZE - (. - __boot_pgdir) # pad to PGSIZE

.set i, 0
__boot_pt1:
# 一个1024个32位long数据的数组，每一项映射一个物理地址
.rept 1024
    .long i * PGSIZE + (PTE_P | PTE_W)
    .set i, i + 1
.endr
```

[![cKjFBR.png](https://z3.ax1x.com/2021/04/04/cKjFBR.png)](https://imgtu.com/i/cKjFBR)

-   为什么要这样映射？

    前面也说了，内核运行在`0-4M`的低地址，为了保证分页机制开启后内核能正常运行，因此要添加`Virtual Address = Linear Address = Phisical Address(0-4M)`的映射关系。

之后移动`eip`到高虚拟地址，然后就取消了上面的映射，此时就只有`Address = Linear Address = 0xC0000000 + Phisical Address(0-4M)`的映射关系

```assembly
next:
    # unmap va 0 ~ 4M, it's temporary mapping
    xorl %eax, %eax							# 清空__boot_pgdir的第一个页目录项
    movl %eax, __boot_pgdir

    # set ebp, esp							# 建立一个内核栈
    movl $0x0, %ebp
    # the kernel stack region is from bootstack -- bootstacktop,
    # the kernel stack size is KSTACKSIZE (8KB)defined in memlayout.h
    movl $bootstacktop, %esp
    # now kernel stack is ready , call the first C function
    call kern_init							# 调用kern_init

# should never get here
spin:
    jmp spin
```

-   为什么内核运行在 0-4M 的地址空间？：

    在`tools/kernel.ld`中定义了内核的起始地址

    ```
    /* Load the kernel at this address: "." means the current address */
        . = 0xC0100000;
    ```

    这是一个虚拟地址，但由于内核加载时，页表映射还没有开，虚拟地址=物理地址，但是`bootloader`把`ucore`放在了起始物理地址为`0x100000`的物理内存空间，因此在跳转时，地址与`0xFFFFFF`做与手动做一个映射。

---

### `pmm.c & pmm.h`

#### `pmm_init`

进入`kern_init`后便会执行`pmm_init()`完成物理内存的管理。它完成的主要工作有

_前面也提到了，一些工作移到了`entry.S`中，即 4 和 8_

1. 初始化物理内存页管理器框架`pmm_manager`；
2. 建立空闲的`page`链表，这样就可以分配以页（4KB）为单位的空闲内存了；
3.
4. 检查物理内存页分配算法；
5. 为确保切换到分页机制后，代码能够正常执行，先建立一个临时二级页表；
6. 建立一一映射关系的二级页表；
7. 使能分页机制；
8. 重新设置全局段描述符表；
9. 取消临时二级页表；
10. 检查页表建立是否正确；
11. 通过自映射机制完成页表的打印输出（这部分是扩展知识）

```c
//pmm.c

//pmm_init - setup a pmm to manage physical memory, build PDT&PT to setup paging mechanism
//         - check the correctness of pmm & paging mechanism, print PDT&PT
void
pmm_init(void) {

    // We've already enabled paging
    // 此时已经开启了页机制，由于boot_pgdir是内核页表地址的虚拟地址。通过PADDR宏转化为boot_cr3物理地址，供后续使用
    boot_cr3 = PADDR(boot_pgdir);

    //We need to alloc/free the physical memory (granularity is 4KB or other size).
    //So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    //First we should init a physical memory manager(pmm) based on the framework.
    //Then pmm can alloc/free the physical memory.
    //Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.

    // 初始化物理内存管理器
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list

    // 探测物理内存空间，初始化可用的物理内存
    page_init();

    //use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    check_pgdir();

    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

    // recursively insert boot_pgdir in itself
    // to form a virtual page table at virtual address VPT
    // 将当前内核页表的物理地址设置进对应的页目录项中(内核页表的自映射)
    boot_pgdir[PDX(VPT)] = PADDR(boot_pgdir) | PTE_P | PTE_W;

    // map all physical memory to linear memory with base linear addr KERNBASE
    // linear_addr KERNBASE ~ KERNBASE + KMEMSIZE = phy_addr 0 ~ KMEMSIZE
    // 将内核所占用的物理内存，进行页表<->物理页的映射
    // 令处于高位虚拟内存空间的内核，正确的映射到低位的物理内存空间
    // (映射关系(虚实映射): 内核起始虚拟地址(KERNBASE)~内核截止虚拟地址(KERNBASE+KMEMSIZE) =  内核起始物理地址(0)~内核截止物理地址(KMEMSIZE))
    boot_map_segment(boot_pgdir, KERNBASE, KMEMSIZE, 0, PTE_W);

    // Since we are using bootloader's GDT,
    // we should reload gdt (second time, the last time) to get user segments and the TSS
    // map virtual_addr 0 ~ 4G = linear_addr 0 ~ 4G
    // then set kernel stack (ss:esp) in TSS, setup TSS in gdt, load TSS
    // 重新设置GDT
    gdt_init();

    //now the basic virtual memory map(see memalyout.h) is established.
    //check the correctness of the basic virtual memory map.
    check_boot_pgdir();

    print_pgdir();
}
```

下面解析上面这个代码：

##### `boot_cr3 = PADDR(boot_pgdir)`

```c
//pmm.c

// virtual address of boot-time page directory
extern pde_t __boot_pgdir;
pde_t *boot_pgdir = &__boot_pgdir;
// physical address of boot-time page directory
uintptr_t boot_cr3;


//pmm.h

/* *
 * PADDR - takes a kernel virtual address (an address that points above KERNBASE),
 * where the machine's maximum 256MB of physical memory is mapped and returns the
 * corresponding physical address.  It panics if you pass it a non-kernel virtual address.
 * */
//把内核虚拟地址转换为物理地址
#define PADDR(kva) ({                                                   \
            uintptr_t __m_kva = (uintptr_t)(kva);                       \
            if (__m_kva < KERNBASE) {                                   \
                panic("PADDR called with invalid kva %08lx", __m_kva);  \
            }                                                           \
            __m_kva - KERNBASE;                                         \
        })
```

##### `init_pmm_manager()`

`pmm_init`在得到了内核页目录表的物理地址(`boot_cr3`)后，便通过`init_pmm_manager`函数初始化了物理内存管理器。这是一个用于表达物理内存管理行为的函数指针集合，内核启动时会对这一函数指针集合进行赋值。

可以理解为这是 C 语言的面向对象的结构，只用修改对应函数指针便可以不改变其他逻辑完成修改。

> 先介绍下函数指针
>
> 函数本身也是有地址的，因此也就可以有一个指针指向这个函数，如果知道函数的具体类型，就可以通过这个地址来调用这个函数。
>
> 返回值类型 (\*函数指针名)（参数 1，参数 2，...）
>
> `int (*fun_ptr)(int,int)`
>
> `void (*funP)(int)`
>
> 最面前的是函数的返回值类型，括号中是名字，最后面括号是参数表
>
> 调用时，直接`函数指针名()`即可。
>
> ---
>
> 再解释下 lab1 中的那个函数指针
>
> `((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))()`
>
> -   为什么这个这么长？
>
>     -   因为它本身只是一个地址，首先强制类型转换成函数指针才能调用
>
> -   具体结构解析
>     -   先看最右边（）是函数调用的括号，无参数。
>     -   左边整体括号表明这是一个整体，与优先级相关。
>     -   `(void (*)(void))`外侧括号是强制类型转换的括号，类比`(int)(3.0)`，同理`(ELFHDR->e_entry & 0xFFFFFF)`的括号。
>     -   再看类型`void (*)(void)`，类比`int`，这是这个函数指针的类型，具体是没有参数也没有返回值的函数指针。

首先看`pmm_manager`的结构。

```c
//pmm.h

// pmm_manager is a physical memory management class. A special pmm manager - XXX_pmm_manager
// only needs to implement the methods in pmm_manager class, then XXX_pmm_manager can be used
// by ucore to manage the total physical memory space.
struct pmm_manager {
    const char *name;                                 // XXX_pmm_manager's name
    void (*init)(void);                               // initialize internal description&management data structure (free block list, number of free block) of XXX_pmm_manager
    void (*init_memmap)(struct Page *base, size_t n); // setup description&management data structcure according to the initial free physical memory space
    												  // 设置可管理的内存,初始化可分配的物理内存空间，初始化管理空闲内存页的数据结构
    struct Page *(*alloc_pages)(size_t n);            // allocate >=n pages, depend on the allocation algorithm
    												  // 分配>=N个连续物理页,返回分配块首地址指针
    void (*free_pages)(struct Page *base, size_t n);  // free >=n pages with "base" addr of Page descriptor structures(memlayout.h)
    												  // 释放包括自Base基址在内的，起始的>=N个连续物理内存页
    size_t (*nr_free_pages)(void);                    // return the number of free pages
    void (*check)(void);                              // check the correctness of XXX_pmm_manager
};
```

再来看`init_pmm_manager`函数：

```c
//pmm.c

// physical memory management
const struct pmm_manager *pmm_manager;

//init_pmm_manager - initialize a pmm_manager instance
static void
init_pmm_manager(void) {
    //使用了默认的pmm_manager，采用first-fit分配算法
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}
```

具体`default_pmm_manager`后面练习 1 再看。

##### `page_init()`

通过`pmm_manager`获得可用物理内存范围后，`ucore`使用`Page`来管理物理页（按 4KB 对齐，且大小为 4KB 的物理内存单元）。由于一个物理页需要占用一个`Page`结构的空间，`Page`结构在设计时须尽可能小，以减少对内存的占用。

首先看`Page`结构：

```c
//memlayout.h
/* *
 * struct Page - Page descriptor structures. Each Page describes one
 * physical page. In kern/mm/pmm.h, you can find lots of useful functions
 * that convert Page to other data types, such as phyical address.
 * */
struct Page {
    // 当前物理页被虚拟页面引用的次数(共享内存时，影响物理页面的回收)
    int ref;                        // page frame's reference counter
    // 标志位集合(目前只用到了第0和第1个bit位) bit 0 PG_reserve 保留位（可否用于物理内存分配: 0未保留，1被保留）;bit1 PG_property 用处也是不同算法意义不同(first fit 空闲块头部置1，否则为0)
    uint32_t flags;                 // array of flags that describe the status of the page frame
    // 在不同分配算法中意义不同(first fit算法中表示当前空闲块中总共所包含的空闲页个数 ，只有位于空闲块头部的Page结构才拥有该属性，否则为0)
    unsigned int property;          // the num of free block, used in first fit pm manager
    // 空闲链表free_area_t的链表节点引用，是一个双向连接Page的链表指针
    // 用到这个变量的Page也是空闲块的头一页
    list_entry_t page_link;         // free list link
};
```

关于这个`list_entry_t`：

```c
//list.h

struct list_entry {
    struct list_entry *prev, *next;
};

typedef struct list_entry list_entry_t;
```

关于`free_area_t`：

```c
/* free_area_t - maintains a doubly linked list to record free (unused) pages */
typedef struct {
    list_entry_t free_list;         // the list header
    unsigned int nr_free;           // # of free pages in this free list
} free_area_t;
```

再看`page_init()`函数，这个函数主要就是利用之前通过`BIOS 0x15`中断探测出的物理内存布局来初始化物理内存管理。

```c
//pmm.c

/* page_init - initialize the physical memory management */
static void
page_init(void) {
    // 通过e820map结构体指针，关联上在bootasm.S中通过e820中断探测出的硬件内存布局
    // 之所以加上KERNBASE是因为指针寻址时使用的是线性虚拟地址。按照最终的虚实地址关系(0x8000 + KERNBASE)虚拟地址 = 0x8000 物理地址
    struct e820map *memmap = (struct e820map *)(0x8000 + KERNBASE);
    uint64_t maxpa = 0;

    cprintf("e820map:\n");
    int i;
    // 遍历memmap中的每一项(共nr_map项)，为了找到最大的物理内存地址maxpa
    for (i = 0; i < memmap->nr_map; i ++) {
        // 获取到每一个布局entry的起始地址、截止地址
        uint64_t begin = memmap->map[i].addr, end = begin + memmap->map[i].size;
        //这里占位符中 ll表示64位数据，因为之前ARD中存的就是64位的
        cprintf("  memory: %08llx, [%08llx, %08llx], type = %d.\n",
                memmap->map[i].size, begin, end - 1, memmap->map[i].type);
        // 如果是E820_ARM类型的内存空间块
        if (memmap->map[i].type == E820_ARM) {
            if (maxpa < end && begin < KMEMSIZE) {
                // 最大可用的物理内存地址 = 当前项的end截止地址
                maxpa = end;
            }
        }
    }

    // 迭代每一项完毕后，发现maxpa超过了定义约束的最大可用物理内存空间
    if (maxpa > KMEMSIZE) {
        // maxpa = 定义约束的最大可用物理内存空间
        maxpa = KMEMSIZE;
    }

    // 全局指针变量end记录的是bootloader加载ucore 的结束地址
    // 其上的高位内存空间并没有被使用,因此以end为起点，存放用于管理物理内存页面的数据结构
    extern char end[];

    // 需要管理的物理页数 = 最大物理地址/物理页大小（x86的起始物理内存地址为0）
    npage = maxpa / PGSIZE;
    // pages指针指向->可用于分配的，物理内存页面Page数组起始地址（虚地址）
    // 位于内核空间之上(通过ROUNDUP PGSIZE取整，保证其位于一个新的物理页中)
    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE);

    for (i = 0; i < npage; i ++) {
        // 遍历每一个可用的物理页，默认标记为被保留无法使用
        // SetPageReserved是一个宏，就把PG_reserve置位，在memlayout.h中定义
        SetPageReserved(pages + i);
    }

    // 为了简化，从地址0到地址pages+ sizeof(struct Page) * npage)结束的物理内存空间设定为已占用物理内存空间（起始0~640KB的空间是空闲的）
    //地址pages+ sizeof(struct Page) * npage)以上的空间为空闲物理内存空间，这时的空闲空间起始地址为freemem，结束地址为0x38000000（KMEMSIZE）
    //PADDR 把内核虚拟地址转换为物理地址
    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * npage);

    // freemem之上的高位物理空间都是可以用于分配的free空闲内存
    for (i = 0; i < memmap->nr_map; i ++) {
        // 遍历探测出的内存布局memmap
        uint64_t begin = memmap->map[i].addr, end = begin + memmap->map[i].size;
        if (memmap->map[i].type == E820_ARM) {
            if (begin < freemem) {
                // 限制空闲地址的最小值
                begin = freemem;
            }
            if (end > KMEMSIZE) {
                // 限制空闲地址的最大值
                end = KMEMSIZE;
            }
            if (begin < end) {
                // begin起始地址以PGSIZE为单位，向高位取整
                begin = ROUNDUP(begin, PGSIZE);
                // end截止地址以PGSIZE为单位，向低位取整
                end = ROUNDDOWN(end, PGSIZE);
                if (begin < end) {
                    // 进行空闲内存块的映射，将其纳入物理内存管理器中管理，用于后续的物理内存分配
                    // 这里的begin、end都是探测出来的物理地址
                    // init_memmap函数把空闲物理页对应的Page结构中的flags和引用计数ref清零，并加到free_area.free_list指向的双向列表中，为将来的空闲页管理做好初始化准备工作。
                    // 具体实现是通过调用pmm_manager的init_memmap()完成的
                    init_memmap(pa2page(begin), (end - begin) / PGSIZE);
                }
            }
        }
    }
}
```

最后一行中`pa2page`定义在`pmm.h`中，就是根据物理地址，返回对应`Page`的指针，索引就是物理地址的前 20 位。

```c
static inline struct Page *
pa2page(uintptr_t pa) {
    if (PPN(pa) >= npage) {
        panic("pa2page called with invalid pa");
    }
    return &pages[PPN(pa)];
}

// mmu.h

 A linear address 'la' has a three-part structure as follows:

 +--------10------+-------10-------+---------12----------+
 | Page Directory |   Page Table   | Offset within Page  |
 |      Index     |     Index      |                     |
 +----------------+----------------+---------------------+
  \--- PDX(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
  \----------- PPN(la) -----------/

 The PDX, PTX, PGOFF, and PPN macros decompose linear addresses as shown.
 To construct a linear address la from PDX(la), PTX(la), and PGOFF(la),
 use PGADDR(PDX(la), PTX(la), PGOFF(la)).
```

注意`page_init`代码中遍历`e820map`两次，第一次是为了找到最大的的物理内存地址`maxpa`，然后根据它来计算所需的`Page`数目从而分配空间，空间是从内核结束的地址开始。结束的地址到`KMEMSiZE`都是空闲空间。第二次是调用`init_memmap`对这些空闲空间初始化。

[<img src="https://z3.ax1x.com/2021/04/07/cGWNwV.png" alt="cGWNwV.png" style="zoom: 150%;" />](https://imgtu.com/i/cGWNwV)

---

后面的过程需要练习 123 的代码，所以先做练习，之后再继续分析。

## 练习 1：**实现`first-fit`连续物理内存分配算法**

### 准备工作

下面进入练习 1，实现`first-fit`连续物理内存分配算法。

首先分析`default_pmm_manager`，因为实现`first-fit`就是要重写其中的几个函数：`default_init`，`default_init_memmap`，`default_alloc_pages`，`default_free_pages`。

`first-fit`需要维护一个双向链表，其中空闲块（以页为最小单位的连续地址空间）按照地址由小到大连续排列。当受到需要内存块的请求时，会遍历链表找到第一个满足大小的块，满足就分配出去，多余的继续串在链表中。释放分区时，需要考虑分区的合并。

这种分配算法的优点：1、简单；2、在高地址空间有大块的空闲分区；缺点：1、外部碎片；2、分配大块时较慢

再看一下`free_area_t`和`list_entry`结构。

关于这个`list_entry_t`：

```c
//list.h

struct list_entry {
    struct list_entry *prev, *next;
};

typedef struct list_entry list_entry_t;
```

关于`free_area_t`：

```c
/* free_area_t - maintains a doubly linked list to record free (unused) pages */
typedef struct {
    list_entry_t free_list;         // the list header
    unsigned int nr_free;           // # of free pages in this free list
} free_area_t;
```

再回顾一下`Page`结构，有一个`list_entry_t page_link`可以连接到`free_area_t`中。

注意`ucore`中双向链表没有`data`，而是在具体的数据结构中包含链表节点。

先看一下`list.h`熟悉一些函数的用法。

```c
void list_init(list_entry_t *elm)    						     // 链表初始化
void list_add(list_entry_t *listelm, list_entry_t *elm)    		 // 在listelm后面添加elm
void list_add_after(list_entry_t *listelm, list_entry_t *elm)    // 在listelm后面添加elm
void list_add_before(list_entry_t *listelm, list_entry_t *elm)   // 在listelm前面添加elm
void list_del(list_entry_t *listelm)							 // 删除listelm
void list_del(list_entry_t *listelm)							 // 删除listelm并初始化链表
bool list_empty(list_entry_t *list) 							 // 判断链表是否空
list_entry_t *list_next(list_entry_t *listelm)					 // 下一个元素
list_entry_t *list_prev(list_entry_t *listelm)					 // 上一个元素
```

还有一个问题就是，由于是数据里保存链表指针，但我们只能获取链表指针，怎么获取到数据。`ucore`提供宏`le2page`，传入链表指针`le`与结构体变量名`member`即可得到对应的`page`指针。

```c
// memlayout.h
// convert list entry to page
#define le2page(le, member)                 \
    to_struct((le), struct Page, member)

//defs.h
/* *
 * to_struct - get the struct from a ptr
 * @ptr:    a struct pointer of member
 * @type:   the type of the struct this is embedded in
 * @member: the name of the member within the struct
 * */
#define to_struct(ptr, type, member)                               \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* Return the offset of 'member' relative to the beginning of a struct type */
#define offsetof(type, member)                                      \
    ((size_t)(&((type *)0)->member))
```

下面解释一下这几行代码：

首先，结构体就是一些不同类型数据连续排列到内存中，如果知道结构体的定义和其中某一项数据的开始地址，就可以通过减法来计算出结构体的开始地址。

> 题外话：
>
> 联系 Lab1 中的 ELFHDR 等文件头，所有文件的格式也是这样，一些数据连续排列构成文件。但是为了解析出这些数据，就需要在保存一下这些数据的排列方式，因此就需要有文件头。文件头就是文件的真正数据前的一些数据，保存了后面的数据的排列方式。因此读取文件需要先读文件头，再根据文件头来读取后面的数据。

其次，看代码。`offsetof(type, member)`接受一个结构体类型`type`和结构体中一个变量`member`，返回`member`变量到结构体头部的地址。

具体原理：首先构造一个`type*`，其指向的地址是 0，然后获取得到其`member`变量`((type *)0)->member`，之后通过`&`取其地址，再减去结构体起始地址 0，就得到了地址差。再转换成`size_t`（即`unsigned int`）类型即可。

`to_struct(ptr, type, member)`接受链表指针`ptr`，从指针指向的地址向前移动，移动距离即为结构体中链表指针与结构体头的差，用`offsetof`得到。再把其类型转换为对应的结构体类型即可。

[![cQl7lT.png](https://z3.ax1x.com/2021/04/05/cQl7lT.png)](https://imgtu.com/i/cQl7lT)

---

准备工作就做完了，下面开始具体修改函数代码。

### `default_init`

```c
static void
default_init(void) {
    list_init(&free_list);
    nr_free = 0;
}
```

不用修改，直接复用。初始化链表并把计数清零。

### `default_init_memmap`

回顾一下这个函数的调用过程。（其实也没啥回顾的）

```
kern_init ==> pmm_init ==> page_init ==> init_memmap ==> pmm_manager->init_memmap
```

这个函数根据`page_init`传来的参数（连续内存空间的起始页和页的个数）来建立一个连续内存空闲块的双向链表。

> 这一段翻译自注释，可能是我理解有误，反正这个过程是不完全正确的
>
> 首先你需要初始化每一个页，步骤如下：
>
> -   把`Page`中的`eflags`的`PG_property`位置位。
>
>     `PG_property`如果是 1，那么这个`Page`是一个连续空闲块的第一个页，可以用于分配；如果为 0，并且是第一个块，说明被占用，不能分配，否则不是第一个块。
>
>     另一个标志位`PG_reserved`已经在`page_init`中被置位了。
>
> -   设置`Page`的`property`。在`first-fit`中，第一页为页的个数，余下页为 0。
>
> -   `Page`的`ref`置 0，因为目前是空闲态，没有被引用。

然后把`Page`的`page_link`连接到`free_list`中，再更新`free_area`的`nr_free`。

先看原来的代码。

```c
static void default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));					// 确认是否为保留页
        p->flags = p->property = 0;					// 标志位和property置0
        set_page_ref(p, 0);							// 清除ref
    }
    base->property = n;								// 设置第一个页的property
    SetPageProperty(base);							// 给第一个页的PG_property置位
    nr_free += n;									// 数目增加
    list_add(&free_list, &(base->page_link));		// 连入链表
}
```

其中几个函数：

```c
//memlayout.h
#define PageReserved(page)          test_bit(PG_reserved, &((page)->flags))
#define SetPageProperty(page)       set_bit(PG_property, &((page)->flags))

//pmm.h
static inline void set_page_ref(struct Page *page, int val) {
    page->ref = val;
}
```

`free_list`中连接的是空闲块的头部，而这个函数调用的时候是初始化一个空闲块并把它连入`free_list`，所以肯定只有第一个页的`PG_property`和`property`有值。（所以前面注释肯定是不对的）

还有就是注意这个函数是在前面`page_init`中根据`e820map`循环调用的，并且`free_list`中是按地址由低到高排序，所以修改一下`list_add`为`list_add_before`，使每次新连入的空闲块在后面（即高地址）。

### `default_alloc_pages`

找到第一个满足大小的空闲块并分配出去，接受页数`n`为参数，返回值是`Page *`。

因此需要遍历`free_list`，找到第一个大小不小于`n`的空闲块，将其前`n`个块分配出去，再把多余的块重新连接到链表之中。

先看原本代码。

```c
static struct Page * default_alloc_pages(size_t n) {
    assert(n > 0);
    // nr_free 是全局变量，就是free_list的nr_free，如果总共的Page都不够肯定没法分配
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    // 遍历free_list来找到第一个满足条件(property>n)的
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    if (page != NULL) {
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            list_add(&free_list, &(p->page_link));
    }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}
```

这个代码应该不难读，但其中有几点细微的错误，主要问题在于如果拿出的空闲块大小大于`n`的情况上。由于`free_list`中块是按照地址顺序排列的，因此多余的页仍处于原本的块在链表中的对应位置，而所给代码中将其连接在了`free_list`的链表头处，这显然是错误的。而且，一个空闲块的第一个页的`PG_property`是应该置位的，而所给代码并没有置位。因此需要进行修改。

由于分裂后的块仍处于原本位置，所以我们先添加再移除原本块。更正如下。

```c
if (page != NULL) {
    if (page->property > n) {
        struct Page *p = page + n;
        SetPageProperty(p);
        p->property = page->property - n;
        list_add(&(page->page_link), &(p->page_link));
    }
    list_del(&(page->page_link));
    nr_free -= n;
    ClearPageProperty(page);
}
```

### `default_free_pages`

释放`base`开始的连续`n`个物理页。这个工作比分配复杂，主要在于需要考虑合并的问题。

首先遍历`free_list`找到合适的位置，然后再插入进去，再修改一些标志位和字段，最后再考虑合并问题。

看原来代码。

```c
static void default_free_pages(struct Page* base, size_t n) {
    assert(n > 0);
    struct Page* p = base;
    // 清除标志位和ref
    for (; p != base + n; p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    // 再次连入链表设置第一个页的相关属性
    base->property = n;
    SetPageProperty(base);
    list_entry_t* le = list_next(&free_list);
    while (le != &free_list) {
        p = le2page(le, page_link);
        le = list_next(le);
        // 如果base开始的空闲块的下一个页与p相等，即他们连上了，就进行合并
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        // 如果p开始的空闲块的下一个页与base相等，也进行合并
        } else if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            base = p;
            list_del(&(p->page_link));
        }
    }
    nr_free += n;
    list_add(&free_list, &(base->page_link));
}
```

这个代码也有点问题，主要在于合并和最后的代码中。合并的第一种情况中并没有清空`p->property`，合并的第二种情况中并没有清空`base->property`，只清空了`base->eflags`中的`PG_property`。在最终的代码中，再一次将`base`连接到了链表头部而没有考虑地址的排列问题。

更正如下:

```c
static void default_free_pages(struct Page* base, size_t n) {
    assert(n > 0);
    struct Page* p = base;
    // 清除标志位和ref
    for (; p != base + n; p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0 );
    }
    // 再次连入链表设置第一个页的相关属性
    base->property = n;
    SetPageProperty(base);
    list_entry_t* le = list_next(&free_list);
    while (le != &free_list) {
        p = le2page(le, page_link);
        le = list_next(le);
        // 如果base开始的空闲块的下一个页与p相等，即他们连上了，就进行合并
        if (base + base->property == p) {
            base->property += p->property;
            p->property = 0;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        // 如果p开始的空闲块的下一个页与base相等，也进行合并
        } else if (p + p->property == base) {
            p->property += base->property;
            base->property = 0;
            ClearPageProperty(base);
            base = p;
            list_del(&(p->page_link));
        }
    }
    nr_free += n;
    le = list_next(&free_list);
    // 找到合适的位置
    while (le != &free_list){
        p = le2page(le, page_link);
        if (base + base->property < p){
            break;
        }
        le = list_next(le);
    }
    // 注意这里要用list_add_before，因为按顺序要添加到前面
    list_add_before(le, &(base->page_link));
}
```

### 改进空间

上面代码中，链表的查找和插入都需要遍历整个链表，比较耗时。改进可以采用树型结构，或者改进一下空闲块的结构，使得第一页和最后一页均有标记。

---

至此练习 1 就完成了，`default_pmm_manager`函数还有两个函数，一个返回`nr_free`，一个做检查便不再解释了。

此时可以`make qemu`执行`pmm_init`中的`check_alloc_page`去调用`pmm_check`检验我们的`first_fit`算法了，这也是`pmm_init`的工作之一。

结果如下：

[![cQR8Qf.png](https://z3.ax1x.com/2021/04/05/cQR8Qf.png)](https://imgtu.com/i/cQR8Qf)

最上方首先打印出由`BIOS 0x15`中断探测物理内存得到的`e820map`，然后便是`check_alloc_page() succeed!`，表明`first_fit`实现无错误。

---

## 练习 2：实现寻找虚拟地址对应的页表项

通过设置页表和对应的页表项，可建立虚拟内存地址和物理内存地址的对应关系。其中的` get_pte` 函数是设置页表项环节中的一个重要步骤。此函数找到一个虚地址对应的二级页表项的内核虚地址，如果此二级页表项不存在，则分配一个包含此项的二级页表。本练习需要补全`get_pte`函数` in kern/mm/pmm.c` ，实现其功能。

---

[<img src="https://z3.ax1x.com/2021/04/07/cGWWFO.png" alt="cGWWFO.png" style="zoom:150%;" />](https://imgtu.com/i/cGWWFO)

在保护模式中，x86 体系结构将内存地址分成三种：逻辑地址（也称虚地址）、线性地址和物理地址。逻辑地址即是程序指令中使用的地址，物理地址是实际访问内存的地址。逻辑地址通过段式管理的地址映射可以得到线性地址，线性地址通过页式管理的地址映射得到物理地址。

在`ucore`中段式管理只起到了一个过渡作用，它将逻辑地址不加转换直接映射成线性地址。

---

-   `PTE`：`Page Table Entry` 页表项，每一个页表项对应一个物理页，每一个二级页表有 1024 个页表项。

-   `PDE`：`Page Directory Entry`页目录项，每一个页目录项对应一个二级页表，每一个一级页表（页目录表）有 1024 个页目录项。

-   `pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)`

    其中三个类型`pte_t`、`pde_t`、`uintptr_t`，都为`unsigned int`。

    -   `pde_t`全称为 `page directory entry`，也就是一级页表的表项（注意：`pgdir`实际不是表项，而是一级页表本身。实际上应该新定义一个类型`pgd_t`来表示一级页表本身）
    -   `pte_t`全称为 `page table entry`，表示二级页表的表项。
    -   `uintptr_t`表示为线性地址，由于段式管理只做直接映射，所以它也是逻辑地址。

    如果在查找二级页表项时，发现对应的二级页表不存在，则需要根据`create`参数的值来处理是否创建新的二级页表。如果`create`参数为 0，则`get_pte`返回`NULL`；如果`create`参数不为 0，则`get_pte`需要申请一个新的物理页，再在一级页表中添加页目录项指向表示二级页表的新物理页。注意，新申请的页必须全部设定为零，因为这个页所代表的虚拟地址都没有被映射。

    当建立从一级页表到二级页表的映射时，需要注意设置控制位。这里应该设置同时设置上`PTE_U`、`PTE_W`和`PTE_P`。如果原来就有二级页表，或者新建立了页表，则只需返回对应项的地址即可。

    只有当一级二级页表的项都设置了用户写权限后，用户才能对对应的物理地址进行读写。所以我们可以在一级页表先给用户写权限，再在二级页表上面根据需要限制用户的权限，对物理页进行保护。

了解了功能和一些细节之后，开始写代码。

其中用到的一些宏/函数：

```c
//pmm.h
/* *
 * KADDR - takes a physical address and returns the corresponding kernel virtual
 * address. It panics if you pass an invalid physical address.
 * */
// 把物理地址转换成内核虚拟地址
#define KADDR(pa) ({                                                    \
            uintptr_t __m_pa = (pa);                                    \
            size_t __m_ppn = PPN(__m_pa);                               \
            if (__m_ppn >= npage) {                                     \
                panic("KADDR called with invalid pa %08lx", __m_pa);    \
            }                                                           \
            (void *) (__m_pa + KERNBASE);                               \
        })

//mmu.h
// page directory index
// 根据虚拟地址返回对应的页目录表的下标，实际上就是la的高10位
#define PDX(la) ((((uintptr_t)(la)) >> PDXSHIFT) & 0x3FF)

//pmm.h
// 设置Page的ref的值
static inline void
set_page_ref(struct Page *page, int val) {
    page->ref = val;
}

// 返回Page的物理地址
static inline uintptr_t
page2pa(struct Page *page) {
    return page2ppn(page) << PGSHIFT;
}
```

```c
pte_t* get_pte(pde_t* pgdir, uintptr_t la, bool create) {
    // 获取la对应的页表项
    pde_t* pdep = &pgdir[PDX(la)];
    // 通过检查页目录项的存在位判断对应的二级页表是否存在
    if (!(*pdep & 0x1)) {
        if (create) {
            // 不存在且create=1,则创建一个二级页表
            struct Page* page = alloc_page();
            // 给这个页设置引用位
            set_page_ref(page, 1);
            // 得到这个页的物理地址
            uintptr_t pa = page2pa(page);
            // 清空这一页表，注意memset使用的地址是内核虚拟地址
            memset(KADDR(pa), 0, PGSIZE);
            // 设置页目录表项的内容
            // (页表起始物理地址 & ~0x0FFF) | PTE_U | PTE_W | PTE_P
            // 为什么要去除后12位，因为页目录项指向的是一个二级页表的起始地址，具体看下面的结构
            *pdep = (pa & ~0x0FFF) | PTE_U | PTE_W | PTE_P;
        } else {
            return NULL;
        }
    }
    // 这里使用的地址也是虚拟地址
    // 返回的是pte，因此要先在页目录表找到这一个二级页表，再取这一项
    return &(((pte_t*)KADDR(*pdep & ~0xfff))[PTX(la)]);
}
```

> 最后一行解析
>
> -   `&(((pte_t*)KADDR(*pdep & ~0xfff))[PTX(la)])`&(...) 取地址，因为返回的是`pte`的指针，括号内就是要的`pte`
> -   `((pte_t*)KADDR(*pdep & ~0xfff))[PTX(la)]` 右侧`[PTX(la)]`数组下标，`PTX(la)`是返回`la`在对应二级页表中的下标，其实就是中间十位。
> -   `((pte_t*)KADDR(*pdep & ~0xfff))`外侧括号表明是个整体，是一个数组
> -   `(pte_t*)KADDR(*pdep & ~0xfff)`左侧`(pre_t*)`把右边这个地址转换成`pte_t`的指针，从而可以用数组方式访问
> -   `KADDR(*pdep & ~0xfff)` `KADDR()`把物理地址转换成内核虚拟地址
> -   `*pdep & ~0xfff*` `*pdep*`是页目录项的内容，去除后十二位标志，得到对应二级页表的起始地址

---

-   请描述页目录项（`Page Directory Entry`）和页表项（`Page Table Entry`）中每个组成部分的含义以及对`ucore`而言的潜在用处。

    -   `PDE`

        [![cYn1Wn.png](https://z3.ax1x.com/2021/04/08/cYn1Wn.png)](https://imgtu.com/i/cYn1Wn)

        -   P (`Present`) 位：表示该页保存在物理内存中（1），或者不在（0）。
        -   R/W (`Read/Write`) 位：表示该页只读/可读可写。
        -   U/S (`User/Superviosr `) 位：表示该页可以被任何权限用户/超级用户访问。
        -   WT (`Write Through`) 位：写直达/写回。
        -   CD (`Cache Disable`) 位：`Cache`缓存禁止（1）/开启（0）
        -   A (`Access`) 位：表示该页被写过。
        -   PS (`Size`) 位：表示一个页 4MB(1)/4KB(0) 。
        -   G（`global`位）：表示是否将虚拟地址与物理地址的转换结果缓存到 `TLB `中。
        -   Avail： 9-11 位保留给 OS 使用。
        -   12-31 位： `PTE` 基址的高 20 位（由于按页对齐，后 12 位均为 0）。

    -   `PTE`

        [![cYQ3AU.png](https://z3.ax1x.com/2021/04/08/cYQ3AU.png)](https://imgtu.com/i/cYQ3AU)

        -   D(`Dirty`)位：脏位。
        -   12-31 位： 页基址的高 20 位（由于按页对齐，后 12 位均为 0）。

    -   高 20 位保存对应二级页表/页的高二十位地址，低 12 位保存一些标志位。

-   如果`ucore`执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？

    会触发缺页异常， CPU 将产生页访问异常的线性地址放到 cr2 寄存器中，然后触发异常，由异常处理程序根缺页异常类型来进行不同处理，如从磁盘中读取出内存页等。

---

## 练习 3：释放某虚地址所在的页并取消对应二级页表项的映射

当释放一个包含某虚地址的物理内存页时，需要让对应此物理内存页的管理数据结构`Page`做相关的清除处理，使得此物理内存页成为空闲；另外还需把表示虚地址与物理地址对应关系的二级页表项清除。

---

用到的一些宏/函数

```c
// pmm.h
// 根据页表项返回对应的Page
static inline struct Page *
pte2page(pte_t pte) {
    if (!(pte & PTE_P)) {
        panic("pte2page called with invalid pte");
    }
    return pa2page(PTE_ADDR(pte));
}

// 释放一页
#define free_page(page) free_pages(page, 1)

// Page的ref减1
// 当ref为0时，这一个Page需要释放
static inline int
page_ref_dec(struct Page *page) {
    page->ref -= 1;
    return page->ref;
}

// pmm.c
// 标记快表中的不可用
// invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor.
void tlb_invalidate(pde_t* pgdir, uintptr_t la) {
    if (rcr3() == PADDR(pgdir)) {
        invlpg((void*)la);
    }
}
```

根据所给注释可以很容易的写出代码。

```c
static inline void page_remove_pte(pde_t* pgdir, uintptr_t la, pte_t* ptep){
	// 首先查看目前的页表项是否有效，查看最低位PTE_P
    if (*ptep & 0x1) {
        // 有效就获得对应的Page结构
        struct Page* page = pte2page(*ptep);
        // 然后减少Page的ref位-1，并且根据返回值来判断是否应该释放这一Page
        if (!page_ref_dec(page)) {
            // 如果为0则释放这一Page
            free_page(page);
        }
        // 清除二级页表项
        *ptep = 0;
        // 标记TLB中的这一Page无效
        tlb_invalidate(pgdir, la);
    }
}
```

这样`Lab2`的`Exercise`就完成了。

`make qemu`得到如下输出：

```c
memory management: default_pmm_manager
e820map:
  memory: 0009fc00, [00000000, 0009fbff], type = 1.
  memory: 00000400, [0009fc00, 0009ffff], type = 2.
  memory: 00010000, [000f0000, 000fffff], type = 2.
  memory: 07ee0000, [00100000, 07fdffff], type = 1.
  memory: 00020000, [07fe0000, 07ffffff], type = 2.
  memory: 00040000, [fffc0000, ffffffff], type = 2.
check_alloc_page() succeeded!
check_pgdir() succeeded!
check_boot_pgdir() succeeded!
-------------------- BEGIN --------------------
PDE(0e0) c0000000-f8000000 38000000 urw
  |-- PTE(38000) c0000000-f8000000 38000000 -rw
PDE(001) fac00000-fb000000 00400000 -rw
  |-- PTE(000e0) faf00000-fafe0000 000e0000 urw
  |-- PTE(00001) fafeb000-fafec000 00001000 -rw
--------------------- END ---------------------
++ setup timer interrupts
100 ticks
```

---

-   数据结构`Page`的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥

    每一个`Page`对应一个物理页帧，这每一个`Page`是通过页目录项和页表项来找到的。

    具体这问题啥意思我也不太懂，但是怎么找明白就行。

    [<img src="https://z3.ax1x.com/2021/04/11/cwLvbn.png" alt="cwLvbn.png" style="zoom:150%;" />](https://imgtu.com/i/cwLvbn)

-   如果希望虚拟地址与物理地址相等，则需要如何修改`lab2`，完成此事？ **鼓励通过编程来具体完成这个问题**

    回顾一下之前的过程，虚拟地址和物理地址为什么不相等？

    因为 ① 内核被加载到了`0xC0100000`开始的地址空间 ② 虚拟内存空间被抬高到了高地址（`KERNBASE`）③`0-4M`的直接映射被取消

    因此只需要

    ① 修改连接脚本，把内核起始虚拟地址修改为`0x100000`

    ```
    // tools/kernel.ld
    /* Load the kernel at this address: "." means the current address */
        . = 0x100000;
    ```

    ② 调整`KERNBASE = 0`

    ```c
    // memlayout.h
    /* All physical memory mapped at this address */
    #define KERNBASE            0x00000000
    ```

    ③ 恢复`0-4M`的直接映射

    ```c
    // entry.S
    # unmap va 0 ~ 4M, it's temporary mapping
    # xorl %eax, %eax
    # movl %eax, __boot_pgdir
    ```

    ④`check...`函数中有很多对于`pgdir[0]`的检查，应该注释掉

---

## 代码分析

#### `pmm_init`

继续前面没分析完的`pmm_init`函数。

---

##### `check_alloc_page()`

通过调用`pmm_manager`的`check()`来检查物理内存页分配，实际调用的是`default_pmm_manager`的`default_check()`函数。

内部主要是申请一些页然后再释放的过程，主要是完成对`first-fit`算法的检查。具体过程不再叙述。

```c
static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
```

---

##### `check_pgdir()`

检查页目录表和页表，主要也是通过分配和释放来检测`PTE`是否正常，完成的是对练习 2 和练习 3 的检查。具体过程也不再叙述。

---

##### `static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0)`

`PTSIZE`是一个二级页表的一个页能对应的物理页的大小（`1024*4096`）。

这里检测两个定义好的常量，具体意义不是很清楚，应该是为了下面内核页表的自映射。

---

##### `boot_pgdir[PDX(VPT)] = PADDR(boot_pgdir) | PTE_P | PTE_W`

将当前内核页表的物理地址设置进对应的页目录项中(内核页表的自映射)

关于页表自映射，首先主要目的是为了少存储进程的`4GB`的虚拟内存空间所对应的`4KB`的页表，从而节省`4KB`空间，代价是要求这`4GB`的页表连续在一个`4MB`的按`4MB`对齐的内存空间中。

下面说具体原理。（有些复杂）

[<img src="https://z3.ax1x.com/2021/04/10/cdQ1W8.png" alt="cdQ1W8.png" style="zoom: 33%;" />](https://imgtu.com/i/cdQ1W8)

所有页表位于连续内存，也就是说所有页表可以存储在一个`Table Frame`中。

下面，假设所有页表就存储在`Table Frame x `中，那么这个`Table Frame x`存储着 1024 个`4KB`的页表（`Table 0`、`Table 1`、...、`Table 1023`），其中每个页表对应一个`Table Frame`，也就是说`Table x`就对应`Table Frame x`。

页目录表中每个`PDE`，指向对应的一个页表`Table`。

`Table x`有 1024 个`PTE`，每个`PTE`对应一个`Page`（也就是`Table Frame x`中的对应`Page`），`Page`实质上就是页表中的`Table`。

因此`Table x`指向了页表，和页目录的作用一样，因此就可以用`Table x`来代替页目录表。

---

下面解读这行代码`boot_pgdir[PDX(VPT)] = PADDR(boot_pgdir) | PTE_P | PTE_W`

```c
// memlayout.h
/* *
 * Virtual page table. Entry PDX[VPT] in the PD (Page Directory) contains
 * a pointer to the page directory itself, thereby turning the PD into a page
 * table, which maps all the PTEs (Page Table Entry) containing the page mappings
 * for the entire virtual address space into that 4 Meg region starting at VPT.
 * */
#define VPT                 0xFAC00000
```

`VPT`虚拟页表，就是一个地址，这里存的就是指向`Table x`的页目录表。

我们知道`Table x`就是页目录表，这句话就是给这个`PDE`的值赋为页目录表的地址。

首先要得到这一项，`boot_pgdir`是页目录表，利用`PDX(VPT)`获得这一地址对应的下标，从而得到这一个`PDE`。

然后设置其值为页目录表的地址，这里注意是物理地址。

---

##### `boot_map_segment(boot_pgdir, KERNBASE, KMEMSIZE, 0, PTE_W)`

将内核所占用的物理内存，进行页表<=>物理页的映射。令处于高位虚拟内存空间的内核，正确的映射到低位的物理内存空间。

映射关系(虚实映射): 内核起始虚拟地址(`KERNBASE`)~内核截止虚拟地址(`KERNBASE+KMEMSIZE`) = 内核起始物理地址(0)~内核截止物理地址(`KMEMSIZE`)。

看函数。

```c
// pmm.c
// boot_map_segment - setup&enable the paging mechanism
// parameters
//  la:   linear address of this memory need to map (after x86 segment map)
//  size: memory size
//  pa:   physical address of this memory
//  perm: permission of this memory
static void boot_map_segment(pde_t* pgdir,
                             uintptr_t la,
                             size_t size,
                             uintptr_t pa,
                             uint32_t perm) {
    assert(PGOFF(la) == PGOFF(pa));
    // 计算一下有多少个页需要去进行映射
    size_t n = ROUNDUP(size + PGOFF(la), PGSIZE) / PGSIZE;
    la = ROUNDDOWN(la, PGSIZE);
    pa = ROUNDDOWN(pa, PGSIZE);
    for (; n > 0; n--, la += PGSIZE, pa += PGSIZE) {、
        // 调用get_pte给每个页分配一个PTE
        pte_t* ptep = get_pte(pgdir, la, 1);
        assert(ptep != NULL);
        // 给PTE填写对应页的物理地址
        *ptep = pa | PTE_P | perm;
    }
}

// mmu.h
// offset in page
// 地址就是最后十二位，页内偏移
#define PGOFF(la) (((uintptr_t)(la)) & 0xFFF)
```

这个函数建立了完整的一一映射的二级页表，彻底使能了分页机制。（之前只有内核所在的`0-4MB`空间的映射）

---

##### `gdt_init()`

重新设置`GDT`与`TSS`。

---

`TSS`是任务状态段，保存了一些寄存器的值和不同特权级下的`SS`，方便在特权级切换时得到对应特权级的`SS`和`ESP`。

[![cd0DsI.jpg](https://z3.ax1x.com/2021/04/10/cd0DsI.jpg)](https://imgtu.com/i/cd0DsI)

---

`TSS descriptor`用来描述`TSS`的某些性质，只能存放在`GDT`中。结构如下:

[![cdcgnH.gif](https://z3.ax1x.com/2021/04/10/cdcgnH.gif)](https://imgtu.com/i/cdcgnH)

基址`BASE`由三部分拼成。`Limit`是`TSS`的长度。

`OS`通过查看`GDT`里面的`TSS descriptor`来找到`TSS`。

---

`TR Task Rigister`分为可见部分和不可见部分。可见部分指向`GDT`中的`TSS Descriptor`，不可见部分保存`TSS Descriptor`中的`Base`和`Limit`，从而加快对于`TSS`的访问。

[![cd29it.gif](https://z3.ax1x.com/2021/04/10/cd29it.gif)](https://imgtu.com/i/cd29it)

---

在这个函数中，我们就是要对`GDT`中的`TSS Descriptor`设置一下，然后重新装载`GDT`并设置`TR`。

注意在`ucore`中只有`SS0`和`ESP0`有用处。

```c
// pmm.c
/* gdt_init - initialize the default GDT and TSS */
static void gdt_init(void) {
    // set boot kernel stack and default SS0
    load_esp0((uintptr_t)bootstacktop);
    ts.ts_ss0 = KERNEL_DS;

    // initialize the TSS filed of the gdt
    // STS_T32A表明是可用的
    gdt[SEG_TSS] = SEGTSS(STS_T32A, (uintptr_t)&ts, sizeof(ts), DPL_KERNEL);

    // reload all segment registers
    lgdt(&gdt_pd);

    // load the TSS
    // 调用ltr指令设置TR寄存器，操作数是对应TSS段描述符的选择子。
    ltr(GD_TSS);
}

/* *
 * load_esp0 - change the ESP0 in default task state segment,
 * so that we can use different kernel stack when we trap frame
 * user to kernel.
 * */
void load_esp0(uintptr_t esp0) {
    ts.ts_esp0 = esp0;
}

// 构造一个TSS Descriptor
#define SEGTSS(type, base, lim, dpl)                        \
    (struct segdesc) {                                      \
        (lim) & 0xffff, (base) & 0xffff,                    \
        ((base) >> 16) & 0xff, type, 0, dpl, 1,             \
        (unsigned) (lim) >> 16, 0, 0, 1, 0,                 \
        (unsigned) (base) >> 24                             \
    }

// GDT是在上面写好的，但是TSS是空
static struct segdesc gdt[] = {
    SEG_NULL,
    [SEG_KTEXT] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_KDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_UTEXT] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_UDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_TSS] = SEG_NULL,
};
```

---

##### `check_boot_pgdir()`

检查页表建立是否正确。

目前虚拟地址空间就建立好了，结构如图：

```
 Virtual memory map:                                          Permissions
                                                              kernel/user

     4G ------------------> +---------------------------------+
                            |                                 |
                            |         Empty Memory (*)        |
                            |                                 |
                            +---------------------------------+ 0xFB000000
                            |   Cur. Page Table (Kern, RW)    | RW/-- PTSIZE
     VPT -----------------> +---------------------------------+ 0xFAC00000
                            |        Invalid Memory (*)       | --/--
     KERNTOP -------------> +---------------------------------+ 0xF8000000
                            |                                 |
                            |    Remapped Physical Memory     | RW/-- KMEMSIZE
                            |                                 |
     KERNBASE ------------> +---------------------------------+ 0xC0000000
                            |                                 |
                            |                                 |
                            |                                 |
                            ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 (*) Note: The kernel ensures that "Invalid Memory" isnever* mapped.
     "Empty Memory" is normally unmapped, but user programs may map pages
     there if desired.
```

检查的具体代码就不看了，检查的项目主要有页表对应的地址与物理地址是不是相等；页的自映射机制；临时的二级页表有没有取消；两个不同逻辑地址映射到一个物理地址。

---

##### `print_pgdir()`

利用页的自映射机制打印出当前的页目录表和页表。

```c
//print_pgdir - print the PDT&PT
void
print_pgdir(void) {
    cprintf("-------------------- BEGIN --------------------\n");
    size_t left, right = 0, perm;
    while ((perm = get_pgtable_items(0, NPDEENTRY, right, vpd, &left, &right)) != 0) {
        cprintf("PDE(%03x) %08x-%08x %08x %s\n", right - left,
                left * PTSIZE, right * PTSIZE, (right - left) * PTSIZE, perm2str(perm));
        size_t l, r = left * NPTEENTRY;
        while ((perm = get_pgtable_items(left * NPTEENTRY, right * NPTEENTRY, r, vpt, &l, &r)) != 0) {
            cprintf("  |-- PTE(%05x) %08x-%08x %08x %s\n", r - l,
                    l * PGSIZE, r * PGSIZE, (r - l) * PGSIZE, perm2str(perm));
        }
    }
    cprintf("--------------------- END ---------------------\n");
}
```

其中一些变量

```c
pte_t* const vpt = (pte_t*)VPT;
pde_t* const vpd = (pde_t*)PGADDR(PDX(VPT), PDX(VPT), 0);
```

`vpd`的值就是页目录表的起始虚地址，`vpt`是页目录表中第一个目录表项指向的页表的起始虚地址，由于页表自映射机制，因此就两个相同的拼起来。

其中`PGADDR`就是通过知道`PDE`的`index`和`offset`拼出的逻辑地址。

利用`get_pgtable_items`找到一块连续的地址空间，具备相同的属性。再打印出来。具体也不再看了。

```c
// get_pgtable_items - In [left, right] range of PDT or PT, find a continuous
// linear addr space
//                  - (left_store*X_SIZE~right_store*X_SIZE) for PDT or PT
//                  - X_SIZE=PTSIZE=4M, if PDT; X_SIZE=PGSIZE=4K, if PT
// paramemters:
//  left:        no use ???
//  right:       the high side of table's range
//  start:       the low side of table's range
//  table:       the beginning addr of table
//  left_store:  the pointer of the high side of table's next range
//  right_store: the pointer of the low side of table's next range
// return value: 0 - not a invalid item range, perm - a valid item range with
// perm permission
static int get_pgtable_items(size_t left,
                             size_t right,
                             size_t start,
                             uintptr_t* table,
                             size_t* left_store,
                             size_t* right_store) {
    if (start >= right) {
        return 0;
    }
    while (start < right && !(table[start] & PTE_P)) {
        start++;
    }
    if (start < right) {
        if (left_store != NULL) {
            *left_store = start;
        }
        int perm = (table[start++] & PTE_USER);
        while (start < right && (table[start] & PTE_USER) == perm) {
            start++;
        }
        if (right_store != NULL) {
            *right_store = start;
        }
        return perm;
    }
    return 0;
}
```

---

置此`pmm_init`过程结束，余下过程与`lab1`一致，不再赘述。

---

## 系统执行中地址映射的三个阶段

其实具体过程前面都讲完了，简单总结一下。

---

在`lab1`的链接脚本中，内核起始虚拟地址`0x100000`。

```
/* Load the kernel at this address: "." means the current address */
            . = 0x100000;
```

`bootloader`实际上也加载内核到了地址`0x100000`，因此在`lab1`中有对等映射`virt addr = linear addr = phy addr`

在`lab2`的链接脚本中，内核起始虚拟地址`0xC0100000`。

```
/* Load the kernel at this address: "." means the current address */
            . = 0xC0100000;
```

内核加载位置不变，但虚拟地址发生改变。所以最终映射关系` virt addr = linear addr = phy addr + 0xC0000000`

---

### 第一个阶段`bootloader`

开启保护模式，探测物理内存，建立堆栈，加载`kernel`，跳转给`kernel`

从`bootloader`的`start`函数（在`boot/bootasm.S`中）到执行`ucore kernel`的`kern_entry`函数之前，其虚拟地址、线性地址以及物理地址之间的映射关系与 lab1 的一样，即：

```
 lab2 stage 1: virt addr = linear addr = phy addr
```

---

### 第二个阶段 临时页表

通过加载临时页表，有了如下两个映射。

```
 lab2 stage 2 before:
     virt addr = linear addr = phy addr # 线性地址在0~4MB之内三者的映射关系
     virt addr = linear addr = phy addr + 0xC0000000 # 线性地址在0xC0000000~0xC0000000+4MB之内三者的映射关系
```

仅比刚才多了一个高地址映射，实际上是为了正确调整内核虚地址到高地址。因为在调用操作系统时，手动与`0xFFFFFF`做与将其映射到低地址，而低地址是留给用户程序的，因此要把内核地址改为正确的高地址。

设置好之后，`eip`还在低地址，因此抬高`eip`。（具体过程见代码分析`entry.S`）

之后再取消低地址的对等映射，因此二阶段最终映射关系如下。

```
lab2 stage 2: virt addr = linear addr = phy addr + 0xC0000000 # 线性地址在0~4MB之内三者的映射关系
```

这还不是我们期望的映射关系，因为它仅仅映射了`0~4MB`。对于段表而言，也缺少了运行`ucore`所需的用户态段描述符和`TSS`（段）描述符相应表项。

---

### 第三个阶段 完善页表映射

在`pmm_init`中的`boot_map_segment(boot_pgdir, KERNBASE, KMEMSIZE, 0, PTE_W)`建立了完整的页表映射。

之后又在`gdt_init`函数中用了一个新的段表。这个新段表除了包括内核态的代码段和数据段描述符，还包括用户态的代码段和数据段描述符以及`TSS`（段）的描述符。

最终映射关系：

```
 lab2 stage 3: virt addr = linear addr = phy addr + 0xC0000000
```

对比一下前后的`gdt`内容

前：

```assembly
# Bootstrap GDT
.p2align 2                                          # force 4 byte alignment
gdt:
    SEG_NULLASM                                     # null seg
    SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)           # code seg for bootloader and kernel
    SEG_ASM(STA_W, 0x0, 0xffffffff)                 # data seg for bootloader and kernel
```

后：

```c
static struct segdesc gdt[] = {
    SEG_NULL,
    [SEG_KTEXT] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_KDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_UTEXT] = SEG(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_UDATA] = SEG(STA_W, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_TSS] = SEG_NULL,
};
```

后来的`TSS`是后面代码补充上的。

## 实验指导书中的一些内容

### 关于`end`

**`edata/end/text`的含义**

在基于`ELF`执行文件格式的代码中，存在一些对代码和数据的表述，基本概念如下：

-   `BSS`段（`bss segment`）：指用来存放程序中未初始化的全局变量的内存区域。`BSS`是英文`Block Started by Symbol`的简称。`BSS`段属于静态内存分配。
-   数据段（`data segment`）：指用来存放程序中已初始化的全局变量的一块内存区域。数据段属于静态内存分配。
-   代码段（`code segment/text segment`）：指用来存放程序执行代码的一块内存区域。这部分区域的大小在程序运行前就已经确定，并且内存区域通常属于只读, 某些架构也允许代码段为可写，即允许修改程序。在代码段中，也有可能包含一些只读的常数变量，例如字符串常量等。

在`lab2/kern/init/init.c`的`kern_init`函数中，声明了外部全局变量：

```c
extern char edata[], end[];
```

但搜寻所有源码文件`*.[ch]`，没有发现有这两个变量的定义。那这两个变量从哪里来的呢？其实在`lab2/tools/kernel.ld`中，可以看到如下内容：

```
…
.text : {
        *(.text .stub .text.* .gnu.linkonce.t.*)
}
…
    .data : {
        *(.data)
}
…
PROVIDE(edata = .);
…
    .bss : {
        *(.bss)
}
…
PROVIDE(end = .);
…
```

这里的`.`表示当前地址，`.text`表示代码段起始地址，“`.data`”也是一个地址，可以看出，它即代表了代码段的结束地址，也是数据段的起始地址。类推下去，`edata`表示数据段的结束地址，`.bss`表示数据段的结束地址和`BSS`段的起始地址，而“`end`”表示`BSS`段的结束地址。

这样回头看`kerne_init`中的外部全局变量，可知`edata[]`和 `end[]`这些变量是`ld`根据`kernel.ld`链接脚本生成的全局变量，表示相应段的起始地址或结束地址等，它们不在任何一个`.S`、`.c`或`.h`文件中定义。
