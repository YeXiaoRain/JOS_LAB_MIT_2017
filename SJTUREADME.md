SJTU JOS LAB
---

下载 老师已经配置好qemu的虚拟机 `http://ipads.se.sjtu.edu.cn/courses/os/2017/tools/jos-student.rar`

(用户名,密码)=(oslab,111111)

登录后 安装`vmware tools`

`Virtual Machine`->`Install VMware Tools`

```
sudo cp /media/cdrom/VMwareTools-10.0.10-4301679.tar.gz /tmp/
cd /tmp/
tar zxf VMwareTools-10.0.10-4301679.tar.gz
cd vmware-tools-distrib/
sudo ./vmware-install.pl -d
sudo reboot -t now
```

# 换源安装

`> sudo apt edit-sources`

```
deb http://archive.debian.org/debian squeeze main
```

如果`NO_PUBKEY`

`sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 8B48AD6246925553`

添加`expect`以及你想用的软件

`sudo apt-get install expect build-essential vim openssh-server ctags -y`

接下来 就可以完全不看README.md开始做lab了
