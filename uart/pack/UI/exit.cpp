#include "exit.h"
#include "format.h"
#include <sstream>

int edit_flag = 0;

using namespace std;

ExitItem::ExitItem(int ID)
{
		pos_x = ID;
	
}

void ExitItem::ShowItem()
{
		stringstream stream;
		string num;
		
		stream << pos_x;
		stream >> num;
		
		mvaddstr(pos_x,POS_Y,('0'+num).c_str());
	  mvaddstr(pos_x,POS_Y+5,"EXIT");
    mvaddstr(pos_x,POS_Y+30,"     ");	
}

void ExitItem::ChangeState(int flag)
{
		if(flag==-1)
		{
				exit_mainmenu();//退出主菜单			
		}	
}

EditItem::EditItem(int ID,string head)
{
	
		pos_x = ID;
		text_head = head;
		text_tail = " ";	
}

void EditItem::ShowItem()
{
		stringstream stream;
		string num;
		
		stream << pos_x;
		stream >> num;
		
		mvaddstr(pos_x,POS_Y,('0'+num).c_str());
	  mvaddstr(pos_x,POS_Y+5,text_head.c_str());
    mvaddstr(pos_x,POS_Y+29,text_tail.c_str());	
}

void EditItem::ChangeState(int flag)
{
		char buf[2];
		int len;
		
		len = text_tail.size();
						
		if(flag==-2)
		{
				if((delete_flag==1)&&(len>1))
				{
						text_tail.erase(text_tail.end()-1);
						mvprintw(pos_x,POS_Y+29,"                     "); /* 将字符串string打印在坐标(y,x)处*/ 
						mvprintw(pos_x,POS_Y+29,text_tail.c_str()); /* 将字符串string打印在坐标(y,x)处*/	
						delete_flag = 0;
					
				}else
				{
						edit_flag = 1;
						buf[0]= (char)recv_data;
						buf[1]= '\0';
	
						if((buf[0]>='0'&&buf[0]<='9')||(buf[0]>='a'&&buf[0]<='z')||(buf[0]>='A'&&buf[0]<='Z'))
						{
							if(len<9)
							{
									text_tail += buf;
									mvprintw(pos_x,POS_Y+29,text_tail.c_str()); /* 将字符串string打印在坐标(y,x)处*/
							}
						}		
			}
		}
}