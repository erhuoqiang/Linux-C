#ifndef __FORMAT_H
#define __FORMAT_H

extern void Menu_init();
extern void screen_init();
extern void init_startpage();
extern void recvprocess(int);
extern void exit_mainmenu();

extern int recv_data;
extern int delete_flag;
extern int item_pos;//current position
extern int prev_pos;//previous positon
extern int m_item_counts;//number of items in current page
extern bool sleep_flag;

extern BasePage* cur_page;

#endif

