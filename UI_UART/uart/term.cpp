#include"uart.h"

int main(int argc, char **argv)
{
     INT8  port_fd = 0;
     msg_que msg_buf;
     int err = 0;
     rev_pthread_param rev_param;
     pthread_t rev_pthid,read_msg_pthid; //pthread;

     port_fd = Open_Port(DEV_PORT0);
     Serial_Init(port_fd, 115200, 8, 'N', 1);
     rev_param.port_id = port_fd;// Init port_id

     Create_MSG_QUE("./Makefile",2);  //Create message queue
     err = pthread_create(&rev_pthid, NULL,(void *(*)(void *))Data_Rev, (void *)&rev_param);
     if(err != 0 )
        printf("Create Data_Rev pthread error!\n");
     else
        printf("Create Data_Rev pthread success!\n");
     err = pthread_create(&read_msg_pthid, NULL,(void *(*)(void *))Read_MSG_QUE, (void *)NULL);
     if(err != 0 )
        printf("Create Read_MSG_QUE pthread error!\n");
     else
        printf("Create Read_MSG_QUE pthread success!\n");
     while(1);

}
