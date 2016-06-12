/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            main.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "proto.h"
#include "string.h"
#include "proc.h"
#include "global.h"

/*======================================================================*
                           新增队列
 *======================================================================*/

int num;	// 顾客的序号

#define MAX_QUEUE_SIZE 10	//	队列最大容量
typedef struct queue  
{    
    int front;	// 队列开头位置
    int rear;	// 队列尾部指针
    int queue_array[MAX_QUEUE_SIZE];	// 队列
      
}SqQueue;  
SqQueue S;

SqQueue Init_Queue()	// 队列初始化
{      
    SqQueue S;  
    S.front=S.rear=0;    
    return (S);  
}  
int push(SqQueue *S,int e)	// 使数据元素e进队列成为新的队尾
{    
    (*S).queue_array[(*S).rear]=e;	// e成为新的队尾  
    (*S).rear++ ;	// 队尾指针加1 
}  
int pop(SqQueue *S,int *e )	// 弹出队首元素 
{    
    *e=(*S).queue_array[(*S).front] ;    
    (*S).front++;    
}  


/*======================================================================*
                            kernel_main
 *======================================================================*/
PUBLIC int kernel_main()
{
	TASK*		p_task		= task_table;
	PROCESS*	p_proc		= proc_table;
	char*		p_task_stack	= task_stack + STACK_SIZE_TOTAL;
	u16		selector_ldt	= SELECTOR_LDT_FIRST;
	int i;

	/* 理发师问题中所使用到的信号量 */
	waiting = 0;
	customers = 0;
	barbers = 0;
	mutex = 1;
	num = 0;
	
	for (i = 0; i < NR_TASKS; i++) {
		strcpy(p_proc->p_name, p_task->name);	// name of the process
		p_proc->pid = i;			// pid

		p_proc->ldt_sel = selector_ldt;

		memcpy(&p_proc->ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3],
		       sizeof(DESCRIPTOR));
		p_proc->ldts[0].attr1 = DA_C | PRIVILEGE_TASK << 5;
		memcpy(&p_proc->ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3],
		       sizeof(DESCRIPTOR));
		p_proc->ldts[1].attr1 = DA_DRW | PRIVILEGE_TASK << 5;
		p_proc->regs.cs	= ((8 * 0) & SA_RPL_MASK & SA_TI_MASK)
			| SA_TIL | RPL_TASK;
		p_proc->regs.ds	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)
			| SA_TIL | RPL_TASK;
		p_proc->regs.es	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)
			| SA_TIL | RPL_TASK;
		p_proc->regs.fs	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)
			| SA_TIL | RPL_TASK;
		p_proc->regs.ss	= ((8 * 1) & SA_RPL_MASK & SA_TI_MASK)
			| SA_TIL | RPL_TASK;
		p_proc->regs.gs	= (SELECTOR_KERNEL_GS & SA_RPL_MASK)
			| RPL_TASK;

		p_proc->regs.eip = (u32)p_task->initial_eip;
		p_proc->regs.esp = (u32)p_task_stack;
		p_proc->regs.eflags = 0x1202; /* IF=1, IOPL=1 */

		p_task_stack -= p_task->stacksize;
		p_proc++;
		p_task++;
		selector_ldt += 1 << 3;
		proc_table[i].ticks = proc_table[i].isWait = 0;	//默认初始化为正在等待
	}


	k_reenter = 0;
	ticks = 0;

	p_proc_ready	= proc_table;

        /* 初始化 8253 PIT */
        out_byte(TIMER_MODE, RATE_GENERATOR);
        out_byte(TIMER0, (u8) (TIMER_FREQ/HZ) );
        out_byte(TIMER0, (u8) ((TIMER_FREQ/HZ) >> 8));

        put_irq_handler(CLOCK_IRQ, clock_handler); /* 设定时钟中断处理程序 */
        enable_irq(CLOCK_IRQ);                     /* 让8259A可以接收时钟中断 */

	disp_pos=0;
	for(i=0;i<80*25;i++){
		disp_str(" ");
	}
	disp_pos=0;

	S=Init_Queue();	// 初始化队列

	restart();

	while(1){}
}


void P(int index, int p)
{
	int i = sys_sem_p(index);
	if(i < 0) {
		p_proc_ready->isWait = 1;	// 置为等待
		push(&S,p);
		while(p_proc_ready->isWait){}
	}
}


void V(int index, int p)
{
	int i = sys_sem_v(index);
	if(i <= 0) {
		int t=0;
		pop(&S,&t);
		proc_table[t].isWait = 0;	// 释放等待队列的第一个进程
	}
}

/*======================================================================*
                               TestA
 *======================================================================*/
void TestA(){
	while (1) {
		sys_process_sleep(10);
	}
}

/*======================================================================*
                               TestB
 *======================================================================*/
void TestB(){	// 理发师进程
	while(1){
		P(CUSTOMERS, 1);
		P(MUTEX, 1);
		waiting--;
		V(BARBERS, 1);
		V(MUTEX, 1);
		disp_color_str("Barber cut hair\n", 1);
		my_milli_delay(2000);	// 两个时间片
	}
}

/*======================================================================*
                               TestC
 *======================================================================*/
void TestC(){	// 一把椅子的进程
	while(1){
		P(MUTEX, 2);
		sys_disp_str("Customer ");
		printNum(++num);
		sys_disp_str(" come\n");
		int i = num;
		if(waiting < CHAIRS){
			waiting++;
			V(CUSTOMERS, 2);
			V(MUTEX, 2);
			P(BARBERS, 2);
			dispstr("Customer ");
			printNum(i);
			dispstr(" cut hair\n");
			my_milli_delay(1000);
		} else {
			V(MUTEX, 2);
			sys_disp_str("Customer ");
			printNum(i);
			sys_disp_str(" have no chairs and leave\n");
			my_milli_delay(1000);
		}
	}
}

/*======================================================================*
                               TestD
 *======================================================================*/
void TestD(){	// 两把椅子的进程
	// int i = 0x3000;
	while(1){
		P(MUTEX, 3);
		sys_disp_str("Customer ");
		printNum(++num);
		sys_disp_str(" come\n");
		int i = num;
		if(waiting < CHAIRS){
			waiting++;
			V(CUSTOMERS, 3);
			V(MUTEX, 3);
			P(BARBERS, 3);
			sys_disp_str("Customer ");
			printNum(i);
			sys_disp_str(" cut hair\n");
			my_milli_delay(1000);
		} else {
			V(MUTEX, 3);
			sys_disp_str("Customer ");
			printNum(i);
			sys_disp_str(" have no chairs and leave\n");
			my_milli_delay(1000);
		}
	}
}

/*======================================================================*
                               TestE
 *======================================================================*/
void TestE(){	// 三把椅子的进程
	while(1){
		P(MUTEX, 4);
		sys_disp_str("Customer ");
		printNum(++num);
		sys_disp_str(" come\n");
		int i = num;
		if(waiting < CHAIRS){
			waiting++;
			V(CUSTOMERS, 4);
			V(MUTEX, 4);
			P(BARBERS, 4);
			sys_disp_str("Customer ");
			printNum(i);
			sys_disp_str(" cut hair\n");
			my_milli_delay(1000);
		} else {
			V(MUTEX, 4);
			sys_disp_str("Customer ");
			printNum(i);
			sys_disp_str(" have no chairs and leave\n");
			my_milli_delay(1000);
		}
	}
}

void printNum(int num){
	char str[3];
	str[0] = num / 10 + '0';
	str[1] = num % 10 + '0';
	str[2] = '\0';
	sys_disp_str(str);
}

