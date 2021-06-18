<center><h3>实验五 用户进程管理</h3></center>
<div align='right'>学号： 1911463 姓名: 时浩铭 </div>
<div align='right'>学号： 1911477 姓名: 王耀群 </div>
<div align='right'>学号： 1911547 姓名: 李文婕 </div>

实验4实现了内核线程，仍只在内核中运行。

用户线程通常用于承载和运行应用程序，为了保护操作系统内核，避免其被不够鲁棒的应用程序破坏。应用程序都运行在低特权级中，无法直接访问高特权级的内核数据结构，也无法通过程序指令直接的访问各种外设。但应用程序访问高特权级数据、外设的需求是不可避免的(即使简单的打印数据到控制台中也是在对显卡这一外设进行控制)，因此在lab5中也实现了系统调用机制。应用程序平常运行在用户态，在有需要时可以通过系统调用的方式间接的访问外设等受到保护的资源。

本实验中创建用户进程，在用户态运行，并且可以通过系统调用让`ucore`为我们提供服务。

采用系统调用机制为用户进程提供一个获得操作系统服务的统一接口层，这样一来可简化用户进程的实现，把一些共性的、繁琐的、与硬件相关、与特权指令相关的任务放到操作系统层来实现，但提供一个简洁的接口给用户进程调用；二来这层接口事先可规定好，且严格检查用户进程传递进来的参数和操作系统要返回的数据，使得让操作系统给用户进程服务的同时，保护操作系统不会被用户进程破坏。

### `proc_struct`

进程退出时，其内核栈与进程控制块会由其父进程进行最后的回收工作，因此需要通知其父进程进行回收。增加维护父子、兄弟关系链表节点以及`exit_code`与`wait_status`。

```c
// proc.c

struct proc_struct {
    enum proc_state state;                      // Process state
    int pid;                                    // Process ID
    int runs;                                   // the running times of Proces
    uintptr_t kstack;                           // Process kernel stack
    volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
    struct proc_struct *parent;                 // the parent process
    struct mm_struct *mm;                       // Process's memory management field
    struct context context;                     // Switch here to run process
    struct trapframe *tf;                       // Trap frame for current interrupt
    uintptr_t cr3;                              // CR3 register: the base addr of Page Directroy Table(PDT)
    uint32_t flags;                             // Process flag
    char name[PROC_NAME_LEN + 1];               // Process name
    list_entry_t list_link;                     // Process link list 
    list_entry_t hash_link;                     // Process hash list
    
    // 子线程退出的原因
    int exit_code;                              // exit code (be sent to parent proc)
    // 陷入阻塞态的原因
    uint32_t wait_state;                        // waiting state
    // cptr 子线程，指的是最新的一个
    // yptr younger 上一个兄弟
    // optr older 下一个兄弟
    struct proc_struct *cptr, *yptr, *optr;     // relations between processes
};
```

### 练习0

#### `idt_init`

下面有

#### `trap_dispatch`

加入时间片轮转

```c
// trap.c 
static void trap_dispatch(struct trapframe *tf) {
    char c;
    int ret=0;
    switch (tf->tf_trapno) { 
    case IRQ_OFFSET + IRQ_TIMER:
        /* LAB5 YOUR CODE */
		/* you should upate you lab1 code (just add ONE or TWO lines of
         * code): Every TICK_NUM cycle, you should set current process's
         * current->need_resched = 1
         */
        ticks++;
        if (ticks % TICK_NUM == 0) {
            // print_ticks();
            assert(current != NULL);
            current->need_resched = 1;
        }
        break;
    }
}
```

#### `alloc_proc`

增加了对于新加入的变量的初始化。

```c
// proc.c
// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct* alloc_proc(void) {
    struct proc_struct* proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
    //LAB5 YOUR CODE : (update LAB4 steps)
    /*
     * below fields(add in LAB5) in proc_struct need to be initialized	
     *       uint32_t wait_state;                        // waiting state
     *       struct proc_struct *cptr, *yptr, *optr;     // relations between processes
	 */
        proc->wait_state = 0;
        proc->cptr = proc->optr = proc->yptr = NULL;
    }
```

#### `do_fork`

```c
// proc.c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe* tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct* proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    // LAB5 YOUR CODE : (update LAB4 steps)
    /* Some Functions
     *    set_links:  set the relation links of process.  ALSO SEE:
     * remove_links:  lean the relation links of process
     *    -------------------
     *    update step 1: set child proc's parent to current process, make sure
     * current process's wait_state is 0
     * update step 5: insert proc_struct into
     * hash_list && proc_list, set the relation links of process
     */
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }
    proc->parent = current;
    //确保当前进程不再等待
    assert(current->wait_state == 0);
    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }
    copy_thread(proc, stack, tf);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        // 换用set_links
        // nr_process++;
        // list_add(&proc_list, &(proc->list_link));
        set_links(proc);
    }
    local_intr_restore(intr_flag);

    wakeup_proc(proc);
    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```

```c
//proc.c 
// 主要是同时设置了几个新加入的变量
// set_links - set the relation links of process
static void set_links(struct proc_struct* proc) {
    list_add(&proc_list, &(proc->list_link));
    proc->yptr = NULL;
    // 设置自己的older以及older的younger为自己
    if ((proc->optr = proc->parent->cptr) != NULL) {
        proc->optr->yptr = proc;
    }
    proc->parent->cptr = proc;
    nr_process++;
}

// remove_links - clean the relation links of process
static void remove_links(struct proc_struct* proc) {
    list_del(&(proc->list_link));
    if (proc->optr != NULL) {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL) {
        proc->yptr->optr = proc->optr;
    } else {
        proc->parent->cptr = proc->optr;
    }
    nr_process--;
}
```

### 系统调用

系统调用是提供给运行在用户态的应用程序使用的，且由于需要进行`CPL`特权级的提升，因此是通过硬件中断来实现的。

当用户进程调用`INT T_SYSCALL`后，执行路径：

`vector128(vectors.S)->__alltraps(trapentry.S)-->trap(trap.c)-->trap_dispatch(trap.c)-->syscall(syscall.c)`

#### `idt_init`

```c
// unistd.h
#define T_SYSCALL           0x80
// trap.c 
// idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S
void idt_init(void) {
    extern uintptr_t __vectors[];
    for (int i = 0; i < 256; i++) {
        SETGATE(idt[i], 0, GD_KTEXT, __vectors[i], DPL_KERNEL);
    }
    // 增加了系统调用的中断描述符，DPL特权级为DPL_USER，因为是提供给用户进程的
    SETGATE(idt[T_SYSCALL], 0, GD_KTEXT, __vectors[T_SYSCALL], DPL_USER);
    lidt(&idt_pd);
}
```

#### `trap_dispatch`

```c
// trap.c 
static void trap_dispatch(struct trapframe *tf) {
    char c;
    int ret=0;
    switch (tf->tf_trapno) { 
    case T_SYSCALL:
        syscall();
        break; 
｝
```

相应的要增加对于`T_SYSCALL`的处理。

#### `syscall`

`ucore`利用`EAX`存储系统调用号，用`EDX/ECX/EBX/EDI/ESI`存储系统调用参数，返回结果保存在`EAX`之中。

##### 内核

```c
// kern/syscall/syscall.c
static int sys_exit(uint32_t arg[]) {
    int error_code = (int)arg[0];
    return do_exit(error_code);
}

static int sys_fork(uint32_t arg[]) {
    struct trapframe* tf = current->tf;
    uintptr_t stack = tf->tf_esp;
    return do_fork(0, stack, tf);
}

static int sys_wait(uint32_t arg[]) {
    int pid = (int)arg[0];
    int* store = (int*)arg[1];
    return do_wait(pid, store);
}

static int sys_exec(uint32_t arg[]) {
    const char* name = (const char*)arg[0];
    size_t len = (size_t)arg[1];
    unsigned char* binary = (unsigned char*)arg[2];
    size_t size = (size_t)arg[3];
    return do_execve(name, len, binary, size);
}

static int sys_yield(uint32_t arg[]) {
    return do_yield();
}

static int sys_kill(uint32_t arg[]) {
    int pid = (int)arg[0];
    return do_kill(pid);
}

static int sys_getpid(uint32_t arg[]) {
    return current->pid;
}

static int sys_putc(uint32_t arg[]) {
    int c = (int)arg[0];
    cputchar(c);
    return 0;
}

static int sys_pgdir(uint32_t arg[]) {
    print_pgdir();
    return 0;
}

// 构造一个函数指针数组，是所有的系统调用函数
// 数组里面的[]我也没看懂
static int (*syscalls[])(uint32_t arg[]) = {
    [SYS_exit]              sys_exit,
    [SYS_fork]              sys_fork,
    [SYS_wait]              sys_wait,
    [SYS_exec]              sys_exec,
    [SYS_yield]             sys_yield,
    [SYS_kill]              sys_kill,
    [SYS_getpid]            sys_getpid,
    [SYS_putc]              sys_putc,
    [SYS_pgdir]             sys_pgdir,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

void syscall(void) {
    struct trapframe* tf = current->tf;
    uint32_t arg[5];
    // 从eax中取系统调用号
    int num = tf->tf_regs.reg_eax;
    if (num >= 0 && num < NUM_SYSCALLS) {
        if (syscalls[num] != NULL) {
            // 取参数
            arg[0] = tf->tf_regs.reg_edx;
            arg[1] = tf->tf_regs.reg_ecx;
            arg[2] = tf->tf_regs.reg_ebx;
            arg[3] = tf->tf_regs.reg_edi;
            arg[4] = tf->tf_regs.reg_esi;
            // 执行，结果保存在eax里
            tf->tf_regs.reg_eax = syscalls[num](arg);
            return;
        }
    }
    print_trapframe(tf);
    panic("undefined syscall %d, pid = %d, name = %s.\n", num, current->pid,
          current->name);
}
```

##### 用户

在操作系统中初始化好系统调用相关的中断描述符、中断处理起始地址等后，还需在用户态的应用程序中初始化好相关工作，简化应用程序访问系统调用的复杂性。为此在用户态建立了一个中间层。

看一下用户是怎么调用系统调用的。

>C语言可变参数函数
>
>- 必须有一个强制参数
>- 可选参数类型可变，数量由强制参数的值确定
>- 获取可变参数，需要借助`va_list` 对象。
>- 宏`va_start`使用第一个可选参数的位置来初始化参数指针，第二个参数是该函数最后一个有名称参数的名称。必须先调用该宏，才可以开始使用可选参数。
>- 展开宏 `va_arg` 会得到当前可选参数，并把指针移动到下一个。
>- 当不再需要使用参数指针时，必须调用宏 `va_end`。
>
>一个例子
>
>```c
>double add( int n, ... )
>{
>  int i = 0;
>  double sum = 0.0;
>  va_list argptr;
>  va_start( argptr, n );             // 初始化argptr
>  for ( i = 0; i < n; ++i )       // 对每个可选参数，读取类型为double的参数，
>    sum += va_arg( argptr, double ); // 然后累加到sum中
>  va_end( argptr );
>  return sum;
>}
>```

```c
// user/libs/syscall.c
#define MAX_ARGS            5

static inline int syscall(int num, ...) {
    // 这里num显然是系统调用号，不是参数数量，下面选择直接读5个参数
    // 得到参数
    va_list ap;
    va_start(ap, num);
    uint32_t a[MAX_ARGS];
    int i, ret;
    for (i = 0; i < MAX_ARGS; i ++) {
        a[i] = va_arg(ap, uint32_t);
    }
    va_end(ap);

    asm volatile (
        "int %1;"
        : "=a" (ret)		// 返回值保存在eax
        : "i" (T_SYSCALL), 	// 调用INT 0x80调用系统调用中断
          "a" (num),		// 系统调用中断号给eax
          "d" (a[0]),		// 参数对应赋值
          "c" (a[1]),
          "b" (a[2]),
          "D" (a[3]),
          "S" (a[4])
        : "cc", "memory");
    return ret;
}

int sys_exit(int error_code) {
    return syscall(SYS_exit, error_code);
}

int sys_fork(void) {
    return syscall(SYS_fork);
}

int sys_wait(int pid, int* store) {
    return syscall(SYS_wait, pid, store);
}

int sys_yield(void) {
    return syscall(SYS_yield);
}

int sys_kill(int pid) {
    return syscall(SYS_kill, pid);
}

int sys_getpid(void) {
    return syscall(SYS_getpid);
}

int sys_putc(int c) {
    return syscall(SYS_putc, c);
}

int sys_pgdir(void) {
    return syscall(SYS_pgdir);
}
```

```c
// 一个类似标准C库的实现
// ulib.c
#include <defs.h>
#include <stdio.h>
#include <syscall.h>
#include <ulib.h>

void exit(int error_code) {
    sys_exit(error_code);
    cprintf("BUG: exit failed.\n");
    while (1)
        ;
}

int fork(void) {
    return sys_fork();
}

int wait(void) {
    return sys_wait(0, NULL);
}

int waitpid(int pid, int* store) {
    return sys_wait(pid, store);
}

void yield(void) {
    sys_yield();
}

int kill(int pid) {
    return sys_kill(pid);
}

int getpid(void) {
    return sys_getpid();
}

// print_pgdir - print the PDT&PT
void print_pgdir(void) {
    sys_pgdir();
}

```

与用户态的函数库调用执行过程相比，系统调用执行过程的有四点主要的不同：

- 不是通过`CALL`指令而是通过`INT`指令发起调用；
- 不是通过`RET`指令，而是通过`IRET`指令完成调用返回；
- 当到达内核态后，操作系统需要严格检查系统调用传递的参数，确保不破坏整个系统的安全性；
- 执行系统调用可导致进程等待某事件发生，从而可引起进程切换；

如果包括系统调用在内的中断发生时，会在中断栈帧中压入中断发生前一刻的`CS`的值。如果是位于用户态的应用程序发起的系统调用中断，那么内核在接受中断栈帧时其`CS`的`CPL`将会是3，并在执行中断服务例程时被临时的设置`CS`的`CPL`特权级为0以提升特权级，获得访问内核数据、外设的权限。在系统调用处理完毕返回后，`iret`指令会将之前`CPU`硬件自动压入的`cs(ring3的CPL)`弹出。系统调用处理完毕中断返回时，应用程序便自动回到了`ring3`这一低特权级中。

### 练习1：加载应用程序并执行

#### 应用程序的链接与虚拟地址空间

在`make`的最后一步执行了一个`ld`命令，把`hello`应用程序的执行码`obj/__user_hello.out`连接在了`ucore kernel`的末尾。且`ld`命令会在`kernel`中会把`__user_hello.out`的位置和大小记录在全局变量`_binary_obj___user_hello_out_start`和`_binary_obj___user_hello_out_size`中，这样这个`hello`用户程序就能够和`ucore`内核一起被 `bootloader` 加载到内存里中，并且通过这两个全局变量定位`hello`用户程序执行码的起始位置和大小。而到了与文件系统相关的实验后，`ucore`会提供一个简单的文件系统，那时所有的用户程序就都不再用这种方法进行加载了，而可以用大家熟悉的文件方式进行加载了。

在`tools/user.ld`描述了用户程序的用户虚拟空间的执行入口虚拟地址：

```
SECTIONS {
    /* Load programs at this address: "." means the current address */
    . = 0x800020;
```

这样`ucore`把用户进程的虚拟地址空间分了两块，一块与内核线程一样，是所有用户进程都共享的内核虚拟地址空间，映射到同样的物理内存空间中，这样在物理内存中只需放置一份内核代码，使得用户进程从用户态进入核心态时，内核代码可以统一应对不同的内核程序；另外一块是用户虚拟地址空间，虽然虚拟地址范围一样，但映射到不同且没有交集的物理内存空间中。这样当`ucore`把用户进程的执行代码（即应用程序的执行代码）和数据（即应用程序的全局变量等）放到用户虚拟地址空间中时，确保了各个进程不会“非法”访问到其他进程的物理内存空间。

```c
/* *
 * Virtual memory map:                                          Permissions
 *                                                              kernel/user
 *
 *     4G ------------------> +---------------------------------+
 *                            |                                 |
 *                            |         Empty Memory (*)        |
 *                            |                                 |
 *                            +---------------------------------+ 0xFB000000
 *                            |   Cur. Page Table (Kern, RW)    | RW/-- PTSIZE
 *     VPT -----------------> +---------------------------------+ 0xFAC00000
 *                            |        Invalid Memory (*)       | --/--
 *     KERNTOP -------------> +---------------------------------+ 0xF8000000
 *                            |                                 |
 *                            |    Remapped Physical Memory     | RW/-- KMEMSIZE
 *                            |                                 |
 *     KERNBASE ------------> +---------------------------------+ 0xC0000000
 *                            |        Invalid Memory (*)       | --/--
 *     USERTOP -------------> +---------------------------------+ 0xB0000000
 *                            |           User stack            |
 *                            +---------------------------------+
 *                            |                                 |
 *                            :                                 :
 *                            |         ~~~~~~~~~~~~~~~~        |
 *                            :                                 :
 *                            |                                 |
 *                            ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                            |       User Program & Heap       |
 *     UTEXT ---------------> +---------------------------------+ 0x00800000
 *                            |        Invalid Memory (*)       | --/--
 *                            |  - - - - - - - - - - - - - - -  |
 *                            |    User STAB Data (optional)    |
 *     USERBASE, USTAB------> +---------------------------------+ 0x00200000
 *                            |        Invalid Memory (*)       | --/--
 *     0 -------------------> +---------------------------------+ 0x00000000
 * (*) Note: The kernel ensures that "Invalid Memory" is *never* mapped.
 *     "Empty Memory" is normally unmapped, but user programs may map pages
 *     there if desired.
 *
 * */
```

#### `init_main`

lab4内，内核创建了两个线程`idle_proc`和`init_proc`，其中后者执行`init_main`，只打印`hello world`然后退出。

lab5中，`init_main`需要创建`user_main`内核线程。

```c
// proc.c
// init_main - the second kernel thread used to create user_main kernel threads
static int init_main(void* arg) {
    size_t nr_free_pages_store = nr_free_pages();
    size_t kernel_allocated_store = kallocated();
	// 创建user_main
    int pid = kernel_thread(user_main, NULL, 0);
    if (pid <= 0) {
        panic("create user_main failed.\n");
    }
	// 等待回收僵尸态子线程， 第一个参数为0表示回收任意子线程
    while (do_wait(0, NULL) == 0) {
        // 回收一个以后就调度
        schedule();
    }
	// 循环结束表明所有子线程都回收了
    cprintf("all user-mode processes have quit.\n");
    assert(initproc->cptr == NULL && initproc->yptr == NULL &&
           initproc->optr == NULL);
    assert(nr_process == 2);
    assert(list_next(&proc_list) == &(initproc->list_link));
    assert(list_prev(&proc_list) == &(initproc->list_link));

    cprintf("init check memory pass.\n");
    return 0;
}
```

#### `user_main`

最终调用`kernel_execve`并调用`SYS_exec`系统调用。

两个全局变量`_binary_obj___user_##x##_out_start`与`_binary_obj___user_##x##_out_size`是 链接是定义的，前者为执行码的起始位置，后者为大小。

```c
// proc.c
// user_main - kernel thread used to exec a user program
static int user_main(void* arg) {
#ifdef TEST
    KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    KERNEL_EXECVE(exit);
#endif
    panic("user_main execve failed.\n");
}

// kernel_execve - do SYS_exec syscall to exec a user program called by
// user_main kernel_thread
static int kernel_execve(const char* name, unsigned char* binary, size_t size) {
    int ret, len = strlen(name);
    asm volatile("int %1;"
                 : "=a"(ret)
                 : "i"(T_SYSCALL), "0"(SYS_exec), "d"(name), "c"(len),
                   "b"(binary), "D"(size)
                 : "memory");
    return ret;
}

#define __KERNEL_EXECVE(name, binary, size)                                \
    ({                                                                     \
        cprintf("kernel_execve: pid = %d, name = \"%s\".\n", current->pid, \
                name);                                                     \
        kernel_execve(name, binary, (size_t)(size));                       \
    })

#define KERNEL_EXECVE(x)                                           \
    ({                                                             \
        extern unsigned char _binary_obj___user_##x##_out_start[], \
            _binary_obj___user_##x##_out_size[];                   \
        __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,    \
                        _binary_obj___user_##x##_out_size);        \
    })

#define __KERNEL_EXECVE2(x, xstart, xsize)          \
    ({                                              \
        extern unsigned char xstart[], xsize[];     \
        __KERNEL_EXECVE(#x, xstart, (size_t)xsize); \
    })

#define KERNEL_EXECVE2(x, xstart, xsize) __KERNEL_EXECVE2(x, xstart, xsize)
```

#### `sys_exec`

通过系统调用，最终会执行到处理函数`sys_exec`。

```c
// kern/syscall/syscall.c
static int sys_exec(uint32_t arg[]) {
    const char* name = (const char*)arg[0];
    size_t len = (size_t)arg[1];
    unsigned char* binary = (unsigned char*)arg[2];
    size_t size = (size_t)arg[3];
    return do_execve(name, len, binary, size);
}
```

取出系统统调用传入的参数，然后调用`do_execve`。

#### `do_execve`

这个函数会把当前线程掏空换成要加载的二进制程序。

此函数的主要工作流程如下：

- 首先为加载新的执行码做好用户态内存空间清空准备。如果`mm`不为`NULL`，则设置页表为内核空间页表，且进一步判断`mm`的引用计数减1后是否为0，如果为0，则表明没有进程再需要此进程所占用的内存空间，为此将根据`mm`中的记录，释放进程所占用户空间内存和进程页表本身所占空间。最后把当前进程的`mm`内存管理指针为空。由于此处的`initproc`是内核线程，所以`mm`为`NULL`，整个处理都不会做。
- 接下来的一步是加载应用程序执行码到当前进程的新创建的用户态虚拟空间中。这里涉及到读`ELF`格式的文件，申请内存空间，建立用户态虚存空间，加载应用程序执行码等。`load_icode`函数完成了整个复杂的工作。

```c
// proc.c
// do_execve - call exit_mmap(mm)&put_pgdir(mm) to reclaim memory space of
// current process
//           - call load_icode to setup new memory space accroding binary prog.
int do_execve(const char* name,
              size_t len,
              unsigned char* binary,
              size_t size) {
    struct mm_struct* mm = current->mm;
    if (!user_mem_check(mm, (uintptr_t)name, len, 0)) {
        return -E_INVAL;
    }
    if (len > PROC_NAME_LEN) {
        len = PROC_NAME_LEN;
    }

    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    memcpy(local_name, name, len);

    if (mm != NULL) {
        // 设置页表为内核页表
        lcr3(boot_cr3);
        // mm_count减1，如果减之后为0了，释放内存空间
        if (mm_count_dec(mm) == 0) {
            // 删除用户进程占用的内存空间（包括页表）
            exit_mmap(mm);
            // 释放页目录表占的空间
            put_pgdir(mm);
            // 释放mm以及vma结构占的空间（先释放vma再释放mm）（之前有不再贴了）
            mm_destroy(mm);
        }
        // 都释放完了设置为NULL
        current->mm = NULL;
    }
    int ret;
    // 加载二进制文件
    if ((ret = load_icode(binary, size)) != 0) {
        goto execve_exit;
    }
    // 设置进程名
    set_proc_name(current, local_name);
    return 0;

execve_exit:
    // 这个后面再说
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}
```

##### `user_mem_check`

搜索`vma`链表，检查是否是一个合法的空间范围

```c
// vmm.c
bool user_mem_check(struct mm_struct* mm,
                    uintptr_t addr,
                    size_t len,
                    bool write) {
    if (mm != NULL) {
        // 检查这个程序的地址范围是不是在用户的地址空间内
        if (!USER_ACCESS(addr, addr + len)) {
            return 0;
        }
        struct vma_struct* vma;
        uintptr_t start = addr, end = addr + len;
        while (start < end) {
            if ((vma = find_vma(mm, start)) == NULL || start < vma->vm_start) {
                return 0;
            }
            // 执行到此处时，说明start在一个vma中
            if (!(vma->vm_flags & ((write) ? VM_WRITE : VM_READ))) {
                return 0;
            }
            // 执行到此处时，说明这个vma的读写权限是正确的
            if (write && (vma->vm_flags & VM_STACK)) {
                if (start < vma->vm_start + PGSIZE) {  // check stack start & size
                    return 0;
                }
            }
            // 这里暂时没看懂
            // 检查下一个vma
            start = vma->vm_end;
        }
        // 都没问题最后返回1
        return 1;
    }
    // 为空检查下是不是在内核的地址空间
    return KERN_ACCESS(addr, addr + len);
}

// memlayout.h
#define USERTOP             0xB0000000
#define USTACKTOP           USERTOP
#define USTACKPAGE          256                         // # of pages in user stack
#define USTACKSIZE          (USTACKPAGE * PGSIZE)       // sizeof user stack

#define USERBASE            0x00200000
#define UTEXT               0x00800000                  // where user programs generally begin
#define USTAB               USERBASE                    // the location of the user STABS data structure

#define USER_ACCESS(start, end)                     \
(USERBASE <= (start) && (start) < (end) && (end) <= USERTOP)

#define KERN_ACCESS(start, end)                     \
(KERNBASE <= (start) && (start) < (end) && (end) <= KERNTOP)
```

##### `mm_count_dec`

给`mm`的引用次数-1并返回减之后的值

```c
// vmm.h
static inline int mm_count_dec(struct mm_struct *mm) {
    mm->mm_count -= 1;
    return mm->mm_count;
}
```

##### `exit_mmap`

删除用户进程占用的内存空间

```c
// vmm.c
void exit_mmap(struct mm_struct* mm) {
    assert(mm != NULL && mm_count(mm) == 0);
    pde_t* pgdir = mm->pgdir;
    list_entry_t *list = &(mm->mmap_list), *le = list;
    // 释放对应的页及清空页表项
    while ((le = list_next(le)) != list) {
        struct vma_struct* vma = le2vma(le, list_link);
        unmap_range(pgdir, vma->vm_start, vma->vm_end);
    }
    // 释放可以释放的页表项
    while ((le = list_next(le)) != list) {
        struct vma_struct* vma = le2vma(le, list_link);
        exit_range(pgdir, vma->vm_start, vma->vm_end);
    }
}

// vmm.h
static inline int mm_count(struct mm_struct *mm) {
    return mm->mm_count;
}

// pmm.c
void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end) {
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));
	// 循环释放对应的页
    do {
        pte_t *ptep = get_pte(pgdir, start, 0);
        if (ptep == NULL) {
            // 这里直接加一个PTSIZE的原因是，get_pte的第三个参数是当对应页表不存在时是否创建，为0表示不创建，此时如果访问传入这个页表映射的地址才会返回NULL，因此表明这个页表不存在，因此直接去下一个页表对应的地址找
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue ;
        }
        if (*ptep != 0) {
            // 释放对应页并清除页表项
            page_remove_pte(pgdir, start, ptep);
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
}

void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end) {
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));

    start = ROUNDDOWN(start, PTSIZE);
    do {
        int pde_idx = PDX(start);
        if (pgdir[pde_idx] & PTE_P) {
            // 清空页目录项并释放对应页表占据的空间
            free_page(pde2page(pgdir[pde_idx]));
            pgdir[pde_idx] = 0;
        }
        start += PTSIZE;
    } while (start != 0 && start < end);
}
```

##### `put_pgdir`

释放页目录表的空间

```c
// proc.c
// put_pgdir - free the memory space of PDT
static void put_pgdir(struct mm_struct* mm) {
    free_page(kva2page(mm->pgdir));
}
```

#### `load_icode`

`load_icode`函数的主要工作就是给用户进程建立一个能够让用户进程正常运行的用户环境。此函数有一百多行，完成了如下重要工作：

1. 调用`mm_create`函数来申请进程的内存管理数据结构`mm`所需内存空间，并对`mm`进行初始化；
2. 调用`setup_pgdir`来申请一个页目录表所需的一个页大小的内存空间，并把描述`ucore`内核虚空间映射的内核页目录表（`boot_pgdir`所指）的内容拷贝到此新目录表中，最后让`mm->pgdir`指向此页目录表，这就是进程新的页目录表了，且能够正确映射内核虚空间；
3. 根据应用程序执行码的起始位置来解析此`ELF`格式的执行程序，并调用`mm_map`函数根据`ELF`格式的执行程序说明的各个段（代码段、数据段、`BSS`段等）的起始位置和大小建立对应的`vma`结构，并把`vma`插入到`mm`结构中，从而表明了用户进程的合法用户态虚拟地址空间；
4. 调用根据执行程序各个段的大小分配物理内存空间，并根据执行程序各个段的起始位置确定虚拟地址，并在页表中建立好物理地址和虚拟地址的映射关系，然后把执行程序各个段的内容拷贝到相应的内核虚拟地址中，至此应用程序执行码和数据已经根据编译时设定地址放置到虚拟内存中了；
5. 需要给用户进程设置用户栈，为此调用`mm_mmap`函数建立用户栈的`vma`结构，明确用户栈的位置在用户虚空间的顶端，大小为256个页，即`1MB`，并分配一定数量的物理内存且建立好栈的虚地址<-->物理地址映射关系；
6. 至此,进程内的内存管理`vma`和`mm`数据结构已经建立完成，于是把`mm->pgdir`赋值到`cr3`寄存器中，即更新了用户进程的虚拟内存空间，此时的`initproc`已经被传入的二进制代码和数据覆盖，成为了第一个用户进程，但此时这个用户进程的执行现场还没建立好；
7. 先清空进程的中断帧，再重新设置进程的中断帧，使得在执行中断返回指令`iret`后，能够让`CPU`转到用户态特权级，并回到用户态内存空间，使用用户态的代码段、数据段和堆栈，且能够跳转到用户进程的第一条指令执行，并确保在用户态能够响应中断；

```c
// proc.c
/* load_icode - load the content of binary program(ELF format) as the new
 * content of current process
 * @binary:  the memory addr of the content of binary program
 * @size:  the size of the content of binary program
 */
static int load_icode(unsigned char* binary, size_t size) {
    if (current->mm != NULL) {
        panic("load_icode: current->mm must be empty.\n");
    }

    int ret = -E_NO_MEM;
    struct mm_struct *mm;
    //(1) create a new mm for current process
    // 为当前进程创建一个新的mm结构
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    //(2) create a new PDT, and mm->pgdir= kernel virtual addr of PDT
    // 为mm分配并设置一个新的页目录表
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }
    //(3) copy TEXT/DATA section, build BSS parts in binary to memory space of process
    // 从进程的二进制数据空间中分配内存，复制出对应的代码/数据段，建立BSS部分
    struct Page *page;
    //(3.1) get the file header of the binary program (ELF format)
    // 从二进制程序中得到ELF格式的文件头(二进制程序数据的最开头的一部分是elf文件头,以elfhdr指针的形式将其映射、提取出来)
    struct elfhdr *elf = (struct elfhdr *)binary;
    //(3.2) get the entry of the program section headers of the bianry program (ELF format)
    // 找到并映射出binary中程序段头的入口起始位置
    struct proghdr *ph = (struct proghdr *)(binary + elf->e_phoff);
    //(3.3) This program is valid?
    // 根据elf的magic，判断其是否是一个合法的ELF文件
    if (elf->e_magic != ELF_MAGIC) {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }

    uint32_t vm_flags, perm;
    // 找到并映射出binary中程序段头的入口截止位置(根据elf->e_phnum进行对应的指针偏移)
    struct proghdr *ph_end = ph + elf->e_phnum;
    // 遍历每一个程序段头
    for (; ph < ph_end; ph ++) {
    //(3.4) find every program section headers
        if (ph->p_type != ELF_PT_LOAD) {
            // 如果不是需要加载的段，直接跳过
            continue ;
        }
        if (ph->p_filesz > ph->p_memsz) {
            // 如果文件头标明的文件段大小大于所占用的内存大小(memsz可能包括了BSS，所以这是错误的程序段头)
            // 就要回收之前分配的物理页及清空页表项
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        if (ph->p_filesz == 0) {
            // 文件段大小为0，直接跳过
            continue ;
        }
        //(3.5) call mm_map fun to setup the new vma ( ph->p_va, ph->p_memsz)
        // vm_flags => VMA段的权限
        // perm => 对应物理页的权限(因为是用户程序，所以设置为PTE_U用户态)
        vm_flags = 0, perm = PTE_U;
        // 根据文件头中的配置，设置VMA段的权限
        if (ph->p_flags & ELF_PF_X) 
            vm_flags |= VM_EXEC;
        if (ph->p_flags & ELF_PF_W) 
            vm_flags |= VM_WRITE;
        if (ph->p_flags & ELF_PF_R) 
            vm_flags |= VM_READ;
        // 设置程序段所包含的物理页的权限
        if (vm_flags & VM_WRITE) 
            perm |= PTE_W;
        // 在mm中建立ph->p_va到ph->va+ph->p_memsz的合法虚拟地址空间段
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
            goto bad_cleanup_mmap;
        }
        unsigned char *from = binary + ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);

        ret = -E_NO_MEM;

        //(3.6) alloc memory, and copy the contents of every program section (from, from+end) to process's memory (la, la+end)（这两个区间有点问题）
        end = ph->p_va + ph->p_filesz;
        //(3.6.1) copy TEXT/DATA section of bianry program
        // 上面建立了合法的虚拟地址段，现在为这个虚拟地址段分配实际的物理内存页
        while (start < end) {
            // 分配一个内存页，建立la对应页的虚实映射关系
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            // 根据elf中程序头的设置，将binary中的对应数据复制到新分配的物理页中
            memcpy(page2kva(page) + off, from, size);
            start += size, from += size;
        }

        //(3.6.2) build BSS section of binary program
        // 设置当前程序段的BSS段
        end = ph->p_va + ph->p_memsz;
        // start < la 最后一个物理页没有被填满。剩下空间作为BSS段
        if (start < la) {
            /* ph->p_memsz == ph->p_filesz */
            if (start == end) {
                continue ;
            }
            off = start + PGSIZE - la, size = PGSIZE - off;
            if (end < la) {
                size -= la - end;
            }
            // 将BSS段所属的部分格式化清零
            memset(page2kva(page) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
        }
        // start < end代表还需要为BSS段分配更多的物理空间
        while (start < end) {
            // 为BSS段分配更多的物理页
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            // 将BSS段所属的部分格式化清零
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }
    //(4) build user stack memory
    // 建立用户栈空间
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    // 为用户栈设置对应的合法虚拟内存空间
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    // 这里为什么只分配了4个页啊
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-2*PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-3*PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-4*PGSIZE , PTE_USER) != NULL);
    
    //(5) set current process's mm, sr3, and set CR3 reg = physical addr of Page Directory（sr3又是啥。。。）
    // 当前mm被线程引用次数+1
    mm_count_inc(mm);
    // 设置当前线程的mm
    current->mm = mm;
    // 设置当前线程的cr3
    current->cr3 = PADDR(mm->pgdir);
    // 将指定的页表地址mm->pgdir，加载进cr3寄存器
    lcr3(PADDR(mm->pgdir));

    //(6) setup trapframe for user environment
    // 设置用户环境下的中断栈帧
    struct trapframe *tf = current->tf;
    memset(tf, 0, sizeof(struct trapframe));
    /* LAB5:EXERCISE1 YOUR CODE
     * should set tf_cs,tf_ds,tf_es,tf_ss,tf_esp,tf_eip,tf_eflags
     * NOTICE: If we set trapframe correctly, then the user level process can return to USER MODE from kernel. So
     *          tf_cs should be USER_CS segment (see memlayout.h)
     *          tf_ds=tf_es=tf_ss should be USER_DS segment
     *          tf_esp should be the top addr of user stack (USTACKTOP)
     *          tf_eip should be the entry point of this binary program (elf->e_entry)
     *          tf_eflags should be set to enable computer to produce Interrupt
     */
    // 为了令内核态完成加载的应用程序能够在加载流程完毕后顺利的回到用户态运行，需要对当前的中断栈帧进行对应的设置
    // CS段设置为用户态段
    tf->tf_cs = USER_CS;
    // DS、ES、SS段设置为用户态的段
    tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS; 
    // 设置用户态的栈顶指针
    tf->tf_esp = USTACKTOP;
    // 设置系统调用中断返回后执行的程序入口，令其为elf头中设置的e_entry(中断返回后会复原中断栈帧中的eip)
    tf->tf_eip = elf->e_entry;
    // 默认中断返回后，用户态执行时是开中断的
    tf->tf_eflags = FL_IF;
    ret = 0;
out:
    return ret;
bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    goto out;
}
```

##### `setup_pgdir`

分配一个新的页作为页目录，并把内核页目录表拷贝过来。

```c
// proc.c
// setup_pgdir - alloc one page as PDT
static int setup_pgdir(struct mm_struct* mm) {
    struct Page* page;
    if ((page = alloc_page()) == NULL) {
        return -E_NO_MEM;
    }
    pde_t* pgdir = page2kva(page);
    memcpy(pgdir, boot_pgdir, PGSIZE);
    // 设置自映射
    pgdir[PDX(VPT)] = PADDR(pgdir) | PTE_P | PTE_W;
    // 设置mm的pgdir
    mm->pgdir = pgdir;
    return 0;
}
```

##### `proghdr`

```c
// elf.h
/* program section header */
struct proghdr {
    uint32_t p_type;   // loadable code or data, dynamic linking info,etc.
    uint32_t p_offset; // file offset of segment
    uint32_t p_va;     // virtual address to map segment
    uint32_t p_pa;     // physical address, not used
    uint32_t p_filesz; // size of segment in file
    uint32_t p_memsz;  // size of segment in memory (bigger if contains bss）
    uint32_t p_flags;  // read/write/execute bits
    uint32_t p_align;  // required alignment, invariably hardware page size
};
```

##### `mm_map`

向`mm`中加入一段指定的`vma`

```c
// proc.c
int mm_map(struct mm_struct* mm,
           uintptr_t addr,
           size_t len,
           uint32_t vm_flags,
           struct vma_struct** vma_store) {
    uintptr_t start = ROUNDDOWN(addr, PGSIZE),
              end = ROUNDUP(addr + len, PGSIZE);
    if (!USER_ACCESS(start, end)) {
        return -E_INVAL;
    }

    assert(mm != NULL);

    int ret = -E_INVAL;

    struct vma_struct* vma;
    if ((vma = find_vma(mm, start)) != NULL && end > vma->vm_start) {
        goto out;
    }
    ret = -E_NO_MEM;

    if ((vma = vma_create(start, end, vm_flags)) == NULL) {
        goto out;
    }
    insert_vma_struct(mm, vma);
    if (vma_store != NULL) {
        *vma_store = vma;
    }
    ret = 0;

out:
    return ret;
}
```

#### 后续

`do_execve`执行完之后的工作就与上个实验后面差不多了。创建出来的线程当被调度器选中后，会再`forkret`中按照它的`trapframe`恢复其中的值，这样就完成了内核态到用户态的转变，此时`EIP`是`elf->e_entry`，是用户进程的入口点，会开始执行用户进程。

首先，“硬”构造出第一个进程（lab4中已有描述），它是后续所有进程的祖先；然后，在`proc_init`函数中，通过`alloc_proc`把当前`ucore`的执行环境转变成`idle`内核线程的执行现场；然后调用`kernel_thread`来创建第二个内核线程`init_main`，而`init_main`内核线程又创建了`user_main`内核线程。到此，内核线程创建完毕，应该开始用户进程的创建过程，这第一步实际上是通过`init_main`函数调用`kernel_thread`创建子进程，通过`do_execve`调用来把某一具体程序的执行内容放入内存。具体的放置方式是根据`ld`在此文件上的地址分配为基本原则，把程序的不同部分放到某进程的用户空间中，从而通过此进程来完成程序描述的任务。一旦执行了这一程序对应的进程，就会从内核态切换到用户态继续执行。

### 练习2：父进程复制自己的内存空间给子进程

> 按照真正的顺序来说，这也算加载应用程序的过程，但为了区分两个练习，就这样写了

创建子进程的函数`do_fork`在执行中将拷贝当前进程（即父进程）的用户内存地址空间中的合法内容到新进程中（子进程），完成内存资源的复制。具体是通过`copy_range`函数（位于`kern/mm/pmm.c`中）实现的，请补充`copy_range`的实现，确保能够正确执行。

#### `do_fork`

代码前面有，具体关注`copy_mm`

#### `copy_mm`

lab4中由于只有内核线程，`copy_mm`什么也没做。但lab5中，有了用户进程，所以`copy_mm`会把父进程的所有内容拷贝到子进程中。

`ucore`中并不是写时复制，而是根据`CLONE_VM`来判断是共享还是复制。

```c
// proc.c
// copy_mm - process "proc" duplicate OR share process "current"'s mm according
// clone_flags
//         - if clone_flags & CLONE_VM, then "share" ; else "duplicate"
static int copy_mm(uint32_t clone_flags, struct proc_struct* proc) {
    struct mm_struct *mm, *oldmm = current->mm;

    /* current is a kernel thread */
    if (oldmm == NULL) {
        return 0;
    }
    if (clone_flags & CLONE_VM) {
        // 共享就只需要指向同一个mm就可以了
        mm = oldmm;
        goto good_mm;
    }

    int ret = -E_NO_MEM;
    // 先创建一个mm
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    // 给其分配一个页目录表，内容为内核页目录表的拷贝
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }
	// 加锁保证只有这个进程在操作
    lock_mm(oldmm);
    // 复制
    { ret = dup_mmap(mm, oldmm); }
    unlock_mm(oldmm);

    if (ret != 0) {
        goto bad_dup_cleanup_mmap;
    }

good_mm:
    // 但要给其引用计数+1
    mm_count_inc(mm);
    // 并设置好proc中的相关属性
    proc->mm = mm;
    proc->cr3 = PADDR(mm->pgdir);
    return 0;
bad_dup_cleanup_mmap:
    exit_mmap(mm);
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
}
```

#### `dup_mmap`

复制一个`mm`到另一个`mm`，复制的内容有`vma`和对应的内存

```c
// vmm.c
int dup_mmap(struct mm_struct* to, struct mm_struct* from) {
    assert(to != NULL && from != NULL);
    list_entry_t *list = &(from->mmap_list), *le = list;
    while ((le = list_prev(le)) != list) {
        // 遍历from的vma链表，然后创建一个一样的vma块并加入到to的vma链表之中
        struct vma_struct *vma, *nvma;
        vma = le2vma(le, list_link);
        nvma = vma_create(vma->vm_start, vma->vm_end, vma->vm_flags);
        if (nvma == NULL) {
            return -E_NO_MEM;
        }

        insert_vma_struct(to, nvma);
        
		// 调用copy_range完成对应内存数据的拷贝
        bool share = 0;
        if (copy_range(to->pgdir, from->pgdir, vma->vm_start, vma->vm_end,
                       share) != 0) {
            return -E_NO_MEM;
        }
    }
    return 0;
}
```

#### `copy_range`

复制一个页表的内容到另一个页表，包括所有映射的内存的复制，同时要保证复制后的映射关系不变。

会调用`int page_insert(pde_t* pgdir, struct Page* page, uintptr_t la, uint32_t perm) `，这个函数是建立指定页与指定虚拟地址之间的映射关系。（忘记的话去看lab3）

```c
// pmm.c
/* copy_range - copy content of memory (start, end) of one process A to another process B
 * @to:    the addr of process B's Page Directory
 * @from:  the addr of process A's Page Directory
 * @share: flags to indicate to dup OR share. We just use dup method, so it
 * didn't be used.
 *
 * CALL GRAPH: copy_mm-->dup_mmap-->copy_range
 */
int copy_range(pde_t* to,
               pde_t* from,
               uintptr_t start,
               uintptr_t end,
               bool share) {
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));
    // copy content by page unit.
    do {
        // call get_pte to find process A's pte according to the addr start
        pte_t *ptep = get_pte(from, start, 0), *nptep;
        if (ptep == NULL) {
            // 这里与前面unmap一样
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue;
        }
        // call get_pte to find process B's pte according to the addr start. If
        // pte is NULL, just alloc a PT
        if (*ptep & PTE_P) {
            // 这里第三个参数为1就会创建对应的页表
            if ((nptep = get_pte(to, start, 1)) == NULL) {
                return -E_NO_MEM;
            }
            uint32_t perm = (*ptep & PTE_USER);
            // get page from ptep
            struct Page* page = pte2page(*ptep);
            // alloc a page for process B
            struct Page* npage = alloc_page();
            assert(page != NULL);
            assert(npage != NULL);
            int ret = 0;
            /* LAB5:EXERCISE2 YOUR CODE
             * replicate content of page to npage, build the map of phy addr of
             * nage with the linear addr start
             *
             * Some Useful MACROs and DEFINEs, you can use them in below
             * implementation. MACROs or Functions: 
             * page2kva(struct Page *page):
             * return the kernel vritual addr of memory which page managed (SEE
             * pmm.h) 
             * page_insert: build the map of phy addr of an Page with the
             * linear addr la 
             * memcpy: typical memory copy function
             *
             * (1) find src_kvaddr: the kernel virtual address of page
             * (2) find dst_kvaddr: the kernel virtual address of npage
             * (3) memory copy from src_kvaddr to dst_kvaddr, size is PGSIZE
             * (4) build the map of phy addr of nage with the linear addr start
             */
            // 剩下的工作就是拷贝对应页的内容，然后建立映射关系
            void* src_kvaddr = page2kva(page);
            void* dst_kvaddr = page2kva(npage);
            memcpy(dst_kvaddr, src_kvaddr, PGSIZE);
            ret = page_insert(to, npage, start, perm);
            assert(ret == 0);
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
    return 0;
}
```

### 练习3：阅读分析源代码，理解进程执行`fork/exec/wait/exit ` 的实现，以及系统调用的实现

下面以用户进程`exit.c`的执行为例，分析一下这几个系统调用的实现。

#### `exit.c`

```c
// exit.c
#include <stdio.h>
#include <ulib.h>

int magic = -0x10384;

int
main(void) {
    int pid, code;
    cprintf("I am the parent. Forking the child...\n");
    // 调用库中的fork执行do_fork系统调用创建一个子线程
    if ((pid = fork()) == 0) {
        // 子线程返回的pid为0
        cprintf("I am the child.\n");
        // 调用yield主动让出CPU
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        // 之后执行do_exit系统调用退出
        exit(magic);
    }
    else {
        // 父线程返回的pid为子线程的pid
        cprintf("I am parent, fork a child pid %d\n",pid);
    }
    assert(pid > 0);
    cprintf("I am the parent, waiting now..\n");
	// 调用do_wait系统调用，等待子线程退出并回收
    assert(waitpid(pid, &code) == 0 && code == magic);
    // 再次回收会失败
    assert(waitpid(pid, &code) != 0 && wait() != 0);
    cprintf("waitpid %d ok.\n", pid);

    cprintf("exit pass.\n");
    return 0;
}
```

`make qemu`，操作系统会执行至`exit.c`的退出，得到输出信息如下：

```
kernel_execve: pid = 2, name = "exit".
I am the parent. Forking the child...
I am parent, fork a child pid 3
I am the parent, waiting now..
I am the child.
waitpid 3 ok.
exit pass.
all user-mode processes have quit.
init check memory pass.
kernel panic at kern/process/proc.c:457:
    initproc exit.
```

#### `fork`

在`exit.c`中首先会输出`I am the parent. Forking the child...`然后会执行`fork`，从而调用系统调用`do_fork`完成子线程的创建。

```c
// kern/syscall/syscall.c
static int sys_fork(uint32_t arg[]) {
    struct trapframe *tf = current->tf;
    uintptr_t stack = tf->tf_esp;
    return do_fork(0, stack, tf);
}

// proc.c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe* tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct* proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    // 给子线程申请一个进程控制块
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }
    proc->parent = current;
    //确保当前进程不在等待
    assert(current->wait_state == 0);
    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }
    // clone_flags为0表示是复制
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }
    // 设置context与trapframe
    copy_thread(proc, stack, tf);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        // 换用set_links
        // nr_process++;
        // list_add(&proc_list, &(proc->list_link));
        set_links(proc);
    }
    local_intr_restore(intr_flag);

    wakeup_proc(proc);
    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```

##### 父子线程的返回值

显然父线程`fork`出子线程后也会继续执行完`do_fork`，下面代码中有返回值`ret = proc->pid`，`proc`是子线程的进程控制块，其`pid`即为子线程的`pid`，因此父线程返回值是子线程的`pid`。

而子线程创建时，传给`do_fork`的`tf`是`current->tf`，即父线程的中断帧，并且其中记录的`eip`为`exit.c`中调用`fork`后的下一条语句。父线程的中断帧作为参数又传给了`copy_thread`去进行复制。除复制外，还设置了`proc->tf->tf_regs.reg_eax = 0`, 这就导致当子线程被调度以后执行`forkret`，`eax`寄存器被覆盖为0，即导致返回值为0。而由于`eip`并未修改仍为父线程的`eip`,因此会返回到与父线程同样的语句，但由于返回值不同而导致执行不同的分支。

因此输出的那几行也就不难理解。

```c
// proc.c
// copy_thread - setup the trapframe on the  process's kernel stack top and
//             - setup the kernel entry point and stack of process
static void copy_thread(struct proc_struct* proc,
                        uintptr_t esp,
                        struct trapframe* tf) {
    proc->tf = (struct trapframe*)(proc->kstack + KSTACKSIZE) - 1;
    *(proc->tf) = *tf;
    proc->tf->tf_regs.reg_eax = 0;
    proc->tf->tf_esp = esp;
    proc->tf->tf_eflags |= FL_IF;

    proc->context.eip = (uintptr_t)forkret;
    proc->context.esp = (uintptr_t)(proc->tf);
}
```

#### `exit`

子线程结束后，会调用`exit`退出。最终执行函数`do_exit`完成系统调用。

前面也提到了，线程退出需要回收资源，并且不是所有资源都是可以由线程自行回收，如线程控制块及内核栈等的回收需要由父线程完成。

在`do_exit`函数中，当前线程会将自己的线程状态设置为僵尸态，并且尝试着唤醒可能在等待子线程退出而被阻塞的父线程。

如果当前退出的子线程还拥有着自己的子线程，那么还需要将其托管给内核的第一个线程`initproc`，令`initproc`代替被退出线程，成为这些子线程的父线程，以完成后续的子线程回收工作。

```c
// ulib.c
void exit(int error_code) {
    sys_exit(error_code);
    cprintf("BUG: exit failed.\n");
    while (1)
        ;
}

// proc.c
// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory
//   space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask
//   parent reclaim itself.
//   3. call scheduler to switch to other process
int do_exit(int error_code) {
    if (current == idleproc) {
        panic("idleproc exit.\n");
    }
    if (current == initproc) {
        panic("initproc exit.\n");
    }

    struct mm_struct* mm = current->mm;
    if (mm != NULL) {
        // 切换为内核页表
        lcr3(boot_cr3);
        // 给mm引用计数-1，如果减之后为0了，就需要回收空间
        if (mm_count_dec(mm) == 0) {
            // 回收内存及页表
            exit_mmap(mm);
            // 回收页目录表
            put_pgdir(mm);
            // 回收vma及mm
            mm_destroy(mm);
        }
        current->mm = NULL;
    }
    // 设置状态为僵尸态以及退出代码
    current->state = PROC_ZOMBIE;
    current->exit_code = error_code;
	
    bool intr_flag;
    struct proc_struct* proc;
    local_intr_save(intr_flag);
    {
        proc = current->parent;
        // 如果父线程在等待子线程则唤醒父线程令其回收
        if (proc->wait_state == WT_CHILD) {
            wakeup_proc(proc);
        }
        // 遍历这个线程的子线程链表
        while (current->cptr != NULL) {
            // 这三行相当于取走了最新的子线程并完成了循环控制变量(current->cptr的切换)
            proc = current->cptr;
            current->cptr = proc->optr;
            proc->yptr = NULL;
            // 把子线程加入了initproc的子线程链表
            if ((proc->optr = initproc->cptr) != NULL) {
                initproc->cptr->yptr = proc;
            }
            proc->parent = initproc;
            initproc->cptr = proc;
            // 如果还需要回收就交给initproc进行回收了
            if (proc->state == PROC_ZOMBIE) {
                if (initproc->wait_state == WT_CHILD) {
                    wakeup_proc(initproc);
                }
            }
        }
    }
    local_intr_restore(intr_flag);
	// 退出以后调度其他线程
    schedule();
    panic("do_exit will not return!! %d.\n", current->pid);
}

//sched.c
void wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE) {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
        }
        else {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(intr_flag);
}
```

#### `wait`

`exit.c`中，父线程通过`waitpid`等待子线程退出。`waitpid`与`wait`都会执行`sys_wait`系统调用。

```c
// ulib.c
int wait(void) {
    return sys_wait(0, NULL);
}

int waitpid(int pid, int* store) {
    return sys_wait(pid, store);
}

// proc.c
// do_wait - wait one OR any children with PROC_ZOMBIE state, and free memory
// space of kernel stack
// 令当前线程等待一个或多个子线程进入僵尸态，并且回收其内核栈和线程控制块
//         - proc struct of this child.
// NOTE: only after do_wait function, all resources of the child proces are
// free.
// 注意：只有在do_wait函数执行完成之后，子线程的所有资源才被完全释放
int do_wait(int pid, int* code_store) {
    struct mm_struct* mm = current->mm;
    // 检查传入的int指针处是否有一个int的合法空间
    if (code_store != NULL) {
        if (!user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1)) {
            return -E_INVAL;
        }
    }

    struct proc_struct* proc;
    bool intr_flag, haskid;
repeat:
    haskid = 0;
    if (pid != 0) {
        // 如果给定pid，表明回收pid对应的线程
        proc = find_proc(pid);
        if (proc != NULL && proc->parent == current) {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE) {
                // 检查确保没问题，进行回收
                goto found;
            }
        }
    } else {
        // 没指定的话，遍历子线程找一个可以回收的
        proc = current->cptr;
        for (; proc != NULL; proc = proc->optr) {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE) {
                goto found;
            }
        }
    }
    if (haskid) {
        // 执行到这里，表明没有找到可以回收的，主动让出CPU
        current->state = PROC_SLEEPING;
        // 修改自身等待原因为等待子线程
        current->wait_state = WT_CHILD;
        schedule();
        // PF_EXITING表明进程正在退出(被kill了)
        if (current->flags & PF_EXITING) {
            // 就退出
            do_exit(-E_KILLED);
        }
        // 没有被kill就再找一下
        goto repeat;
    }
    return -E_BAD_PROC;

found:
    if (proc == idleproc || proc == initproc) {
        // 不能回收idleproc/initproc
        panic("wait idleproc or initproc.\n");
    }
    if (code_store != NULL) {
        // 保存退出的代码
        *code_store = proc->exit_code;
    }
    local_intr_save(intr_flag);
    {
        // 从链表中取出这一proc
        unhash_proc(proc);
        remove_links(proc);
    }
    local_intr_restore(intr_flag);
    // 回收内核栈
    put_kstack(proc);
    // 回收进程控制块
    kfree(proc);
    return 0;
}


// unhash_proc - delete proc from proc hash_list
static void unhash_proc(struct proc_struct* proc) {
    list_del(&(proc->hash_link));
}
```

父线程回收子线程后会打印出`waitpid 3 ok.`和`exit pass.`。之后便`return 0`了，就回到了`kernel thread entry`处，就调用`call do_exit`退出了，就会唤醒父线程`init_main`，`init_main`回收之后就会打印`all user-mode processes have quit.`和`init check memory pass.`。之后`return 0`，调用`do_exit`结束系统。

```assembly
# entry.S
.text
.globl kernel_thread_entry
kernel_thread_entry:        # void kernel_thread(void)

    pushl %edx              # push arg
    call *%ebx              # call fn

    pushl %eax              # save the return value of fn(arg)
    call do_exit            # call do_exit to terminate current thread
```

> 请分析 fork/exec/wait/exit 在实现中是如何影响进程的执行状态的？

- fork 执行完毕后，如果创建新进程成功，则出现两个进程，一个是子进程，一个是父进程。在子进程中，fork 函数返回 0，在父进程中，fork 返回新创建子进程的进程 ID。我们可以通过 fork 返回的值来判断当前进程是子进程还是父进程。fork 不会影响当前进程的执行状态，但是会将子进程的状态标记为 RUNNALB，使得可以在后续的调度中运行起来；
- exec 完成用户进程的创建工作。首先为加载新的执行码做好用户态内存空间清空准备。接下来的一步是加载应用程序执行码到当前进程的新创建的用户态虚拟空间中。exec 不会影响当前进程的执行状态，但是会修改当前进程中执行的程序；
- wait 是等待任意子进程的结束通知。wait_pid 函数等待进程 id 号为 pid 的子进程结束通知。这两个函数最终访问 sys_wait 系统调用接口让 ucore 来完成对子进程的最后回收工作。wait 系统调用取决于是否存在可以释放资源（ZOMBIE）的子进程，如果有的话不会发生状态的改变，如果没有的话会将当前进程置为 SLEEPING 态，等待执行了 exit 的子进程将其唤醒；
- exit 会把一个退出码 error_code 传递给 ucore，ucore 通过执行内核函数 do_exit 来完成对当前进程的退出处理，主要工作简单地说就是回收当前进程所占的大部分内存资源，并通知父进程完成最后的回收工作。exit 会将当前进程的状态修改为 ZOMBIE 态，并且会将父进程唤醒（修改为RUNNABLE），然后主动让出 CPU 使用权；

> 请给出 ucore 中一个用户态进程的执行状态生命周期图（包执行状态，执行状态之间的变换关系，以及产生变换的事件或函数调用）。（字符方式画即可）

首先，我们梳理一下流程：

[![2cLmxH.png](https://z3.ax1x.com/2021/06/09/2cLmxH.png)](https://imgtu.com/i/2cLmxH)

最终，我们可以画出执行状态图如下所示：

[![2cLZGD.png](https://z3.ax1x.com/2021/06/09/2cLZGD.png)](https://imgtu.com/i/2cLZGD)

#### `yield`

```c
// proc.c
// do_yield - ask the scheduler to reschedule
int do_yield(void) {
    current->need_resched = 1;
    return 0;
}
```

#### `kill`

```c
// proc.c
// do_kill - kill process with pid by set this process's flags with PF_EXITING
int do_kill(int pid) {
    struct proc_struct* proc;
    if ((proc = find_proc(pid)) != NULL) {
        if (!(proc->flags & PF_EXITING)) {
            proc->flags |= PF_EXITING;
            if (proc->wait_state & WT_INTERRUPTED) {
                wakeup_proc(proc);
            }
            return 0;
        }
        return -E_KILLED;
    }
    return -E_INVAL;
}
```

设置标志`PF_EXITING`后，当有中断时，就会在`trap`中检测此标志，检测到了就调用`do_exit`使其退出。

```c
// trap.c
void trap(struct trapframe* tf) {
    // dispatch based on what type of trap occurred
    // used for previous projects
    if (current == NULL) {
        trap_dispatch(tf);
    } else {
        // keep a trapframe chain in stack
        struct trapframe* otf = current->tf;
        current->tf = tf;

        bool in_kernel = trap_in_kernel(tf);

        trap_dispatch(tf);

        current->tf = otf;
        if (!in_kernel) {
            if (current->flags & PF_EXITING) {
                do_exit(-E_KILLED);
            }
            if (current->need_resched) {
                schedule();
            }
        }
    }
}

```

