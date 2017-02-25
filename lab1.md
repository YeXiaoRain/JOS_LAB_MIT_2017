lab1
---

# 知识依赖
 * master分支的配置或mit官方配置文档
 * git 的使用 (pull push commit checkout branch merge archive)
 * [官方文档](https://pdos.csail.mit.edu/6.828/2016/labs/lab1/)
 * [相关参考](https://pdos.csail.mit.edu/6.828/2016/reference.html)

# 查看我的完成代码

`git checkout -b finish_lab1 origin/finish_lab1`

[TODO] 整理这里 加上结构图

[TODO] kernel内部 结构图

部分代码完成后可以通过`make grade`命令测试

生成提交文件 `make handin`

# Tutorial

**SJTU的同学 请以SJTU的指导为准 我记录了一些的需要调整的部分 见底部**

[TODO] [sjtu lab1 链接]

> 本篇写到最后 实际还是做的sjtu的jos lab 两者比较

> mit 的评测写得,要更好,但涉及的代码练习相对更少,推荐按照sjtu的要求做

> 两个的代码实现有略微不同

> 虽然这个为sjtu的lab1的过程记录 但为了保持那个仓库的整洁 攻略就放这里了，这里的代码实现为mit jos lab的

## 指令建议

 我认为熟悉以下指令 对做lab有很好的帮助

    git checkout
    git diff
    git rebase -i

    grep -r
    grep -n
    grep -A

    gdb b
    gdb c
    gdb p
    gdb si
    gdb x/Ni
    gdb x/Nx

    find -name

    ssh oslab@192.168.?.?
    scp -r

## 开始做lab

首先切换到分支`git checkout -b lab1 origin/lab1`

在此分支上 建立一个工作分支`git checkout -b mylab1`

在完成master的readme中的环境配置后，开始阅读[lab1的文档](https://pdos.csail.mit.edu/6.828/2016/labs/lab1/)

## Part1

首先要熟悉x86汇编语言
 * [电脑汇编语言](https://pdos.csail.mit.edu/6.828/2016/readings/pcasm-book.pdf)
 * [inline Assembly Guide](http://www.delorie.com/djgpp/doc/brennan/brennan_att_inline_djgpp.html)
 * [80386](https://pdos.csail.mit.edu/6.828/2016/readings/i386/toc.htm)

与lab无关 但若有兴趣可以看的
 * [英特尔IA-32 软件开发手册](http://www.intel.com/content/www/us/en/processors/architectures-software-developer-manuals.html)
 * [AMD](http://developer.amd.com/documentation/guides/Pages/default.aspx#manuals)

任务是实现一个真正能运行的系统，如果有一个能模拟硬件，来运行/调试我们的系统的工具就好了，我们将用便是之前下载的`qemu`

先在lab1文件夹下执行`make`

```
> make
***
*** Error: Couldn't find a working QEMU executable.
*** Is the directory containing the qemu binary in your PATH
*** or have you tried setting the QEMU variable in conf/env.mk?
***
***
*** Error: Couldn't find a working QEMU executable.
*** Is the directory containing the qemu binary in your PATH
*** or have you tried setting the QEMU variable in conf/env.mk?
***
+ as kern/entry.S
+ cc kern/entrypgdir.c
+ cc kern/init.c
+ cc kern/console.c
+ cc kern/monitor.c
+ cc kern/printf.`c
+ cc kern/kdebug.c
+ cc lib/printfmt.c
+ cc lib/readline.c
+ cc lib/string.c
+ ld obj/kern/kernel
+ as boot/boot.S
+ cc -Os boot/main.c
+ ld boot/boot
boot block is 390 bytes (max 510)
+ mk obj/kern/kernel.img
```

**使用sjtu的虚拟机 已经配置好了qemu,可跳到下方`标记点1`继续阅读**

这里提示我找不到`QEMU`,需要修改配置文件`conf/env.mk`,并提示生成了`obj/kern/kernel.img`

如果有`__udivdi3`这样的错误，请按照`master`分支下的`README.md`进行安装依赖软件

配置`conf/env.mk` 以我的为例,我使用的是github上的[qemu](https://github.com/qemu/qemu)

通过 在qemu文件夹里[已经配置编译过的]输入命令

```
> find ./ -name "*qemu*" -type f | grep dist
./dist/libexec/qemu-bridge-helper
./dist/share/qemu/qemu-icon.bmp
./dist/share/qemu/qemu_logo_no_text.svg
./dist/bin/qemu-ga
./dist/bin/qemu-nbd
./dist/bin/qemu-io
./dist/bin/qemu-system-i386
./dist/bin/qemu-img
```

发现 编译出的在`./dist/bin/qemu-system-i386`

配置`conf/env.mk`设置我们编译出的qemu的路径，例如我的设置为`QEMU=/home/yexiaorain/Documents/os/qemu-2.7/dist/bin/qemu-system-i386`

#### 标记点1

测试`make qemu` 有形如以下输出表示配置的工作已经顺利完成
 * 如果是通过ssh连接的,没有图形界面请使用`make qemu-nox`代替
 * 在此附赠一条关闭qemu的命令给非图形界面玩家`ps -aux | grep qemu | awk '{print $2}'| xargs kill -9`或`pkill -9 qemu`

```
> make qemu-nox
***
*** Use Ctrl-a x to exit qemu
***
/home/yexiaorain/Documents/os/qemu-2.7/dist/bin/qemu-system-i386 -nographic -drive file=obj/kern/kernel.img,index=0,media=disk,format=raw -serial mon:stdio -gdb tcp::26000 -D qemu.log
6828 decimal is XXX octal!
entering test_backtrace 5
entering test_backtrace 4
entering test_backtrace 3
entering test_backtrace 2
entering test_backtrace 1
entering test_backtrace 0
leaving test_backtrace 0
leaving test_backtrace 1
leaving test_backtrace 2
leaving test_backtrace 3
leaving test_backtrace 4
leaving test_backtrace 5
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands.
K>
```

#### 使用GDB,打开两个终端
 * 第一个输入`make qemu-gdb`或`make qemu-nox-gdb`(对于无图形化用户)
 * 第二个输入`make gdb`

然后你会看到如下这般的

```
>make gdb
gdb -n -x .gdbinit
GNU gdb (Ubuntu 7.11.1-0ubuntu1~16.04) 7.11.1
Copyright (C) 2016 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "i686-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
<http://www.gnu.org/software/gdb/documentation/>.
For help, type "help".
Type "apropos word" to search for commands related to "word".
+ target remote localhost:26000
warning: A handler for the OS ABI "GNU/Linux" is not built into this configuration
of GDB.  Attempting to continue with the default i8086 settings.

The target architecture is assumed to be i8086
[f000:fff0]    0xffff0:0xffff0ljmp   $0xf000,$0xe05b
0x0000fff0 in ?? ()
+ symbol-file obj/kern/kernel
```

看输出的`[f000:fff0] 0xffff0: ljmp   $0xf000,$0xe05b`

这是返汇编指令得到的，由此我们知道
 * IBM PC先执行0x000ffff0物理地址的位置,这是在ROM BIOS 顶部64KB的地方
 * 计算机开始执行的时候CS=0xf000, IP=0xfff0, (cs,ip)元组代表的地址
 * 第一条指令是跳转，目标地址是 CS=0xf000,IP=0xe05b

其原因是IBM PC原始设计，在电脑上电时被定到BIOS的位置，而BIOS是被出厂时固定的写在(0xf000,0xfff0)的地方，这里QEMU完全模拟IBM PC的实际运行。

(CS,IP)怎么得到物理地址呢，这里在"real mode"(**实模式** 即计算机起始的模式)下,物理地址计算表达式
 * `physical address = 16 * segment + offset = 16 * cs + ip`
 * 在这里即`physical address = 0x10 * 0xf000 + 0xfff0 = 0x000ffff0`

用`si`来看看 计算机然后干了什么,当BIOS运行的时候
 * 设置了**中断描述表**
 * 初始化了一系列设备，例如VGA 显示[即你可以看到Starting SeaBIOS的]
 * 在初始化PCI总线和所有BIOS知道的硬件以后,它开始找可以加载的设备，例如软盘 硬盘 光盘,例如当找到硬盘的时候,BIOS读取硬盘上的加载器,并把控制权转交给硬盘

第一部分结束，没有任何代码，但我们搭了一个qemu,看了电脑启动的一个大体面貌，

## Part2

盘的最小单位为512byte的扇区,这是最小的传输单位,如果一个盘可以被引导,那么第一个扇区为启动扇区,也就是启动代码放置的位置

当BIOS发现一个盘,就把它的首个扇区的数据加载到地址0x7c00~0x7dff，再用jmp指令 跳转到(cs,ip)=(0000:7c00),并把控制权交给boot loader

这些位置0x7c00(再如前面跳到0xe05b)看着是一个随意的值，但它们是硬件定下的值，是一个标准

电脑硬件架构师为了让电脑的boot过程更轻巧简洁(slightly),CD-ROM加载就很晚了(也更复杂,更功能强大..)，CD-ROM使用 2048bytes的扇区,它也能加载更大的引导程序

这个lab使用传统的引导,意味这全部引导程序只能放在512bytes内,**请阅读他们**,包括
 * `boot/boot.S`
 * `boot/main.c`
 * `obj/boot/boot.asm`是它们编译后的反编译出的汇编,它可以用来帮助你理解

boot loader进行了模式切换,即从real mode切换成了32-bit protected mode.
 * 物理地址的计算方式改变不再是 `16*cs+ip` [TODO新的转换方式], 地址从1MB变到16MB，
 * 可以使用paging和virtual memory
 * boot loader直接从磁盘读取了kernel

做跟踪[TODO exact assembly instructions 目前直接读的C代码]
 * `bootmain()->readsect()->exact assembly instructions->readsect()->bootmain()->then`

#### Exercise 3

 * 什么时候具体什么操作让模式从real mode 切换到32-bit?

> `ljmp $PROT_MODE_CSEG, $protcseg` 让16位切换到32位 见源代码boot/boot.S里的注释 [TODO 为什么不是在设置完CR0时呢？]
```assembly
    lgdt    gdtdesc
    movl    %cr0, %eax
    orl     $CR0_PE_ON, %eax
    movl    %eax, %cr0
    ljmp    $PROT_MODE_CSEG, $protcseg
```

 * boot loader 和 kernel 的交接处(最后一条 和 第一条)的具体代码是什么? Kernel的第一条指令 在哪?

> bootloader最后一条在`main.c`中
```c
    ((void (*)(void)) (ELFHDR->e_entry))();
```
>
```
定位指令地址 发现是7d74
终端1> grep -r e_entry obj/boot/boot.asm -A1
  ((void (*)(void)) (ELFHDR->e_entry))();
    7d74: ff 15 18 00 01 00      call   *0x10018
终端1> make qemu-gdb
终端2> make gdb
(gdb) b *0x7d74
(gdb) c
(gdb) x/1x 0x10018
0x10018:  0x0010000c
(gdb) x/1i 0x0010000c
0x10000c: movw   $0x1234,0x472
```

> bootloader最后一条在`boot.asm`中 即编译后反编译得到的 即该c代码的最后实际对应的指令

> kernel第一条`movw   $0x1234,0x472` 该指令地址为`0x0010000c`

 * boot loader 怎样知道要具体加载多少个 扇区?

> 在`boot/main.c`的bootmain 函数里
```c
`ELFHDR->e_phnum`
```

### Loading the Kernel

 熟.....熟悉c的指针和地址 哇 还提供了用来熟悉的[代码](https://pdos.csail.mit.edu/6.828/2016/labs/lab1/pointers.c)

为了要更深入的理解`boot/main.c`,需要一些ELF binary(Executable and Linkable Format 可执行链接的二进制文件)的知识
 * ELF 很复杂 作者给了一个文档和一个[wikipedia的链接](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)
 * 关于整个6.8282的所有lab,你只用把ELF看做{没有loading的header+代码段+数据段} ,并且这些在运行时不会被boot loader修改,它们被直接加载并执行
 * ELF = 定长header + 变长程序头部(用于列出每一块要被加载的程序块)
 * 定义在`inc/elf.h`
 * 我们关心的 `.text`可执行段 ,`.rodata`只读数据,`.data`初始化段 如全局变量`int x=5;`就在这里
 * 当linker 计算程序的内存结构时,它为未初始化的全局变量预留空间如`int x;`,在elf中它们在`.data`的后面的`.bss`段里,它们被"boot loader"默认初始化为0

如下反编译得到的表
 * 程序执行分两步{把硬盘数据搬到内存,执行(会使用VMA)}
 * LMA(Load Memory Address) 表示该段需要被装载的地址
 * The load address of a binary is the memory address at which a binary is actually loaded. For example, the BIOS is loaded by the PC hardware at address 0xf0000. So this is the BIOS's load address. Similarly, the BIOS loads the boot sector at address 0x7c00. So this is the boot sector's load address.
 * The link address of a binary is the memory address for which the binary is linked. Linking a binary for a given link address prepares it to be loaded at that address. The linker encodes the link address in the binary in various ways, for example when the code needs the address of a global variable, with the result that a binary usually won't work if it is not loaded at the address that it is linked for.
 * VMA(Virtural Memory Address/Link address) VMA 需要被linker处理,并按照linker的意思调整, 在6.828里不会用到
 * ics architecture lab里我们手写的y86代码的.pos是VMA+LMA的功能
 * [link address 和 load address的区别](http://www.iecc.com/linker/linker01.html)

```
> objdump -h obj/kern/kernel
kern/kernel:     file format elf32-i386
Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .text         00001871  f0100000  00100000  00001000  2**4
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  1 .rodata       00000714  f0101880  00101880  00002880  2**5
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  2 .stab         000038d1  f0101f94  00101f94  00002f94  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  3 .stabstr      000018bb  f0105865  00105865  00006865  2**0
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  4 .data         0000a300  f0108000  00108000  00009000  2**12
                  CONTENTS, ALLOC, LOAD, DATA
  5 .bss          00000644  f0112300  00112300  00013300  2**5
                  ALLOC
  6 .comment      00000034  00000000  00000000  00013300  2**0
                  CONTENTS, READONLY
```

通过 命令`objdump -x obj/kern/kernel` 可以看到程序每一个段的会被占用的目的地址

通过 命令`objdump -f obj/kern/kernel` 可以看到程序的起始地址

#### Exercise 5

[TODO 和上方疑问相同 why and what 引起16-bit to 32-bit 以及在该处设置段点会执行错误指令 需要在更前的地方设置断点再不断`si`]

>Q: Trace through the first few instructions of the boot loader again and identify the first instruction that would "break" or otherwise do the wrong thing if you were to get the boot loader's link address wrong.

Obviously the `ljmp    $PROT_MODE_CSEG, $protcseg` is the first instruction that breaks:
```asm
Right:
[   0:7c2d] 0x7c2d: ljmp   $0x8,$0x7c32
The target architecture is assumed to be i386
0x7c32: mov    $0x10,%ax
0x7c36: mov    %eax,%ds
0x7c38: mov    %eax,%es
0x7c3a: mov    %eax,%fs
```
```asm
Wrong:
[   0:7c2d] 0x7c2d: ljmp   $0x8,$0x7c36
[f000:e05b] 0xfe05b:  cmpl   $0x0,%cs:0x66d4
[f000:e062] 0xfe062:  jne    0xfd3da
[f000:d3da] 0xfd3da:  cli
[f000:d3db] 0xfd3db:  cld
```

#### Exercise 6
 * 在 进入boot loader的时候和进入kernel的时候 检测0x00100000处的`8 words`长度的值
```
(gdb) b *0x7c00
(gdb) c
(gdb) x/8x 0x00100000
0x100000: 0x00000000 0x00000000 0x00000000 0x00000000
0x100010: 0x00000000 0x00000000 0x00000000 0x00000000

(gdb) b *0x7d74
(gdb) c
(gdb) si
(gdb) x/8x 0x00100000
0x100000:0x1badb002 0x00000000 0xe4524ffe 0x7205c766
0x100010:0x34000004 0x0000b812 0x220f0011 0xc0200fd8
(gdb) x/8i 0x00100000
0x100000:add    0x1bad(%eax),%dh
0x100006:add    %al,(%eax)
0x100008:decb   0x52(%edi)
0x10000b:in     $0x66,%al
0x10000d:movl   $0xb81234,0x472
0x100017:add    %dl,(%ecx)
0x100019:add    %cl,(%edi)
0x10001b:and    %al,%bl
```

### Part2 总结

这一部分完成后,我们认识到
 * 计算机上电后,先执行BIOS的指令(在这里由qemu模拟)，对硬件等检查,如果找到可用disk，就读把该头部移动到内存并执行(转交处理权限)
 * 该头部即是boot loader,(在lab里即是由boot/main.c boot.s共同实现),在实际的电脑中由安装系统时写入磁盘头部
 * boot loader的功能是 配置全局描述符，一些段寄存器(DS ES SS)，切换到32-bit模式(boot/boot.S),再把kernel装入内存并执行(boot/main.c)
 * 然后开始执行kernel

与关中断对应，没有找到sti 只找到popf在`inc/x86.h`里 `_(:з」∠)_`

## Part3

kernel 会被加载到 0xf0100000，但实际的地址空间很可能没有这么大，我们用硬件管理内存 把0x00100000物理地址映射到0xf0100000虚拟地址

下一个lab 我们会把0x00000000 到 0x0fffffff的物理地址 和 0xf0000000 到 0xffffffff 的虚拟地址(共256MB)进行一一映射

在这个lab 我们通过` kern/entrypgdir.c`手工映射,前4MB,可以看一下代码 非常的萌

这里 `entry_pgdir`把虚拟地址 `0xf0000000~0xf0400000` 和`0x00000000~0x004000000` 都 映射到物理地址`0x00000000~0x00400000`

在 `kern/entry.S`设置`CR0_PG`位以前 都是把地址访问当做物理地址对待,一旦该位被设置，地址访问将被**硬件**视为虚拟地址,并被**硬件**根据设置的映射表(`entry_pgdir`)进行选择物理地址

访问以上两端虚拟地址以外的地址 会引发`hardware exception `,又我们尚未设置中断处理，所以会引发qemu崩掉

#### Exercise 7

在`CR0_PG`被设置前后 `0x00100000`和`0xf0100000`位置的值是什么,在**设置**完后执行的命令，如果没有进行**映射**会出什么问题

```
1>make qemu-gdb-nox
2>make gdb
(gdb)b *0x100025
(gdb)c
(gdb) p/1x *0x00100000
$8 = 0x1badb002
(gdb) p/1x *0xf0100000
$9 = 0xffffffff
(gdb) si
(gdb) p/1x *0x00100000
$10 = 0x1badb002
(gdb) p/1x *0xf0100000
$11 = 0x1badb002
(gdb) si
0x10002d: jmp    *%eax
(gdb) p/x $eax
$13 = 0xf010002f
```

值如上所示，会jmp到`0xf010002f`，但因为没映射会发生上述的`hardware exception`

---

**该部分代码任务的完成都可以通过`make qemu-nox`查看效果。**

任务格式化输出到console

哇 开始写代码，读`kern/printf.c`, `lib/printfmt.c`, 和 `kern/console.c` 理解关系

阅读结果 `kern/console.c`负责和用户交互处理键盘相应对应的显示字符,根据决策调用输出响应，其输出会调用cprintf函数

`cprintf`函数在`kern/printf.c`里,该函数会调用`vprintfmt` ,`va_start`,`va_end`

关于`va_`可参考[cplusplus.com](http://www.cplusplus.com/reference/cstdarg/),其功能是对于不定长参数的处理

其中`vprintfmt`在`lib/printfmt.c`中

该任务要求我们%o能输出八进制,参考`case 'u'`, 最初我参考的'd'但试了一试标准的printf的`%o`发现是看作无符号的

```c++
    case 'o':
      num = getuint(&ap, lflag);
      base = 8;
      goto number;
```

SJTU的 要求%o输出八进制以0引导,并且要支持'+'flag` 即正数输出正好 负数输出负号

```c++
    case 'o':
      num = getuint(&ap, lflag);
      putch('0', putdat);
      base = 8;
      goto number;
```

符号的支持 见变量`precedeflag`因为只有`%d`有正负区别 所以 只对`case 'd'`里改动

```c++
    case 'd':
      num = getint(&ap, lflag);
      if ((long long) num < 0) {
        putch('-', putdat);
        num = -(long long) num;
      }else if(precedeflag && num){
        putch('+', putdat);
      }
      base = 10;
      goto number;

```

---

**该部分代码 可在`kern/monitor.c`中的`monitor`函数里添加尝试**

#### Exercise 8

 * 解释 `printf.c` 和`console.c` 之间的接口
 * `console.c`提供了哪些接口
 * 它们怎么被`printf.c`使用

注意到`console.c`的注释`//High'-level console I/O.  Used by readline and cprintf.`说明`console`提供的函数有`cputchar`,`getchar`,`iscons`

查看调用 知道`printf.c`只调用了`cputchar`

```
kern> grep -r "cputchar" *
console.c:cputchar(int c)
printf.c:// based on printfmt() and the kernel console's cputchar().
printf.c:      cputchar(ch);

kern> grep -r "getchar" *
console.c:getchar(void)

kern> grep -r "iscons" *
console.c:iscons(int fdnum)
```

 * 解释`console.c`中下面的代码

```c
if (crt_pos >= CRT_SIZE) {
  int i;
  memcpy(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
  for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
    crt_buf[i] = 0x0700 | ' ';
  crt_pos -= CRT_COLS;
}
```

通搜索`crt`(Cathode ray tube)会看到这样的注释`/***** Text-mode CGA/VGA display output *****/`,那么这段代码也就好理解了:如果屏幕满了则向下滚1行

 * 在`cprintf()`中`fmt`和`ap`分别的指向是?

```c
int x = 1, y = 3, z = 4;
cprintf("x %d, y %x, z %d\n", x, y, z);
```

`fmt`指向格式字串,`ap`指向除了格式字串以外的

 * List (in order of execution) each call to cons_putc, va_arg, and vcprintf. For cons_putc, list its argument as well. For va_arg, list what ap points to before and after the call. For vcprintf list the values of its two arguments.

使用`(gdb) b cprintf`和`(gdb) c`定位到**自己添加的** `cprintf("x %d, y %x, z %d\n", x, y, z)`这句

再使用`(gdb) b cons_putc`和`(gdb) i stack` 查看输出

 * 运行下面的代码 会输出什么 为什么

```
    unsigned int i = 0x00646c72;
    cprintf("H%x Wo%s", 57616, &i);
```

输出`He110 World`， 因为......c++基础知识`57616` == `0xe110` ,字符串的编译后的保存形式除了大小端和整数无差别，都是以值的形式保存,所以 这里`rld\0`

 * In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?

`cprintf("x=%d y=%d", 3);`

 `ics`函数的参数传递堆栈知识 取栈上的`3`的再+4bytes的位置

 * Q: Let's say that GCC changed its calling convention so that it pushed arguments on the stack in declaration order, so that the last argument is pushed last. How would you have to change cprintf or its interface so that it would still be possible to pass it a variable number of arguments?

在最后push一个表示个数的,根据改值,再倒着读即可

 * Challenge Enhance the console to allow text to be printed in different colors. The traditional way to do this is to make it interpret ANSI escape sequences embedded in the text strings printed to the console, but you may use any mechanism you like. There is plenty of information on the 6.828 reference page and elsewhere on the web on programming the VGA display hardware. If you're feeling really adventurous, you could try switching the VGA hardware into a graphics mode and making the console draw text onto the graphical frame buffer.

这里让尝试支持不同颜色,大自定位到`kern/monitor.c`的`cga_putc`函数，源代码也有和颜色相关的注解[TODO]

---

#### SJTU Exercise 10

 * Enhance the cprintf function to allow it print with the %n specifier, you can consult the `%n` specifier specification of the C99 printf function for your reference by typing "man 3 printf" on the console. In this lab, we will use the `char *` type argument **instead of** the C99 `int *` argument, that is, "the number of characters written so far is stored into the signed char type integer indicated by the char * pointer argument. No argument is converted." **You must deal with some special cases properly**, because we are in kernel, such as when the argument is a NULL pointer, or when the char integer pointed by the argument has been overflowed. Find and fill in this code fragment.

 支持`%n` 传入`char *`,把当前输出的位置 写回传来的变量 例如`printf("fff%n",&b)`则把3写入b

搜索到代码`case:'n'`,源代码提供了错误输出信息,

当前我们有的变量
 * putdat 看着是`void *` ,它都是作为putch的参数使用的,再看`kern/printf.c`的`putch`函数.........然后发现实际用法是个`int *`...刚好用来表示输出的位置
 * fmt 即格式化字符串
 * ap 除了字符串以外传给printf 的参数
 * p  为`%s`或`%e`输出作为临时的`char *`
 * ch 当前从fmt取的字符
 * err `%e`错误号
 * num 为数字输出的值
 * base 为数字输出的基/进制
 * lflag 为数字输出long或long long的标识
 * width 小数的宽度标识
 * precision 小数或字符串长度的具体值
 * altflag 为`%#`,表示输出字符串时是否需要把非`Printable characters` 转换成? 和标准的[printf](http://www.cplusplus.com/reference/cstdio/printf/)的`#`不一样
 * padc 表示用来填补宽度所用的字符

那么这里我们能用的,也就是ap和putdat了刚好它也是`void *`

 查了`va_arg`的[源代码](https://www.rpi.edu/dept/cis/software/g77-mingw32/include/stdarg.h),`type va_arg (va_list ap, type)`把ap向的指向按照type类型返回 并将ap指向(ap+(type指针所指向类型大小))的位置 这里题目说要传入一个`char *` 所以实现代码如下

```c
case 'n': {
  const char *null_error = "\nerror! writing through NULL pointer! (%n argument)\n";
  const char *overflow_error = "\nwarning! The value %n argument pointed to has been overflowed!\n";

  char * posp ; //position pointer
  if ((posp = va_arg(ap, char *)) == NULL){
    printfmt(putch,putdat,"%s",null_error);
  }else if(*((unsigned int *)putdat) > 127 ){// or between ' ' to '~'
    printfmt(putch,putdat,"%s",overflow_error);
    *posp = -1;
  }else{
    *posp = *(char *)putdat;
  }
  break;
}
```

#### SJTU Exercise 11

 * Modify the function `printnum()` in `lib/printfmt.c` to support `"%-"` when printing **numbers**. With the directives starting with "%-", the printed number should be `left adjusted`. (i.e., paddings are **on the right side**.) For example, the following function call:

`cprintf("test:[%-5d]", 3)`, should give a result as`"test:[3    ]"`(4 spaces after '3'). Before modifying printnum(), make sure you know what happened in function vprintffmt().

 首先看`printnum`函数 它的接受参数刚刚已经熟悉过了，那么接下来看它的执行方式，递归!,

方法有很多:加全局变量,加外置函数,递归展开,函数内部判断,反转值再顺序输出(注意溢出问题)

```
recursive:
               111111
              2
             3
            4
           5
string:
           12345sssss
```

根据上方递归的访问顺序 和输出的顺序，内部判断需要在第一层 **注意不要影响到其它调用 仅对'-'处理**,在`printnum`函数里原代码前添加以下代码

这里实现的是利用反转再输出,估计速度比原来慢`2~4`倍

```c
  if(padc=='-'){ //only for '-'
    padc = ' ';
    //careful about overflow and value 0
    //ensure that the conversion of the number less than the original one of digit in `base` number system
    unsigned long long reversenum = 0;
    int numlen = 0;
    for(; num >= base; ++numlen , num /= base){
      reversenum *= base ;
      reversenum += num % base;
    }
    // print any needed pad characters after last digit **for overflow**
    putch("0123456789abcdef"[num],putdat);

    // print left number
    for( width -= numlen ; numlen > 0 ; --numlen, reversenum /= base){
      putch("0123456789abcdef"[reversenum % base],putdat);
    }
    // print any needed pad characters after last digit
    while (--width > 0)
      putch(padc, putdat);
    return ;
  }
```

### The Stack

#### Exercise 9

 * 找到kernel初始化stack的地方,和准确的stack在memory中的位置,kernel怎么为stack预留位置? And at which "end" of this reserved area is the stack pointer initialized to point to?

`kern/entry.S`里

```asm
  # Clear the frame pointer register (EBP)
  # so that once we get into debugging C code,
  # stack backtraces will be terminated properly.
  movl  $0x0,%ebp     # nuke frame pointer

  # Set the stack pointer
  movl  $(bootstacktop),%esp
```

`kernel.asm`里

```asm
movl  $(bootstacktop),%esp
f0100034: bc 00 00 11 f0  mov  $0xf0110000,%esp
```

初始的精确位置在 `0xf0110000`. 初始为%ebp=0 ,%esp=下方标签bootstacktop

这里的文档在继续讲ics的栈知识[不了解的请回看ics 的attack lab/buffer lab]，esp指向当前使用的最低处，push会使esp-4,pop会使esp+4 (32bit模式下),然后再讲 函数调用时候的ebp和esp变换

根据以上ics就学过的栈知识，我们要实现一个backtrace功能,利用ebp 和 esp 对调用过程进行回溯

#### Exercise 10

 * 熟悉C的x86调用过程, 在`obj/kern/kernel.asm` 中找到`test_backtrace`函数的地址，设置断点并看 多少东西被 `test_backtrace` push 到了stack上，分别表示什么意思?

```
(gdb) b test_backtrace
(gdb) c
(gdb) i r
ebx            0xf010feee	-267321618
esp            0xf010fecc	0xf010fecc
ebp            0xf010fff8	0xf010fff8
eip            0xf01000e4	0xf01000e4 <test_backtrace>
(gdb) c
(gdb) i r
ebx            0x5	5
esp            0xf010feac	0xf010feac
ebp            0xf010fec8	0xf010fec8
eip            0xf01000e4	0xf01000e4 <test_backtrace>
(gdb) c
(gdb) x/30x $ebp
0xf010fea8:	0xf010fec8	0xf010010d	0x00000004	0x00000005
0xf010feb8:	0x00000000	0xf010feee	0x00010094	0xf010feee
0xf010fec8:	0xf010fff8	0xf010027a	0x00000005	0x00000400
0xf010fed8:	0xfffffc00	0xf010ffee	0x00000000	0x00000000
0xf010fee8:	0x00000000	0x0d0d0000	0x0d0d0d0d	0x0d0d0d0d
0xf010fef8:	0x0d0d0d0d	0x0d0d0d0d	0x0d0d0d0d	0x0d0d0d0d
0xf010ff08:	0x0d0d0d0d	0x0d0d0d0d	0x0d0d0d0d	0x0d0d0d0d
0xf010ff18:	0x0d0d0d0d	0x0d0d0d0d
```

加上阅读`test_backtrace`的asm 可以知道，也就是以两个eip 之间 为一个"循环节"

|栈|寄存器|
|---|---|
|`test_backtrace` 接受到本层参数1 | <- old esp|
|return addr(old eip+4 因为call会放置 call下一条指令的address) ||
|old ebp |<- ebp|
|old ebx||
|函数内临时数据/下一层调用第5个参数||
|函数内临时数据/下一层调用第4个参数||
|函数内临时数据/下一层调用第3个参数||
|函数内临时数据/下一层调用第2个参数||
|函数内临时数据/下一层调用第1个参数|<- esp|
|可能的未来的会放置的eip+4||


#### Exercise11 实现代码`mon_backtrace`函数

作者提供了 `read_ebp`，每一行按给定格式输出ebp,eip以及各个参数,因此,思路为

 * 读取当前ebp 且ebp!=0
 * 输出 ebp 到 ebp+5 (注意指针类型要正确 文档上也有提示)
 * ebp = old ebp 循环

这里要注意的是前半部分到第一个参数的格式(包括内部空格数量)一定要正确,因为测试用的输出匹配.....

 * 为什么backtrace不能知道传递了多少个参数,这个限制如何解决?

因为传参的检查等是由c代码的编译器在编译时判断是否正确和设置push和pop的,在汇编层面没有一个具体的括号 或分界符号

解决方法我能想到的,根据值判断是否为代码段地址, 但有可能判断错,如果传递的参数也是如此的话,更改c编译器的编译过程 会添加一个表示个数的值

#### Exercise 12

 * 让你的backtrace提供 函数名,文件名,eip相对的行号

作者提供了`debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info)` (接受addr 返回info)的部分实现,要添加对`stab_binsearch`的调用实现`debuginfo`

关于`stab_binsearch(const struct Stab *stabs, int *region_left, int *region_right,int type, uintptr_t addr)`的使用见`kern/kdebug.c`中的源代码注释

```c
//	For example, given these N_SO stabs:
//		Index  Type   Address
//		0      SO     f0100000
//		13     SO     f0100040
//		117    SO     f0100176
//		118    SO     f0100178
//		555    SO     f0100652
//		556    SO     f0100654
//		657    SO     f0100849
//	this code:
//		left = 0, right = 657;
//		stab_binsearch(stabs, &left, &right, N_SO, 0xf0100184);
//	will exit setting left = 118, right = 554.
```

接下来在`kern/kdebug.c`中添加,作者也提供了友好的注释

```
// Search within [lline, rline] for the line number stab.
// If found, set info->eip_line to the right line number.
// If not found, return -1.
//
// Hint:
//  There's a particular stabs type used for line numbers.
//  Look at the STABS documentation and <inc/stab.h> to find
//  which one.
// Your code here.
```

并通过搜索`info->`,发现 除了`info->eip_line`作者都已经帮你写好了,并且有以下常量,来自`inc/stab.h`

```c
#define N_SLINE   0x44  // text segment line number
#define N_DSLINE  0x46  // data segment line number
#define N_BSLINE  0x48  // bss segment line number
```

又注意到前部分已经定位到了函数了`lline`和`rline`也不用再改动,这里我们要找的是在`代码段`的数据的行号,所以用`N_SLINE`,实现如下

```c
stab_binsearch(stabs, &lline, &rline, N_SLINE, addr);
if (lline <= rline)
  info->eip_line = stabs[lline].n_desc;
else
  return -1;
```

至此我们有一个完整的`debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info)`,下面开始实现在`mon_backtrace`中输出 这些信息,实现代码如下,其中结构体见`kern/kdebug.h`

```c
struct Eipdebuginfo info;
debuginfo_eip((uintptr_t)eip, &info);
cprintf("         %s:%u %.*s+%u\n",
    info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, eip - (uint32_t)info.eip_fn_addr);
```

至此 MIT的JOS lab1已经完成，学到了

 * 计算机启动的过程
 * 复习了gdb的使用
 * 复习了函数的堆栈
 * 锻炼的代码阅读和查找能力,个人认为更希望大家掌握的如顶部列出的那些命令

---

#### SJTU Exercise 16

 * 利用ics lab的 buffer overflow attack 技术修改 `start_overflow`函数,来引发` do_overflow`函数. 你需要用 above cprintf 函数和前面Exercise实现的`%n` (只有这样你才能得分) 并且要 `do_overflow` 函数正常返回.

 原理和ics lab3一样，利用写也就是`%n`提供的`char*`写功能，来把返回地址写为`do_overflow`即可, 并且根据注释提示可以试用`read_pretaddr`获取指向返回地址的指针 ,也就不用我们自己去根据ebp算了

 设计为`cprintf("%s%n",把do_overflow地址拆分成4个char并类型转换为int,通过read_pretaddr得到的地址偏移)`,这样就可以把`do_overflow`的地址分四个char写入`read_pretaddr`,**注意如果你的对参数`%n`的允许值有范围允许(mit 的要求) 则无法触发 需要改掉**

为了保证还能正常返回,这也是原代码里用了两层函数的原因,我们需要把正确的返回地址保存到原来的地址之上,这样`start_overflow`返回到->`do_overflow`->返回到`overflow_me`的结尾,实现如下

```c
void
start_overflow(void)
{
    char * pret_addr = (char *) read_pretaddr();
    uint32_t overflow_addr = (uint32_t) do_overflow;
    int i;
    for (i = 0; i < 4; ++i)
      cprintf("%*s%n\n", pret_addr[i] & 0xFF, "", pret_addr + 4 + i);
    for (i = 0; i < 4; ++i)
      cprintf("%*s%n\n", (overflow_addr >> (8*i)) & 0xFF, "", pret_addr + i);
}
```

[TODO] 感觉原来给了一个char数组 考虑有没有办法让cprintf输出到本地变量而不是 stdout


吐槽一下`case‘n’`里的判断 因为`grade-lab1`+`kern/init.c`的原因 还不能改成255 只能254,简直了,**差评**

#### SJTU Exercise 17

至此已经`make grade` 90/90了

 * 把backtrace 变成命令

 * 实现一个`time`命令 要求[TODO]

首先怎么变成命令,已知原来的`kerninfo`和`help`,使用grep , 接下来依样画葫芦 的在同样的位置添加backtrace和 time

```
> grep -r "kerninfo" *
monitor.c:	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
monitor.c:mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
monitor.h:int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);
```

写完后可以`make qemu-nox`发现 time和backtrace 不会被判断为`Unknown command`,接下来实现time

文档提示查阅`rdtsc`指令 [搜到一个demo](https://github.com/consen/demo/blob/master/c/syntax/asm/rdtsc.c)

再模拟指令运行,因为还没有fork/exec这种 这里经过阅读代码发现调用runcmd函数,再回看代码定位到`lib/readline.c`

这样我们把 argv 重新组成`char数组`再传给runcmd,代码如下

```c
int
mon_time(int argc, char **argv, struct Trapframe *tf){
  const int BUFLEN = 1024;
  char buf[BUFLEN];
  int bufi=0;
  int i;
  for(i=1;i<argc;i++){
    char * argi =argv[i];
    int j,ch;
    for(j=0,ch=argi[0];ch!='\0';ch=argi[++j]){
      buf[bufi++]=ch;
    }
    if(i == argc-1){
      buf[bufi++]='\n';
      buf[bufi++]='\0';
      break;
    }else{
      buf[bufi++]=' ';
    }
  }

  unsigned long eax, edx;
  __asm__ volatile("rdtsc" : "=a" (eax), "=d" (edx));
  unsigned long long timestart  = ((unsigned long long)eax) | (((unsigned long long)edx) << 32);

  runcmd(buf, NULL);

  __asm__ volatile("rdtsc" : "=a" (eax), "=d" (edx));
  unsigned long long timeend  = ((unsigned long long)eax) | (((unsigned long long)edx) << 32);

  cprintf("kerninfo cycles: %08d\n",timeend-timestart);
  return 0;
}
```

#### 最后

使用`git diff`进行比对,以下是一些建议
 * `--stat` 参数可以显示统计量
 * 配置用vimdiff作为比较工具能分左右两侧进行比对
 * `git log` 可以显示提交历史
 * `git diff <commitid1> <commitid2> -- <filename>` 可以比较两个不同的commit 下指定的文件名

接下来可能会用到的命令
 * `git checkout <commitid> -- <filename or pathname>` 可以把**本地指定文件** 变为指定commitid下的文件
 * `git checkout -b  <branchname>` 可以新建一个名为branchname的分支,如果对操作不熟，担心弄掉本地文件,可以先把本地文件add并commit，再用该命令建立新分支进行整理,出错了 checkout回原分支把新分支删掉即可
 * `git branch --move <oldbranchname> <newbranchname>` 可以调整branchname

以下操作**请勿**影响公共提交内容
 * `git reset <commitid> --soft` 可以把**git的记录**设置到指定提交历史,不会改变**本地文件**
 * `git rebase -i HEAD~4` 可以对历史4个提交进行整理，用来整理自己的提交
 * `git brancd -D <branchname>` 删除分支

整理完代码以后使用commit提交即可,push到origin是没权限的哦
 * `git remote add <newlocalmirrorepositoryname> <remoterepositoryuri>` 如果你要再同步到远端你的某个仓库可以使用改命令添加
 * `git push <newlocalmirrorepositoryname> <branchname>` 使用该命令push

### 总结

 * 总的来说 关于这个lab和标题真正扣题的在于 前面的阅读+gdb 以及 最后的stack
 * 中间的关系对于任务来说不是太紧密,更多的是实现一个c代码,还是看的c代码能力,但也同时让同学了解到print VGA等 都是靠一行行代码实现的
 * 所以根据以上,个人认为完成代码的意义远小于阅读代码的意义
 * 结构看图能更快理解和回想
 * 对于快速完成lab , ~~根据grade 一心都在grep 判分 ,可以直接在monitor里加点printf也就拿满了~~ :-)

## 参考资料
 * [how-computer-pc-boots-up](https://www.engineersgarage.com/tutorials/how-computer-pc-boots-up)
 * [What happens when you switch on a computer?](http://www.tldp.org/HOWTO/Unix-and-Internet-Fundamentals-HOWTO/bootup.html)
 * [PCI memory hole](https://en.wikipedia.org/wiki/PCI_hole)
 * [real mode](https://en.wikipedia.org/wiki/Real_mode)
 * [protected mode](https://en.wikipedia.org/wiki/Protected_mode)
 * [real mode vs protected mode](http://www.geek.com/chips/difference-between-real-mode-and-protected-mode-574665/)
 * [A20 Line](https://en.wikipedia.org/wiki/A20_line)
 * [A20相关讲解](https://www.zhihu.com/question/29375534)
 * [A20检测](https://gist.github.com/sakamoto-poteko/0d50af2d9eb78f71c74f)
 * [Disk sector](https://en.wikipedia.org/wiki/Disk_sector)
 * [Control Register](https://en.wikipedia.org/wiki/Control_register)
 * [ASCII - Printable characters](https://en.wikipedia.org/wiki/ASCII)
 * [Clann24 lab1 exercise](https://github.com/Clann24/jos/blob/master/lab1/README.md)
 * [get cpu cycle count with rdtsc](http://stackoverflow.com/questions/13772567/get-cpu-cycle-count)
 * [rdtsc 32bit vs 64bit](http://stackoverflow.com/questions/17401914/why-should-i-use-rdtsc-diferently-on-x86-and-x86-x64)

# For SJTU JOS LAB1
---

SJTU的额外要求

需要文档！文档越详细分越多

```
ftp: ftp://public.sjtu.edu.cn/upload/lab1/
user: Dd_nirvana
password: public
```

可以直接使用`linux`命令访问ftp提交作业 example:

```shell
>ftp public.sjtu.edu.cn
ftp> cd upload/lab1
ftp> pwd
/upload/lab1
ftp> !
>ls
5130379000.tar.gz . ..
>exit
ftp> put 5130379000.tar.gz 5130379000.tar.gz
local: 5130379000.tar.gz remote: 5130379000.tar.gz
200 PORT command successful
150 Opening BINARY mode data connection for 5130379000.tar.gz
226 Transfer complete
138003488 bytes sent in 0.01 secs (1.4 GB/s)
ftp>exit
221 Goodbye.
```

不需要回答lab里的问题，因为老师会当面问:-)真是刺激呢,但lab里的问题会帮助你理解lab.

# 修复`make gdb`并不懂为什么老师给的 GNUmakefile里没有gdb了

在GNUmakefile 里`qemu:`前加上

```
gdb:
    gdb -n -x .gdbinit
```

# OLD COMMIT LOG

```
commit d32fbfa0c51897c04494fc8e04045a72d58c58fe
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Thu Mar 16 21:24:19 2017 +0800

    SJTU OS LAB1 FINISH

commit 1ee72289e3c9ab73d0ddc98e83e808e3a6af4bc9
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Mon Mar 13 10:09:25 2017 -0700

    update files

commit 516823681961e19846055e5511124c12c86e502a
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Mon Mar 13 09:59:40 2017 -0700

    mit jos lab1 (50/50)

commit ba159d267f88a0f0551896de27f92ce6117739ac
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Mon Mar 13 23:24:22 2017 +0800

    finish lab1 except time

commit d1789ed43d9ad90bfd02a66f39e9703f0263201e
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Mon Mar 13 19:42:23 2017 +0800

    70/90

commit 857f483543c1fe8c6754b6bea22ae99426365285
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Mon Feb 27 23:19:07 2017 -0800

    first readme

commit 30f38c2ba0023d2179c6a7f1921d14436d36d507
```
