#include "interface.h"

volatile int MSG_QUE_KEY = 0; //KEY VALUE
volatile int MSG_QUE_ID = 0;  //msg_id

volatile unsigned long long rev_count = 0;
/*READ data from message queue*/
INT8 Read_MSG_QUE(rev_pthread_param * rev)
{
    msg_que msg_buf;
    Data_Protocal package;

    rev_pthread_param rev_param;
    char str[1] = {0x46}; //end
    int err = 0;

        err = msgrcv(MSG_QUE_ID, (void *)&msg_buf,  sizeof(msg_buf) - sizeof(long),0, 0);
        if(err <= 0)
        {
#ifdef DEBUG
	        printf("Mssage queue rev data failed!\n");
#endif
        }
        else
        {
#ifdef RESULT
	        printf("Mssage queue rev data success!\n");
	        printf("REV MSG_TYPE is :%ld\n DATA is:%d\n", msg_buf.msg_type, msg_buf.data[0]);
#endif
            Data_Protocal_Package(&package, PROPERTY_CMD, str, 1);
	        Send_data(rev->port_id, &package, (ssize_t (*)(int, const void*, size_t))rev->read_data);

#ifdef RESULT
	        printf("Current successful Rev package num:%lld\n", ++rev_count);
#endif
        }


    return msg_buf.data[0];
}

/*strlen by myself*/
int My_Strlen(const char * src)
{
    const char * temp = src;
    if(src == NULL)
        return -1;
    while(*src++);
    return src - temp - 1;
}

