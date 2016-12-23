/*
 *  linux/kernel/traps.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'Traps.c' handles hardware traps and faults after we have saved some
 * state in 'asm.s'. Currently mostly a debugging-aid, will be extended
 * to mainly kill the offending process (probably by giving it a signal,
 * but possibly by killing it outright if necessary).
 */
#include <string.h>

#include <linux/head.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/io.h>

#define get_seg_byte(seg,addr) ({ \
register char __res; \
__asm__("push %%fs;mov %%ax,%%fs;movb %%fs:%2,%%al;pop %%fs" \
	:"=a" (__res):"0" (seg),"m" (*(addr))); \
__res;})

#define get_seg_long(seg,addr) ({ \
register unsigned long __res; \
__asm__("push %%fs;mov %%ax,%%fs;movl %%fs:%2,%%eax;pop %%fs" \
	:"=a" (__res):"0" (seg),"m" (*(addr))); \
__res;})

#define _fs() ({ \
register unsigned short __res; \
__asm__("mov %%fs,%%ax":"=a" (__res):); \
__res;})

int do_exit(long code);

void page_exception(void);

void divide_error(void);
void debug(void);
void nmi(void);
void int3(void);
void overflow(void);
void bounds(void);
void invalid_op(void);
void device_not_available(void);
void double_fault(void);
void coprocessor_segment_overrun(void);
void invalid_TSS(void);
void segment_not_present(void);
void stack_segment(void);
void general_protection(void);
void page_fault(void);
void coprocessor_error(void);
void reserved(void);
void parallel_interrupt(void);
void irq13(void);

static void die(char * str,long esp_ptr,long nr)
{
	long * esp = (long *) esp_ptr;
	int i;

	printk("%s: %04x\n\r",str,nr&0xffff);
	printk("EIP:\t%04x:%p\nEFLAGS:\t%p\nESP:\t%04x:%p\n",
		esp[1],esp[0],esp[2],esp[4],esp[3]);
	printk("fs: %04x\n",_fs());
	printk("base: %p, limit: %p\n",get_base(current->ldt[1]),get_limit(0x17));
	if (esp[4] == 0x17) {
		printk("Stack: ");
		for (i=0;i<4;i++)
			printk("%p ",get_seg_long(0x17,i+(long *)esp[3]));
		printk("\n");
	}
	str(i);
	printk("Pid: %d, process nr: %d\n\r",current->pid,0xffff & i);
	for(i=0;i<10;i++)
		printk("%02x ",0xff & get_seg_byte(esp[1],(i+(char *)esp[0])));
	printk("\n\r");
	do_exit(11);		/* play segment exception */
}

void do_double_fault(long esp, long error_code)
{
	die("double fault",esp,error_code);
}

void do_general_protection(long esp, long error_code)
{
	die("general protection",esp,error_code);
}

void do_divide_error(long esp, long error_code)
{
	die("divide error",esp,error_code);
}

void do_int3(long * esp, long error_code,
		long fs,long es,long ds,
		long ebp,long esi,long edi,
		long edx,long ecx,long ebx,long eax)
{
	int tr;

	__asm__("str %%ax":"=a" (tr):"0" (0));
	printk("eax\t\tebx\t\tecx\t\tedx\n\r%8x\t%8x\t%8x\t%8x\n\r",
		eax,ebx,ecx,edx);
	printk("esi\t\tedi\t\tebp\t\tesp\n\r%8x\t%8x\t%8x\t%8x\n\r",
		esi,edi,ebp,(long) esp);
	printk("\n\rds\tes\tfs\ttr\n\r%4x\t%4x\t%4x\t%4x\n\r",
		ds,es,fs,tr);
	printk("EIP: %8x   CS: %4x  EFLAGS: %8x\n\r",esp[0],esp[1],esp[2]);
}

void do_nmi(long esp, long error_code)
{
	die("nmi",esp,error_code);
}

void do_debug(long esp, long error_code)
{
	die("debug",esp,error_code);
}

void do_overflow(long esp, long error_code)
{
	die("overflow",esp,error_code);
}

void do_bounds(long esp, long error_code)
{
	die("bounds",esp,error_code);
}

void do_invalid_op(long esp, long error_code)
{
	die("invalid operand",esp,error_code);
}

void do_device_not_available(long esp, long error_code)
{
	die("device not available",esp,error_code);
}

void do_coprocessor_segment_overrun(long esp, long error_code)
{
	die("coprocessor segment overrun",esp,error_code);
}

void do_invalid_TSS(long esp,long error_code)
{
	die("invalid TSS",esp,error_code);
}

void do_segment_not_present(long esp,long error_code)
{
	die("segment not present",esp,error_code);
}

void do_stack_segment(long esp,long error_code)
{
	die("stack segment",esp,error_code);
}

void do_coprocessor_error(long esp, long error_code)
{
	if (last_task_used_math != current)
		return;
	die("coprocessor error",esp,error_code);
}

void do_reserved(long esp, long error_code)
{
	die("reserved (15,17-47) error",esp,error_code);
}

void trap_init(void)
{
	int i;

	set_trap_gate(0,&divide_error);
	set_trap_gate(1,&debug);
	set_trap_gate(2,&nmi);
	set_system_gate(3,&int3);	/* int3-5 can be called from all */
	set_system_gate(4,&overflow);
	set_system_gate(5,&bounds);
	set_trap_gate(6,&invalid_op);
	set_trap_gate(7,&device_not_available);
	set_trap_gate(8,&double_fault);
	set_trap_gate(9,&coprocessor_segment_overrun);
	set_trap_gate(10,&invalid_TSS);
	set_trap_gate(11,&segment_not_present);
	set_trap_gate(12,&stack_segment);
	set_trap_gate(13,&general_protection);
	set_trap_gate(14,&page_fault);
	set_trap_gate(15,&reserved);
	set_trap_gate(16,&coprocessor_error);
	for (i=17;i<48;i++)
		set_trap_gate(i,&reserved);
	set_trap_gate(45,&irq13);
	outb_p(inb_p(0x21)&0xfb,0x21);
	outb(inb_p(0xA1)&0xdf,0xA1);
	set_trap_gate(39,&parallel_interrupt);
}



//+1. 用图表示下面的几种情况，并从代码中找到证据：
//A当进程获得第一个缓冲块的时候，hash表的状态
//		B经过一段时间的运行。已经有2000多个buffer_head挂到hash_table上时，hash表（包括所有的buffer_head）的整体运行状态。
//C经过一段时间的运行，有的缓冲块已经没有进程使用了（空闲），这样的空闲缓冲块是否会从hash_table上脱钩？
//D经过一段时间的运行，所有的buffer_head都挂到hash_table上了，这时，又有进程申请空闲缓冲块，将会发生什么？
//A
//getblk(int dev, int block)  get_hash_table(dev,block) -> find_buffer(dev,block) -> hash(dev, block)
//哈希策略为：
//#define _hashfn(dev,block)(((unsigned)(dev block))%NR_HASH)
//#define hash(dev,block) hash_table[_hashfn(dev, block)]
//此时，dev为0x300，block为0，NR_HASH为307，哈希结果为154，将此块插入哈希表中次位置后
//
//		B
////代码路径 ：fs/buffer.c:
//…
//static inline void insert_into_queues(struct buffer_head * bh) {
///*put at end of free list */
//
//
//	bh->b_next_free= free_list;
//	bh->b_prev_free= free_list->b_prev_free;
//	free_list->b_prev_free->b_next_free= bh;
//	free_list->b_prev_free= bh;
///*put the buffer in new hash-queue if it has a device */
//	bh->b_prev= NULL;
//	bh->b_next= NULL;
//	if (!bh->b_dev)
//		return;
//	bh->b_next= hash(bh->b_dev,bh->b_blocknr);
//	hash(bh->b_dev,bh->b_blocknr)= bh;
//	bh->b_next->b_prev= bh
//}
//C
//		不会脱钩，会调用brelse()函数，其中if(!(buf->b_count--))，计数器减一。没有对该缓冲块执行remove操作。由于硬盘读写开销一般比内存大几个数量级，因此该空闲缓冲块若是能够再次被访问到，对提升性能是有益的。
//D
//		进程顺着freelist找到没被占用的，未被上锁的干净的缓冲块后，将其引用计数置为1，然后从哈西队列和空闲块链表中移除该bh，然后根据此新的设备号和块号重新插入空闲表和哈西队列新位置处，最终返回缓冲头指针。
//Bh->b_count=1;
//Bh->b_dirt=0;
//Bh->b_uptodate=0;
//Remove_from_queues(bh);
//Bh->b_dev=dev;
//Bh->b_blocknr=block;
//Insert_into_queues(bh);
//
//+2. Rd_load()执行完之后，虚拟盘已经成为可用的块设备，并成为根设备。在向虚拟盘中copy任何数据之前，虚拟盘中是否有引导快、超级快、i节点位图、逻辑块位图、i节点、逻辑块？
//虚拟盘中没有引导快、超级快、i节点位图、逻辑块位图、i节点、逻辑块。在rd_load()函数中的memcpy(cp, bh->b_data,BLOCK_SIZE)执行以前，对虚拟盘的操作仅限于为虚拟盘分配2M的内存空间，并将虚拟盘的所有内存区域初始化为0.所以虚拟盘中并没有数据，仅是一段被’\0’填充的内存空间。
//（代码路径：kernel/blk_dev/ramdisk.c   rd_load:）
//Rd_start = (char *)mem_start;
//Rd_length = length;
//Cp = rd_start;
//For (i=0; i<length; i++)
//*cp++=’\0\;
//
//+3. 在虚拟盘被设置为根设备之前，操作系统的根设备是软盘，请说明设置软盘为根设备的技术路线。
//首先，将软盘的第一个山区设置为可引导扇区:
//（代码路径：boot/bootsect.s）				boot_flag: .word 0xAA55
//在主Makefile文件中设置ROOT_DEV=/dev/hd6。并且在bootsect.s中的508和509处设置ROOT_DEV=0x306；在tools/build中根据Makefile中的ROOT_DEV设置MAJOR_TOOT和MINOR_ROOT，并将其填充在偏移量为508和509处：
//(代码路径：Makefile)			tools/build boot/bootsect boot/setup tools/system $(ROOT_DEV) > Image
//		随后被移至0x90000+508(即0x901FC)处，最终在main.c中设置为ORIG_ROOT_DEV并将其赋给ROOT_DEV变量：
//(代码路径：init/main.c)
//62 #define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)
//113 ROOT_DEV = ORIG_ROOT_DEV;
//
//
//+4. Linux0.11是怎么将根设备从软盘更换为虚拟盘，并加载了根文件系统？
//rd_load函数从软盘读取文件系统并将其复制到虚拟盘中并通过设置ROOT_DEV为0x0101将根设备从软盘更换为虚拟盘，然后调用mount_root函数加载跟文件系统，过程如下：初始化file_table和super_block，初始化super_block并读取根i节点，然后统计空闲逻辑块数及空闲i节点数：
//(代码路径：kernel/blk_drv/ramdisk.c:rd_load)		ROOT_DEV=0x0101;
//主设备好是1，代表内存，即将内存虚拟盘设置为根目录。
//
//+5在Linux操作系统中大量的使用了中断、异常类的处理，为什么，有什么好处？
//CPU是主机中关键的组成部分，进程在主机中的运算肯定离不开CPU，而CPU在参与运算过程中免不了进行“异常处理”，这些异常处理都需要具体的服务程序来执行。这种32位中断服务体系是为适应一种被动响应中断信号而建立的。这样CPU就可以把全部精力都放在为用户程序服务上，对于随时可能产生而又不可能时时都产生的中断信号，不用刻意去考虑，这就提高了操作系统的综合效率。以“被动模式”代替“主动轮询”模式来处理终端问题是现在操作系统之所以称之为“现代”的一个重要标志。
