/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#include <signal.h>

#define _S(nr) (1<<((nr)-1))
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

void show_task(int nr,struct task_struct * p)
{
	int i,j = 4096-sizeof(struct task_struct);

	printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
	i=0;
	while (i<j && !((char *)(p+1))[i])
		i++;
	printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

void show_stat(void)
{
	int i;

	for (i=0;i<NR_TASKS;i++)
		if (task[i])
			show_task(i,task[i]);
}

#define LATCH (1193180/HZ)

extern void mem_use(void);

extern int timer_interrupt(void);
extern int system_call(void);

union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK,};

long volatile jiffies=0;
long startup_time=0;
struct task_struct *current = &(init_task.task);
struct task_struct *last_task_used_math = NULL;

struct task_struct * task[NR_TASKS] = {&(init_task.task), };

long user_stack [ PAGE_SIZE>>2 ] ;

struct {
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };
/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
void math_state_restore()
{
	if (last_task_used_math == current)
		return;
	__asm__("fwait");
	if (last_task_used_math) {
		__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
	}
	last_task_used_math=current;
	if (current->used_math) {
		__asm__("frstor %0"::"m" (current->tss.i387));
	} else {
		__asm__("fninit"::);
		current->used_math=1;
	}
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
void schedule(void)
{
	int i,next,c;
	struct task_struct ** p;

/* check alarm, wake up any interruptible tasks that have got a signal */

	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {
			if ((*p)->alarm && (*p)->alarm < jiffies) {
					(*p)->signal |= (1<<(SIGALRM-1));
					(*p)->alarm = 0;
				}
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
			(*p)->state==TASK_INTERRUPTIBLE)
				(*p)->state=TASK_RUNNING;
		}

/* this is the scheduler proper: */

	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i;
		}
		if (c) break;
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
	switch_to(next);
}

int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();
	if (tmp)
		tmp->state=0;
}

void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp=*p;
	*p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
	schedule();
	if (*p && *p != current) {
		(**p).state=0;
		goto repeat;
	}
	*p=NULL;
	if (tmp)
		tmp->state=0;
}

void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;
		*p=NULL;
	}
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 */
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
static int  mon_timer[4]={0,0,0,0};
static int moff_timer[4]={0,0,0,0};
unsigned char current_DOR = 0x0C;

int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;

	if (nr>3)
		panic("floppy_on: nr>3");
	moff_timer[nr]=10000;		/* 100 s = very big :-) */
	cli();				/* use floppy_off to turn it off */
	mask |= current_DOR;
	if (!selected) {
		mask &= 0xFC;
		mask |= nr;
	}
	if (mask != current_DOR) {
		outb(mask,FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ/2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();
	return mon_timer[nr];
}

void floppy_on(unsigned int nr)
{
	cli();
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();
}

void floppy_off(unsigned int nr)
{
	moff_timer[nr]=3*HZ;
}

void do_floppy_timer(void)
{
	int i;
	unsigned char mask = 0x10;

	for (i=0 ; i<4 ; i++,mask <<= 1) {
		if (!(mask & current_DOR))
			continue;
		if (mon_timer[i]) {
			if (!--mon_timer[i])
				wake_up(i+wait_motor);
		} else if (!moff_timer[i]) {
			current_DOR &= ~mask;
			outb(current_DOR,FD_DOR);
		} else
			moff_timer[i]--;
	}
}

#define TIME_REQUESTS 64

static struct timer_list {
	long jiffies;
	void (*fn)();
	struct timer_list * next;
} timer_list[TIME_REQUESTS], * next_timer = NULL;

void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list * p;

	if (!fn)
		return;
	cli();
	if (jiffies <= 0)
		(fn)();
	else {
		for (p = timer_list ; p < timer_list + TIME_REQUESTS ; p++)
			if (!p->fn)
				break;
		if (p >= timer_list + TIME_REQUESTS)
			panic("No more time requests free");
		p->fn = fn;
		p->jiffies = jiffies;
		p->next = next_timer;
		next_timer = p;
		while (p->next && p->next->jiffies < p->jiffies) {
			p->jiffies -= p->next->jiffies;
			fn = p->fn;
			p->fn = p->next->fn;
			p->next->fn = fn;
			jiffies = p->jiffies;
			p->jiffies = p->next->jiffies;
			p->next->jiffies = jiffies;
			p = p->next;
		}
	}
	sti();
}

void do_timer(long cpl)
{
	extern int beepcount;
	extern void sysbeepstop(void);

	if (beepcount)
		if (!--beepcount)
			sysbeepstop();

	if (cpl)
		current->utime++;
	else
		current->stime++;

	if (next_timer) {
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0) {
			void (*fn)(void);
			
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();
		}
	}
	if (current_DOR & 0xf0)
		do_floppy_timer();
	if ((--current->counter)>0) return;
	current->counter=0;
	if (!cpl) return;
	schedule();
}

int sys_alarm(long seconds)
{
	int old = current->alarm;

	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
	return (old);
}

int sys_getpid(void)
{
	return current->pid;
}

int sys_getppid(void)
{
	return current->father;
}

int sys_getuid(void)
{
	return current->uid;
}

int sys_geteuid(void)
{
	return current->euid;
}

int sys_getgid(void)
{
	return current->gid;
}

int sys_getegid(void)
{
	return current->egid;
}

int sys_nice(long increment)
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}

void sched_init(void)
{
	int i;
	struct desc_struct * p;

	if (sizeof(struct sigaction) != 16)
		panic("Struct sigaction MUST be 16 bytes");
	set_tss_desc(gdt+FIRST_TSS_ENTRY,&(init_task.task.tss));
	set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));
	p = gdt+2+FIRST_TSS_ENTRY;
	for(i=1;i<NR_TASKS;i++) {
		task[i] = NULL;
		p->a=p->b=0;
		p++;
		p->a=p->b=0;
		p++;
	}
/* Clear NT, so that we won't have troubles with that later on */
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");
	ltr(0);
	lldt(0);
	outb_p(0x36,0x43);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */
	outb(LATCH >> 8 , 0x40);	/* MSB */
	set_intr_gate(0x20,&timer_interrupt);
	outb(inb_p(0x21)&~0x01,0x21);
	set_system_gate(0x80,&system_call);
}


//11.特权级的目的和意义是什么？为什么特权级是基于段的？
//目的：在于保护高特权级的段，其中操作系统的内核处于最高的特权级。
//意义：保护模式中的特权级，对操作系统的“主奴机制”影响深远。
//在操作系统设计中，一个段一般实现的功能相对完整，可以把代码放在一个段，数据放在一个段，并通过段选择符（包括CS、SS、DS、ES、FS和GS）获取段的基址和特权级等信息。特权级基于段，这样当段选择子具有不匹 配的特权级时，按照特权级规则评判是否可以访问。特权级基于段，是结合了程序的特点和硬件 实现的一种考虑。
//
//
//12.在setup程序里曾经设置过一次gdt，为什么在head程序中将其废弃，又重新设置了一个？为什么折腾两次，而不是一次搞好？
//原来的 GDT 位于 setup 中，将来此段内存会被缓冲区覆盖， 所以必须将 GDT 设置 head.s 所在 位置。
//如果先将 GDT 设置在 head 所在区域，然后移动 system 模块，则 GDT 会被覆盖掉，如果先移 动 system 再复制 GDT，则 head.s 对应的程序会被覆盖掉，所以必须重建 GDT。
//若先移动 system 至 0x0000 再将 GDT 复制到 0x5cb8~0x64b8 处，虽可以实现， 但由于 setup.s 与 head.s 连接时不在同一文件， setup 无法直接获取 head 中的 gdt 的偏移量，需事先写入， 这会使设计失去一般性，给程序编写带来很大不便。
//
//13.在head程序执行结束的时候，在idt的前面有184个字节的head程序的剩余代码，剩余了什么？为什么要剩余？
//剩余的内容： 0x5400~0x54b7 处包含了 after_page_tables、 ignore_int 中断服务程序和 setup_paging 设 置分页的代码。
//原因： after_page_tables中压入了一些参数，为内核进入main函数的跳转做准备。为了谨慎起见，设计者在栈中压入了L6，以使得系统可能出错时，返回到L6处执行。ignore_int: 使用 ignore_int将 idt全部初始化，因此如果中断开启后，可能使用了未设置的中断向量，那么将默认跳转到 ignore_int处执行。这样做的好处是使得系统不会跳转到随机的地方执行错误的代码，所以ignore_int不能被覆盖。 setup_paging:为设置分页机制的代码，它在分页完成前不能被覆盖
//
//14.进程0的task_struct在哪？具体内容是什么？给出代码证据。
//进程0的task_struct位于内核数据区，即task结构的第0项init_task。
//struct task_struct * task[NR_TASKS] = {&(init_task.task), };
//具体内容：包含了进程0的进程状态、进程0的LDT、进程0的TSS等等。其中ldt设置了代码段和堆栈段的基址和限长(640KB)，而TSS则保存了各种寄存器的值，包括各个段选择符。具体值如下：（课本P68）
//
//15.进程0创建进程1时，为进程1建立了自己的task_struct、内核栈，第一个页表，分别位于物理内存16MB的顶端倒数第一页、第二页。请问，这个了页究竟占用的是谁的线性地址空间，内核、进程0、进程1、还是没有占用任何线性地址空间（直接从物理地址分配）？说明理由并给出代码证据。
//这两个页占用的是内核的线性地址空间，依据在setup_paging（文件head.s）中，
//movl $pg3+4092,%edi
//        movl $0xfff007,%eax  /*  16Mb -4096 + 7 (r/w user,p) */
//        std
//1: 	stosl/* fill pages backwards -more efficient :-) */
//        subl $0x1000,%eax
//        上面的代码，指明了内核的线性地址空间为0x000000 ~ 0xffffff（即前16M），且线性地址与物理地址呈现一一对应的关系。为进程1分配的这两个页，在16MB的顶端倒数第一页、第二页，因此占用内核的线性地址空间。
//进程0的线性地址空间是内存前640KB，因为进程0的LDT中的limit属性限制了进程0能够访问的地址空间。进程1拷贝了进程0的页表（160项），而这160个页表项即为内核第一个页表的前160项，指向的是物理内存前640KB，因此无法访问到16MB的顶端倒数的两个页。
//
//
//16.假设：经过一段时间的运行，操作系统中已经有5个进程在运行，且内核分别为进程4、进程5分别创建了第一个页表，这两个页表在谁的线性地址空间？用图表示这两个页表在线性地址空间和物理地址空间的映射关系。
//这两个页面均占用内核的线性空间。
//
//
//17.进程0开始创建进程1，调用了fork（），跟踪代码时我们发现，fork代码执行了两次，第一次，跳过init（）直接执行了for(;;) pause()，第二次执行fork代码后，执行了init（）。奇怪的是，我们在代码中并没有看见向后的goto语句，也没有看到循环语句，是什么原因导致反复执行？请说明理由，并给出代码证据。
//首先在copy_process（）函数中，对进程1 做个性化调整设置，调整tss的数据。
//Int copy_process（int nr, long ebp,…）
//P92
//        然后再执行到如下代码：
//＃define switch_to()……
//ljmp….
//p196
//        程序在执行到“ljmp ％0/n/t”这一行，ljmp通过cpu任务们机制自动将进程1 的tss值恢复给cpu,自然也将其中tss.eip恢复给cpu,现在cpu指向fork的if(_res >=0)这一行。
//而此时的_res值就是进程1中tss的eax的值，这个值在前面被写死为0，即p->tss.eax=0;所以直行到return (type) _res这一行返回值为0.
//Voidmain（void）
//｛
//if（！fork（））
//｛
//init（）；
//｝
//｝
//返回后，执行到main函数中if(!fork())这一行，!0值为真，调用init（）函数。
//
//18.copy_process函数的参数最后五项是：long eip,long cs,long eflags,long esp,long ss。查看栈结构确实有这五个参数，奇怪的是其他参数的压栈代码都能找得到，确找不到这五个参数的压栈代码，反汇编代码中也查不到，请解释原因。
//copy_process执行时因为进程调用了fork函数，会导致中断，中断使CPU硬件自动将SS、ESP、EFLAGS、CS、EIP这几个寄存器的值按照顺序压入 进程0内核栈，又因为函数专递参数是使用栈的，所以刚好可以做为copy_process的最后五项参数。
//
//19.为什么static inline _syscall0(type,name)中需要加上关键字inline？
//因为_syscall0(int,fork)展开是一个真函数，普通真函数调用事需要将eip入栈，返回时需要讲eip出栈。
// inline是内联函数，它将标明为inline的函数代码放在符号表中，而此处的fork函数需要调用两次，加上inline后先进行词法分析、语法分析正确后就地展开函数，不需要有普通函数的call\ret等指令，也不需要保持栈的eip，效率很高。
// 若不加上inline，第一次调用fork结束时将eip 出栈，第二次调用返回的eip出栈值将是一个错误值。
//
//20.根据代码详细说明copy_process函数的所有参数是如何形成的？
//long eip, long cs, long eflags, long esp, long ss；这五个参数是中断使CPU自动压栈的。
//long ebx, long ecx, long edx, long fs, long es, long ds为__system_call压进栈的参数。
//long none 为__system_call调用__sys_fork压进栈EIP的值。
//Int nr, long ebp, long edi, long esi, long gs,为__system_call压进栈的值。
//
//21.根据代码详细分析，进程0如何根据调度第一次切换到进程1的。（P103-107）
//进程0通过fork函数创建进程1，使其处在就绪态。
//进程0调用pause函数。pause函数通过int 0x80中断，映射到sys_pause函数，将自身设为可中断等待状态，调用schedule函数。
//schedule函数分析到当前有必要进行进程调度，第一次遍历进程，只要地址指针不为为空，就要针对处理。第二次遍历所有进程，比较进程的状态和时间骗，找出处在就绪态且counter最大的进程，此时只有进程0和1，且进程0是可中断等待状态，只有进程1是就绪态，所以切换到进程1去执行。
//
//
//22.内核的线性地址空间是如何分页的？画出从0x000000开始的7个页（包括页目录表、页表所在页）的挂接关系图，就是页目录表的前四个页目录项、第一个个页表的前7个页表项指向什么位置？给出代码证据。
//head.s再setup_paging开始创建分页机制。将页目录表和4个页表放到物理内存的起始位置，从内存起始位置开始的5个页空间内容全部清零（每页4kb），然后设置页目录表的前4项，使之分别指向4个页表。然后开始从高地址向低地址方向填写4个页表，依次指向内存从高地址向低地址方向的各个页面。即将第4个页表的最后一项（pg3+4092指向的位置）指向寻址范围的最后一个页面。即从0xFFF000开始的4kb 大小的内存空间。将第4个页表的倒数第二个页表项（pg3-4+4092）指向倒数第二个页面，即0xFFF000-0x1000开始的4KB字节的内存空间，依此类推。
//Head.s中：（P39）
// setup_paging: 
//movl $1024*5,%ecx  /* 5 pages - pg_dir+4 page tables */ 
//xorl %eax,%eax 
//xorl %edi,%edi  /* pg_dir is at 0x000 */ 
//cld;rep;stosl
// movl $pg0+7,pg_dir  /* set present bit/user r/w */  
//movl $pg1+7,pg_dir+4  /*  --------- " " --------- */ 
//movl $pg2+7,pg_dir+8  /*  --------- " " --------- */  
//movl $pg3+7,pg_dir+12  /*  --------- " " --------- */ 
//_pg_dir用于表示内核分页机制完成后的内核起始位置，也就是物理内存的起始位置0x000000，以上四句完成页目录表的前四项与页表1，2,3,4的挂接 
//movl $pg3+4092,%edi 
// movl $0xfff007,%eax  /*  16Mb - 4096 + 7 (r/w user,p) */ 
// std 
//1: stosl   /* fill pages backwards - more efficient :-) */ 
// subl $0x1000,%eax  
//jge 1b 
//完成页表项与页面的挂接，是从高地址向低地址方向完成挂接的，16M内存全部完成挂接（注意页表从0开始，页表0-页表3）
//图见P39
//
//
//23.用文字和图说明中断描述符表是如何初始化的，可以举例说明（比如：set_trap_gate(0,&divide_error)），并给出代码证据。
//对中断描述符表的初始化，就是将异常处理一类的中断服务程序与中断描述符表进行挂接。以set_trap_gate(0,&divide_error)为例，0就表示该中断函数的地址挂接在中断描述符表的第0项位置处，而&devide_error就是该异常处理函数的地址，对set_trap_gate(0,&divide_error)进行宏展开后得到
//
//%0=0x8f00，%1指向idt[0]的起始地址，%2指向四个字节之后的地址处。
//#1、将地址&devide_error放在EAX的低两个字节，EAX的高两字节不变。#2把0x8f00放入EDX的低两字节，高两字节保持不变。#3、把EAX放在%1所指的地址处，占四字节。#4、将EDX放在%2所指的地址处，占四字节。
//
//24.进程0 fork进程1之前，为什么先要调用move_to_user_mode()？用的是什么方法？解释其中的道理。（P78-79）
//因为在Linux-0.11中，除进程0之外，所有进程都是由一个已有进程在用户态下完成创建的。但是此时进程0还处于内核态，因此要调用move_to_user_mode()函数，模仿中断返回的方式，实现进程0的特权级从内核态转化为用户态。又因为在Linux-0.11中，转换特权级时采用中断和中断返回的方式，调用系统中断实现从3到0的特权级转换，中断返回时转换为3特权级。因此，进程0从0特权级到3特权级转换时采用的是模仿中断返回。
//
//
//25.进程0创建进程1时调用copy_process函数，在其中直接、间接调用了两次get_free_page函数，在物理内存中获得了两个页，分别用作什么？是怎么设置的？给出代码证据。（P89 91 92P97-98 ）
//第一个设置进程1的tasks_truct，另外设置了进程1的堆栈段
//        sched.c
//struct task_struct *current = &(init_task.task);
//fork.c
//        p = (struct task_struct *) get_free_page();
//…
//*p = *current;
//p->tss.esp0 = PAGE_SIZE + (long) p;
//其中*current 即为进程0的task结构，在copy_process中，先复制进程0的task_struct，然
//        后再对其中的值进行修改。esp0 的设置，意味着设置该页末尾为进程1的堆栈的起始地址。
//第二个为进程1的页表，在创建进程1执行copy_process中，执行copy_mem(nr,p)时，内核
//        为进程 1 拷贝了进程 0 的页表（ 160 项）。
//copy_mem
//…
//if (copy_page_tables(old_data_base,new_data_base,data_limit)){
//…
//其中， copy_page_tables 内部
//…
//from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
//if (!(to_page_table = (unsigned long *) get_free_page()))
//…
//for ( ; nr-- > 0 ; from_page_table++,to_page_table++) {
//… }
//获取了新的页，且从 from_page_table 将页表值拷贝到 to_page_table 处。
//
//26.在IA-32中，有大约20多个指令是只能在0特权级下使用，其他的指令，比如cli，并没有这个约定。奇怪的是，在Linux0.11中，在3特权级的进程代码并不能使用cli指令，会报特权级错误，这是为什么？请解释并给出代码证据。
//根据Intel Manual，cli和sti指令与CPL和EFLAGS[IOPL]有关。
//CLI：如果 CPL 的权限高于等于eflags中的IOPL的权限，即数值上：cpl <= IOPL，则IF 位清
//除为0；否则它不受影响。EFLAGS 寄存器中的其他标志不受影响。
//#GP(0) –如果CPL大于（特权更小）当前程序或过程的 IOPL，产生保护模式异常。
//由于在内核IOPL的值初始时为0，且未经改变。进程0在move_to_user_mode中，继承了内核的eflags。
//move_to_user_mode()
//…
//"pushfl\n\t" \
//…
//"iret\n" \
//而进程1再copy_process中，在进程的TSS中，设置了eflags中的IOPL位为0。总之，通过
//        设置 IOPL，可以限制 3 特权级的进程代码使用 cli。
//
//27.根据代码详细分析操作系统是如何获得一个空闲页的。（P89-90）
//通过逆向扫描页表位图mem_map，并由第一空页的下标左移12位加LOW_MEM得到该页的物理地址， 位于 16M 内存末端。 代码如下（ get_free_page）
//unsigned long get_free_page(void)
//64 {
//65 register unsigned long __res asm("ax");
//66
//67 __asm__("std ; repne ; scasb\n\t" //反向扫描串,al(0)与di不等则重复
//        68 "jne 1f\n\t"  //找不到空闲页跳转1
//69 "movb $1,1(%%edi)\n\t"  //将1付给edi+1的位置，在mem_map中将找到0的项引用计                                     ------------------------------------------数置为1
//70 "sall $12,%%ecx\n\t"   //ecx算数左移12位，页的相对地址
//71 "addl %2,%%ecx\n\t"    //LOW MEN +ecx页物理地址
//72 "movl %%ecx,%%edx\n\t"
//73 "movl $1024,%%ecx\n\t"
//74 "leal 4092(%%edx),%%edi\n\t"     //将edx+4kb的有效地址赋给edi
//75 "rep ; stosl\n\t"        //将eax赋给edi指向的地址，目的是页面清零。
//76 " movl %%edx,%%eax\n"
//77 "1: cld"
//78 :"=a" (__res)
//79 :"0" (0),"i" (LOW_MEM),"c" (PAGING_PAGES),
//80 "D" (mem_map+PAGING_PAGES-1)           //edx,mem_map[]的嘴鸥一个元素
//81 );
//82 return __res;
//83 }
//
//28、用户进程自己设计一套LDT表，并与GDT挂接，是否可行，为什么？（P259）
//不可行。首先，用户进程不可以设置GDT、LDT，因为Linux0.11将GDT、LDT这两个数据结构设置在内核数据区，是0特权级的，只有0特权级的额代码才能修改设置GDT、LDT；而且，用户也不可以在自己的数据段按照自己的意愿重新做一套GDT、LDT，如果仅仅是形式上做一套和GDT、LDT一样的数据结构是可以的，但是真正起作用的GDT、LDT是CPU硬件认定的，这两个数据结构的首地址必须挂载在CPU中的GDTR、LDTR上，运行时CPU只认GDTR和LDTR指向的数据结构，其他数据结构就算起名字叫GDT、LDT，CPU也一概不认；另外，用户进程也不能将自己制作的GDT、LDT挂接到GDRT、LDRT上，因为对GDTR和LDTR的设置只能在0特权级别下执行,3特权级别下无法把这套结构挂接在CR3上。
//
//29、保护模式下，线性地址到物理地址的转化过程是什么？（P260-261）
//在保护模式下，先行地址到物理地址的转化是通过内存分页管理机制实现的。其基本原理是将整个线性和物理内存区域划分为4K大小的内存页面，系统以页为单位进行分配和回收。每个线性地址为32位，MMU按照10-10-12的长度来识别线性地址的值。CR3中存储着页目录表的基址，线性地址的前十位表示也目录表中的页目录项，由此得到所在的页表地址。21~12位记录了页表中的页表项位置，由此得到页的位置，最后12位表示页内偏移。
//
//30、为什么get_free_page（）将新分配的页面清0？（P265）
//因为无法预知这页内存的用途，如果用作页表，不清零就有垃圾值，就是隐患。
//
//31、内核和普通用户进程并不在一个线性地址空间内，为什么仍然能够访问普通用户进程的页面？（P272）
//内核的线性地址空间和用户进程不一样，内核是不能通过跨越线性地址访问进程的，但由于早就占有了所有的页面，而且特权级是0，所以内核执行时，可以对所有的内容进行改动，“等价于”可以操作所有进程所在的页面。
//32、详细分析一个进程从创建、加载程序、执行、退出的全过程。
//1.	创建进程，调用fork函数。
//a)	准备阶段，为进程在task[64]找到空闲位置，即find_empty_process（）；
//b)	为进程管理结构找到储存空间：task_struct和内核栈。
//c)	父进程为子进程复制task_struct结构
//        d)	复制新进程的页表并设置其对应的页目录项
//        e)	分段和分页以及文件继承。
//f)	建立新进程与全局描述符表（GDT）的关联
//        g)	将新进程设为就绪态
//2.	加载进程
//        a)	检查参数和外部环境变量和可执行文件
//        b)	释放进程的页表
//        c)	重新设置进程的程序代码段和数据段
//        d)	调整进程的task_struct
//3.	进程运行
//        a)	产生缺页中断并由操作系统响应
//        b)	为进程申请一个内存页面
//        c)	将程序代码加载到新分配的页面中
//        d)	将物理内存地址与线性地址空间对应起来
//        e)	不断通过缺页中断加载进程的全部内容
//        f)	运行时如果进程内存不足继续产生缺页中断，
//4.	进程退出
//        a)	进程先处理退出事务
//        b)	释放进程所占页面
//        c)	解除进程与文件有关的内容并给父进程发信号
//        d)	进程退出后执行进程调度
//
//33、详细分析多个进程（无父子关系）共享一个可执行程序的完整过程。
//假设有三个进程A、B、C，进程A先执行，之后是B最后是C，它们没有父子关系。A进程启动后会调用open函数打开该可执行文件，然后调用sys_read()函数读取文件内容,该函数最终会调用bread函数，该函数会分配缓冲块，进行设备到缓冲块的数据交换，因为此时为设备读入，时间较长，所以会给该缓冲块加锁，调用sleep_on函数，A进程被挂起，调用schedule()函数B进程开始执行。
//B进程也首先执行open（）函数，虽然A和B打开的是相同的文件，但是彼此操作没有关系，所以B继承需要另外一套文件管理信息，通过open_namei()函数。B进程调用read函数，同样会调用bread（），由于此时内核检测到B进程需要读的数据已经进入缓冲区中，则直接返回，但是由于此时设备读没有完成，缓冲块以备加锁，所以B将因为等待而被系统挂起，之后调用schedule()函数。
//C进程开始执行，但是同B一样，被系统挂起，调用schedule()函数，假设此时无其它进程，则系统0进程开始执行。
//假设此时读操作完成，外设产生中断，中断服务程序开始工作。它给读取的文件缓冲区解锁并调用wake_up()函数，传递的参数是&bh->b_wait,该函数首先将C唤醒，此后中断服务程序结束，开始进程调度，此时C就绪，C程序开始执行，首先将B进程设为就绪态。C执行结束或者C的时间片削减为0时，切换到B进程执行。进程B也在sleep_on()函数中，调用schedule函数进程进程切换，B最终回到sleep_on函数，进程B开始执行，首先将进程A设为就绪态，同理当B执行完或者时间片削减为0时，切换到A执行，此时A的内核栈中tmp对应NULL，不会再唤醒进程了。
//
//
//34、缺页中断是如何产生的，页写保护中断是如何产生的，操作系统是如何处理的？
//每一个页目录项或页表项的最后3位，标志着所管理的页面的属性，分别是U/S,R/W,P.如果和一个页面建立了映射关系，P标志就设置为1，如果没有建立映射关系，则就是0.进程执行时，线性地址被MMU即系，如果解析出某个表项的P位为0，就说明没有对应页面，此时就会产生缺页中断。
//当两个进程共享了一个页面，即R/w为1，导致该页面设为“只读”属性，当其中一个进程需要压栈时就会引发页写保护中断。当页写保护中断产生时，系统会为进程申请新页面，并把原页面内容复制到新页面里。
//
//35、为什么要设计缓冲区，有什么好处？（P310）
//缓冲区是内存与外设（块设备，如硬盘等）进行数据交互的媒介。内存与外设最大的区别在于：外设（如硬盘）的作用仅仅就是对数据信息以逻辑块的形式进行断电保存，并不参与运算（因为CPU无法到硬盘上进行寻址）；而内存除了需要对数据进行保存以外，还要通过与CPU和总线的配合，进行数据运算（有代码和数据之分）；缓冲区则介于两者之间，有了缓冲区这个媒介以后，对外设而言，它仅需要考虑与缓冲区进行数据交互是否符合要求，而不需要考虑内存中内核、进程如何使用这些数据；对内存的内核、进程而言，它也仅需要考虑与缓冲区交互的条件是否成熟，而并不需要关心此时外设对缓冲区的交互情况。它们两者的组织、管理和协调将由操作系统统一操作，这样就大大降低了数据处理的维护成本。
//缓冲区的好处主要有两点：①形成所有块设备数据的统一集散地，操作系统的设计更方便、更灵活；②对块设备的文件操作运行效率更高。
//
//36、操作系统如何利用buffer_head中的 b_data，b_blocknr，b_dev，b_uptodate，b_dirt，b_count，b_lock，b_wait管理缓冲块的？
//buffer_head负责进程与缓冲块的数据交互，让数据在缓冲区中停留的时间尽可能长。
//b_data 是缓冲块的数据内容。
//b_dev和b_blocknr两个字段把缓冲块和硬盘数据块的关系绑定，同时根据b_count决定是否废除旧缓冲块而新建缓冲块以保证数据在缓冲区停留时间尽量长。
//b_dev为设备标示，b_blocknr标示block块好。b_count用于记录缓冲块被多少个进程共享了。
//b_uptodate和b_dirt用以保证缓冲块和数据块的正确性。b_uptodate为1说明缓冲块的数据就是数据块中最新的，进程可以共享缓冲块中的数据。b_dirt为1时说明缓冲块数据已被进程修改，需要同步到硬盘上。
//b_lock为1时说明缓冲块与数据块在同步数据，此时内核会拦截进程对该缓冲块的操作，直到交互结束才置0。b_wait用于记录因为b_lock=1而挂起等待缓冲块的进程数。
//

