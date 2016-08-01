#ifndef __APP_H
#define __APP_H

#include <iostream>

using namespace std;

class MenuItem;
class BasePage;

class APP
{
public:
		APP();
private:
		int item_pos;//记录光标位置
		int prev_pos;//记录前一个光标位置
		int m_item_counts;//记录当前活动页条目数
		bool sleep_flag;//记录是否进入睡眠状态
		int recv_data;//底层传来的数据

		BasePage* cur_page;//记录当前活动页的指针		
		MenuItem* current_item;//记录当前活动项的指针
		
		void app_exec(BasePage* page,int recv);
		void app_exit();			
		void app_init();
	
};

#endif