<center><h3>实验一 系统软件启动过程</h3></center>
<div align='right'>学号： 1911463 姓名: 时浩铭 </div>
<div align='right'>学号： 1911477 姓名: 王耀群 </div>
<div align='right'>学号： 1911547 姓名: 李文婕 </div>

### 练习1：理解通过`make`生成执行文件的过程。

- 操作系统镜像文件`ucore.img`是如何一步一步生成的?
- 一个被系统认为是符合规范的硬盘主引导扇区的特征是什么?

-----

#### `ucore.img`

- 首先使用`make V=`命令编译、连接并打印出命令。略去警告以后得到如下命令。

  ``` makefile
  + cc kern/init/init.c
  gcc -Ikern/init/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/init/init.c -o obj/kern/init/init.o
  + cc kern/libs/stdio.c
  gcc -Ikern/libs/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/libs/stdio.c -o obj/kern/libs/stdio.o
  + cc kern/libs/readline.c
  gcc -Ikern/libs/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/libs/readline.c -o obj/kern/libs/readline.o
  + cc kern/debug/panic.c
  gcc -Ikern/debug/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/debug/panic.c -o obj/kern/debug/panic.o
  + cc kern/debug/kdebug.c
  gcc -Ikern/debug/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/debug/kdebug.c -o obj/kern/debug/kdebug.o
  gcc -Ikern/debug/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/debug/kmonitor.c -o obj/kern/debug/kmonitor.o
  + cc kern/driver/clock.c
  gcc -Ikern/driver/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/driver/clock.c -o obj/kern/driver/clock.o
  + cc kern/driver/console.c
  gcc -Ikern/driver/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/driver/console.c -o obj/kern/driver/console.o
  + cc kern/driver/picirq.c
  gcc -Ikern/driver/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/driver/picirq.c -o obj/kern/driver/picirq.o
  + cc kern/driver/intr.c
  gcc -Ikern/driver/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/driver/intr.c -o obj/kern/driver/intr.o
  + cc kern/trap/trap.c
  gcc -Ikern/trap/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/trap/trap.c -o obj/kern/trap/trap.o
  gcc -Ikern/trap/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/trap/vectors.S -o obj/kern/trap/vectors.o
  + cc kern/trap/trapentry.S
  gcc -Ikern/trap/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/trap/trapentry.S -o obj/kern/trap/trapentry.o
  + cc kern/mm/pmm.c
  gcc -Ikern/mm/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/mm/pmm.c -o obj/kern/mm/pmm.o
  + cc libs/string.c
  gcc -Ilibs/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/  -c libs/string.c -o obj/libs/string.o
  + cc libs/printfmt.c
  gcc -Ilibs/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/  -c libs/printfmt.c -o obj/libs/printfmt.o
  + ld bin/kernel
  ld -m    elf_i386 -nostdlib -T tools/kernel.ld -o bin/kernel  obj/kern/init/init.o obj/kern/libs/stdio.o obj/kern/libs/readline.o obj/kern/debug/panic.o obj/kern/debug/kdebug.o obj/kern/debug/kmonitor.o obj/kern/driver/clock.o obj/kern/driver/console.o obj/kern/driver/picirq.o obj/kern/driver/intr.o obj/kern/trap/trap.o obj/kern/trap/vectors.o obj/kern/trap/trapentry.o obj/kern/mm/pmm.o  obj/libs/string.o obj/libs/printfmt.o
  + cc boot/bootasm.S
  gcc -Iboot/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Os -nostdinc -c boot/bootasm.S -o obj/boot/bootasm.o
  + cc boot/bootmain.c
  gcc -Iboot/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Os -nostdinc -c boot/bootmain.c -o obj/boot/bootmain.o
  + cc tools/sign.c
  gcc -Itools/ -g -Wall -O2 -c tools/sign.c -o obj/sign/tools/sign.o
  gcc -g -Wall -O2 obj/sign/tools/sign.o -o bin/sign
  + ld bin/bootblock
  ld -m    elf_i386 -nostdlib -N -e start -Ttext 0x7C00 obj/boot/bootasm.o obj/boot/bootmain.o -o obj/bootblock.o
  'obj/bootblock.out' size: 496 bytes
  build 512 bytes boot sector: 'bin/bootblock' success!
  dd if=/dev/zero of=bin/ucore.img count=10000
  ```
  
- 然后分析`makefile`。`ucore.img`产生位于`makefile`文件178-186行。其产生需要两个前置条件，`kernel`和`bootblock`

  ```makefile
  UCOREIMG	:= $(call totarget,ucore.img)
  
  $(UCOREIMG): $(kernel) $(bootblock)
  	$(V)dd if=/dev/zero of=$@ count=10000
  	$(V)dd if=$(bootblock) of=$@ conv=notrunc
  	$(V)dd if=$(kernel) of=$@ seek=1 conv=notrunc
  
  $(call create_target,ucore.img)
  ```

  - `$(call VARIABLE,PARAM,PARAM,...)`：`call`函数是唯一一个可以用来创建新的参数化的函数。可以在一个表达式中可以定义许多参数，然后通过`call`函数来向这个表达式传递参数。`call`函数执行时，将参数依次赋给临时变量`$(1)`、`$(2)`，最后在对`VARIABLE`展开后的表达式进行处理，返回表达式的值。

  - `dd`命令用于读取、转换并输出数据，可以从标准输入或文件中读取数据，根据指定的格式来转换数据，再输出到文件、设备或标准输出。

    > `if=name`，输入文件名，默认为标准输入。
    >
    > `of=name`，输出文件名，默认为标准输出。
    >
    > `ibs=bytes`，一次读入bytes个字节，即指定一个块大小为bytes个字节，默认大小512。
    >
    > `count=blocks`，仅拷贝blocks个块，块大小等于`ibs`指定的字节数。
    >
    > `seek=blocks`，从输入文件开头跳过blocks个块后再开始复制。
    >
    > `conv=notrunc`，不截断输出文件。

  - `$@`指当前目标，这段代码中指`UCOREIMG`。
  - 读代码可以明白，首先创建一个大小为10000个块的区域，然后把`bootblock`拷贝过去，再把`kernel`拷贝过去，拷贝在`bootblock`所在块的后面。

- 产生`bootblock`的代码位于`makefile`文件156-168行。

  ```makefile
  bootfiles = $(call listf_cc,boot)
  $(foreach f,$(bootfiles),$(call cc_compile,$(f),$(CC),$(CFLAGS) -Os -nostdinc))
  
  bootblock = $(call totarget,bootblock)
  
  $(bootblock): $(call toobj,$(bootfiles)) | $(call totarget,sign)
  	@echo + ld $@
  	$(V)$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 $^ -o $(call toobj,bootblock)
  	@$(OBJDUMP) -S $(call objfile,bootblock) > $(call asmfile,bootblock)
  	@$(OBJCOPY) -S -O binary $(call objfile,bootblock) $(call outfile,bootblock)
  	@$(call totarget,sign) $(call outfile,bootblock) $(bootblock)
  
  $(call create_target,bootblock)
  ```

  需要依赖`$(bootfiles)`和`sign`。

  ```shell
  + cc boot/bootasm.S
  gcc -Iboot/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Os -nostdinc -c boot/bootasm.S -o obj/boot/bootasm.o
  + cc boot/bootmain.c
  gcc -Iboot/ -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Os -nostdinc -c boot/bootmain.c -o obj/boot/bootmain.o
  + cc tools/sign.c
  gcc -Itools/ -g -Wall -O2 -c tools/sign.c -o obj/sign/tools/sign.o
  gcc -g -Wall -O2 obj/sign/tools/sign.o -o bin/sign
  + ld bin/bootblock
  ld -m    elf_i386 -nostdlib -N -e start -Ttext 0x7C00 obj/boot/bootasm.o obj/boot/bootmain.o -o obj/bootblock.o
  ```

  根据`make V=`命令打印出的结果，可以分析出为生成`bootblock`首先需要生成`bootasm.o`、`bootmain.o`和`sign`，其中前两者即为`$(bootfiles)`，由宏定义批量实现。

  ```makefile
  bootfiles = $(call listf_cc,boot)
  $(foreach f,$(bootfiles),$(call cc_compile,$(f),$(CC),$(CFLAGS) -Os -nostdinc))
  ```

  编译的具体命令为上面代码框中的第2行和第4行，其中一些参数的含义如下：

  > `-march`，指定进行优化的型号，此处是i686。
  >
  > `-fno-builtin`，不使用c语言的内建函数（函数重名时使用）。
  >
  > `-fno-PIC`，不生成与位置无关的代码(position independent code)。
  >
  > `-Wall`，编译后显示所有警告。
  >
  > `-ggdb`，为GDB生成更为丰富的调试信息。
  >
  > `-m32`，生成适用于32位系统的代码。
  >
  > `-gstabs`，以stabs格式生成调试信息，但不包括上一条的GDB调试信息。
  >
  > `-nostdinc`，不使用标准库。
  >
  > `-fno-stack-protector`，不生成用于检测缓冲区溢出的代码。
  >
  > `-Os`，为减小代码大小而进行优化。

  生成`sign`的代码如下。

  ```makefile
  $(call add_files_host,tools/sign.c,sign,sign)
  $(call create_target_host,sign,sign)
  ```

  具体命令见上面代码框第6行和第7行。

  三个依赖文件生成完后，就可以生成`bootblock`，具体命令如下。

  ```shell
  ld -m    elf_i386 -nostdlib -N -e start -Ttext 0x7C00 obj/boot/bootasm.o obj/boot/bootmain.o -o obj/bootblock.o
  ```

  其中`ld`是GNU的连接器，将目标文件连接为可执行文件。

  > `-m`，类march操作，模拟i386的链接器。
  >
  > `-nostdlib`，不使用标准库。
  >
  > `-N`，设置全读写权限。
  >
  > `-e`，指定程序的入口。
  >
  > `-Ttext`，指定代码段的开始位置。

- 生成`kernel`的代码位于`makefile`文件141-151行。

  ```makefile
  kernel = $(call totarget,kernel)
  
  $(kernel): tools/kernel.ld
  
  $(kernel): $(KOBJS)
  	@echo + ld $@
  	$(V)$(LD) $(LDFLAGS) -T tools/kernel.ld -o $@ $(KOBJS)
  	@$(OBJDUMP) -S $@ > $(call asmfile,kernel)
  	@$(OBJDUMP) -t $@ | $(SED) '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(call symfile,kernel)
  
  $(call create_target,kernel)
  ```

  `kernel`的依赖文件是一个集合`KOBJS`，而文件具体内容由`KSRCDIR`指定。

  具体命令为。

  ```shell
  ld -m    elf_i386 -nostdlib -T tools/kernel.ld -o bin/kernel  obj/kern/init/init.o obj/kern/libs/stdio.o obj/kern/libs/readline.o obj/kern/debug/panic.o obj/kern/debug/kdebug.o obj/kern/debug/kmonitor.o obj/kern/driver/clock.o obj/kern/driver/console.o obj/kern/driver/picirq.o obj/kern/driver/intr.o obj/kern/trap/trap.o obj/kern/trap/vectors.o obj/kern/trap/trapentry.o obj/kern/mm/pmm.o  obj/libs/string.o obj/libs/printfmt.o
  ```

  可以看出，生成`kernel`前，先要用`gcc`编译`kern`目录下所有的`.c`文件，再用连接器将他们连接起来。

- 综上所述，`ucore.img`的生成主要有以下几个步骤：
  - 编译所有生成`kerenel`所需的文件，并将他们连接从而生成`kernel`。
  - 编译`bootasm.S`、`bootmain.c`和`sign.c`，并根据`sign`生成`bootblock`
  - 创建一个大小为10000个块的`ucore.img`，每个块为512字节。将`bootblock`拷贝到第一个块，`kernel`拷贝到第二个块。

#### 符合规范的硬盘主引导扇区的特征

阅读`tools/sign.c`中相关代码。

```c++
char buf[512]; //定义buf数组
memset(buf, 0, sizeof(buf)); //初始化置0
buf[510] = 0x55; //把buf数组最后两位置为0x55AA
buf[511] = 0xAA;
FILE *ofp = fopen(argv[2], "wb+");
size = fwrite(buf, 1, 512, ofp);
if (size != 512) {
    fprintf(stderr, "write '%s' error, size is %d.\n", argv[2], size);
    return -1;
}
fclose(ofp);
printf("build 512 bytes boot sector: '%s' success!\n", argv[2]);
return 0;
```

因此可知特征有如下几点

- 磁盘主引导扇区大小为521字节。
- 磁盘主引导扇区最后两个字节为`0x55AA`
- [由不超过466字节的启动代码和不超过64字节的硬盘分区表加上两个字节的结束符组成](https://zh.wikipedia.org/wiki/%E4%B8%BB%E5%BC%95%E5%AF%BC%E8%AE%B0%E5%BD%95)，多余的空间为0。

-----

### 练习2：使用`qemu`执行并调试`lab1`中的软件。

#### 从CPU加电后执行的第一条指令开始，单步跟踪BIOS的执行

首先修改`lab1/tools/gdbinit`文件，这是`gdb`的配置文件。我们并没有用到`kernel`中的符号，因此不需要`file bin/kernel`，也不用在`kern_init`处打断点，修改文件为以下内容。

```text
set architecture i8086
target remote :1234
```

然后在`lab1`根目录执行`make debug`，会弹出`qemu`模拟器和`gdb`窗口。

在`gdb`内输入`x $pc`查看当前`$pc`的值，得到如下输出。

```asm
=> 0xfff0:      add    %al,(%eax)
```

再输入`x $cs`查看`CS`段寄存器的值。

```asm
0xf000:      add    %al,(%eax)
```

因此CPU要执行的第一条指令的地址为`CS:IP=0xffff0`，这条指令是一条长跳转指令，输入`x 0xffff0`查询。

```asm
0xffff0:     ljmp   $0x3630,$0xf000e05b
```

这也即为CPU加电后执行的第一条指令。

之后便可以输入`si`进行单步调试。

#### 在初始化位置0x7c00设置实地址断点,测试断点正常

再次修改`gdbinit`文件，加入以下指令。

```text
b *0x7c00   //在0x7c00处设置断点。此地址是bootloader入口点地址，boot/bootasm.S的start地址处
c     //continue简称，表示继续执行
```

再次`make debug`，发现有以下输出，说明断点正常。

```asm
Breakpoint 1 at 0x7c00

Breakpoint 1, 0x00007c00 in ?? ()
```

输入`x /10i $pc`查看pc后十条指令。

```asm
=> 0x7c00:      cli
   0x7c01:      cld
   0x7c02:      xor    %eax,%eax
   0x7c04:      mov    %eax,%ds
   0x7c06:      mov    %eax,%es
   0x7c08:      mov    %eax,%ss
   0x7c0a:      in     $0x64,%al
   0x7c0c:      test   $0x2,%al
   0x7c0e:      jne    0x7c0a
   0x7c10:      mov    $0xd1,%al
```

说明断点正常。

此时再次查看`CS`的值，得到如下输出：

```assembly
0x0:    0xf000ff53
```

说明目前执行的指令地址即为`eip`寄存器中的地址。

#### 从0x7c00开始跟踪代码运行,将单步跟踪反汇编得到的代码与`bootasm.S`和`bootblock.asm`进行比较

`bootasm.S`中部分代码如下：

```assembly
start:
.code16                                             # Assemble for 16-bit mode
    cli                                             # Disable interrupts
    cld                                             # String operations increment

    # Set up the important data segment registers (DS, ES, SS).
    xorw %ax, %ax                                   # Segment number zero
    movw %ax, %ds                                   # -> Data Segment
    movw %ax, %es                                   # -> Extra Segment
    movw %ax, %ss                                   # -> Stack Segment

    # Enable A20:
    #  For backwards compatibility with the earliest PCs, physical
    #  address line 20 is tied low, so that addresses higher than
    #  1MB wrap around to zero by default. This code undoes this.
seta20.1:
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.1

    movb $0xd1, %al                                 # 0xd1 -> port 0x64
    outb %al, $0x64                                 # 0xd1 means: write data to 8042's P2 port
```

观察可知，与上面打印出的十行代码基本一致。

`bootblock.asm`中部分代码如下：

```assembly
start:
.code16                                             # Assemble for 16-bit mode
    cli                                             # Disable interrupts
    7c00:	fa                   	cli    
    cld                                             # String operations increment
    7c01:	fc                   	cld    

    # Set up the important data segment registers (DS, ES, SS).
    xorw %ax, %ax                                   # Segment number zero
    7c02:	31 c0                	xor    %eax,%eax
    movw %ax, %ds                                   # -> Data Segment
    7c04:	8e d8                	mov    %eax,%ds
    movw %ax, %es                                   # -> Extra Segment
    7c06:	8e c0                	mov    %eax,%es
    movw %ax, %ss                                   # -> Stack Segment
    7c08:	8e d0                	mov    %eax,%ss

00007c0a <seta20.1>:
    # Enable A20:
    #  For backwards compatibility with the earliest PCs, physical
    #  address line 20 is tied low, so that addresses higher than
    #  1MB wrap around to zero by default. This code undoes this.
seta20.1:
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    7c0a:	e4 64                	in     $0x64,%al
    testb $0x2, %al
    7c0c:	a8 02                	test   $0x2,%al
    jnz seta20.1
    7c0e:	75 fa                	jne    7c0a <seta20.1>
```

观察可知，与上面打印出的十行代码基本一致。

后面的代码也基本一致。

综上所述，三者代码基本一致。

#### 自己找一个`bootloader`或内核中的代码位置，设置断点并进行测试

再次修改`gdbinit`文件，修改断点地址。例如修改为`0x7c4a`。

再次运行`make debug`，可以看到如下输出。

```text
Breakpoint 1 at 0x7c4a

Breakpoint 1, 0x00007c4a in ?? ()
```

表明断点设置正常，可以通过`si`进行单步调试。

-----

### 练习3：分析`bootloader`进入保护模式的过程。

- 首先阅读`lab1/boot/bootasm.S`代码。

  ```assembly
  start:
  .code16                                             # Assemble for 16-bit mode
      cli                                             # Disable interrupts，屏蔽中断
      cld                                             # String operations increment，清空方向标志
  
      # Set up the important data segment registers (DS, ES, SS).
      xorw %ax, %ax                                   # Segment number zero
      movw %ax, %ds                                   # -> Data Segment
      movw %ax, %es                                   # -> Extra Segment
      movw %ax, %ss                                   # -> Stack Segment
  ```

  注意此时代码为16位代码。`bootloader`首先会关闭中断，并将各个寄存器置0。

#### 为何开启A20，以及如何开启A20

Intel早期的8086 CPU拥有1MB内存空间，存在“回卷”机制，用于避免寻址到超过1MB的内存发生异常。但后续发布的CPU内存空间超过1MB，但由于“回卷”机制，无法访问到超过1MB以上的内存。为了保持完全的向下兼容性，因此设计了`A20 Gate`。一开始A20地址线控制总是被屏蔽（为0），直到系统软件通过一定I/O操作打开它。简单的说，初始时A20为0，只能访问1MB以内的内存（大于1MB的地址则取余数），将其置为1后，才可以访问4GB内存。

A20地址位由键盘控制器8042控制，8042有两个I/O端口：0x60和0x64。

打开流程:

- 等待8042 Input buffer为空
- 发送Write 8042 Output Port （P2）命令到8042 Input buffer
- 等待8042 Input buffer为空
- 将8042 Output Port（P2）得到字节的第2位置1，然后写入8042 Input buffer

`bootasm.S`中代码如下：

```assembly
seta20.1:
    inb $0x64, %al                                  # 等待8042键盘控制器闲置
    testb $0x2, %al									# 判断输入缓存是否为空,al第二位是"input register (60h/64h) 有数据"
    jnz seta20.1									# 如果是1则循环等待

    movb $0xd1, %al                                 # 向0x64发送0xd1命令
    outb %al, $0x64                                 # 0xd1 means: write data to 8042's P2 port

seta20.2:
    inb $0x64, %al                                  # 同上
    testb $0x2, %al
    jnz seta20.2

    movb $0xdf, %al                                 # 向0x60发送0xdf命令
    outb %al, $0x60                                 # 0xdf = 11011111, means set P2's A20 bit(the 1 bit) to 1，成功打开A20
```

#### 如何初始化`GDT`表

首先载入`GDT`表。

```assembly
lgdt gdtdesc
```

再进入保护模式。

```assembly
movl %cr0, %eax			# 加载cr0到eax
orl $CR0_PE_ON, %eax	# 将eax的第0位置1
						# $CR0_PE_ON = 1
movl %eax, %cr0			# 将cr0的第0位置1
```

`cr0`寄存器是一个控制寄存器，其中第0位是PE(Protection Enabled)位。当其值为1时，CPU进入保护模式，同时启动段机制；当其值为0时，则为实地址模式。

之后通过长跳转到32位代码段并更新CS和EIP。

```assembly
ljmp $PROT_MODE_CSEG, $protcseg						# $PROT_MODE_CSEG=0x8
.code32                                             # Assemble for 32-bit mode
protcseg:
```

之后设置段寄存器，建立堆栈。

```assembly
# Set up the protected-mode data segment registers
movw $PROT_MODE_DSEG, %ax                       # Our data segment selector
movw %ax, %ds                                   # -> DS: Data Segment
movw %ax, %es                                   # -> ES: Extra Segment
movw %ax, %fs                                   # -> FS
movw %ax, %gs                                   # -> GS
movw %ax, %ss                                   # -> SS: Stack Segment

# Set up the stack pointer and call into C. The stack region is from 0--start(0x7c00)
movl $0x0, %ebp
movl $start, %esp
```

最后成功进入保护模式，进入boot方法。

```assembly
call bootmain
```

#### 如何使能和进入保护模式

将`cr0.PE`置1。

-----

### 练习4：分析`bootloader`加载ELF格式的OS的过程。

通过对之前`bootmain.S`的分析，我们知道系统上电后启动BIOS，加载`bootloader`开启保护模式并建立堆栈，因此便可以继续执行C代码，会执行`bootmain.c`加载ELF格式的OS kernel。由于OS存储在磁盘之中，因此需要先读取磁盘再加载。

#### `bootloader`如何读取硬盘扇区的

读取磁盘扇区主要有以下四步：

- 等待磁盘准备好
- 发出读取扇区的命令
- 等待磁盘准备好
- 把磁盘扇区数据读到指定内存中

```c
/* readsect - read a single sector at @secno into @dst */
static void
readsect(void *dst, uint32_t secno) {
    // wait for disk to be ready
    waitdisk();
    
    outb(0x1F2, 1);                         // count = 1
    outb(0x1F3, secno & 0xFF);
    outb(0x1F4, (secno >> 8) & 0xFF);
    outb(0x1F5, (secno >> 16) & 0xFF);
    outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0);
    outb(0x1F7, 0x20);                      // cmd 0x20 - read sectors

    // wait for disk to be ready
    waitdisk();

    // read a sector，
    insl(0x1F0, dst, SECTSIZE / 4);
}
```

首先执行`waitdisk`，等待磁盘不忙。再向`0x1f2`写入要读取的扇区数，之后将LBA的参数分别写入，再向`0x1f7`写入读信号`0x20`，最后再次等待磁盘闲置后，读取所要读取的扇区。

写入LBA参数时，在`0x1f3`、`0x1f4`和`0x1f5`每次写入8位，在`0x1f6`中，secno只写入低四位，而高四位中，第4位为0，表明是主盘；第5位和第7位必须为1；第6位为1，表明是LBA模式。因此需要与`0xe0`做或操作。

最后调用`insl()`从`0x1f0`中读取扇区到`dst`处，注意读取是以DWORD为单位，因此给参数为`SECTSIZE / 4`。

#### `bootloader`是如何加载ELF格式的OS

``` c
/* bootmain - the entry of bootloader */
void
bootmain(void) {
    // read the 1st page off disk
    readseg((uintptr_t)ELFHDR, SECTSIZE * 8, 0);

    // is this a valid ELF?
    if (ELFHDR->e_magic != ELF_MAGIC) {
        goto bad;
    }

    struct proghdr *ph, *eph;

    // load each program segment (ignores ph flags)
    ph = (struct proghdr *)((uintptr_t)ELFHDR + ELFHDR->e_phoff);
    eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph ++) {
        readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
    }

    // call the entry point from the ELF header
    // note: does not return
    ((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))();

bad:
    outw(0x8A00, 0x8A00);
    outw(0x8A00, 0x8E00);

    /* do nothing */
    while (1);
}
```

首先通过`readseg()`函数确定OS所在的ELF文件所处的磁盘扇区并通过`readsect()`读取磁盘扇区，把ELF文件头读取到内存中的`0x10000`处。

注意在`readseg()`函数中的细节：

- `va`可能不在一个扇区头处，因此要调整到扇区开始处。
- `secno`要加一因为通过之前实验我们知道，第一个扇区存储的是`bootblock`，而`OS kernel`从第二个扇区开始存储。

之后检验`ELFHDR`中的`e_magic`，从而确保文件正确。再根据`ELFHDR`中的`phoff`读取出`program header`表的位置偏移，从`phnum`中读取数目。之后便一一读取至内存的对应位置。读取完毕后，调用`ELF`文件的入口点处的函数，进入OS程序，完成系统启动。

由于静态代码分析已完全了解启动过程，便不再用`qemu`进行单步调试跟踪。

### 练习5：实现函数调用堆栈跟踪函数。

这一个练习只需要完成`kern/debug/kdebug.c`中的`print_stackframe()`函数。这个函数的主要作用是打印出当前函数栈中的嵌套调用关系（类似于调试报错时的栈帧信息打印）。

由于栈帧的特殊结构，我们可以通过不断读取`ebp`的值来覆盖当前`ebp`从而实现栈帧切换。栈帧中存储着函数的返回地址、参数等，而只要知道`ebp`的值，便可以通过地址来读取到参数和`eip`等信息。

简易的栈模型如下：

```text
ss:[ebp+12]==>  参数2
ss:[ebp+8] ==>  参数1
ss:[ebp+4] ==>  返回地址
ss:[ebp]   ==>  上一层[ebp]
ss:[ebp-4] ==>  局部变量1
ss:[ebp-8] ==>  局部变量2
```

为了方便我们实现，这个文件中提供了以下几个函数来辅助我们完成：

- `read_ebp()`：读取当前`ebp`的值。
- `read_eip()`：读取当前`eip`的值。
- `print_debuginfo()`：打印出函数的名称文件行号等信息。

在文件中给了丰富的提示代码，我们可以照着提示去一步步的实现。下面给出代码。

```c
void print_stackframe(void)
{
    /* LAB1 YOUR CODE : STEP 1 */
    // (1) call read_ebp() to get the value of ebp. the type is (uint32_t);
    uint32_t ebp = read_ebp();
    // (2) call read_eip() to get the value of eip. the type is (uint32_t);
    uint32_t eip = read_eip();
    // (3) from 0 .. STACKFRAME_DEPTH
    for (int i = 0; i < STACKFRAME_DEPTH && ebp != 0; i++)
    {
        // (3.1) printf value of ebp, eip
        cprintf("ebp:0x%08x eip:0x%08x args:", ebp, eip);
        for (int j = 0; j < 4; j++)
        {
            // (3.2)(uint32_t) calling arguments[0..4] = the contents in address(uint32_t) ebp + 2 [0..4] 
            uint32_t arg = *((uint32_t *)ebp + 2 + j);
            cprintf("0x%08x ", arg);
        }
        // (3.3) cprintf("\n");
        cprintf("\n");
        // (3.4) call print_debuginfo(eip-1) to print the C calling function name and line number, etc.
        print_debuginfo(eip - 1);
        // (3.5) popup a calling stackframe
        //    *NOTICE : the calling funciton's return addr eip  = ss:[ebp+4]
        //                  *the calling funciton's ebp = ss:[ebp] eip = *((uint32_t *)ebp + 1);
        ebp = *((uint32_t *)ebp);
    }
}

```

运行`make qemu`得到如下输出。

```asm
+ cc kern/debug/kdebug.c
+ ld bin/kernel
记录了10000+0 的读入
记录了10000+0 的写出
5120000字节（5.1 MB，4.9 MiB）已复制，0.0636968 s，80.4 MB/s
记录了1+0 的读入
记录了1+0 的写出
512字节已复制，0.00268164 s，191 kB/s
记录了154+1 的读入
记录了154+1 的写出
78912字节（79 kB，77 KiB）已复制，0.00359308 s，22.0 MB/s
WARNING: Image format was not specified for 'bin/ucore.img' and probing guessed raw.
         Automatically detecting the format is dangerous for raw images, write operations on block 0 will be restricted.
         Specify the 'raw' format explicitly to remove the restrictions.
(THU.CST) os is loading ...

Special kernel symbols:
  entry  0x00100000 (phys)
  etext  0x0010341d (phys)
  edata  0x0010fa16 (phys)
  end    0x00110d20 (phys)
Kernel executable memory footprint: 68KB
ebp:0x00007b28 eip:0x00100ab3 args:0x00010094 0x00010094 0x00007b58 0x00100096 
    kern/debug/kdebug.c:334: print_stackframe+25
ebp:0x00007b38 eip:0x00100db5 args:0x00000000 0x00000000 0x00000000 0x00007ba8 
    kern/debug/kmonitor.c:125: mon_backtrace+14
ebp:0x00007b58 eip:0x00100096 args:0x00000000 0x00007b80 0xffff0000 0x00007b84 
    kern/init/init.c:48: grade_backtrace2+37
ebp:0x00007b78 eip:0x001000c4 args:0x00000000 0xffff0000 0x00007ba4 0x00000029 
    kern/init/init.c:53: grade_backtrace1+42
ebp:0x00007b98 eip:0x001000e7 args:0x00000000 0x00100000 0xffff0000 0x0000001d 
    kern/init/init.c:58: grade_backtrace0+27
ebp:0x00007bb8 eip:0x00100111 args:0x0010343c 0x00103420 0x0000130a 0x00000000 
    kern/init/init.c:63: grade_backtrace+38
ebp:0x00007be8 eip:0x00100055 args:0x00000000 0x00000000 0x00000000 0x00007c4f 
    kern/init/init.c:28: kern_init+84
ebp:0x00007bf8 eip:0x00007d74 args:0xc031fcfa 0xc08ed88e 0x64e4d08e 0xfa7502a8 
    <unknow>: -- 0x00007d73 --
```

可见满足输出要求。

写代码时遇到的几个坑：

- 没有判断`ebp!=0`导致一直到栈低还未停止，因此加入循环控制条件`ebp!=0`。
- 一开始错误理解了`ebp`，`ebp`寄存器中存储的是栈中的地址，指向的空间内存储的是上一层的`ebp`。因此访问`ebp`地址附近的栈空间，只需把其类型转换为`uint32_t *`，再移动这个指针就可以得到参数和返回地址。

-----

### 练习6：完善中断初始化和处理

#### 中断描述符表（也可简称为保护模式下的中断向量表）中一个表项占多少字节？其中哪几位代表中断处理代码的入口？

中断描述符表中一个表项叫做一个门描述符，占8个字节，其一般格式如下：

```text
63                               48 47      46  44   42    39             34   32 
+-------------------------------------------------------------------------------+
|                                  |       |  D  |   |     |      |   |   |     |
|       Offset 31..16              |   P   |  P  | 0 |Type |0 0 0 | 0 | 0 | IST |
|                                  |       |  L  |   |     |      |   |   |     |
 -------------------------------------------------------------------------------+
31                                   16 15                                      0
+-------------------------------------------------------------------------------+
|                                      |                                        |
|          Segment Selector            |                 Offset 15..0           |
|                                      |                                        |
+-------------------------------------------------------------------------------+
```

其中：

- `Selector` - 目标代码段的段选择子；
- `Offset` - 处理程序入口点的偏移量；
- `DPL` - 描述符权限级别；
- `P` - 当前段标志；
- `IST` - 中断堆栈表；
- `TYPE` - 本地描述符表（LDT）段描述符，任务状态段（TSS）描述符，调用门描述符，中断门描述符，陷阱门描述符或任务门描述符之一。

因此第0-15位和48-63位拼接起来即为中断处理代码的入口地址。

#### 请编程完善`kern/trap/trap.c`中对中断向量表进行初始化的函数`idt_init`。

在`idt_init`函数中，依次对所有中断入口进行初始化。使用`mmu.h`中的`SETGATE`宏，填充`idt`数组内容。每个中断的入口由`tools/vectors.c`生成，使用`trap.c`中声明的`vectors`数组即可。

-----

```c
void idt_init(void)
{
    /* LAB1 YOUR CODE : STEP 2 */
    /* (1) Where are the entry addrs of each Interrupt Service Routine (ISR)?
      *     All ISR's entry addrs are stored in __vectors. where is uintptr_t __vectors[] ?
      *     __vectors[] is in kern/trap/vector.S which is produced by tools/vector.c
      *     (try "make" command in lab1, then you will find vector.S in kern/trap DIR)
      *     You can use  "extern uintptr_t __vectors[];" to define this extern variable which will be used later.
      * (2) Now you should setup the entries of ISR in Interrupt Description Table (IDT).
      *     Can you see idt[256] in this file? Yes, it's IDT! you can use SETGATE macro to setup each item of IDT
      * (3) After setup the contents of IDT, you will let CPU know where is the IDT by using 'lidt' instruction.
      *     You don't know the meaning of this instruction? just google it! and check the libs/x86.h to know more.
      *     Notice: the argument of lidt is idt_pd. try to find it!
      */
    extern uintptr_t __vectors[];
    for (int i = 0; i < 256; i++)
    {
        SETGATE(idt[i], 0, GD_KTEXT, __vectors[i], DPL_KERNEL);
    }
    SETGATE(idt[T_SWITCH_TOU], 0, GD_KTEXT, __vectors[T_SWITCH_TOU], DPL_USER);
    lidt(&idt_pd);
}
```

这一节代码注释也很详细，首先声明保存所有入口地址的数组`__vectors[]`，再对`idt[]`数组中每一项用`SETGATE`进行初始化，最后通过指令`lidt`让CPU知道IDT数组已经初始化完毕。

下面具体说明`SETGATE`宏。

```c
#define SETGATE(gate, istrap, sel, off, dpl) {}
/* *
 * Set up a normal interrupt/trap gate descriptor
 *   - istrap: 1 for a trap (= exception) gate, 0 for an interrupt gate
 *   - sel: Code segment selector for interrupt/trap handler
 *   - off: Offset in code segment for interrupt/trap handler
 *   - dpl: Descriptor Privilege Level - the privilege level required
 *          for software to invoke this interrupt/trap gate explicitly
 *          using an int instruction.
 * */
```

- 第一个参数，门描述符，显然为`idt[i]`
- 第二个参数，表明门的类型是`Interrupt Gate`还是`Trap Gate`，题目中描述使用中断门，因此为0
- 第三个参数，表明代码段选择子，表示用于中断/陷阱处理程序的代码段选择器，采用内核代码段，即`GD_KTEXT`
- 第四个参数，表明偏移量，显然为`__vectors[i]`
- 第五个参数，表明权限等级描述，题目描述为0，即`DPL_KERNEL`

由于题目中说，实现特权级3到特权级0的切换的中断门描述符为3，因此需要单独设置一个。

在`trap.h`中关于`processor-defined`中有

```c
/* *
 * These are arbitrarily chosen, but with care not to overlap
 * processor defined exceptions or interrupt vectors.
 * */
#define T_SWITCH_TOU                120    // user/kernel switch
#define T_SWITCH_TOK                121    // user/kernel switch
```

因此，随便选择一个即可。

#### 请编程完善`trap.c`中的中断处理函数`trap`，在对时钟中断进行处理的部分填写`trap`函数中处理时钟中断的部分，使操作系统每遇到100次时钟中断后，调用`print_ticks`子程序，向屏幕上打印一行文字”100 ticks”

查看`trap()`函数。

```c
void trap(struct trapframe *tf)
{
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}
```

再查看`trap_dispatch()`函数。

这个代码非常简单，给的提示也很充足。

```c
switch (tf->tf_trapno)
{
    case IRQ_OFFSET + IRQ_TIMER:
        /* LAB1 YOUR CODE : STEP 3 */
        /* handle the timer interrupt */
        /* (1) After a timer interrupt, you should record this event using a global variable (increase it), such as ticks in kern/driver/clock.c
         * (2) Every TICK_NUM cycle, you can print some info using a funciton, such as print_ticks().
         * (3) Too Simple? Yes, I think so!
         */
        ticks++;
        if(ticks % TICK_NUM == 0){
            print_ticks();
        }
        break;
}
```

完成后，再次运行`make qemu`，即可看到下图，每隔1s打印一次`100 ticks`，并会显示出键盘输入的字母。

[![6vaSB9.png](https://z3.ax1x.com/2021/03/26/6vaSB9.png)](https://imgtu.com/i/6vaSB9)

-----

### 挑战1：

扩展proj4,增加`syscall`功能，即增加一用户态函数（可执行一特定系统调用：获得时钟计数值），当内核初始完毕后，可从内核态返回到用户态的函数，而用户态的函数又通过系统调用得到内核态的服务。