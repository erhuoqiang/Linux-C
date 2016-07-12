#include<stdlib.h>
#include<math.h>
#include<stdio.h>
#include<fcntl.h>          /*File control*/
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<termio.h>         /*provide the common interface*/
#include<sys/types.h>
#include<sys/stat.h>
#include<pthread.h>
#include<sys/ipc.h>
#include<sys/msg.h>

/*发送调用的函数：
 1 data_Send()
 2 data_package();
 3 crc16_check()
 4 escape_character()

接受调用的函数:
   1 data_Rev()
   2 data_process()
   3 anti_escape_character
   4 crc16_check()
   5 tsk_run*/

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned long U32;
typedef signed char INT8;
typedef unsigned int UINT16;
typedef signed int INT16;

/*用于决定是否运行调试代码*/
//#define DEBUG 1

#define FALSE -1
#define TRUE 0

/*define the device port*/
#if 0
#define DEV_PORT "/dev/pts/4"
#else
#define DEV_PORT "/dev/ttyUSB0"
#endif

/*数据属性初始化值*/
#define PROPERTY_INIT 0x11
#define PROPERTY_0x01 0x51
#define PROPERTY_0x02 0x91
#define PROPERTY_0x03 0xd1
#define PROPERTY_0x04 0x111
#define PROPERTY_0x05 0x151
#define PROPERTY_OTHER 0x155

#define DATA_LENGTH_MAX 8
#define PROTOCAL_LENGTH 9
#define BUFFER_SIZE 72
#define BAUDRATE 115200

/*控制tsk_run和data_rev    while循环的运行当接受到数据exit字符串则为1，两函数退出循环*/
U8 g_exit_flag = 0;

/*用于描述mes_que结构体的msg_type*/
#define MSG_TYPE 17
int MSG_QUE_KEY = 0;
typedef struct
{
    INT8 msg_type;
    U8 msg_len;
    /*exclude the head and tail*/
    U8 msg_buf[DATA_LENGTH_MAX + PROTOCAL_LENGTH - 2];
}msg_que;

typedef struct
{
    INT8 port_fd;
    U8 buf[BUFFER_SIZE];
}recv_param;
#if 0
typedef struct
{
    U8 package_data_length;
    /*exclude the head and tail*/
    U8 buf[DATA_LENGTH_MAX + PROTOCAL_LENGTH - 2];
}tsk_param;
#endif



/*串口发送的数据帧协议 ：帧头1  流水号2 总长度 1 属性 2 数据 X 校验和2 帧尾1*/
typedef struct
{
	U8 head[1];
	U16 serial_num;
	U8 length[1];  /*length = 一帧数据的长度 =  PROTOCAL_LENGTH(除了数据的协议长度)+（数据长度）DATA_LENGTH_MAX*/
	U16 property;
	U8 package_data[DATA_LENGTH_MAX];
	U16 crc_check;
	U8 tail[1];
}scom_protocal;

U8 serialport_init(INT8 port_fd, U32 baud_rate,  U8 data_bits, U8 parity, U8 stop_bit);
void scom_protocal_init(scom_protocal *protocal_info_ptr);  INT8 open_port(void);
int crc16_check(unsigned char *addr, int num,int crc);
U8 escape_character(U8 *package_data_ptr, U8 data_length,   U8 *buffer_ptr);
U8 anti_escape_character(U8 *buffer_ptr, U8 *package_data_ptr,  U8 data_length);
void  *tsk_run(INT8 *param_ptr);
U8 data_process(U8 *data_proc_ptr, U8 data_start_pos,  U8 data_end_pos);
U8 data_package(scom_protocal *protocal_info_ptr,  U8 *package_data_ptr, U8 data_length);
U8 data_send(INT8 port_fd, scom_protocal *protocal_info_ptr,  U8 *data_send_ptr, U8 data_send_length);
void *data_recv(recv_param *data_recv_info_ptr);  U8 close_port(INT8 port_fd);

/*如果传输的数据是ASCII 码表内 则数据的范围在0-0x7F内
则标志符可以采用0x80-0xFF的范围来表示  这样就不会出现
数据中出现和标志符一样的情况 如果数据的范围不确定则 为了
避免数据中出现标志符情况可以引入转义字符 如数据中出现0XF4
则将其转化为0XFF 0X01两个字符去发送，接收端受到转义字符则
将其还原 escape_character函数就是实现这个功能的*/
#define HEAD 0xf4     //头标志
#define TAIL 0x4f    //尾标志
#define ESC 0xff   //转义字符
#define ESC_HEAD  0x01 //HEAD 转义成ESC + ESC_HEAD
#define ESC_ESC  0x02  //ESC 转义成ESC + ESC_ESC
#define ESC_TAIL 0x03  //TAIL 转义成ESC + ESC_TAIL

/*串口数据帧初始化*/
void scom_protocal_init(scom_protocal *protocal_info_ptr)
{
	protocal_info_ptr->head[0] = HEAD;
	protocal_info_ptr->serial_num = 0x0000;
	/*init length is data_length + PROTOCAL_LENGTH*/
	protocal_info_ptr->length[0] = PROTOCAL_LENGTH + DATA_LENGTH_MAX;
	protocal_info_ptr->property = PROPERTY_INIT;

	memset(protocal_info_ptr->package_data, 0, sizeof(U8) * DATA_LENGTH_MAX);

	protocal_info_ptr->crc_check = 0x0000;
	protocal_info_ptr->tail[0] =TAIL;
}

/*串口初始化程序 设置波特率 数据位 停止位  奇偶校验位 */
U8 serialport_init(INT8 port_fd, U32 baud_rate,   U8 data_bits, U8 parity, U8 stop_bit)
{  /*参数为要设置的端口号设备，波特率，数据位，奇偶校验位，停止位*/
    struct termios newtio, oldtio;   //终端设备特性的结构体 P508

    /*save the primary params of the serial port stop_bit*/
    if (tcgetattr(port_fd, &oldtio )!= 0)  //获得终端属性给oldtio
    {
        perror("setup serial failed!!!\n");
        return -1;
    }

    bzero(&newtio, sizeof(newtio));
    /*set the data_bits stop_bit parity*/
    newtio.c_cflag |= CLOCAL | CREAD;  //CLOCAL忽略解调器状态线，意味该设备是直连的
							      //CREAD 启动接受装置
    newtio.c_cflag &= ~CSIZE;    // CSIZE设置1位关闭传输数据大小的控制 为0 则允许数据大小的控制

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
        case 'N':
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

    /*active the newtio*/
    if ((tcsetattr(port_fd, TCSANOW, &newtio)) != 0)   //设置终端属性
    {
        perror("serial port set error!!!\n");
        return -1;
    }

    printf("serial port set done!!!\n");
    return 0;
}

/*CRC校验函数
参数：addr是数据数组首地址，num是数据长度，crc是crc的初始值*/
#define CRC_INIT   0xffff
#define POLY 0x1021 // 这里省略了一个1 相当于多项式为0x11021
int crc16_check(unsigned char *addr, int num,int crc)
{
	int i;
	for (; num > 0; num--) /* Step through bytes in memory */
	{
		crc = crc ^ (*addr++ << 8); /* Fetch byte from memory, XOR into CRC top byte*/
		for (i = 0; i < 8; i++) /* Prepare to rotate 8 bits */
		{
			if (crc & 0x8000) /* b15 is set... */
				crc = (crc << 1) ^ POLY; /* rotate and XOR with polynomic */
			else /* b15 is clear... */
				crc <<= 1; /* just rotate */
		} /* Loop for 8 bits */
		crc &= 0xFFFF; /* Ensure CRC remains 16-bit value */
	} /* Loop until num=0 */
	return(crc); /* Return updated CRC */
}

/*避免数据中出现和HEAD,TAIL和转义字符标志字段同ASCII码的 采用字节填充的方法
package_data_ptr是数据帧，生成的buffer是经过转义的数据帧确保标志符
不出现在信息字段 HEAD ->ESC 0X01 TAIL->ESC 0X03 ESC->ESC 0X02*/
U8 escape_character(U8 *package_data_ptr, U8 data_length, U8 *buffer)
{
	if ((NULL == package_data_ptr) || (0 == data_length)|| (*package_data_ptr != HEAD))
	{
		printf("input error!!!\n");
		return FALSE;
	}

	U8 count = 1;

	buffer[0] = package_data_ptr[0];
	buffer++;
	/*except the head and the tail*/
	for (; count < (data_length -1); count++)
	{
		if (HEAD == package_data_ptr[count])
		{
			*buffer++ = ESC;
			*buffer++ = ESC_HEAD;
		}
		else if (ESC == package_data_ptr[count])
		{
			*buffer++ = ESC;
			*buffer++ = ESC_ESC;
		}
		else if (TAIL == package_data_ptr[count])
		{
			*buffer++ = ESC;
			*buffer++ = ESC_TAIL;
		}
		else
		{
			*buffer++ = package_data_ptr[count];
		}/*end if*/
	}/*end for*/
	*buffer++ = TAIL;
	*buffer = '\0';

	return TRUE;
}
/*反转义 HEAD<-ESC 0X01 TAIL<-ESC 0X03 ESC<-ESC 0X02
将未转义的数据包转义给package_data_ptr  data_length这里没有效果可忽略
这里加上是为了以后需要对没有HEAD和TAIL标识符的数据进行反转义*/
U8 anti_escape_character(U8 *buffer_ptr, U8 *package_data_ptr,  U8 data_length)
{
    if ((NULL == package_data_ptr) || (0 == data_length) || (*buffer_ptr != HEAD))
    {
        printf("input data error!\n");
        return FALSE;
    }

    U8 count = 0;

    package_data_ptr[0] = buffer_ptr[0];
    package_data_ptr++;
    buffer_ptr++;
    /*exclude the tail 0x4f*/
    while (TAIL != *buffer_ptr)
    {
        if ( ESC== *buffer_ptr )
        {
            buffer_ptr++;

            if ( ESC_HEAD == *buffer_ptr  )
            {
                *package_data_ptr++ = HEAD;
            }
            else if ( ESC_ESC == *buffer_ptr )
            {
                *package_data_ptr++ = ESC;
            }
            else if ( ESC_TAIL == *buffer_ptr )
            {
                *package_data_ptr++ = TAIL;
            }
            else
            {
                *package_data_ptr++ =  ESC;  //如果不是前面三种情况则说明传输过程中该位或者后一位出现的错误
									     //则可以选择对错误位操作 可以用全局变量来通知或者进一步操作
                *package_data_ptr++ = *buffer_ptr;    //这里是收下这两个可能出错的位数
            }
            buffer_ptr++;
        }
        else
        {
            *package_data_ptr++ = *buffer_ptr++;
        }/*end if*/

    }/*end while*/
    *package_data_ptr = TAIL;
    return TRUE;
}

/*打开串口号 将其设置为堵塞状态*/
INT8 open_port(void)
{
	INT8 port_fd;

	/*可读可写  非阻塞打开文件 O_NOCTTY的作用是不将设备分配位该进程的控制终端 */
	port_fd = open(DEV_PORT, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (-1 == port_fd)
	{
		perror("open the serial port failed!!!\n");
		return -1;
	}

	/*fcntl用于改变已打开文件的性质让其变为第三
	个参数，下行的作用为将文件设置为阻塞状态*/
	if (fcntl(port_fd, F_SETFL, 0) < 0)
	{
		printf("fcntl failed!\n");
	}
	else
	{
		printf("fcntl = %d \n", fcntl(port_fd, F_SETFL, 0));
	}

	return port_fd;
}
/*关闭串口*/
U8 close_port(INT8 port_fd)
{
	if (close(port_fd) < 0)
	{
		printf("close the serial port failed!\n");
		return FALSE;
	}
	else
	{
		printf("close the serial port success\n");
		return TRUE;
	}/*end if*/
}



/*消息列队接收一帧接受解析的数据msg_que类型，并执行数据对应指令 参数为对应的串口端口*/
void  *tsk_run(INT8 *param_ptr)
{
    INT8 port_fd = *param_ptr;
    U8 *exit_ptr = "exit";
#if DEBUG
    printf("in tsk_run,port_fd is %4d\n", port_fd);
#endif
    /*use to data send*/
    scom_protocal scom_protocal_info;
    memset(&scom_protocal_info, 0, sizeof(scom_protocal_info));
    scom_protocal_init(&scom_protocal_info);
    U8 data_send_length = 0;
    U8 data_send_array[32] = {0};
    U8 num = 0;
    /*use to data proc*/
    U8 package_data_ptr[DATA_LENGTH_MAX + PROTOCAL_LENGTH - 2] = {0};
    U8 data_ptr[DATA_LENGTH_MAX] = {0};
    U8 package_data_length = 0;
    /*define the var to recv msg form msg_que*/
    INT16 msg_que_id;
    INT8 ret;
    msg_que msg_que_info;
    memset(&msg_que_info, 0, sizeof(msg_que_info));
    /*proc the sub package*/
    U8 sub_package_num = 0;
    U8 sub_package_flag = 0;
    U8 sub_package_ID = 0;
    U8 sub_package_count = 0;
    U8 sub_package_buf[DATA_LENGTH_MAX * 8] = {0};
    INT16 cur_time = 0;
    INT16 last_time = 0;

    while (1)
    {
        /*g_exit_flag全局变量如果为1 退出while循环 退出tsk_run函数*/
        if(1 == g_exit_flag)
        {
            printf("int tsk_run,g_exit_flag is:%4d\n", g_exit_flag);
            break;
        }

        /*IPC_EXCL一般和IPC_CREAT一起使用，如果已存在则返会错误
	而这里适用于检查列队是否存在 存在则返回对应消息列队描述符*/
        msg_que_id = msgget(MSG_QUE_KEY, IPC_EXCL);
        if (msg_que_id <= 0)
        {
            printf("msg que is not exist!\n");
            sleep(1);
            continue;
        }
        else
        {
            printf("\nin tsk_run,msg_que_id is %4d\n", msg_que_id);
        }
        /*读取消息列队中的第一个消息，type为0 （倒数第二个参数）代表读取第一个消息
	>0读取第一个类型为type大小的消息，<0读取小于等于type绝对值大小的类型，如
	果有多个取最小的*/
        ret = msgrcv(msg_que_id, &msg_que_info, sizeof(msg_que_info), 0, 0);
        if (ret < 0)
        {
            printf("recv data from the msg que failed!\n");
            continue;
        }
        else
        {
            printf("recv data from the msg que success!\n");
        }/*end if*/

        /*用于分包接收时超时计算 如果相邻报等待超过10秒则 判断接受失败
	time函数是获取当前的秒数（从1970.1.1.0时开始到现在的秒数*/
        if (0 != last_time)
        {
            printf("*****last_time is %4d*****\n", last_time);
            cur_time = time((time_t*)NULL);
            printf("cur_time is :%4d\n", cur_time);
            if (cur_time - last_time > 10)
            {
                printf("cannot get the  complete package_data!!!\n");
                memset(sub_package_buf, 0, DATA_LENGTH_MAX * 8);
                sub_package_count = 0;
                last_time = 0;
                continue;
            }
        }/*end if*/

        package_data_length = msg_que_info.msg_len;
        memcpy(package_data_ptr, msg_que_info.msg_buf, msg_que_info.msg_len);
#if DEBUG
        printf("package_data_length is %4d\n", package_data_length);
        U8 i = 0;
        while (i < package_data_length)
        {
            printf("%02x ", package_data_ptr[i++]);
        }
        printf("\n");
#endif
	/*去掉头尾后 数据包中的3，4字节则是代表属性*/
        U16 property = 0;
        property = package_data_ptr[4];
        property = property << 8;
        property = (property | package_data_ptr[3]);
#if DEBUG
        printf("property is:%04x\n", property);
#endif
        /*判断数据是否是分包数据 属性的13-15位如果为0 代表不分包 不为0 代表分包数，
	10-12位表示当前子包的ID号  6-9位是协议类型 4-5 代表通讯属性如设备的类型等
	2 - 3 代表 数据的加密类型  0-1是校验方式 CRC是01*/
        sub_package_num = (property >> 13);
        sub_package_ID = (property >> 10);
        sub_package_ID &= 0x07;
        printf("the sub_package_ID is %4d\n", sub_package_ID);
  	/*判断是否分包*/
        if (0 != sub_package_num)
        {
            sub_package_flag = 1;
            sub_package_count++;
	    /*这里不用sub_package_ID来统计子包数的原因是，不知道发送先发拿高ID的子包还是低ID的子包
		所以sub_package_ID用来将接收的子包存入对应顺序的缓存区*/
            memcpy(&sub_package_buf[ sub_package_ID * DATA_LENGTH_MAX],
                        &package_data_ptr[5],package_data_ptr[2] - PROTOCAL_LENGTH);
            printf("sub_package_count is %4d subpackage_num is %4d\n", sub_package_count, sub_package_num);
            /*判断是否接受完所有子包*/
            if(sub_package_count == (sub_package_num + 1))
            {
#if DEBUG
                U8 i = 0;
                for (; i < 28; i++)
                {
                    printf("%c",sub_package_buf[i]);
                }
                printf("\n");
#endif
		/*system函数用来执行指令sub_package_buf*/
                system(sub_package_buf);
                memset(sub_package_buf, 0, DATA_LENGTH_MAX * 8);
                sub_package_count = 0;
                last_time = 0;
                continue;
            }
            else
            {
		/*获取接收完当前子包的时间 用于和上面的cur time一起判断是否接收超时*/
                last_time = time((time_t*)NULL);
                printf("last_time is:%4d\n",last_time);
            }/*end if*/
            continue;
        }/*end if*/
  	/*获取取掉帧头帧尾数据帧的中的数据部分给data_ptr*/
        memcpy(data_ptr, &package_data_ptr[5], package_data_ptr[2] - PROTOCAL_LENGTH);
	/*获取0-9位的属性*/
        property &= 0x3ff;
        switch (property)
        {
            case PROPERTY_INIT:
                   {
                       printf("The receive cmd is linux system cmd:%s\n", data_ptr);

                       if (0 == strcmp(exit_ptr, data_ptr))
                       {
                           g_exit_flag = 1;
                       }
                       else
                       {
                           system(data_ptr);
                       }
                       break;
                   }
            case PROPERTY_0x01:
                   {
#if DEBUG
                       printf("The receive cmd is get the GPS data:\n");
#endif
                       printf("GPS:longitude :%04x, latitude :%04x, height :%04x, time :%04x\n",
                               0xf101,0xf202, 0xf123, 0xffff);

                       num = 0;
                       data_send_array[num++] = 0x01;
                       data_send_array[num++] = 0xf1;
                       data_send_array[num++] = 0x02;
                       data_send_array[num++] = 0xf2;
                       data_send_array[num++] = 0x23;
                       data_send_array[num++] = 0xf1;
                       data_send_array[num++] = 0xff;
                       data_send_array[num++] = 0xff;
                       scom_protocal_info.property = PROPERTY_0x01;
                       data_send(port_fd, &scom_protocal_info, data_send_array, 8);
                       break;
                    }
            case PROPERTY_0x02:
                   {
                       printf("The receive cmd is get the gyroscope data:\n");
                       printf("gyroscope:\ncabrage:%04x, yaw:%04x, roll;%04x\n",
                               0xf1f2, 0xf3f4, 0xf5f6);
                       num = 0;
                       data_send_array[num++] = 0xf2;
                       data_send_array[num++] = 0xf1;
                       data_send_array[num++] = 0xf4;
                       data_send_array[num++] = 0xf3;
                       data_send_array[num++] = 0xf6;
                       data_send_array[num++] = 0xf5;

                       scom_protocal_info.property = PROPERTY_0x02;
                       data_send(port_fd, &scom_protocal_info, data_send_array, num);
                       break;
                   }
            case PROPERTY_0x03:
                   {
                       printf("The receive cm d is get the accelerometer data:\n");
                       printf("accelerometer:\nX:%04x, Y:%04x, Z:%04x\n",
                               0x0102, 0x0304, 0x0506);
                       num = 0;
                       data_send_array[num++] = 0x02;
                       data_send_array[num++] = 0x01;
                       data_send_array[num++] = 0x04;
                       data_send_array[num++] = 0x03;
                       data_send_array[num++] = 0x06;
                       data_send_array[num++] = 0x05;

                       scom_protocal_info.property = PROPERTY_0x03;
                       data_send(port_fd, &scom_protocal_info, data_send_array, num);
                       break;
                   }
            case PROPERTY_0x04:
                   {
                       printf("The receive cmd is get th e target info:\n");

                       num = 0;
                       data_send_array[num++] = 0x01;
                       data_send_array[num++] = 0x34;
                       data_send_array[num++] = 0x12;
                       data_send_array[num++] = 0x21;
                       data_send_array[num++] = 0x43;
                       data_send_array[num++] = 0x02;
                       data_send_array[num++] = 0x78;
                       data_send_array[num++] = 0x56;
                       data_send_array[num++] = 0x65;
                       data_send_array[num++] = 0x87;
                       data_send_array[num++] = 0x03;
                       data_send_array[num++] = 0x14;
                       data_send_array[num++] = 0x23;
                       data_send_array[num++] = 0x21;
                       data_send_array[num++] = 0x34;

                       scom_protocal_info.property = PROPERTY_0x04;
                       data_send(port_fd, &scom_protocal_info, data_send_array, num);
                       break;
                   }
            case PROPERTY_0x05:
                   {
                       printf("the cmd  is get the D5 info:\n");

                       num = 0;
                       data_send_array[num++] = 0x01;
                       data_send_array[num++] = 0x34;
                       data_send_array[num++] = 0x12;
                       data_send_array[num++] = 0x21;
                       data_send_array[num++] = 0x43;
                       data_send_array[num++] = 0x02;
                       data_send_array[num++] = 0x78;
                       data_send_array[num++] = 0x56;
                       data_send_array[num++] = 0x65;
                       data_send_array[num++] = 0x87;
                       data_send_array[num++] = 0x03;
                       data_send_array[num++] = 0x14;
                       data_send_array[num++] = 0x23;
                       data_send_array[num++] = 0x21;
                       break;
                   }
            default:
                   {
                       printf("cannot discrimate the cmd:\n");

                       U8 i = 0;
                       for (; i < package_data_ptr[2] - PROTOCAL_LENGTH; i++)
                       {
                            printf("%02x ", data_ptr[i]);
                       }
                       printf("\n");
                       break;
                   }
        }/*end switch*/
    }/*end ehile*/
}




/*将 data_recv接受到的的数据包根据HEAD和TAIL格式分组，如果数据包的数据包含3.5组
，则将前三组反转义，然后CRC校验（对数据帧去HEAD和TAIL后校验），最后根据数据
和长度构成mes_que结构体传递给消息列队在tsk_run中进一步处理。剩下的0.5则返回
0.5组HEAD的位置给data_rev，下次接受完整后在传入处理。
参数：data_proc_ptr是数据包首地址，data_start_pos 从数据包的这个位置开始处理，
data_end_pos代表数据结束为止*/
U8 data_process(U8 *data_proc_ptr,  U8 data_start_pos, U8 data_end_pos)
{
    if ((NULL == data_proc_ptr) || (data_start_pos >= data_end_pos))
    {
        printf("input data error!\n");
        return 0;
    }

    /*use msg_que to do the ipc*/
    msg_que msg_que_info;
    INT8 ret = 0;
    INT16 msg_que_id;
    memset(&msg_que_info, 0, sizeof(msg_que_info));
    msg_que_id = msgget((key_t)MSG_QUE_KEY, IPC_EXCL);
#if DEBUG
    printf("the msg_que_id is:%4d\n", msg_que_id);
#endif

  /*buffer用于存放一帧数据反转义前的内容，考虑到除了HEAD和TAIL，其他数据都被转义
   成2个字符的可能所以长度是(DATA_LENGTH_MAX + PROTOCAL_LENGTH) * 2  - 2*/
    U8 buffer[(DATA_LENGTH_MAX + PROTOCAL_LENGTH) * 2  - 2] = {0};
    /*package_data用来存放反转义后的数据，DATA_LENGTH_MAX + PROTOCAL_LENGTH
	大小足够   但是由于担心传输过程中有数据出错，2个转义字符并没有变成1个，所以就多申请了一倍*/
    U8 package_data[(DATA_LENGTH_MAX + PROTOCAL_LENGTH) * 2] = {0};


    U8 head_flag = 0;
    U8 tail_flag = 0;
    U8 pos = 0;
    U8 package_data_length = 0;
   /*从data_start_pos到data_end_pos可能有很多组*/
    for (; data_start_pos < data_end_pos; data_start_pos++)
    {
        /*根绝HEAD和TAIL 得到一帧数据的开始和结束位置*/
        if ((HEAD == data_proc_ptr[data_start_pos])
                && (0 == head_flag))
        {
            head_flag = 1;
            pos = data_start_pos;
            continue;
        }
        /*avoid the double head or triple head*/
        if ((HEAD == data_proc_ptr[data_start_pos])
                && (1 == head_flag))
        {
            pos = data_start_pos;
        }

        if ((TAIL == data_proc_ptr[data_start_pos])
                && (0 == tail_flag))
        {
            if(1 == head_flag)
            {
                tail_flag = 1;
            }
            else
            {
                tail_flag = 0;
            }
        }

        /*head_flag 和 tail_flag为1则代表接受完一帧数据*/
        if ((1 == head_flag) && (1 == tail_flag))
        {
            printf("data_start_pos is %2d, pos is %2d ", data_start_pos, pos);
            memset(buffer, 0x00, (DATA_LENGTH_MAX + PROTOCAL_LENGTH) * 2 - 2);
	       /*将数据帧拷贝给buffer*/
	    memcpy(buffer, &data_proc_ptr[pos],    (data_start_pos - pos + 1));

            /*anti escape character*/
            printf("\nanti escape character! data_start_pos is %4d pos is %4d\n",
                    data_start_pos, pos);
            memset(package_data, 0x00, (DATA_LENGTH_MAX + PROTOCAL_LENGTH) * 2);
            /*buffer反转义后的数据给package_data*/
	    anti_escape_character(buffer, package_data,  (data_start_pos - pos + 1));

#if DEBUG
            U8 m = 0;
            for (; m < 17; m++)
            {
                printf("%02x ", package_data[m]);
            }
            printf("\n");
#endif
            /*data length exclude head and tail*/
#if DEBUG
            printf("data length is: package[3] = %2d\n", package_data[3] - 2);
#endif
	    /*数据帧的第4个字节是长度*/
            package_data_length = package_data[3] - 2;
		/*CRC校验不包括HEAD和TAIL*/
            printf("crc16_check is 0x%04x\n",crc16_check(&package_data[1], package_data_length,CRC_INIT));
            if (0x00 == crc16_check(&package_data[1], package_data_length,CRC_INIT))
            {
                printf("crc16_check success!\n");
            }
            else
            {
                printf("crc16_ check error!\n");
                /*
                 * theoretically,it will return the serial_num
                 * and the sub_package_ID
                 */
                return 0;
            }
            /*将不包括HEAD和TAIL的的数据包给msg_que_info.buf msg_type消
		息类型成员在程序中并没有什么作用 因为数据的类型标志放在了串口
		结构体中的属性中，不过可以用来以后根据需求扩充		*/
            msg_que_info.msg_type = MSG_TYPE;
            msg_que_info.msg_len = package_data_length;
#if DEBUG
            printf("msg_que_info.msg_len is %4d\n",msg_que_info.msg_len);
#endif
            memcpy(&(msg_que_info.msg_buf), &package_data[1],package_data_length);
            /*将msg_que结构体传递给消息列队 在tsk_run中处理*/
            ret = msgsnd(msg_que_id, &msg_que_info, package_data_length, IPC_NOWAIT);
            if (ret < 0)
            {
                printf("msg send failed!\n");
                return 0;
            }
            else
            {
                printf("send msg success! ret is %4d\n", ret);
                sleep(2);
            }

            head_flag = 0;
            tail_flag = 0;
            pos = 0;
        }
    }
#if 0
    printf("\nreturn data is: %4d\n", pos);
#endif
   /*如果存在0.5组这类情况这这里返回0.5组的起始位置，否则返回0*/
    return pos;
}


/*************************************************
 * Function: data_package()
 * Description: package the data with the scom_protocal
 * Calls:
 * Called By: data_send
 * Input: protocal_info_ptr, package_data, data_length
 * Output:
 * Return: TRUE/FALSE
 * Author: xhniu
 * History: <author> <date>  <desc>
 * Others: none
*************************************************/
U8 data_package(scom_protocal *protocal_info_ptr,
        U8 *package_data_ptr, U8 data_length)
{
    /*decide whether need subpackage*/
    if (data_length > DATA_LENGTH_MAX)
    {
        printf("input valid!!!\n");
        return -1;
    }

    U8 len = 0;

    package_data_ptr[0] = protocal_info_ptr->head[0];
    /*U16 to U8 low 8 bit*/
    package_data_ptr[1] = (U8)(protocal_info_ptr->serial_num);
    package_data_ptr[2] = (U8)((protocal_info_ptr->serial_num)
            >> 8);

    package_data_ptr[3] = data_length + PROTOCAL_LENGTH;

    /*U16 to U8 low 8 bit*/
    package_data_ptr[4] = (U8)(protocal_info_ptr->property);
    /*high 8 bit*/
    package_data_ptr[5] = (U8)((protocal_info_ptr->property)
            >> 8);

    for (; len < data_length; len++)
    {
        package_data_ptr[len + 6] =
            protocal_info_ptr->package_data[len];
    }
    /*
     *generate the CRC16 check data
     * U16 to U8
     */
    U16 check_code;
    /*data_length + property(2 byte) + length(1 byte) +
     * serial_num(2 byte)
     */
    U8 *ptr = malloc(sizeof(U8) * (data_length + 5));
    memcpy(ptr, package_data_ptr + 1, data_length + 5);
    check_code = crc16_check(ptr, data_length + 5,CRC_INIT);
#if  DEBUG
     printf("crc16_check is 0x%04x\n",check_code);
#endif // DEBUG
    /*free the ptr*/
    free(ptr);
    ptr = NULL;
 /*这里尤其要注意CRC得到的校验码应该由高位到低位，自左向右存放在数据包*/
    /*高位先放*/
    package_data_ptr[len + 6] = (U8)(check_code>>8);
    /*在放低位*/
    package_data_ptr[len + 7] = (U8)(check_code );
    package_data_ptr[len + 8] = protocal_info_ptr->tail[0];

    return TRUE;
}
U8 data_send(INT8 port_fd, scom_protocal *protocal_info_ptr,U8 *data_send_ptr, U8 data_send_length)
{
    if (NULL == data_send_ptr)
    {
        printf("input data error!!!\n");
        return FALSE;
    }

    /*number of the char need escape*/
    U8 num_escape_char = 0;
    U8 num_escape_char_temp = 0;

    U8 *package_data = NULL;
    U8 *escape_char_ptr = NULL;

    /*when send new data, the serial num will plus 1*/
    (protocal_info_ptr->serial_num)++;

    if (data_send_length > DATA_LENGTH_MAX)
    {
        /*need the subpackage calculate the sun_package_num
         * set the flag of the subpackage*/
        int sub_package_num = 0, count = 0;
        if (0 != data_send_length % DATA_LENGTH_MAX)
        {
            sub_package_num = (data_send_length / DATA_LENGTH_MAX) + 1;
        }
        else
        {
            sub_package_num = (data_send_length / DATA_LENGTH_MAX);
        }

        /*3 byte of the sub_package_num the data area is
         *1 ~ 7
         */
        if (sub_package_num > 8)
        {
            return FALSE;
        }
        protocal_info_ptr->property &= 0x1fff;
        protocal_info_ptr->property |= ((sub_package_num - 1) << 13);
#if DEBUG
        printf("sub_package_num is:%4x\n",sub_package_num);
#endif
        /*
         * sub_package_num - 1
         * the length of the last sub_package is not sure
         */
        for (; count < (sub_package_num - 1); count++)
        {
            /*set the length of the protcal_info_ptr->length[0]*/
            protocal_info_ptr->length[0] =
                DATA_LENGTH_MAX + PROTOCAL_LENGTH;
            /*set the ID of the subpackage*/
            protocal_info_ptr->property &= 0xe3ff;
            protocal_info_ptr->property |= (count << 10);

            memcpy(protocal_info_ptr->package_data,
                    (data_send_ptr + count * DATA_LENGTH_MAX ),
                    DATA_LENGTH_MAX);
#if 0
            U8 m = 0;
            for (; m < protocal_info_ptr->length[0]; m++)
            {
                printf("%02x ", protocal_info_ptr->package_data[m]);
            }
            printf("\n");
#endif
            /*malloc a mem to restore the packaged data
             *plus the head(1),tail(1),length(1),property(2),crc(2),serial_num(2),
             */
            package_data = (U8 *)malloc(sizeof(U8) *
                    (DATA_LENGTH_MAX + PROTOCAL_LENGTH));
            if (NULL == package_data)
            {
                printf("malloc failed\n");
                return FALSE;
            }

            data_package(protocal_info_ptr, package_data,
                    DATA_LENGTH_MAX);
#if DEBUG
            U8 m = 0;
            for (; m < 17; m++)
            {
                printf("%02x ", package_data[m]);
            }
            printf("\n");
#endif
            /* malloc a buffer use to send data
             * character transfer
             */
            num_escape_char = 0;
            num_escape_char_temp = 0;
            /*count the num of the 0xff 0xf4 0x4f except tail and head*/
            num_escape_char_temp = protocal_info_ptr->length[0] - 2;

            while ((num_escape_char_temp--) > 1)
            {
                if ((0xff == package_data[num_escape_char_temp])
                        | (0xf4 == package_data[num_escape_char_temp])
                        | (0x4f == package_data[num_escape_char_temp]))
                {
                    num_escape_char++;
                }
            }
#if DEBUG
            printf("num_escape_char is %4d\n", num_escape_char);
#endif
            if(0 == num_escape_char)
            {
                /*no escape characters send data diretly*/
                if (write(port_fd, package_data,
                        (protocal_info_ptr->length[0])) < 0)
                {
                    printf("write data error! \n");
                    return FALSE;
                }
                else
                {
                    printf("write data success!\n");
                }
            }
            else
            {
                escape_char_ptr = (U8 *)malloc(sizeof(U8)
                        * (protocal_info_ptr->length[0] + num_escape_char));
#if DEBUG
                printf("%4d \n", protocal_info_ptr->length[0]);
#endif
                escape_character(package_data,
                        protocal_info_ptr->length[0], escape_char_ptr);
#if DEBUG
                U8 k = 0;
                for (; k < protocal_info_ptr->length[0]
                        + num_escape_char; k++)
                {
                    printf("%04x ", escape_char_ptr[k]);
                }
#endif
                /*send data*/
                if (write(port_fd, escape_char_ptr,
                        (protocal_info_ptr->length[0] + num_escape_char)) < 0)
                {
                    printf("write data error! \n");
                    return FALSE;
                }
                else
                {
                    printf("write data success!\n");
                }

                /*free the memory*/
                free(escape_char_ptr);
                escape_char_ptr = NULL;
            }

            free(package_data);
            package_data = NULL;

        }/*end for*/
        if (data_send_length - count * DATA_LENGTH_MAX > 0)
        {
            protocal_info_ptr->length[0] = data_send_length
                - count * DATA_LENGTH_MAX + PROTOCAL_LENGTH;

            protocal_info_ptr->property &= 0xe3ff;
            protocal_info_ptr->property |= (count << 10);
            memcpy(protocal_info_ptr->package_data,
                    (data_send_ptr + count * DATA_LENGTH_MAX),
                    (data_send_length - count * DATA_LENGTH_MAX));
            /*malloc a mem to restore the packaged data*/
            package_data = (U8 *)malloc(sizeof(U8) * (data_send_length
                        - count * DATA_LENGTH_MAX + PROTOCAL_LENGTH));
            if (NULL == package_data)
            {
                printf("malloc failed \n");
            }
#if DEBUG
            printf("the rest data length is %4d\n",
                    data_send_length - count * DATA_LENGTH_MAX );
#endif

            data_package(protocal_info_ptr, package_data,
                    data_send_length - count * DATA_LENGTH_MAX);
#if DEBUG
            U8 n = 0;
            for (; n < protocal_info_ptr->length[0]; n++)
            {
                printf("%02x ", package_data[n]);
            }
            printf("\n");
#endif
            /* malloc a buffer use to send data
             * character transfer
             */
            num_escape_char = 0;
            num_escape_char_temp = 0;
            /*count the num of the 0xff 0xf4 0x4f except tail and head*/
            num_escape_char_temp = protocal_info_ptr->length[0] - 2;

            while ((num_escape_char_temp--) > 1)
            {
                if ((0xff ==  package_data[num_escape_char_temp])
                        | (0xf4 == package_data[num_escape_char_temp])
                        | (0x4f == package_data[num_escape_char_temp]))
                {
                    num_escape_char++;
                }
            }
#if DEBUG
            printf("num_escape_char is %4d\n", num_escape_char);
#endif
            if(0 == num_escape_char)
            {
                /*no escape characters send data diretly*/
                if (write(port_fd, package_data,
                        (protocal_info_ptr->length[0])) < 0)
                {
                    printf("write data error! \n");
                    return FALSE;
                }
                else
                {
                    printf("write data success!\n");
                }
            }
            else
            {
                escape_char_ptr = (U8 *)malloc(sizeof(U8)
                        * (protocal_info_ptr->length[0] + num_escape_char));
#if DEBUG
                printf("%4d \n", protocal_info_ptr->length[0]);
#endif
                escape_character(package_data,
                        protocal_info_ptr->length[0], escape_char_ptr);
#if DEBUG
                U8 k = 0;
                for (; k < protocal_info_ptr->length[0]
                        + num_escape_char; k++)
                {
                    printf("%04x ", escape_char_ptr[k]);
                }
#endif
            /*send data*/
#if 1
                if (write(port_fd, escape_char_ptr,
                        (protocal_info_ptr->length[0] + num_escape_char)) < 0)
                {
                    printf("write data error!\n");
                    return FALSE;
                }
#endif
                free(escape_char_ptr);
                escape_char_ptr = NULL;
                /*send data*/
            }

            /*free the memory*/
            free(package_data);
            package_data = NULL;

            return TRUE;
            /**/
        }
        else/* data_send_length - count * DATA_LENGTH_MAX = 0 */
        {
            return TRUE;
        }/*end if*/
    }
    else /*data_send_length <= DATA_LENGTH_MAX no subpackage*/
    {
        /*set the length of tht protocal_info_ptr->length[0]*/
        protocal_info_ptr->length[0] = data_send_length + PROTOCAL_LENGTH;

        memcpy(protocal_info_ptr->package_data, data_send_ptr, data_send_length);
        /*malloc a mem to restore the packaged data*/
        package_data = (U8 *)malloc(sizeof(U8)
                * (data_send_length + PROTOCAL_LENGTH));
#if DEBUG
        printf("data_send_length is %4d\n", data_send_length);
#endif
        if (NULL == package_data)
        {
            printf("malloc failed!!!\n");
            return FALSE;
        }
#if DEBUG
        printf("data_send_length + PROTOCAL_LENGTH is %4d \n",
                data_send_length + PROTOCAL_LENGTH);
#endif
        data_package(protocal_info_ptr, package_data, data_send_length);

#if DEBUG
        printf("protocal_info_ptr_length[0] is %4d \n", protocal_info_ptr->length[0]);
        U8 j = 0;
        for (; j < data_send_length + PROTOCAL_LENGTH; j++)
        {
            printf("%02x ", package_data[j]);
        }
        printf("\n");
#endif
        /* malloc a buffer use to send data
         * character transfer
         */
        U8 num_escape_char = 0;
        U8 num_escape_char_temp = 0;
        /*count the num of the 0xff 0xf4 0x4f except tail and head*/
        num_escape_char_temp = protocal_info_ptr->length[0] - 2;
#if 0
        printf("%4d\n", num_escape_char_temp);
#endif
        while ((num_escape_char_temp--) > 1)
        {
            if ((0xff == package_data[num_escape_char_temp])
                    | (0xf4 == package_data[num_escape_char_temp])
                    | (0x4f == package_data[num_escape_char_temp]))
            {
                num_escape_char++;
            }/*end if*/
        }/*end while*/
#if DEBUG
        printf("num_escape_char is %4d\n", num_escape_char);
#endif
        if(0 == num_escape_char)
        {
            /*no escape characters send data diretly*/
            if (write(port_fd, package_data,
                        protocal_info_ptr->length[0]) < 0)
            {
                printf("write data error!!!\n");
                return FALSE;
            }
            else
            {
                printf("write to port_fd data success!\n");
            }

            /*free the malloc*/
            free(package_data);
            package_data = NULL;
        }/*end if*/
        else
        {
            escape_char_ptr = (U8 *)malloc(sizeof(U8)
                    * (protocal_info_ptr->length[0] + num_escape_char));
#if DEBUG
            printf("%4d \n", protocal_info_ptr->length[0]);
#endif
            escape_character(package_data,
                    protocal_info_ptr->length[0], escape_char_ptr);
#if DEBUG
            U8 k = 0;
            for (; k < protocal_info_ptr->length[0]
                    + num_escape_char; k++)
            {
                printf("%04x ", escape_char_ptr[k]);
            }
            printf("\nport_fd is %4d\n", port_fd);
#endif
            /*send data*/
            INT8 res = 0;
#if 1
            printf("writing data......\n");
            /*escape_char_ptr */
            res = write(port_fd, escape_char_ptr,
                        (protocal_info_ptr->length[0] + num_escape_char));
#if DEBUG
            printf("res is %2d\n", res);
#endif
            if (res < 0)
            {
                printf("write data error!\n");
                return FALSE;
            }
            printf("write data end!!!\n");
#endif
            free(escape_char_ptr);
            escape_char_ptr = NULL;
        }/*end else*/

        /*free the memory*/
        free(package_data);
        package_data = NULL;
    }/*end else*/

    return TRUE;
}
/*************************************************
 * Function: data_recv()
 * Description:  the data and
 * Calls: none
 * Called By: none
 * Input: none
 * Output: none
 * Return:
 * Author: xhniu
 * History: <author> <date>  <desc>
 * Others: none
************************************************/
void *data_recv(recv_param *data_recv_info_ptr)
{
    INT8 port_fd = data_recv_info_ptr->port_fd;
    U8 *data_recv_buf_ptr = data_recv_info_ptr->buf;
#if DEBUG
    printf("port_fd is %4d\n", data_recv_info_ptr->port_fd);
#endif
    U8 pos = 0;
    U8 len = 0;
    U8 data_proc_pos = 0;
    U8 data_start_pos = 0;
    U8 data_new_flag = 0;
#if DEBUG
    printf("start to read data!\n");
#endif
    while(1)
    {
        /*judge the exit flag*/
        if (1 == g_exit_flag)
        {
            printf("in data_recv thread,g_exit_flag%4d\n", g_exit_flag);
            break;
        }
        sleep(1);
#if DEBUG
        printf("pos is %4dlen is %4d\n", pos, len);
#endif
        len = read(port_fd, &data_recv_buf_ptr[pos], (BUFFER_SIZE - pos));
        printf("len is %4d\n", len);
        if (len > 0)
        {
            printf("len is %4d ", len);
            pos += len;
            data_new_flag = 1;
            continue;
        }
        else if (0 == data_new_flag && 0 == len)
        {
            printf("no new data come......\n");
            continue;
        }/*end if*/

        /* receiving data */
        if ((0 == len) && (pos < BUFFER_SIZE) &&(1 == data_new_flag))
        {
            /*start to process data*/
#if DEBUG
            printf("data_start_pos is%2d pos is %3d\n", data_start_pos, pos);
#endif
            data_proc_pos = data_process(data_recv_buf_ptr, data_start_pos, pos);
            if (data_proc_pos > 0)
            {
                data_start_pos = data_proc_pos;
            }
            else
            {
                /*//这里可以改成data_Start_pos = 0; pos = 0;当完整接受后可以从头接受数据减少进入BUFFER_SIZE == pos的IF中*/
                data_start_pos = pos;
            }
            printf("data_proc_pos is %3d\n", data_proc_pos);
            data_new_flag = 0;
        }

        if (BUFFER_SIZE == pos)
        {
#if DEBUG
            printf("stop recv data, data_start_pos is %2d pos is %4d\n",
                    data_start_pos, pos);
#endif
            data_proc_pos = data_process(data_recv_buf_ptr, data_start_pos, pos);

            printf("data_proc_pos is %4d\n", data_proc_pos);

            memcpy(&data_recv_buf_ptr[0], &data_recv_buf_ptr[data_proc_pos],
                    (BUFFER_SIZE - data_proc_pos));

            pos = (BUFFER_SIZE - data_proc_pos);
            data_start_pos = 0;
        }/*end if*/

    }/*end while*/
}/*end function*/
/*************************************************
 * Function: tsk_thread_create()
 * Description:  create a thread for a tsk
 * Calls: main
 * Called By: pthread_create
 * Input: task
 * Output: error infomation
 * Return: TURE?FALSE
 * Author: xhniu
 * History: <author> <date>  <desc>
 * Others:
************************************************/
pthread_t tsk_thread_create(void *(*start_routine)(void *),void *arg)
{
    INT8 ret = 0;
    /*create a msg_que*/
    msg_que msg_que_info;
    INT16 msg_que_id;
    key_t key;
    key = (key_t)MSG_QUE_KEY;
    /*judge the msg que is exist*/
    msg_que_id = msgget(MSG_QUE_KEY, IPC_EXCL);
#if 1
    printf("%4d\n", msg_que_id);
#endif
    if (msg_que_id <= 0)
    {
        /*create the msg_que*/
        printf("the msg que is not exist!\n");
        msg_que_id = msgget(key, IPC_CREAT | 0666);
        if (msg_que_id < 0)
        {
            printf("create the msg que failed!\n");
            return FALSE;
        }
        else
        {
            printf("create the msg que success,and the msg_que_id is:%4d\n",
                    msg_que_id);
        }
    }
    else
    {
        printf("the msg_que is exist the msg_que_id is:%4d\n", msg_que_id);
    }/*end if*/
    pthread_t thread_id;

    ret = pthread_create(&thread_id, NULL, (void *)*start_routine, arg);
    if (FALSE == ret)
    {
        perror("cannot create a tsk!!!\n");
        return FALSE;
    }
    else
    {
        printf("new tsk create success! thread_id is %4d\n", (U16)thread_id);
    }

    return thread_id;
}
/*************************************************
 * Function: tsk_thread_delete()
 * Description:  delete a thread
 * Calls: main
 * Called By: pthread_exit()
 * Input:
 * Output: none
 * Return:
 * Author: xhniu
 * History: <author> <date>  <desc>
 * Others: none
************************************************/
U8 tsk_thread_delete(void)
{
    /*free the related system resource*/
    INT16 msg_que_id = 0;
    msg_que_id = msgget(MSG_QUE_KEY, IPC_EXCL);
    if (msg_que_id < 0)
    {
        printf("msg que is not exist!\n");
        return TRUE;
    }
    else
    {
        if(msgctl(msg_que_id, IPC_RMID, 0) < 0)
        {
            printf("delete msg_que failed!\n");
            return FALSE;
        }
        else
        {
            printf("delete msg_que: %4d success\n",msg_que_id);
            return TRUE;
        }/*end if*/
    }/*end if*/
}

int my_strlen(const char *src)
{
    const  char  * temp  = src;
    if(src == NULL)
        return -1;
    while(*src++);
    return src  -  temp  - 1;
}
int main()
{
    char data[72] = "";
    scom_protocal scom; //定义协议结构
    recv_param rev_data; //接受数据存放的结构体
    INT8  port_fd = 0;
    port_fd = open_port();//串口设备的文件描述符
    serialport_init(port_fd, 115200,8,'N', 1);//设置设备串口属性波特率停止位 数据位 校验位
    scom_protocal_init(&scom);  //  初始化串口协议结构体
    rev_data.port_fd = port_fd;
    MSG_QUE_KEY = ftok("./",5);
    tsk_thread_create((void *(*)(void *))data_recv, (void *)&rev_data);
     tsk_thread_create((void *(*)(void *))tsk_run, (void *)&port_fd);
    while(1)
    {
        printf("input command:\n");
        fgets(data, sizeof(data), stdin);
        data_send(port_fd,&scom,data,my_strlen(data));
    }
}
