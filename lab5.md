lab 5
---

[文件系统](https://pdos.csail.mit.edu/6.828/2016/labs/lab5/)

# 总

测试`make grade`,

提交

```
> make handin

ftp:  public.sjtu.edu.cn/upload/os2017/lab5/
user:  georeth
password: public
```

# 准备

我执行的准备工作命令

```bash
> git stash
> git fetch
> git checkout -b lab5 origin/new_lab5
> git checkout -b mylab5
> git merge mylab4
> vim kern/env.c
> git commit -m "manual merge mylab4 to lab5"
> git stash pop
```

# 开始

jos 要实现一个微内核的文件系统,也就是把文件系统放在用户空间,其它程序希望调用时通过IPC调用

lab5 新增的代码主要是文件系统相关的,代码在fs文件夹目录下,可以通过`git diff origin/lab4  origin/lab5 --stat`来查看什么有什么更新的,除了fs目录下还有`lib/file.c,lib/fd.c,lib/spawn.c,inc/fs.h,inc/fd.h`

如文档所说 在注释掉`kern/init.c`的`ENV_CREATE(fs_fs)`和`kern_init.c`的`close_all`以后,通过了pingpong primes 和 forktree,其中 primes会爆掉因为它不停申请环境 当环境用完的时候就爆了,

# File system preliminaries

jos只需要实现一个可以增删读写的分级文件系统即可,我们的系统设计提供了bug 追踪保护,但因为它到目前为止实现的都是单用户系统,文件系统中不同用户之间的保护,不需要权限,没有硬连接,符号链接,时间戳类似于其它Unix的设备文件在jos lab中都不需要实现

# On-Disk File System Structure

大多数Unix系统把磁盘分为两部分,用于存放inode和用于存放数据,inode记录了文件的信息以及文件的入口,每个inode对应一个具体的数据,存放信息的磁盘部分被分成了很多小块,多为8KB大小,它们用来存文件数据和目录数据,目录包含文件名和指向inode的指针,hard-linked是指 多个文件指向同一个inode(jos 并不需要支持),our file system will not use inodes at all and instead will simply store all of a file's (or sub-directory's) meta-data within the (one and only) directory entry describing that file. 呵呵

目录 和数据 都是无区别写在磁盘上的,并且用户可以直接读取 目录结构,这样可以让用户级别来实现ls,然而现代的Unix并不鼓励这样做,因为这样让应用程序依赖于文件目录结构会导致耦合性太强 难以更新内核

# Sectors and Blocks

记录文件系统的信息通常保存在硬盘上一个 易于找到的位置,例如在磁盘头或者磁盘尾,其保存的数据包括例如block size,disk-size,meta-data,root directory,这一块数据被称作superblocks

jos文件系统会有一个 超块,放在 block 1(block 0 是用于 boot的) ,它的数据结构如`inc/fs.h`中`struct Super`所示

有的文件系统设计有多个超块,因为超块相当于索引,如果一个超块坏了 那么其它超块还能提供服务

```c
struct Super {
  uint32_t s_magic;   // Magic number: FS_MAGIC
  uint32_t s_nblocks;   // Total number of blocks on disk
  struct File s_root;   // Root directory node
};
```

和页面管理类似,一个物理页在一个时间点只能被给予做一间特定的事情,一个磁盘小块在一个时间点也只能被给予做一个特定的事情,pmap.c利用结构体来记录的每个物理页的使用情况,在记录使用情况更常用的是bitmap而不是linked list,因为bitmap的存储效率更高[虽然在返回了磁盘位置后访问才是主要的耗时部分]

jos用连续的区域bitmap来记录使用情况,例如用4096字节的blocks,每一个bitmap的block就有4098×8=32768的信息量,也就是说每32768个block大小的磁盘就需要一个block来记录,1表示空闲,0表示使用...在jos中bitmap从block 2 开始,并且正在bitmap中标记0,1,bitmap所占的块始终为已使用

# File Meta-data

数据结构如`inc/fs.h`的`struct File`(256 bytes)所示

```c
struct File {
  char f_name[MAXNAMELEN];  // filename
  off_t f_size;     // file size in bytes
  uint32_t f_type;    // file type

  // Block pointers.
  // A block is allocated iff its value is != 0.
  uint32_t f_direct[NDIRECT]; // direct blocks
  uint32_t f_indirect;    // indirect block

  // Pad out to 256 bytes; must do arithmetic in case we're compiling
  // fsformat on a 64-bit machine.
  uint8_t f_pad[256 - MAXNAMELEN - 8 - 4*NDIRECT - 4];
} __attribute__((packed));  // required only on some 64-bit machines
```

因为没有inode所以这些都是存在目录之中

`f_direct`指向可以存储 `NDIRECT*BLOCKSIZE = 10 * 4096BYTES=40KB`

`f_indirect`指向一个块 可以间接指向 `4096/4=1024`个块,所以一共可以指向`10+1024=1034`块,刚刚超过4GB,在别的系统中要支持更大的块 它们会使用 二级 三级j间接块

type 用来区分目录和常规文件,超块指向的应当为根目录

# The File System

你不需要实现完整的文件系统,只需要实现关键部分,因此你需要熟悉已有代码,需要实现的有

 * reading blocks into the block cache and flushing them back to disk
 * allocating disk blocks
 * mapping file offsets to disk blocks
 * implementing read, write, and open in the IPC interface.

## Disk Access

首先要让内核能够访问磁盘,PIO让我们易于实现,因为硬盘中断很难处理.

x86处理器使用EFLAGS中的IOPL位来表示是否允许`protected-mode`下的代码来执行设备I/O操作,例如IN和OUT指令,给文件系统环境读写权限是我们唯一要做的而不需要让它访问这些寄存器,但不要把该权限给其它环境!

# Exercise 1.

`i386_init` 通过传递`ENV_TYPE_FS`标识了 文件系统环境.修改`env.c`中的`env_create`, 让它可以给文件系统环境 I/O 权限 ,但不要给其它任何环境I/O权限.

实现如下
```c
// If this is the file server (type == ENV_TYPE_FS) give it I/O privileges.
if(type == ENV_TYPE_FS)
  e->env_tf.tf_eflags |= FL_IOPL_MASK;
```

Make sure you can start the file environment without causing a General Protection fault. You should pass the "fs i/o" test in make grade.

测试成功,如果出错请检查你对eflags其它地方的操作 比如是否真的是做到保存和恢复 而不是保存和覆盖

# Question

Do you have to do anything else to ensure that this I/O privilege setting is saved and restored properly when you subsequently switch from one environment to another? Why?

不需要 因为都在非内核态 只需要通过寄存器pop 和 push修改 对应 页表 I/O权限也就 不会出错.

---

`fs/ide.c`实现一个最简洁的PIO-based 磁盘驱动,`fs/serv.c`包含了umain函数 用于文件系统环境.

GNUmakefile让QEMU把kernel.img作为disk 0使用,fs.img作为disk 1使用

```
/* TODO
Challenge! Implement interrupt-driven IDE disk access, with or without DMA. You can decide whether to move the device driver into the kernel, keep it in user space along with the file system, or even (if you really want to get into the micro-kernel spirit) move it into a separate environment of its own.
*/
```

# The Block Cache

jos要实现一个 buffer cache (一个block的cache)在`fs/bc.c`中,jos需要控制的文件系统大小<=3GB,利用虚拟地址技术把3G的虚拟地址做页表map `0x10000000 (DISKMAP) up to 0xD0000000 (DISKMAP+DISKMAX)`,例如块0被映射到0x10000000,块1被映射到0x10001000,`fs/bc.c`中的diskaddr实现了从磁盘号到虚拟地址的转换

因为我们的 文件系统环境 有它自己的虚拟地址空间,因此它需要做的只有实现文件访问,目前以上的方法都是因为我们进行了大小限制,如果是在真实的有更大硬盘的设备或者有64位地址中 这样就很僵硬了.映射是做了 但并不是需要把整个磁盘都读入内存,需要实现的是当有相应位置的访问 产生一个page fault 然后再去真实读取,读取以后这页就可以访问

# Exercise 2.

实现`fs/bc.c`中的`bc_pgfault`和`flush_block`. `bc_pgfault` 是一个页错误处理函数,和我们之前实现的`copy-on-write`很像,这里需要进行的工作是把该页对应的磁盘内容读入. 要注意

 1. addr may not be aligned to a block boundary
 2. `ide_read` operates in sectors, not blocks.

如果需要`flush_block`应当写磁盘. `flush_block`不应当对非dirty或不在block cache (也就是没有映射的页)中的块进行写或者它非dirty. 我们需要用虚拟硬件来记录磁盘在最后一次读入/写入后 是否再被修改. 检查一个块是否需要写入只需要检查vpt入口的`PTE_D` "dirty" 位. (The `PTE_D` bit is set by the processor in response to a write to that page; see 5.2.4.3 in chapter 5 of the 386 reference manual.) 在写入磁盘以后`flush_block`应当使用`sys_page_map`清除`PTE_D`位.

使用`make grade` 应该通过`check_bc`, `check_super`, and `check_bitmap`.

先`bc_pgfault`,看到注释提示`ide_read`和`BLKSECTS`,grep它们得到

```
fs/bc.c:set_pgfault_handler(bc_pgfault);
fs/ide.c:ide_read(uint32_t secno, void *dst, size_t nsecs)
fs/fs.h:int ide_read ide_read(uint32_t secno, void *dst, size_t nsecs);
fs/fs.h:#define BLKSECTS (BLKSIZE / SECTSIZE)// sectors per block
```

实现如下

```c
int r;
if((r = sys_page_alloc((envid_t)0, ROUNDDOWN(addr, PGSIZE), PTE_P | PTE_U | PTE_W)) < 0 )
  panic("sys_page_alloc: %e",r);
ide_read(blockno * BLKSECTS, ROUNDDOWN(addr, PGSIZE), BLKSECTS);
```

在这里查了一下 pte中的`PTE_AVAIL`项指向的bits是 给用户随意设置的 也就是说它实际是用来mask 或 unmask用的,所以不应该设置为`PTE_SYSCALL`,在如上的权限设置中页就把dirty清除掉了,顺便看了一下以前实现`sys_page_alloc`的代码 看来是在注释指引下对perm进行了检查

代码中的注释有个问题`Check that the block we read was allocated. (exercise for the reader: why do we do this *after* reading the block in?`

是因为bitmap也是需要经过这样读取的? 也就是说如果在bitmap都没有被allocated的时候这样检测就会出错

---

然后`flush_block`,注释提示

 * Use `va_is_mapped, va_is_dirty, and ide_write`
 * 清除`PTE_D`用 `PTE_SYSCALL` constant when calling `sys_page_map`.
 * Don't forget to round addr down.

这里`sys_page_map` 自己到自己 修改权限 会调用`page_insert`,`page_insert`又因为以前的设计 不会对计数产生错误并且不会放弃又申请,感觉就像先知一样,所以又是一股别人已经设计好的系统只有往后写才知道为什么前面要这样设计,也说 不断变更需求 要设计好有多难...简直

实现如下

```
addr = ROUNDDOWN(addr,PGSIZE);
if(va_is_mapped(addr) && va_is_dirty(addr)){
  int r;
  if((r = ide_write(blockno * BLKSECTS, addr, BLKSECTS)) < 0)
    panic("ide_write: %e",r);
  if((r = sys_page_map(0, addr, 0, addr, PTE_SYSCALL)) < 0)
    panic("sys_page_map: %e",r);
}
```

`make grade`可以看到

```
fs i/o [fs]: OK (1.5s)
check_bc [fs]: OK (1.5s)
check_super [fs]: OK (1.6s)
check_bitmap [fs]: OK (1.6s)
```

`fs/fs.c`中的`fs_init`做了一个如何使用block cache的例子,在初始化以后它直接把指针指向了super和bitmap,然后我们借助于上面的`pgfault_handler`即可正常的如同读取内存一样读取它们的数据

```
/* TODO
Challenge! The block cache has no eviction policy. Once a block gets faulted in to it, it never gets removed and will remain in memory forevermore. Add eviction to the buffer cache. Using the PTE_A "accessed" bits in the page tables, which the hardware sets on any access to a page, you can track approximate usage of disk blocks without the need to modify every place in the code that accesses the disk map region. Be careful with dirty blocks.
*/
```

# The Block Bitmap

在`fs_init`设置了以后我们可以用bitmap来记录哪些block是空闲的,例如函数`block_is_free`

# Exercise 3

仿照`free_block`实现`alloc_block`,`free_block`会从bitmap上找到一个空闲的块 并且标记它为使用,并且返回块号. 当你申请了一个块,你应当立即用`flush_block`把bitmapflush进disk中,来保证系统一致性.

`make grade`应当通过`alloc_block`.

实现如下

```
int
alloc_block(void)
{
  // The bitmap consists of one or more blocks.  A single bitmap block
  // contains the in-use bits for BLKBITSIZE blocks.  There are
  // super->s_nblocks blocks in the disk altogether.
  uint32_t blockno;
  for (blockno = 0; blockno < super->s_nblocks; blockno++) {
    if(block_is_free(blockno)) {
      bitmap[blockno/32] &= ~(1<<(blockno%32));
      flush_block(&bitmap[blockno/32]);
      return blockno;
    }
  }
  return -E_NO_DISK;
}
```

# File Operations

这里说`fs/fs.c`实现了很多辅助函数,阅读整个文件先理解一共有哪些已提供的实现

# Exercise 4.

实现`file_block_walk`,`file_get_block`.`file_block_walk` 映射一个块的文件偏移量 指向对应的`struct File`或`indirect block`, 和`pgdir_walk`在页表的行为类似. `file_get_block` 会映射实际的硬盘块,如果需要allocating一个新的.

`make grade`,你应当通过`file_open`,`file_get_block` ,`file_flush/file_truncated/file rewrite`.

`fs/fs.c`有以下函数 [说句题外话 以前vim一直用得不够爽,在熟悉了大量快捷键以后,我配置一些常用对我有帮助的插件,之后也尝试了几天emacs,也试了一试spacemacs,现在开始用spf13-vim的配置,它的很多自动补全 git 检测,状态信息,查看tagbar等等都能很大提高coding效率]

```
    alloc_block(void)
    block_is_free(uint32_t blockno)
    check_bitmap(void)
    check_super(void)
   -dir_alloc_file(struct File *dir, struct File **file)
   -dir_lookup(struct File *dir, const char *name, struct File **file)
   -file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
    file_create(const char *path, struct File **pf)
    file_flush(struct File *f)
   -file_free_block(struct File *f, uint32_t filebno)
    file_get_block(struct File *f, uint32_t filebno, char **blk)
    file_open(const char *path, struct File **pf)
    file_read(struct File *f, void *buf, size_t count, off_t offset)
    file_set_size(struct File *f, off_t newsize)
   -file_truncate_blocks(struct File *f, off_t newsize)
    file_write(struct File *f, const void *buf, size_t count, off_t offset)
    free_block(uint32_t blockno)
    fs_init(void)
    fs_sync(void)
   -skip_slash(const char *p)
   -walk_path(const char *path, struct File **pdir, struct File **pf, char *lastelem)
```

```c
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
  if(filebno >= NDIRECT + NINDIRECT)
    return -E_INVAL;
  else if(filebno < NDIRECT){
    *ppdiskbno = &(f->f_direct[filebno]);
  }else{
    if(!f->f_indirect){
      if(!alloc)
        return -E_NOT_FOUND;
      int r;
      if((r = alloc_block()) < 0)
        return r;
      memset(diskaddr(r), 0, BLKSIZE);
      f->f_indirect = r;
      flush_block(diskaddr(r));
    }
    *ppdiskbno = &(((uint32_t *)(diskaddr(f->f_indirect)))[filebno-NDIRECT]);
  }
  return 0;
}
```

其中`f_indirect`应该存地址还是块号 我参照grep到的`fs/fsformat.c`中的`finishfile`函数 是储存的块号

---

实现如下

```c
int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{
  uint32_t * pdiskno;
  int r;
  if((r = file_block_walk(f,filebno,&pdiskno,1)) < 0)
    return r;
  if(!*pdiskno){
    if((r = alloc_block()) < 0)
      return r;
    *pdiskno = r;
  }
  *blk = diskaddr(*pdiskno);
  return 0;
}
```

`make grade`可以看到

```
file_open [fs]: OK (1.6s)
file_get_block [fs]: OK (1.6s)
file_flush/file_truncate/file rewrite [fs]: OK (1.6s)
```

`file_block_walk`,`file_get_block`是文件系统的主力`For example, file_read and file_write are little more than the bookkeeping atop file_get_block necessary to copy bytes between scattered blocks and a sequential buffer.`

```c
/* TODO
Challenge! The file system is likely to be corrupted if it gets interrupted in the middle of an operation (for example, by a crash or a reboot). Implement soft updates or journalling to make the file system crash-resilient and demonstrate some situation where the old file system would get corrupted, but yours doesn't.
*/
```

# Client/Server File System Access

已经实现了 重要的文件系统环境中的功能,接下来需要让它们可以被其它环境使用,因为其它环境不能直接调用这个文件系统环境的函数,需要通过RPC(remote procedure call) 基于jos的IPC机制.

对文件系统的say,read的设计如文档的图所示 .

```
      Regular env           FS env
   +---------------+   +---------------+
   |      read     |   |   file_read   |
   |   (lib/fd.c)  |   |   (fs/fs.c)   |
...|.......|.......|...|.......^.......|...............
   |       v       |   |       |       | RPC mechanism
   |  devfile_read |   |  serve_read   |
   |  (lib/file.c) |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |     fsipc     |   |     serve     |
   |  (lib/file.c) |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |   ipc_send    |   |   ipc_recv    |
   |       |       |   |       ^       |
   +-------|-------+   +-------|-------+
           |                   |
           +-------------------+
```

在点行 以下的是 简单的从regular环境获得到文件系统环境的读请求. 最开始, read (which we provide) 运行在任何文件描述符并且简易的分发到适当的设备读函数, 比如这里`devfile_read` (在未来lab中有更多的设备类型 比如 管道 网络 sockets). `devfile_read`只负责磁盘上的文件读取. 这些和其它`lib/file.c`中的`devfile_*`函数实现了用户端的 文件操作and all work in roughly the same way, bundling up arguments in a request structure, calling fsipc to send the IPC request, and unpacking and returning the results.  fsipc 函数简单的处理了同样的与server之间的发送接受请求.

文件系统server的代码可以在`fs/serv.c`中查看. 它循环`serve`函数,无限接受通过IPC发来的请求, 分发请求到对应的处理函数, 并把结果通过IPC返回. 在读取的代码例子中, serve 会分发到`serve_read`, 它会处理IPC 细节来读取读取请求 并解压请求结构 最终 调用`file_read` 进行实际的文件读取.

回忆 JOS IPC 机制 可以让一个环境发送一个32-bit number 并可以选择分享一个页. 从client发送请求到server, 使用`32-bit` (the file system server RPCs are numbered, just like how syscalls were numbered) 并利用IPC建立的页分享储存请求参数在 `union Fsipc`中. 在client端始终的接受页映射到fsipcbuf;在server端 分享页从fsreq (0x0ffff000) 开始增加.

server 也需要通过IPC发送response. 使用`32-bit`作为函数的返回值, 对于大多数RPC 也就只需要返回值.`FSREQ_READ`和`FSREQ_STAT`需要返回数据,server只需要在它们扔过来的页面上进行写. 并不需要再映射/发送一个页. 同样 作为返回`FSREQ_OPEN` 分享client a new "Fd page". 综上我们的返回额外开销会很小.

# Exercise 5

实现`fs/serv.c`的`serve_read`和`lib/file.c`的`devfile_read`.

`serve_read`的繁重的工作已经被 已经实现的`fs/fs.c:file_read`实现了(which, in turn, is just a bunch of calls to `file_get_block`). `serve_read`只需要给RPC接口提供文件读取. 阅读`serve_set_size`中的注释及代码 来裂解server函数怎么被架构.

类似`devfile_read`需要打包它的参数放入`fsipcbuf`来进行`serve_read`, call fsipc, and handle the result.

使用`make grade` 应当通过`lib/file.c`和`file_read`.

在`inc/fs.h`可以看到该union 一个典型的union写法!!!,这里只用注意

```
struct Fsreq_read {
  int req_fileid;
  size_t req_n;
} read;
struct Fsret_read {
  char ret_buf[PGSIZE];
} readRet;
```

Fd的定义在`inc/fd.h`

```c
struct FdFile {
  int id;
};

struct Fd {
  int fd_dev_id;
  off_t fd_offset;
  int fd_omode;
  union {
    // File server files
    struct FdFile fd_file;
  };
};
```

`file_read(struct File *f, void *buf, size_t count, off_t offset)`

`fs/serv.c`里的

```
   -FILEVA
   -MAXOPEN
   -debug

▼ typedefs
   -fshandler

▼-OpenFile : struct
    [members]
   +o_fd
   +o_file
   +o_fileid
   +o_mode

▼ variables
    fsreq
    handlers
    opentab

▼ functions
    openfile_alloc(struct OpenFile **o)
    openfile_lookup(envid_t envid, uint32_t fileid, struct OpenFile **po)
    serve(void)
    serve_flush(envid_t envid, struct Fsreq_flush *req)
    serve_init(void)
    serve_open(envid_t envid, struct Fsreq_open *req, void **pg_store, int *perm_store)
    serve_read(envid_t envid, union Fsipc *ipc)
    serve_set_size(envid_t envid, struct Fsreq_set_size *req)
    serve_stat(envid_t envid, union Fsipc *ipc)
    serve_sync(envid_t envid, union Fsipc *req)
    serve_write(envid_t envid, struct Fsreq_write *req)
    umain(int argc, char **argv)
```

`fs/serv.c:serve_read`实现如下 按照注释实现即可...感觉mit的助教变懒了 注释还是有但没那么细致了,函数都要自己去找 去看:-),怀念以前没把vim配置好的自己

```c
struct OpenFile *o;
int r;
if((r = openfile_lookup(envid, req->req_fileid, &o)) < 0)
  return r;
if((r = file_read(o->o_file, ret->ret_buf, MIN(req->req_n, sizeof(ret->ret_buf)), o->o_fd->fd_offset)) < 0)
  return r;
o->o_fd->fd_offset += r;
return r;
```

---

`lib/file.c`

```
▼ macros
   -debug

▼ prototypes
   -devfile_flush(struct Fd *fd)
   -devfile_read(struct Fd *fd, void *buf, size_t n)
   -devfile_stat(struct Fd *fd, struct Stat *stat)
   -devfile_trunc(struct Fd *fd, off_t newsize)
   -devfile_write(struct Fd *fd, const void *buf, size_t n)

▼ variables
    devfile
    fsipcbuf

▼ functions
   -devfile_flush(struct Fd *fd)
   -devfile_read(struct Fd *fd, void *buf, size_t n)
   -devfile_stat(struct Fd *fd, struct Stat *st)
   -devfile_trunc(struct Fd *fd, off_t newsize)
   -devfile_write(struct Fd *fd, const void *buf, size_t n)
   -fsipc(unsigned type, void *dstva)
    open(const char *path, int mode)
    sync(void)
```

实现如下

```c
static ssize_t
devfile_read(struct Fd *fd, void *buf, size_t n)
{
  // Make an FSREQ_READ request to the file system server after
  // filling fsipcbuf.read with the request arguments.  The
  // bytes read will be written back to fsipcbuf by the file
  // system server.
  int r;
  fsipcbuf.read.req_fileid = fd->fd_file.id;
  fsipcbuf.read.req_n = n;
  if ((r = fsipc(FSREQ_READ, NULL)) < 0)
    return r;
  memmove(buf, fsipcbuf.readRet.ret_buf, r);
  return r;
}
```

`make grade`通过

```
lib/file.c [testfile]: OK (1.7s)
file_read [testfile]: OK (1.7s)
```

mit 的作者给的代码中已经实现了

```
static ssize_t
devfile_read(struct Fd *fd, void *buf, size_t n)
{
  // Make an FSREQ_READ request to the file system server after
  // filling fsipcbuf.read with the request arguments.  The
  // bytes read will be written back to fsipcbuf by the file
  // system server.
  int r;

  fsipcbuf.read.req_fileid = fd->fd_file.id;
  fsipcbuf.read.req_n = n;
  if ((r = fsipc(FSREQ_READ, NULL)) < 0)
    return r;
  assert(r <= n);
  assert(r <= PGSIZE);
  memmove(buf, fsipcbuf.readRet.ret_buf, r);
  return r;
}
```

并不很懂这样assert 是看做底层代码可能出错的意思么=.= 可是 难道不该是以kern和用户为分割线 可以对下层保持绝对信任么`_(:з」∠)_`

做MIT的这个部分 程序爆了 通过debug 发现是 对权限检查擅自 做了额外的检查 以为发生错误了 现在理解了`PTE_SYSCALL`的作用了`_(:з」∠)_`

然后我看了看我sjtu的lab中的代码 啊哈 竟然 没报错 还是做了改动

`if((*pte & perm) == perm)`这个检查 还是换成注释中所说的`if((perm & PTE_W) && !(*pte & PTE_W))`

这样 mit的这部分 也通过了`serve_open/file_stat/file_close: OK`

---

# Exercise 6.

`Implement serve_write in fs/serv.c and devfile_write in lib/file.c.`

`Use make grade to test your code. Your code should pass "file_write" and "file_read after file_write".`

也就是写和上面的读对应

这次注释没有说是否要和PGSIZE比大小,这次使用联合体的

```
struct Fsreq_write {
  int req_fileid;
  size_t req_n;
  char req_buf[PGSIZE - (sizeof(int) + sizeof(size_t))];
} write;
```

实现如下

```
int
serve_write(envid_t envid, struct Fsreq_write *req)
{
  if (debug)
    cprintf("serve_write %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

  struct OpenFile *o;
  int r;
  if((r = openfile_lookup(envid, req->req_fileid, &o)) < 0)
    return r;
  if((r = file_write(o->o_file, req->req_buf, req->req_n, o->o_fd->fd_offset)) < 0)
    return r;
  o->o_fd->fd_offset += r;
  return r;
}
```

---

然后是`lib/file.c:devfile_write`

棒！这里注释说 要对大小进行限制 也就上面不加MIN也行

实现如下

```
fsipcbuf.write.req_fileid = fd->fd_file.id;
fsipcbuf.write.req_n = MIN(n, sizeof(fsipcbuf.write.req_buf));
memmove(fsipcbuf.write.req_buf, buf, fsipcbuf.write.req_n);
return fsipc(FSREQ_WRITE, NULL);
```

`make grade`以后通过了

```
file_write [testfile]: OK (1.6s)
file_read after file_write [testfile]: OK (1.6s)
```

哎进度条都快到 document 结束了 结果分数才60/105 :-(,总想骗分跑路,前面的lab也是 总是实现了一大页 然后拿了5分:-)

# Client-Side File Operations

`lib/file.c`是只针对于磁盘上的文件的,但是UNIX文件描述符是 万物都是文件的思想 也包括 pipes, console I/O, etc. 在jos中 这些设备类型有一个(or will have in later labs) 对应的结构体Dev, 并有指针指向实现了read/write/etc.的函数,对于哪些设备类型. `lib/fd.c`实现了general UNIX-like file descriptor的顶层接口. 每一个Fd结构 表明它的 device type, 大多`lib/fd.c`中的函数 简易的分发操作到 struct Dev中的目标函数

`lib/fd.c` 还有file descriptor table region 在它们每个程序环境的地址空间中,从FSTABLE开始. 这个区域保存一页(4KB) 地址空间 对于<=MAXFD (currently 32) file descriptors the application can have open at once. 在任何时间,一旦切只当对应的文件描述符在使用的时候一个特定的particular file descriptor table page 会被映射. 每一个 file descriptor 也可选择的有"data page" in the region starting at FILEDATA, 在下一个lab之前我们不会使用它.

对于几乎所有 文件的交互操作,用户的代码只需要使用`lib/fd.c`中的函数. 在`lib/file.c`中一个"public" function is open, 它会在通过打开一个命名的在磁盘上的文件 构建一个新的文件描述符.

# Exercise 7.

实现open. `open`函数 需要用`fd_alloc()`函数(已提供)找到一个未使用的文件描述符, 发出一个IPC请求到文件系统环境中 来打开文件.. 确保你的代码能优雅的退出,如果 超过了文件打开的上限, 或者任何IPC到文件系统环境请求失败了.

`make grade`应当通过`open`,`large file`, `motd display`, and `motd change`.

哇 刚刚上面那一段吓死我了 我还以为 要写pipe console I/O,,,,,,技不如人甘拜下风.

看`fd_alloc`代码 这个alloc是个假的 注释举例说如果你第一次调用以后并没有真正的申请,那么第二次再调用则会返回和第一次一样的值

open实现如下

```
struct Fd *fd;
int r;
if(strlen(path) >= MAXPATHLEN)
  return -E_BAD_PATH;
if((r = fd_alloc(&fd)) < 0)
  return r;
strcpy(fsipcbuf.open.req_path, path);
fsipcbuf.open.req_omode = mode;
if((r = fsipc(FSREQ_OPEN, fd)) < 0){
  fd_close(fd, 0);
  return r;
}
return fd2num(fd);
```

`make grade` 通过了

```
open [testfile]: OK (2.1s)
large file [testfile]: OK (2.1s)
motd display [writemotd]: OK (1.6s)
motd change [writemotd]: OK (1.6s)
```

mit 的open由作者提供了 当真 注释不完整的都不是mit的咯？

---

```
/* TODO
Challenge! Change the file system to keep most file meta-data in Unix-style inodes rather than in directory entries, and add support for hard links.
*/
```

# Spawning Processes

作者已经提供了 产生新环境的代码, 从文件系统装载一个程序镜像到其中, 然后启动一个子环境来运行的这个程序. 父进程仅需独立于子环境运行. spawn和Unix的fork很像followed by an immediate exec in the child process.

我们实现spawn而不是UNIX-style exec因为spawn更容易在`exokernel fasion`的用户空间实现,不需要内核的特殊帮助. 思考你需要怎样在用户空间实现exec,从而理解为什么在exec实现更难.

# Exercise 8

spawn 依赖于新的`syscall sys_env_set_trapframe` 来初始化 新建立的环境,实现`sys_env_set_trapframe`,用`user_icode`来测试,

实现如下

```
struct Env *e;
int r;
if ((r = envid2env(envid, &e, 1)) < 0)
  return r;
e->env_tf = *tf;
e->env_tf.tf_cs |= 3;
e->env_tf.tf_eflags |= FL_IF;
return 0;
```

还要在syscall.c的syscall函数加上分发

```
case SYS_env_set_trapframe:
  return sys_env_set_trapframe((envid_t)a1, (struct Trapframe*)a2);
```

使用`make grade`测试 105/105.(SJTU)

---

提交到git本地仓库

```
> git status
> git add fs kern lib
> git stash -k
> git commit -m "finish lab5"
> git stash pop
> git status
```

---

```
/* TODO
Challenge! Implement Unix-style exec.

Challenge! Implement mmap-style memory-mapped files and modify spawn to map pages directly from the ELF image when possible.
*/
```

---

以下是MIT的剩余的Exercise

# Sharing library state across fork and spawn

看到开头两段我就想说 技不如人甘拜下风 ,这两段就是上面讲文件描述符的两段...........上面讲了怎么分享页 相关的然后

jos希望能够分享file descriptor state 跨越fork和spawn,但现在 文件描述符 被保存在用户空间的内存中,也就是在fork的时候 内存会被设计为COW(copy-on-write),因此状态是会被复制而不是分享(这意味者环境无法seek 不是它们自己打开的文件,以及fork出的pipe也无法工作.) 在spawn中内存会被保留不会完全复制. (Effectively, the spawned environment starts with no open file descriptors.)

我们要修改fork 让它知道当前的 区域的内存在被"library operating system"使用 需要始终是被分享. 也不应当在一个 硬编码的某区域(感觉在逗我 不是专门分了一个区域么), jos将会设置一个未使用的位在PTE中(just like we did with the PTE_COW bit in fork).

已经在`inc/lib.h`中定义了`PTE_SHARE`位. 这个PTE上的位属于"available for software use"的范围中 也就是`PTE_AVAIL`范围中 in the Intel and AMD manuals. 将要建立一个机制,如果 PTE的这一个位被设置, PTE在fork and spawn都应该直接从父到子都是 拷贝.注意 这和 标记 copy-on-write不同: 因为要确保 共享页的数据更新也要同步.

# Exercise 8(MIT)

按照以下修改`lib/fork.c`的duppage ,如果PTE的 `PTE_SHARE` 位已经被设置,则只需要直接拷贝映射,你应当 使用`PTE_SYSCALL` 而不是`0xfff`(就是说别硬编码)0xfff picks up the accessed and dirty bits as well.)

类似实现`lib/spawn.c`中的`copy_shared_pages` 它应当遍历当前进程的所有PTE(和fork做的一样) 把任何有`PTE_SHARE`位设置的页映射复制到子进程中

`grep -r "PTE_SHARE" * | grep -v obj` 看到 该位的设置 是用户行为,以及一个`fs/serv.c`中的设置 (还是用户行为),我们要做的就是对该位已经设置的页特殊处理

duppage 也就是上个lab实现 COW的地方,按照上面说的方法 实现如下

```c
static int
duppage(envid_t envid, unsigned pn)
{
  void * addr = (void *)(pn * PGSIZE);
  int r;
  if (uvpt[pn] & PTE_SHARE) {
    if((r = sys_page_map((envid_t)0, addr, envid, addr, uvpt[pn] & PTE_SYSCALL)) < 0)
      panic("sys_page_map: %e\n", r);
  }else if (uvpt[pn] & (PTE_W | PTE_COW)) {
    if((r = sys_page_map((envid_t)0, addr, envid, addr, PTE_U | PTE_P | PTE_COW) < 0))
      panic("sys_page_map: %e\n", r);
    if((r = sys_page_map((envid_t)0, addr, 0    , addr, PTE_U | PTE_P | PTE_COW) < 0))
      panic("sys_page_map: %e\n", r);
  } else {
    if((r = sys_page_map((envid_t)0, addr, envid, addr, PTE_U | PTE_P )) < 0)
      panic("sys_page_map: %e\n", r);
  }
  return 0;
}
```

---

接下来 `copy_shared_pages`,grep一下 只有这个spawn.c里调用过,实现如下

```c
static int
copy_shared_pages(envid_t child)
{
  uintptr_t addr;
  int r;
  // We're the parent.
  // Do the same mapping in child's process as parent
  // Search from UTEXT to USTACKTOP map the PTE_P | PTE_U | PTE_SHARE page
  for (addr = UTEXT; addr < USTACKTOP; addr += PGSIZE)
    if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & (PTE_P | PTE_U | PTE_SHARE)) == (PTE_P | PTE_U | PTE_SHARE)){
      if((r = sys_page_map((envid_t)0, (void *)addr, child, (void *)addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
        panic("sys_page_map: %e\n", r);
    }
  return 0;
}
```

根据文档`make run-testpteshare-nox`和`make run-testfdsharing-nox`都得到了文档所说的输出

---

# The keyboard interface

为了让shell工作,需要一种输入方式,QEMU已经显示了输出到CGA的结构 和一些端口,但到目前为止 我们都只能在内核的monitor上输入,在QEMU中 从键盘输入到显示的图形窗口中,输入到console 显示字符需要一些端口,`kern/console.c`已经包括了键盘和一系列 drivers 已经在lab 1 monitor中使用的,但你现在需要实现剩下的部分.

# Exercise 9

在`kern/trap.c`中调用`kbd_intr`来处理`trap IRQ_OFFSET+IRQ_KBD` 和`serial_intr`函数来处理`trap IRQ_OFFSET+IRQ_SERIAL`.

在`lib/console.c`中实现了console input/output file type for you. `kbd_intr`和`serial_intr` 会填充一个buffer with the recently read input while the console file type drains the buffer (the console file type is used for stdin/stdout by default unless the user redirects them).

使用`make run-testkbd`并输入几行来测试.jos 应该会 返回你输出的.尝试在console和 graphical window中都试试.

看`kern/trap.c`上一个lab已经把这两个函数的ipt映射做了,但没有分发.... 实现如下

```c
// Handle keyboard and serial interrupts.
if (tf->tf_trapno == IRQ_OFFSET + IRQ_KBD) {
  kbd_intr();
  return;
}
if (tf->tf_trapno == IRQ_OFFSET + IRQ_SERIAL) {
  serial_intr();
  return;
}
```

`make run-testkbd-nox`测试成功 .并没有graphical window没测

# The Shell

运行`make run-icode`或`make run-icode-nox` 这会运行jos内核并启动`user/icode`. icode 执行 init, 会设置console 为一个文件描述符0 和文件描述符1(标准输入 输出).然后他会 spawn sh, the shell. 你应当可以运行 下面的命令

```
echo hello world | cat
cat lorem |cat
cat lorem |num
cat lorem |num |num |num |num |num
lsfd
```

注意 user library routine cprintf 直接输出到console, 如果没有文件描述code. 这对debug很好用 但对其它程序的pipe并不友好. 对一个指定的文件描述符输出 (例如 1, 标准输出),使用 fprintf(1, "...", ...). printf("...", ...) 是 printing to FD 1的缩写. `user/lsfd.c`是个例子.

# Exercise 10.

shell 不支持 I/O 重定向. 如果能运行`sh < script` 而不需要所用命令都靠手动输入 会更好,在`user/sh.c`中增加 I/O 重定向 `<`的功能.

在shell中使用`sh <script`测试你的实现

运行`make run-testshell`来测试. testshell simply feeds the above commands (also found in fs/testshell.sh) into the shell and then checks that the output matches fs/testshell.key.

看`user/sh.c` 哇 精彩 虽然需要写`<`的重定向 但是`>`的重定向 作者已经实现了

```
if ((fd = open(t, O_RDONLY)) < 0) {
  cprintf("open %s for read: %e", t, fd);
  exit();
}
if (fd != 0) {
  dup(fd, 0);
  close(fd);
}
break;
```

按照 文档上的`make run-testshell-nox`运行显示了(`shell ran correctly`)然后TRAP到break point了,但`make run-icode-nox`再`sh <script`就一脸比较正常 虽然和`fs/testshell.key`也不完全一样

---

然后`make grade`得到145/150,`Protection I/O space`错了 对应查到是`user/spawnfaultio`错了

测试期望 输出bug ,我通过在`kern/init.c`中修改以后`make qemu-nox`输出的是`(null)made it here --- bug`,该输出来自`faultio.c` 注释说 用户不应当有 输出到disk 1 的权限,然而在`spawnfaultio.c`中同样outb会被保护 也就是说spawnl的过程把权限位设置错了,

然后`grep -r "FL_IOP" * | grep -v obj`在spawn 中找到

`child_tf.tf_eflags |= FL_IOPL_3;   // devious: see user/faultio.c`

注释掉 就好150/150....故意搞我????? 不是很懂 想让我干嘛

---

```
/* TODO
Challenge! Add more features to the shell. Possibilities include (a few require changes to the file system too):

backgrounding commands (ls &)
multiple commands per line (ls; echo hi)
command grouping ((ls; echo hi) | cat > out)
environment variable expansion (echo $hello)
quoting (echo "a | b")
command-line history and/or editing
tab completion
directories, cd, and a PATH for command-lookup.
file creation
ctl-c to kill the running environment
but feel free to do something not on this list.
*/
```

# Questions:

* How long approximately did it take you to do this lab?

接近1周.注释越来越不走心了XD

* We redesigned the file system this year with the goal of making it more comprehensible in a week-long lab. Do you feel like you gained an understanding of how to build a file system? Feel free to suggest things we could improve.

很好 和CSE的文件系统大lab有很明显的区别.作为os的fs重心放在了如何设计用户环境的fs,做好错误隔离,以及如何通过IPC去调用,而CSE的fs重心放在fs的设计与实现上,os的fs实现除了文档的介绍,代码中基本没体会到多少 也就bitmap以及硬盘分块,多级目录 都是作者提供好了的.

建议可以 做两遍OS ,第二遍写出对原来早的lab 的代码设计优化理解,因为越往后面做 才能真实理解到原来代码中注释所要我们返回和判断的值,讲道理 整个的教学设计细节还是相当僵硬.

感觉需要 就算需要更早的在课程中穿插更多的git啊linux的命令使用

---

关于 这个lab的感悟是 一切按照注释所讲的行动 不要多增或少些注释中的东西,对于注释 将的不够细致的 尽量简洁实现,所以真是一个完成别人带有预知未来的设计下的实现:-)

# 参考

 * [page translation](https://pdos.csail.mit.edu/6.828/2016/readings/i386/s05_02.htm)
 * [pte](https://www.google.com.hk/search?q=page+table+entry&newwindow=1&safe=strict&noj=1&tbm=isch&tbo=u&source=univ&sa=X&ved=0ahUKEwiJkPi63f3TAhUJRo8KHVBEA-cQsAQIQw&biw=1366&bih=675)
 * [MyVim](https://github.com/YeXiaoRain/MyVim)
 * [spf13-vim](https://github.com/spf13/spf13-vim)
 * [spf13-cheat-sheet](http://conglang.github.io/2015/04/06/spf13-vim-cheat-sheet/)
