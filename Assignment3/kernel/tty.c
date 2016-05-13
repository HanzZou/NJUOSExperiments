
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               tty.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

#define TTY_FIRST	(tty_table)
#define TTY_END		(tty_table + NR_CONSOLES)

PUBLIC void init_tty(TTY* p_tty);
PRIVATE void tty_do_read(TTY* p_tty);
PRIVATE void tty_do_write(TTY* p_tty);
PRIVATE void put_key(TTY* p_tty, u32 key);
PRIVATE void do_the_search(TTY* p_tty);
PRIVATE void leave_search_mode(TTY* p_tty);

PUBLIC int is_in_search_mode;	// 处于查询模式的输入状态下
PUBLIC int is_show_result;		// 处于查询模式的显示结果状态下
PUBLIC int is_col;	//以不同颜色输出

unsigned int text_end;	// 正文结束的地址

/*======================================================================*
                           task_tty
 *======================================================================*/
PUBLIC void task_tty()
{
	TTY*	p_tty;

	init_keyboard();

	for (p_tty=TTY_FIRST;p_tty<TTY_END;p_tty++) {
		init_tty(p_tty);
	}
	select_console(0);
	while (1) {
		for (p_tty=TTY_FIRST;p_tty<TTY_END;p_tty++) {
			tty_do_read(p_tty);
			tty_do_write(p_tty);
		}
	}
}

/*======================================================================*
			   init_tty
 *======================================================================*/
PUBLIC void init_tty(TTY* p_tty)
{
	p_tty->inbuf_count = 0;
	p_tty->p_inbuf_head = p_tty->p_inbuf_tail = p_tty->in_buf;
	is_in_search_mode = 0;
	is_show_result = 0;
	is_col = 0;

	init_screen(p_tty);
}

/*======================================================================*
				in_process
 *======================================================================*/
PUBLIC void in_process(TTY* p_tty, u32 key)
{
        char output[2] = {'\0', '\0'};

        if (!(key & FLAG_EXT)) {
        	if(is_show_result)
        		return;
			put_key(p_tty, key);
        }else {
            int raw_code = key & MASK_RAW;
			int i;
			if(is_show_result && raw_code != ESC) {
				return;
			}
            switch(raw_code) {
           		case ENTER:
           			if(!is_in_search_mode) {
						put_key(p_tty, '\n');
					} else if(is_in_search_mode && !is_show_result) {
						do_the_search(p_tty);
						is_show_result = 1;
					}
					break;
            	case BACKSPACE:
					put_key(p_tty, '\b');
					break;
                case UP:
                    if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
						scroll_screen(p_tty->p_console, SCR_DN);
                    }
					break;
				case DOWN:
					if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
						scroll_screen(p_tty->p_console, SCR_UP);
					}
					break;
				case TAB:
					put_key(p_tty, '\t');
					break;
				case F1:
				case F2:
				case F3:
				case F4:
				case F5:
				case F6:
				case F7:
				case F8:
				case F9:
				case F10:
				case F11:
				case F12:
					/* Alt + F1~F12 */
					if ((key & FLAG_ALT_L) || (key & FLAG_ALT_R)) {
						select_console(raw_code - F1);
					}
					break;
				case ESC:
					if(is_in_search_mode && is_show_result) {
						is_in_search_mode = 0;
						is_show_result = 0;
						is_col = 0;
						leave_search_mode(p_tty);
					} else if(!is_in_search_mode) {
						is_in_search_mode = 1;
						is_col = 1;
						text_end = p_tty->p_console->cursor;
					}
		        	break;
		        default:
		            break;
            }
        }

}

/*======================================================================*
			      put_key
*======================================================================*/
PRIVATE void put_key(TTY* p_tty, u32 key)
{
	if (p_tty->inbuf_count < TTY_IN_BYTES) {
		*(p_tty->p_inbuf_head) = key;
		p_tty->p_inbuf_head++;
		if (p_tty->p_inbuf_head == p_tty->in_buf + TTY_IN_BYTES) {
			p_tty->p_inbuf_head = p_tty->in_buf;
		}
		p_tty->inbuf_count++;
	}
}


/*======================================================================*
			      tty_do_read
 *======================================================================*/
PRIVATE void tty_do_read(TTY* p_tty)
{
	if (is_current_console(p_tty->p_console)) {
		keyboard_read(p_tty);
	}
}


/*======================================================================*
			      tty_do_write
 *======================================================================*/
PRIVATE void tty_do_write(TTY* p_tty)
{
	if (p_tty->inbuf_count) {
		char ch = *(p_tty->p_inbuf_tail);
		p_tty->p_inbuf_tail++;
		if (p_tty->p_inbuf_tail == p_tty->in_buf + TTY_IN_BYTES) {
			p_tty->p_inbuf_tail = p_tty->in_buf;
		}
		p_tty->inbuf_count--;

		out_char(p_tty->p_console, ch);
	}
}

// 进行查找
PRIVATE void do_the_search(TTY* p_tty) {
	int len = p_tty->p_console->cursor - text_end;	// 查找信息的长度
	p_tty->p_console->cursor = p_tty->p_console->original_addr;
	int i;
	u8* text = (u8*)(V_MEM_BASE + p_tty->p_console->original_addr * 2);

	is_col = 0;
	if(text_end - p_tty->p_console->original_addr < len) {
		return;
	}
	for (i = 0; i <= text_end - p_tty->p_console->original_addr - len; i++) {
		p_tty->p_console->cursor = p_tty->p_console->original_addr + i;
		int j;
		int cmp = 1;
		for (j = 0; j < len; j++) {
			if (*(text + i * 2 + j * 2) != *(text + (text_end - p_tty->p_console->original_addr + j) * 2))
			{
				cmp = 0;
				break;
			}
		}
		if (cmp) {
			is_col = 1;
			for (j = 0; j < len; j++) {
				out_char(p_tty->p_console, *(text + i * 2 + j * 2));		
			}
			i += len;
			is_col = 0;
		} else {
			out_char(p_tty->p_console, *(text + i * 2));
		}
	}

	int start = text_end - p_tty->p_console->cursor;
	for (i = 0; i < start; ++i)
	{
		out_char(p_tty->p_console, *(text + (p_tty->p_console->cursor - p_tty->p_console->original_addr) * 2));
	}

	is_col = 1;
	for (i = 0; i < len; ++i)
	{
		out_char(p_tty->p_console, *(text + (p_tty->p_console->cursor - p_tty->p_console->original_addr) * 2));
	}
	is_col = 0;

}

// 恢复原模式
PRIVATE void leave_search_mode(TTY* p_tty) {
	int len = text_end - p_tty->p_console->original_addr;
	int len2 = p_tty->p_console->cursor - text_end;
	int i;
	p_tty->p_console->cursor = p_tty->p_console->original_addr;

	u8* text = (u8*)(V_MEM_BASE + p_tty->p_console->original_addr * 2);

	for (i = 0; i < len; i++)
	{
		out_char(p_tty->p_console, *(text + 2 * i));
	}
	for (i = 0; i < len2; i++)
	{
		out_char(p_tty->p_console, ' ');
	}
	p_tty->p_console->cursor = text_end;
	flush(p_tty->p_console);
	is_col = 0;
}