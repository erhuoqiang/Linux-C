#ifndef __UI_H
#define __UI_H

#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>
#include <curses.h>

using namespace std;
#define POS_Y    5

class MenuItem;
class BasePage;
class Event;

struct item_info
{
    int x_pos;
    int y_pos;
    int item_type;
    char* item_text[];
    int item_index;
    int page_index;
    bool state;
    BasePage* nextpage;

};

class BasePage
{
public:
    BasePage(string);
    MenuItem* m_items[20];

    int item_count;//item counts in this page
    BasePage* prevpage;
    string page_name;

    void ShowPage(BasePage*);
    void AddItem(MenuItem* a);

private:

    int page_index;//page index
    void PageSetIndex(int);
    void ClearOneItem();
    void ClearAllItems();
    int GetItemCount();

};

class MenuItem
{
public:		
//		void CreateItem(int ID,string head);
    virtual void ShowItem(){};
    virtual void ChangeState(int flag){};

    int pos_x;
    int item_type;
    string text_head;
    string text_tail;
};

class SwitchingItem:public MenuItem
{
public:	
		SwitchingItem(int ID,string head,bool state);
		void ShowItem();
		void ChangeState(int flag);
	
		bool switching_state;			
	
};

class NumericalItem:public MenuItem
{
public:	
		NumericalItem(int ID,string head,int setvalue,int minnum,int maxnum);
		void ShowItem();
		void ChangeState(int flag);
	
    int set_num;
    int max_num;
    int min_num;	
	
};

class EnterItem:public MenuItem
{

public:	
		EnterItem(int ID,string head,BasePage* next_page,BasePage* prevpage);
		void ShowItem();
		void ChangeState(int flag);
	
		BasePage* nextpage; 
    BasePage* prevpage;		
	
};

#endif