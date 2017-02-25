LAB2
===

# 依赖

 * c++ static 知识
 * git知识
 * lab1我的攻略中所提到的指令
 * 页表,虚拟地址与物理地址的映射规则，和物理运作方式
 * [官方网址](https://pdos.csail.mit.edu/6.828/2016/labs/lab2/)

# 总览

 该lab 要实现
 * 建立结构体数组一一对应物理地址
 * 建立页目录和页表，填写项，一一对应虚拟地址
 * 实现把输入的虚拟地址和物理地址做映射的函数
 * 对内核部分的 虚拟地址完全映射到指定物理地址

**mit 的lab结构体名字为PageInfo,而sjtu的结构体名字为Page**

TA函数建议是mit的，我开始还以为sjtu的TA良心发现，比如SJTU加的`boot_map_region_large()`函数的注释 是复制上面的`(╯‵□′)╯︵┻━┻`,

测试`make grade`

SJTU 提交`make clean && make handin`

```
ftp: public.sjtu.edu.cn/upload/os2017/lab2/
user: georeth
password: public
```

查看我的修改`git diff lab2 finish_lab2`

```bash
>git diff HEAD^ HEAD  --stat
 kern/entry.S |   4 ++++
 kern/pmap.c  | 138 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++-----------------------------
 2 files changed, 112 insertions(+), 30 deletions(-)
```

## 准备

在上一个lab完成并add和commit后,开始lab2

首先从远端获取lab2,`git fetch --all`

切换到lab2分支`git checkout -b lab2 origin/lab2` 如果你使用的本仓库,在之前已经获取过了，则使用`git checkout lab2`即可

基于`lab2`分支建立新分支`git checkout -b mylab2`

将完成的lab2的提交与之合并`git merge mylab1 --no-commit`

通过`git diff --staged <filename>`检查 修改 是否

对冲突的文件编辑解决冲突，并add，在冲突的位置以`<<<<<<<<<<`,`========`和`>>>>>>>>`这样划分 其中上部分为lab2的,而下部分为mylab1的

完成合并后,add并commit,例如以下为我的合并过程

```bash
> git fetch --all
> git checkout -b lab2 origin/lab2
> git checkout -b mylab2
> git merge mylab1 --no-commit
> git status
> vim <conflict files>
> git diff --staged <filenames>
> vim <file which i want to modified>
> git add .
> git commit -m "Manual merge mylab1 to lab2"
```

整体的branch结构,理想构造如下，提交查看`git log --oneline --abbrev-commit --all --graph --decorate --color`

```bash
 from  teacher                    we write

                     * (origin/finish_lab6,finish_lab6)
                    _+ (base is lab6 and merge finish_lab5)
 (origin/lab6,lab6)* |
                   | * (origin/finish_lab2,finish_lab2)
                   |_+ (base is lab5 and merge finish_lab4)
 (origin/lab5,lab5)* |
                   | * (origin/finish_lab2,finish_lab2)
                   |_+ (base is lab4 and merge finish_lab3)
 (origin/lab4,lab4)* |
                   | * (origin/finish_lab2,finish_lab2)
                   |_+ (base is lab3 and merge finish_lab2)
 (origin/lab3,lab3)* |
                   | * (origin/finish_lab2,finish_lab2)
                   |_+ (base is lab2 and merge finish_lab1)
 (origin/lab2,lab2)* |
                   | * (origin/finish_lab1,finish_lab1)
                   |/
 (origin/lab1,lab1)*
```

# 开始

在完成分支建立和和并后 开始这个lab

这个lab要完成内存管理,分为两个部分

第一部分 是要让提供给kernel可以new/free内存的一个组件,你需要操作4096字节大小的空间作为页，你需要管理哪些已经分配，哪些没有被分配，多少进程分享一个页,你还需要写一个例程如何使用

第二部分是虚拟地址管理，映射，需要设立MMU页表根据作者提供的规范

[吐槽]SJTU的这种merge 提示信息并没有做对应的修改...在当年我不熟git的时候,就会很迷茫,不过这里照着我上面的**准备**中的merge过程即可

lab2新增的文件请**务必**阅读一遍
 * inc/memlayout.h
 * kern/pmap.c
 * kern/pmap.h
 * kern/kclock.h
 * kern/kclock.c

`memlayout.h`描述了虚拟地址空间,你需要通过编辑`pmap.c`来实现它

`memlayout.h`和`pmap.h`定义了页结构，你需要用它们来记录物理地址的页是否被使用 (**需要着重注意**,你可能还需要看`inc/mmu.h`)

`kclock.c`和`kclock.h`是操作电脑的电池始终 和 CMOS RAM 硬件(BIOS记录物理地址总量和其它数据)

`pmap.c`需要通过硬件设备来,知道有多少物理内存可用(该部分代码已经由作者完成),对于lab你不需要知道CMOS硬件的工作细节


## Part 1 : 物理页管理

操作系统需要知道RAM上物理地址哪一块是 free的 哪一块已经被使用。操作系统通过把物理地址按照一页为最小划分,用上MMU(内存 管理 单元 位于CPU上)进行管理

接下来我们要开始写物理页allocator,要使用`memlayout.h`中的`struct Page`的链表结构

你需要实现`kern/pmap.c`中的下列函数

```bash
boot_alloc()
mem_init() (only up to the call to check_page_free_list(1))
page_init()
page_alloc()
page_free()
```

`check_page_free_list()`和`check_page_alloc()`是用于测试正确性的,`assert()`对你调试很有帮助

lab 的文档里也没有说更多了，那么关于这个part,我们的设计来源为**代码中的注释**

---

通过阅读`inc/memlayout.h`中`struct Page`前后的注释，知道了它是一个单向链表`pp_ref`表示指针的总数,可以用`kern/pmap.h`中的`page2pa()`把`Page *`转换为物理地址,也就是这里的Page 和物理地址一一对应

`inc/memlayout.h`中还定义了很多对RAM分段的常量宏，表示的是物理地址的位置。还定义了两个类型`pte_t`和`pde_t`都是`uint32_t`,以及

```c
extern volatile pte_t vpt[];     // VA of "virtual page table"
extern volatile pde_t vpd[];     // VA of current page directory
```

其中volatile的[解释](http://en.cppreference.com/w/cpp/language/cv),易失,即不会被编译器优化,对于代码没有什么特殊的

再看`kern/pmap.h`对外提供的

PADDR(kva内核虚拟地址) 需要输入大于KERNBASE,返回相对KERNBASE的位置,即kva-KERNBASE

KADDR(pa物理地址)需要输入PGNUM(pa) < npages也就是 在正确的0~最大物理地址内,返回kva = pa+KERNBASE,即上下互为逆函数,封装为`page2kva(Page *)`返回`void *`

`ALLOC_ZERO = 1<<0;//For page_alloc, zero the returned physical page.`

一堆函数头的声明

```c
void  mem_init(void);
void  page_init(void);
struct Page *page_alloc(int alloc_flags);
void  page_free(struct Page *pp);
int page_insert(pde_t *pgdir, struct Page *pp, void *va, int perm);
void  page_remove(pde_t *pgdir, void *va);
struct Page *page_lookup(pde_t *pgdir, void *va, pte_t **pte_store);
void  page_decref(struct Page *pp);
void  tlb_invalidate(pde_t *pgdir, void *va);
pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);
```

两个互逆的转换函数page2pa和pa2page

---

再阅读`inc/mmu.h`的代码和注释

可以看到它提供段，页，位的相关常量宏和函数宏

理清以上三部分代码后开始阅读并实现`kern/pmap.c`,注释很友好的，不仅说明了变量的用途还说了在哪个函数中初始化的

首先映入眼帘的是作者已经写好的获取硬件关于内存信息的函数,帮我们设置好了`npages`和`npages_basemem`

接下来是`Set up memory mappings above UTOP.` 一堆声明,

`boot_alloc()`为物理地址allocator,而对于虚拟地址`page_alloc()`是真正的allocator

`void * boot_alloc(n)`接受参数n

 * 如果n>0且能分配n bytes的连续空间,则分配,不要初始化,返回kva
 * 如果n==0 返回下一个空闲页的地址 但不allocte
 * 如果越界 则panic，因为该函数在初始化时执行,`page_free_list`尚未建立 所以不需要检查它

通过`grep -nr "boot_alloc" *` 看到只有两个地方调用该函数，并且参数分别为PGSIZE和0，但我们还是友好的实现注释中提到的对齐

关于**注释删除**,最多把code here之类的删除了，解释功能的应当保留

有一点疑问如何保证到内存的位置的 靠的是`end' is a magic symbol automatically generated by the linker, which points to the end of the kernel's bss segment:`这个吗？

这里 注意到我们前面读到的函数 PADDR，KADDR 做了panic检查，所以实现为`KADDR(PADDR(ROUNDUP(nextfree+n, PGSIZE)))`整个逻辑实现为

```c
if(n > 0){
  char * kva_start = nextfree;
  nextfree = KADDR(PADDR(ROUNDUP(nextfree+n, PGSIZE)));
  return kva_start;
}else if(n==0){
  return nextfree;
}else{ //ERROR
  return NULL;
}
```

---

接下来到`mem_init()` 读注释 把该Remove 的删掉

`mem_init()`只会设置 address>= UTOP也就是为root服务的,用户的地址空间会稍后设置,UTOP~ULIM用户可读不可写，`>ULIM`用户不可读不可写

作者 首先申请了PGSIZE bytes大小的内存地址，并对这部分的值清0 ,根据注释和`kern/pmap.h`中的定义,`kern_pgdir`是一个线性(虚拟)地址

将PD(page dictionary)本身递归的插入页表，以虚拟地址UVPT(User read-only virtual page table)形成虚拟页表，然后作者说 你现在不用理解下面两行的具体意义(不过 关于该代码可以看看memlayout.h和mmu.h 里面有相关定义以及注释)

申请npages大小的struct Page数组到变量pages,这三个我们已经在上面的代码中阅读到了,核使用这个来跟踪页,即

```c
pages = (struct Page *) boot_alloc(npages * sizeof (struct Page));
```

**MIT JOSLAB明确要求要初始化为零**加上`memset(pages, 0, npages * sizeof(struct PageInfo));` 虽然读代码逻辑，不初始化并没有任何问题，以及也没有相关测试


---

开始实现`page_init()`根据`mem_init`中的注释,我们要初始化pages 也就是我们刚刚分配了空间的，我们要用它来记录哪些物理地址是空闲的，作者的代码展示了如何把所有的物理地址都表示为空闲的，但我们需要按照注释中所说的按不同段进行初始化。

回看`inc/memlayout.h`中画的内存结构的设计,我们以一个页 为单位进行分配，所以这里一个`Page *`对应一个页是否使用，所以注释中的四步都要除以1个PGSIZE

这里 回看一下函数`i386_detect_memory()`可以得知`npages = (EXTPHYSMEM / PGSIZE) + npages_extmem`

来看一下分层

 * `0x000000~0x0A0000(npages_basemem*PGSIZE or IOPHYSMEM)`,basemem，是可用的。`npages_basemem`记录basemem的页数
 * `0x0A0000(IOPHYSMEM)~0x100000(EXTPHYSMEM)`，这部分叫做IO hole，是不可用的，主要被用来分配给外部设备了。
 * `0x100000(EXTPHYSMEM)~0x???`,`npages_extmem`记录extmem的页数

也就有了上面npages的表达式,实现如下 为了保持格式每个都用了for,不清楚`chunk_list`的用途 暂时保留原样[TODO]

```c
size_t i = 0;
// 1)
for (; i < 1; i++) {
  pages[i].pp_ref = 1;
  pages[i].pp_link = NULL;
}
// 2)
for (; i < npages_basemem; i++) {
  pages[i].pp_ref = 0;
  pages[i].pp_link = page_free_list;
  page_free_list = &pages[i];
}
// 3)
for (; i < (EXTPHYSMEM / PGSIZE); i++) {
  pages[i].pp_ref = 1;
  pages[i].pp_link = NULL;
}
// 4)
for (; i < npages; i++) {
  pages[i].pp_ref = 0;
  pages[i].pp_link = page_free_list;
  page_free_list = &pages[i];
}
chunk_list = NULL;
```

---

继续读`mem_init()` 做了3个check [TODO 具体check的内容]

下面开始实现`page_alloc` 注释中有清零条件`(alloc_flags & ALLOC_ZERO)` ,函数提示`page2kva and memset`以及超界返回NULL

这里返回的是`Page *`也就是结构体指针，我们从pages 链表上取一页就行了，而如果要初始化为0 需要把对应kva的位置初始化则会用到提示的两个函数

思路为 从链表上取头部 如果非空且需要初始化，则通过辅助函数初始化为零。对`free_list`移动并返回申请到的Page，实现如下

```c
struct Page *
page_alloc(int alloc_flags)
{
  if( page_free_list == NULL )
    return NULL;

  if( alloc_flags & ALLOC_ZERO )
    memset(page2kva(page_free_list),0,PGSIZE);

  struct Page * ret_page_alloc = page_free_list;
  page_free_list = page_free_list->pp_link;
  //ret_page_alloc->pp_ref = 0 ; [TODO]
  return ret_page_alloc;
}
```

**FIX WITH MIT-JOS**

> 以上代码跑过了SJTU的grade,然后跑MIT的grade崩在`kernel panic at kern/pmap.c:815: assertion failed: pp1->pp_link == NULL`

> 也就是说mit会检测alloc出来的page的`pp_link`

> 将上方注释有TODO的换为以下即可

```
   ret_page_alloc->pp_ref = 0 ;
   ret_page_alloc->pp_link = NULL ;
```



紧接着是free 和alloc对应 需要把一页 重新加入`free_list`,实现如下

```c
void
page_free(struct Page *pp)
{
  pp->pp_link = page_free_list;
  page_free_list = pp;
}
```

到这里我的Part 1就完成了，可是很不幸通过运行发现`check_page_free_list`在`memset(page2kva(pp), 0x97, 128);`让我的实现崩了

也就是说 我把一个已经使用的位置认为为未使用,因为当前在一个kern态,可以读写任何位置的内存,回顾我们修改以及阅读过的函数,发现`boot_alloc(0)`可以获取到下一个空闲位置 也就是能知道最后一个使用位置，因此对pages的初始化修改

```c
// 4)
for (; i < PGNUM(PADDR(boot_alloc(0))); i++) {
  pages[i].pp_ref = 1;
  pages[i].pp_link = NULL;
}
for (; i < npages; i++) {
  pages[i].pp_ref = 0;
  pages[i].pp_link = page_free_list;
  page_free_list = &pages[i];
}
```

至此fix一个bug以后 可以通过`make qemu-nox`看到

```bash
check_page_alloc() succeeded!
kernel panic at kern/pmap.c:...
```

测试`make grade` MIT可以看到

```
Physical page allocator: OK
```

lab1完成的工作
 * 硬件初始化检查 设置
 * bios 运行
 * 模式16切换到32位
 * 手工设置了pgtable `0x000000~0x3ff000`,`Map VA's [0, 4MB) to PA's [0, 4MB)`,`Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)`，也就是0xf0000000+0~4MB 映射到 0+4MB
 * 开启了页,虚拟地址模式

至此Part I部分的工作
 * 我们首先获取了内存信息 由原作者完成 (填写了`npages`, `npages_basemem`等参数)
 * 申请了一页来放pgdir 初始化由原作者完成
 * 申请了能表示整个内存信息的 Page 数组
 * 对申请的数组初始化，使它能表示当前的内存使用状况，连接成空闲链表到`page_free_list`上
 * 提供单页的申请和释放函数

疑惑 [TODO]
 * end 来保证初始位置？
 * 其它代码段等 函数的数据 在内存的什么地方,是上面手工映射的位置？
 * 整理结构`www.draw.io`

# Part 2

在往下继续做之前，需要熟悉x86保护模式下的内存管理结构,给了我们个链接，[这个链接可以看图](https://pdos.csail.mit.edu/6.828/2007/readings/i386/c05.htm)，找别的资料不如耐心把这英文的看完

通过以上的链接以及网页下方的介绍了解 `逻辑地址(虚拟地址)->线性地址->物理地址`

通过阅读也就是说 线性地址(linear)=逻辑地址(virtual)+段首地址()

在x86也保留这个习性,也就是这里实际是`线性地址=逻辑地址+ 0x00000000`,逻辑地址和线性地址相等,因此该部分的关注点在于利用页来做转换，看做虚拟地址=线性地址就好了

回想一下lab1....哎 我觉得这句话应该放在lab早一些的地方，前面一直对于映射有些懵逼，

lab1 做的是手工映射了4MB[见上方的总结]，这个lab我们的任务是映射`0xf0000000+0~256MB[虚拟/线性]`到`0~256MB[物理]`

接下来，是线性地址到物理地址的部分的转换,图片见上方连接英文文档内部

 * 首先CPU得到一个地址，看看页的开关开没有(该开关在CR0上，见 entry.S的设置) 没有的话就直接视为物理地址访问了
 * 而打开了的话则 把这个地址给MMU
 * MMU 去TLB(目前的lab并没有这个东西吧 这是类似cache一样的)里找(lab1手工映射了一部分) 如果找到了那就好咯，访问对应物理地址
 * 没有找到的话，告诉CPU，CPU去安排该虚拟地址对应的物理地址，安排好了,写入TLB,返回MMU

对于具体的一个地址32位 [31..22]为DIR，[21..12]为page，[11..0]为offset
 * 第一步通过CR3存的地址(entry.S中设置)找到PAGE DIRECTORY
 * 通过DIR作为偏移量 定位到 PAGE DIRECTORY中的具体一项DIR ENTRY
 * 以该项的值找到PAGE TABLE
 * 通过page作为偏移量 定位到PAGE TABLE的具体一项 PAGE TABLE ENTRY
 * 以该项的值定位到物理页
 * 以offset 定位到该物理页的物理地址

页表的每一项是一个32位数，页表本身也是一个页，因此4KB页能存4KB÷32bit=1024项，一项对应一页，一共可以映射1024×4KB=4MB的虚拟内存

上面的DIR，page，offset分化则[31..12]就算分为两层 也是共同确定一页，可以确定2的20次方页，也就一共可以映射2^20×4KB=4GB的虚拟内存，也就是1M的页，[实际上地址长度，和按什么编址才与可映射的虚拟内存大小直接相关4GB=2^32(虚拟地址长度) * 1B(按字节编址)]

当前的page directory的物理地址储存在CPU的CR3(page directory base register (PDBR))中，给MMU读的。MMU的软件层来决定如何使用页表

接下来展示了上述PAGE DIRECTORY中的具体一项DIR ENTRY的结构[31..12]是地址[11..0]是权限等的设置位详细见上方英文连接的介绍 其中作为`页表`的[0]位必须为1

搜了一堆资料,目前看来 page table 和page directory 的每一项的[31..0]的结构都一样，也就是说 它们两个实际没什么差别或者我更喜欢叫它们page table 1和page table 0, 区别在于一个的地址指向page table一个指向页，再简化就是 `CR3->PAGETABLE->PAGETABLE->PAGE`,也就是一个虚拟地址通过划分两次在PAGETABLE中定位后找到具体页，对于地址更长的或者页分化不一样的甚至可能`CR3->PAGETABLE->PAGETABLE->PAGETABLE->PAGETABLE->PAGETABLE->PAGE`

哎，那不同程序相同虚拟地址怎么映射到不同物理地址呢？搜了以后告诉我，装载地址才是虚拟地址，在不同程序运行时候也是没有相同的虚拟地址！也就是二进制编码中的地址和实际运行的地址并不相同。

然后建议使用GDB的xp命令(用来直接看物理地址数据的),以及附上了参考命令的[链接](http://ipads.se.sjtu.edu.cn/courses/os/2017/labs/labguide.html#qemu)

```bash
xp/Nx paddr
  Display a hex dump of N words starting at physical address paddr. If N is omitted, it defaults to 1. This is the physical memory analogue of GDB's x command.
```

说的提供了`info pg`和`info mem`指令但我试了试，并没有`info pg`。。。`_(:з」∠)_`

找到了！！！不需要`make gdb`,在`make qemu-nox`正确运行看到`K>`以后，按`ctrl+A`再按c再按回车就会看到`(qemu)`，也就是内置的一个？？？

在这里就可以输入`info pg`，`info mem`等等了！！！现在输入可以得到,也就看到了手工map的0~4M部分

```bash
(qemu) info mem
00000000-00400000 00400000 -rw
f0000000-f0400000 00400000 -rw
(qemu) info pg
 |-- PTE(000400) 00000000-00400000 00400000 -rw
 |-- PTE(000400) f0000000-f0400000 00400000 -rw
(qemu) info registers
EAX=ffffffff EBX=f0117544 ECX=ffffffff EDX=f0100260
ESI=f010023e EDI=f0117340 EBP=f0114dd8 ESP=f0114dc0
EIP=f0100295 EFL=00000046 [---Z-P-] CPL=0 II=0 A20=1 SMM=0 HLT=0
ES =0010 00000000 ffffffff 00cf9300
CS =0008 00000000 ffffffff 00cf9a00
SS =0010 00000000 ffffffff 00cf9300
DS =0010 00000000 ffffffff 00cf9300
FS =0010 00000000 ffffffff 00cf9300
GS =0010 00000000 ffffffff 00cf9300
LDT=0000 00000000 0000ffff 00008200
TR =0000 00000000 0000ffff 00008b00
GDT=     00007c4c 00000017
IDT=     00000000 000003ff
CR0=e0010011 CR2=00000000 CR3=00115000 CR4=00000000
DR0=00000000 DR1=00000000 DR2=00000000 DR3=00000000
DR6=ffff0ff0 DR7=00000400
FCW=037f FSW=0000 [ST=0] FTW=00 MXCSR=00001f80
FPR0=0000000000000000 0000 FPR1=0000000000000000 0000
FPR2=0000000000000000 0000 FPR3=0000000000000000 0000
FPR4=0000000000000000 0000 FPR5=0000000000000000 0000
FPR6=0000000000000000 0000 FPR7=0000000000000000 0000
XMM00=00000000000000000000000000000000 XMM01=00000000000000000000000000000000
XMM02=00000000000000000000000000000000 XMM03=00000000000000000000000000000000
XMM04=00000000000000000000000000000000 XMM05=00000000000000000000000000000000
XMM06=00000000000000000000000000000000 XMM07=00000000000000000000000000000000
```

这里呢用`uintptr_t`表示虚拟地址，而`physaddr_t`表示物理地址(虽然这两个实际都是`uint32_t`)

但是kernel只应当把`uintptr_t`转换为指针,也就是虚拟地址的指针，而物理地址要通过MMU和配置的表等去转换，而不当kernel直接操作，如果cpu直接把`physaddr_t`地址转换为指针，实际上硬件会将其视为虚拟地址，可能访问到错误的位置从而挂掉

|C type|Address type|
|---|---|
|`T*`| Virtual|
|`uintptr_t` |  Virtual|
|`physaddr_t` |  Physical|

### Question

Assuming that the following JOS kernel code is correct, what type should variable x have, `uintptr_t or physaddr_t`?

```c
 mystery_t x;
 char* value = return_a_pointer();
 *value = 10;
 x = (mystery_t) value;
```

...这个问题不愧是外国人出的教你1+1问你1+1，此处应为`uintptr_t`因为对于程序来说只有虚拟地址

然而kernel有些时候需要直接对一个具体的物理地址进行操作，比如对一个page table进行映射，需要申请物理地址来储存page directory然后初始化那块内存

kernel和普通程序一样无法直接把虚拟地址转换为物理地址，也就有了lab1的手工map的前面4MB，一一映射，这样有了一一对应就可以假装直接访问一样的访问了，感觉想法萌萌哒，你可以使用`KADDR(pa)`得到对应的虚拟地址

然后。。。它又说全局变量之类由`boot_alloc()`得到的都在0xf0000000之后,,,,,,困扰了我半天，so,那局部变量和栈在哪呢？[TODO]

直接减0xf0000000即是物理地址，应该用`PADDR(va)`来做这个减操作

然后讲`引用计数`也就是代码里之前设置为0或者1的`pp_ref`，说未来的操作会对这个值进行改变，当值变为0时应当被free掉

In general, this count should equal to the number of times the physical page appears below UTOP in all page tables (the mappings above UTOP are mostly set up at boot time by the kernel and should never be freed, so there's no need to reference count them).We'll also use it to keep track of the number of pointers we keep to the page directory pages and, in turn, of the number of references the page directories have to page table pages.

哦豁 感觉自己的`page_free_list`的部分代码写得有点多于，懒得改了

**注意** `page_alloc`返回的应该在`pp_ref`值上为0，你应当在`page_insert`或者直接在返回时对`pp_ref`进行增加

## 页表管理

作者建议按照下面的顺序进行实现函数，`check_page()`是用来测试正确性的 会在`mem_init()`中调用

```
pgdir_walk()
boot_map_region()
page_lookup()
page_remove()
page_insert()
```

---

`pgdir_walk`注释里说 我们要做的是走一个二级页表，该函数需要返回一个PTE指针(linear address)

```bash
pmap.c: assert(pgdir_walk(kern_pgdir, (void*)PGSIZE, 0) == ptep+PTX(PGSIZE));
pmap.c: assert(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U);
pmap.c: assert(!(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U));
pmap.c: ptep = pgdir_walk(kern_pgdir, va, 1);
pmap.c: pgdir_walk(kern_pgdir, 0x0, 1);
```

`kern_pgdir`也就是我们前面init部分最开始申请的空间，当时作者说不需要弄清那行代码的意思,这里我们做的应该是对其类似的操作

吐槽一下c没有false但是注释里写的false而不是0

再看`pgdir_walk`上的注释说了对参数的不同处理方法以及,`inc/mmu.h`辅助宏的使用,在上方理清了`线性地址到物理地址的转换`,那么这里也就是拿到一个虚拟地址va

和一个页目录，也就是最外层页表，需要返回一个指向下一层页表的指针,也就是下一层页表的地址。

那么 根据上面的分析 以下步骤
 * 把va分段提取 DIR
 * 根据DIR 的到 一个具体的ENTRY
 * 如果 需要分配 且没分配 则 分配
 * 否则返回地址ENTRY中记录的地址，或者没分配返回NULL

分配的过程
 * 申请一个页用Page结构,**这个页用于作为页表**，注意清空
 * 获取该空闲页 的真实物理地址,刚好它有一个shift操作 能和页表项的直接吻合，用或操作对权限位等进行设置
 * 修改好页目录项也就好了
 * 最后 在页目录项写好以后 返回 指向的页表中的根据va分段算出page得到的具体的一个pte

这样就有一个空的 页表，和指向它的新的页目录项，

除了`PTE_P`以外的各个权限位具体[TODO]

实现代码如下

```c
pte_t *
pgdir_walk(pde_t *pgdir, const void *va, int create)
{
  pde_t * target_pde = &pgdir[PDX(va)];
  pte_t * target_pt = NULL;
  if(!(*target_pde & PTE_P) && create){
    struct Page * pp = (struct Page *)page_alloc(ALLOC_ZERO);
    if(pp == NULL){
      return NULL;
    }
    pp->pp_ref++;
    *target_pde = page2pa(pp)|PTE_P|PTE_W|PTE_U;
  }
  if(!(*target_pde & PTE_P))
    return NULL;
  target_pt = KADDR(PTE_ADDR(*target_pde));
  return &target_pt[PTX(va)];
}
```

---

`boot_map_region` 虽然grep并没有找到哪里会调用它，但根据注释还是写一写，这里说把`[va, va+size)`的虚拟地址映射到`[pa, pa+size)`的物理地址,要设置`perm|PTE_P`位,这里的参数size是PGSIZE的倍数,这个是为了设置静态的在UTOP之上的映射,它不应该修改`pp_ref`


那用上刚刚的函数 可以得到一个 page table entry，我们只要把对应page table entry中记录的地址 写为pa即可

```c
static void
boot_map_region(pde_t * pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm)
{
  int i;
  for (i = 0; i < size; i += PGSIZE) {
    pte_t * pte = pgdir_walk(pgdir, (void *) (va + i), 1 );
    *pte = (pa + i) | perm | PTE_P;
  }
}
```

这样 我们就完全 把虚拟地址和物理地址连接上了

---

`page_lookup`返回`Page *`,在参数中实际还返回了 虚拟地址va对应的 页表项pte的地址,提示使用函数`pa2page`,功能是通过va获取`Page *`和`pte的地址`

 * 首先 通过该va算出对应的pte的地址 由之前实现的函数`pgdir_walk`完成
 * 然后 该干嘛干嘛

实现如下

```c
struct Page *
page_lookup(pde_t *pgdir, void *va, pte_t **pte_store)
{
  pte_t * pte_address = pgdir_walk(pgdir, va, 0 );
  if(pte_store){
    *pte_store = pte_address;
  }
  if(pte_address && (*pte_address & PTE_P))
    return pa2page(PTE_ADDR(*pte_address));
  return NULL ;
}
```

---

`page_remove` 把va对应的物理地址解绑，如果对应的没有分配就啥也不干，建议函数`page_lookup,tlb_invalidate, page_decref`

细节
 * `pp_ref`需要减1
 * 如果`pp_ref==0`需要被free掉 用part1的函数
 * 如果有对应的 pte也需要设为0，我还以为只清一个`PTE_P`位呢
 * The TLB must be invalidated if you remove an entry from the page table.

看了一下我们之前没见过的两个函数
 * `tlb_invalidate` 是为第四点细节服务
 * `page_decref`是为 减1和free服务的

那这样不就so easy,实现如下

```c
void
page_remove(pde_t *pgdir, void *va)
{
  pte_t * pte = NULL;
  struct Page * page = page_lookup(pgdir, va, &pte);
  if(!page)
    return ;

  page_decref(page);
  tlb_invalidate(pgdir, va);
  *pte = 0;
}
```

---

最后一个`page_insert`,和`boot_map_region`差不多的pte要求，把物理地址和虚拟地址做映射

 * 如果va已经映射了 就解绑？还要继续
 * 如果对应的pgdir之类的里面都没有的话 就增加 [这翻译感觉翻了半天 也就是`page_walk`的最后一个参数]
 * 如果成功映射就`pp_ref`+1
 * TLB balabala

想一下一个物理地址被多个virtual memory映射是什么体会，请在知乎分享....哦不对，是说 同一个虚拟地址到物理地址的多次映射，会怎么样，有一种优雅的方法？？？？？但不要写特判,吊人胃口真是讨厌[TODO???]

推荐函数`pgdir_walk, page_remove, page2pa` (推荐函数什么的感觉很棒

返回值 成功0，失败`-E_NO_MEM`

 * 首先 `pgdir_walk`获得pte 失败就返回`-E_NO_NEM`
 * 先对`pp->pp_ref`增加
 * 如果有，再解除原来的va关系
 * 最后建立新的映射

二三条不能换，保证持有状态,实现如下

```c
int
page_insert(pde_t *pgdir, struct Page *pp, void *va, int perm)
{
  pte_t *pte = pgdir_walk(pgdir, va, 1 );
  if (!pte)
    return -E_NO_MEM;

  pp->pp_ref++;
  if (*pte & PTE_P)
    page_remove(pgdir, va);

  *pte = page2pa(pp) | perm | PTE_P;
  return 0;
}
```

---

Part 总结

咦 为什么我隐约记得去年没这么简单 还有`alloc_npages`之类的,

利用最底层的alloc 这样我们实现一个 va到pa的映射函数给上层
 * 本层实现的有 页表的走(难道该用for感觉walk的翻译是？)？
 * 实际的创建页 也就va和pa的映射 同时把Page结构用上

这事`make qemu-nox`就可以看到

```bash
check_page_alloc() succeeded!
check_page() succeeded!
```

至此MIT JOS LAB执行`make grade`可以看到`Page management: OK`

#  Part 3: Kernel Address Space

核地址空间,,,

JOS把32位线性地址分为两部分，用户地址(物理地址高 虚拟地址低)(lab 3 会使用)，内核地址(物理地址低 虚拟地址高),具体分割线请看`inc/memlayout.h`的`ULIM` 我特地把它移过来了 :-) 恍恍惚惚红红火火哈哈哈哈

```
 *    ULIM     ------> +------------------------------+ 0xef800000      --+
```

可以看到为内核地址预留了约256MB，内核地址对用户地址是完全控制,还给了一个[彩色版](http://ipads.se.sjtu.edu.cn/courses/os/2017/labs/jos_layout.pdf)


## 权限和故障隔离

在页表上设置权限位来保证 用户态的错误不会操作到内核态的数据，从而引起kernel崩溃

用户态对ULIM以上的部分无权限

对于`[UTOP,ULIM)`之间的 内核 和 用户 都有权限读 都无权限写[其实在kernel开始的时候写的，写完就跑真刺激]，这部分用来表示内核数据结构的一些信息

低于UTOP的就算是 用户可以随便玩的了，在内核干涉之前随便玩，玩坏了你打我呀

## 那么开工

现在开始把UTOP以上的虚拟地址进行适当的映射，把`mem_init()` 中`check_page()`调用以后的代码实现

哇 这么多要实现的，吓得我搜了一下`Your code goes here` 还好只有三条

第一个做映射，grep到UPAGES就是一个分界线 也就是一个地址

```bash
> grep -r "UPAGES" *
inc/memlayout.h: *    UPAGES    ---->  +------------------------------+ 0xef000000
inc/memlayout.h:#define UPAGES(UVPT - PTSIZE)
```

那么要映射这一块的地址，通过看`inc/memlayout.h`的图知道了这一块大小为PTSIZE,再利用刚刚实现的一块虚拟到一块物理的映射

也就是`boot_map_region(kern_pgdir,UPAGES, PTSIZE, PADDR(pages), PTE_U);`

有一点懵逼的是这里注释叫我`(ie. perm = PTE_U | PTE_P)` 可是`PTE_P`不是该`boot_map_region`来负责么，反正用或运算结果都一样

---

下面两句同理 对照`inc/memlayout.h`以及注释确定每一个变量，和上面一样的映射方法，做了对齐 三句总的为

```c
 boot_map_region(kern_pgdir,UPAGES            , PTSIZE   , PADDR(pages)    , PTE_U);
 boot_map_region(kern_pgdir,KSTACKTOP-KSTKSIZE, KSTKSIZE , PADDR(bootstack), PTE_W);
 boot_map_region(kern_pgdir,KERNBASE          , -KERNBASE, 0               , PTE_W);
```

这里注意`-KERNBASE` 这里利用 位数只有32位

那么`2^32-KERNBASE`的低三十二位等于`-KERNBASE`,补码大法好啊

又传过去的是`size_t`也就是`uint32_t`也就是`unsigned int` 所以也就刚刚好 `2^32-KERNBASE`的值 = `-KERNBASE`的补码

在这里`make grade`已经有(70/80)分了,还差`Large kernel page size`

---

至此 MIT JOS LAB `make grade` 已经满分,以下为SJTU的部分[不过4M页的使用是MITJOSLAB的challenge :-)只是mit的make grade 里没有评测]

```
>make grade
  Physical page allocator: OK
  Page management: OK
  Kernel page directory: OK
  Page management 2: OK
Score: 70/70
```

### Question

* 现在PD里已经有哪些了? 映射了哪些地址 指向了哪些地方？填表

PD是啥，是为虚拟地址服务和虚拟地址关联的,PDX在`inc/mmu.h`中取的虚拟地址高10位,在通过`inc/memlayout.h`可以找到上面三个`boot_map_region`的虚拟地址和物理地址

```bash
 UPAGES             : [0xef000000,0xef400000)    ->[pages    -0xf0000000,pages    -0xefc00000) cprintf得到[0x0011a000,0x0015a000)
 KSTACKTOP-KSTKSIZE : [0xefbf8000,0xefc00000)    ->[bootstack-0xf0000000,bootstack-0xefff8000) cprintf得到[0x0010e000,0x00116000)
 KERNBASE           : [0xf0000000,0x100000000)   ->[0,0x10000000)
```

取虚拟地址前十位分别是508`0111 1111 00b`,510`0111 1111 10b`,960~1023`0x1111 0000 00~0x1111 1111 11`

还有作者之前手工映射的UVPT`[0xef400000,0xef800000)`->`[kern_pgdir-0xf0000000,kern_pgdir-0xefc00000)`

|Entry|Base Virtual Address |Points to (logically):|
|---|---|---|
|1023|0xffc00000|Page table for top 4MB of phys memory|
|1022|0xff800000|?|
|.| ?| ?|
|.| ?| ?|
|.| ?| ?|
|2| 0x00800000| ?|
|1| 0x00400000| ?|
|0| 0x00000000| [see next question]|


这里想了半天[这里怎么就和Page table连上了 TODO]

* 我们把内核和用户环境放在同一个地址空间，为什么用户程序不能读到内核的数据，哪一个特殊机制保障了内核内存?

靠的是页表中的权限位`PTE_U` ，在MMU处理虚拟地址到内核地址的时候会检验该权限位，

* 这个操作系统最大能用多少物理内存？

这个qemu模拟得到`npages=0x40ff`,`PGSIZE=0x1000`, `容量≈40MB`

或者说这里为`pages`第二次映射的最大空间为`PTSIZE=PGSIZE*NPTENTRIES=0x400000`字节，可以有`0x400000B/sizeof(struct Page)B=0x80000`项，一项一页可以有`0x80000*0x1000B=0x80000000B=2GB<32位对应的4G`

所以最大2GB

* 我们为了管理内存用了多少内存?假设用完了上述的2GB?如何减少使用？

有三部分，用于转换的 PD，PT和用于记录空闲和引用的pages

PD:`1024*32bit=4K`

PT:`0x80000*32bit=2MB`

pages:`0x80000*8B=4MB`

使用大页，减少pages开销

* 回头看看`kern/entry.S and kern/entrypgdir.c`中设置的页表，在我们启用页模式的时候，eip依然是一个小于1MB的。什么时候我们的eip大于KERNBASE? 什么让我们在开启页模式以后，eip处于低位时依然可行，这个转换有必要吗?

.....这就是我上面提到了不下3遍的 对`0~4MB`的两个映射...有必要 因为page模式下cpu到mmu的全是虚拟地址，没有映射会找不到对应物理地址而出错

## Exercise 6.

我们消耗了很多物理页来记录对KERNBASE的映射.用`PTE_PS ("Page Size")位`做一个更加空间效率高的 来映射顶部的256MB PD.[参考文档](http://ipads.se.sjtu.edu.cn/courses/os/2015/readings/ia32/IA32-3A.pdf)

实现`boot_map_region_large()`函数,并且在loading cr3以前启用PSE(page size extension ) before loading cr3. 然后调用`boot_map_region_large()`来映射 KERNBASE.

可以在参考文档中3.9章看到 它会变成`[31..22][21..0]`的划分，一个页变成了4MB页,并且看到`PSE flag (bit 4) in control register CR4 and the PS flag in PDE — Set to 1 to enable the page size extension for 4-MByte pages. `

也就是我们的走页表过程会变成,只有`CR3->PD->PAGE`,

那一共三个步骤
 * 实现`boot_map_region_large` 这里按照上面只有一层PD也就是PDE直接指向页
 * 修改PSE位
 * 替换 原来KERNBASE的`boot_map_region`函数

代码如下

```c
static void
boot_map_region_large(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm)
{
  int i;
  for (i = 0; i < size; i += 0x400000) {
    pde_t * target_pde = &pgdir[PDX(va+i)];
    if((*target_pde & (PTE_P | PTE_PS)) != (PTE_P | PTE_PS)){
      if(*target_pde & PTE_P){
        cprintf("DANGEROUS!COVER OLD PT,UNTRACK PT\n");
      }
      *target_pde = (pa + i) | perm | PTE_P | PTE_PS;
    }
  }
}
```

```assembly
# Turn on page size extension.
movl  %cr4, %eax
orl $(CR4_PSE), %eax
movl  %eax, %cr4
```

```c
boot_map_region_large(kern_pgdir,KERNBASE          , -KERNBASE, 0               , PTE_W);
```

这里可以看到 也只是指向，并没有实际的4MB的管理，我的代码中也体现了对已经部分映射的担心=。= 感觉可能还是需要一些处理机制
 * 如果原来有指向PT
 * 则把原来两层的内部的指向备份过来
 * 指向新的
 * 清除原来的PD

不过没有相关CHECK函数 也就没写

以及页申请也没有支持4MB页的`_(:з」∠)_`

到这里`make grade`也就`80/80`了

最后通过命令欣赏欣赏

```bash
(qemu) info mem
ef000000-ef400000 00400000 ur-
ef7bc000-ef7bd000 00001000 urw
ef7bd000-ef7be000 00001000 ur-
ef7be000-ef7bf000 00001000 urw
ef7c0000-ef800000 00040000 -rw
efbf8000-efc00000 00008000 -rw
f0000000-00000000 10000000 -rw
(qemu) info pg
PDE(001) ef000000-ef400000 00400000 ur-
 |-- PTE(000400) ef000000-ef400000 00400000 ur-
PDE(001) ef400000-ef800000 00400000 ur-
 |-- PTE(000001) ef7bc000-ef7bd000 00001000 urw
 |-- PTE(000001) ef7bd000-ef7be000 00001000 ur-
 |-- PTE(000001) ef7be000-ef7bf000 00001000 urw
 |-- PTE(000040) ef7c0000-ef800000 00040000 -rw
 |-- PTE(000008) efbf8000-efc00000 00008000 -rw
 PDES(040) f0000000-00000000 10000000 -rw
```


---

# Challenge[TODO]

```c
/*
Challenge! Extend the JOS kernel monitor with commands to:

Display in a useful and easy-to-read format all of the physical page mappings (or lack thereof) that apply to a particular range of virtual/linear addresses in the currently active address space. For example, you might enter 'showmappings 0x3000 0x5000' to display the physical page mappings and corresponding permission bits that apply to the pages at virtual addresses 0x3000, 0x4000, and 0x5000.
Explicitly set, clear, or change the permissions of any mapping in the current address space.
Dump the contents of a range of memory given either a virtual or physical address range. Be sure the dump code behaves correctly when the range extends across page boundaries!
Do anything else that you think might be useful later for debugging the kernel. (There's a good chance it will be!)
Challenge! Extend the JOS physical page allocator to support page coloring.

Page coloring assigns each page a "color", so that pages have different color reside in different part of cache. This technique ensures contiguous virtual pages do not contend for the same cache line.

Read this document to figure out what page coloring is.

Assume cache is set-associative:

          -----------------------------------------------
 in page: | PPN (20)              |    page offset (12) |
   -----------------------------------------------
 in cache:| label (18) | set index (9) | line offset(5) |
   -----------------------------------------------

Determine the number of colors, then implement a function struct PageInfo *alloc_page_with_color(int alloc_flags, int color). This function should return physical page with specific color.

Address Space Layout Alternatives

The address space layout we use in JOS is not the only one possible. An operating system might map the kernel at low linear addresses while leaving the upper part of the linear address space for user processes. x86 kernels generally do not take this approach, however, because one of the x86's backward-compatibility modes, known as virtual 8086 mode, is "hard-wired" in the processor to use the bottom part of the linear address space, and thus cannot be used at all if the kernel is mapped there.

It is even possible, though much more difficult, to design the kernel so as not to have to reserve any fixed portion of the processor's linear or virtual address space for itself, but instead effectively to allow allow user-level processes unrestricted use of the entire 4GB of virtual address space - while still fully protecting the kernel from these processes and protecting different processes from each other!

This completes the lab. Type make grade in the lab directory for test, then type make handin to pack the files, rename the lab2-handin.tar.gz file to {your student id}.tar.gz, and follow the directions to upload the tarball onto ta's ftp.
*/
```

# 总结

* `Part I`  是实现了 让我们不再看到硬件 对齐上面封装，用Page 和与page相关的宏来管理
* `Part II` 则实现了 页表相关，指定虚拟地址和物理地址的映射接口，还提供指定Page和va 创建/查看/删除页的接口
* `Part III`把非用户的虚拟地址 完全映射掉,实际的pages等，在物理地址中还是只有一份，但现在应该是有2~3个虚拟地址都指向它[和它物理地址相等的，KERNBASE以上的，最后新建的映射的]

# 参考

 * [Translation lookaside buffer](https://en.wikipedia.org/wiki/Translation_lookaside_buffer)
 * [Page Translation](https://pdos.csail.mit.edu/6.828/2007/readings/i386/s05_02.htm)
 * [Chapter 5 Memory Management](https://pdos.csail.mit.edu/6.828/2007/readings/i386/c05.htm)
 * [Memory management unit](https://en.wikipedia.org/wiki/Memory_management_unit#IA-32_.2F_x86)
 * [Translating a Virtual Address to a Physical Address](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.den0024a/ch12s03.html)
 * [Page_table](https://en.wikipedia.org/wiki/Page_table#The_translation_process)
 * [How are same virtual address for different processes mapped to different physical addresses](http://stackoverflow.com/questions/3435606/how-are-same-virtual-address-for-different-processes-mapped-to-different-physical)
 * [In virtual memory, can two different processes have the same address?](http://stackoverflow.com/questions/3552633/in-virtual-memory-can-two-different-processes-have-the-same-address)
 * [Page Size Extension](https://en.wikipedia.org/wiki/Page_Size_Extension)
 * [逻辑地址、虚拟地址、物理地址以及内存管理](http://blog.csdn.net/newcong0123/article/details/52792070)

# OLD COMMIT LOG

```bash
commit beecc9ad9522a10797fee0f9f00bb6a5bdbb4db5
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Wed Mar 29 19:57:04 2017 -0700

    PART III with JOS

commit 18ebc5e0f4ac15da36626f7fc1018e2d852a4918
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Wed Mar 29 19:51:26 2017 -0700

    PART II with JOS

commit b5c2df39215e594b9b547e0d9be606bed496352e
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Wed Mar 29 19:03:58 2017 -0700

    PART I with JOS

commit 6e48e924956830101b86d5d6498e4c9f40878c5c
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Mon Mar 27 06:53:07 2017 +0800

    fix README of white space

commit acb3bac72ab6c542d29d074424a8fd831181a21f
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Mon Mar 27 06:51:10 2017 +0800

    fix README clear cout

commit 8d1b9e2dc827d9ac5859fd82dd767ad74e60373e
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Mon Mar 27 06:07:48 2017 +0800

    finish part III with SJTU JOS

commit 7ea961db160e53a890f85cd29dc027a68d6bdc61
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Sun Mar 26 07:22:02 2017 +0800

    PART II finish with SJTU JOS

commit 69f344808a9dca8ba3aee6bcecb9a1f937ad1fde
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Mon Mar 20 00:33:13 2017 +0800

    add INTRODUCTION

commit 65047b9e5f89ce172227b8b67ba57efb4994bcb9
Author: yexiaorain <yexiaorain@gmail.com>
Date:   Mon Mar 20 00:31:36 2017 +0800

    add README.md

```
