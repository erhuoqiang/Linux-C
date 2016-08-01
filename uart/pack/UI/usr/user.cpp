#include <stdio.h>
#include "ui.h"
#include "control.h"
#include "exit.h"

BasePage page1("Index Page");
BasePage page2("Panel Setting");
BasePage page3("Picture Setting");
BasePage page4("Sound Setting");
BasePage* page_list[5];

void Menu_init()
{
/*********************UI initial****************************/
		page_list[0] = &page1;
    page_list[1] = &page2;
    page_list[2] = &page3;
    page_list[3] = &page4;
    
    MenuItem* item1 = new EnterItem(1,"Panel Setting",&page2,NULL);
		MenuItem* item2 = new EnterItem(2,"Picture Setting",&page3,NULL);
		MenuItem* item3 = new EnterItem(3,"Sound Setting",&page4,NULL);
		MenuItem* item4 = new SwitchingItem(4,"Debug On",false);
		MenuItem* item12 = new SwitchingItem(5,"Debug On",false);
		MenuItem* item13 = new SwitchingItem(6,"Debug On",false);
		MenuItem* item14 = new SwitchingItem(7,"Debug On",false);
		MenuItem* item15 = new SwitchingItem(8,"Debug On",false);
		MenuItem* item16 = new SwitchingItem(9,"Debug On",false);
		
		MenuItem* item5 = new SwitchingItem(1,"Panel On",false);
		MenuItem* item6 = new SwitchingItem(2,"Backlight On",false);
		
		MenuItem* item7 = new NumericalItem(1,"Brightness",50,0,100);
		MenuItem* item8 = new NumericalItem(2,"Contrast",20,0,100);
		
		MenuItem* item9 = new NumericalItem(1,"Treble",50,0,64);
		MenuItem* item10 = new NumericalItem(2,"Bass",16,0,32);
		MenuItem* item11 = new NumericalItem(3,"Balance",30,-50,50);
		
		MenuItem* edit_item = new EditItem(10,"CH_NAME");
		MenuItem* exit_item = new ExitItem(11);

    page1.AddItem(item1);
    page1.AddItem(item2);
    page1.AddItem(item3);
    page1.AddItem(item4);
    page1.AddItem(item12);
    page1.AddItem(item13);
    page1.AddItem(item14);
    page1.AddItem(item15);
    page1.AddItem(item16);
    page1.AddItem(edit_item);
    page1.AddItem(exit_item);


    page2.AddItem(item5);
    page2.AddItem(item6);

    page3.AddItem(item7);
    page3.AddItem(item8);

    page4.AddItem(item9);
    page4.AddItem(item10);

    page4.AddItem(item11);
    
}
