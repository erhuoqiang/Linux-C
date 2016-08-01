#include <iostream>
#include <stdio.h>
#include "control.h"

void LeftKeyEvent::control(MenuItem* m_item,int cmd)
{
		m_item->ChangeState(command);
}