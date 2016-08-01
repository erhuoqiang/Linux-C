#include <iostream>
#include <curses.h>
#include "ui.h"
#include "uart.h"
#include "format.h"
#include "control.h"
#include "user.h"
#include "protocal.h"
#include "interface.h"

INT8  port_fd = 0;
void read_key()
{
    while(1)
    {
        recv_data = getch();
        recvprocess(recv_data);
    }
}

void read_msg()
{
    rev_pthread_param msg_pth_param;
    msg_pth_param.port_id = port_fd;
    msg_pth_param.read_data = (ssize_t (*)(int,  void*, size_t))write;
    while(1)
    {
	 recv_data = Read_MSG_QUE(&msg_pth_param);
	 recvprocess(recv_data);
    }
}


typedef struct
{
    int port;
    int us;
    ssize_t (*write_data)(int fd,  const void * buf, size_t data_len);
}Inter_Signal_St;


/* Send Interference Signal */
void Send_Inter_Sinal(int sign)
{
    char signal[]="helloworld2";

    if( write(port_fd, (void *)signal,(size_t) My_Strlen(signal)) == -1)
    {
#ifdef DEBUG
        printf("Send helloworld1 failed!\n");
#endif
    }
#ifdef DEBUG
        printf("Send helloworld1 success!\n");
#endif
}

/* Interference_Signal pthread 10ms send Interference_Signal*/
void  Interference_Signal(void )
{
    int res = 0;
    struct itimerval tick;

    signal(SIGALRM, Send_Inter_Sinal);
    memset(&tick, 0, sizeof(tick));

    //Timeout to run first time
    tick.it_value.tv_sec = 0;
    tick.it_value.tv_usec = 10000;

    //After first, the Interval time for clock
    tick.it_interval.tv_sec = 0;
    tick.it_interval.tv_usec = 10000;
    if(setitimer(ITIMER_REAL, &tick, NULL) < 0)
            printf("Set timer failed!\n");

    //When get a SIGALRM, the main process will enter another loop for pause()
    while(1);

}
int main()
{
     int rec;

/****************************uart.c*****************************/

     msg_que msg_buf;
     int err = 0;
     rev_pthread_param rev_param;
     pthread_t rev_pthid,read_msg_pthid,send_pthid; //pthread;

     port_fd = Open_Port(DEV_PORT0);
     Serial_Init(port_fd, 115200, 8, 'N', 1);

    Inter_Signal_St st;
     rev_param.port_id = port_fd;// Init port_id
     rev_param.read_data = read;  //read function point

   // st.port = port_fd;
    // st.write_data = write;
    // st.us = 10000;

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

    err = pthread_create(&send_pthid, NULL,(void *(*)(void *))Interference_Signal, (void *)NULL);
     if(err != 0 )
        printf("Create Interference Signal pthread error!\n");
     else
#ifdef DEBUG
        printf("Create Interference Signal pthread success!\n");
#endif
     while(1);
/***************************************************************/
    endwin();

    return 0;
}
