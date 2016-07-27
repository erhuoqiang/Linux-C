#include"uart.h"

volatile int MSG_QUE_KEY = 0; //KEY VALUE
volatile int MSG_QUE_ID = 0;  //msg_id

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
        printf("crc16_check parameter error! \n");
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

    package->crc_fsc = CRC16_Check((U8 *)package + 1,data_length + 3,CRC_INIT);
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
		printf("Input error!\n");
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


int Send_data(INT8 port,Data_Protocal * package)
{
    int i = 0, escape_count = 0, len = 0;
    U8 * send_buf = NULL;
    U8 * package_buf = (U8*)malloc(package->length[0]);

    if(package == NULL)
    {
        printf("Input error!\n");
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

        if( write(port, send_buf,package->length[0] + escape_count) == -1)
        {
            printf("Send data failed!\n");
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
        if( write(port, package_buf,package->length[0]) == -1)
        {
            printf("Send data failed!\n");
            free(send_buf);
            free(package_buf);
            return -1;
        }
#ifdef DEBUG
        printf("Send data success!\n");
#endif
    }

    return 0;
}

/*****************REV PART********************/

/*反转义 HEAD<-ESC 0X01 TAIL<-ESC 0X03 ESC<-ESC 0X02
rev_buf是接收到还没有解析的数据，data_package是解析后数据存放的数组*/
INT8 Reverse_Escape_Character(U8 *rev_buf, U8 *data_package)
{
    U8 count = 0;

    if (NULL == data_package|| NULL == rev_buf)
    {
		printf("Input error!\n");
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
                msg_buf.msg_type = package[2];
                msg_buf.data[0]  =  package[3];
                //MSG_QUE_ID = msgget(MSG_QUE_KEY,IPC_EXCL);
              //  printf("%d\n",MSG_QUE_ID);
                err = msgsnd(MSG_QUE_ID, (void *)&msg_buf, sizeof(msg_buf) - sizeof(long), IPC_NOWAIT);
                if(err < 0)
               {
                   printf("Mssage queue send data failed!\n");
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
                printf("crc check is failed!\n");
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

void Data_Rev(rev_pthread_param * rev_param)
{
    U8 rev_buf[512];
    int data_end_pos =  0;
    int data_start_pos = 0;
    int process_pos = 0;
    int len = 0;

    while(1)
    {
	/*读取串口数据*/
        len = read(rev_param->port_id, rev_param->buf, REV_BUFFER_SIZE - data_end_pos);
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


/*打开串口号 将其设置为堵塞状态*/
INT8 Open_Port(const char * DEV)
{
	INT8 port_fd;

	/*可读可写  非堵塞打开文件 O_NOCTTY的作用是不将设备分配为该进程的控制终端 */
	port_fd = open(DEV, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (-1 == port_fd)
	{
		printf("open the serial port failed!\n");
		return -1;
	}
    printf("Open the  port success!\n");

	return port_fd;
}

/*关闭串口*/
INT8 Close_Port(INT8 port_fd)
{
	if (close(port_fd) < 0)
	{
		printf("close the serial port failed!\n");
		return -1;
	}
	else
	{
		printf("close the serial port success\n");
		return  0;
	}
}

/*串口初始化程序 参数为要设置的端口号设备，波特率，数据位，奇偶校验位，停止位*/
U8 Serial_Init(INT8 port_fd, U32 baud_rate, U8 data_bits, U8 parity, U8 stop_bit)
{
    struct termios newtio;   //终端设备特性的结构体

    memset(&newtio, 0, sizeof(newtio));
    /*set the data_bits stop_bit parity*/
    newtio.c_cflag |= CLOCAL | CREAD;  //CLOCAL忽略解调器状态线，意味该设备是直连的
                                      //CREAD 启动接受装置
    newtio.c_cflag &= ~CSIZE;        // CSIZE设置1位关闭传输数据大小的控制 为0 则允许数据大小的控制

    switch (data_bits)
    {
        case 7:
            newtio.c_cflag |= CS7;   //CSIZE 为0 开启数据位控制后，这里设置数据位为7位
            break;
        case 8:
            newtio.c_cflag |= CS8;
            break;
        default:
            break;
    }

    switch (parity)
    {
        case 'O':/*odd number*/
            newtio.c_cflag |= PARENB;   //parity enable 奇偶校验位产生和检测的使能端 为1开启
            newtio.c_cflag |= PARODD;   // 为1是奇校验 0 偶校验
            newtio.c_iflag |= (INPCK | ISTRIP); //为1 使输入奇偶校验起作用，如果输入数据奇偶校验是错的
								   //则检查IGNPAR，来决定是否忽略出错的输入字节
								//ISTRIP设置时有效输入字节被剥离为7位
            break;
        case 'E':/*even number*/
            newtio.c_iflag |= (INPCK | ISTRIP);
            newtio.c_cflag |= PARENB;
            newtio.c_cflag &= ~PARODD;
            break;
        case 'N':/*none number*/
            newtio.c_cflag &= ~PARENB;
            break;
        default:
            break;
    }

    switch (baud_rate)
    {
        case 9600:
            cfsetispeed(&newtio, B9600);   //设置波特率位B9600，B9600是一个宏定义
            cfsetospeed(&newtio, B9600);
            break;
        case 115200:
            cfsetispeed(&newtio, B115200);
            cfsetospeed(&newtio, B115200);
            break;
        default:
            break;
    }

    if (1 == stop_bit)
    {
        newtio.c_cflag &= ~CSTOPB;   //CSTOPB为0 是一位停止位 为1是两位停止位
    }
    else if (2 == stop_bit)
    {
        newtio.c_cflag |= CSTOPB;
    }

     //设置终端属性 TCSANOW不等数据传输完立马生效属性
    if ((tcsetattr(port_fd, TCSANOW, &newtio)) != 0)
    {
        perror("serial port set error!!!\n");
        return -1;
    }

    printf("Port %d init success!\n",port_fd);
    return 0;
}

/*READ data from message queue*/
INT8 Read_MSG_QUE()
{
    msg_que msg_buf;
    int err = 0;
    err = msgrcv(MSG_QUE_ID, (void *)&msg_buf,  sizeof(msg_buf) - sizeof(long),0, 0);
    if(err <= 0)
    {
	printf("Mssage queue rev data failed!\n");
    }
    else
    {
#ifdef RESULT
	printf("Mssage queue rev data success!\n");
	printf("REV MSG_TYPE is :%ld\n DATA is:%d\n", msg_buf.msg_type, msg_buf.data[0]);
#endif
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

/*strlen by myself*/
int My_Strlen(const char * src)
{
    const char * temp = src;
    if(src == NULL)
        return -1;
    while(*src++);
    return src - temp - 1;
}

