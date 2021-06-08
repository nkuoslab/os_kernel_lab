<center><h3>实验四 内核线程管理</h3></center>
<div align='right'>学号： 1911463 姓名: 时浩铭 </div>
<div align='right'>学号： 1911477 姓名: 王耀群 </div>
<div align='right'>学号： 1911547 姓名: 李文婕 </div>

### 进程与线程

进程，一个具有一定独立功能的程序在一个数据集合上的一次动态执行过程。这里有四个关键词：程序、数据集合、执行和动态执行过程。从`CPU`的角度来看，所谓程序就是一段特定的指令机器码序列而已。`CPU`会一条一条地取出在内存中程序的指令并按照指令的含义执行各种功能；所谓数据集合就是使用的内存；所谓执行就是让`CPU`工作。这个数据集合和执行其实体现了进程对资源的占用。动态执行过程体现了程序执行的不同“生命”阶段：诞生、工作、休息/等待、死亡。如果这一段指令执行完毕，也就意味着进程结束了。从开始执行到执行结束是一个进程的全过程。

物理层面上，一个`CPU`核心同一时间只能运行一个程序，或者说一个`CPU`核心某一时刻只能归属于一个特定进程。但逻辑层面上，操作系统可以进行进程调度，既可以为进程分配`CPU`资源，令其执行，也可以在发生等待外设I/O时，避免`CPU`空转而暂时挂起当前进程，令其它进程获得`CPU`。

进程能够随时在执行与挂起中切换，且每次恢复运行时都能够接着上次被打断挂起的地方接着执行。这就需要操作系统有能力保留进程在被挂起时的`CPU`寄存器上下文快照，当`CPU`中的寄存器被另外的进程给覆盖后，在恢复时能正确的还原之前被打断时的执行现场。新老进程在`CPU`上交替时，新调度线程上下文的恢复和被调度线程上下文的保存行为被称作进程的上下文切换。

进程是一个独立的程序，与其它进程的内存空间是相互隔离的，也作为一个`CPU`调度的单元工作。

线程是属于进程的，同一进程下所有线程都共享进程拥有的同一片内存空间，没有额外的访问限制；但每个线程有着自己的执行流和调度状态，包括程序计数器在内的`CPU`寄存器上下文是线程间独立的。这样上述的需求就能通过在文件处理进程中开启两个线程分别提供用户服务和后台批处理服务来实现。通过操作系统合理的调度，既能实时的处理用户指令，又不耽误后台的批处理任务。

进程管理就是管理进程执行的指令，进程占用的资源，进程执行的状态。这可归结为对一个进程内的管理工作。但实际上在计算机系统的内存中，可以放很多程序，这也就意味着操作系统需要管理多个进程，那么，为了协调各进程对系统资源的使用，进程管理还需要做一些与进程协调有关的其他管理工作，包括进程调度、进程间的数据共享、进程间执行的同步互斥关系。

操作系统负责进程管理，即从程序加载到运行结束的全过程，这个程序运行过程将经历从“出生”到“死亡”的完整“生命”历程。所谓“进程”就是指这个程序运行的整个执行过程。为了记录、描述和管理程序执行的动态变化过程，需要有一个数据结构，这就是进程控制块。进程与进程控制块是一一对应的。

-----

报告中进程与线程在本实验中均指内核线程，下面对这两种说法不做区分。

-----

### 关键数据结构——进程控制块

#### `proc_state`

在`ucore`中，并不对进程与线程显示区分，都适用同样的数据结构`proc_struct`来进行管理。当不同的`proc_struct`对应的页表`cr3`相同时，`ucore`认为是同一进程的不同线程。

```c
// proc.h

// process's state in his life cycle
// 进程所处的状态
enum proc_state {
    // 未初始化
    PROC_UNINIT = 0,  // uninitialized
    // 休眠、阻塞状态
    PROC_SLEEPING,    // sleeping
    // 可运行、就绪状态
    PROC_RUNNABLE,    // runnable(maybe running)
    // 僵尸状态(几乎已经终止，等待父进程回收其所占资源)
    PROC_ZOMBIE,      // almost dead, and wait parent proc to reclaim his resource
};

// 保存寄存器的目的就在于在内核态中能够进行上下文之间的切换
// 只保存会随着进程切换而改变的寄存器
// Saved registers for kernel context switches.
// Don't need to save all the %fs etc. segment registers,
// because they are constant across kernel contexts.
// Save all the regular registers so we don't need to care
// which are caller save, but not the return register %eax.
// (Not saving %eax just simplifies the switching code.)
// The layout of context must match code in switch.S.
struct context {
    uint32_t eip;
    uint32_t esp;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
};

/**
 * 进程控制块结构（ucore进程和线程都使用proc_struct进行管理）
 * */
struct proc_struct {
    // 进程状态
    enum proc_state state;                      // Process state
    // 进程id
    int pid;                                    // Process ID
    // 被调度执行的总次数/时间片数
    int runs;                                   // the running times of Proces
    // 当前进程内核栈底地址（思考下下面哪里可以反映出这是栈底）
    uintptr_t kstack;                           // Process kernel stack
    // 是否需要被重新调度，以使当前线程让出CPU
    volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
    // 当前进程的父进程
    struct proc_struct *parent;                 // the parent process
    // 当前进程关联的内存总管理器
    struct mm_struct *mm;                       // Process's memory management field
    // 切换进程时保存的上下文快照
    struct context context;                     // Switch here to run process
    // 切换进程时的当前中断栈帧
    struct trapframe *tf;                       // Trap frame for current interrupt
    // 当前进程页表基地址寄存器cr3(指向当前进程的页表物理地址)
    uintptr_t cr3;                              // CR3 register: the base addr of Page Directroy Table(PDT)
    // 当前进程的状态标志位
    uint32_t flags;                             // Process flag
    // 进程名
    char name[PROC_NAME_LEN + 1];               // Process name
    // 进程控制块链表节点
    list_entry_t list_link;                     // Process link list 
    // 进程控制块哈希表节点
    list_entry_t hash_link;                     // Process hash list
};
```

##### 几个变量的说明

- `mm`：内存管理的信息，包括内存映射列表、页表的指针等，用于虚拟内存的管理。在`lab4`中由于只涉及到内核线程，不涉及内存的换入换出，因此直接`mm=null`即可。但这样就无法访问关键变量`pgdir`，因此单独再设一个`cr3`记录页目录的地址。
- `parent`：父进程（创建它的进程）的指针。只有内核创建的0号线程`idleproc`没有父进程。内核根据父子关系构建树结构，用于一些特殊操作。
- `context`：上下文，用于进程切换。实际利用它进行上下文切换的函数`switch_to`位于`switch.S`里。
- `tf`：中断帧的指针，指向内核栈的某个位置。当进程从用户空间跳到内核空间时，中断帧记录了进程在被中断前的状态。当内核需要跳回用户空间时，需要调整中断帧以恢复让进程继续执行的各寄存器值。`ucore`内核允许嵌套中断，因此需要维护`tf`的链。
- `cr3`：保存页目录表的**物理**地址，目的就是进程切换的时候方便直接使用 `lcr3`实现页表切换，避免每次都根据 `mm` 来计算 `cr3`。某个进程是一个普通用户态进程的时候， `cr3` 就是 `mm` 中页目录表（`pgdir`）的物理地址；而当它是内核线程的时候，`cr3` 等于`boot_cr3`。而`boot_cr3`指向了`ucore`启动时建立好的内核虚拟空间的页目录表首地址。

- `kstack`：内核栈。每个线程都有一个内核栈，并且位于内核地址空间的不同位置。对于内核线程，该栈就是运行时的程序使用的栈；而对于普通进程，该栈是发生特权级改变的时候使保存被打断的硬件信息用的栈。内核栈的空间仅有`8KB`。

  两个作用：①当内核准备从一个进程切换到另一个的时候，需要根据`kstack`的值正确的设置好`tss`；②内核栈位于内核地址空间，并且是不共享的（每个线程都拥有自己的内核栈），因此不受到`mm `的管理，当进程退出的时候，内核能够根据`kstack`的值快速定位栈的位置并进行回收。

##### 几个相关的全局变量

```c
// proc.c

#define HASH_SHIFT          10
#define HASH_LIST_SIZE      (1 << HASH_SHIFT)

// the process set's list
list_entry_t proc_list;

// hash list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0;
```

- `current`：当前占用CPU且处于“运行”状态进程控制块指针。通常这个变量是只读的，只有在进程切换的时候才进行修改，并且整个切换和修改过程需要保证操作的原子性，目前至少需要屏蔽中断。
- `initproc`：本实验中，指向一个内核线程。本实验以后，此指针将指向第一个用户态进程。
- `hash_list`：所有进程控制块的哈希表，`proc_struct`中的成员变量`hash_link`将基于`pid`链接入这个哈希表中。
- `proc_list`：所有进程控制块的双向链表，`proc_struct`中的成员变量`list_link`将链接入这个链表中。
- `nr_process`：进程的数量

#### 练习1：分配并初始化一个进程控制块——`alloc_page`

`alloc_proc`函数（位于`kern/process/proc.c`中）负责分配并返回一个新的`struct proc_struct`结构，用于存储新建立的内核线程的管理信息。`ucore`需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。

很简单的初始化过程没什么可说的。

```c
// proc.c
// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
    //LAB4:EXERCISE1 YOUR CODE
    /*
     * below fields in proc_struct need to be initialized
     *       enum proc_state state;                      // Process state
     *       int pid;                                    // Process ID
     *       int runs;                                   // the running times of Proces
     *       uintptr_t kstack;                           // Process kernel stack
     *       volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
     *       struct proc_struct *parent;                 // the parent process
     *       struct mm_struct *mm;                       // Process's memory management field
     *       struct context context;                     // Switch here to run process
     *       struct trapframe *tf;                       // Trap frame for current interrupt
     *       uintptr_t cr3;                              // CR3 register: the base addr of Page Directroy Table(PDT)
     *       uint32_t flags;                             // Process flag
     *       char name[PROC_NAME_LEN + 1];               // Process name
     */
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = NULL;
        proc->need_resched = NULL;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->cr3 = boot_cr3;
        proc->flags = 0;
        memset(&(proc->name), 0, PROC_NAME_LEN + 1);
    }
    return proc;
}
```

- 请说明proc_struct中`struct context context`和`struct trapframe *tf`成员变量含义和在本实验中的作用是啥

  `context`前面也说了，保存八个寄存器的内容，保存进程上下文，为进程调度做准备。

  `trapframe`保存用于特权级转换的栈和`esp`寄存器等，当发生特权级的转换时，中断帧记录了进入中断时进程的上下文，当中断退出时恢复环境。

### 进程的创建与初始化

#### `proc_init`

`proc_init`函数完成了`idleproc`内核线程和`initproc`内核线程的创建或复制工作。

整个`ucore`内核可以被视为一个进程(内核进程)，而上述两个线程的`cr3`指向内核页表`boot_cr3`，且其代码段、数据段选择子特权级都处于内核态，属于内核线程。

`idleproc`内核线程的工作就是不停地查询，看是否有其他内核线程可以执行了，如果有，马上让调度器选择那个内核线程执行。所以`idleproc`内核线程是在`ucore`操作系统没有其他内核线程可执行的情况下才会被调用。

接着就是调用`kernel_thread`函数来创建`initproc`内核线程。`initproc`内核线程的工作就是显示`“Hello World”`，表明自己存在且能正常工作了。*后续实验中，其工作是创建特定的其他内核线程或用户进程。*

```c
// proc.c
// proc_init - set up the first kernel thread idleproc "idle" by itself and 
//           - create the second kernel thread init_main
// 初始化第一个内核线程idleproc线程、第二个内核线程initproc线程
void proc_init(void) {
    int i;

    // 初始化全局的线程控制块双向链表
    list_init(&proc_list);
    // 初始化全局的线程控制块hash表
    for (i = 0; i < HASH_LIST_SIZE; i++) {
        list_init(hash_list + i);
    }

    // 分配idleproc的proc_struct
    if ((idleproc = alloc_proc()) == NULL) {
        panic("cannot alloc idleproc.\n");
    }

    // 为idle线程进行初始化
    idleproc->pid = 0; // idle线程pid作为第一个内核线程，其不会被销毁，pid为0
    idleproc->state = PROC_RUNNABLE; // idle线程被初始化时是就绪状态的
    idleproc->kstack = (uintptr_t)bootstack; // idle线程是第一个线程，其内核栈指向bootstack，而以后的线程的内核栈都需要通过分配得到
    idleproc->need_resched = 1; // idle线程被初始化后，需要马上被调度
    // 设置idle线程的名称
    set_proc_name(idleproc, "idle");
    nr_process++;

    // current当前执行线程指向idleproc
    current = idleproc;

    // 初始化第二个内核线程initproc， 用于执行init_main函数，参数为"Hello world!!"
    int pid = kernel_thread(init_main, "Hello world!!", 0);
    if (pid <= 0) {
        // 创建init_main线程失败
        panic("create init_main failed.\n");
    }

    // 获得initproc线程控制块
    initproc = find_proc(pid);
    // 设置initproc线程的名称
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}
```

##### `kernel_thread`

创建一个内核线程。

``` c
// proc.c
// kernel_thread - create a kernel thread using "fn" function
// NOTE: the contents of temp trapframe tf will be copied to 
//       proc->tf in do_fork-->copy_thread function
// 创建一个内核线程，并执行参数fn函数，arg作为fn的参数
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) {
    struct trapframe tf;
    // 构建一个临时的中断栈帧tf，用于do_fork中的copy_thread函数(因为线程的创建和切换是需要利用CPU中断返回机制的)
    memset(&tf, 0, sizeof(struct trapframe));
    // 设置tf的值
    tf.tf_cs = KERNEL_CS; // 内核线程，设置中断栈帧中的代码段寄存器CS指向内核代码段
    tf.tf_ds = tf.tf_es = tf.tf_ss = KERNEL_DS; // 内核线程，设置中断栈帧中的数据段寄存器指向内核数据段
    tf.tf_regs.reg_ebx = (uint32_t)fn; // 设置中断栈帧中的ebx指向fn的地址
    tf.tf_regs.reg_edx = (uint32_t)arg; // 设置中断栈帧中的edx指向arg的起始地址
    tf.tf_eip = (uint32_t)kernel_thread_entry; // 设置tf.eip指向kernel_thread_entry这一统一的初始化的内核线程入口地址
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}
```

这个函数主要构造了一个临时的中断帧`tf`，传给`do_fork`从而创建新的进程。其中执行的函数地址保存在`tf_regs.reg_ebx`中，参数保存在`tf_regs.reg_edx`中。

所有内核线程（除`idleproc`）的初始化入口函数为`kernel_thread_entry`。

##### `kernel_thread_entry`

```ass
// /kern/process/entry.S
.text
.globl kernel_thread_entry
kernel_thread_entry:        # void kernel_thread(void)

    pushl %edx              # push arg
    call *%ebx              # call fn

    pushl %eax              # save the return value of fn(arg)
    call do_exit            # call do_exit to terminate current thread
```

`kernel_thread_entry`函数主要为内核线程的主体`fn`函数做了一个准备开始和结束运行的“壳”，并把函数`fn`的参数`arg`（保存在`edx`寄存器中）压栈，然后调用`fn`函数，把函数返回值`eax`寄存器内容压栈，调用`do_exit`函数退出线程执行。

##### `do_exit`

```c
// proc.c
// 这些注释应该不是这次实验需要的
// 这次实验只有initproc会do_exit，因此执行完以后结束整个系统就行了
// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory
//   space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask
//   parent reclaim itself.
//   3. call scheduler to switch to other process
int do_exit(int error_code) {
    panic("process exit!!.\n");
}
```

##### `find_proc`

根据`pid`查找`proc`，在哈希链表里查找。

```c
#define pid_hashfn(x)       (hash32(x, HASH_SHIFT))

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct* find_proc(int pid) {
    if (0 < pid && pid < MAX_PID) {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list) {
            struct proc_struct* proc = le2proc(le, hash_link);
            if (proc->pid == pid) {
                return proc;
            }
        }
    }
    return NULL;
}

```

#### 练习2：为新创建的内核线程分配资源

`do_fork`的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。

大致执行步骤：

1. 分配并初始化进程控制块（`alloc_proc`函数）；
2. 分配并初始化内核栈（`setup_kstack`函数）；
3. 根据`clone_flag`标志复制或共享进程内存管理结构（`copy_mm`函数）；（但内核线程不必做此事）
4. 设置进程在内核（将来也包括用户态）正常运行和调度所需的中断帧和执行上下文（`copy_thread`函数）；
5. 把设置好的进程控制块放入`hash_list`和`proc_list`两个全局进程链表中；
6. 自此，进程已经准备好执行了，把进程状态设置为“就绪”态；
7. 设置返回码为子进程的`id`号。

如果上述前3步执行没有成功，则需要做对应的出错处理，把相关已经占有的内存释放掉。

##### `setup_kstack`

```c
// proc.c
// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int setup_kstack(struct proc_struct* proc) {
    struct Page* page = alloc_pages(KSTACKPAGE);
    if (page != NULL) {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}
```

##### `copy_mm`

`copy_mm`函数目前只是把`current->mm`设置为`NULL`，这是由于目前在实验四中只能创建内核线程，`proc->mm`描述的是进程用户态空间的情况，所以目前`mm`还用不上。

根据`clone_flag`标志复制或共享进程内存管理结构。

```c
// proc.c
// copy_mm - process "proc" duplicate OR share process "current"'s mm according
// clone_flags
//         - if clone_flags & CLONE_VM, then "share" ; else "duplicate"
static int copy_mm(uint32_t clone_flags, struct proc_struct* proc) {
    assert(current->mm == NULL);
    /* do nothing in this project */
    return 0;
}
```

##### `copy_thread`

此函数首先在内核堆栈的顶部设置中断帧大小的一块栈空间，并在此空间中拷贝在`kernel_thread`函数建立的临时中断帧的初始值，并进一步设置中断帧中的栈指针`esp`和标志寄存器`eflags`，特别是`eflags`设置了`FL_IF`标志，这表示此内核线程在执行过程中，能响应中断，打断当前的执行。

设置好中断帧后，最后就是设置`initproc`的进程上下文。一旦`ucore`调度器选择了`initproc`执行，就需要根据`initproc->context`中保存的执行现场来恢复`initproc`的执行。这里设置了`initproc`的执行现场中主要的两个信息：上次停止执行时的下一条指令地址`context.eip`和上次停止执行时的堆栈地址`context.esp`。其实`initproc`还没有执行过，所以这其实就是`initproc`实际执行的第一条指令地址和堆栈指针。

```c
// proc.c
// copy_thread - setup the trapframe on the process's kernel stack top and
//             - setup the kernel entry point and stack of process
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    // 在内核堆栈的顶部设置中断帧大小的一块栈空间
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    // 拷贝在kernel_thread函数建立的临时中断帧的初始值
    *(proc->tf) = *tf;
    // 设置子进程/线程执行完do_fork后的返回值
    proc->tf->tf_regs.reg_eax = 0;
    // 设置中断帧中的栈指针esp
    proc->tf->tf_esp = esp;
    // 使能中断
    proc->tf->tf_eflags |= FL_IF;

    // 令proc上下文中的eip指向forkret,切换恢复上下文后，新线程proc便会跳转至forkret
    proc->context.eip = (uintptr_t)forkret;
    // 令proc上下文中的esp指向proc->tf，指向中断返回时的中断栈帧
    // 因为这个地址就是栈的顶部的地址，这里存的就是tf
    proc->context.esp = (uintptr_t)(proc->tf);
}
```

至此，`initproc`内核线程已经做好准备执行了。

其中断帧如下：

```c
//所在地址位置
initproc->tf= (proc->kstack + KSTACKSIZE) – sizeof(struct trapframe);
//具体内容
initproc->tf.tf_cs = KERNEL_CS;
initproc->tf.tf_ds = initproc->tf.tf_es = initproc->tf.tf_ss = KERNEL_DS;
initproc->tf.tf_regs.reg_ebx = (uint32_t)init_main;
initproc->tf.tf_regs.reg_edx = (uint32_t) ADDRESS of "Helloworld!!";
initproc->tf.tf_eip = (uint32_t)kernel_thread_entry;
initproc->tf.tf_regs.reg_eax = 0;
initproc->tf.tf_esp = esp;
initproc->tf.tf_eflags |= FL_IF;
```

##### `forkret`

`forkret`是所有线程完成初始化后统一跳转的入口。

`forkrets`中令栈顶指针指向了前面设置好的`trap_frame`首地址后，便跳转至`__trapret`，进行了中断返回操作。

在`__trapret`中，会依次将前面设置好的临时`trap_frame`中断栈帧中的各个数据依次还原，执行`iret`，完成中断返回。

```c
// proc.c
void forkrets(struct trapframe *tf);

// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void forkret(void) {
    forkrets(current->tf);
}

// trapentry.S
.globl forkrets
forkrets:
    # set stack to this new process's trapframe
    movl 4(%esp), %esp
    jmp __trapret
        
.globl __trapret
__trapret:
    # restore registers from stack
    popal

    # restore %ds, %es, %fs and %gs
    popl %gs
    popl %fs
    popl %es
    popl %ds

    # get rid of the trap number and error code
    addl $0x8, %esp
    # 此时栈顶是tf.tf_eip 
    iret
```

中断返回时，其`cs`、`eip`会依次从中断栈帧中还原，中断栈帧中`eip`是通过`kern_thread`中的语句指向了`kernel_thread_entry`。因此中断返回后会跳转到`kernel_thread_entry`函数入口处执行。

##### `hash_proc`

```c
// proc.c 

#define pid_hashfn(x)       (hash32(x, HASH_SHIFT))

// hash_proc - add proc into proc hash_list
// 将新进程加入进程的哈希列表中
static void hash_proc(struct proc_struct* proc) {
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}
```

##### `get_pid`

给进程分配`id`

```c
// proc.h
#define MAX_PROCESS                 4096
#define MAX_PID                     (MAX_PROCESS * 2)

// proc.c
// get_pid - alloc a unique pid for process
// 这里的分配范围是(0, MAX_PID)
static int get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct* proc;
    list_entry_t *list = &proc_list, *le;
    // 静态变量的生存期是整个程序运行期间
    // last_pid就是上次分配的pid
    // next_safe是大于last_pid并且值最小的已占用的pid，是为了减少探测次数
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID) {
        // 这里注意0号线程已经被idleproc占用，因此从1开始
        // 并且如果last_pid重置了，那么next_safe也应该重置
        last_pid = 1;
        goto inside;
    }
    
    // 如果last_pid（注意加过1了）仍小于next_safe，说明这个pid未分配，可以直接返回
    // 如果不小于next_safe了，就需要重置next_safe
    
    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list) {
            proc = le2proc(le, list_link);
            // 如果这个last_pid已经被占用了，那就再看看他的下一个满足不满足与MAX_PID和next_safe之间的大小关系，不满足则又需要重置了
            if (proc->pid == last_pid) {
                if (++last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            // 看看next_safe能不能更新成更小的
            } else if (proc->pid > last_pid && next_safe > proc->pid) {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}
```

##### `wakeup_proc`

唤醒线程

```c
// sched.c
void wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}
```

##### `do_fork`

大致执行步骤：

1. 分配并初始化进程控制块（`alloc_proc`函数）；
2. 分配并初始化内核栈（`setup_kstack`函数）；
3. 根据`clone_flag`标志复制或共享进程内存管理结构（`copy_mm`函数）；（但内核线程不必做此事）
4. 设置进程在内核（将来也包括用户态）正常运行和调度所需的中断帧和执行上下文（`copy_thread`函数）；
5. 把设置好的进程控制块放入`hash_list`和`proc_list`两个全局进程链表中；
6. 自此，进程已经准备好执行了，把进程状态设置为“就绪”态；
7. 设置返回码为子进程的`id`号。

如果上述前3步执行没有成功，则需要做对应的出错处理，把相关已经占有的内存释放掉。

```c
// proc.c
/* do_fork -     parent process for a new child process
 * @clone_flags: used to guide how to clone the child process
 * @stack:       the parent's user stack pointer. if stack==0, It means to fork a kernel thread.
 * @tf:          the trapframe info, which will be copied to child process's proc->tf
 */
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    // 分配一个未初始化的线程控制块
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }
    // 其父进程属于current当前进程
    proc->parent = current;

    // 设置分配新线程的内核栈
    if (setup_kstack(proc) != 0) {
        // 分配失败，回滚释放之前所分配的内存
        goto bad_fork_cleanup_proc;
    }
    if (copy_mm(clone_flags, proc) != 0) {
        // 分配失败，回滚释放之前所分配的内存
        goto bad_fork_cleanup_kstack;
    }
    // 复制proc线程时，设置proc的上下文信息
    copy_thread(proc, stack, tf);

    // 关闭中断，防止被中断打断
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        // 生成并设置新的pid
        proc->pid = get_pid();
        // 加入全局线程控制块哈希表
        hash_proc(proc);
        // 加入全局线程控制块双向链表
        list_add(&proc_list, &(proc->list_link));
        nr_process++;
    }
    local_intr_restore(intr_flag);
    // 唤醒proc，令其处于就绪态PROC_RUNNABLE
    wakeup_proc(proc);

    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
    
// put_kstack - free the memory space of process kernel stack
static void put_kstack(struct proc_struct* proc) {
    free_pages(kva2page((void*)(proc->kstack)), KSTACKPAGE);
}
```

##### 请说明`ucore`是否做到给每个新`fork`的线程一个唯一的`id`？

读了`get_pid`的代码应该就明白了。

### 进程的调度与上下文的切换

当`proc_init`执行完毕后，两个线程就被创建好了，但此时在执行的仍是`idleproc`。接着会执行`cpu_idle`进行线程的调度，去执行`initproc`。等到`initproc`执行结束，本实验中操作系统的工作就会结束了。

#### `cpu_idle`

第一次调用时，`current`是`idleproc`，其`need_resched`为1，因此会调用`schedule()`函数要求调度器切换其他处于就绪态的线程执行。

这是`idleproc`的工作。

```c
// proc.c
// cpu_idle - at the end of kern_init, the first kernel thread idleproc will do
// below works
void cpu_idle(void) {
    while (1) {
        if (current->need_resched) {
            schedule();
        }
    }
}
```

#### `schedule`

`schedule`函数中，会先关闭中断，避免调度的过程中被中断再度打断而出现并发问题。然后从`ucore`的就绪线程队列中，按照`FIFO`选择出下一个需要获得`CPU`的就绪线程。

通过`proc_run`函数，令就绪线程的状态从就绪态转变为运行态，并切换线程的上下文，保存`current`线程(例如：`idle_proc`)的上下文，并在`CPU`上恢复新调度线程(例如：`init_proc`)的上下文。

```c
// sched.c
void schedule(void) {
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;
    // 暂时关闭中断，避免被中断打断，引起并发问题
    local_intr_save(intr_flag);
    {
        // 令current线程处于不需要调度的状态
        current->need_resched = 0;
        // lab4中暂时没有更多的线程，没有引入线程调度框架，而是直接先进先出的获取init_main线程进行调度
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last;
        do {
            if ((le = list_next(le)) != &proc_list) {
                next = le2proc(le, list_link);
                // 找到一个处于PROC_RUNNABLE就绪态的线程
                if (next->state == PROC_RUNNABLE) {
                    break;
                }
            }
        } while (le != last);
        if (next == NULL || next->state != PROC_RUNNABLE) {
            // 没有找到，则next指向idleproc线程
            next = idleproc;
        }
        // 找到的需要被调度的next线程runs自增
        next->runs++;
        if (next != current) {
            // next与current进行上下文切换，令next获得CPU资源
            proc_run(next);
        }
    }
    // 恢复中断
    local_intr_restore(intr_flag);
}
```

#### `proc_run`

设置任务状态段`tss`中特权态0下的栈顶指针`esp0`的目的是建立好内核线程或将来用户线程在执行特权态切换（从特权态0<-->特权态3，或从特权态3<-->特权态0）时能够正确定位处于特权态0时进程的内核栈的栈顶，而这个栈顶其实放了一个`trapframe`结构的内存空间。

```c
// proc.c
// proc_run - make process "proc" running on cpu
// NOTE: before call switch_to, should load base addr of "proc"'s new PDT
// 进行线程调度，令当前占有CPU的让出CPU，并令参数proc指向的线程获得CPU控制权
void proc_run(struct proc_struct *proc) {
    if (proc != current) {
        // 只有当proc不是当前执行的线程时，才需要执行
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;

        // 切换时新线程任务时需要暂时关闭中断，避免出现嵌套中断
        local_intr_save(intr_flag);
        {
            current = proc;
            // 设置TSS任务状态段的esp0的值，令其指向新线程的栈顶
            // ucore参考Linux的实现，不使用80386提供的TSS任务状态段这一硬件机制实现任务上下文切换，ucore在启动时初始化TSS后(init_gdt)，便不再对其进行修改。
            // 但进行中断等操作时，依然会用到当前TSS内的esp0属性。发生用户态到内核态中断切换时，硬件会将中断栈帧压入TSS.esp0指向的内核栈中
            // 因此ucore中的每个线程，需要有自己的内核栈，在进行线程调度切换时，也需要及时的修改esp0的值，使之指向新线程的内核栈顶。
            load_esp0(next->kstack + KSTACKSIZE);
            // 设置cr3寄存器的值，令其指向新线程的页表
            lcr3(next->cr3);
            // switch_to用于完整的进程上下文切换，定义在统一目录下的switch.S中
            // 由于涉及到大量的寄存器的存取操作，因此使用汇编实现
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag);
    }
}
```

#### `switch_to`

参数是前一个进程和后一个进程的执行现场。

此时的栈如下图。

```
 [esp]  ==> 返回地址
[esp+4] ==> from
[esp+8] ==> to
```

```c
// proc.c
void switch_to(struct context *from, struct context *to);

// switch.S
.text
.globl switch_to
switch_to:                      # switch_to(from, to)

    # save from registers
    # 令eax保存第一个参数from(context)的地址
    movl 4(%esp), %eax          # eax points to from
    # from.context 保存eip、esp等等寄存器的当前快照值
    popl 0(%eax)                # save eip !popl
    movl %esp, 4(%eax)          # save esp::context of from
    movl %ebx, 8(%eax)          # save ebx::context of from
    movl %ecx, 12(%eax)         # save ecx::context of from
    movl %edx, 16(%eax)         # save edx::context of from
    movl %esi, 20(%eax)         # save esi::context of from
    movl %edi, 24(%eax)         # save edi::context of from
    movl %ebp, 28(%eax)         # save ebp::context of from

    # restore to registers
    # 令eax保存第二个参数next(context)的地址,因为之前popl了一次，所以4(%esp)目前指向第二个参数
    movl 4(%esp), %eax          # not 8(%esp): popped return address already
                                # eax now points to to
    # 恢复next.context中的各个寄存器的值
    movl 28(%eax), %ebp         # restore ebp::context of to
    movl 24(%eax), %edi         # restore edi::context of to
    movl 20(%eax), %esi         # restore esi::context of to
    movl 16(%eax), %edx         # restore edx::context of to
    movl 12(%eax), %ecx         # restore ecx::context of to
    movl 8(%eax), %ebx          # restore ebx::context of to
    movl 4(%eax), %esp          # restore esp::context of to
    pushl 0(%eax)               # push eip

    # ret时栈上的eip为next(context)中设置的值(fork时，eip指向forkret,esp指向分配好的trap_frame)
    ret
```

- 在本实验的执行过程中，创建且运行了几个内核线程？

  两个，`idleproc`与`initproc`

- 语句`local_intr_save(intr_flag);....local_intr_restore(intr_flag);`在这里有何作用?请说明理由

  用于中断禁止和中断允许，保护进程切换不被中断，避免进程切换时其他进程进行调度。

### `initproc`生命周期

前面的过程有些凌乱了，下面捋一下`initproc`的整个过程。

- 通过`kernel_thread`函数，构造一个临时的`trap_frame`栈帧，其中设置了`CS`指向内核代码段选择子、`DS/ES/SS`等指向内核的数据段选择子。令中断栈帧中的`tf_regs.ebx`、`tf_regs.edx`保存函数`fn`和参数`arg`，`tf_eip`指向`kernel_thread_entry`。
- 通过`do_fork`分配一个未初始化的线程控制块`proc_struct`，设置并初始化其一系列状态。通过`copy_thread`中设置中断帧，设置线程上下文`struct context`中`eip`值为`forkret`，令上下文切换`switch`返回后跳转到`forkret`处，设置上下文中`esp`的值为内核栈的栈顶，此处存储的是中断帧。将`init_proc`加入`ucore`的就绪队列，等待`CPU`调度。
- ` idle_proc`在`cpu_idle`中触发`schedule`，将`init_proc`线程从就绪队列中取出，执行`switch_to`进行`idle_proc`和`init_proc`的`context`线程上下文的切换。
- `switch_to`返回时，`CPU`开始执行`init_proc`，跳转至`forkret`处。
- `fork_ret`中，进行中断返回。将之前存放在内核栈中的中断帧中的数据依次弹出，并调整栈顶为`tf_eip`，`iret`后跳转至`kernel_thread_entry`处。
- `kernel_thread_entry`中，利用之前在中断栈中设置好的`ebx(fn)`，`edx(arg)`执行真正的`init_proc`逻辑的处理(`init_main`函数)，在`init_main`返回后，跳转至`do_exit`终止退出。

### 为什么在`switch_to`上下文切换后，还需要进行一次中断返回？

**为什么在`init_proc`线程上下文切换时，不直接控制流跳转至`init_main`函数，而是绕了一个大弯，非要通过中断间接实现？**

这是因为`ucore`在`lab4`中需要为后续的用户态进程/线程的创建打好基础。由于目前我们所有的程序逻辑都是位于内核中的，拥有`ring0`的最高优先级，所以暂时感受不到通过中断间接切换线程上下文的好处。但是在后面引入用户态进程/线程概念后，这一机制将显得十分重要。

**当应用程序申请创建一个用户态进程时，需要`ucore`内核为其分配各种内核数据结构。由于特权级的限制，需要令应用程序通过一个调用门陷入内核(执行系统调用)，令其`CPL`特权级从`ring3`提升到`ring0`。但是当用户进程被初始化完毕后，进入调度执行状态后，为了内核的安全就不能允许用户进程继续处于内核态了，否则操作系统的安全性将得不到保障。而要令一个`ring0`的进程回到`ring3`的唯一方法便是使用中断返回机制，在用户进程/线程创建过程中“伪造”一个中断栈帧，令其中断返回到`ring3`的低特权级中，开始执行自己的业务逻辑。**

以上述`init_proc`的例子来说，如果`init_proc`不是一个内核线程，那么在构造临时的中断栈帧时，其`cs`、`ds/es/ss`等段选择子将指向用户态特权级的段选择子。这样中断返回时通过对栈上临时中断栈帧数据的弹出，进行各个寄存器的复原。当跳转至用户态线程入口时，应用程序已经进入`ring3`低特权级了。这样既实现了用户线程的创建，也使得应用程序无法随意的访问内核数据而破坏系统内核。

### `trapframe`与`context`

其实这段意思和上一段是一个意思。

结构体`trapframe`用于切换优先级、页表目录等，而`context`则是用于轻量级的上下文切换。从技术上来看，两者的区别在于`context`仅仅能够切换普通寄存器，而`trapframe`可以切换包括普通寄存器、段寄存器以及少量的控制寄存器。

有两个地方使用了`trapframe`，一个是中断调用，另一个是进程切换。两者对于`trapframe`的使用有相似之处，但也并不完全相同。

`trapframe`在中断中，在前期负责中断信息的储存，后期负责中断的恢复。同时，`trapframe`结构体是位于栈中的，其生成和使用都是通过栈的`push`、`pop`命令实现的。

```assembly
# trapentry.S
.text
.globl __alltraps
__alltraps:
    # push registers to build a trap frame
    # therefore make the stack look like a struct trapframe
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    pushal

    # load GD_KDATA into %ds and %es to set up data segments for kernel
    movl $GD_KDATA, %eax
    movw %ax, %ds
    movw %ax, %es

    # push %esp to pass a pointer to the trapframe as an argument to trap()
    pushl %esp

    # call trap(tf), where tf=%esp
    call trap

    # pop the pushed stack pointer
    popl %esp

    # return falls through to trapret...
.globl __trapret
__trapret:
    # restore registers from stack
    popal

    # restore %ds, %es, %fs and %gs
    popl %gs
    popl %fs
    popl %es
    popl %ds

    # get rid of the trap number and error code
    addl $0x8, %esp
    iret
```

注意在`call trap`之后，有一句`popl %esp`，事实上这里存储的应该就是中断帧`trapframe`，后续寄存器的恢复均是从`trapframe`中恢复。如果可以修改`eip`的值为其他的值，则可以实现向任意地址的跳转。（例如`lab1`标准答案中`challenge1`的实现）

`context`结构体干的事情也很简单，可以用`switch_to`函数囊括，即保存一系列寄存器，并恢复一系列寄存器。

那是不是进程的切换就可以直接用`switch_to`函数呢？答案是否定的，因为`switch_to`仅仅保存、恢复了普通寄存器，无法实现优先级跳转、段寄存器修改等等。这时，就要借助`trapframe`了。

由于`switch_to`函数跳转后，将调到`context.eip`位置。而这个跳转我们没法完全实现进程切换，所以我们可以将其设置为一个触发二级跳转的函数，`forkret`。

```c
proc->context.eip = (uintptr_t)forkret;
proc->context.esp = (uintptr_t)(proc->tf);
```

其中，`forkret`定义如下（current是当前进程，也就是进程切换的目标进程），`forkret`不同于`switch_to`，它尝试使用`trapframe`作为进程切换的手段，而相比于`context`，`trapframe`的功能就强大多了。

```assembly
.globl __trapret
__trapret:
    # restore registers from stack
    popal

    # restore %ds, %es, %fs and %gs
    popl %gs
    popl %fs
    popl %es
    popl %ds

    # get rid of the trap number and error code
    addl $0x8, %esp
    iret

.globl forkrets
forkrets:
    # set stack to this new process's trapframe
    movl 4(%esp), %esp
    jmp __trapret
```

`forkrets`代码与中断恢复的代码一模一样。最终跳转到`tf.ef_eip`。