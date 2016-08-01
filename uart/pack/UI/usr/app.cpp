#include "app.h"
#include "ui.h"
#include "control.h"
#include <curses.h>
#include <iostream>

//初始化窗口
APP::APP()
{
	  //初始化窗口管理参数
		int item_pos = 1;
		int prev_pos = 0;
		int m_item_counts = 0;
		bool sleep_flag = false;
		int recv_data = 0;
		BasePage* cur_page = NULL;
		BasePage* back_page = NULL;
		MenuItem* current_item = NULL;	
		
		//初始化curses库相关
		initscr();
    keypad(stdscr,true);
    clear();	
	
}

void APP::app_init()
{
			
	
}

void APP::app_exec(BasePage* page,int recv)
{
	
		
			
	
}
