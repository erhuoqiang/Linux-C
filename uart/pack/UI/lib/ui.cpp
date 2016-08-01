#include "ui.h"
#include "format.h"
#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>
#include <curses.h>
#include <sstream>

//开关型ITEM
SwitchingItem::SwitchingItem(int ID,string head,bool state)
{
		pos_x = ID;
    switching_state = state;

    text_head = head;
    if(switching_state==true)
        text_tail = "TRUE";
    else
        text_tail = "FALSE";
}

void SwitchingItem::ShowItem()
{
		stringstream stream;
		string num;
		
		stream << pos_x;
		stream >> num;
		
		mvaddstr(pos_x,POS_Y,('0'+num).c_str());
	  mvaddstr(pos_x,POS_Y+5,text_head.c_str());
    mvaddstr(pos_x,POS_Y+30,text_tail.c_str());
	
}

void SwitchingItem::ChangeState(int flag)
{
	if(!(flag < 0))
	{
	  if(switching_state==true)
		{
	   switching_state = false;
	    text_tail = "FALSE";
		}
		else
		{
	    switching_state = true;
	    text_tail = "TRUE ";
		}	
		mvaddstr(pos_x,POS_Y+30,text_tail.c_str());	
	}	
}


//数值型ITEM
NumericalItem::NumericalItem(int ID,string head,int setvalue,int minnum,int maxnum)
{
    stringstream stream;

		pos_x = ID;
    set_num = setvalue;
    max_num = maxnum;
    min_num = minnum;

    stream << set_num;
    stream >> text_tail;
    text_head = head;
   
}

void NumericalItem::ShowItem()
{	
		stringstream stream;
		string num;
		
		stream << pos_x;
		stream >> num;
		
		mvaddstr(pos_x,POS_Y,('0'+num).c_str());
	  mvaddstr(pos_x,POS_Y+5,text_head.c_str());
    mvaddstr(pos_x,POS_Y+30,text_tail.c_str());
	
}

void NumericalItem::ChangeState(int flag)
{
    stringstream stream;
    if(flag==1)
  	{
		    if(set_num < max_num)
			 	 {
			      set_num += 1;
			      stream << set_num;
			      stream >>text_tail;
			  }		
  	}else if(flag == 0)
  		{
          if(set_num > min_num)
          {
              set_num -= 1;
              stream << set_num;
              stream >> text_tail;

          }  			
  		}
  		mvaddstr(pos_x,POS_Y+30,"     ");
      mvaddstr(pos_x,POS_Y+30,text_tail.c_str());	
		
}


//进入页面型
EnterItem::EnterItem(int ID,string head,BasePage* next_page,BasePage* prev_page)
{
		pos_x = ID;
		
		nextpage = next_page;
    prevpage = prev_page;		
			
    text_tail = ">>>>";
    text_head = head;	
	
}

void EnterItem::ShowItem()
{
	
		stringstream stream;
		string num;
		
		stream << pos_x;
		stream >> num;
		
		mvaddstr(pos_x,POS_Y,('0'+num).c_str());
	  mvaddstr(pos_x,POS_Y+5,text_head.c_str());
    mvaddstr(pos_x,POS_Y+30,text_tail.c_str());
	
}

void EnterItem::ChangeState(int flag)
{
		if(flag == 1)
		{
		    prev_pos = item_pos;
		    item_pos = 1;
		    if(nextpage!=NULL)
		    {
		    		nextpage->prevpage = cur_page;
		        cur_page = nextpage;
		        m_item_counts = cur_page->item_count;
		    }
		    cur_page->ShowPage(cur_page);
		    mvaddch(item_pos,0,'*');	
		    
		}
}

/***basepage construct func***/
BasePage::BasePage(string str)
{
    page_name = str;
    prevpage = NULL;
    item_count = 0;
    page_index = 0;
}

void BasePage::AddItem(MenuItem* item)
{
    if(item!=NULL)
    {
        m_items[item_count] = item;//add one item to vector
//        item->pos_x = item_count+1;
        item_count = item_count + 1;
    }
    //要添加错误处理，指针为空时
}

/***get current page's real numbers of items***/
int BasePage::GetItemCount()
{
    int current_count;
    current_count = item_count;
    return current_count;
}

void BasePage::PageSetIndex(int index)
{
    page_index = index;
}

void BasePage::ShowPage(BasePage* page)
{

    int j;
    clear();

    mvaddstr(0,15,(page->page_name).c_str());

    for(j=0;j<page->item_count;j++)
    {
        page->m_items[j]->ShowItem();
    }
    move(LINES-1,0);
    refresh();

}

