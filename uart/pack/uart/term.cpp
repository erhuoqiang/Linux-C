#include"uart.h"

void  Interference_Signal(Inter_Signal_St * st)
{
    char signal[]="helloworld2";
    while(1)
    {
        if( (*st->write_data)(st->port, (void *)signal,(size_t) My_Strlen(signal)) == -1)
        {
#ifdef DEBUG
            printf("Send helloworld1 failed!\n");
#endif
        }
#ifdef DEBUG
       // printf("Send helloworld1 success!\n");
        usleep(st->us);
#endif
    }
}

int main(int argc, char **argv)
{
     INT8  port_fd = 0;
     msg_que msg_buf;
     int err = 0;
     rev_pthread_param rev_param, msg_pth_param;
     pthread_t rev_pthid,read_msg_pthid ,send_pthid; //pthread;
     Inter_Signal_St st;
     port_fd = Open_Port(DEV_PORT0);
     Serial_Init(port_fd, 115200, 8, 'N', 1);

     rev_param.port_id = port_fd;// Init port_id
     rev_param.read_data = read;  //read function point
     msg_pth_param.port_id = port_fd;
     msg_pth_param.read_data = (ssize_t (*)(int,  void*, size_t))write;

     st.port = port_fd;
     st.write_data = write;
     st.us = 10000;


     tcflush(port_fd,TCIOFLUSH);
     Create_MSG_QUE("./Makefile",2);  //Create message queue
     err = pthread_create(&rev_pthid, NULL,(void *(*)(void *))Data_Rev, (void *)&rev_param);
     if(err != 0 )
        printf("Create Data_Rev pthread error!\n");
     else
        printf("Create Data_Rev pthread success!\n");
     err = pthread_create(&read_msg_pthid, NULL,(void *(*)(void *))Read_MSG_QUE, (void *)&msg_pth_param);
     if(err != 0 )
        printf("Create Read_MSG_QUE pthread error!\n");
     else
        printf("Create Read_MSG_QUE pthread success!\n");

    err = pthread_create(&send_pthid, NULL,(void *(*)(void *))Interference_Signal, (void *)&st);
     if(err != 0 )
        printf("Create Interference Signal pthread error!\n");
     else
#ifdef DEBUG
        printf("Create Interference Signal pthread success!\n");
#endif
     while(1);

}
