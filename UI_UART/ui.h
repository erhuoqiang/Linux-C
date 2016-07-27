#include <iostream>
#include <stdio.h>
#include <string.h>
#include <vector>

using namespace std;

class MenuItem;
class BasePage;

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
    MenuItem* m_items[10];

    int item_count;//item counts in this page
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
    MenuItem(string text_head,int x,int y,int page_index,int item_index,bool state);
    MenuItem(string text_head,int x,int y,int page_index,int item_index,int value,int max_value,int min_value);
    MenuItem(string text_head,int x,int y,int page_index,int item_index,BasePage* nextpage,BasePage* prevpage,BasePage* curpage);
    void ShowItem(MenuItem* );

    //general parameter as followed
    int item_type;
    int pos_x;
    int pos_y;
    int index_p;
    int index_i;
    string text_head;
    string text_tail;
    //specificied parameter
    bool switching_state;
    int set_num;
    int max_num;
    int min_num;
    BasePage* nextpage;
    BasePage* prevpage;
    BasePage* currentpage;
private:
    item_info* GetItemInfo(MenuItem* );//get infomation of the certain item
    int GetItem_type(MenuItem* );

};
