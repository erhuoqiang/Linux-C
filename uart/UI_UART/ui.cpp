#include "ui.h"
#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>
#include <curses.h>
#include <sstream>


MenuItem::MenuItem(string text_head_t,int x_t,int y_t,int page_index_t,int item_index_t,bool state_t)
{
    item_type = 1; //switchitem
    pos_x = x_t;
    pos_y = y_t;
    index_p = page_index_t;
    index_i = item_index_t;
    switching_state = state_t;

    text_head = text_head_t;
    if(switching_state==true)
        text_tail = "TRUE";
    else
        text_tail = "FALSE";
}
MenuItem::MenuItem(string text_head_t,int x_t,int y_t,int page_index_t,int item_index_t,int value_t,int max_value_t,int min_value_t)
{
    stringstream stream;
    item_type = 2; //numericalitem
    pos_x = x_t;
    pos_y = y_t;
    index_p = page_index_t;
    index_i = item_index_t;
    set_num = value_t;
    max_num = max_value_t;
    min_num = min_value_t;

    stream << set_num;
    stream >> text_tail;
    text_head = text_head_t;
 //   sprintf(text_tail,"%x",set_num);
}

MenuItem::MenuItem(string text_head_t,int x_t,int y_t,int page_index_t,int item_index_t,BasePage* next_page_t,BasePage* prev_page_t,BasePage* curpage)
{
    item_type = 3; //subenteritem
    pos_x = x_t;
    pos_y = y_t;
    index_p = page_index_t;
    index_i = item_index_t;
    nextpage = next_page_t;
    prevpage = prev_page_t;
    currentpage = curpage;

    text_head = text_head_t;
    text_tail = ">>>>";
}

void MenuItem::ShowItem(MenuItem* m_item)
{
    mvaddstr(m_item->pos_x,m_item->pos_y,(m_item->text_head).c_str());
    mvaddstr(m_item->pos_x,m_item->pos_y+30,(m_item->text_tail).c_str());
}


/***basepage construct func***/
BasePage::BasePage(string str)
{
    page_name = str;
    item_count = 0;
    page_index = 0;
}

void BasePage::AddItem(MenuItem* item)
{
    if(item!=NULL)
    {
        m_items[item_count] = item;//add one item to vector
        item_count = item_count + 1;
    }
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

    for(j=0;j<page->item_count;j++)
    {
        page->m_items[j]->ShowItem(m_items[j]);
    }
    move(LINES-1,0);
    refresh();

}

