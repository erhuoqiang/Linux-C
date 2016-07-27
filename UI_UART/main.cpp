#include <iostream>
#include <curses.h>
#include <string>
#include <sstream>
#include "ui.h"
#include "uart.h"

#define RECV_UP    65
#define RECV_DOWN  66
#define RECV_LEFT  68
#define RECV_RIGHT 67
#define RECV_BACK  32
#define RECV_MENU  109

int item_pos = 1;//current position
int prev_pos = 0;//previous positon
int m_item_counts = 0;//number of items in current page
int cur_type = 0;//current enter item's type
bool in_flag = false;//0:none setting   1:in setting
bool sleep_flag = false;
int recv_data = 0;

BasePage* page_list[5];
BasePage* cur_page = NULL;
BasePage* back_page = NULL;
MenuItem* current_item;

BasePage page1;
BasePage page2;
BasePage page3;
BasePage page4;

MenuItem item1 = MenuItem("01 Panel Setting",1,5,1,1,&page2,NULL,&page1);
MenuItem item2 = MenuItem("02 Picture Setting",2,5,1,2,&page3,NULL,&page1);
MenuItem item3 = MenuItem("03 Sound Setting",3,5,1,3,&page4,NULL,&page1);
MenuItem item4 = MenuItem("04 Debug On",4,5,1,4,false);
MenuItem item5 = MenuItem("01 Panel On",1,5,2,1,false);
MenuItem item6 = MenuItem("02 Backlight On",2,5,2,2,false);
MenuItem item7 = MenuItem("01 Brightness",1,5,3,1,50,100,0);
MenuItem item8 = MenuItem("02 Contrast",2,5,3,2,20,100,0);
MenuItem item9 = MenuItem("01 Treble",1,5,4,1,50,64,0);
MenuItem item10 = MenuItem("02 Bass",2,5,4,2,16,32,0);
MenuItem item11 = MenuItem("03 Balance",3,5,4,3,30,50,-50);

void screen_init()
{
    initscr();
    keypad(stdscr,true);
    clear();
}

void Menu_init()
{
/*********************UI initial****************************/
    page_list[0] = &page1;
    page_list[1] = &page2;
    page_list[2] = &page3;
    page_list[3] = &page4;

    page1.AddItem(&item1);
    page1.AddItem(&item2);
    page1.AddItem(&item3);
    page1.AddItem(&item4);

    page2.AddItem(&item5);
    page2.AddItem(&item6);

    page3.AddItem(&item7);
    page3.AddItem(&item8);

    page4.AddItem(&item9);
    page4.AddItem(&item10);
    page4.AddItem(&item11);

}

void ShowPage(BasePage* page)
{

    int j;
    clear();

    for(j=0;j<page->item_count;j++)
    {
        page->m_items[j]->ShowItem(page->m_items[j]);
    }
    move(LINES-1,0);
    refresh();

}
void rightchecktype()
{
    int current_type;
 //   MenuItem* current_item;
    stringstream stream;
    current_type = current_item->item_type;
    switch(current_type)
    {
        case 1://switchitem
            cur_type = 1;
            in_flag = true;
            if(current_item->switching_state==true)
            {
                current_item->switching_state = false;
                current_item->text_tail = "FALSE";
                mvaddstr(current_item->pos_x,current_item->pos_y+30,(current_item->text_tail).c_str());
            }
            else
            {
                current_item->switching_state = true;
                current_item->text_tail = "TRUE ";
                mvaddstr(current_item->pos_x,current_item->pos_y+30,(current_item->text_tail).c_str());
            }
            break;
        case 2://numericaltiem
            cur_type = 2;
            in_flag = true;
            if(current_item->set_num < current_item->max_num)
            {
                current_item->set_num += 1;
                stream << (current_item->set_num);
                stream >> (current_item->text_tail);
                mvaddstr(current_item->pos_x,current_item->pos_y+30,"     ");
                mvaddstr(current_item->pos_x,current_item->pos_y+30,(current_item->text_tail).c_str());
            }
            break;
        case 3://enteritem
            prev_pos = item_pos;
            item_pos = 1;
            if(current_item->nextpage!=NULL)
            {
                back_page = cur_page;
                cur_page = current_item->nextpage;
                m_item_counts = cur_page->item_count;
            }
            ShowPage(cur_page);
            mvaddch(item_pos,0,'*');
            break;
    }
}

void leftchecktype()
{
    int current_type;
    stringstream stream;

    current_type = current_item->item_type;
    switch(current_type)
    {
        case 1://switchitem
            in_flag = true;
            if(current_item->switching_state==true)
            {
                current_item->switching_state = false;
                current_item->text_tail = "FALSE";
                mvaddstr(current_item->pos_x,current_item->pos_y+30,(current_item->text_tail).c_str());
            }
            else
            {
                current_item->switching_state = true;
                current_item->text_tail = "TRUE ";
                mvaddstr(current_item->pos_x,current_item->pos_y+30,(current_item->text_tail).c_str());
            }
            break;
        case 2://numericaltiem
            in_flag = true;
            if(current_item->set_num > current_item->min_num)
            {
                current_item->set_num -= 1;
                stream << (current_item->set_num);
                stream >> (current_item->text_tail);
                mvaddstr(current_item->pos_x,current_item->pos_y+30,"     ");
                mvaddstr(current_item->pos_x,current_item->pos_y+30,(current_item->text_tail).c_str());
            }
            break;
        case 3://enteritem
        /****
            in_flag = false;
            item_pos = 1;
            cur_page = current_item->nextpage;
            ShowPage(cur_page);
            break;
        ******/
            break;
    }

}

void exit_mainmenu()
{
    clear();
    sleep_flag = true;
}

void return_menu()
{
    in_flag = false; // exit setting mode
    if(back_page)
    {
        item_pos = prev_pos;
        cur_page = back_page;
        back_page = NULL;
        m_item_counts = cur_page->item_count;
        ShowPage(cur_page);
        mvaddch(item_pos,0,'*');

    }
    else
    {
        exit_mainmenu();

    }
}

void init_startpage()
{
    item_pos = 1;
    cur_page = page_list[0];
    m_item_counts = cur_page->item_count;
    ShowPage(&page1);
    mvaddch(item_pos,0,'*');
    move(LINES-1,0);
    refresh();
}

void recvprocess(int recv,int recv_flag)
{
    current_item = cur_page->m_items[item_pos-1];
    if((sleep_flag==true)&&(recv!=RECV_MENU))
    {
        return ;
    }
    //receive from port
    if(recv_flag==1)
    {
        switch(recv)
        {
            case RECV_DOWN:
                if(in_flag==true)
                {
                    in_flag = false;
                }
                mvaddch(item_pos,0,' ');//clear previous symbol '*'
                if(item_pos == m_item_counts)
                    item_pos = 0;
                item_pos += 1;//record current item's index
                mvaddch(item_pos,0,'*');//draw symbol * on new position
            break;
            case RECV_UP:
                if(in_flag==true)
                {
                    in_flag = false;
                }
                mvaddch(item_pos,0,' ');//clear previous symbol '*'
                if(item_pos == 1)
                    item_pos = m_item_counts+1;
                item_pos -= 1;
                mvaddch(item_pos,0,'*');
            break;
        case RECV_LEFT:
            leftchecktype();
            break;
        case RECV_RIGHT:
            rightchecktype();
            break;
        case RECV_BACK:
            return_menu();
            break;
        case RECV_MENU:
            if(sleep_flag==true)
            {
                init_startpage();
                sleep_flag = false;
            }
            break;
        default:
            break;
        }
    }
    else
    {
        switch(recv)
        {
            case KEY_DOWN:
                if(in_flag==true)
                {
                    in_flag = false;
                }
                mvaddch(item_pos,0,' ');//clear previous symbol '*'
                if(item_pos == m_item_counts)
                    item_pos = 0;
                item_pos += 1;//record current item's index
                mvaddch(item_pos,0,'*');//draw symbol * on new position
            break;
            case KEY_UP:
                if(in_flag==true)
                {
                    in_flag = false;
                }
                mvaddch(item_pos,0,' ');//clear previous symbol '*'
                if(item_pos == 1)
                    item_pos = m_item_counts+1;
                item_pos -= 1;
                mvaddch(item_pos,0,'*');
            break;
        case KEY_LEFT:
            leftchecktype();
            break;
        case KEY_RIGHT:
            rightchecktype();
            break;
        case RECV_BACK:
            return_menu();
            break;
        case RECV_MENU:
            if(sleep_flag==true)
            {
                init_startpage();
                sleep_flag = false;
            }
            break;
        default:
            break;
        }
    }

    move(LINES-1,0);
    refresh();
}

void read_key()
{
    while(1)
    {
        recv_data = getch();
        recvprocess(recv_data, 0);
    }
}

void read_msg()
{
    while(1)
    {
	 recv_data = Read_MSG_QUE();
	 recvprocess(recv_data, 1);
    }
}
int main()
{
     int rec;

/****************************uart.c*****************************/
     INT8  port_fd = 0;
     msg_que msg_buf;
     int err = 0;
     rev_pthread_param rev_param;
     pthread_t rev_pthid,read_msg_pthid; //pthread;

     port_fd = Open_Port(DEV_PORT0);
     Serial_Init(port_fd, 115200, 8, 'N', 1);
     rev_param.port_id = port_fd;// Init port_id

     Create_MSG_QUE("./Makefile",2);  //Create message queue
/******/
     screen_init();
     Menu_init();
     init_startpage();
/******/
     err = pthread_create(&rev_pthid, NULL,(void *(*)(void *))Data_Rev, (void *)&rev_param);
     if(err != 0 )
        printf("Create Data_Rev pthread error!\n");
     else
#ifdef DEBUG
        printf("Create Data_Rev pthread success!\n");
#endif
     err = pthread_create(&read_msg_pthid, NULL,(void *(*)(void *))read_key, (void *)NULL);
     if(err != 0 )
        printf("Create read_key pthread error!\n");
     else
#ifdef DEBUG
        printf("Create read_key pthread success!\n");
#endif
     err = pthread_create(&read_msg_pthid, NULL,(void *(*)(void *))read_msg, (void *)NULL);
     if(err != 0 )
        printf("Create read_key pthread error!\n");
     else
#ifdef DEBUG
        printf("Create read_key pthread success!\n");
#endif
     while(1);
/***************************************************************/
    endwin();

    return 0;
}
