lab4 Preemptive Multitasking
---

lab4 抢占式多任务

# 总览

[官网](https://pdos.csail.mit.edu/6.828/2016/labs/lab4/)

你将在本lab中实现一个用户态 多个同时运行的程序 的抢占式多任务

* part A 需要支持多处理器/多核, 实现round-robin循环调度, 和一个几班的 环境 管理系统调用 (create/destroy 环境 allocate 内存)

* part B 需要实现一个类似unix的fork(),可以让用户态的程序 产生一个环境自拷贝

* part C 需要支持  inter-process communication (IPC), 允许 不同的用户态程序相互 交流/同步. 你还需要支持 硬件钟的中断抢占

测试`make grade`,提交打包`make handin`

```
ftp: public.sjtu.edu.cn/upload/os2017/lab4/
user: georeth
password: public
```

# 准备

代码

```bash
>git fetch --all
>git checkout -b lab4 origin/lab4
>git checkout -b mylab4
>git merge mylab3 --no-commit
>vim <the files that conflict>
>git add .
>git commit -m "manual merge mylab3 to lab4"
```

---

【ONLY FOR SJTU START】

环境：为lab4安装并配置新的qemu ,我这里采取 不影响其它用户的方式 安装到用户的dist下 所以不需要sudo[当然jos虚拟机里你想怎么玩都行]

```bash
> sudo apt-get install build-essential autoconf libtool tree -y
> cd ~
> wget http://ipads.se.sjtu.edu.cn/courses/os/2015/tools/qemu-1.5.2.tar.bz2
> tar xf qemu-1.5.2.tar.bz2 && cd qemu-1.5.2 && pwd
/home/oslab/qemu-1.5.2
> ./configure --prefix=/home/oslab/qemu-1.5.2/dist --target-list="i386-softmmu"
> make && make install
> tree ~/qemu-1.5.2/dist
```

可以看到 我们通过下载源码  配置安装位置 编译并安装 产生了`/home/oslab/qemu-1.5.2/dist/bin/qemu-system-i386`

所以修改jos项目中`conf/env.mk`中的QEMU配置为`QEMU=/home/oslab/qemu-1.5.2/dist/bin/qemu-system-i386` [注意 在提交时不应提交该配置的修改？？]

【ONLY FOR SJTU END】

---

lab4 的新文件

|file|description|
|---|---|
|`kern/cpu.h`|  Kernel-private definitions for multiprocessor support|
|`kern/mpconfig.c`|  Code to read the multiprocessor configuration|
|`kern/lapic.c`|  Kernel code driving the local APIC unit in each processor|
|`kern/mpentry.S`|  Assembly-language entry code for non-boot CPUs|
|`kern/spinlock.h`|  Kernel-private definitions for spin locks, including the big kernel lock|
|`kern/spinlock.c`|  Kernel code implementing spin locks|
|`kern/sched.c`|  Code skeleton of the scheduler that you are about to implement|

# Part A: Multiprocessor Support and Cooperative Multitasking

在lab的第一部分 需要扩展jos的功能,让它能在多核处理器上运行,然后 实现一些新的 jos 内核的系统调用,来允许用户级别 创建新的 环境.你将实现一个合作循环调度.允许内核 在不同的环境中切换,如果当前的环境自愿放弃CPU/或结束运行.在之后的Part C将会实现抢占方式调度,能让内核 在一个确定的时间间隔 回CPU的使用权.

## Multiprocessor Support

我们将让jos系统 支持对称多核处理SMP,它是一个所有CPU有相同的系统资源访问权的多核模型.虽然所有CPU在SMP中 功能相同,但在boot 阶段分为两个 BSP(引导的处理器 用来初始化引导系统 )和APs(应用程序处理器 它们被BSP激活/唤醒 在操作系统运行好后 ),具体哪一个是BSP由硬件和BIOS共同决定,至此 你的所有已有的
JOS代码都是在BSP上运行的

在SMP系统中, 每一个CPU中都有一个APIC(LAPIC)单元,LAPIC单元为系统提供中断. LAPIC也为其连接CPU提供了一个独特的标识符. 这个lab中,我们使用`kern/lapic.c`提供的LAPIC函数

* `cpunum()` 读取LAPIC identifier (APIC ID) 来告诉代码当前运行的CPU.
* `lapic_startap()`从BSP向APs发送STARTUP interprocessor interrupt (IPI)来 启动其它的CPU.
* `apic_init()` 在 part C, 我们的LAPIC's 的内置时钟 触发 clock interrupts 来支持 抢占式多进程.

一个处理器用memory-mapped I/O (MMIO)来访问它的LAPIC. 在MMIO,一些I/O设备的物理内存和寄存器采用硬接线的方式, 因此用于访内存的load/store指令同样可以用来访问设备寄存器.你已经看到了物理地址0xA00000上的IO hole (we use this to write to the CGA display buffer). LAPIC hole 起始于物理地址0xFE000000(32MB of 4GB)的位置, 它用普通的direct map 到kernbase来访问  is too high . 因此 在这个lab 我们调整 JOS的内存结构来 映射 kernel virtual address space的顶部32MB空间, 从IOMEMBASE (0xFE000000)开始到 IO hole containing the LAPIC. 因为从物理地址0xFE000000开始所以它是一个标示映射. 我们已经 在`kern/pmap.c`中的`mem_init_mp()`函数中给你创建好了,并更新了`inc/memlayout.h`和 JOS VM handout to illustrate the change.

---

The JOS virtual memory map leaves a 4MB gap at MMIOBASE so we have a place to map devices like this. Since later labs introduce more MMIO regions, you'll write a simple function to allocate space from this region and map device memory to it.

## MIT Exercise 1.

* 实现`kern/pmap.c`中的`mmio_map_region` .可以看`kern/lapic.c:lapic_init`的对它的调用.

`kern/lapic.c:lapic_init`中对该函数的调用为

```c
// lapicaddr is the physical address of the LAPIC's 4K MMIO 
// region.  Map it in to virtual memory so we can access it.
lapic = mmio_map_region(lapicaddr, 4096);    
```

实现如下

```c
size = ROUNDUP(size+PGOFF(pa), PGSIZE);
pa   = ROUNDDOWN(pa, PGSIZE);
if(base + size >= MMIOLIM)
  panic("mmio_map_region overflow MMIOLIM");
boot_map_region(kern_pgdir, base, size, pa, PTE_PCD|PTE_PWT|PTE_W);
base += size;
return (void *)(base - size);
```

其中需要注意 调用者希望 映射物理地址`[pa,pa+size)`,做了对齐以后是`[ROUNDDOWN(pa),ROUNDUP(pa+size))`,所以`size=ROUNDUP(pa+size)-ROUNDDOWN(pa)=ROUNDUP(size+PGOFF(pa), PGSIZE)`

## Application Processor Bootstrap

在启动APs前, BSP应当先手机 多核系统的信息 比如CPU的数量 他们的APIC IDs 以及 LAPIC单元的MMIO 地址. `kern/mpconfig.c`中的`mp_init()`函数 通过读取MP配置表(在BIOS中的)获得了这些信息

`kern/init.c`中的`boot_aps()`函数 运行AP bootstrap 进程. APs 以实模式开始执行,和 bootloader started in boot/boot.S相似, 因此`boot_aps()` 复制 AP entry code (kern/mpentry.S) 到一个实模式下addressable 的内存位置. 和bootloader不同的是 我们需要控制 AP的起始执行代码; 我们复制 entry code 到`0x7000 (MPENTRY_PADDR)` 在640KB下方 未使用的位置.

拷贝完后`boot_aps()`通过发送STARTUP IPIs给每个AP绑定的LAPIC单元 逐个唤醒APs, 同时还发送给每个LAPIC (CS:IP)地址来告诉AP应该的初始代码运行入口(在我们的例子中是`MPENTRY_PADDR`). `kern/mpentry.S`中的入口代码 和 `boot/boot.S`的入口代码很相似. 在一些简短的配置后, 它把AP从 实模式 切换到 保护模式 通过启用页表, 然后调用`kern/init.c`的`mp_main()`. `boot_aps()`等待AP在`Cpu`结构`cpu_status`产生`CPU_STARTED` flag信号后 再唤醒下一个AP.

# Exercise 1.

* 阅读`kern/init.c`中的`boot_aps()`+`mp_main()` 以及`kern/mpentry.S`. 理解APs的bootstrap的流程. 然后 修改`kern/pmap.c`中的`page_init()` 避免 把`MPENTRY_PADDR`加到free list, 这样我们可以安全的复制并运行AP bootstrap 代码在那一块物理地址. 至此你的代码应当通过`check_page_free_list()`测试, 但可能`check_kern_pgdir()`测试失败.

先看`kern/lapic.c`的代码 一堆宏 一个全局变量lapic 指针,几个函数,3个上面已经给出功能说明

再看`kern/init.c` 通过`git vimdiff mylab3 kern/init.c`可以看到新增的变化,多了函数`boot_aps`,`mp_main`,`spinlock_test`以及一些辅助变量,有些输出函数,再有就是`i386_init`函数的内部增加更多的调用
 * `mp_init` 来自`kern/mpconfig.c` 初始化了一些信息,目测是创建了一些结构体对所有cpu做了一个映射
 * `lapic_init` 紧接着`mp_init`的 它初始化了 当前的lapic的结构
 * `pic_init`  Initialize the 8259A interrupt controllers.
 * 需要加kernel lock
 * `boot_aps`先把 kern/mpentry.S中的`mpentry_start`到`mpentry_end`复制到`KADDR(MPENTRY_PADDR)` 然后 循环映射的cpu数组 找未启动的cpu 通过函数`lapic_startap`来逐个唤醒
 * `kern/mpentry.S`初始化一堆难用c++初始化的硬件信息 然后调用`mp_main`
 * `mp_main` 每个cpu的独自的事了

开始修改`kern/pmap.c`中的`page_init`

```c
// LAB 4:
// Change your code to mark the physical page at MPENTRY_PADDR
// as in use
```

这里认为 我们的`mpentry_start`到`mpentry_end`的 也就要复制的代码大小不会超过一页,也就是说 我们只需要把一页分配标识为已使用即可,如果通过计算结尾位置判断究竟占了多少页会更安全,这里我实现还是把它当作只有一页去标记,实现如下

```c
// 2)
for (; i < npages_basemem; i++) {
  if (i == MPENTRY_PADDR / PGSIZE) {
    pages[i].pp_ref = 1;
    pages[i].pp_link = NULL;
    continue;
  }
  pages[i].pp_ref = 0;
  pages[i].pp_link = page_free_list;
  page_free_list = &pages[i];
}
```

`make qemu-nox`可以看到

```
check_page_alloc() succeeded!
check_page() succeeded!
```

## Question

Compare kern/mpentry.S side by side with boot/boot.S. Bearing in mind that kern/mpentry.S is compiled and linked to run above KERNBASE just like everything else in the kernel, what is the purpose of macro MPBOOTPHYS? Why is it necessary in kern/mpentry.S but not in boot/boot.S? In other words, what could go wrong if it were omitted in kern/mpentry.S?

Hint: recall the differences between the link address and the load address that we have discussed in Lab 1.

代码的注释中已经写了

 * 它不需要启用A20
 * 它需要用MPBOOTPHYS来计算它的符号中的gdt的绝对地址,而不是用linker来计算,因为在这个时候 BSP上的页模式已经开启,但它自己的页模式并未开启所以对它来说 还只能使用物理地址

## Per-CPU State and Initialization

当写一个多核 OS, 很重要的是 区分 per-CPU 的状态对其它处理器是私有的, 全局状态是整个系统共同分享的. `kern/cpu.h`定义了常见的`per-CPU`状态,包括`struct Cpu`, 用来储存per-CPU 状态变量. cpunum() 总是返回 调用该函数的CPU的ID, 这个ID可以用于在cpus数组的index. 除此, 宏thiscpu可以快速得到当前的CPU的`struct Cpu`

下面是你需要在意的 per-CPU 状态:

 * Per-CPU kernel stack. 因为多个CPU可以同时的trap进内核态, 我们需要为每一个CPU分化kernel stack 来保证它们不会相互干扰执行.数组`percpu_kstacks[NCPU][KSTKSIZE]` 保存了每个kernel stacks的空间. 在 Lab 2中 已经把bootstack指向的物理地址映射到了BSP's kernel stack just below KSTACKTOP. 类似的,在这个lab你将映射每一个CPU's的内核栈 到这个区域which guard pages acting as a buffer between them. CPU 0's 的栈依然从KSTACKTOP向下增长; CPU 1's 的栈将从CPU 0's的底部下方 KSTKGAP 字节的位置开始, 结构图见`inc/memlayout.h`.
 * Per-CPU TSS and TSS descriptor. per-CPU 的task state segment (TSS) 也需要 放在每一个CPU对应的位置. CPU i 的TSS的位置保存在`cpus[i].cpu_ts`中,corresponding TSS descriptor 在GDT入口的`gdt[(GD_TSS0 >> 3) + i]` 位置定义. 在`kern/trap.c`中定义的全局`ts variable`不再有用
 * Per-CPU current environment pointer. 每一个CPU可以同时运行不同的用户进程, 我们重定义 curenv来指向`cpus[cpunum()].cpu_env` (或`thiscpu->cpu_env`), 它指向在当前的CPU上的 执行的当前环境(the CPU on which the code is running).
 * Per-CPU system registers. 所有寄存器,包括系统寄存器 对于CPU都是私有的.因此 指令初始化这些寄存器, 例如lcr3(), ltr(), lgdt(), lidt(), etc.,需要在不同的CPU上执行一次. `env_init_percpu()`和`trap_init_percpu()`函数即是用来做这些的.
 * Per-CPU idle environment. JOS 需用空闲环境作为回退,如果没有足够的正常的环境去跑.然而在一个时间点一个环境只会在一个CPU上跑. 虽然多个CPUs在一个时间点会都处于空闲. 我们给每一个CPU创建一个空闲的环境.根据约定 envs[cpunum()] 是当前cpu的 空闲环境.

## Exercise 2.

编辑`kern/pmap.c`中的`mem_init_mp()` 映射per-CPU 的栈的起始点按照`inc/memlayout.h`中所画的指向KSTACKTOP 下方. 每一个stack的大小是KSTKSIZE字节+KSTKGAP字节(没有映射的保护页). 你的代码需要通过新的`check_kern_pgdir()`检测.

```c
//KSTACKTOP ---->  +------------------------------+ 0xefc00000      --+
//                 |     CPU0's Kernel Stack      | RW/--  KSTKSIZE   |
//                 | - - - - - - - - - - - - - - -|                   |
//                 |      Invalid Memory (*)      | --/--  KSTKGAP    |
//                 +------------------------------+                   |
//                 |     CPU1's Kernel Stack      | RW/--  KSTKSIZE   |
//                 | - - - - - - - - - - - - - - -|                 PTSIZE
//                 |      Invalid Memory (*)      | --/--  KSTKGAP    |
//                 +------------------------------+                   |
//                 :              .               :                   |
//                 :              .               :                   |
```

`inc/memlayout.h`的所说的该部分结构如上,再根据之前我们实现的映射代码,发现这里也是一整块的线性映射即可,也就是for 一遍每个CPU做映射 也就是

虚拟地址`KSTACKTOP-i*(KSTKSIZE+KSTKGAP)+[KSTKGAP,KSTKGAP+KSTKSIZE)`映射到物理地址`percpu_kstacks[i][KSTKSIZE]`实现如下

```c
int i;
for (i = 0; i < NCPU; ++i)
  boot_map_region(kern_pgdir, KSTACKTOP - i * (KSTKSIZE + KSTKGAP) - KSTKSIZE, KSTKSIZE, PADDR(percpu_kstacks[i]), PTE_W);
```

这样做,而不是把所有的虚拟地址都指向KSTACKTOP(也就是该结构各个CPU不对称),1是因为文档以及既有代码要我们这样做,2是 它们同为kern mode下的,我们在kern下 就算不同CPU 目前使用的是同一个`kern_pgdir`如果设计上 不同的cpu再用不同的`kern_pgdir`应该也可以做成虚拟全都KSTACKTOP的样子

这里如果没有KSTKGAP这一段,那么对于kernel mode下 将变成连续可写地址,这样当一个stack超界限时 要么需要额外检查 要么出现错误,其修改代码成本较大(😕其实还是因为设计文档是要我们这样 和 额外检查相比这种方法也的确很机制方便)

**然后`make qemu-nox`竟然没有过!!**

通过cprintf定位到了bug所在——在sjtu的lab历史中有一个叫`boot_map_region_large`的东西 它把整个4MB做了映射,而现在我们的`mem_init_mp`函数 要`boot_map_region(kern_pgdir, IOMEMBASE, -IOMEMBASE, IOMEM_PADDR, PTE_W);` 然后因为一个走pdir另一个走pdir+pt,然后我的处理没有做得那么细致😕 (也就是 在替换时检查类型,我都是pdir替换pdir,pdir+pt替换pdir+pt,而这里要pdir+pt替换pdir)于是就GG了,我这里采用最简洁的改动,把`boot_map_region_large(kern_pgdir,KERNBASE          , -KERNBASE, 0               , PTE_W);`的`_large`去掉 即改回4K页映射,再`make qemu-nox`则可以看到

```
check_kern_pgdir() succeeded!
check_page_installed_pgdir() succeeded!
```

至此我们完成了上面所说要在意的第一条里的内容

In addition to this, if you have added any extra per-CPU state or performed any additional CPU-specific initialization (by say, setting new bits in the CPU registers) in your solutions to challenge problems in earlier labs, be sure to replicate them on each CPU here!

## Exercise 3.

`kern/trap.c` 中的`trap_init_percpu()`函数 初始化了BSP的 TSS and TSS descriptor for the BSP. 它在lab3是可以工作的,但在其它CPU上不能工作. 修改代码让它在所有CPU上都能工作(note: 你的新的代码 不应该 再使用全局的ts变量.),,,,这不就是上面的第二条的内容么,也就是

 * `cpus[i].cpu_ts = &gdt[(GD_TSS0 >> 3) + i]`
 * `修改gdt[(GD_TSS0 >> 3) + i]的项 内核栈位置指向Exercise 2中我们新映射的栈`
 * 以前调用ts的全改成`thiscpu->cpu_ts`

需要注意的是`trap_init_percpu`的执行时间是在BSP唤醒 APs后 也就是说这个函数又AP自己执行而不BSP和上一个Exercise不同,所以 我们的第二条只需要对当前的进行配置即可

根据已有的代码+注释 一行一行修改即可(其中 有关数据的esp采取每个CPU一个,而代码ss0采取大家公用),完成代码如下 (感谢sjtu 上一个lab的`evilhello2.c`让我提前感受了 gdt的index)

```c
void
trap_init_percpu(void)
{
    // Setup a TSS so that we get the right stack
    // when we trap to the kernel.
    int index = thiscpu->cpu_id;
    thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - index * (KSTKSIZE + KSTKGAP);
    thiscpu->cpu_ts.ts_ss0  = GD_KD;

    // Initialize the TSS slot of the gdt.
    int GD_TSSi = GD_TSS0 + (index << 3);
    gdt[GD_TSSi >> 3] = SEG16(STS_T32A, (uint32_t) (&(thiscpu->cpu_ts)),
        sizeof(struct Taskstate), 0);
    gdt[GD_TSSi >> 3].sd_s = 0;

    // Load the TSS selector (like other segment selectors, the
    // bottom three bits are special; we leave them 0)
    ltr(GD_TSSi);

    // Load the IDT
    lidt(&idt_pd);
}
```

[MIT 的这里 多减了一个1 以及多了一个`ts_tomb`项]

```
int index = thiscpu->cpu_id;
thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - index * (KSTKSIZE + KSTKGAP);
thiscpu->cpu_ts.ts_ss0  = GD_KD;                                  
thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate);               

// Initialize the TSS slot of the gdt.
int GD_TSSi = GD_TSS0 + (index << 3);                             
gdt[GD_TSSi >> 3] = SEG16(STS_T32A, (uint32_t) (&(thiscpu->cpu_ts)),
    sizeof(struct Taskstate) - 1, 0);
gdt[GD_TSSi >> 3].sd_s = 0;

// Load the TSS selector (like other segment selectors, the       
// bottom three bits are special; we leave them 0)
ltr(GD_TSSi);
```

然后`make qemu-nox CPUS=4` 得到了如文档所说的输出

## Locking

我们现有的代码在AP的`mp_main()`初始化后自循环了. 在让AP做更多事情前,我们需要先解决 多核同时运行的竞争状态. 最简单的实现方法是用一个大的kernel lock. 一个大的kernel lock是一个全局锁 当环境进入内核态时打开, 在从kernel退出到用户态时释放. 在这种模型下, 用户态环境可以同时运行在多个CPUs上,但是 同一时间最多一个在内核态; 其它需要进入内核态的需要等待.

`kern/spinlock.h`声明了一个大的 kernel lock, 命名为`kernel_lock`.它还提供`lock_kernel()`和`unlock_kernel()`来方便的申请和释放锁 你需要在下面这4个地方添加
 * `init.c i386_init()` 在`BSP wakes up the other CPUs`时 申请锁
 * `init.c mp_main()`  在初始化AP后申请锁 并调用`sched_yield()`来开始在该AP上运行 环境.
 * `trap.c trap()` 当从user mode trap申请锁. 通过`tf_cs`来检测当前处于用户态还是内核态.
 * `env.c env_run()` 在切换到用户态前 的最后时刻 释放锁.不要释放得过早 或 过晚,否则你可能遇到 资源竞争或者死锁.

## Exercise 4.

在上述提到的地方使用锁通过`lock_kernel()`和`unlock_kernel()`函数.
 * 加上`lock_kernel();`
 * 加上`lock_kernel();sched_yield();`
 * 加上`lock_kernel();`
 * 在`env_pop_tf();`前加上`unlock_kernel();`

实现以后`make qemu-nox CPUS=4` 发现 出现了general protection,,,,然后 发现又是sjtu的lab的贡献😕 真是令人惊喜呢......[虽然文档说在该部分还不能测试正确性 但至少发现了一个错误]

原因是`lab3的sysenter` 它不走idt而是通过它的wrmsr来 配置, 其中`trap_init`的 sysenter服务的几行删掉并在刚刚完成的`trap_init_percpu`中添加以下代码

```c
extern void sysenter_handler();
wrmsr(0x174, GD_KT, 0);                   /* SYSENTER_CS_MSR */
wrmsr(0x175, thiscpu->cpu_ts.ts_esp0 , 0);/* SYSENTER_ESP_MSR */
wrmsr(0x176, sysenter_handler, 0);        /* SYSENTER_EIP_MSR */
```

> Ticket spinlock is a FIFO spinlock that can avoid starving. The ticket spinlock has two fields, one is owner's ticket number, the other one is next ticket to be taken. When acquiring a lock, read the next ticket and increase it with an atomic instruction. Then wait until the owner's ticket equals to the read one. When releasing the lock, increase the owner's ticket number to give it to the next waiter.

讲了实现原理 而我们只需要知道在`lock_kernel`和`unlock_kernel`之间的代码有原子性

sysenter 不会走trap 需要再加锁吗？

## Exercise 4.1.

实现`kern/spinlock.c`中的spinlock. 你可以定义一个宏`USE_TICKET_SPIN_LOCK` 在`kern/spinlock.h`的开始的位置来让它工作. 在你正确的实现了ticket spinlock 并定义了宏 ,应当能通过`spinlock_test()`. 在你完成所有代码以前不要使用`ticket spinlock`...因为它低效率 超级慢....

好 我收回上面的话 还是要看实现原理,有两个项(拥有者的值A和下一个可以被取得的值B),当申请时,读取B并原子的B++再等待A=B,当释放时,A++

搜了一下有4个LAB 4的注释分别实现
 * `holding()` 里的 用于判断当前CPU是否持有该锁`return lock->own != lock->next && lock->cpu == thiscpu;`
 * `__spin_initlock()` 里的 用初始化锁`lk->own = lk->next = 0;`
 * `spin_lock()` 里的 用于申请锁

```c
unsigned thisticket = atomic_return_and_add(&(lk->next), 1);
while ( thisticket != lk->own )
asm volatile ("pause");
```

 * `spin_unlock()`里的 用于释放锁`atomic_return_and_add(&(lk->own), 1);`

最后在`kern/spinlock.h`中去掉`#define USE_TICKET_SPIN_LOCK`的注释 再`make qemu-nox CPUS=4` **然后卡住了😿？？？？**

试了半天把申请锁的`lk->own`改为 `atomic_return_and_add(&(lk->own), 0)`然后可以运行😿！！？？ `lk->own`竟然不够原子,通过`make grade CPUS=4`得到输出

```
spinlock_test() succeeded on CPU 1!
spinlock_test() succeeded on CPU 2!
spinlock_test() succeeded on CPU 3!
spinlock_test() succeeded on CPU 0!
```

至少说明对了,**我们重新注释掉我们的`#define`**

## Question

It seems that using the big kernel lock guarantees that only one CPU can run the kernel code at a time. Why do we still need separate kernel stacks for each CPU? Describe a scenario in which using a shared kernel stack will go wrong, even with the protection of the big kernel lock.

比如CPUA 和 CPUB 都在执行用户态程序,然后CPUA进入内核态 加锁 用内核栈,这时CPUB上产生了中断,硬件 会push一些参数到内核栈上,哦豁 因为两个指向同一个位置,导致可能复写了CPUA正在使用的一些信息,而这时候 硬件并不会去检查锁,所以要想这样搞 需要再改改硬件😿

```c
/* TODO
Challenge! The big kernel lock is simple and easy to use. Nevertheless, it eliminates all concurrency in kernel mode. Most modern operating systems use different locks to protect different parts of their shared state, an approach called fine-grained locking. Fine-grained locking can increase performance significantly, but is more difficult to implement and error-prone. If you are brave enough, drop the big kernel lock and embrace concurrency in JOS!

It is up to you to decide the locking granularity (the amount of data that a lock protects). As a hint, you may consider using spin locks to ensure exclusive access to these shared components in the JOS kernel:

The page allocator.
The console driver.
The scheduler.
The inter-process communication (IPC) state that you will implement in the part C.
*/
```

## Round-Robin Scheduling

你的下一个任务是 改变jos内核让它不总是只运行 idle 环境,而是可以交替的在多个环境中轮循

* 和之前提到的一样,开始NCPU环境一直是特殊 idle 环境. 它们总是运行 user/idle程序,这种方式 简单的浪费时间,如果处理器没有别的事情做 它会一直尝试把CPU给另一个环境,阅读代码`user/idle.c`,我们已经修改了`kern/init.c`的部分代码,来让你创建这些特殊的 idle环境 从envs[0]到envs[NCPU-1],在你第一次真正创建envs[NCPU]之前
* `kern/sched.c`中的函数`sched_yield()` 意味着选一个新的环境来运行. 它在envs 数组中从上一次搜索的末尾逐个循环的搜索,选择第一个`ENV_RUNNABLE` (see inc/env.h)的环境 并调用`env_run()`去运行它. 然而`sched_yield()` 是一个特殊的空闲环境,如果没有可运行的环境 它永远也不会选出一个
* `sched_yield()`也不应 在一个时间点让两个CPU运行同一个环境. 它可以从`ENV_RUNNING`得知一个环境正在运行.
* 作者已经实现了`sys_yield()`,用户可以调用它来 调用内核的`sched_yield()` 然后资源的放弃CPU到一个不同的环境. As you can see in user/idle.c, the idle environment does this routinely.
* Whenever the kernel switches from one environment to another, it must ensure the old environment's registers are saved so they can be restored properly later. Why? Where does this happen?

## Exercise 5.

在`sched_yield()`中实现上述的循环调度. 别忘了 修改syscall来分发`sys_yield()`

修改`kern/init.c` 创建>=3个运行`user/yield.c`的环境. You should see the environments switch back and forth between each other five times before terminating, like this:

```
Hello, I am environment 00001008.
Hello, I am environment 00001009.
Hello, I am environment 0000100a.
Back in environment 00001008, iteration 0.
Back in environment 00001009, iteration 0.
Back in environment 0000100a, iteration 0.
Back in environment 00001008, iteration 1.
Back in environment 00001009, iteration 1.
Back in environment 0000100a, iteration 1.
```

在yield programs退出后, 只有idle environments 可运行的时候, 调度器应当调用jos的monitor.

注意：现在有两种机制可以进入内核态: sysenter and int 0x80, which are both used in jos. Be careful they are different when entering and exiting kernel because they handle kernel stack in different ways. You need to handle these two methodes rightly and carefully when jos schedules a new environment using `env_run_tf()`.

看`kern/sched.c` 发现忘记了`kern/env.c:env_run()`具体干什么 和`envs`的初始化,看了一下`env_run`的实现里我有写把原来的置为runnable并且它是无返回的函数,

然后已有的代码 貌似实现了找 空闲 以及无空闲的处理,不过并没理解 上面通过i来找 下面却idle = &envs[cpunum()]; (目测是文档中`Per-CPU idle environment. `所说的)

再看env区别 runnable 首先要看 它的`env_type`是否是`ENV_TYPE_USER`即当前有程序 再要看`env_status` [TODO 关于这里 我们每次释放环境后 需要把`env_type`置为`ENV_TYPE_IDLE`吗？感觉只是 置为free也行？]

在`kern/init.c:i386_init`中找到了用`ENV_CREATE`的初始化

通过回看代码 发现了以前不需要但现在需要的修改,先说原因
 * envs数组是通过`kern/pmap.c:mem_init()`中申请的未初始化
 * 紧接着 在`kern/env.c:env_init`中加入了`env_free_list`链表
 * 之前的申请调用 都是通过 `env_create/ENV_CREATE`来进行的 见`kern/init.c`
 * 这里我们要遍历数组 判断是否被使用,那么没有被create的 依然在`env_free_list`中也在envs数组中 它们没有被初始化`env_status/env_type项`, 所以我们应当 在之前的`env_init()`中对 其中一个项进行初始化 这里我选择`env_type`,也就是把`env_status`视为`env_type!=ENV_TYPE_IDLE`时的副属性 运用短路运算

修改`kern/env.c:env_init`如下

```c
void
env_init(void)
{
  // Set up envs array
  int i;
  for (i = NENV - 1 ; i >= 0 ; --i ) {
    envs[i].env_link = env_free_list;
    envs[i].env_type = ENV_TYPE_IDLE;
    env_free_list = &envs[i];
  }
  // Per-CPU part of the initialization
  env_init_percpu();
}
```

**MIT的lab里**我用的`envs[i].env_status = ENV_FREE;`来标识

根据`kern/init.c:i386_init`理解设计envs数组为

* `[0~NCPU-1]`     不在freelist中 永远idle
* `[NCPU~NENVS-1]` 用户申请 有的在有的不在freelist中 idle或user `env_states`都有可能

不过我不是很明确这是这个lab的设计还是jos的设计😿所以 这里采用`0~NENVS-1`的循环 而不是`NCPU~NENVS-1`的循环来检测,在`kern/sched.c:sched_yield`中原来注释的地方添加代码如下

```c
envid_t env_id = curenv == NULL ? 0 : ENVX(curenv->env_id);
for(i = (env_id + 1) % NENV; i != env_id; i = (i + 1) % NENV){
  if(envs[i].env_type != ENV_TYPE_IDLE && envs[i].env_status == ENV_RUNNABLE) {
    env_run(&envs[i]);
  }
}
if(curenv && curenv->env_type != ENV_TYPE_IDLE && curenv->env_status == ENV_RUNNING){
  env_run(curenv);
}
```

**MIT lab** 因为没有`ENV_TYPE_IDLE` 而是通过上面用`ENV_FREE`来标识的，这一段实现为

```c
int i;                  
if(!curenv){            
  for(i = 0 ; i < NENV; i++)
    if(envs[i].env_status == ENV_RUNNABLE) 
      env_run(&envs[i]);
}else{                  
  envid_t env_id = ENVX(curenv->env_id);
  for(i = (env_id + 1) % NENV; i != env_id; i = (i + 1) % NENV)
    if(envs[i].env_status == ENV_RUNNABLE)
      env_run(&envs[i]);
  if(curenv->env_status == ENV_RUNNING)
    env_run(curenv);    
}       
```

调了半天以为哪里写错了 结果看到MIT:If you use CPUS=1 at this point, all environments should successfully run. Setting CPUS larger than 1 at this time may result in a general protection fault, kernel page fault, or other unexpected interrupt once there are no more runnable environments due to unhandled timer interrupts (which we will fix below!). 

而MIT的代码中并没有`0~NCPU-1`和`NCPU~NENVS-1`这样的设计,也就是说 第一次运行的时候或者从无curenv开始的时候应该是 找RUNNABLE,

总结出一个结论 凡是可能导致bug,文档中又没有提示的 一定是sjtu自己加的....

然后 要让用户可以调用,sysenter还是走以前的 但需要增加新的分发 然后我打开`inc/syscall.h`一看...哇 多了这么多 先管这里的😿,在`kern/syscall.c:syscall`中加上 [哇 这段代码 写了5遍 感觉每次的bug都好蠢😞]

```c
case SYS_yield:
  sys_yield();
  return 0;
```

然后我`make qemu-nox CPUS=4` 然后崩了`kernel page faults?` 😿？就算把CPUS调为1也崩了😿找了半天没有找到问题,然后看了我上面文档的 可能bug的记录 发现 在 sysenter 我没有维护锁

* trap逻辑 用户trap->硬件反应 push+call->trap内核处理->如果是用户态来的请求 则lock->修改相关 记录信息->`env_run(或sys_yield)`->恢复并 释放锁
* sysenter逻辑 用户call->硬件反应 push+call->sysenterhandler内核处理-> 这里没有加锁 ->返回值->sysexit

在`kern/syscall.c`中加上头文件`#include <kern/spinlock.h>` 并对`syscall`函数起始加上`lock_kernel()`然后`make qemu-nox CPUS=4` 出现了`kernel panic on CPU 0 at kern/spinlock.c:86: CPU 0 cannot acquire kernel_lock: already holding` 😿毕竟只有`sched_yield()`回到用户态会放锁 而且`sched_yield()`实际是无返回的

于是我重构了`kern/syscall.c:syscall`的结构 把要返回值先暂存,放开锁以后再返回 实现如下

```c
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
  int32_t ret = 0;
  lock_kernel();
  // Call the function corresponding to the 'syscallno' parameter.
  // Return any appropriate return value.
  switch(syscallno){
    case SYS_cputs:
      sys_cputs((char *)a1,(size_t)a2);
      break;
    case SYS_cgetc:
      ret = sys_cgetc();
      break;
    case SYS_getenvid:
      ret = sys_getenvid();
      break;
    case SYS_env_destroy:
      ret = sys_env_destroy((envid_t) a1);
      break;
    case SYS_map_kernel_page:
      ret = sys_map_kernel_page((void*) a1, (void*) a2);
      break;
    case SYS_sbrk:
      ret = sys_sbrk((uint32_t)a1);
      break;
    case SYS_yield:
      sys_yield();
      break;
    case NSYSCALLS:
    default:
      ret = -E_INVAL;
  }
  unlock_kernel();
  return ret;
}
```

再把`kern/init.c`中的初始化换成3条`ENV_CREATE(user_yield, ENV_TYPE_USER);`即

```c
#if defined(TEST)
  // Don't touch -- used by grading script!
  ENV_CREATE(TEST, ENV_TYPE_USER);
#else
  // Touch all you want.
  ENV_CREATE(user_yield, ENV_TYPE_USER);
  ENV_CREATE(user_yield, ENV_TYPE_USER);
  ENV_CREATE(user_yield, ENV_TYPE_USER);
  //ENV_CREATE(user_primes, ENV_TYPE_USER);
#endif // TEST*
```

然后...No idle environment...然后debug 发现了
 * CPUS这个是用来 设置qemu的个数 NCPUS一直等于8 ,真实执行时 都申请这么多空间,而变量ncpu 才是获取到的真实cpu个数 ,所以做映射时 就算用NCPUS也没问题,但做操作时应该用ncpu 虽然grep了一下发现前面的代码并不需要修改
 * 很不幸 代码依然过不了,`make qemu-nox CPUS=4`无限循环输出 但！！user/hello.c可以正常的跑😿

也就是说我的yield 还是没对...debug了半天还是老的sjtu的code的遗留问题————没有把old env的CPU各个寄存器状态 保存下来...也就导致再入的时候会挂掉 在权衡了代码修改量的情况下我决定 在`kern/syscall.c:syscall`中添加 来进行保存(虽然讲道理 还是应该在lab3用 汇编+c来实现),这也是上一个lab所说的 可以通过测试 但是代码有问题的地方

根据阅读`Trapframe`结构体以及`env_pop_tf`函数 发现 需要填写的部分为`tf_regs`,`tf_es`,`tf_ds`,`tf_eip` 然后我试了很久很久很久很久 最后把Trapframe 整个重新覆盖了,然后通过**5+小时**的尝试发现 只改这几项并不可行 而且`tf_eip`比较难拿(虽然最后发现它=`tf_regs.reg_esi`)

通过在syscall中添加如下的测试代码

```c
cprintf("01%c "  ," x"[curenv->env_tf.tf_regs.reg_edi  != tf->tf_regs.reg_edi ]);
cprintf("02%c "  ," x"[curenv->env_tf.tf_regs.reg_esi  != tf->tf_regs.reg_esi ]); //
cprintf("03%c "  ," x"[curenv->env_tf.tf_regs.reg_ebp  != tf->tf_regs.reg_ebp ]); //
cprintf("04%c "  ," x"[curenv->env_tf.tf_regs.reg_oesp != tf->tf_regs.reg_oesp]); //
cprintf("05%c "  ," x"[curenv->env_tf.tf_regs.reg_ebx  != tf->tf_regs.reg_ebx ]);
cprintf("06%c "  ," x"[curenv->env_tf.tf_regs.reg_edx  != tf->tf_regs.reg_edx ]); //
cprintf("07%c "  ," x"[curenv->env_tf.tf_regs.reg_ecx  != tf->tf_regs.reg_ecx ]); //
cprintf("08%c "  ," x"[curenv->env_tf.tf_regs.reg_eax  != tf->tf_regs.reg_eax ]); //
cprintf("11%c "  ," x"[curenv->env_tf.tf_es            != tf->tf_es           ]);
cprintf("12%c "  ," x"[curenv->env_tf.tf_ds            != tf->tf_ds           ]);
cprintf("13%c "  ," x"[curenv->env_tf.tf_trapno        != tf->tf_trapno       ]);
cprintf("14%c "  ," x"[curenv->env_tf.tf_err           != tf->tf_err          ]);
cprintf("15%c "  ," x"[curenv->env_tf.tf_eip           != tf->tf_eip          ]); //
cprintf("16%c "  ," x"[curenv->env_tf.tf_cs            != tf->tf_cs           ]);
cprintf("17%c "  ," x"[curenv->env_tf.tf_eflags        != tf->tf_eflags       ]); //
cprintf("18%c "  ," x"[curenv->env_tf.tf_esp           != tf->tf_esp          ]); //
cprintf("19%c \n"," x"[curenv->env_tf.tf_ss            != tf->tf_ss           ]);

curenv->env_tf.tf_regs.reg_edi  = tf->tf_regs.reg_edi ;
curenv->env_tf.tf_regs.reg_esi  = tf->tf_regs.reg_esi ;
curenv->env_tf.tf_regs.reg_ebp  = tf->tf_regs.reg_ebp ;
curenv->env_tf.tf_regs.reg_oesp = tf->tf_regs.reg_oesp;
curenv->env_tf.tf_regs.reg_ebx  = tf->tf_regs.reg_ebx ;
curenv->env_tf.tf_regs.reg_edx  = tf->tf_regs.reg_edx ;
curenv->env_tf.tf_regs.reg_ecx  = tf->tf_regs.reg_ecx ;
curenv->env_tf.tf_regs.reg_eax  = tf->tf_regs.reg_eax ;
curenv->env_tf.tf_es            = tf->tf_es           ;
curenv->env_tf.tf_ds            = tf->tf_ds           ;
curenv->env_tf.tf_trapno        = tf->tf_trapno       ;
curenv->env_tf.tf_err           = tf->tf_err          ;
curenv->env_tf.tf_eip           = tf->tf_eip          ;
curenv->env_tf.tf_cs            = tf->tf_cs           ;
curenv->env_tf.tf_eflags        = tf->tf_eflags       ;
curenv->env_tf.tf_esp           = tf->tf_esp          ;
curenv->env_tf.tf_ss            = tf->tf_ss           ;
```

通过user/yield的 输出 和 反复注释和取消注释 可以发现,有变动的只有 02 03 04 06 07 08 15 17 18,而影响正确执行的只有15 和 18,回想其原因,在用户态的时候 我们做了 暂存寄存器,所以02-08的变动不会有影响,eflags没影响就不是很清楚了😿？

这里最后修改为在`kern/trapentry.S`中修改`sysenter_handler`为如下 即手工push一个Trapframe 把第5个参数传该结构体的指针

```assembly
sysenter_handler:
  pushw $0
  pushw $GD_UD | 3
  pushl %ebp
  pushfl
  pushw $0
  pushw $GD_UT | 3
  pushl %esi
  pushl $0
  pushl $0
  pushw $0
  pushw %ds
  pushw $0
  pushw %es
  pushal
  pushl %esp
  pushl %edi
  pushl %ebx
  pushl %ecx
  pushl %edx
  pushl %eax
  call syscall
  movl %ebp, %ecx
  movl %esi, %edx
  sysexit
```

`kern/syscall.c:syscall`的代码中添加`curenv->env_tf = *((struct Trapframe *)a5);`

```c
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
  lock_kernel();
  curenv->env_tf = *((struct Trapframe *)a5);

  int32_t ret = 0;
```

终于 `make qemu-nox CPUS=4`可以看到 类似如下的输出(多跑几遍 可以看到并行 以及多个CPU的输出即可)

```
[00000000] new env 00001008
[00000000] new env 00001009
[00000000] new env 0000100a
Hello, I am environment 00001008.
Hello, I am environment 00001009.
Back in environment 00001008, iteration 0.
Back in environment 00001009, iteration 0.
Back in environment 00001008, iteration 1.
Back in environment 00001009, iteration 1.
Back in environment 00001008, iteration 2.
Back in environment 00001009, iteration 2.
Back in environment 00001008, iteration 3.
Back in environment 00001009, iteration 3.
Back in environment 00001008, iteration 4.
All done in environment 00001008.
[00001008] exiting gracefully
[00001008] free env 00001008
Back in environment 00001009, iteration 4.
All done in environment 00001009.
[00001009] exiting gracefully
[00001009] free env 00001009
Hello, I am environment 0000100a.
Back in environment 0000100a, iteration 0.
Back in environment 0000100a, iteration 1.
Back in environment 0000100a, iteration 2.
Back in environment 0000100a, iteration 3.
Back in environment 0000100a, iteration 4.
All done in environment 0000100a.
[0000100a] exiting gracefully
[0000100a] free env 0000100a
```

## Question

> In your implementation of env_run() you should have called lcr3(). Before and after the call to lcr3(), your code makes references (at least it should) to the variable e, the argument to env_run. Upon loading the %cr3 register, the addressing context used by the MMU is instantly changed. But a virtual address (namely e) has meaning relative to a given address context--the address context specifies the physical address to which the virtual address maps. Why can the pointer e be dereferenced both before and after the addressing switch?

切的是用户态 也就是不同的`e->env_pgdir` 根据lab3的实现`e->env_pgdir`的内核部分的映射都是相同的,而且是"静态的".e指向的是内核位置 切换前后并不会有任何影响

```c
/* TODO
Challenge! Add a less trivial scheduling policy to the kernel, such as a fixed-priority scheduler that allows each environment to be assigned a priority and ensures that higher-priority environments are always chosen in preference to lower-priority environments. If you're feeling really adventurous, try implementing a Unix-style adjustable-priority scheduler or even a lottery or stride scheduler. (Look up "lottery scheduling" and "stride scheduling" in Google.)

Write a test program or two that verifies that your scheduling algorithm is working correctly (i.e., the right environments get run in the right order). It may be easier to write these test programs once you have implemented fork() and IPC in parts B and C of this lab.
*/
```

```c
/* TODO
Challenge! The JOS kernel currently does not allow applications to use the x86 processor's x87 floating-point unit (FPU), MMX instructions, or Streaming SIMD Extensions (SSE). Extend the Env structure to provide a save area for the processor's floating point state, and extend the context switching code to save and restore this state properly when switching from one environment to another. The FXSAVE and FXRSTOR instructions may be useful, but note that these are not in the old i386 user's manual because they were introduced in more recent processors. Write a user-level test program that does something cool with floating-point.
*/
```

## System Calls for Environment Creation

尽管你现在的kernel可以把多个用户环境运行,切换.然而你的内核依然是手工硬编码创建的用户环境. 现在你需要实现一些必要的JOS系统调用 来允许用户创建/运行一个新的用户环境.

Unix提供fork()系统调用作为它的原始的进程创建,Unix的fork()复制 调用进程(父进程)的入口地址 来创造一个新的进程(子进程), 它们的唯一不同是 进程的ID ,也就是getpid()和getppid()返回的值, 在父进程中fork()返回自进程的进程ID,在子进程中fork()返回0,默认的 每一个进程都有它们私有的地址空间,它们不应当 修改它们看不到的内存

我们需要实现一个不同的 更加原始的 JOS 系统调用 来创建 用户态环境,用这些系统调用 你将可以实现一个完整的用户级的 Unix-like fork(),以及其它形式的环境创建,你需要实现的系统调用如下

 * `sys_exofork(void)` 此系统调用创建一个几乎空白的新环境：它的用户态的地址空间没有映射(也就是初始状态),并且它不可运行.  新环境将具有与父环境`sys_exofork`调用时相同的寄存器状态.在父进程中`sys_exofork`返回 新创建环境的`envid_t` (or a negative error code if the environment allocation failed). 在子进程中 返回0. ( 虽然子进程开始标记为 非 runnable, `sys_exofork` 并不会真的返回进子进程 直到父进程 明确的用...允许标记子进程为)
 * `sys_env_set_status(envid_t envid, int status)` 设定特定一个环境为`ENV_RUNNABLE` or `ENV_NOT_RUNNABLE`. 这个系统调是专门用来mark 一个新的已经初始化完地址空间 和寄存器状态准备运行的用户环境
 * `sys_page_alloc(envid_t envid, void *va, int perm)` 申请一页的物理内存 并映射到给定的环境空间中的一个虚拟地址
 * `sys_page_map(envid_t srcenvid, void *srcva, envid_t dstenvid, void *dstva, int perm)` 复制一个页的映射关系(不是页的内容) 从一个环境地址空间到另一个、 让内存可以share 这样新的旧的映射都 指向同一个物理内存页
 * `sys_page_unmap(envid_t envid, void *va)` 取消映射环境中的一个制定虚拟地址

对于上面所有接受环境ID的系统调用,如果ID值为0 表示 当前环境 转换可以由`kern/env.c`中的`envid2env()`实现

我们在`user/dumbfork.c`中实现了 非常基础的Unix-like fork(). 这个测试程序使用上面的系统调用来创建运行 子环境 通过拷贝它自己的地址空间,这两个环境切回 然后使用之前实现的`sys_yield`. 父进程在 10 iterations后退出, 子进程在 20个后退出.

## Exercise 6.

在`kern/syscall.c`中实现上述的系统调用. 你会用到 很多`kern/pmap.c`和`kern/env.c`中的函数, 尤其是envid2env(),现在 只要你要调用`envid2env()` 传递的`checkperm`参数始终传1. 确保你对任何 无效的系统调用参数做了检查 并返回`-E_INVAL`. 用`user/dumbfork`来测试.

作为一个专业的面向测试编码的程序员,打开`user/dumbfork.c`看看代码,哇 就普通的调用 还不如去看代码的定义文件, 构思实现步骤

1. 实现上述函数
2. 在dispatch中 控制分发
3. 修改init中的`ENV_CREATE`来测试

`sys_exofork`实现如下 (根据我前面debug 知道的`pop tf`的`tf_regs` 来让子进程可以"返回"0)

```c
struct Env *e;
int r;
if((r = env_alloc(&e, curenv->env_id)) < 0)
  return r;
e->env_tf = curenv->env_tf;
e->env_status = ENV_NOT_RUNNABLE;
e->env_tf.tf_regs.reg_eax = 0;
return e->env_id;
```

---

`sys_env_set_status`实现如下 照着文档的文字翻译即可

```c
struct Env *e;
int r;
if ((r = envid2env(envid, &e, 1)) < 0)
  return r;
if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
  return -E_INVAL;
e->env_status = status;
return 0;
```

---

`sys_page_alloc` 按照注释处理错误情况,实现如下,**需要注意的是** 如果insert失败 需要释放申请的页

```c
struct Env *e;
struct Page *p;
int r;
if( (uintptr_t)va >= UTOP || PGOFF(va) || (perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P) || (perm & (~PTE_SYSCALL)) )
  return -E_INVAL;
if((r = envid2env(envid, &e, 1)) < 0 )
  return r;
if(!(p = page_alloc(ALLOC_ZERO)))
  return -E_NO_MEM;
if((r = page_insert(e->env_pgdir, p, va, perm)) < 0){
  page_free(p);
  return r;
}
return 0;
```

---

`sys_page_map`  同样按照注释处理错误情况,实现如下

```c
struct Env* srcenv;
struct Env* dstenv;
struct Page* p;
pte_t* pte;
int r;
if ((uintptr_t)srcva >= UTOP || PGOFF(srcva) ||
    (uintptr_t)dstva >= UTOP || PGOFF(dstva) ||
    (perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P) || (perm & (~PTE_SYSCALL)))
  return -E_INVAL;
if((r = envid2env(srcenvid, &srcenv, 1)) < 0)
  return r;
if((r = envid2env(dstenvid, &dstenv, 1)) < 0)
  return r;
if(!(p = page_lookup(srcenv->env_pgdir, srcva, &pte)))
  return -E_INVAL;
if ((perm & PTE_W) && !(*pte & PTE_W))
  return -E_INVAL;
return page_insert(dstenv->env_pgdir, p, dstva, perm);
```

---

`sys_page_unmap` 实现如下

```c
struct Env *e;
int r;
if (((uintptr_t)va) >= UTOP || PGOFF(va))
  return -E_INVAL;
if ((r = envid2env(envid, &e, 1)) < 0)
  return r;
page_remove(e->env_pgdir, va);
return 0;
```

以上代码需要讲的一点是 虽然注释中说了一些return情况,但 在我反复阅读调用的函数后,我设计为用r来接受 返回状态的函数 的返回值,也就是如果是用r接受则直接返回r,其与注释中描述的要返回的错误值 也是吻合的,这样 代码可读性 可以很快区分 返回的是个指针一样的 还是状态r.

以及 从来记不清运算符的优先级的细节 我在这里尽可能多的用括号来保证运算顺序,反正变成汇编也不会多出语句😕

---

**注意 以上在MIT中Page结构体应该改为PageInfo**

---

然后开始上面提到的毫无技术含量的第二步 dispatch [然而实际情况 虽然这里没有技术含量但还是找到了各种 sjtu 的任务导致的bug(或者说我以前的结构设计得不好)😕 mit的就真的是毫无技术含量的分发就好了]

在实现的过程中我发现原来我用的syscall 有a1~a5,而我在lib/syscall.c中只内联汇编用了了a1~a4,还剩push了的esi没用,但esi用来存返回地址了 根据lab3的这个设计 一个lab3的遗留BUG😿

```
  eax                - syscall number
  edx, ecx, ebx, edi - arg1, arg2, arg3, arg4
  esi                - return pc
  ebp                - return esp
  esp                - trashed by sysenter
```

所以我的第5个参数要怎么传,这一块目前只看到tcbbd实现的逻辑是对的,不过 他没有用esi保存return pc, 然后google一堆32位 传参 相关的 以及objdump去试并没找到 一个科学的寄存器

最后 设计如下 还是a5用esi保存 因为原来有push esi 那么我们 在进入sysenter 因为是“原子”执行,所以 我采用 通过相对位置 0x4(%ebp)去找到它,依然用返回pc存入esi

把`lib/syscall.c`修改为/添加上a5使用esi(顺便改了一下缩进)

```c
"D" (a4),
"S" (a5)
   : "cc", "memory");
```

再修改了`trapentry.S`的`sysenter_handler`加上push 0x4(%ebp)

```assembly
pushal
pushl %esp
pushl 0x4(%ebp)
pushl %edi
pushl %ebx
```

接下来修改`kern/syscall.c:syscall`的函数为`syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, struct Trapframe * tf)` 以及`kern/syscall.h`中的定义,把原来a5 改为tf即可,最新的`syscall`如下

```c
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, struct Trapframe * tf)
{
  lock_kernel();
  curenv->env_tf = *tf;

  int32_t ret = 0;
  // Call the function corresponding to the 'syscallno' parameter.
  // Return any appropriate return value.
  switch(syscallno){
    case SYS_cputs:
      sys_cputs((char *)a1,(size_t)a2);
      break;
    case SYS_cgetc:
      ret = sys_cgetc();
      break;
    case SYS_getenvid:
      ret = sys_getenvid();
      break;
    case SYS_env_destroy:
      ret = sys_env_destroy((envid_t) a1);
      break;
    case SYS_map_kernel_page:
      ret = sys_map_kernel_page((void*) a1, (void*) a2);
      break;
    case SYS_sbrk:
      ret = sys_sbrk((uint32_t)a1);
      break;
    case SYS_yield:
      sys_yield();
      break;
    case SYS_exofork:
      sys_exofork();
      break;
    case SYS_env_set_status:
      sys_env_set_status((envid_t)a1, (int)a2);
      break;
    case SYS_page_alloc:
      sys_page_alloc((envid_t)a1, (void *)a2, (int)a3);
      break;
    case SYS_page_map:
      sys_page_map((envid_t)a1, (void *)a2, (envid_t)a3, (void *)a4, (int)a5);
      break;
    case SYS_page_unmap:
      sys_page_unmap((envid_t)a1, (void *)a2);
      break;
    case NSYSCALLS:
    default:
      cprintf("syscallno not implement = %d\n",syscallno);
      ret = -E_INVAL;
  }
  unlock_kernel();
  return ret;
}
```

执行`make qemu-nox CPUS=4`还是通过了上面的yield测试😿然后把`kern/init.c:i386_init` 中改为`ENV_CREATE(user_dumbfork, ENV_TYPE_USER);`再`make qemu-nox` 哇.....得到了`General Protection` 的trap

又debug了很久,没错 想也想到了 又双叒叕 是sjtu的遗留问题😕,感觉在锻炼自己的debug能力,问题在于 `inc/lib.h`中`sys_exofork`并没有调用我们的`lib/syscall.c:syscall`而是采用内联汇编,并且还很嘲讽的有一个注释`// This must be inlined.  Exercise for reader: why?`

也就是说 当用户调用这个函数时,根据上面文档的行为描述 不应当再修改用户环境中的栈和寄存器(除了eax),进入内核态 所以这里用内联并且没有其它影响.这样就能复制出调用时的用户 栈和寄存器😕,同时也就是说 这样要走trap的SYSCALL而不是sysenter

因此在`kern/trap.c:trap_init`中加上 [注意SETGATE的权限为3]

```c
extern void ENTRY_SYSCALL();/* 48 system call*/
SETGATE(idt[T_SYSCALL],0,GD_KT,ENTRY_SYSCALL,3);
```

在`kern/trapentry.S`中加上

```assembly
TRAPHANDLER_NOEC( ENTRY_SYSCALL , T_SYSCALL)  /* 48 system call*/
```

在`kern/trap.c:trap_dispatch`中加上,[虽然目前只有`sys_exofork`会调用 而且只传了eax]

```c
case T_SYSCALL:
  tf->tf_regs.reg_eax = syscall(
    tf->tf_regs.reg_eax,
    tf->tf_regs.reg_edx,
    tf->tf_regs.reg_ecx,
    tf->tf_regs.reg_ebx,
    tf->tf_regs.reg_edi,
    tf->tf_regs.reg_esi,
    tf);
  return ;
```

实现以后 令人惊喜的事又发生了`CPU 0 cannot acquire kernel_lock: already holding` 也就是trap里我加了锁 syscall里我也有,这样 如果从`trap->syscall` 就会有两次去申请....哇 真的想把sysenter相关的全删了😕,,不过秉着还是保留sysenter 又要可以trap:SYSCALL 最后决定给syscall包装一下,两种syscall的路径

 * trap的路径 `用户(int syscall)->trap->分发->syscall`
 * syscall的路径 `用户(sysenter)->sysenter_handler->syscall_wrapper->syscall`

所以`kern/syscall.c:中修改如下`

```c
int32_t
syscall_wrapper(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, struct Trapframe * tf)
{
  lock_kernel();
  //curenv->env_tf = *tf;
  curenv->env_tf.tf_regs = tf->tf_regs;
  curenv->env_tf.tf_eip  = tf->tf_eip;
  curenv->env_tf.tf_esp  = tf->tf_esp;

  int32_t ret = syscall(syscallno, a1, a2, a3, a4, a5);
  unlock_kernel();
  return ret;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
  // Call the function corresponding to the 'syscallno' parameter.
  // Return any appropriate return value.
  switch(syscallno){
    case SYS_cputs:
      sys_cputs((char *)a1,(size_t)a2);
      return 0;
    case SYS_cgetc:
      return sys_cgetc();
    case SYS_getenvid:
      return sys_getenvid();
    case SYS_env_destroy:
      return sys_env_destroy((envid_t) a1);
    case SYS_map_kernel_page:
      return sys_map_kernel_page((void*) a1, (void*) a2);
    case SYS_sbrk:
      return sys_sbrk((uint32_t)a1);
    case SYS_yield:
      sys_yield();
      return 0;// infact not return
    case SYS_exofork:
      return sys_exofork();
    case SYS_env_set_status:
      return sys_env_set_status((envid_t)a1, (int)a2);
    case SYS_page_alloc:
      return sys_page_alloc((envid_t)a1, (void *)a2, (int)a3);
    case SYS_page_map:
      return sys_page_map((envid_t)a1, (void *)a2, (envid_t)a3, (void *)a4, (int)a5);
      break;
    case SYS_page_unmap:
      return sys_page_unmap((envid_t)a1, (void *)a2);
    case NSYSCALLS:
    default:
      cprintf("syscallno not implement = %d\n",syscallno);
      return -E_INVAL;
  }
}

```

**同时要修改**
 * `kern/syscall.h`中的定义,
 * `kern/trapentry.S`的`call syscall`改为`call syscall_wrapper`
 * `kern/trap.c:trap_dispatch`的调用去掉tf

现在`make qemu-nox CPUS=4`终于正常输出了 子进程0~19的输出,父进程0~9的输出

`make grade`终于拿到了除了一上来就有的10分以外的5分,而这花了2天多的时间 哇的一声就哭了,分析起来 主要debug还是sjtu用了mit的challenge但文档却没变,,,,于是 相关的bug并没有提示😕 真是精彩呢,也是提高了debug能力😕

以上还需要改的部分 目测是`syscall_wrapper`了 现在还不太确定`curenv->env_tf`哪些值是必要赋的😕 [TODO]

这部分我已经单独做了一个提交备注为`finish lab4 part A`可以使用`git diff merge完成的版本号 该版本号`查看所有改动,

在我当前的位置

```bash
git diff --stat HEAD^ HEAD
 kern/env.c       |    2 +
 kern/init.c      |    8 ++---
 kern/pmap.c      |   12 +++++--
 kern/sched.c     |   11 +++++-
 kern/spinlock.c  |   14 +++----
 kern/syscall.c   |  100 +++++++++++++++++++++++++++++++++++++++++++++--------
 kern/syscall.h   |    1 +
 kern/trap.c      |   41 +++++++++++++++-------
 kern/trapentry.S |   19 ++++++++++-
 lib/syscall.c    |   58 ++++++++++++++++---------------
 10 files changed, 191 insertions(+), 75 deletions(-)
```

---

```c
/* TODO
Challenge! Add the additional system calls necessary to read all of the vital state of an existing environment as well as set it up. Then implement a user mode program that forks off a child environment, runs it for a while (e.g., a few iterations of sys_yield()), then takes a complete snapshot or checkpoint of the child environment, runs the child for a while longer, and finally restores the child environment to the state it was in at the checkpoint and continues it from there. Thus, you are effectively "replaying" the execution of the child environment from an intermediate state. Make the child environment perform some interaction with the user using sys_cgetc() or readline() so that the user can view and mutate its internal state, and verify that with your checkpoint/restart you can give the child environment a case of selective amnesia, making it "forget" everything that happened beyond a certain point.
*/
```

# Part B: Copy-on-Write Fork

如同前面提到的 Unix 提供fork() 系统调用 作为它的基本的 进程创建. fork() 系统调用 把调用者/父进程的地址空间拷贝来创建子进程.

xv6 Unix 通过复制所有父的页的数据 到一个为子进程新申请的页里来实现fork(). 这也是dumbfork() 用来实现的方法.  复制的步骤是fork()的主要操作开销.

然而 一个fork() 在子进程里通常紧接着exec() ,[可回顾ics的shell lab],它会用新的程序的 覆盖/替代 子进程的内存. 例如这也是shell 常做的[.....看来mit的作者写文档真的细致 跟我差不多😕]. 这种情况下 复制父进程的地址空间的时间 就很浪费了 因为子进程在调用exec()前 要用的内存数据 和父进程中的所有数据相比 是非常少的.

因此 后来的Unix版本 利用了这个特点+虚拟地址 ,让父进程 和 子进程 指向同一个位置,直到它们其中有一个 要去修改它,这个技术叫做copy-on-write (写时复制).要实现这种设计,fork()函数需要 拷贝 父对象的地址映射 而不是新建一个页,并且把这个noe-shared页它标识为 `用户只读`,当父/子进程中的一个尝试写, 进程会发生page fault😿 (哇 原来写只读页是page fault ),这时trap进系统,系统可以知道 这个目标va是 真的不可用 还是 因为copy-on-write 假装/临时/设计不可用,如果是因为copy-on-write不可用,则创建一个 新的,可以写的对这个进程私有的复制了的页,这样只要一个非独立页没有被写就不会被真实的拷贝.这种设计可以 很大程度的优化 fork+exec流派

在这个lab后面的部分 你将实现一个合适的 Unix-like fork() with copy-on-write, 作为一个用户空间的library routine. 实现fork() and copy-on-write 的支持在用户空间里 的好处是 内核依然简单更容易正确,而且它依然允许用户态的程序去实现它们自己设计的fork(). 比如一个程序希望每次都是 完全拷贝 比如dumbfork() 或者 某些程序希望 父进程和子进程有 内存共享) can easily provide its own.

## User-level page fault handling

一个用户级别的 copy-on-write fork() 需要知道page faults on write-protected pages, 这是你最先要实现的. Copy-on-write 只是一种我们高可能性要用的一种用户级别page fault handling.

通常 设置一个地址空间 当页错误发生时 来方便操作.比如 大多Unix内核 在一个新的进程开始时只初始化一个stack page,之后根据 实际需求 再创建新的stack page,比如栈增长触发了page fault,一个标准的Unix 内核 需要跟踪在page fault产生时的用户环境进程空间的action引起的 .比如 堆栈中的page fault 的原因通常为 申请内存或map 页. 在program's BSS 区域的 fault 一般为 allocate a new page, 用0填充 并映射它. In systems with demand-paged executables,在text区域的 fault 则会 从硬盘中读取二进制到内存再映射它.

有很多kernel需要跟踪的信息,和传统的Unix实现方法不同, you will decide what to do about each page fault in user space (in user space 修饰的哪个😿), 这样产生的bug的 危害会更小. 这样的设计的好处是 允许用户程序 灵活的定义它们内存区域; 你将使用 用户级别的 page fault handling later 来映射和访问文件 a disk-based file system.

## Setting the Page Fault Handler

为了处理它自己的页错误,一个用户环境将需要在jos kernel中注册一个page fault handler 入口.用户环境 通过新的`sys_env_set_pgfault_upcall` 系统调用 来使用 page fault entrypoint 处理它的错误. 我们在 Env 结构提中加了新的成员`env_pgfault_upcall`来记录这个信息.

## Exercise 7.

实现`sys_env_set_pgfault_upcall`系统调用. 保证 查询目标环境的ID时 启用 permission checking, 因为它是一个 "dangerous" 系统调用.

先看看定义和调用

```bash
grep -nr "sys_env_set_pgfault_upcall" * | grep -v tags  | grep -v obj
inc/lib.h:52:int  sys_env_set_pgfault_upcall(envid_t env, void *upcall);
kern/syscall.c:141:sys_env_set_pgfault_upcall(envid_t envid, void *func)
kern/syscall.c:144:  panic("sys_env_set_pgfault_upcall not implemented");
lib/syscall.c:112:sys_env_set_pgfault_upcall(envid_t envid, void *upcall)
user/faultnostack.c:10:  sys_env_set_pgfault_upcall(0, (void*) _pgfault_upcall);
user/faultevilhandler.c:9:  sys_env_set_pgfault_upcall(0, (void*) 0xF0100020);
user/faultbadhandler.c:12:  sys_env_set_pgfault_upcall(0, (void*) 0xDeadBeef);
```

再根据 文档+函数接受参数+函数上方注释 知道 这个函数 把envid对应的Env的`env_pgfault_upcall`设置为 对应的func,这样 以后发生用户级别的 page fault 就可以调用这个函数,需要注意的是上面文档也说了 就是传入的函数的指针的合法性,实现如下[grep也看到了 也没有对这个项的其它操作 目测一会又要自己实现]

```c
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
  struct Env *e;
  int r;
  if((r = envid2env(envid, &e, 1)) < 0 )
    return r;
  e->env_pgfault_upcall = func;
  return 0;
}
```

---

## Normal and Exception Stacks in User Environments

在正常的执行的时候 JOS中用户环境在用户栈上运行,它的esp寄存器 会指向USTACKTOP, 栈上数据会push 在[USTACKTOP-PGSIZE,USTACKTOP-1] 页上,当在用户模式上发生了page fault,内核会重新开始一个用户环境,也就是我们指定的page fault handler,但在不同的栈上(叫做用户异常栈),在本质上,我们要让JOS实现在用户环境中自动的 栈转换,和在x86 已经实现的栈转换 是几乎相同的方式,

JOS 的 用户异常栈 也是只有1页的大小,它的顶部定义在虚拟地址UXSTACKTOP, 所以有效的区域只有[UXSTACKTOP-PGSIZE,UXSTACKTOP-1] 当在用户异常栈上运行时,user-level page fault handler 可以使用常规的 系统调用来映射新的页 或者调整映射关系 来修复引发page fault发生的的问.the user-level page fault handler 的返回通过an assembly language stub, 到原来产生fault的代码的栈上.

每一用户环境 希望支持user-level page fault handling 需要为它们自己的 用户异常栈 申请内存空间, 使用`sys_page_alloc()`调用.

## Invoking the User Page Fault Handler

你需要修改`kern/trap.c` 中的page fault handling代码 按照下面所讲的方法.我们会在发生trap的时候 滴啊用这种用户环境.

如果没有page fault handler 已经注册(也就是上面的`env_pgfault_upcall`), JOS和以前一样销毁用户环境. 否则,内核在 用户异常栈上 设置一个`inc/trap.h`的 struct UTrapframe:

```
                    <-- UXSTACKTOP
trap-time esp
trap-time eflags
trap-time eip
trap-time eax       start of struct PushRegs
trap-time ecx
trap-time edx
trap-time ebx
trap-time esp
trap-time ebp
trap-time esi
trap-time edi       end of struct PushRegs
tf_err (error code)
fault_va            <-- %esp when handler is run
```

内核然后安排用户环境的page fault handler 执行在 用户异常栈上,你需要理解 它的运作原理和过程`fault_va`是引发错误的虚拟地址.

如果 已经是page fault handler 在user exception stack上,它自己产生了fault.在这种情况下 你应该 开启一个新的stack frame 就在当前的`tf->tf_esp`的下方而不是UXSTACKTOP. 你需要先push an empty 32-bit word,然后push struct UTrapframe.检查`tf->tf_esp` 是否已经在user exception stack,只需要检查它是否在[UXSTACKTOP-PGSIZE,UXSTACKTOP-1]

## Exercise 8.

实现 `kern/trap.c:page_fault_handler` 分发`page faults`到用户态的`user-mode handler`. 确保 在写 异常栈时 使用适当的措施. ( 如果用户环境 使用完了  异常栈的空间 会发生什么?)

文档+再看注释 要注意的有
 * `curenv->env_pgfault_upcall` 需要存在 则push struct 然后调用,如果没有 则按原来的处理方式
 * 可能自己产生 fault 需要检查 `tf->tf_esp`是否[UXSTACKTOP-PGSIZE,UXSTACKTOP-1]
 * 在递归的例子中 需要`an extra word between the current top of the exception stack and the new stack frame`

Hint

 * `user_mem_assert()` and `env_run()` are useful here.
 * To change what the user environment runs, modify `curenv->env_tf`
 * (the `tf` variable points at `curenv->env_tf`).

结构体

```c
struct UTrapframe {
  /* information about the fault */
  uint32_t utf_fault_va;  /* va for T_PGFLT, 0 otherwise */
  uint32_t utf_err;
  /* trap-time return state */
  struct PushRegs utf_regs;
  uintptr_t utf_eip;
  uint32_t utf_eflags;
  /* the trap-time stack to return to */
  uintptr_t utf_esp;
} __attribute__((packed));
```

流程
 1. 用户环境发生页错误
 2. trap进内核 并且 分发到 `page_fault_handler`
 3. `page_fault_handler` 该函数 判断错误来源,即 是`page_fault_handler`过程中产生的 还是用户产生的
 4. 如果是用户产生的 则,把当前的用户的tf 中需要的数据 放入UTrapframe(加上错误的va),修改 tf 的 eip(执行的代码位置 具体的处理代码) esp(使用的堆栈位置)
 5. 这样就可以调用具体的处理 代码,而且和原来的用户进程 在 同一个进程里,只是切换了 eip,esp,它有访问原来进程所有可访问的权限,又在用户态
 6. 如果 刚刚是`page_fault_handler`产生的,则 递归方式 fix,需要push栈开始的位置 将不再是`UXSTACKTOP` 而是`tf_esp` [需要多一个 word来存地址！！ 因为在发生fault时 硬件向当前栈push了一个 而递归的话 也就是在UXSTACK这个栈中push的 也就是下面的`-sizeof(void *)`]

实现如下 [😕 想了想 按照结构体顺序 也就是push倒序]

```c
if (curenv->env_pgfault_upcall) {
  struct UTrapframe * utf;
  if ((uint32_t)(UXSTACKTOP - tf->tf_esp) < PGSIZE)
    utf = (struct UTrapframe *)(tf->tf_esp - sizeof(void *) - sizeof(struct UTrapframe));
  else
    utf = (struct UTrapframe *)(UXSTACKTOP - sizeof(struct UTrapframe));
  user_mem_assert(curenv, (void *)utf, sizeof(struct UTrapframe), PTE_W);

  utf->utf_fault_va = fault_va;
  utf->utf_err      = tf->tf_err;
  utf->utf_regs     = tf->tf_regs;
  utf->utf_eip      = tf->tf_eip;
  utf->utf_eflags   = tf->tf_eflags;
  utf->utf_esp      = tf->tf_esp;

  curenv->env_tf.tf_eip = (uintptr_t)curenv->env_pgfault_upcall;
  curenv->env_tf.tf_esp = (uintptr_t)utf;
  env_run(curenv);
}
```

---

## User-mode Page Fault Entrypoint

然后需要实现 一个汇编流派 的😿, 将处理调用C 的 page fault handler 并在原始故障指令下恢复执行. 并且这个汇编的代码 也就是我们要用`sys_env_set_pgfault_upcall()`注册的函数.

## Exercise 9.

实现 `lib/pfentry.S:_pgfault_upcall`. 有趣的部分是返回到引起page fault的用户代码中的位置时. 您将直接返回那里,而不需要通过内核.硬件部分同时切换堆栈并重新加载EIP.

看汇编 注释说`and then it pushes a UTrapframe` 栈结构如下

```c
  trap-time esp
  trap-time eflags
  trap-time eip
  utf_regs.reg_eax
  ...
  utf_regs.reg_esi
  utf_regs.reg_edi
  utf_err (error code)
  utf_fault_va            <-- %esp
```

然而我刚刚实现的 直接c代码 地址手工写😕

`If this is a recursive fault, the kernel will reserve for us a blank word above the trap-time esp for scratch work when we unwind the recursive call.`

汇编已经 把 调用做好了,我们要做的是调用结束以后的善后工作😕,注释说 我们需要恢复 发生fault时的 现场 各个寄存器,而且不能直接用 jmp和ret,我们应该 push the trap-time %eip 到*trap-time* 栈上! 然后 我们要切换到那个栈 然后 'ret', 它这样就会重装载 pre-fault value. In the case of a recursive fault on the exception stack, note that the word we're pushing now will fit in the blank word that the kernel reserved for us.现在需要想一想 在调用函数以后 还有哪些寄存器的值有意义——esp,可以通过它找到我们之前的UTrapframe,那我们通过它 来恢复寄存器即可也就是

```
  utf->utf_regs
  utf->utf_eip
  utf->utf_eflags
  utf->utf_esp
```

注释还转门 根据 每一块来分化了`LAB 4: Your code here.`实现如下

```assembly
// Push old eip to old stack
// Set utf->utf_esp = old stack bottom - 4
movl 0x28(%esp), %ebx   // ebx = utf->utf_eip
movl 0x30(%esp), %eax
subl $0x4, %eax         // eax = utf->utf_esp - 4
movl %ebx, (%eax)       // *(utf->utf_esp - 4) = utf->utf_eip
movl %eax, 0x30(%esp)   // utf->utf_esp = utf->utf_esp - 4

// Restore the trap-time registers.  After you do this, you
// can no longer modify any general-purpose registers.
addl $0x8, %esp
popal                   // hardware utf_regs = urf->utf_regs

// Restore eflags from the stack.  After you do this, you can
// no longer use arithmetic operations or anything else that
// modifies eflags.
addl $0x4, %esp
popfl                   // hardware utf_eflags = urf->utf_eflags

// Switch back to the adjusted trap-time stack.
popl %esp

// Return to re-execute the instruction that faulted.
ret
```

---

最后 需要实现 C user library side of the user-level page fault handling mechanism.

## Exercise 10.

* 实现 `lib/pgfault.c:set_pgfault_handler()`

先看调用

```bash
grep -r "set_pgfault_handler" * | grep -v obj | grep -v tags | grep -v Binary | grep set_pgfault_handler
inc/lib.h:void  set_pgfault_handler(void (*handler)(struct UTrapframe *utf));
lib/pgfault.c:set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
lib/pgfault.c:        panic("set_pgfault_handler not implemented");
lib/pfentry.S:// a page fault in user space (see the call to sys_set_pgfault_handler
user/faultalloc.c:    set_pgfault_handler(handler);
user/faultregs.c:     set_pgfault_handler(pgfault);
user/faultallocbad.c: set_pgfault_handler(handler);
user/faultdie.c:      set_pgfault_handler(handler);
```

咦 之前不是实现了一个么？？ 刚刚实现的1.它在内核里不是用户的 虽然可以通过syscall调用,2 它不做申请之类的,只做处理,

现在要做的是用户环境中的,给用户程序直接调用的 也就是流程

 1. 用户 `set_pgfault_handler(A)`
 2. 用户模式`_pgfault_handler = handler`
 3. 第一次的话 会`sys_env_set_pgfault_upcall((envid_t) 0, _pgfault_upcall)`  其中`_pgfault_upcall`使我们实现的 用户层的汇编

当page fault 发生时
 1. trap -> `_pgfault_upcall` (被 `sys_env_set_pgfault_upcall`设置的)
 2. `_pgfault_upcall` 调用 `_pgfault_handler` (用户传入的handler)
 3. 处理完后 返回 trap

实现如下

```c
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
  int r;

  if (_pgfault_handler == 0) {
    // First time through!
    if((r = sys_page_alloc((envid_t) 0, (void*)(UXSTACKTOP-PGSIZE), PTE_U | PTE_P | PTE_W)) < 0 )
      panic("set_pgfault_handler %e\n",r);
    if((r = sys_env_set_pgfault_upcall((envid_t)0, _pgfault_upcall)) < 0)
      panic("sys_env_set_pgfault_upcall: %e\n", r);
  }

  // Save handler pointer for assembly to call.
  _pgfault_handler = handler;
}
```

根据文档测试`user/faultdie`跪了😿

```
syscallno not implement = 10
[00001008] user panic in <unknown> at lib/pgfault.c:35: sys_env_set_pgfault_upcall: invalid parameter
```

感谢我自己的cprintf 😕,这也同时说明了文档设计 具体处理放到 user 而不是 kernel的好处,

`kern/syscall.c:syscall`加上

```c
case SYS_env_set_pgfault_upcall:
  return sys_env_set_pgfault_upcall((envid_t)a1, (void *)a2);
```

现在`make qemu-nox CPUS=4` 可以看到

```bash
faultread: OK (1.7s)
faultwrite: OK (1.6s)
faultdie: OK (1.6s)
faultregs: OK (1.7s)
faultalloc: OK (1.6s)
faultallocbad: OK (1.6s)
faultnostack: OK (1.6s)
faultbadhandler: OK (1.6s)
faultevilhandler: OK (1.6s)
forktree: missing '....: I am .0.'
```

forktree 前面都通过了😀😅😆😃😄

---

```c
/* TODO
Make sure you understand why user/faultalloc and user/faultallocbad behave differently.

Challenge! Extend your kernel so that not only page faults, but all types of processor exceptions that code running in user space can generate, can be redirected to a user-mode exception handler. Write user-mode test programs to test user-mode handling of various exceptions such as divide-by-zero, general protection fault, and illegal opcode.
*/
```

---

## Implementing Copy-on-Write Fork

你现在可以用户空间+上面实现的系统调用来实现fork(),也就是不需要再额外修改内核代码

作者 在`lib/fork.c:fork()`已经写好了框架(换句话说就是 把函数名定义好了). 类似于dumbfork(), fork()应当穿件新的environment, 然后 扫描父环境的entire address space 并在子进程中设置它. 关键的不同的是, dumbfork() 复制页内容, fork() 初始化时 复制 页映射关系. fork() 只会在父/子 其中一个尝试写时再复制.

fork()的基本控制流:

 1. 父进程 用`set_pgfault_handler`设置pgfault()为 page fault handler
 2. 父进程调用`sys_exofork()` 创建子环境.
 3. 对于 每一个 在UTOP下方的 可以writable 或 copy-on-write 的页, 父函数 调用 `duppage`, duppage 会映射copy-on-write页到 子进程的地址 然后再重新把copy-on-write页映射到它自己的地址空间. duppage 会设置父和子的 PTE 因此 页都是不可写的, 并且在`avail`项中包含 `PTE_COW`  来区分copy-on-write pages 和真正的只读页. 用户异常栈 不需要重映射,它应当在子进程中重新申请并映射. fork() 页需要处理哪些现有的 不可写 也不是 copy-on-write的页. [感觉文档这里不太合理 duppage 具体 可以再分开讲]
 4. 父进程设置 子进程的user page fault entrypoint .
 5. 父进程标记子进程runnable.

每一次 环境写向一个没有权限写的copy-on-write 页, 会触发page fault. 下面是处理流程:

 1. 内核传递 页错误到`_pgfault_upcall` 也就是上面说的pgfault().
 2. pgfault() 检测导致fault操作是否是写 (check for `FEC_WR` in the error code) 并且检查页是否是`PTE_COW` 如果不满足则panic.
 3. pgfault() 申请 新的页 映射到一个零时的位置 并复制 copy-on-write 页的内容到 新的页里. 然后修改映射关系到新的页,新的页的权限为W+R.

## Exercise 11.

实现`lib/fork.c`中的fork, duppage, pgfault .

框架真是精彩😕,还是参考`user/dumbfork.c`,它用的迷之end grep了一遍依然没理解

```bash
> grep -nr "[^a-z_(]end[^a-z_.']" * | grep -v Binary | grep -v obj | grep -v tags | grep end
inc/error.h:18:  E_EOF    = 8,  // Unexpected end of file
inc/stab.h:38:#define  N_ECOMM    0xe4  // end common
inc/stab.h:39:#define  N_ECOML    0xe8  // end common (local name)
kern/pmap.c:98:  // which points to the end of the kernel's bss segment:
kern/pmap.c:102:    extern char end[];
kern/pmap.c:103:    nextfree = ROUNDUP((char *) end, PGSIZE);
kern/mpconfig.c:87:  struct mp *mp = KADDR(a), *end = KADDR(a + len);
kern/mpconfig.c:89:  for (; mp < end; mp++)
kern/init.c:50:  extern char edata[], end[];
kern/init.c:55:  memset(edata, 0, end - edata);
kern/monitor.c:55:  extern char entry[], etext[], edata[], end[];
kern/monitor.c:61:  cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
kern/monitor.c:265:// putting at the end of the file seems to prevent inlining.
lib/printfmt.c:30:  [E_EOF]    = "unexpected end of file",
user/user.ld:42:   * the stabs, the end of the stabs, the beginning of the stabs
user/user.ld:43:   * string table, and the end of the stabs string table, respectively.
user/dumbfork.c:46:  extern unsigned char end[];
user/dumbfork.c:68:  for (addr = (uint8_t*) UTEXT; addr < end; addr += PGSIZE)
user/sbrktest.c:10:  uint32_t start, end;
user/sbrktest.c:14:  end = sys_sbrk(ALLOCATE_SIZE);
```

根据`inc/memlayout.h` 把`UTEXT到KSTACKTOP`都找一遍,原来的duppage()实现流程为 [在实现fork函数时也可以先不管它]

 1. 在child 的va申请新页
 2. 在parent 的UTEMP地址同样映射到 那个新页
 3. 在parent 里把va的内容复制到UTEMP 也就是 child的va里
 4. 取消parent 里UTEMP的映射

提示里的vpd vpt

```bash
grep -r "vpd" * | grep -v obj | grep -v Binary | grep -v fork
inc/memlayout.h: * which vpd is set in entry.S.
inc/memlayout.h:extern volatile pde_t vpd[];     // VA of current page directory
lib/libmain.c:// entry.S already took care of defining envs, pages, vpd, and vpt.
lib/entry.S:// Define the global symbols 'envs', 'pages', 'vpt', and 'vpd'
lib/entry.S:    .globl vpd
lib/entry.S:    .set vpd, (UVPT+(UVPT>>12)*4)

grep -r "vpt" * | grep -v obj | grep -v Binary | grep -v fork
inc/memlayout.h:// User read-only virtual page table (see 'vpt' below)
inc/memlayout.h: * which vpt is set in entry.S).  The PTE for page number N is stored in
inc/memlayout.h: * vpt[N].  (It's worth drawing a diagram of this!)
inc/memlayout.h:extern volatile pte_t vpt[];     // VA of "virtual page table"
lib/libmain.c:// entry.S already took care of defining envs, pages, vpd, and vpt.
lib/entry.S:// Define the global symbols 'envs', 'pages', 'vpt', and 'vpd'
lib/entry.S:    .globl vpt
lib/entry.S:    .set vpt, UVPT
```

关于错误 注释说

```c
 Returns: child's envid to the parent, 0 to the child, < 0 on error.
 It is also OK to panic on error.
```

dumbfork做的是 panic,这个fork()实现的原子性 我做得不太好,如果只是 ret < 0,中间的有些做了一半也不对,所以我这里用的panic [TODO] 虽然是panic 但目测如果父进程被destroy 依然有子进程残留的问题😥

[残留问题 目前我使用的linux 有杀死父进程 和 杀死进程树，如果有dead的 可以通过terminal/内核查看到 再通过terminal/内核 kill,最后决定 残留就残留吧]

实现如下,其中该从0开始还是UTEXT 不是很清楚,看了`inc/memlayout.h`的画的[0~UTEXT-1]用来作为临时的处理区域,但它毕竟也是用户区域,最后还是照着dumbfork从UTEXT开始 [TODO]

```c
envid_t
fork(void)
{
  set_pgfault_handler(pgfault);

  envid_t envid;
  uintptr_t addr;
  int r;
  // Allocate a new child environment.
  // The kernel will initialize it with a copy of our register state,
  // so that the child will appear to have called sys_exofork() too -
  // except that in the child, this "fake" call to sys_exofork()
  // will return 0 instead of the envid of the child.
  envid = sys_exofork();
  if (envid < 0)
    panic("sys_exofork: %e", envid);
  if (envid == 0) {
    // We're the child.
    // The copied value of the global variable 'thisenv'
    // is no longer valid (it refers to the parent!).
    // Fix it and return 0.
    thisenv = &envs[ENVX(sys_getenvid())];
    return 0;
  }
  // We're the parent.
  // Do the same mapping in child's process as parent
  // Search from UTEXT to USTACKTOP map the PTE_P | PTE_U page
  for (addr = UTEXT; addr < USTACKTOP; addr += PGSIZE)
    if ((vpd[PDX(addr)] & PTE_P) && (vpt[PGNUM(addr)] & (PTE_P | PTE_U)) == (PTE_P | PTE_U))
      duppage(envid, PGNUM(addr));

  if((r = sys_page_alloc(envid, (void *)(UXSTACKTOP-PGSIZE), PTE_U|PTE_W|PTE_P)) < 0)
    panic("sys_page_alloc: %e\n",r);
  if((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
    panic("sys_env_set_pgfault_upcall: %e\n",r);

  if((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
    panic("sys_env_set_status: %e\n",r);
  return envid;
}
```

**除了实现以上函数** 还要在`lib/fork.c`中加上`extern void _pgfault_upcall(void);`

---

然后是`duppage`,和dumbfork不同的接受参数不同,虽然看注释又说`pn*PGSIZE`,底层做了对齐,不懂 把它除一次 又乘一次干嘛 😪

实现如下

```c
void * addr = (void *)(pn * PGSIZE);
int r;
if (vpt[pn] & (PTE_W | PTE_COW)) {
  if((r = sys_page_map((envid_t)0, addr, envid, addr, PTE_U | PTE_P | PTE_COW) < 0))
    panic("sys_page_map: %e\n", r);
  if((r = sys_page_map((envid_t)0, addr, 0    , addr, PTE_U | PTE_P | PTE_COW) < 0))
    panic("sys_page_map: %e\n", r);
} else {
  if((r = sys_page_map((envid_t)0, addr, envid, addr, PTE_U | PTE_P )) < 0)
    panic("sys_page_map: %e\n", r);
}
return 0;
```

其中感觉有问题的在于 权限,`只写PTE_U | PTE_P | PTE_COW `这样对权限的覆写 有点"硬编码"的意味 吗？感觉还是该改成对权限位的修改?

然后我尝试了 修改为 `((vpt[pn]&permmask) | PTE_U | PTE_P | PTE_COW ) & ~(PTE_W)`,其中`permmask = (1 << PGSHIFT) - 1`,结果这样做测试挂了

通过debug 发现 这样做出来的权限是865,而只有`PTE_U | PTE_P`的是`805` 对照`PTE_`表,也就是我们清除了 Accessed+Dirty,对于其它权限位还是不知道是也需要清除 还是保留原样,最后决定还是"硬编码"🐱

不过这里也看到了代码中存在的漏洞 perm可以被设置高位,传进去之后没有 判断或者mask,这样"可能"被利用,不过目测 利用的结果只会影响user mode 不会到kernel mode,,,,,没打算改 😿

```bash
grep -r "PTE_" * | grep define
inc/mmu.h:#define  PTE_P         0x001    // Present
inc/mmu.h:#define  PTE_W         0x002    // Writeable
inc/mmu.h:#define  PTE_U         0x004    // User
inc/mmu.h:#define  PTE_PWT       0x008    // Write-Through
inc/mmu.h:#define  PTE_PCD       0x010    // Cache-Disable
inc/mmu.h:#define  PTE_A         0x020    // Accessed
inc/mmu.h:#define  PTE_D         0x040    // Dirty
inc/mmu.h:#define  PTE_PS        0x080    // Page Size
inc/mmu.h:#define  PTE_G         0x100    // Global
inc/mmu.h:#define  PTE_AVAIL     0xE00    // Available for software use
inc/mmu.h:#define  PTE_SYSCALL   (PTE_AVAIL | PTE_P | PTE_W | PTE_U)
inc/mmu.h:#define  PTE_ADDR(pte) ((physaddr_t) (pte) & ~0xFFF)
inc/lib.h:#define  PTE_SHARE     0x400
lib/fork.c:#define PTE_COW       0x800
```

---

最后pgfault

```c
void *addr = (void *) utf->utf_fault_va;
uint32_t err = utf->utf_err;
int r;

// Check that the faulting access was (1) a write, and (2) to a
// copy-on-write page.  If not, panic.
// Hint:
//   Use the read-only page table mappings at vpt
//   (see <inc/memlayout.h>).
if(!((err & FEC_WR) && (vpd[PDX(addr)] & PTE_P) &&  (vpt[PGNUM(addr)] & (PTE_P | PTE_COW)) == (PTE_P | PTE_COW)))
  panic("pgfault: real page fault😶\n")

// Allocate a new page, map it at a temporary location (PFTEMP),
// copy the data from the old page to the new page, then move the new
// page to the old page's address.
// Hint:
//   You should make three system calls.
//   No need to explicitly delete the old page's mapping.
addr = ROUNDDOWN(addr, PGSIZE);
if ((r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
  panic("sys_page_alloc: %e", r);
memmove(PFTEMP, addr, PGSIZE);
if ((r = sys_page_map(0, PFTEMP, 0, addr, PTE_P|PTE_U|PTE_W)) < 0)
  panic("sys_page_map: %e", r);
if ((r = sys_page_unmap(0, PFTEMP)) < 0)
  panic("sys_page_unmap: %e", r);
```

---

**注意mit的**这里的`vpt`和`vpd`分别为`uvpt`和`uvpd`

根据文档测试成功

`make grade` Part B 50/50

做了一个commit `finish lab4 part B`

```bash
git diff HEAD^ HEAD --stat
 kern/syscall.c |   10 ++++++-
 kern/trap.c    |   20 ++++++++++++++-
 lib/fork.c     |   73 ++++++++++++++++++++++++++++++++++++++++++++++----------
 lib/pfentry.S  |   22 +++++++++++-----
 lib/pgfault.c  |    6 +++-
 5 files changed, 106 insertions(+), 25 deletions(-)
```

---

```c
/* TODO
Challenge! Implement a shared-memory fork() called sfork(). This version should have the parent and child share all their memory pages (so writes in one environment appear in the other) except for pages in the stack area, which should be treated in the usual copy-on-write manner. Modify user/forktree.c to use sfork() instead of regular fork(). Also, once you have finished implementing IPC in part C, use your sfork() to run user/pingpongs. You will have to find a new way to provide the functionality of the global thisenv pointer.
*/
```

```c
/* TODO
Challenge! Your implementation of fork makes a huge number of system calls. On the x86, switching into the kernel using interrupts has non-trivial cost. Augment the system call interface so that it is possible to send a batch of system calls at once. Then change fork to use this interface.

How much faster is your new fork?

You can answer this (roughly) by using analytical arguments to estimate how much of an improvement batching system calls will make to the performance of your fork: How expensive is an int 0x30 instruction? How many times do you execute int 0x30 in your fork? Is accessing the TSS stack switch also expensive? And so on...

Alternatively, you can boot your kernel on real hardware and really benchmark your code. See the RDTSC (read time-stamp counter) instruction, defined in the IA32 manual, which counts the number of clock cycles that have elapsed since the last processor reset. QEMU doesn't emulate this instruction faithfully (it can either count the number of virtual instructions executed or use the host TSC, neither of which reflects the number of cycles a real CPU would require).
*/
```

# Part C: Preemptive Multitasking and Inter-Process communication (IPC)

在lab的最后一个part你要让kernel
 1. 能抢占不合作的环境
 2. 允许环境之间 显示的交流/传递信息

## Clock Interrupts and Preemption

运行`user/spin`测试程序 这个测试程序 fork了一个子环境, 子环境只要获得了CPU的控制权则无限spin. 父进程和内核都无法 重新获得CPU.这显然不是理想状态,因为用户级错误不应该影响到 其它用户环境 更不应该影响到内核, 因为任何一个用户态程序 都可以让整个系统'halt' 只要它无限循环且不把CPU使用权交出来. 为了让内核可以抢占一个正在运行的用户环境, 强制抢回CPU的控制权, 我们需要扩展JOS内核 让它支持接受 硬件时钟的外部中断.

## Interrupt discipline

外部中断(或者说 设备中断)被称为IRQs(Interrupt request). 有16个可能的IRQs,标号从0到15............, 并不直接对应IDT上0~15,废话. `picirq.c:pic_init` 映射了IRQs 0-15 到IDT 的入口`IRQ_OFFSET`~`IRQ_OFFSET+15`.

在`inc/trap.h`中, `IRQ_OFFSET = 32`. 因此IDT 32-47  对应IRQs 0-15. This `IRQ_OFFSET` is chosen so that the device interrupts do not overlap with the processor exceptions, which could obviously cause confusion. (实际上在 以前的运行MS-DOS的电脑,`IRQ_OFFSET = 0`, 它引起了很多处理 硬件中断和 处理器异常的问题!)

相对于xv6,在jos 中 我们做了一个关键的简化. 在内核时 外部的设备中断始终被禁用(和 xv6相同的是,在用户态 这些中断可用). 外部中断的启用状态 由eflags的`FL_IF`标志位标示(see inc/mmu.h). 当这个位被设为1时外部中断可用. 有多种方式可以修改这个位,因为我们的简化,我们仅在 进入退出时 对%eflags进行操作.

你需要保证在用户环境下`FL_IF`被设置,这样当中断发生时才会根据IDT去调用你的处理代码. 否则的话 中断会被 masked, or ignored 知道中断再次可用.We masked interrupts with the very first instruction of the bootloader, and so far we have never gotten around to re-enabling them.

## Exercise 12.

* `kern/trapentry.S`和`kern/trap.c`初始化IRQs 0~15的 IDT表.然后修改`kern/env.c:env_alloc()`确保中断在用户环境时一直启用.
* 处理器不会push an error code 或者 检查IDT如空 描述符权限(Descriptor Privilege Level (DPL)) . 你也许需要看一下 section 9.2 of the 80386 Reference Manual, or section 5.8 of the IA-32 Intel Architecture Software Developer's Manual, Volume 3, at this time.
* 在做了这个exercise, 如果你运行了任何有一些时长测试程序(比如spin),你应当看到内核 输出了 硬件中断的 trap frames. 因为interrupts 已经启用, JOS 还没有具体的处理它们, 因此你会看到内核误认了这些中断在当前的用户环境 并且 销毁了用户环境,最终会销毁所有用户环境 进入等待的monitor.

先看`inc/trap.h`说是要映射0~15 实际只给了6个

```c
// Hardware IRQ numbers. We receive these as (IRQ_OFFSET+IRQ_WHATEVER)
#define IRQ_TIMER        0
#define IRQ_KBD          1
#define IRQ_SERIAL       4
#define IRQ_SPURIOUS     7
#define IRQ_IDE         14
#define IRQ_ERROR       19
```

在`kern/trap.c`中加上

```c
extern void ENTRY_IRQ_TIMER   ();/*  0*/
extern void ENTRY_IRQ_KBD     ();/*  1*/
extern void ENTRY_IRQ_2       ();/*  2*/
extern void ENTRY_IRQ_3       ();/*  3*/
extern void ENTRY_IRQ_SERIAL  ();/*  4*/
extern void ENTRY_IRQ_5       ();/*  5*/
extern void ENTRY_IRQ_6       ();/*  6*/
extern void ENTRY_IRQ_SPURIOUS();/*  7*/
extern void ENTRY_IRQ_8       ();/*  8*/
extern void ENTRY_IRQ_9       ();/*  9*/
extern void ENTRY_IRQ_10      ();/* 10*/
extern void ENTRY_IRQ_11      ();/* 11*/
extern void ENTRY_IRQ_12      ();/* 12*/
extern void ENTRY_IRQ_13      ();/* 13*/
extern void ENTRY_IRQ_IDE     ();/* 14*/
extern void ENTRY_IRQ_15      ();/* 15*/
extern void ENTRY_IRQ_ERROR   ();/* 19*/
```

和

```c
SETGATE(idt[IRQ_OFFSET+IRQ_TIMER   ], 0, GD_KT, ENTRY_IRQ_TIMER   , 0);
SETGATE(idt[IRQ_OFFSET+IRQ_KBD     ], 0, GD_KT, ENTRY_IRQ_KBD     , 0);
SETGATE(idt[IRQ_OFFSET+    2       ], 0, GD_KT, ENTRY_IRQ_2       , 0);
SETGATE(idt[IRQ_OFFSET+    3       ], 0, GD_KT, ENTRY_IRQ_3       , 0);
SETGATE(idt[IRQ_OFFSET+IRQ_SERIAL  ], 0, GD_KT, ENTRY_IRQ_SERIAL  , 0);
SETGATE(idt[IRQ_OFFSET+    5       ], 0, GD_KT, ENTRY_IRQ_5       , 0);
SETGATE(idt[IRQ_OFFSET+    6       ], 0, GD_KT, ENTRY_IRQ_6       , 0);
SETGATE(idt[IRQ_OFFSET+IRQ_SPURIOUS], 0, GD_KT, ENTRY_IRQ_SPURIOUS, 0);
SETGATE(idt[IRQ_OFFSET+    8       ], 0, GD_KT, ENTRY_IRQ_8       , 0);
SETGATE(idt[IRQ_OFFSET+    9       ], 0, GD_KT, ENTRY_IRQ_9       , 0);
SETGATE(idt[IRQ_OFFSET+    10      ], 0, GD_KT, ENTRY_IRQ_10      , 0);
SETGATE(idt[IRQ_OFFSET+    11      ], 0, GD_KT, ENTRY_IRQ_11      , 0);
SETGATE(idt[IRQ_OFFSET+    12      ], 0, GD_KT, ENTRY_IRQ_12      , 0);
SETGATE(idt[IRQ_OFFSET+    13      ], 0, GD_KT, ENTRY_IRQ_13      , 0);
SETGATE(idt[IRQ_OFFSET+IRQ_IDE     ], 0, GD_KT, ENTRY_IRQ_IDE     , 0);
SETGATE(idt[IRQ_OFFSET+    15      ], 0, GD_KT, ENTRY_IRQ_15      , 0);
SETGATE(idt[IRQ_OFFSET+IRQ_ERROR   ], 0, GD_KT, ENTRY_IRQ_ERROR   , 0);
```

在`trap/trapentry.S`中加上

```c
TRAPHANDLER_NOEC( ENTRY_IRQ_TIMER   , IRQ_OFFSET+IRQ_TIMER   )  /*  0*/
TRAPHANDLER_NOEC( ENTRY_IRQ_KBD     , IRQ_OFFSET+IRQ_KBD     )  /*  1*/
TRAPHANDLER_NOEC( ENTRY_IRQ_2       , IRQ_OFFSET+    2       )  /*  2*/
TRAPHANDLER_NOEC( ENTRY_IRQ_3       , IRQ_OFFSET+    3       )  /*  3*/
TRAPHANDLER_NOEC( ENTRY_IRQ_SERIAL  , IRQ_OFFSET+IRQ_SERIAL  )  /*  4*/
TRAPHANDLER_NOEC( ENTRY_IRQ_5       , IRQ_OFFSET+    5       )  /*  5*/
TRAPHANDLER_NOEC( ENTRY_IRQ_6       , IRQ_OFFSET+    6       )  /*  6*/
TRAPHANDLER_NOEC( ENTRY_IRQ_SPURIOUS, IRQ_OFFSET+IRQ_SPURIOUS)  /*  7*/
TRAPHANDLER_NOEC( ENTRY_IRQ_8       , IRQ_OFFSET+    8       )  /*  8*/
TRAPHANDLER_NOEC( ENTRY_IRQ_9       , IRQ_OFFSET+    9       )  /*  9*/
TRAPHANDLER_NOEC( ENTRY_IRQ_10      , IRQ_OFFSET+    10      )  /* 10*/
TRAPHANDLER_NOEC( ENTRY_IRQ_11      , IRQ_OFFSET+    11      )  /* 11*/
TRAPHANDLER_NOEC( ENTRY_IRQ_12      , IRQ_OFFSET+    12      )  /* 12*/
TRAPHANDLER_NOEC( ENTRY_IRQ_13      , IRQ_OFFSET+    13      )  /* 13*/
TRAPHANDLER_NOEC( ENTRY_IRQ_IDE     , IRQ_OFFSET+IRQ_IDE     )  /* 14*/
TRAPHANDLER_NOEC( ENTRY_IRQ_15      , IRQ_OFFSET+    15      )  /* 15*/
TRAPHANDLER_NOEC( ENTRY_IRQ_ERROR   , IRQ_OFFSET+IRQ_ERROR   )  /* 19*/
```

测试挂了。。。grep一下上面提到的`FL_IF`只有定义和检查

```c
grep -r "FL_IF" *
inc/mmu.h:#define FL_IF        0x00000200    // Interrupt Flag
kern/trap.c:    assert(!(read_eflags() & FL_IF));
```

以及

`grep -r "L[Aa][Bb] 4" * -A1 -B1 --exclude-dir=obj`

找到`kern/env.c`中的`Enable interrupts while in user mode.`加上`e->env_tf.tf_eflags |= FL_IF;` 哦豁 依然不对,debug半天的结果是

sysenter会禁用中断,而sysexit并不会开启😕不对称真的好吗😣,所以 还要在`kern/trapentry.S`中的`sysexit`前面加上`sti`,,

 * trap->会到`env_run`它会pop env,而env靠上面在初始化时`FL_IF`设置好了 //destroy其它回到内核态的路径就不谈
 * sysenter->会到刚刚fix用的sti 也启用了中断

这样就保证了都有 中断,现在这样运行`user/spin.c`用`make qemu-nox`可以看到`Hardware Interrupt`

## Handling Clock Interrupts

在`user/spin`程序中,在子程序第一次运行时, 它就陷入一个循环,内核一直无法再获得CPU权限,我们需要对硬件编程让它周期性的产生时钟中断,然后我们就可以利用中断强行让kernel获得权限, 从而对用户程序做出操作.

这在`kern/init.c:i386_init`中对`lapic_init`和`pic_init`的调用, which we have written for you, 设置了时钟和 interrupt controller 来生成interrupts. 你你现在只需要去处理它们.

## Exercise 13.

修改`trap_dispatch()` 函数让它在时钟中断发生时掉用`sched_yield()`去寻找一个 不同的环境。

现在你应该可以通过`user/spin`测试: 父进程会 fork出一个子进程, `sys_yield()` 恢复到内核控制CPU,最终父进程杀死子进程. [看了一下代码感觉我的env 整个设计的状态有些问题 TODO]

在分发里加上

```c
case IRQ_OFFSET + IRQ_TIMER:
  sched_yield();
  return;
```

然而并不能跑.....😢,debug了半天 发现在下面有

```c
// Handle clock interrupts. Don't forget to acknowledge the
// interrupt using lapic_eoi() before calling the scheduler!
// LAB 4: Your code here.
```

哇 心态炸了,实现如下

```c
if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
  lapic_eoi();
  sched_yield();
  return;
}
```

用`make qemu-nox CPUS=2`运行如文档所描述

且用`make grade`在 `stresssched`及`stresssched`以前的部分都通过了 65/75

---

## Inter-Process communication (IPC)

啊 终于是最后一块

(技术上将JOS中这个是 "inter-environment communication" or "IEC", 但其它系统中叫它IPC,(process),所以我们也就用标准的术语IPC好了.)

我们一直关注于os的隔离方面, 也就是它分割每一个用户程序的机制. 另一个os重要的功能/服务是 允许程序之间相互交流. 能让程序间交流将会让整个功能更加强大. Unix 的pipe 模型就是一个典型的例子.

有很多程序间交流的模型, 甚至至今 也有哪一个模型是最好的 的争论,我们不用在意那个争论,我们将实现一个简单的IPC机制,并试试能否运作

## IPC in JOS

你会要再实现一些新的jos 内核的 syscall 来为程序间交流机制做一些基本的支持.`sys_ipc_recv` and `sys_ipc_try_send`.然后你需要实现两个library wrappers `ipc_recv` and `ipc_send`.

JOS IPC机制 设计中 程序间能传递的 信息由两部分组成：一个32位的值 + 一个可选的单页映射. 允许环境 之间传递页映射 可以让两个环境间的交流更加高效率,也就是它们 可以对同一个物理页进行读写,而这样的设计 也是易于管理.

## Sending and Receiving Messages

接受 信息, 环境调用`sys_ipc_recv`. 这个系统调用 de-schedules 当前的环境 并且在接受到信息以前不会再运行它. 当一个环境等待接受信息时,任何/任意其它的环境可以发送一个信息给它. 也就是说 你在Part A实现的的权限检查和这里IPC的权限检查不同,  因为IPC设计的是一个环境不会引起其它环境的故障 (除非目标环境 本来就是buggy),所以不用怎么检查

环境通过调用`sys_ipc_try_send` + 接受者的环境id和发送的 信息 来发送值. 如果 对应的环境真实的接受到了(it has called `sys_ipc_recv` and not gotten a value yet), 则返回0给发送这. 否则返回`-E_IPC_NOT_RECV` 来表示当前的目标环境并不准备接受值.

用户lib中调用`sys_ipc_rec` 的`ipc_recv` 会检查received values 在当前的环境的 Env结构体中.

类似的 a library function `ipc_send` will take care of repeatedly calling `sys_ipc_try_send` until the send succeeds.

## Transferring Pages

当一个环境调用`sys_ipc_recv` 带有一个有效的`dstva`(目标虚拟地址)(below UTOP), 这个环境 说明它希望接收到一个页的映射. 如果发送者发送了一个页, 那么这个被发送的页应当被映射到 接受者的`dstva`的位置. 如果接受者已经map了 那么原来map的应该被取消映射。

当一个环境调用`sys_ipc_try_send` 带有一个有效的`srcva` (below UTOP), 它的意思是 发送这希望把当前环境中`srcva`对应的页发送给接受者 并且权限为perm,在IPC成功后, 发送者原来的 映射关系不变, 但接受者 也可以映射到这个页面 到接受者自己的`dstva` with perm .这样一个页面就被共享了.

如果 发送者和接受者 都没有明确的标示页需要转移 则页不会被转移. 在每一个新的IPC以后 内核设置新的`env_ipc_perm`在接受者的结构体重 来表示目标页的权限或者0 表示没有接受到页.

## Implementing IPC

## Exercise 14.

 * 在`kern/syscall.c`中实现`sys_ipc_recv`和`sys_ipc_try_send`. 阅读注释并实现他们. 现在你调用envid2env函数checkperm flag 应该设为0 (这个flag 用来检测是否是自身或父子), 并且 内核没有特殊的权限检查 除了保证envid是有效的
 * 再实现`lib/ipc.c`中的`ipc_recv`和`ipc_send`
 * 使用`user/pingpong`和`user/primes` 来测试你的IPC机制 你会发现`user/primes.c` 很有趣  to see all the forking and IPC going on behind the scenes.

质数生成器
 * 父生成子1 并向子1发送`[2,+无穷大)`,
 * 子1把第一个接受到的(也就是 2)作为质数 输出,子1生成子2,把从父 接受到的所有 mod  2 有余的 发给子2
 * 子2把第一个接受到的(也就是 3)作为质数 输出,子2生成子3,把从子1接受到的所有 mod  3 有余的 发给子3
 * 子3把第一个接受到的(也就是 5)作为质数 输出,子3生成子4,把从子2接受到的所有 mod  5 有余的 发给子4
 * 子4把第一个接受到的(也就是 7)作为质数 输出,子4生成子5,把从子3接受到的所有 mod  7 有余的 发给子5
 * 子5把第一个接受到的(也就是11)作为质数 输出,子5生成子6,把从子4接受到的所有 mod 11 有余的 发给子6
 * 子6把第一个接受到的(也就是13)作为质数 输出,子6生成子7,把从子5接受到的所有 mod 13 有余的 发给子7
 * ...

因为NENVS的数量有限 会在`8081 [000013ff] user panic in <unknown> at lib/fork.c:106: sys_exofork: out of environments`

先做一个毫无技术含量的分发,在`kern/syscall.c:syscall`中

```c
case SYS_ipc_try_send:
  return sys_ipc_try_send((envid_t)a1, (uint32_t)a2, (void *)a3, (unsigned)a4);
case SYS_ipc_recv:
  return sys_ipc_recv((void *)a1);
```

看注释,除了文档讲的,注释还说如下 等 具体细节

```c
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
```

再看看`inc/env.h`中的Env 结构体

```c
 // Lab 4 IPC
 bool env_ipc_recving;   // Env is blocked receiving
 void *env_ipc_dstva;    // VA at which to map received page
 uint32_t env_ipc_value;   // Data value sent to us
 envid_t env_ipc_from;   // envid of the sender
 int env_ipc_perm;   // Perm of page mapping received
```

`sys_ipc_recv`实现如下

```c
 315 static int
 316 sys_ipc_recv(void *dstva)
 317 {
 318   if(dstva < (void*)UTOP && !PGOFF(dstva)){
 319     curenv->env_ipc_recving = 1;
 320     curenv->env_ipc_dstva   = dstva;
 321     curenv->env_status      = ENV_NOT_RUNNABLE;
 322     sched_yield();
 323     return 0;               //NEVER RUN HERE JUST BACK TO USER ENVIRONMENT CODE
 324   }
 325   return -E_INVAL;
 326 }
```

`ENV_NOT_RUNNABLE` + `sched_yield()`沉睡自己以后只有收到信息才会被唤醒,这里没说超过UTOP会怎样 感觉如果做成等待状,就算接收到了 返回给用户 也没法用,所以我这里做的是返回`-E_INVAL`

---

然后`sys_ipc_try_send` 按照注释逐句翻译就好 感谢不用自己安排判断顺序 实现如下

```c
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
  struct Env *e;
  int r;
  if((r = envid2env(envid, &e, 0) ) < 0)
    return r;
  if(!e->env_ipc_recving)
    return -E_IPC_NOT_RECV;
  if(srcva < (void*)UTOP && !PGOFF(srcva) && (perm & (PTE_U | PTE_P)) == (PTE_U | PTE_P) && !(perm & (~PTE_SYSCALL))){
    pte_t *pte;
    struct Page *pg;// IN MIT LAB HERE SHOULD BE PageInfo
    if(!(pg = page_lookup(curenv->env_pgdir, srcva, &pte)))
      return -E_INVAL;
    if((*pte & perm) != perm)
      return -E_INVAL;
    if((r = page_insert(e->env_pgdir, pg, e->env_ipc_dstva, perm)) < 0)
      return r;
    e->env_ipc_recving        = 0;
    e->env_ipc_from           = curenv->env_id;
    e->env_ipc_value          = value;
    e->env_ipc_perm           = perm;
    e->env_status             = ENV_RUNNABLE;
    e->env_tf.tf_regs.reg_eax = 0;
    return 0;
  }
  return -E_INVAL;
}
```

其中 这里做的是 dst权限 属于 src权限子集,也就包括了注释中的`if (perm & PTE_W), but srcva is read-only in the`

实现呢 这里首先要感谢一个巨大的内核锁 保证了 这些都是原子的,这样内部赋值顺序不会有影响,如果要做到并行,锁变小 感觉有一大堆要改`_(:з」∠)_`

**注意** 一个用户调用recv以后 会陷入内核 并且 not runnable,再次运行则会通过`env_yield`去调用`env_run`走`pop_tf` 所以 这里我们要让recv正常返回 需要`e->env_tf.tf_regs.reg_eax = 0`

---

然后是`lib/ipc.c`中的 先做注释短的...

注释说`If 'pg' is null, pass sys_ipc_recv a value that it will understand as meaning "no page".  (Zero is not the right value.)`

再结合上面的取消映射,感觉上面两个函数要重新设计,我们可以通过合法的UTOP以下的来映射(UTOP以下的非对齐看作错误),再通过非法的UTOP以上来取消映射???

上面函数分别改为

```c
static int
sys_ipc_recv(void *dstva)
{
  if(!(dstva < (void*)UTOP) || !PGOFF(dstva)){
    curenv->env_ipc_recving = 1;
    curenv->env_ipc_dstva   = dstva;
    curenv->env_status      = ENV_NOT_RUNNABLE;
    sched_yield();
    return 0;               //NEVER RUN HERE JUST BACK TO USER ENVIRONMENT CODE
  }
  return -E_INVAL;
}
```

```c
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
  struct Env *e;
  int r;
  if((r = envid2env(envid, &e, 0) ) < 0)
    return r;
  if(!e->env_ipc_recving)
    return -E_IPC_NOT_RECV;
  if(srcva < (void*)UTOP){
    if(PGOFF(srcva) || (perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P) || (perm & (~PTE_SYSCALL)))
      return -E_INVAL;
    pte_t *pte;
    struct Page *pg;
    if(!(pg = page_lookup(curenv->env_pgdir, srcva, &pte)))
      return -E_INVAL;
    if((*pte & perm) != perm)
      return -E_INVAL;
    if(e->env_ipc_dstva < (void *)UTOP){
      if((r = page_insert(e->env_pgdir, pg, e->env_ipc_dstva, perm)) < 0)
        return r;
    }
  }
  e->env_ipc_recving        = 0;
  e->env_ipc_from           = curenv->env_id;
  e->env_ipc_value          = value;
  e->env_ipc_perm           = perm;
  e->env_status             = ENV_RUNNABLE;
  e->env_tf.tf_regs.reg_eax = 0;
  return 0;
}
```

即 超界限 或者 界限内页对齐 都会发送接受,只是超界限的发送不会申请/映射页

下面继续`lib/ipc.c`中的`ipc_send`实现如下,用注释提到的`sys_yield()`主动交出CPU权限

```c
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
  if (!pg)
    pg = (void*)UTOP;
  int r;
  while((r = sys_ipc_try_send(to_env, val, pg, perm))) {
    if(r != -E_IPC_NOT_RECV)
      panic("sys_ipc_try_send %e", r);
    sys_yield();
  }
}
```

然后`ipc_recv`实现如下

```c
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
  if(!pg)
    pg = (void*)UTOP;
  int32_t r = sys_ipc_recv(pg);
  if(r >= 0) {
    if(perm_store)
      *perm_store = thisenv->env_ipc_perm;
    if(from_env_store)
      *from_env_store = thisenv->env_ipc_from;
    return thisenv->env_ipc_value;
  }
  if(perm_store)
    *perm_store = 0;
  if(from_env_store)
    *from_env_store = 0;
  return r;
}
```

现在看来 pg == null也就是syscall层的 无效地址 用来单纯的值交流了,并没有取消映射的功能

至此`make grade`通过了所有测试75/75

MIT的80/80

```bash
> make grade
dumbfork: OK (2.8s) 
Part A score: 5/5

faultread: OK (2.0s) 
faultwrite: OK (1.7s) 
faultdie: OK (2.1s) 
faultregs: OK (2.3s) 
faultalloc: OK (1.9s) 
faultallocbad: OK (1.8s) 
faultnostack: OK (1.9s) 
faultbadhandler: OK (2.0s) 
faultevilhandler: OK (2.0s) 
forktree: OK (2.3s) 
Part B score: 50/50

spin: OK (2.1s) 
stresssched: OK (2.4s) 
sendpage: OK (1.7s) 
pingpong: OK (1.6s) 
primes: OK (5.2s) 
Part C score: 25/25

Score: 80/80
```

part C的修改量

```diff
git diff HEAD HEAD^ --stat
 kern/env.c       |    2 +-
 kern/syscall.c   |   44 +++++---------------------------------------
 kern/trap.c      |   48 +++++-------------------------------------------
 kern/trapentry.S |   19 -------------------
 lib/ipc.c        |   28 +++++-----------------------
 5 files changed, 16 insertions(+), 125 deletions(-)
```

---

* 然后我把

`USE_TICKET_SPIN_LOCK`的定义加上了 挂了😔,看了一下宏 别人也只是测试,注意到错误信息是`No more runnable environments!`grep一下发现来自`sched_yield`

然后看了半天逻辑,出错原因在于 主CPU 在未`ENV_CREATE`之前就 启动了其它CPU,然而其它CPU调用了`sched_yield`导致了错误,把`boot_aps`移动到所有CREATE以后即可 然后因为spinlock超级慢 最后一个点根本过不了30s的限时😔

可以看出 上面做出来的jos,它的 kernel是不会爆 但用户层面 还是很松散 隔离等做得目前没想到什么bug,但提供的借口灵活性很大 很容易就用户程序实现错误就崩了`_(:з」∠)_`

---

```c
/* TODO
Challenge! Why does ipc_send have to loop? Change the system call interface so it doesn't have to. Make sure you can handle multiple environments trying to send to one environment at the same time.
*/
```

```c
/* TODO
Challenge! The prime sieve is only one neat use of message passing between a large number of concurrent programs. Read C. A. R. Hoare, ``Communicating Sequential Processes,'' Communications of the ACM 21(8) (August 1978), 666-667, and implement the matrix multiplication example.
*/
```

```c
/* TODO
Challenge! One of the most impressive examples of the power of message passing is Doug McIlroy's power series calculator, described in M. Douglas McIlroy, ``Squinting at Power Series,'' Software--Practice and Experience, 20(7) (July 1990), 661-683. Implement his power series calculator and compute the power series for sin(x+x^3).
*/
```

```c
/* TODO
Challenge! Make JOS's IPC mechanism more efficient by applying some of the techniques from Liedtke's paper, "Improving IPC by Kernel Design", or any other tricks you may think of. Feel free to modify the kernel's system call API for this purpose, as long as your code is backwards compatible with what our grading scripts expect.
*/
```

# 总结

其实,比较难受的一点是,它的教学 是自底向上,虽然感觉和计算机系统的设计发展可能有一些联系,但这种顺序真的难受,也不知道是不是mit的学生的思维习惯就是这样.从lab1到lab4,都是 先实现一个又一个的下层函数,往后做,又有新功能,这时发现我们已经有了实现好的函数,再拼一拼代码.个人感觉是整体设计->分块划分->块之间的接口设计->每一块的具体设计 能让我学习体验更好😕,

这个lab很僵的地方也是一边编码一边体现,mit的版本还好,sjtu的就因为把 challenge直接作为必做,比如lab3 的sysenter,而直接省去了trap+syscall,坑啊.不过也是锻炼了debug能力,,~~因为debug~~整个lab时间开销也是很大了

os设计也就那样,,甚至柑橘,我国那么多连笛卡尔坐标系都能学会的普通高中生,如果有同样详尽的中文文档,它们学会os毫无难度,,

感觉**最有收获的**在于 不同设计之间的改动(比如从 原始fork到新fork的改动),如何做到 “破坏”最少的改动.还有就是 锻炼grep git vim等等,,,课上不细教真的好吗？

至于设计可以去看[卓神写的](https://github.com/tcbbd/joslabs/blob/lab4/lab4.pdf)

整个lab

```diff
git diff HEAD HEAD^^^ --stat
 kern/env.c       |    4 +-
 kern/init.c      |    8 ++-
 kern/pmap.c      |   12 +---
 kern/sched.c     |   11 +---
 kern/spinlock.c  |   14 +++--
 kern/syscall.c   |  154 ++++++++----------------------------------------------
 kern/syscall.h   |    1 -
 kern/trap.c      |  109 +++++++-------------------------------
 kern/trapentry.S |   38 +-------------
 lib/fork.c       |   73 +++++---------------------
 lib/ipc.c        |   28 ++--------
 lib/pfentry.S    |   22 +++-----
 lib/pgfault.c    |    6 +--
 lib/syscall.c    |   58 ++++++++++-----------
 14 files changed, 116 insertions(+), 422 deletions(-)
```

MIT:

```diff
git diff HEAD HEAD^^^ --stat
 kern/env.c       |   6 ++----
 kern/init.c      |   8 +++++---
 kern/pmap.c      |  19 ++++--------------
 kern/sched.c     |  14 +------------
 kern/syscall.c   | 139 ++++++++++++++++++++----------------------------------------------------------------------------------------------------------
 kern/trap.c      |  93 ++++++++++++++++--------------------------------------------------------------------
 kern/trapentry.S |  18 -----------------
 lib/fork.c       |  74 ++++++++++++-------------------------------------------------------
 lib/ipc.c        |  28 +++++---------------------
 lib/pfentry.S    |  18 +++++------------
 lib/pgfault.c    |   6 ++----
 11 files changed, 76 insertions(+), 347 deletions(-)
```

# 参考

 * [APIC](https://en.wikipedia.org/wiki/Advanced_Programmable_Interrupt_Controller)
 * [POPA](http://faydoc.tripod.com/cpu/popa.htm)
 * [GCC inline assembly](https://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html)
 * [how-many-parameters-are-too-many](http://stackoverflow.com/questions/174968/how-many-parameters-are-too-many)
 * [How to access more than 4 arguments in an ARM assembly function?](http://stackoverflow.com/questions/15071506/how-to-access-more-than-4-arguments-in-an-arm-assembly-function)
 * [List of x86 calling conventions](https://en.wikipedia.org/wiki/X86_calling_conventions#Register_preservation)
 * [List of emoticons 😕](https://en.wikipedia.org/wiki/List_of_emoticons)
 * [Interrupt request (PC architecture)](https://en.wikipedia.org/wiki/Interrupt_request_(PC_architecture))
 * [EOI](https://en.wikipedia.org/wiki/End_of_interrupt)
 * [EOI in PIC](https://en.wikipedia.org/wiki/Programmable_Interrupt_Controller)
