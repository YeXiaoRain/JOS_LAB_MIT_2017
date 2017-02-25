# jos-lab
---

[The official website](https://pdos.csail.mit.edu/6.828/2016/labguide.html)

**本仓库的代码为mit的**

**本仓库的过程记录为mit+sjtu的**

## lab tools guide

[计算机汇编语言](https://pdos.csail.mit.edu/6.828/2016/readings/pcasm-book.pdf)

[课程参考](https://pdos.csail.mit.edu/6.828/2016/reference.html)


强烈建议用虚拟机完成lab[sjtu有提供配置好了的vm **方法见SJTUREADME.md**] 下面讲手工配置

下载[ubuntu 32位 桌面版](https://www.ubuntu.com/download/desktop)并用虚拟机安装 **注意一定要是32位**

sjtu的同学可以使用`wget http://ftp.sjtu.edu.cn/ubuntu-cd/16.04.2/ubuntu-16.04.2-desktop-i386.iso`进行下载

**警告** 我有尝试64位的ubuntu16.10,配了我40+小时没有配好[虽然有人说mit已经把lab从32位移植到64位上了],所以如果没有闲情逸致,**请勿尝试64位或其它版本**,我测试可用的是**32位ubuntu16.04.2**

> 我有尝试下载ubuntu server版,然后用ssh连接,之后的`configure`里加上选项`--disable-kvm`,并将`make qemu`全部换为`make qemu-nox`,一旦运行崩了,通过另一个ssh 去kill 进程,再reset之前的ssh连接的窗口,这样也是可以玩的:-), ~~desktop大小可是server的两倍多哦~~

> 当然你如果不够熟练不用鼠标,也没有这般闲情逸致的话,建议还是下载有图形的ubuntu桌面版本,[当然桌面版也可以这样用ssh玩

**依赖安装** (安装前注意换源(`sudo apt edit-sources`),sjtu有ubuntu内源哦 亲测≈10MB/s)
 * `sudo apt-get update && sudo apt-get install git gcc-multilib build-essential python libsdl1.2-dev libtool-bin libglib2.0-dev libz-dev  libpixman-1-dev -y `
 * sjtu 的虚拟机,源要换成`deb http://archive.debian.org/debian squeeze main`

### QEMU
 
获取 三种方法 任选一种 
 * [方法一 mit的qemu2.3.0] `git clone http://web.mit.edu/ccutler/www/qemu.git mitqemu -b 6.828-2.3.0 && cd mitqemu`
 * [方法二 github的qemu2.7] 从github获取qemu : `git clone -b stable-2.7 --single-branch --depth=1 https://github.com/qemu/qemu.git qemu-2.7 && cd qemu-2.7`
 * [方法三 sjtu的qemu1.5.2] 下载并解压 :`wget http://ipads.se.sjtu.edu.cn/courses/os/2015/tools/qemu-1.5.2.tar.bz2 && tar xf qemu-1.5.2.tar.bz2 && mv qemu-1.5.2 sjtuqemu && cd sjtuqemu`

配置
 * mit在`./configure`时有使用`--disable-kvm`选项
 * 绝对路径 建议使用形如`/path_to_your_lab_dir/qemu_dir_name/dist`的,例如我使用的`/home/yexiaorain/Android/Documents/os/qemu-2.7/dist`
 * **请修改命令中prefix的路径指向绝对路径** `./configure --prefix=CHANGE_THE_PATH_TO_ABSOLUTE_DIR --target-list="i386-softmmu" && make && make install`

### lab 代码

下载代码`git clone https://github.com/YeXiaoRain/JOS_LAB_MIT_2017.git`

[分支结构](https://github.com/YeXiaoRain/JOS_LAB_MIT_2017/network)或`git log --graph --decorate --all --oneline`

