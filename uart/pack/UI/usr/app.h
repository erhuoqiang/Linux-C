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
		int item_pos;//��¼���λ��
		int prev_pos;//��¼ǰһ�����λ��
		int m_item_counts;//��¼��ǰ�ҳ��Ŀ��
		bool sleep_flag;//��¼�Ƿ����˯��״̬
		int recv_data;//�ײ㴫��������

		BasePage* cur_page;//��¼��ǰ�ҳ��ָ��		
		MenuItem* current_item;//��¼��ǰ����ָ��
		
		void app_exec(BasePage* page,int recv);
		void app_exit();			
		void app_init();
	
};

#endif