#ifndef __EXIT_H
#define __EXIT_H

#include <curses.h>
#include <iostream>
#include <string>
#include "ui.h"

extern int edit_flag;

class MenuItem;

class ExitItem:public MenuItem
{

public:
		ExitItem(int ID);
		void ShowItem();
    void ChangeState(int flag);
    
};

class EditItem:public MenuItem
{
	
public:
	  EditItem(int ID,string head);
	  void ShowItem();
   	void ChangeState(int flag);

    int pos_x;
    string text_head;
    string text_tail;
};

#endif
