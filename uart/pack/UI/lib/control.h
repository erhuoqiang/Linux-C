#ifndef __CONTROL_H
#define __CONTROL_H

#include <curses.h>
#include "ui.h"

#define RECV_UP    65
#define RECV_DOWN  66
#define RECV_LEFT  68
#define RECV_RIGHT 67
#define RECV_BACK  32
#define RECV_MENU  109

/*enum MyCommand
{
		RECV_UP = 0,
		KEY_DOWN = 0,
		KEY_DOWN = 1,
		RECV_DOWN = 1,
		RECV_LEFT = 2,
		KEY_LEFT = 2,
		RECV_RIGHT = 3,
		KEY_RIGHT =3;
		RECV_BACK,
		RECV_MENU
};
*/
//�¼�����
class Event
{
	public:
		virtual void control(MenuItem* m_item,int cmd){};
//		virtual ~Event() {};
		
		int command;
			
};

//���ֵ�¼�������
class LeftKeyEvent:public Event
{
	public:
		
		MenuItem* m_item;//�˵������ָ��
		
		void control(MenuItem* m_item,int cmd);
};

//�Ҽ�ֵ�¼�������
class RightKeyEvent:public Event
{
	public:
		
		MenuItem* m_item;//�˵������ָ��
		
		void control(MenuItem* m_item,int cmd);
};

//�ϼ�ֵ�¼�������
class UpKeyEvent:public Event
{
		public:
		
		MenuItem* m_item;//�˵������ָ��
		
		void control(MenuItem* m_item,int cmd);
};

//�¼�ֵ�¼�������
class DownKeyEvent:public Event
{
		public:
		
		MenuItem* m_item;//�˵������ָ��
		
		void control(MenuItem* m_item,int cmd);	
};
//���ؼ�ֵ�¼�������
class BackKeyEvent:public Event
{
		public:
		
		MenuItem* m_item;//�˵������ָ��
		
		void control(MenuItem* m_item,int cmd);	
};

//�˵���ֵ�¼�������
class MenuKeyEvent:public Event
{
		public:
		
		MenuItem* m_item;//�˵������ָ��
		
		void control(MenuItem* m_item,int cmd);	
};


#endif