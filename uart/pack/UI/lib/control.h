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
//事件基类
class Event
{
	public:
		virtual void control(MenuItem* m_item,int cmd){};
//		virtual ~Event() {};
		
		int command;
			
};

//左键值事件处理类
class LeftKeyEvent:public Event
{
	public:
		
		MenuItem* m_item;//菜单项类的指针
		
		void control(MenuItem* m_item,int cmd);
};

//右键值事件处理类
class RightKeyEvent:public Event
{
	public:
		
		MenuItem* m_item;//菜单项类的指针
		
		void control(MenuItem* m_item,int cmd);
};

//上键值事件处理类
class UpKeyEvent:public Event
{
		public:
		
		MenuItem* m_item;//菜单项类的指针
		
		void control(MenuItem* m_item,int cmd);
};

//下键值事件处理类
class DownKeyEvent:public Event
{
		public:
		
		MenuItem* m_item;//菜单项类的指针
		
		void control(MenuItem* m_item,int cmd);	
};
//返回键值事件处理类
class BackKeyEvent:public Event
{
		public:
		
		MenuItem* m_item;//菜单项类的指针
		
		void control(MenuItem* m_item,int cmd);	
};

//菜单键值事件处理类
class MenuKeyEvent:public Event
{
		public:
		
		MenuItem* m_item;//菜单项类的指针
		
		void control(MenuItem* m_item,int cmd);	
};


#endif