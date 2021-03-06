
#include "protocal.h"

/*
CRC校验函数 根据自己原本根据书本思路写的改进crc赋初始值是为了考虑开头数据多了很多0
并不影响CRC生成校验值的结果这种情况，提高效率的地方是crc = crc ^ (*addr++ << 8);
这里是根据^位与同组值异或的顺序不同不影响结果的原理写的
参数：addr是数据数组首地址，num是数据长度，crc是crc的初始值
*/

U16 CRC16_Check(U8* data, int num, int crc)
{
    int i = 0;
    if(data == NULL || num < 0)
    {
#ifdef DEBUG
        printf("crc16_check parameter error! \n");
#endif
        return -1;
    }

    for( ;num > 0; num--)
    {
        crc ^= (*data++ << 8);
        for( i = 0; i < 8; i++)
        {
            if( (crc & 0x8000) )
            {
                crc = (crc<<1) ^ POLY;
            }
            else
                crc = crc<<1;
        }
    }

  // crc &= 0xFFFF;
    return crc;
}

/*********************Send Part***********************/
/*参数package是数据格式帧 property是属性 buf是要发送的数据 data_length是数据长度*/
void Data_Protocal_Package(Data_Protocal * package, U8 property, INT8 *buf, U8 data_length)
{
    if(package == NULL || buf == NULL || data_length == 0)
    {
        printf("Input error!\n");
        return;
    }

    if(data_length > MAX_DATA_LENGTH)
    {
        printf("Data is too long, Please consider subpackage.\n");
        return;
    }
    package->head[0] = HEAD;
    package->length[0] = data_length + 6; /*length = 一帧数据的长度 =  协议长度+数据长度*/
    package->property[0] = property;

    memcpy(package->data, buf, data_length);

    package->crc_fsc = CRC16_Check((U8 *)package + 1,data_length + 2,CRC_INIT);
#ifdef DEBUG
    printf("CRC is %d\n",package->crc_fsc);
#endif 
    package->tail[0] = TAIL;
}

/*避免数据中出现和HEAD,TAIL和转义字符标志字段同ASCII码的 采用字节填充的方法
package_buf是数据帧，生成的buf是经过转义的数据帧确保标志符
不出现在信息字段 HEAD ->ESC 0X01 TAIL->ESC 0X03 ESC->ESC 0X02*/
INT8 Escape_Character(U8 *package_buf, U8 package_length, U8 *buf)
{
	if(NULL == package_buf || 0 == package_length)
	{
#ifdef DEBUG
		printf("Input error!\n");
#endif
		return -1;
	}

	U8 count = 1;

	buf[0] = package_buf[0];
	buf++;
	for (; count < (package_length -1); count++)
	{
		if (HEAD == package_buf[count])
		{
			*buf++ = ESC;
			*buf++ = ESC_HEAD;
		}
		else if (ESC == package_buf[count])
		{
			*buf++ = ESC;
			*buf++ = ESC_ESC;
		}
		else if (TAIL == package_buf[count])
		{
			*buf++ = ESC;
			*buf++ = ESC_TAIL;
		}
		else
		{
			*buf++ = package_buf[count];
		}
	}
	*buf++ = TAIL;
	*buf = '\0';

	return 0;
}

/*port是发送端口的文件描述符，package是要发送的数据格式包 write_data 是设备的写函数的函数指针 -1返回错误 否则返回写的数据长度*/
int Send_data(INT8 port,Data_Protocal * package, ssize_t (*write_data)(int fd,  const void * buf, size_t data_len ))
{
    int i = 0, escape_count = 0, len = 0;
    U8 * send_buf = NULL;
    U8 * package_buf = (U8*)malloc(package->length[0]);

    if(package == NULL)
    {
#ifdef DEBUG
        printf("Input error!\n");
#endif
        return -1;
    }
    /*统计需要转移字符的个数*/
    len = package->length[0] - 2;
    for(i = 1; i <= len - 2; i++)  //这里先不考虑CRC16的两个字节 后面单独考虑
    {
        if( HEAD == ((U8 *)package)[i] ||
            TAIL == ((U8 *)package)[i] ||
            ESC == ((U8 *)package)[i] )
            escape_count++;
        package_buf[i] = ((U8 *)package)[i];
    }
    /*CRC high byte*/
    if( HEAD == package->crc_fsc>>8 ||
        TAIL == package->crc_fsc>>8 ||
        ESC == package->crc_fsc>>8 )
        escape_count++;
    package_buf[i++] = package->crc_fsc>>8;

    /*CRC low byte*/
    if( HEAD == (package->crc_fsc & 0xff)  ||
        TAIL == (package->crc_fsc & 0xff)  ||
        ESC == (package->crc_fsc & 0xff) )
        escape_count++;
    package_buf[i++] = package->crc_fsc & 0xff;
    package_buf[i] = TAIL;
    package_buf[0] = HEAD;

    send_buf = (U8*)malloc(package->length[0] + escape_count + 1);


    /*如果包含转义字符 则转义后发送 不包含则直接发送*/
    if( 0 != escape_count )
    {
        Escape_Character(package_buf, package->length[0], send_buf);

        if( (*write_data)(port, (void *)send_buf,(size_t)(package->length[0] + escape_count)) == -1)
        {
#ifdef DEBUG
            printf("Send data failed!\n");
#endif
            free(send_buf);
            free(package_buf);
            return -1;
        }
#ifdef DEBUG
        printf("Send data success!\n");
#endif
    }
    else
    {
#ifdef DEBUG
        for( i = 0; i < package->length[0] + escape_count; i++)
        {
                printf(" %x ",package_buf[i]);
        }
        printf("\n");
#endif
        if( (*write_data)(port, (void *)package_buf,(size_t)package->length[0]) == -1)
        {
#ifdef DEBUG
            printf("Send data failed!\n");
#endif
            free(send_buf);
            free(package_buf);
            return -1;
        }
#ifdef DEBUG
        printf("Send data success!\n");
#endif
    }
    free(send_buf);
    free(package_buf);
    return 0;
}

/********************REV PART***********************/

/*反转义 HEAD<-ESC 0X01 TAIL<-ESC 0X03 ESC<-ESC 0X02
rev_buf是接收到还没有解析的数据，data_package是解析后数据存放的数组*/
INT8 Reverse_Escape_Character(U8 *rev_buf, U8 *data_package)
{
    U8 count = 0;

    if (NULL == data_package|| NULL == rev_buf)
    {
#ifdef DEBUG
	printf("Input error!\n");
#endif
	return -1;
    }

    data_package[0] = rev_buf[0];
    data_package++;
    rev_buf++;

    while (TAIL != *rev_buf)
    {
        if ( ESC== *rev_buf )
        {
            rev_buf++;

            if ( ESC_HEAD == *rev_buf  )
            {
                *data_package++ = HEAD;
            }
            else if ( ESC_ESC == *rev_buf )
            {
                *data_package++ = ESC;
            }
            else if ( ESC_TAIL == *rev_buf )
            {
                *data_package++ = TAIL;
            }
            else
            {
                *data_package++ =  ESC;  //如果不是前面三种情况则说明传输过程中该位或者后一位出现的错误
									     //则可以选择对错误位操作 可以用全局变量来通知或者进一步操作
                *data_package++ = *rev_buf;    //这里是收下这两个可能出错的位数
            }
            rev_buf++;
        }
        else
        {
            *data_package++ = *rev_buf++;
        }

    }
    *data_package = TAIL;
    return 0;
}

/*rev_buf是接受到数据的缓存 可能是3.5组的数据，data_start_pos是数据起始的位置
data_end_pos是数据结束的位置 Rev_process函数的功能则是将收到的3.5数组中的3个
完整组解析，返回剩余0.5组 数据的起始位置*/
int Rev_Process(U8 * rev_buf, int data_start_pos, int data_end_pos)
{
    int i = 0;
    int pos = 0;
    int head_flag = 0, tail_flag = 0;
    msg_que msg_buf;
    int err = 0;
    U8 one_frame[(MAX_DATA_LENGTH + PROTOCAL_LENGTH)*2] = {0}; //考虑到发生端发送的所有数据都经过转义这种情况 所以大小是2倍协议长
    U8 package[(MAX_DATA_LENGTH + PROTOCAL_LENGTH)] = {0};
    int crc = 0;
    for( ; data_start_pos < data_end_pos; data_start_pos++ )
    {
        if( HEAD == rev_buf[data_start_pos] )
        {
            pos = data_start_pos;
            head_flag  = 1;
        }
        else if(TAIL == rev_buf[data_start_pos] && 1 == head_flag )
        {
            tail_flag = 1;
        }
        if( 1 == tail_flag ) //tai_flag =1 为接受完整的一帧数据 下面进一步处理如 反转义和CRC校验
        {
            memcpy(one_frame, rev_buf + pos, data_start_pos - pos + 1);
#ifdef DEBUG
            printf("Before Reveres_Escape:\n");
            for(i = 0; i < data_start_pos - pos + 1; i++)
            {
                printf(" %x ",one_frame[i]);
            }
            printf("\n");
#endif
            Reverse_Escape_Character(one_frame, package);
#ifdef DEBUG
            printf("REV CRC value is %d \n", package[package[1] - 3]*256 + package[package[1] - 2]);
#endif
            crc = CRC16_Check(package + 1, package[1] - 2, CRC_INIT);
            if( 0 == crc )
            {
#ifdef DEBUG
                printf("crc check is success!\n");
#endif
#ifdef RESULT
                printf("Property is :%d\n Key is:%d\n", package[2], package[3]);
#endif
/**********************如果改了数据发送长度 这里需要修改*****************/
                msg_buf.msg_type = package[2];
                for(i = 0; i < package[1] - 6; i++)
                    msg_buf.data[i]  =  package[3 + i];
                //MSG_QUE_ID = msgget(MSG_QUE_KEY,IPC_EXCL);
              //  printf("%d\n",MSG_QUE_ID);
                err = msgsnd(MSG_QUE_ID, (void *)&msg_buf, sizeof(msg_buf) - sizeof(long), IPC_NOWAIT);
                if(err < 0)
               {
#ifdef DEBUG
                   printf("Mssage queue send data failed!\n");
#endif
               }
               else
               {
#ifdef RESULT
                    printf("Mssage queue send data success!\n");
#endif
               }

            }
            else
            {
#ifdef DEBUG
                printf("crc check is failed!\n");
#endif
            }
            head_flag = 0;
            tail_flag = 0;
            pos = 0;
#ifdef DEBUG
            printf("After Reveres_Escape:\n");
            for(i = 0; i < package[1]; i++)
            {
                printf(" %x ",package[i]);
            }
            printf("\n");
#endif
        }

    }
    return pos;
}

/*数据接受函数，rev_param参数一个结构体 是传递用来存放数据缓存的内存，和读取串口的文件描述符号*/
void Data_Rev(rev_pthread_param * rev_param)
{
    U8 rev_buf[512];
    int data_end_pos =  0;
    int data_start_pos = 0;
    int process_pos = 0;
    int len = 0;
    ssize_t (*read_data)(int,void *, size_t) = rev_param->read_data;
    while(1)
    {
	/*读取串口数据*/
        len = read_data(rev_param->port_id, rev_param->buf, REV_BUFFER_SIZE - data_end_pos);
        if(len > 0)
        {
#ifdef DEBUG
            printf("\n\nrev_len is %d\n", len);
#endif
            data_end_pos += len;
            continue;
        }

	/*如果data_end_pos和data_start_pos差小于6（协议长）说明肯定没有一帧数据所以不进入*/
        if( data_end_pos < REV_BUFFER_SIZE && data_end_pos - data_start_pos > 6 )
        {
            process_pos = Rev_Process(rev_param->buf, data_start_pos, data_end_pos);
            if (process_pos > 0)
            {
                data_start_pos = process_pos;
            }
            else
            {
                data_start_pos = data_end_pos = 0;
		//刚好整数帧则将 data_start_pos = data_end_pos =0 减少进入  if(data_end_pos == REV_BUFFER_SIZE)次数
            }
        }

	/*数据存储位置到了缓存数组的末尾，所以将取整数帧后，剩下的不完整的帧 复制到缓存数组开头*/
        if(data_end_pos == REV_BUFFER_SIZE)
        {

            process_pos = Rev_Process(rev_param->buf, data_start_pos, data_end_pos);
            if(0 == process_pos)
            {
                data_end_pos = data_start_pos = 0;
                continue;
            }
            memcpy(rev_param->buf, rev_param->buf + process_pos, data_end_pos - process_pos);
            data_start_pos = 0;
            data_end_pos = REV_BUFFER_SIZE - process_pos;
        }
    }

}



/*Create Message queue  param path,num is give to  ftok function*/
void Create_MSG_QUE(const char * path, int num)
{
     MSG_QUE_KEY = ftok(path,num);
     printf("Create Message Queue KEY is: %d!\n",MSG_QUE_KEY);
     MSG_QUE_ID = msgget(MSG_QUE_KEY,  IPC_EXCL);
     if(MSG_QUE_ID < 0 )
     {
         printf(" Message Queue is not exit!\n");
         MSG_QUE_ID = msgget(MSG_QUE_KEY,  IPC_CREAT | 0666);
         if(MSG_QUE_ID < 0)
         {
             printf("Create MSG QUE is failed\n");
         }
         else
         {
              printf("Create Message Queue is success. ID is: %d!\n",MSG_QUE_ID);
         }
     }
     else
     {

          printf("Create Message Queue is exit. ID is: %d!\n",MSG_QUE_ID);
     }
}



