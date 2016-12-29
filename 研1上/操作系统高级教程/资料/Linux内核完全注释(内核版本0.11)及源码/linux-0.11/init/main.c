/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>
#include <time.h>


/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */
static inline _syscall0(int,fork)
static inline _syscall0(int,pause)
static inline _syscall1(int,setup,void *,BIOS)
static inline _syscall0(int,sync)

#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>
#include <asm/io.h>

#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include <linux/fs.h>

static char printbuf[1024];

extern int vsprintf();
extern void init(void);
extern void blk_dev_init(void);
extern void chr_dev_init(void);
extern void hd_init(void);
extern void floppy_init(void);
extern void mem_init(long start, long end);
extern long rd_init(long mem_start, int length);
extern long kernel_mktime(struct tm * tm);
extern long startup_time;

/*
 * This is set up by the setup-routine at boot-time
 */
#define EXT_MEM_K (*(unsigned short *)0x90002)
#define DRIVE_INFO (*(struct drive_info *)0x90080)
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec = CMOS_READ(0);
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8);
		time.tm_year = CMOS_READ(9);
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	time.tm_mon--;
	startup_time = kernel_mktime(&time);
}

static long memory_end = 0;
static long buffer_memory_end = 0;
static long main_memory_start = 0;

struct drive_info { char dummy[32]; } drive_info;


void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (well, ...) this */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
 	ROOT_DEV = ORIG_ROOT_DEV;
 	drive_info = DRIVE_INFO;
	memory_end = (1<<20) + (EXT_MEM_K<<10);
	memory_end &= 0xfffff000;
	if (memory_end > 16*1024*1024)
		memory_end = 16*1024*1024;
	if (memory_end > 12*1024*1024) 
		buffer_memory_end = 4*1024*1024;
	else if (memory_end > 6*1024*1024)
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;
	main_memory_start = buffer_memory_end;
#ifdef RAMDISK
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);
#endif
	mem_init(main_memory_start,memory_end);
	trap_init();
	blk_dev_init();
	chr_ddev_init();
	tty_init();
	time_init();
	sched_init();
	buffer_init(buffer_memory_end);
	hd_init();
	floppy_init();
	sti();
	move_to_user_mode();
	if (!fork()) {		/* we count on this going ok */
		init();
	}
/*
 *   NOTE!!   For any other task 'pause()' would mean we have to get a
 * signal to awaken, but task0 is the sole exception (see 'schedule()')
 * as task 0 gets activated at every idle moment (when no other tasks
 * can run). For task0 'pause()' just means we go check if some other
 * task can run, and if not we return here.
 */
	for(;;) pause();
}

static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

static char * argv_rc[] = { "/bin/sh", NULL };
static char * envp_rc[] = { "HOME=/", NULL };

static char * argv[] = { "-/bin/sh",NULL };
static char * envp[] = { "HOME=/usr/root", NULL };

void init(void)
{
	int pid,i;

	setup((void *) &drive_info);
	(void) open("/dev/tty0",O_RDWR,0);
	(void) dup(0);
	(void) dup(0);
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);
	if (!(pid=fork())) {
		close(0);
		if (open("/etc/rc",O_RDONLY,0))
			_exit(1);
		execve("/bin/sh",argv_rc,envp_rc);
		_exit(2);
	}
	if (pid>0)
		while (pid != wait(&i))
			/* nothing */;
	while (1) {
		if ((pid=fork())<0) {
			printf("Fork failed in init\r\n");
			continue;
		}
		if (!pid) {
			close(0);close(1);close(2);
			setsid();
			(void) open("/dev/tty0",O_RDWR,0);
			(void) dup(0);
			(void) dup(0);
			_exit(execve("/bin/sh",argv,envp));
		}
		while (1)
			if (pid == wait(&i))
				break;
		printf("\n\rchild %d died with code %04x\n\r",pid,i);
		sync();
	}
	_exit(0);	/* NOTE! _exit, not exit() */
}


//1为什么计算机启动最开始的时候执行的是BIOS代码而不是操作系统自身的代码？
//计算机启动的时候，内存未初始化， CPU不能直接从外设运行操作系统，所以必须将操作系统加载至内存中。而这个工作最开始的部分，BIOS需要完成一些检测工作，和设置实模式下的中断向量表和服务程序，并将操作系统的引导扇区加载值 0x7C00 处，然后将跳转至 0x7C00。这些就是由bios程序来实现的。所以计算机启动最开始执行的是bios代码。
//
//2.为什么BIOS只加载了一个扇区，后续扇区却是由bootsect代码加载？为什么BIOS没有把所有需要加载的扇区都加载？
//对BIOS而言，“约定”在接到启动操作系统的命令后，“定位识别”只从启动扇区把代码加载到0x7c00这个位置。后续扇区则由bootsect代码加载，这些代码由编写系统的用户负责，与BIOS无关。这样构建的好处是站在整个体系的高度，统一设计和统一安排，简单而有效。BIOS和操作系统的开发都可以遵循这一约定，灵活地进行各自的设计。操作系统的开发也可以按照自己的意愿，内存的规划，等等都更为灵活
//
//3.为什么BIOS把bootsect加载到0x07c00，而不是0x00000？加载后又马上挪到0x90000处，是何道理？为什么不一次加载到位？
//1）因为BIOS将从0x00000开始的1KB字节构建了了中断向量表，接着的256KB字节内存空间构建了BIOS数据区，所以不能把bootsect加载到0x00000. 0X07c00是BIOS设置的内存地址，不是bootsect能够决定的。
//2）首先，在启动扇区中有一些数据，将会被内核利用到。
//其次，依据系统对内存的规划，内核终会占用0x0000其实的空间，因此0x7c00可能会被覆盖。
//将该扇区挪到0x90000，在setup.s中，获取一些硬件数据保存在0x90000~0x901ff处，可以对一些后面内核将要利用的数据，集中保存和管理。
//
//4.bootsect、setup、head程序之间是怎么衔接的？给出代码证据。
//1)bootsect跳转到setup程序： jmpi 0,SETUPSEG;
//这条语句跳转到0x90200处，即setup程序加载的位子，CS:IP指向setup程序的第一条指令，意味着setup开始执行。
//2)setup跳转到head程序：CPU工作模式首先转变为保护模式然后执行 jmpi 0,8
//0指的是段内偏移，8是保护模式下的段选择符：01000，其中后两位表示内核特权级，第三位0代表GDT，1则表示GDT表中的第一项，即内核代码段，段基质为0x0000000,而head程序地址就在这里，意味着head程序开始执行。
//
//5.setup程序里的cli是为了什么？
//cli为关中断，以为着程序在接下来的执行过程中，无论是否发生中断，系统都不再对此中断进行响应。
//因为在setup中，需要将位于 0x10000 的内核程序复制到 0x0000 处，bios中断向量表覆盖掉了，若此时如果产生中断，这将破坏原有的中断机制会发生不可预知的错误，所以要禁示中断。
//
//6.setup程序的最后是jmpi 0,8 为什么这个8不能简单的当作阿拉伯数字8看待？
//这里8要看成二进制1000，最后两位00表示内核特权级，第三位0表示GDT表，第四位1表示所选的表（在此就是GDT表）的1项来确定代码段的段基址和段限长等信息。这样，我们可以得到代码是从段基址0x00000000、偏移为0处开始执行的，即head的开始位置。注意到已经开启了保护模式的机制，所以这里的8不能简单的当成阿拉伯数字8来看待。
//
//
//7.打开A20和打开pe究竟是什么关系，保护模式不就是32位的吗？为什么还要打开A20？有必要吗？
//1、打开A20仅仅意味着CPU可以进行32位寻址，且最大寻址空间是4GB。打开PE是进入保护模式。A20是cpu的第21位地址线，A20未打开的时候，实模式中cs：ip最大寻址为1MB+64KB,而第21根地址线被强制为0，所以相当于cpu“回滚”到内存地址起始处寻址。当打开A20的时候，实模式下cpu可以寻址到1MB以上的高端内存区。A20未打开时，如果打开pe，则cpu进入保护模式，但是可以访问的内存只能是奇数1M段，即0-1M,2M-3M,4-5M等。A20被打开后，如果打开pe，则可以访问的内存是连续的。打开A20是打开PE的必要条件；而打开A20不一定非得打开PE。
//2、有必要。打开PE只是说明系统处于保护模式下，但若真正在保护模式下工作，必须打开A20，实现32位寻址。 
//
//8.Linux是用C语言写的，为什么没有从main还是开始，而是先运行3个汇编程序，道理何在？
//main 函数运行在 32 位的保护模式下，但系统启动时默认为 16 位的实模式， 开机时的 16 位实 模式与 main 函数执行需要的 32 位保护模式之间有很大的差距，这个差距需要由 3 个汇编程序来 填补。其中 bootsect 负责加载， setup 与 head 则负责获取硬件参数，准备 idt,gdt,开启 A20， PE,PG， 废弃旧的 16 位中断响应机制，建立新的 32 为 IDT，设置分页机制等。这些工作做完后，计算机处在了32位的保护模式状态了，调用main的条件就算准备完毕。
//
//9.为什么不用call，而是用ret“调用”main函数？画出调用路线图，给出代码证据。（图在P42）
//call指令会将EIP的值自动压栈，保护返回现场，然后执行被调函数的程序，等到执行被调函数的ret指令时，自动出栈给EIP并还原现场，继续执行call的下一条指令。然而对操作系统的main函数来说，如果用call调用main函数，那么ret时返回给谁呢？因为没有更底层的函数程序接收操作系统的返回。用ret实现的调用操作当然就不需要返回了，call做的压栈和跳转动作需要手工编写代码。
//after_page_tables:
//        pushl $__main; //将main的地址压入栈，即EIP
//              setup_paging:
//              ret; //弹出EIP，针对EIP指向的值继续执行，即main函数的入口地址。
//
//10.保护模式的“保护”体现在哪里？
//1）在 GDT、 LDT 及 IDT 中，均有自己界限，特权级等属性，这是对描述符所描述的对象的保护
//2）在不同特权级间访问时，系统会对 CPL、 RPL、 DPL、 IOPL 等进行检验，对不同层级的程 序进行保护， 同还限制某些特殊指令的使用， 如 lgdt, lidt,cli 等
//3）分页机制中 PDE 和 PTE 中的 R/W 和 U/S 等，提供了页级保护。分页机制将线性地址与物 理地址加以映射，提供了对物理地址的保护。
//P438
//