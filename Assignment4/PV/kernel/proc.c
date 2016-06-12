
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               proc.c
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
                              schedule
 *======================================================================*/
PUBLIC void schedule()
{
	PROCESS* p;

	for (p = proc_table; p < proc_table+NR_TASKS; p++) {	// 遍历进程表
		if(p->isWait){	// 进程正在等待
			continue;	// 下一个进程
		}
		if(p->ticks){	// 进程的剩余时间不为0
			p->ticks--;	// 时间减少
		}
	}

	p_proc_ready++;	// 寻找下一个执行进程
	while(1){
		if(p_proc_ready->isWait){	// 进程正在等待
			p_proc_ready++;
		}
		if (p_proc_ready >= proc_table + NR_TASKS) {	// 循环遍历
			p_proc_ready = proc_table;
		}
		if(p_proc_ready->ticks){	// 还剩下时间片
			p_proc_ready++;
		} else {
			break;	// 找到一个时间消耗完的进程，退出循环
		}
	}

}



PUBLIC int sys_get_ticks(){
	return ticks;
}



PUBLIC void sys_process_sleep(int i){
	p_proc_ready->ticks = i*HZ/1000;
}



PUBLIC void sys_disp_str(char* str){
	disp_str(str);
}



PUBLIC int sys_sem_p(int index){
	if(index == MUTEX){
		return --mutex;
	} else if(index == CUSTOMERS){
		return --customers;
	} else if(index == BARBERS){
		return --barbers;
	} else{
		return -1;
	}
}



PUBLIC int sys_sem_v(int index){
	if(index == MUTEX){
		return ++mutex;
	} else if(index == CUSTOMERS){
		return ++customers;
	} else if(index == BARBERS){
		return ++barbers;
	} else{
		return -1;
	}
}
