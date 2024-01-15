# XV6

官方网站：https://pdos.csail.mit.edu/6.S081/2021/index.html

内容：xv6pdf文件，lab实验信息（hints很重要）



中文翻译文字课程：https://mit-public-courses-cn-translatio.gitbook.io/mit6-s081/lec01-introduction-and-examples/1.1-ke-cheng-jian-jie

内容：按照英文课堂视频讲解顺序还原翻译中文，有很多xv6上面没有说明的知识点，有助于加深理解



b站视频lab讲解：https://www.bilibili.com/video/BV1Qi4y1o7tN/?spm_id_from=333.788&vd_source=945ace4c39b8068077f25e5323ccbf65

内容：对lab实验的讲解，包括环境部署、实验代码细节，实验思路；重要的是对前置知识有一定的讲解

视频相关教程：https://tarplkpqsm.feishu.cn/docs/doccnxrUYjtjuoNnAyxwajplSyf



xv6中文文档：https://th0ar.gitbooks.io/xv6-chinese/content/index.html



实现代码：

- https://github.com/YukunJ/xv6-operating-system/tree/util



经验贴&资源贴：

- 28天速通MIT 6.S081：https://zhuanlan.zhihu.com/p/632281381
- 操作系统实验Lab1&Lab2：https://blog.csdn.net/weixin_48283247/article/details/120602005



建议学习顺序：











# [xv6手册与代码笔记](https://www.zhihu.com/column/c_1345025252318007298)





# Chapter 4: Traps and System Calls



## **Foreword:**

通过前面的几章的学习，我们现在对**用户空间**和**内核空间**都有比较好的理解，对于**CPU虚拟化**、**内存虚拟化**这些问题，我们前面也讨论了实现它们的一些方法。在这一章中，我们把更多的关注放在用户空间和内核空间之间的**切换**，了解这个过程中的一些细节。

CPU通常有三种**特殊事件**会暂停当前指令流的执行，并**跳转到一段特定代码**来处理这些事件。

- **系统调用**，执行RISC-V的**ecall**指令时。
- **异常**，用户或内核指令可能进行了一些非法操作，例如除以0或者使用无效的虚拟地址时。
- **设备中断**，当一个设备因某些原因（如磁盘的读/写工作完成）需要CPU及时处理时。

CPU暂停当前指令流执行，跳转到一段特定代码去处理特殊事件的情况，我们称为**陷阱trap**。

以在用户空间下发生trap为例，通常的流程是，trap会强制地从用户空间切换到内核空间。在内核空间下，内核保存一些寄存器和其它状态，以便稍后恢复执行被暂停的进程指令流。然后内核就开始执行一段特定的处理代码（如系统调用函数、设备驱动程序等）。处理完毕后，内核就恢复之前保存的寄存器和其它进程状态，最后从内核空间返回到用户空间，恢复执行之前被暂停的用户指令流。

trap最好是对用户进程**透明**的，即保存和恢复相关寄存器应该由内核负责而不让用户来操心，从trap返回时也应该回到进程指令流被中止的位置，用户进程应该感受不到trap的发生。

xv6的内核处理所有的trap。系统调用显然应该如此；设备中断也说得通，根据我们建立起来的**隔离**机制，用户进程不应该直接访问硬件设备，而内核拥有处理设备所需的状态信息；对于异常，xv6的响应很简单，对于所有来自用户空间的异常，直接杀死该进程。

继续以用户空间下发生trap为例，trap所经历的完整流程，从代码路径来看就是：**uservec->usertrap->usertrapret->userret**

- **uservec**：位于trampoline的前半部分汇编代码，用于做一些陷入内核之前的准备工作，例如保存用户空间下的一系列寄存器，加载内核栈、内核页表等设置，然后跳转到usertrap。这一部分，我们称为**trap vector**。
- **usertrap**：位于内核中的一段C代码，判断引起trap的事件类型，并决定如何处理该trap，如跳转到系统调用函数、设备驱动程序等。我们一般也称其为**trap handler**。
- **usertrapret**：位于内核中的另一段C代码，trap被处理完之后，就会跳转到usertrapret，保存内核栈、内核页表等内核相关信息，进行一些设置，然后跳转到userret。
- **userret**：位于trampoline的后半部分汇编代码，用于做一些返回用户空间的恢复工作，恢复之前保存的用户空间寄存器，最后返回用户空间，恢复用户进程指令流的执行。

这里值得注意的一个点是，为什么我们没有将uservec和usertrap两个代码路径合并在一起。按照我们的解决方案，用两段代码分别负责**保存**和**处理**的工作，好处在于，现在我们可以区分以下三种不同的情况：来自**用户空间的trap**，来自**内核空间的trap**和**计时器中断**。

## **4.1 RISC-V Trap Machinery**

每个RISC-V CPU都有一些**控制寄存器**的集合，内核通过写入这些寄存器，告诉CPU如何处理这次trap；内核通过读入这些寄存器，能够发现一次trap的发生。

完整的控制寄存器集合可以参考相关RISC-V文档，这里介绍一些最重要的控制寄存器：

[RISC-V privileged instructions](https://link.zhihu.com/?target=https%3A//github.com/riscv/riscv-isa-manual/releases/download/draft-20200727-8088ba4/riscv-privileged.pdf)

[github.com/riscv/riscv-isa-manual/releases/download/draft-20200727-8088ba4/riscv-privileged.pdf](https://link.zhihu.com/?target=https%3A//github.com/riscv/riscv-isa-manual/releases/download/draft-20200727-8088ba4/riscv-privileged.pdf)

- **stvec**：内核将trap handler（在用户空间下的trap，是trampoline的**uservec**；在内核空间下的trap，是kernel/kernelvec.S中的**kernelvec**）写到stvec中。当trap发生时，RISC-V就会跳转到stvec中的地址，准备处理trap。你可能发现，被写入stvec的是uservec或者kernelvec，而不是usertrap，其实我们也可以将两个trap vector认为是trap handler的准备工作部分。
- **sepc**：当trap发生时，RISC-V就**将当前pc的值保存在sepc中**（例如，指令ecall就会做这个工作），因为稍后RISC-V将使用stvec中的值来覆盖pc，从而开始执行trap handler。稍后在userret中，sret指令将sepc的值复制到pc中，内核可以设置sepc来控制sret返回到哪里。
- **scause**：RISC-V在这里存放一个数字，代表引发trap的原因。
- **sscratch**：一个特别的寄存器，通常在用户空间发生trap，并进入到uservec之后，sscratch就装载着指向进程**trapframe**的指针（该进程的trapframe，在进程被创建，并从userret返回的时候，就已经被内核设置好并且放置到sscratch中）。RISC-V还提供了一条**交换指令**（csrrw），可以将任意寄存器与sscratch进行值的交换。sscratch的这些特性，便于在uservec中进行一些寄存器的保存、恢复工作。
- **sstatus**：位于该寄存器中的**SIE**位，控制设备中断是否开启，如果SIE被清0，RISC-V会推迟期间的设备中断，直到SIE被再次置位；**SPP**位指示一个trap是来自用户模式下还是监管者模式下的，因此也决定了sret要返回到哪个模式下。

以上这些控制寄存器不能在用户模式下访问，只有在内核中，在监管者模式下处理trap时，才能访问或设置这些寄存器。值得一提的是，机器模式下也有完全对等的一系列控制寄存器，不同的是这些寄存器以m开头命名，而且只有在机器模式下处理trap时，才能用到它们。在xv6中，只有计时器中断这一特殊情况会用到机器模式下的控制寄存器。

在多核芯片上，每个CPU都有各自的上述控制寄存器集合，因此在任一时刻，可能有多个CPU同时都在处理trap。

当trap确实发生了，**RISC-V CPU硬件**按以下步骤处理所有类型的trap（除了计时器中断）：

1. 如果造成trap的是设备中断，将sstatus中的SIE位清0，然后跳过以下步骤。
2. （如果trap的原因不是设备中断）将sstatus中的SIE位清0，关闭设备中断。
3. 将pc的值复制到sepc中。
4. 将发生trap的当前模式（用户模式或监管者模式）写入sstatus中的SPP位。
5. 设置scause的内容，反映trap的起因。
6. 设置模式为监管者模式。
7. 将stvec的值复制到pc中。
8. 从新的pc值开始执行。

值得注意的是，当trap发生了之后，硬件做的事情实际上很少，CPU没有切换到内核页表，没有切换到内核栈，也没有保存任何的寄存器（除了PC）。因此，内核中的代码必须完成以上这些工作。让CPU完成尽可能少的工作，其中一个原因是为内核代码提供灵活性。例如，一些操作系统在trap发生之后可能并不需要切换页表（在它们的设计里，用户空间和内核空间使用同一个页表，按照例如用户空间在低地址，内核空间在高地址等方式设计），如果我们的内核可以自己选择不切换页表，就省去了相关操作，从而提升了性能。

那么我们不禁会想，trap发生时CPU硬件的工作流程还能再简化一些吗？例如，一些CPU可能不改变pc的值，trap可以直接切换到监管者模式，然而依然运行用户指令。但是，这又打破了我们前几章一直在强调的**隔离**机制，这次是打破了用户和内核之间的隔离。问题就在于，你现在能够用监管者模式来运行用户指令，这肯定是不好的。例如你现在可以更改satp寄存器的值，使用内核的页表，然后你就能访问所有的物理内存。因此有些工作，出于隔离性和安全性考虑，我们不再做简化，CPU确实应该从用户指令流中脱离出来，然后执行一段位于内核中的代码，从而保证我们执行的是内核指令，我们可以通过stvec中的值来完成pc值的切换。

## **4.2 Traps From User Space**

我们先关注在用户空间下发生的trap，它比在内核空间下发生的trap要棘手一些，因为在用户空间下发生trap时，satp仍然指向一个用户页表，因此并没有内核的相关映射信息，同时用户的栈指针sp指向的栈，也可能包含无效值或恶意值。

前面提过，在trap发生时，RISC-V硬件并不会切换页表，因此，用户页表应该包含uservec的映射（uservec是我们整个trap的代码路径的第一步）。stvec中应该保存着指向trap vector的指针，在执行用户指令的时候，stvec中保存着指向uservec的指针。

> **第一步：uservec**

**uservec**修改satp的值，切换到内核页表，从而进入内核空间，以便之后执行内核指令来处理trap。为了使页表切换前后，在用户空间下和内核空间下uservec都能正常运行，uservec应该被映射到**内核页表和用户页表中的相同虚拟地址**。通过往用户虚拟地址空间和内核虚拟地址空间的顶端放置trampoline页（其中包含uservec），我们做到了这一点。

下面我们逐步看**trampoline**（kernel/trampoline.S）中的uservec部分。

首先，RISC-V通过csrrw指令，在开始时交换a0和sscratch中的内容。前面我们提到过，内核在返回用户空间之前，就将该进程的trapframe放置进sscratch中。因此交换后在sscratch中保存了原来用户代码的a0，而a0现在装载着之前由内核设置的该进程的trapframe。

```assembly
# sscratch points to where the process's p->trapframe is
# mapped into user space, at TRAPFRAME.
#
# swap a0 and sscratch
# so that a0 is TRAPFRAME,and sscratch is a0
csrrw a0, sscratch, a0
```

接着，有了进程的**trapframe**（我们可以把它当作是一个**容器**），我们就可以保存用户空间下的寄存器。我们将trapframe也映射到用户虚拟地址空间（就在trampoline往下一页）的原因是，现在satp指向的仍是用户页表，所以在用户虚拟地址空间中我们需要这项映射。即使是在内核空间下，我们也可以通过进程结构p->trapframe（放置的是物理地址）来访问。前面我们已经交换过a0和sscratch，因此现在我们可以通过a0，即trapframe来保存用户寄存器。

```assembly
# sd:左边保存到右边
# save the user registers in TRAPFRAME
sd ra, 40(a0)
sd sp, 48(a0)
sd gp, 56(a0)
sd tp, 64(a0)
sd t0, 72(a0)
sd t1, 80(a0)
sd t2, 88(a0)
sd s0, 96(a0)
sd s1, 104(a0)
sd a1, 120(a0)
sd a2, 128(a0)
sd a3, 136(a0)
sd a4, 144(a0)
sd a5, 152(a0)
sd a6, 160(a0)
sd a7, 168(a0)
sd s2, 176(a0)
sd s3, 184(a0)
sd s4, 192(a0)
sd s5, 200(a0)
sd s6, 208(a0)
sd s7, 216(a0)
sd s8, 224(a0)
sd s9, 232(a0)
sd s10, 240(a0)
sd s11, 248(a0)
sd t3, 256(a0)
sd t4, 264(a0)
sd t5, 272(a0)
sd t6, 280(a0)

# save the user a0 in p->trapframe->a0
# 上面a0已经跟sscratch交换过，所以user a0保存在sscratch
# 这时a0还是p->trapframe
csrr t0, sscratch
# 现在t0是sscratch，即user a0
# 将user a0 保存到 trapframe->a0中
sd t0, 112(a0)
```

到此为止，所有的用户寄存器都保存完毕。除此之外，trapframe里面还保存了一些与内核信息相关的指针，现在将它们恢复出来，包括内核栈指针，CPUid，内核页表，以及usertrap的位置。值得注意的是，在我们加载内核页表到satp之后，a0就无效了，因为trapframe只映射在用户虚拟地址空间内，所以我们之前就把usertrap的位置加载到t0，并在最后跳转到t0。

```assembly
# 加载原来保存在trapframe里面的内核相关寄存器
# restore kernel stack pointer from p->trapframe->kernel_sp
ld sp, 8(a0)

# make tp hold the current hartid, from p->trapframe->kernel_hartid
ld tp, 32(a0)

# load the address of usertrap(), p->trapframe->kernel_trap
# 加载usertrap()到t0
ld t0, 16(a0)

# restore kernel page table from p->trapframe->kernel_satp
ld t1, 0(a0)
csrw satp, t1
sfence.vma zero, zero

# a0 is no longer valid, since the kernel page
# table does not specially map p->tf(p->trapframe).
# 内核中没有trapframe的页，所以a0的逻辑地址不再有效

# 可以注意到，trapframe里面唯独epc还没有被更新
# 即sepc寄存器还没有被保存

# jump to usertrap(), which does not return
# 从t0处开始运行，即usertrap()
jr t0
```

之后就跳转到内核的trap handler——**usertrap**（kernel/trap.c）下，我们看看它的处理流程。usertrap整体的工作就是确定用户空间trap的起因，处理该trap，然后返回。

> **第二步：usertrap**

首先，usertrap改变stvec寄存器的值。之前stvec的值是uservec，代表在用户空间下发生trap时，就跳转到uservec；而现在我们是在内核空间下，所以修改为kernelvec，专门处理内核空间下的trap。

```c
// send interrupts and exceptions to kerneltrap(),
// since we're now in the kernel.
w_stvec((uint64)kernelvec);
```

接着，保存sepc寄存器。当用户空间下trap发生时，硬件就保存当前的用户pc值到sepc中。保存sepc是因为，即使在usertrap下，也可能发生进程上下文切换，可能使sepc被覆写。

```c
struct proc *p = myproc();
// save user program counter.
// 在进入trap处理之前，RISC-V硬件就将pc的值复制到sepc中
// 将sepc的值保存到trapframe->epc中
// there might be a process switch in usertrap that could cause sepc to be overwritten
p->trapframe->epc = r_sepc();
```

接着，根据trap的类型，如果是系统调用，调用syscall处理；如果是设备中断，调用devintr处理；如果是其它的异常，直接杀掉用户进程。值得注意的是，对于系统调用的情况，我们将保存起来的pc值加4，因为在RISC-V硬件因系统调用而保存pc值的时候，保存的是ecall的位置。我们对该pc值加4，因此返回用户空间后，会执行系统调用的下一条指令。

```c
if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    // The system call path adds four to the saved user pc because RISC-V, in the case of a system call, 
    // leaves the program pointer pointing to the ecall instruction
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
    // syscall records its return value in p->trapframe->a0
    // System calls conventionally return negative numbers to indicate errors, 
    // and zero or positive numbers for success. 
  } else if((which_dev = devintr()) != 0){
    // devintr() returns 2 if timer interrupt,
    // 1 if other device,
    // 0 if not recognized.
    // ok
  } else {
    // The value in the scause register indicates the type of the page fault 
    // the stval register contains the address that couldn’t be translated
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }
```

在usertrap的最后，即处理完这些trap分支之后，进行一些额外的检查。如果内核认为该进程应该被杀掉，那么就终止它；如果设备中断的类型是计时器中断，即时间片到期，那么就调用yield让出CPU。而至于其它的情况，则跳转到usertrapret。

```c
// On the way out, usertrap checks if the process has been killed 
// or should yield the CPU (if this trap is a timer interrupt)

if(p->killed)
  exit(-1);

// give up the CPU if this is a timer interrupt.
if(which_dev == 2)
  yield();

usertrapret();
```

**usertrapret**是从内核空间返回到用户空间的第一步。无论是内核处理完trap之后，还是新进程被创建之后，又或是另一进程被调度执行之后，第一步都要经过usertrapret。usertrapret设置一系列的RISC-V控制寄存器，以便在用户空间下发生的下一次trap依然能按照我们常规的代码路径来处理。

> **第三步：usertrapret**

首先，重新设置stvec的值，使其指向uservec。这里特别重要的一点是，在此之前要先**关闭中断**。因为我们设置了使stvec指向uservec，但我们当前仍在内核空间下执行，那么内核空间下发生的trap，却从uservec开始处理，这显然是不正确的。所以在一切都准备就绪之前，在我们真正能处理用户空间的trap之前，亦即在我们正式返回用户空间之前（通过sret），我们应该保持设备中断关闭，不能让trap发生在这个过渡期期间，打扰内核的正常执行。

```c
// we're about to switch the destination of traps from
// kerneltrap() to usertrap(), so turn off interrupts until
// we're back in user space, where usertrap() is correct.
intr_off();

// send syscalls, interrupts, and exceptions to trampoline.S
// 改变寄存器stvec的值，指向TRAMPOLINE中的uservec
// 这个动作在返回到用户空间之前完成
w_stvec(TRAMPOLINE + (uservec - trampoline));
```

接着，还记得我们在uservec中恢复出了一些内核相关的信息吗？现在我们即将要返回用户空间，是时候又重新把它们保存回去了。包括内核页表、内核栈、usertrap的位置、CPUid这些，都保存到用户进程的trapframe中。

```c
// set up trapframe values that uservec will need when
// the process next re-enters the kernel.
p->trapframe->kernel_satp = r_satp();         // kernel page table
p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
p->trapframe->kernel_trap = (uint64)usertrap;
p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()
```

然后，完成sstatus寄存器的设置。我们前面提过，SPP位控制sret返回的模式，清空SPP位所以下次调用sret返回时，返回的是用户模式；SPIE位则控制中断的开关，在我们通过sret成功返回用户空间之后，我们确实希望之后中断能够产生，因此置位SPIE位。

```c
// set up the registers that trampoline.S's sret will use
// to get to user space.
  
// set S Previous Privilege mode to User.
unsigned long x = r_sstatus();
x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
x |= SSTATUS_SPIE; // enable interrupts in user mode
w_sstatus(x);
```

然后，将此前保存在trapframe中的，用户空间下trap发生时的pc值加载到sepc中，在sret返回时，会将sepc的值复制到pc中。

```c
// set S Exception Program Counter to the saved user pc.
// setting sepc to the previously saved user program counter
// 加载此前保存在trapframe->epc的，用户空间下的pc值到sepc中，
// 调用sret会将sepc复制到pc
// 用户程序被创建时，也返回到这里，该用户程序的epc在exec()里面已经被初始化
// 这样用户程序被创建并且返回后，sret会将sepc即epc的内容复制到pc中，然后从pc开始继续执行
w_sepc(p->trapframe->epc);
```

最后，我们准备好用户页表，然后调用userret，完成最后的工作，并返回到用户空间中。值得注意的是，这里通过函数指针调用userret，并且将trapframe的虚拟地址作为参数a0，用户页表作为参数a1传入。

```c
// tell trampoline.S the user page table to switch to.
// 注意，原本的satp内核页表已经在上面存放到了trapframe里面
// 这里的satp不是内核页表，而是用户页表
uint64 satp = MAKE_SATP(p->pagetable);

// jump to trampoline.S at the top of memory, which 
// switches to the user page table, restores user registers,
// and switches to user mode with sret.
uint64 fn = TRAMPOLINE + (userret - trampoline);
// 调用userret，参数a0为TRAPFRAME的虚拟地址，参数a1为用户页表satp
((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
```

现在，我们来看**userret**如何完成最后的工作。

> **第四步：userret**

首先，userret马上就更换成用户页表。这里我们将看到和uservec一样的原理，userret也放置在trampoline中，在用户和内核虚拟地址空间下映射到相同的位置。因此即使我们用的是用户页表，执行这段内核代码也不会出错。

```assembly
# switch to the user page table.
csrw satp, a1
# 刷新TLB
sfence.vma zero, zero
```

接着，取出此前保存在trapframe的用户a0值（如果trap是系统调用造成的，那么内核将使用系统调用的返回值，也就是新的a0替换旧的a0），通过间接的寄存器t0，将新的用户a0值写入到sscratch寄存器中。注意， 这里并不是交换，a0仍然是trapframe。

```assembly
# put the saved user a0 in sscratch, so we
# can swap it with our a0 (TRAPFRAME) in the last step.
ld t0, 112(a0)
csrw sscratch, t0
```

然后，我们恢复所有此前在uservec中保存的寄存器。

```assembly
#ld:右边取出到左边
# restore all but a0 from TRAPFRAME
ld ra, 40(a0)
ld sp, 48(a0)
ld gp, 56(a0)
ld tp, 64(a0)
ld t0, 72(a0)
ld t1, 80(a0)
ld t2, 88(a0)
ld s0, 96(a0)
ld s1, 104(a0)
ld a1, 120(a0)
ld a2, 128(a0)
ld a3, 136(a0)
ld a4, 144(a0)
ld a5, 152(a0)
ld a6, 160(a0)
ld a7, 168(a0)
ld s2, 176(a0)
ld s3, 184(a0)
ld s4, 192(a0)
ld s5, 200(a0)
ld s6, 208(a0)
ld s7, 216(a0)
ld s8, 224(a0)
ld s9, 232(a0)
ld s10, 240(a0)
ld s11, 248(a0)
ld t3, 256(a0)
ld t4, 264(a0)
ld t5, 272(a0)
ld t6, 280(a0)
```

倒数第二步，再次使用RISC-V的csrrw指令，交换a0和sscratch，所以交换之后，a0寄存器里面正式我们要返回的用户a0值，而sscratch中则是trapframe。

```assembly
# restore user a0, and save TRAPFRAME in sscratch
csrrw a0, sscratch, a0
```

最后一步，我们调用**sret**结束整个漫长的trap过程。这里sret完成三个工作：复制sepc中的值到pc中；根据sstatus寄存器的设置，切换到用户模式，并重新开放中断。sret完成的工作，与trap发生时硬件做的工作有对应的部分，以系统调用为例，RISC-V的ecall指令也完成三个工作：将pc的值保存到sepc中，从用户模式切换到监管者模式，跳转到stvec中指向的指令。

```assembly
# return to user mode and user pc.
# usertrapret() set up sstatus and sepc.
# sret做3件事，①复制sepc到pc中，
# 根据sstatus中设置的内容，②切换到用户模式，③重新开放中断
sret
```

此后，被暂停的用户指令流将重新执行，我们讨论的整个trap流程对于进程来说是透明的。

## **4.3 Code: Calling System Calls**

在第二章中，我们介绍了系统调用**exec**如何加载用户进程的虚拟地址空间。现在，我们将从系统调用的执行路径来重新看一遍。

首先，在用户代码执行exec，请求系统调用函数exec时，用户参数按顺序分别被放置到寄存器a0和a1，而系统调用号被放置在**a7**，然后执行ecall指令，一个用户空间下的trap正式发生，正如user/usys.S中所示。

```assembly
.global exec
exec:
 li a7, SYS_exec
 ecall
 ret
```

接着，经过uservec的准备工作后，开始处理trap。在usertrap中，发现trap的起因是系统调用，因此usertrap会执行**syscall**函数。syscall根据传入的系统调用号，索引syscalls数组，找到对应的那个系统调用，通过函数指针执行相应的系统调用。以下代码位于kernel/syscall.c。

```c
static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

然后，内核执行系统调用**sys_exec**（kernel/sysfile.c），完成一些检查和相关用户参数的复制之后，就执行kernel/exec.c中的内容。

```c
uint64
sys_exec(void)
{
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if(argstr(0, path, MAXPATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  // 这里正式执行exec()
  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}
```

最后，系统调用有一个返回值，在syscall中，内核将该返回值放置到trapframe中的a0，稍后在userret中会将该返回值取出，从而作为最终返回的用户a0值。RISC-V的调用管理就是将返回值放置在a0寄存器中，并且非负值代表调用成功，负值代表调用失败。

## **4.4 Code: System Call Arguments**

除了系统调用号，内核还需要找到用户传入的其它参数（a0、a1等）。你应该注意到，在trap发生时，uservec就将这些用户寄存器都保存到trapframe里了，因此内核也应该从那里找出它们。内核使用argint、argaddr和argfd等函数，来抽取出用户参数值，并把它作为整数、指针或者文件描述符。它们都利用**argraw**（kernel/syscall.c）这个函数来抽取相应参数。

```c
static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}
```

一些系统调用会传递指针作为用户参数，内核必须使用这些指针，读或写这些属于用户进程的物理内存。例如，exec就给内核传递了一个argv指针，指向一系列的命令行参数。

使用这些用户指针有两个挑战。一是用户进程可能是有漏洞或者有恶意的，因此它会传递一个无效的指针，甚至是一个企图访问，属于其它用户进程内容，或者属于内核内容的指针。二是内核页表和用户页表的不同所造成的，因为各自的映射不同，在使用内核页表时，不能用简单的指令访问这些用户地址。

幸运的是，内核实现了专门的函数，用于安全地从用户地址中复制内容到内核缓冲区中。

来看**fetchstr**（kernel/syscall.c）这个例子，众多系统调用，如exec，就是用它来复制位于用户空间的参数。fetchstr将主要的工作交给copyinstr来完成。

```c
// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
// The kernel implements functions that safely transfer data to and from user-supplied addresses.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  int err = copyinstr(p->pagetable, buf, addr, max);
  if(err < 0)
    return err;
  return strlen(buf);
}
```

**copyinstr**（kernel/vm.c）的作用是，给定一个用户页表，从用户虚拟地址srcva（例如用户缓冲区），安全地拷贝最多max个字节到内核的dst位置中。首先copyinstr调用walkaddr来为srcva找到对应的物理地址pa0，利用内核采用**直接映射**的特性，我们将pa0作为虚拟地址，便可以直接地从pa0中拷贝字节流到dst中。

```c
// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,(user pagetable)
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    // walkaddr checks that the user-supplied virtual address is partof the process’s user address space,
    // so programs cannot trick the kernel into reading other memory
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    // Since the kernel maps all physical RAM addresses to the same kernel virtual address,
    // copyinstr can directly copy string bytes fromp a0 to dst
    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}
```

现在，用户将无法传递一个尝试访问内核的物理内存的指针，因为**walkaddr**会检查该地址是否在用户虚拟地址空间中。因此，一个尝试访问内核物理内存的非法访问，显然不会出现在用户页表的映射项里，walkaddr会返回失败，从而不让恶意的用户进程得逞。同理，尝试访问其它用户进程的物理内存的指针也不会通过walkaddr的严格检查。

```c
// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}
```

以上是从用户空间（如用户缓冲区）复制数据到内核中，从内核中复制数据到用户空间下也遵循相似的原则，内核提供了**copyout**来完成这个任务。同样地，目的地址必须是用户进程自己拥有的物理内存页，不然walkaddr也会阻止这次复制。

```c
// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.(user pagetable)
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}
```

## **4.5 Traps From Kernel Space**

以上我们讨论的都是发生在用户空间下的trap，现在我们来讨论发生在**内核空间下的trap**。

对于这两种模式下的trap，xv6配置相关寄存器的方式也略有不同。当CPU在内核空间下执行内核代码的时候，stvec被设置为指向**kernelvec**（kernel/kernelvec.S），用于处理内核空间下的trap。

> **第一步：kernelvec**

由于此时已经是在内核空间下，就算trap发生，也不需要修改satp和sp，内核页表和内核栈都可以继续使用的。因此kernelvec的工作很简单，当前正在运行的内核线程因trap而被暂停，直接在该内核线程的内核栈上，保存相关的寄存器即可。保存在线程**自己的栈**上这一点很重要，因为内核下的trap可能会导致线程的切换，然后内核会在另一个线程的内核栈上执行，而原线程的相关寄存器则被安全地放置在自己的内核栈上。

```c
kernelvec:
        // make room to save registers.
        // kernelvec saves the registers on the stack of the interrupted kernel thread, 
        // which makes sense because the register values belong to that thread
        addi sp, sp, -256

        // save the registers.
        sd ra, 0(sp)
        sd sp, 8(sp)
        sd gp, 16(sp)
        sd tp, 24(sp)
        sd t0, 32(sp)
        sd t1, 40(sp)
        sd t2, 48(sp)
        sd s0, 56(sp)
        sd s1, 64(sp)
        sd a0, 72(sp)
        sd a1, 80(sp)
        sd a2, 88(sp)
        sd a3, 96(sp)
        sd a4, 104(sp)
        sd a5, 112(sp)
        sd a6, 120(sp)
        sd a7, 128(sp)
        sd s2, 136(sp)
        sd s3, 144(sp)
        sd s4, 152(sp)
        sd s5, 160(sp)
        sd s6, 168(sp)
        sd s7, 176(sp)
        sd s8, 184(sp)
        sd s9, 192(sp)
        sd s10, 200(sp)
        sd s11, 208(sp)
        sd t3, 216(sp)
        sd t4, 224(sp)
        sd t5, 232(sp)
        sd t6, 240(sp)
        // call the C trap handler in trap.c
        call kerneltrap
```

内核空间下的trap发生时，当前线程在自己的栈上保存了相关的寄存器之后，内核从kernelvec跳转到**kerneltrap**（kernel/trap.c）中。

> **第二步：kerneltrap**

kerneltrap处理两种trap，设备中断和异常。内核调用devintr检查设备中断类型，并处理它。如果发现不是设备中断，就一定是异常。这个异常更为严重，因为它不是出现在用户空间下，而是出现在内核中，这是一个致命的错误，这会直接导致内核停止执行。

```c
// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void
kerneltrap()
{
  int which_dev = 0;
  // 在开始将sepc,sstatus先存起来
  // Because a yield may have disturbed the saved sepc and the saved previous mode in sstatus
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  // process’s kernel thread is running(rather than a scheduler thread)
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
  // 没有kerneltrapret，因此直接返回到kernelvec
}
```

如果是设备中断，而且是计时器中断，这就表明当前线程的时间片用完了，应该切换让别的内核线程运行，并调用yield来放弃CPU。你可能会担心，我们的kerneltrap还没有正式执行完，不用担心，我们已经事先保存过了其它线程可能会修改的寄存器，而且这个被挂起的线程，稍后就会被重新唤醒到上次的位置继续执行。

kerneltrap的主要工作执行完了之后，现在正在执行的内核线程，可能和之前是同一个，也可能是被调度运行的最新一个，无论如何，我们重新加载sepc和sstatus寄存器中的值，确保不会出现错误。现在kerneltrap正式结束，由于没有别的函数调用，它会直接返回kernelvec。

> **第三步：kernelvec**

现在我们又回到了kernelvec，接下来我们从当前内核线程的内核栈上，恢复所有保存过的寄存器。

```c
        // restore registers.
        ld ra, 0(sp)
        ld sp, 8(sp)
        ld gp, 16(sp)
        // not this, in case we moved CPUs: ld tp, 24(sp)
        ld t0, 32(sp)
        ld t1, 40(sp)
        ld t2, 48(sp)
        ld s0, 56(sp)
        ld s1, 64(sp)
        ld a0, 72(sp)
        ld a1, 80(sp)
        ld a2, 88(sp)
        ld a3, 96(sp)
        ld a4, 104(sp)
        ld a5, 112(sp)
        ld a6, 120(sp)
        ld a7, 128(sp)
        ld s2, 136(sp)
        ld s3, 144(sp)
        ld s4, 152(sp)
        ld s5, 160(sp)
        ld s6, 168(sp)
        ld s7, 176(sp)
        ld s8, 184(sp)
        ld s9, 192(sp)
        ld s10, 200(sp)
        ld s11, 208(sp)
        ld t3, 216(sp)
        ld t4, 224(sp)
        ld t5, 232(sp)
        ld t6, 240(sp)

        addi sp, sp, 256
```

最后一步，我们同样执行sret，宣告我们对于内核空间下的trap处理完毕，sret将sepc复制到pc中，根据sstatus寄存器的设置，切换到监管者模式，并重新开放中断。因此，我们最后会回到上次被打断的地方，继续执行内核代码。

```c
        // return to whatever we were doing in the kernel.
        // sret是RISV-V指令，复制sepc到pc中
        // resumes the interrupted kernel code
        sret
```

再次回忆我们讨论过的，用户空间下发生trap的第三步，在usertrapret中，xv6内核将stvec指向uservec，但我们仍在内核空间中，这段时间保持设备中断关闭是很重要的，不然内核空间下的trap会被导向uservec。RISC-V的处理方式是，trap发生的时候总是先关闭中断，直到设置好stvec，代码在正确的模式下执行时，再打开中断。

> **user trap**和**kernel trap**都已介绍完毕，那么**计时器中断**呢？为什么它是特别的？

还记得前面说过**计时器中断**是特别的吗？实际上计时器中断在**机器模式**下处理。在xv6的启动阶段中，仍处于机器模式下，在机器模式下我们通过timerinit（kernel/start.c）初始化了计时器，**CLINT**将负责产生计时器中断，更重要的是，我们通过改写mtvec寄存器的值（和stvec类似）将机器模式下的trap handler设置为timervec。因此，当CLINT产生一个计时器中断时，硬件会自动陷入机器模式，并跳转到timervec开始处理。

```c
// ask the CLINT for a timer interrupt.
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

// set the machine-mode trap handler.
  w_mtvec((uint64)timervec);
```

**timervec**如下所示（kernel/kernelvec.S），主要做的事情就是对计时器芯片重新编程，使其开始新的一轮计时，并且产生向监管者模式发出一个**软件中断**。最后，timervec通过mret返回监管者模式下，如果在监管者模式下，我们的中断是开放的，那么内核就能够捕捉到这个软件中断，内核会跳转到kernelvec，保存相关寄存器，然后在kerneltrap中，发现这是一个设备中断而且是计时器中断，从而执行相应的处理。

```c
timervec:
        # start.c has set up the memory that mscratch points to:
        # scratch[0,8,16] : register save area.
        # scratch[32] : address of CLINT's MTIMECMP register.
        # scratch[40] : desired interval between interrupts.
        
        csrrw a0, mscratch, a0
        sd a1, 0(a0)
        sd a2, 8(a0)
        sd a3, 16(a0)

        # schedule the next timer interrupt
        # by adding interval to mtimecmp.
        ld a1, 32(a0) # CLINT_MTIMECMP(hart)
        ld a2, 40(a0) # interval
        ld a3, 0(a1)
        add a3, a3, a2
        sd a3, 0(a1)

        # raise a supervisor software interrupt.
	li a1, 2
        csrw sip, a1

        ld a3, 16(a0)
        ld a2, 8(a0)
        ld a1, 0(a0)
        csrrw a0, mscratch, a0

        mret
```

## **4.6 Page-fault Exceptions**

xv6对于异常情况的处理非常简单粗暴，如果是发生在用户空间下的异常，内核直接终止该进程；如果是发生在内核空间下的异常，内核会停止执行。实际上，现在的操作系统通常有很多不同的方法来响应这些异常。

例如，**缺页错误Page-fault**这种异常，可以被很多操作系统内核利用，从而实现**写时复制Copy-On-Write**方法，例如copy-on-write fork。

我们先回忆xv6的系统调用fork（kernel/proc.c），对一个进程调用fork就会创造一个新的子进程，原来的进程自然成为父进程。最重要的是，fork会把父进程的整个内存布局，通过uvmcopy，为子进程分配物理内存，然后拷贝到子进程的空间中。

出于效率考虑，整份的内存拷贝可能不是必须的，如果子进程很快就会关闭，或者很快就要调用exec加载新的内存映像，这些复制就显得多余。一个简单的想法是，让父子进程共享父进程的物理内存，但这个解决方案显然不够好，因为两个进程现在共用堆区域和栈区域，父子进程因此很容易就干扰对方的正常运行。

当CPU无法转换一个虚拟地址时，即相应的用户页表里没有这一项映射，或者相关权限要求不满足时，就会产生我们熟知的缺页错误。根据执行指令的不同，缺页错误又可以细分为三种：

- **Load Page Faults**：无法转换的虚拟地址位于一条加载（读）指令中。Scause：13
- **Store Page Faults**：无法转换的虚拟地址位于一条存储（写）指令中。Scause：15
- **Instruction Page Faults**：无法转换的虚拟地址位于一条执行指令中。Scause：12

RISC-V会将代表缺页错误类型的数字存放进scause寄存器中，同时将无法转换的虚拟地址存放在stval寄存器中。

通过使用copy-on-write的fork，我们可以改进原来的fork。其基本思想是，在开始时，父进程和子进程共享所有的物理页，但是这些物理页全部标为**只读**。接着，父子进程其中一个尝试对这些物理页进行写入的时候，就会抛出一个缺页错误（Store Page  Fault）。接着，对于引起异常的虚拟地址尝试访问的那页物理页，内核将其复制一页并映射，因此现在父子进程在各自的虚拟地址空间内都有该物理页，开放这两页的权限都为**读和写**，并更新两个进程的页表，之后，内核会恢复执行引起异常的那条存储指令，现在它能够正确的执行。

通过往fork里引入COW，fork的整体表现更好了，因为大多数由fork产生的子进程都会马上调用exec。由于COW的使用，子进程只会出现很少的缺页错误，内核可以因此避免整个父进程内存映像的拷贝，而且COW对于进程来说是透明的。

另一个利用缺页错误而实现的很棒的功能是**懒惰分配Lazy Allocation**，它可以分两步实现。

首先，用户进程可以调用**sbrk**请求更多的物理内存，这点在前一章已经展示过，只不过，现在内核只是简单地标记该进程的大小已增长（例如增加p->sz），却不急着分配物理内存给它，因此，那些新增长的虚拟地址在页表中是无效的。接着，使用这些虚拟地址就会引发缺页错误，现在内核再为该虚拟地址分配一页物理内存，并且更新页表。

如果观察用户进程的行为，会发现它们经常向分配器请求，比它们实际上所需要的，更多的物理内存。在这种情况下，内核只在用户进程实际用到它们的时候，再来为它们分配物理页。通过这种懒惰的思想，内核尽可能地推迟物理内存的分配，可以避免很多因用户进程的贪婪而带来的不必要的分配。同样地，懒惰分配对用户进程也应该是透明的。

同样利用了缺页错误的，还有**请求调页Demand Paging**这一功能。当一个用户进程需要装载一些页到物理内存中，但RAM的空闲空间不足时，内核此时可以**逐出evict**一些物理页，将这些页写到诸如磁盘等地方，同时在相应页表中标记PTE为无效。之后，当别的进程读写这些被逐出的页时，就会产生缺页错误。内核发现这些页已经被换出到磁盘上，因此首先在RAM中为它们找到空间，然后从磁盘上重新读进这些页，改写PTE为有效，更新页表，并且重新执行读写指令。内核在RAM为这些物理页找空间的时候，可能又要逐出别的页。但是，整个过程对用户进程仍然是透明的。

请求调页避免了在进程运行时，直接将所有进程的物理页载入物理内存中，而是在进程需要的时候，才从磁盘上加载这些页面。这种模式，在满足**访存局部性**的情况下表现很好，进程在一个时间段内只访问固定一部分物理页，这避免了频繁地换入换出页。

还有其它一些功能特性也利用了分页和缺页错误来实现，例如自动增长的栈Automatically extending stacks和内存映射文件Memory-mapped files等。

## **4.7 Real World**

我们已经看到，通过将trampoline页同时映射到用户和内核空间下，可以解决很多棘手的问题。但是，如果我们直接**将内核的映射也加入用户页表**中，甚至可以彻底解决切换页表这样的棘手问题，尤其是我们反复穿梭于内核和用户空间之间，只需要适当地设置相关PTE中的标志位，保证我们建立起来的隔离机制不被破坏。系统调用也能从中受益，例如内核可以直接使用用户指针。

事实上，很多操作系统就是这么设计的，在效率上的提升很显著。xv6不这样设计的原因是，内核的代码需要相当小心地编写，否则就会出现一些严重的安全漏洞（例如用户指针使用不当），而且这会使得内核代码会相当复杂，毕竟xv6只是一个简单的教学操作系统。









# boot xv6

### 计算机配置

本机**Windows11**操作系统下使用**Vmware**安装的**Ubuntu20.04**镜像进行实验.

### qemu

````bash
# 按官方指南手册 安装必须的工具链
$ sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu 
# 单独移除掉qemu的新版本, 因为不知道为什么build时候会卡壳
$ sudo apt-get remove qemu-system-misc
# 额外安装一个旧版本的qemu
$ wget https://download.qemu.org/qemu-5.1.0.tar.xz
$ tar xf qemu-5.1.0.tar.xz
$ cd qemu-5.1.0
$ ./configure --disable-kvm --disable-werror --prefix=/usr/local --target-list="riscv64-softmmu"

# 提示ERROR: glib-2.40 gthread-2.0 is required to compile QEMU and ERROR: pixman >= 0.21
# 首先我们使用：apt-cache search glib2 看看应该安装哪个库
	$ sudo apt-get install libglib2.0-dev
# 继续使用  apt-cache search pixman, 看看应该安装哪个库
	$ sudo apt-get install libpixman-1-dev
# 原文链接：https://blog.csdn.net/fuxy3/article/details/104732541
# 编译安装
$ make
$ sudo make install

# 克隆xv6实验仓库
$ git clone git://g.csail.mit.edu/xv6-labs-2020
$ cd xv6-labs-2020
$ git checkout util

# 进行编译
$ make qemu
# 编译成功并进入xv6操作系统的shell
$ xv6 kernel is booting

$ hart 2 starting
$ hart 1 starting
$ init: starting sh
$ (shell 等待用户输入...)
 
# 尝试一下ls指令
$ ls
.              1 1 1024
..             1 1 1024
README         2 2 2226
xargstest.sh   2 3 93
cat            2 4 23680
echo           2 5 22536
forktest       2 6 13256
grep           2 7 26904
init           2 8 23320
kill           2 9 22440
ln             2 10 22312
ls             2 11 25848
mkdir          2 12 22552
rm             2 13 22544
sh             2 14 40728
stressfs       2 15 23536
usertests      2 16 150160
grind          2 17 36976
wc             2 18 24616
zombie         2 19 21816
console        3 20 0


# 在 xv6 中按 Ctrl + p 会显示当前系统的进程信息。

```shell
1 sleep  init
2 sleep  sh
```

# 在 xv6 中按 Ctrl + a ，然后按 x 即可退出 xv6 系统。
````

```shell
sudo apt install gcc-riscv64-unknown-elf
qemu-system-riscv64 --version
```

**参考链接：**https://zhuanlan.zhihu.com/p/624091268



### gdb

**安装：**

```bash
$ sudo apt-get update
# 安装相关依赖
$ sudo apt-get install libncurses5-dev python python-dev texinfo libreadline-dev
# 安装GMP
$ sudo apt install libgmp-dev
# 从清华大学开源镜像站下载gdb源码 9.2 11.1 好像都可以(约23MB) # 注意：下载13.1的话python版本不匹配
$ wget https://mirrors.tuna.tsinghua.edu.cn/gnu/gdb/gdb-9.2.tar.xz
# 解压gdb源码压缩包
$ tar -zxvf gdb-9.2.tar.xz
# 进入gdb源码目录
$ cd gdb-9.2
$ mkdir build && cd build
# 配置编译选项，这里只编译riscv64-unknown-elf一个目标文件
$ ../configure --prefix=/usr/local --with-python=/usr/bin/python --target=riscv64-unknown-elf --enable-tui=yes
# 在上面一行编译配置选项中，很多其他的文章会配置一个python选项
# 但我在尝试中发现配置了python选项后后面的编译过程会报错，不添加python选项则没有问题

# 开始编译，这里编译可能消耗较长时间，具体时长取决于机器性能
$ make clean
$ make -j$(nproc)
# 编译完成后进行安装
$ sudo make install
```

**测试：**

```bash
riscv64-unknown-elf-gdb -v
```

**调试：**

开启两个终端窗口，**保持两个终端的工作目录都位于xv6-labs-2022**，然后在一个窗口中执行`make qemu-gdb`，在另一个窗执行`riscv64-unknown-elf-gdb`

**初始化gdb：**

根据调试信息中 `To enable execution of this file add`下面的路径新建`vim .gdbinit`，加入中间那行的

**完成：**

安装完成，重新启动`riscv64-unknown-elf-gdb`，显示`0x0000000000001000 in ?? () `

**参考链接：**

* **https://blog.csdn.net/csdndogo/article/details/130772956**

* https://zhuanlan.zhihu.com/p/638731320
* https://blog.csdn.net/LostUnravel/article/details/120397168



### XV6使用GDB调试程序

**调试特定文件**

需要在.gdbinit文件中添加`set riscv use-compressed-breakpoints yes`才能在user文件中打断点

调试特定的文件例如xargs.c，则在一开始启动gdb后执行

```bash
$ riscv64-unknown-elf-gdb
(gdb) file user/_xargs
(gdb) b main
(gdb) c
# 三个c直到出现continue，再在运行窗口输入要调试的文件命令
```

打开GDB调试窗口：

```
tui enable
```

**参考链接：**

- https://blog.csdn.net/yihuajack/article/details/116571913
- https://blog.csdn.net/qq_45698833/article/details/120314168



至此, 实验环境设立完成, 我们成功boot了**xv6**这个简易操作系统.





# Lab 1 Xv6 and Unix utilities



### Lab1整体参考链接：

https://blog.csdn.net/weixin_48283247/article/details/120602005



### sleep (easy)

**实验目的**

为 xv6 系统实现 UNIX 的 sleep 程序。你的 sleep 程序应该使当前进程暂停相应的时钟周期数，时钟周期数由用户指定。例如执行 sleep 100 ，则当前进程暂停，等待 100 个时钟周期后才继续执行。

**实验要求及提示**

- 如果系统没有安装 `vim` 的请先使用命令 `sudo apt install vim` 安装 `vim` 编辑器。
- 在开始实现程序前，阅读配套教材的第一章，其内容与实验内容息息相关。
- 实现的程序应该命名为 `sleep.c` 并最后放入 `user` 目录下。
- 可以查看 `user` 目录下的其他程序(如echo.c、grep.c和rm.c)，以它们为参考，了解如何获取和传递给程序相应的命令行参数。
- 如果用户传入参数有误，即没有传入参数或者传入多个参数，程序应该能打印出错误信息。
- C 语言中的 `atoi` 函数可以将字符串转换为整数类型，在 xv6 中也已经定义了相同功能的函数。可以参考 `user/ulib.c` 或者参考机械工业出版社的《C程序设计语言(第2版·新版)》附录B.5。
- 使用系统调用 `sleep`。
- 确保 `main` 函数调用 `exit()` 以退出程序。
- 将你的 `sleep` 程序添加到 `Makefile` 的 `UPROGS` 中。只有这步完成后， `make qemu` 才能编译你写的程序。
- 完成上述步骤后，运行 `make qemu` 编译运行 `xv6` ，输入 `sleep 10` 进行测试，如果 `shell` 隔了一段时间后才出现命令提示符，则证明你的结果是正确的，可以退出 `xv6` 运行 `./grade-lab-util sleep` 或者 `make GRADEFLAGS=sleep grade` 进行单元测试。

**实验思路**

1. 参考 `user` 目录下的其他程序，先把头文件引入，即 `kernel/types.h` 声明类型的头文件和 `user/user.h` 声明系统调用函数和 `ulib.c` 中函数的头文件。
2. 编写 `main(int argc,char* argv[])` 函数。其中，参数 `argc` 是命令行总参数的个数，参数 `argv[]` 是 `argc` 个参数，其中第 0 个参数是程序的全名，其他的参数是命令行后面跟的用户输入的参数。
3. 首先，编写判断用户输入的参数是否正确的代码部分。只要判断命令行参数不等于 2 个，就可以知道用户输入的参数有误，就可以打印错误信息。但我们要如何让命令行打印出错误信息呢？我们可以参考 `user/echo.c` ，其中可以看到程序使用了 `write()` 函数。 `write(int fd, char *buf, int n)` 函数是一个系统调用，参数 `fd` 是文件描述符，0 表示标准输入，1 表示标准输出，2 表示标准错误。参数 `buf` 是程序中存放写的数据的字符数组。参数 n 是要传输的字节数，调用 `user/ulib.c` 的 `strlen()` 函数就可以获取字符串长度字节数。通过调用这个函数，就能解决输出错误信息的问题啦。认真看了提示给出的所有文件代码你可能会发现，像在 `user/grep.c` 打印信息调用的是 `fprintf()` 函数，当然，在这里使用也没有问题，毕竟 `fprintf()` 函数最后还是通过系统调用 `write()` 。最后系统调用 `exit(1)` 函数使程序退出。按照惯例，返回值 0 表示一切正常，而非 0 则表示异常。
4. 接下来获取命令行给出的时钟周期，由于命令行接收的是字符串类型，所以先使用 `atoi()` 函数把字符串型参数转换为整型。
5. 调用系统调用 `sleep` 函数，传入整型参数，使计算机程序(进程、任务或线程)进入休眠。
6. 最后调用系统调用 `exit(0)` 使程序正常退出。
7. 在 `Makefile` 文件中添加配置，照猫画虎，在 `UPROGS` 项中最后一行添加 `$U/_sleep\` ，最后这项配置如下。

```Makefile
UPROGS=\
	$U/_cat\
	$U/_echo\
	$U/_forktest\
	$U/_grep\
	$U/_init\
	$U/_kill\
	$U/_ln\
	$U/_ls\
	$U/_mkdir\
	$U/_rm\
	$U/_sh\
	$U/_stressfs\
	$U/_usertests\
	$U/_grind\
	$U/_wc\
	$U/_zombie\
	$U/_sleep\
```

**实验代码**

```c
// Lab Xv6 and Unix utilities
// sleep.c

// 引入声明类型的头文件
#include "kernel/types.h"
// 引入声明系统调用和 user/ulib.c 中其他函数的头文件
#include "user/user.h"

// int main(int argc,char* argv[])
// argc 是命令行总的参数个数  
// argv[] 是 argc 个参数，其中第 0 个参数是程序的全名，以后的参数是命令行后面跟的用户输入的参数
int
main(int argc, char *argv[])
{
    // 如果命令行参数不等于2个，则打印错误信息
    if (argc != 2)
    {
        // 系统调用 write(int fd, char *buf, int n) 函数输出错误信息
        // 参数 fd 是文件描述符，0 表示标准输入，1 表示标准输出，2 表示标准错误
        // 参数 buf 是程序中存放写的数据的字符数组
        // 参数 n 是要传输的字节数
        // 所以这里调用 user/ulib.c 的 strlen() 函数获取字符串长度字节数
        write(2, "Usage: sleep time\n", strlen("Usage: sleep time\n"));
        // 当然这里也可以使用 user/printf.c 中的 fprintf(int fd, const char *fmt, ...) 函数进行格式化输出
        // fprintf(2, "Usage: sleep time\n");
        // 退出程序
        exit(1);
    }
    // 把字符串型参数转换为整型
    int time = atoi(argv[1]);
    // 调用系统调用 sleep 函数，传入整型参数
    sleep(time);
    // 正常退出程序
    exit(0);
}
```

**实验结果**

在 xv6 中输入命令后一切符合预期。

```shell
$ sleep 10
$ sleep 1 1
Usage: sleep time
$ sleep 
Usage: sleep time
```

退出 xv6 运行单元测试。

```shell
./grade-lab-util sleep
```

提示：如果运行命令 `./grade-lab-util sleep` 报 `/usr/bin/env: ‘python’: No such file or directory` 错误，请使用命令 `vim grade-lab-util`，把第一行 `python` 改为 `python3`。如果系统没装 `python3`，请先安装 `sudo apt-get install python3` 。

通过测试样例。

```shell
make: 'kernel/kernel' is up to date.
== Test sleep, no arguments == sleep, no arguments: OK (0.9s) 
== Test sleep, returns == sleep, returns: OK (1.4s) 
== Test sleep, makes syscall == sleep, makes syscall: OK (0.9s) 
```



### pingpong (easy)

**实验要求**

使用 UNIX 系统调用编写一个程序 pingpong ，在一对管道上实现两个进程之间的通信。父进程应该通过第一个管道给子进程发送一个信息 “ping”，子进程接收父进程的信息后打印 `"<pid>: received ping"` ，其中是其进程 ID 。然后子进程通过另一个管道发送一个信息 “pong” 给父进程，父进程接收子进程的信息然后打印 `"<pid>: received pong"` ，然后退出。

**实验提示**

- 使用 `pipe` 创建管道。
- 使用 `fork` 创建一个子进程。
- 使用 `read` 从管道读取信息，使用 `write` 将信息写入管道。
- 使用 `getpid` 获取当前 `进程 ID` 。
- 将程序添加到 `Makefile` 中的 `UPROGS` 。
- xv6 上的用户程序具有有限的库函数可供它们使用。你可以在 `user/user.h` 中查看，除系统调用外其他函数代码位于 `user/ulib.c` 、`user/printf.c` 、和 `user/umalloc.c` 中。

**实验思路**

1. 首先引入头文件 `kernel/types.h` 和 `user/user.h` 。
2. 开始编写 `main` 函数，定义两个文件描述符 `ptoc_fd` 和 `ctop_fd` 数组，创建两个管道， `pipe(ptoc_fd)` 和 `pipe(ctop_fd)` ，一个用于父进程传递信息给子进程，另一个用于子进程传递信息给父进程。 `pipe(pipefd)` 系统调用会创建管道，并把读取和写入的文件描述符返回到 `pipefd` 中。 `pipefd[0]` 指管道的读取端， `pipefd[1]` 指管道的写入端。可以使用命令 `man pipe` 查看手册获取更多内容。
3. 创建缓冲区字符数组，存放传递的信息。
4. 使用 `fork` 创建子进程，子进程的 `fork` 返回值为 0 ，父进程的 `fork` 返回子进程的 `进程 ID` ，所以通过 if 语句就能让父进程和子进程执行不同的代码块。
5. 编写父进程执行的代码块。使用 `write()` 系统调用传入三个参数，把字符串 `"ping"` 写入管道一，第一个参数为管道的写入端文件描述符，第二个参数为写入的字符串，第三个参数为写入的字符串长度。然后调用 `wait()` 函数等待子进程完成操作后退出，传入参数 0 或者 NULL ，不过后者需要引入头文件 `stddef.h` 。使用 `read()` 系统调用传入三个参数，接收从管道二传来的信息，第一个参数为管道的读取端文件描述符，第二个参数为缓冲区字符数组，第三个参数为读取的字符串长度。最后调用 `printf()` 函数打印当前进程 ID 以及接收到的信息，即缓冲区的内容。
6. 编写子进程执行的代码块。使用 `read()` 系统调用传入三个参数，接收从管道一传来的信息，第一个参数为管道的读取端文件描述符，第二个参数为缓冲区字符数组，第三个参数为读取的字符串长度。然后调用 `printf()` 函数打印当前进程 ID 以及接收到的信息，即缓冲区的内容。最后使用 `write()` 系统调用传入三个参数，把字符串 `"pong"` 写入管道二，第一个参数为管道的写入端文件描述符，第二个参数为写入的字符串，第三个参数为写入的字符串长度。
7. 最后调用 `exit()` 系统调用使程序正常退出。

**实验代码**

```c
// Lab Xv6 and Unix utilities
// pingpong.c
#include "kernel/types.h"
#include "user/user.h"
#include "stddef.h"

int
main(int argc, char *argv[])
{
    int ptoc_fd[2], ctop_fd[2];
    pipe(ptoc_fd);
    pipe(ctop_fd);
    char buf[8];
    if (fork() == 0) {
        // child process
        read(ptoc_fd[0], buf, 4);
        printf("%d: received %s\n", getpid(), buf);
        write(ctop_fd[1], "pong", strlen("pong"));
    }
    else {
        // parent process
        write(ptoc_fd[1], "ping", strlen("ping"));
        wait(NULL);
        read(ctop_fd[0], buf, 4);
        printf("%d: received %s\n", getpid(), buf);
    }
    exit(0);
}
```

**实验结果**

在 `Makefile` 文件中， `UPROGS` 项追加一行 `$U/_pingpong\` 。编译并运行 xv6 进行测试。

```shell
$ make qemu
...
init: starting sh
$ pingpong
4: received ping
3: received pong
$
```

退出 xv6 ，运行单元测试检查结果是否正确。

```shell
./grade-lab-util pingpong
```

通过测试样例。

```shell
make: 'kernel/kernel' is up to date.
== Test pingpong == pingpong: OK (1.1s)
```



### primes (moderate)/(hard)

**实验要求**

使用管道将 `2` 至 `35` 中的素数筛选出来，这个想法归功于 Unix 管道的发明者 Doug McIlroy 。[链接](https://swtch.com/~rsc/thread/)中间的图片和周围的文字解释了如何操作。最后的解决方案应该放在 `user/primes.c` 文件中。
你的目标是使用 `pipe` 和 `fork` 来创建管道。第一个进程将数字 `2` 到 `35` 送入管道中。对于每个质数，你要安排创建一个进程，从其左邻通过管道读取，并在另一条管道上写给右邻。由于 xv6 的文件描述符和进程数量有限，第一个进程可以停止在 `35` 。

**实验提示**

- 请关闭进程不需要的文件描述符，否则程序将在第一个进程到达 `35` 之前耗尽 xv6 资源。
- 一旦第一个进程到达 `35` ，它应该等到整个管道终止，包括所有的子进程，孙子进程等。因此，主进程只有在打印完所有输出后才能退出，即所有其他进程已退出后主进程才能退出。
- 当管道的写入端关闭时， `read` 函数返回 `0` 。
- 最简单的方式是直接将 `32` 位（`4` 字节）的整型写到管道中，而不是使用格式化的 ASCII I/O 。
- 你应该根据需要创建进程。
- 将程序添加到 `Makefile` 中的 `UPROGS` 。

**实验思路**

![primes](https://img-blog.csdnimg.cn/22f6e4b1dd5944e5bf5f2bea059aa211.png)

1. 首先打开提示给出的链接，阅读并观察上面给出的图。由图可见，首先将数字全部输入到最左边的管道，然后第一个进程打印出输入管道的第一个数 `2` ，并将管道中所有 `2` 的倍数的数剔除。接着把剔除后的所有数字输入到右边的管道，然后第二个进程打印出从第一个进程中传入管道的第一个数 `3` ，并将管道中所有 `3` 的倍数的数剔除。接着重复以上过程，最终打印出来的数都为素数。
2. 参考 xv6 手册第一章的 Pipes 部分，其中的例子演示了运行程序 `wc` ，由此受到启发，可以使用文件描述符重定向技巧，避免 xv6 的文件描述符限制所带来的影响。所以，学习示例代码，先写一个重定向函数 `mapping` ，虽然叫重定向，但感觉还是称为映射比较好，原理就是关闭当前进程的某个文件描述符，把管道的读端或者写端的描述符通过 `dup` 函数复制给刚刚关闭的文件描述符，再把管道描述符关闭，节约了资源。
3. 编写 `main` 函数，首先创建文件描述符和创建一个管道，再创建一个子进程，子进程把管道的写端口描述符映射到原来的描述符标准输出 `1` 上。循环通过 `write` 系统调用把 `2` 到 `35` 写入管道。父进程等待子进程把数据写入完毕后，父进程把管道的读端口描述符映射到原来的描述符标准输入 `0` 上。在编写一个 `primes` 函数，调用此函数实现递归筛选的过程。
4. 编写 `primes` 函数。定义两个整型变量，存放从管道获取的数，定义管道描述符数组。从管道中读取数据，第一个数一定是素数，直接打印出来，然后创建另一个管道和子进程，子进程通过管道描述符映射，关闭不必要的文件描述符节约资源。再循环读取原管道中的数据，如果该数与原管道的第一个取模后不为 `0` ，则再次写入刚刚创建的管道。父进程等待此子进程执行完毕，再次调用重定向函数关闭不必要的文件描述符，再递归调用 `primes` 函数重复以上筛选过程，即可将管道中所有素数打印出来。

**实验代码**

```c
// Lab Xv6 and Unix utilities
// primes.c

#include "kernel/types.h"
#include "user/user.h"
#include "stddef.h"

// 文件描述符重定向(讲成映射比较好)
void
mapping(int n, int pd[])
{
  // 关闭文件描述符 n
  close(n);
  // 将管道的 读或写 端口复制到描述符 n 上
  // 即产生一个 n 到 pd[n] 的映射
  dup(pd[n]);
  // 关闭管道中的描述符
  close(pd[0]);
  close(pd[1]);
}

// 求素数
void
primes()
{
  // 定义变量获取管道中的数
  int previous, next;
  // 定义管道描述符数组
  int fd[2];
  // 从管道读取数据
  if (read(0, &previous, sizeof(int)))
  {
    // 第一个一定是素数，直接打印
    printf("prime %d\n", previous);
    // 创建管道
    pipe(fd);
    // 创建子进程
    if (fork() == 0)
    {
      // 子进程
      // 子进程将管道的写端口映射到描述符 1 上
      mapping(1, fd);
      // 循环读取管道中的数据
      while (read(0, &next, sizeof(int)))
      {
        // 如果该数不是管道中第一个数的倍数
        if (next % previous != 0)
        {
          // 写入管道
          write(1, &next, sizeof(int));
        }
      }
    }
    else
    {
      // 父进程
      // 等待子进程把数据全部写入管道
      wait(NULL);
      // 父进程将管道的读端口映射到描述符 0 上
      mapping(0, fd);
      // 递归执行此过程
      primes();
    }  
  }  
}

int 
main(int argc, char *argv[])
{
  // 定义描述符
  int fd[2];
  // 创建管道
  pipe(fd);
  // 创建进程
  if (fork() == 0)
  {
    // 子进程
    // 子进程将管道的写端口映射到描述符 1 上
    mapping(1, fd);
    // 循环获取 2 至 35
    for (int i = 2; i < 36; i++)
    {
      // 将其写入管道
      write(1, &i, sizeof(int));
    }
  }
  else
  {
    // 父进程
    // 等待子进程把数据全部写入管道
    wait(NULL);
    // 父进程将管道的读端口映射到描述符 0 上
    mapping(0, fd);
    // 调用 primes() 函数求素数
    primes();
  }
  // 正常退出
  exit(0);
}
```

**实验结果**

在 `Makefile` 文件中， `UPROGS` 项追加一行 `$U/_primes\` 。编译并运行 xv6 进行测试。

```shell
$ make qemu
...
init: starting sh
$ primes
prime 2
prime 3
prime 5
prime 7
prime 11
prime 13
prime 17
prime 19
prime 23
prime 29
prime 31
$
```

退出 xv6 ，运行单元测试检查结果是否正确。

```shell
./grade-lab-util primes
```

通过测试样例。

```shell
make: 'kernel/kernel' is up to date.
== Test primes == primes: OK (1.4s) 
```



### find (moderate)

**预备知识**

- **dirent结构体**

```
struct dirent
{
   long d_ino; /* inode number 索引节点号 */
   off_t d_off; /* offset to this dirent 在目录文件中的偏移 */
   unsigned short d_reclen; /* length of this d_name 文件名长 */
   unsigned char d_type; /* the type of d_name 文件类型 */
   char d_name [NAME_MAX+1]; /* file name (null-terminated) 文件名，最长255字符 */
}
```

- **stat结构体**

（1）简介：

用来描述一个linux系统文件系统中的文件属性的结构。stat函数获取文件的所有相关信息，一般情况下，我们关心文件大小和创建时间、访问时间、修改时间。

（2）函数原型：

```bash
int stat(const char *path, struct stat *buf);
int lstat(const char *path, struct stat *buf);
# 这些两个函数返回关于文件的信息。两个函数的第一个参数都是文件的路径，第二个参数是struct stat的指针。返回值为0，表示成功执行。
```

```
struct stat {

        mode_t     st_mode;       //文件对应的模式，文件，目录等
        ino_t      st_ino;       //inode节点号
        dev_t      st_dev;        //设备号码
        dev_t      st_rdev;       //特殊设备号码
        nlink_t    st_nlink;      //文件的连接数
        uid_t      st_uid;        //文件所有者
        gid_t      st_gid;        //文件所有者对应的组
        off_t      st_size;       //普通文件，对应的文件字节数
        time_t     st_atime;      //文件最后被访问的时间
        time_t     st_mtime;      //文件内容最后被修改的时间
        time_t     st_ctime;      //文件状态改变时间
        blksize_t st_blksize;    //文件内容对应的块大小
        blkcnt_t   st_blocks;     //伟建内容对应的块数量
      };
```

预备知识参考链接：https://blog.csdn.net/chen1415886044/article/details/102887154



**实验要求**

编写一个简单的 UNIX `find` 程序，在目录树中查找包含特定名称的所有文件。你的解决方案应放在 `user/find.c` 。

**实验提示**

- 查看 `user/ls.c` 了解如何读目录。
- 使用递归查找子目录下的文件。
- 不要递归 `"."` 和 `"..."` 。
- 文件系统的变化在 `qemu` 的运行中持续存在。使用 `make clean` 然后再 `make qemu` 让一个干净的文件系统运行。
- 你需要使用 C 字符串。可以参考机械工业出版社的《C程序设计语言(第2版·新版)》第 5.5 节。
- 请注意，比较字符串不能像在 `Python` 中使用 `==` 一样。请使用 `strcmp()` 函数。
- 支持使用正则表达式匹配字符串，参照 `user/grep.c` 当中的正则匹配器。
- 将程序添加到 `Makefile` 的 `UPROGS` 中。

**实验思路**

1. 根据提示，我们可以先阅读 `user/ls.c` 源码查看如何读目录。
2. 在 `user/ls.c` 中，有一个名为 `fmtname()` 的函数，其目的是将路径格式化为文件名，也就是把名字变成前面没有左斜杠 `/` ，仅仅保存文件名。
3. 在 `user/ls.c` 中，有一个名为 `ls()` 的函数，首先函数里面声明了需要用到的变量，包括文件名缓冲区、文件描述符、文件相关的结构体等等。其次使用 `open()` 函数进入路径，判断此路径是文件还是文件名。
4. 如果是文件类型，则打印出文件信息，包括文件名，类型，Inode号，文件大小（单位为bytes）。如果是目录类型，则获取目录下的文件名，并依次输出文件信息。如果该文件还为路径，则循环遍历输出该路径下的文件信息。
5. 可以把 `user/ls.c` 中的大部分代码重复利用，只要修改传入参数的数量，以及文件名之间的比较部分，就能完成此次实验。
6. 下面的参考实验代码只使用了 `find()` 函数，与 `ls()` 函数一样，首先声明了文件名缓冲区、文件描述符和文件相关的结构体。其次试探是否能进入给定路径，然后调用系统调用获得一个已存在文件的模式，并判断其类型。如果该路径不是目录类型就报错。接着把绝对路径进行拷贝，循环获取路径下的文件名，与要查找的文件名进行比较，如果类型为文件且名称与要查找的文件名相同则输出路径，如果是目录类型则递归调用 `find()` 函数继续查找。

**实验代码**

```c
// Lab Xv6 and Unix utilities
// find.c

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

// find 函数
void
find(char *dir, char *file)
{   
    // 声明 文件名缓冲区 和 指针
    char buf[512], *p;
    // 声明文件描述符 fd
    int fd;
    // 声明与文件相关的结构体
    struct dirent de;
    struct stat st;

    // open() 函数打开路径，返回一个文件描述符，如果错误返回 -1
    if ((fd = open(dir, 0)) < 0)
    {
        // 报错，提示无法打开此路径
        fprintf(2, "find: cannot open %s\n", dir);
        return;
    }

    // int fstat(int fd, struct stat *);
    // 系统调用 fstat 与 stat 类似，但它以文件描述符作为参数
    // int stat(char *, struct stat *);
    // stat 系统调用，可以获得一个已存在文件的模式，并将此模式赋值给它的副本
    // stat 以文件名作为参数，返回文件的 i 结点中的所有信息
    // 如果出错，则返回 -1
    if (fstat(fd, &st) < 0)
    {
        // 出错则报错
        fprintf(2, "find: cannot stat %s\n", dir);
        // 关闭文件描述符 fd
        close(fd);
        return;
    }

    // 如果不是目录类型
    if (st.type != T_DIR)
    {
        // 报类型不是目录错误
        fprintf(2, "find: %s is not a directory\n", dir);
        // 关闭文件描述符 fd
        close(fd);
        return;
    }

    // 如果路径过长放不入缓冲区，则报错提示
    if(strlen(dir) + 1 + DIRSIZ + 1 > sizeof buf)
    {
        fprintf(2, "find: directory too long\n");
        // 关闭文件描述符 fd
        close(fd);
        return;
    }
    // 将 dir 指向的字符串即绝对路径复制到 buf
    strcpy(buf, dir);
    // buf 是一个绝对路径，p 是一个文件名，通过加 "/" 前缀拼接在 buf 的后面
    p = buf + strlen(buf);
    *p++ = '/';
    // 读取 fd ，如果 read 返回字节数与 de 长度相等则循环
    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {
        if(de.inum == 0)
            continue;
        // strcmp(s, t);
        // 根据 s 指向的字符串小于（s<t）、等于（s==t）或大于（s>t） t 指向的字符串的不同情况
        // 分别返回负整数、0或正整数
        // 不要递归 "." 和 "..."
        if (!strcmp(de.name, ".") || !strcmp(de.name, ".."))
            continue;
        // memmove，把 de.name 信息复制 p，其中 de.name 代表文件名
        memmove(p, de.name, DIRSIZ);
        // 设置文件名结束符
        p[DIRSIZ] = 0;
        // int stat(char *, struct stat *);
        // stat 系统调用，可以获得一个已存在文件的模式，并将此模式赋值给它的副本
        // stat 以文件名作为参数，返回文件的 i 结点中的所有信息
        // 如果出错，则返回 -1
        if(stat(buf, &st) < 0)
        {
            // 出错则报错
            fprintf(2, "find: cannot stat %s\n", buf);
            continue;
        }
        // 如果是目录类型，递归查找
        if (st.type == T_DIR)
        {
            find(buf, file);
        }
        // 如果是文件类型 并且 名称与要查找的文件名相同
        else if (st.type == T_FILE && !strcmp(de.name, file))
        {
            // 打印缓冲区存放的路径
            printf("%s\n", buf);
        } 
    }
}

int
main(int argc, char *argv[])
{
    // 如果参数个数不为 3 则报错
    if (argc != 3)
    {
        // 输出提示
        fprintf(2, "usage: find dirName fileName\n");
        // 异常退出
        exit(1);
    }
    // 调用 find 函数查找指定目录下的文件
    find(argv[1], argv[2]);
    // 正常退出
    exit(0);
}
```

**实验结果**

在 `Makefile` 文件中， `UPROGS` 项追加一行 `$U/_find\` 。编译并运行 xv6 进行测试。

```shell
$ make clean
...
$ make qemu
...
init: starting sh
$ echo > b
$ mkdir a
$ echo > a/b
$ find . b
./b
./a/b
$ 
```

退出 xv6 ，运行单元测试检查结果是否正确。

```shell
./grade-lab-util find
```

通过测试样例。

```shell
make: 'kernel/kernel' is up to date.
== Test find, in current directory == find, in current directory: OK (1.4s) 
== Test find, recursive == find, recursive: OK (1.0s) 
```



### ls和find思路代码详解

参考链接：https://zhuanlan.zhihu.com/p/508045883



### xargs (moderate)

预备知识

1.Shell 将一个命令的输出发送给另一个命令

参考链接：https://geek-docs.com/shell/shell-examples/send-the-input-of-one-command-to-another.html

2.管道 `|` 将 `command1` 的输出传递给了 `command2` 的**标准输入**，而**不是**将其作为 `command2` 的**参数**。

**实验要求**

编写一个简单的 UNIX `xargs` 程序，从标准输入中读取行并为每一行运行一个命令，将该行作为命令的**参数**提供。你的解决方案应该放在 `user/xargs.c` 中。

**实验提示**

- 使用 `fork()` 和 `exec()` 在每一行输入上调用命令。在 `parent` 中使用 `wait()` 等待 `child` 完成命令。
- 要读取单个输入行，请一次读取一个字符，直到出现换行符(`'\n'`)。
- `kernel/param.h` 声明了 `MAXARG` ，如果你需要声明一个 `argv` 数组，这可能很有用。
- 将程序添加到 `Makefile` 的 `UPROGS` 中。
- 文件系统的变化在 `qemu` 的运行中持续存在。使用 `make clean` 然后再 `make qemu` 让一个干净的文件系统运行。

**实验思路**

1. 根据提示，我们需要调用 `fork()` 创建子进程，和调用 `exec()` 执行命令。我们知道要从标准输入中读取行并为每行运行一个命令，且将该行作为命令的参数。即把输入的字符放到命令后面，然后调用 `exec()` 。我们可以依次处理每行，根据空格符和换行符分割参数，调用子进程执行命令。
2. 首先，我们定义一个字符数组，作为子进程的参数列表，其大小设置为 `kernel/param.h` 中定义的 `MAXARG` ，用于存放子进程要执行的参数。而后，建立一个索引便于后面追加参数，并循环拷贝一份命令行参数，即拷贝 `xargs` 后面跟的参数。创建缓冲区，用于存放从管道读出的数据。
3. 然后，循环读取管道中的数据，放入缓冲区，建立一个新的临时缓冲区存放追加的参数。把临时缓冲区追加到子进程参数列表后面。并循环获取缓冲区字符，当该字符不是换行符时，直接给临时缓冲区；否则创建一个子进程，把执行的命令和参数列表传入 `exec()` 函数中，执行命令。当然，这里一定要注意，父进程一定得等待子进程执行完毕。

**实验代码**

```c
// Lab Xv6 and Unix utilities
// xargs.c

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXN 1024

int
main(int argc, char *argv[])
{
    // 如果参数个数小于 2
    if (argc < 2) {
        // 打印参数错误提示
        fprintf(2, "usage: xargs command\n");
        // 异常退出
        exit(1);
    }
    // 存放子进程 exec 的参数
    char * argvs[MAXARG];
    // 索引
    int index = 0;
    // 略去 xargs ，用来保存命令行参数
    for (int i = 1; i < argc; ++i) {
        argvs[index++] = argv[i];
    }
    // 缓冲区存放从管道读出的数据
    char buf[MAXN] = {"\0"};
    
    int n;
	// 0 代表的是管道的 0，也就是从管道循环读取数据
    while((n = read(0, buf, MAXN)) > 0 ) {
        // 临时缓冲区存放追加的参数
		char temp[MAXN] = {"\0"};
        // xargs 命令的参数后面再追加参数
        argvs[index] = temp;
        // 内循环获取追加的参数并创建子进程执行命令
        for(int i = 0; i < strlen(buf); ++i) {
            // 读取单个输入行，当遇到换行符时，创建子线程
            if(buf[i] == '\n') {
                // 创建子线程执行命令
                if (fork() == 0) {
                    exec(argv[1], argvs);
                }
                // 等待子线程执行完毕
                wait(0);
            } else {
                // 否则，读取管道的输出作为输入
                temp[i] = buf[i];
            }
        }
    }
    // 正常退出
    exit(0);
}
```

**实验结果**

在 `Makefile` 文件中， `UPROGS` 项追加一行 `$U/_xargs\` 。编译并运行 xv6 进行测试。

```shell
$ make clean
...
$ make qemu
...
init: starting sh
$ echo hello too | xargs echo bye
bye hello too
```

退出 xv6 ，运行单元测试检查结果是否正确。

```shell
./grade-lab-util xargs
```

通过测试样例。

```shell
make: 'kernel/kernel' is up to date.
== Test xargs == xargs: OK (1.8s) 
```



### Lab 1 所有实验测试

退出 xv6 ，运行整个 Lab 1 测试，检查结果是否正确。

```shell
$ make grade
...
== Test sleep, no arguments == 
$ make qemu-gdb
sleep, no arguments: OK (2.4s) 
== Test sleep, returns == 
$ make qemu-gdb
sleep, returns: OK (1.2s) 
== Test sleep, makes syscall == 
$ make qemu-gdb
sleep, makes syscall: OK (0.8s) 
== Test pingpong == 
$ make qemu-gdb
pingpong: OK (1.0s) 
== Test primes == 
$ make qemu-gdb
primes: OK (1.3s) 
== Test find, in current directory == 
$ make qemu-gdb
find, in current directory: OK (0.6s) 
== Test find, recursive == 
$ make qemu-gdb
find, recursive: OK (3.0s) 
== Test xargs == 
$ make qemu-gdb
xargs: OK (3.9s) 
== Test time == 
time: OK 
Score: 100/100
```



### Shell

参考链接：https://blog.csdn.net/woxiaohahaa/article/details/49365957





# Lab4 Traps

You should **add a new `sigalarm(interval, handler)` system call.** If an application calls `sigalarm(n, fn)`, then after every `n` "ticks" of CPU time that the program consumes, the kernel should cause **application function `fn`** to be called. When `fn` returns, the application should **resume where it left off.** A tick is a fairly arbitrary unit of time in xv6, determined by how often a hardware timer generates interrupts. **If an application calls `sigalarm(0, 0)`, the kernel should stop generating periodic alarm calls**.

You'll find a file `user/alarmtest.c` in your xv6 repository. **Add it to the Makefile.** It won't compile correctly until you've added `sigalarm` and `sigreturn` system calls (see below).

`alarmtest` calls `sigalarm(2, periodic)` in `test0` to ask the kernel to force a call to `periodic()` every 2 ticks, and then spins for a while.  You can see the assembly code for alarmtest in user/alarmtest.asm, which may be handy for debugging.  Your solution is correct when `alarmtest` produces output like this and usertests also runs correctly:
