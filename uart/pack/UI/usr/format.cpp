#include "ui.h"
#include "control.h"
#include "format.h"
#include "exit.h"
#include "user.h"
#include <stdio.h>
#include <curses.h>

int item_pos = 1;//current position
int prev_pos = 0;//previous positon
int m_item_counts = 0;//number of items in current page
bool sleep_flag = false;
int recv_data = 0;
int delete_flag = 0;

BasePage* cur_page = NULL;
MenuItem* current_item = NULL;

#define KEY_ENTER_MY  13

void screen_init()
{
    initscr();
    cbreak();
    nonl();
    noecho();
    intrflush(stdscr,FALSE);
    keypad(stdscr,true);
    clear();
}

void UpAndDown(int direction)
{
		
		if(direction==1)
		{
				mvaddch(item_pos,0,' ');//clear previous symbol '*'
				if(item_pos == m_item_counts)
				    item_pos = 0;
				item_pos += 1;//record current item's index
				mvaddch(item_pos,0,'*');//draw symbol * on new position		
			
		}else if(direction==0)
		{
        mvaddch(item_pos,0,' ');//clear previous symbol '*'
        if(item_pos == 1)
            item_pos = m_item_counts+1;
        item_pos -= 1;
        mvaddch(item_pos,0,'*');			
		
		}
		 current_item = cur_page->m_items[item_pos-1];	
		 current_item->ChangeState(-1);	
}

void rightchecktype()
{    
    current_item->ChangeState(1);
}

void leftchecktype()
{ 
	if(edit_flag==1)
	{
		delete_flag = 1;
		current_item->ChangeState(-2);	
			
	}else
	{
    current_item->ChangeState(0);
   }
}

void exit_mainmenu()
{
    clear();
    sleep_flag = true;
}

void return_menu()
{
    if(cur_page->prevpage)
    {
        item_pos = prev_pos;
        cur_page = cur_page->prevpage;
        current_item = cur_page->m_items[item_pos-1];	
        m_item_counts = cur_page->item_count;
        cur_page->ShowPage(cur_page);
        mvaddch(item_pos,0,'*');

    }
    else
    {
        exit_mainmenu();

    }
}

void init_startpage()
{
    item_pos = 1;
    cur_page = page_list[0];
//    current_item = cur_page->m_items[0];
    m_item_counts = cur_page->item_count;
    cur_page->ShowPage(cur_page);
    mvaddch(item_pos,0,'*');
    move(LINES-1,0);
    refresh();
}

void delelte()
{
	
	
}

void recvprocess(int recv)
{
		current_item = cur_page->m_items[item_pos-1];	
		if(edit_flag==0)
		{
		    if((sleep_flag==true)&&(recv!=RECV_MENU))
		    {
		        return ;
		    }
		     
				switch(recv)
				{
				case RECV_DOWN:
				case KEY_DOWN:
						UpAndDown(1);							
				    break;
				case RECV_UP:
				case KEY_UP:
						UpAndDown(0);	
				    break;
				case RECV_LEFT:
				case KEY_LEFT:
				    leftchecktype();
				    break;
				case RECV_RIGHT:
				case KEY_RIGHT:
				    rightchecktype();
				    break;
				case RECV_BACK:
				    return_menu();
				    break;
				case RECV_MENU:
				    if(sleep_flag==true)
				    {
				        init_startpage();
				        sleep_flag = false;
				    }
				    break;
				case KEY_ENTER_MY:
						current_item->ChangeState(-2);
				default:
				    break;
				}
		}else
		{
				switch(recv)
				{
						case RECV_LEFT:
						case KEY_LEFT:
						    leftchecktype();
						    goto stop;
						    break;
						case KEY_ENTER_MY:
								edit_flag = 0;
								goto stop;
								break;
						default:
						    break;
				}	
				current_item->ChangeState(-2);		
		}
stop:		
		move(LINES-1,0);
    refresh();
}

//释放对象指针
void destroy()
{
	
		
}
