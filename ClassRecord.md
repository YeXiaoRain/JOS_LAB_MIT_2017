OS class
---

# class structure

 * 一个实际/真实的系统 可以在真机上运行
 * 将以前的分散的知识串起来(io/memory)

# OS structure

[!graph](#)

structure 分割线
 * ISA(指令接层 arm ibm y86)(User ISA / System ISA)
 * ABI(对软件提供 system calls和user isa 接口,如wine 等模拟器 ABI模拟器 把把系统指令重新实现)
 * API

# why & what learn

对OS的理解对编码有帮助

在底层 手机和电脑类似
 * 能耗需求不同
 * 体验
 * 安全 


可扩展性
 * 什么是:n个core 能达到接近kn的性能
 * 下降的原因:一致性,正确性,不同核的关联性(os可以选择)

安全性
 * os 有最高权限 如果有错将导致所有的进行奔溃

能耗 
 * 算得更快 理论耗时更少 更节能....
 * 操作系统对CPU/Memory调频 

一致性

错误容忍

分布式系统

# 位置

对硬件虚拟化为上层程序提供接支持

cpu 有相关要求,但os可以自己设计 一些相应结构

操作系统告诉硬件映射方式

老式地址有segment registers 有
 * CS for EIP /code
 * SS for SP/BP /stack
 * DS for data
 * ES another
 * ...

看着清晰 但切换进程需要太多转换,在现在的系统运行时依然要设置,但都设置为全部相当于假的

系统逐级加载


当发生终端时,硬件通过表告诉操作系统发生中断的位置

---

内核 常见分类 宏内核 混合内核 微内核

microkernel 虽然设计很理想但明显速度的劣势，由于 用户态和内核态的交互消耗

分不同的模式 用户 系统 ring3 ring2 ring1 ring0，目的管理程序，进行错误隔离,目前常用ring0 ring3 两层

kernel(ring0) 相对 shell(ring3)

exokernel 让应用程序自己管理 直接能使用底层，但我有回收能力,保护能力

unikernel

---

页表 两级页表会比一级页表在运行时省空间

PAE-4k(2,9,9,12),`CR3 -> PDPT(PDPTE)->PD(PDE) -> PT 中的每一项 叫 PTE -> `

PAE-2M(2,9,21),`CR3 -> PDPT(PDPTE)->PD(PDE) ->  `

P=Page,D=Directory,T=Table

DMA 内存比cache新 应该disable cache,对应PTE的CD项(cache disabled)

对于外设需要读memory 不只是cpu时 需要write through 而非write back(WT位)

kernel page(G global page)

kvm = kernel virtual memory




