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

/*发送功能需要调用的函数：
 1 data_Send()
 2 data_package();
 3 crc16_check()
 4 escape_character()

接收功能需要调用的函数:
   1 data_Rev()
   2 data_process()
   3 anti_escape_character
   4 crc16_check()
   5 tsk_run
   */

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

/*数据属性初始化值   属性的13-15位如果为0 代表不分包 不为0 代表分包数，
10-12位表示当前子包的ID号  6-9位是协议类型 4-5 代表通讯属性如设备的类
型等2 - 3 代表 数据的加密类型  0-1是校验方式 CRC是01*/
#define PROPERTY_INIT 0x11
#define PROPERTY_0x01 0x51
#define PROPERTY_0x02 0x91
#define PROPERTY_0x03 0xd1
#define PROPERTY_0x04 0x111
#define PROPERTY_0x05 0x151
#define PROPERTY_OTHER 0x155

/*一次传输数据的最大长度   因为scom_protocal中length只分配一个字节 所以最大长度为255 这里改了缓存区BUFFER_SIZE
也要改 一般为DATA_LENGTH_MAX 9倍*/
#define DATA_LENGTH_MAX 8
#define PROTOCAL_LENGTH 9  //scom_protocal结构体的除去数据长度后的协议长度
/*接收端的缓存区大小 也就是rev_data的BUF大小，大小最小应该大于等于
(DATA_LENGTH_MAX +PROTOCAL_LENGTH)*2 -2 接受一帧数据最大的长度，
这样才可以放的下一帧转义后的数据包，其长度是考虑到数据中的除了HEAD
和TAIL其他的每个字符都需要转义成2两个字符  其长度最好是接受的数据帧最
大长度的4-8倍左右 */
#define BUFFER_SIZE 72
#define BAUDRATE 115200

/*控制tsk_run和data_rev   可以理解为对数据接受和处理的使能端  while循环
的运行当接受到数据exit字符串则为1，两函数退出循环*/
volatile U8 g_exit_flag = 0;

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
	protocal_info_ptr->property = PROPERTY_INIT;  //如果需要发送其他属性的文件则修改这里

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

/*CRC校验函数 根据自己原本写的改进crc赋初始值是为了考虑开头数据多了00
并不影响CRC生成校验值的结果这种情况，提高效率的地方是	crc = crc ^ (*addr++ << 8);
这里是根据^位与同组值异或的顺序不同不影响结果的原理写的
参数：addr是数据数组首地址，num是数据长度，crc是crc的初始值*/
#define CRC_INIT   0xffff
#define POLY 0x1021 // 这里省略了一个1 相当于多项式为0x11021
int crc16_check(unsigned char *addr, int num,int crc)
{
	int i;
	for (; num > 0; num--)
	{
		crc = crc ^ (*addr++ << 8);
		for (i = 0; i < 8; i++)
		{
			if (crc & 0x8000)
				crc = (crc << 1) ^ POLY;
			else
				crc <<= 1;
		}
		crc &= 0xFFFF;
	}
	return(crc);
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



/*消息列队接收一帧接受解析的数据msg_que类型， 判断接受的数据包是否是子包如果是则接受所有子包整和成
一个数据包，然后进一步处理 如果不是子包则直接处理，处理根据数据包属性来决定具体的处理方式
 参数为对应的串口端口*/
void  *tsk_run(INT8 *param_ptr)
{
    INT8 port_fd = *param_ptr;
    /*这里要注意的是如果发送端发送数据结尾会加上一个\n则这里也要加上*/
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
        *(data_ptr+package_data_ptr[2] - PROTOCAL_LENGTH) = 0;
	/*获取0-9位的属性*/
        property &= 0x3ff;
    /*根据接受数据的属性不同做不同的处理*/
        switch (property)
        {
            case PROPERTY_INIT:
                   {
                       printf("The receive cmd is linux system cmd:%s\n", data_ptr);
                    /*输入数据如果是"exit"则g_Exit_flag = 1,否则执行命令*/
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


/*将要发送的scom_protocal格式的数据包的格式和数据有效长度拷贝给package_data_ptr
参数 将protocal_info_ptr格式拷贝到pack_age_data_ptr，数据长度由data_length提供*/
U8 data_package(scom_protocal *protocal_info_ptr,U8 *package_data_ptr, U8 data_length)
{
    /*判断数据长度是否超过格式允许的最大值*/
    if (data_length > DATA_LENGTH_MAX)
    {
        printf("input valid!!!\n");
        return -1;
    }

    U8 len = 0;

    package_data_ptr[0] = protocal_info_ptr->head[0];   //帧头

    package_data_ptr[1] = (U8)(protocal_info_ptr->serial_num); //流水号
    package_data_ptr[2] = (U8)((protocal_info_ptr->serial_num)>> 8);

    package_data_ptr[3] = data_length + PROTOCAL_LENGTH;   //数据有效长度

    package_data_ptr[4] = (U8)(protocal_info_ptr->property); //数据属性
    package_data_ptr[5] = (U8)((protocal_info_ptr->property)>> 8);

/*有效长度的数据*/
    for (; len < data_length; len++)
    {
        package_data_ptr[len + 6] =
            protocal_info_ptr->package_data[len];
    }
    /*存放CRC校验得出来的结果FSC*/
    U16 check_code;
    /*参与校验的长度 = data_length + serial_num(2 byte) + length(1 byte) +property(2 byte) = data_lenth +5
        帧头和帧尾不参与校验得出的CRC值存放在数据包的校验码成员上*/
    U8 *ptr = malloc(sizeof(U8) * (data_length + 5));
    memcpy(ptr, package_data_ptr + 1, data_length + 5);
    check_code = crc16_check(ptr, data_length + 5,CRC_INIT);
#if  DEBUG
     printf("crc16_check is 0x%04x\n",check_code);
#endif // DEBUG
    /*free the ptr*/
    free(ptr);
    ptr = NULL;
 /*这里尤其要注意CRC得到的校验码应该由高位到低位，自左向右存放在数据包
    如果顺序放反则将导致接受端CRC校验失败  原因是如发送前的CRC是0xa211,如果
    把低位0x11先存放如数组则package_data_ptr，则接受端在与CRC校验码校验时
    最后的CRC值为0xa211 ^ 0x11a2 不等于0 */
    /*高位先放*/
    package_data_ptr[len + 6] = (U8)(check_code>>8);
    /*在放低位*/
    package_data_ptr[len + 7] = (U8)(check_code );
    package_data_ptr[len + 8] = protocal_info_ptr->tail[0];

    return TRUE;
}

/*发送数据包 并判断是否需要分包发送，调用data_package()将数据打包成数据包格式，并的到校验码
然后经过转义后写入对应串口文件
参数： port_fd 是串口文件的描述符，protocal_info_ptr是要发送的数据包采用的格式
data_send_ptr是要发送的内容 data_send_length是数据长度*/
U8 data_send(INT8 port_fd, scom_protocal *protocal_info_ptr,U8 *data_send_ptr, U8 data_send_length)
{
    if (NULL == data_send_ptr)
    {
        printf("input data error!!!\n");
        return FALSE;
    }

    /*用来统计需要转义字符的个数*/
    U8 num_escape_char = 0;
    U8 num_escape_char_temp = 0;

    U8 *package_data = NULL;
    U8 *escape_char_ptr = NULL;

    /*发送新数据流水号加1*/
    (protocal_info_ptr->serial_num)++;
 /*判断是否要分包   当要发送的数据的长度大于 一次格式包允许的最大长度则需要分包*/
    if (data_send_length > DATA_LENGTH_MAX)
    {
        int sub_package_num = 0, count = 0;
    /*    if (0 != data_send_length % DATA_LENGTH_MAX)
        {
            sub_package_num = (data_send_length / DATA_LENGTH_MAX) + 1;
        }
        else
        {
            sub_package_num = (data_send_length / DATA_LENGTH_MAX);
        }*/
        /*这行可以取代上面的if*/
         sub_package_num = ((data_send_length+ DATA_LENGTH_MAX - 1)/ DATA_LENGTH_MAX);

    /*属性的13-15位如果为0 代表不分包 不为0 代表分包数，因为是3bit所以分包数最多为8，
        一次最多传输个8个子包， 10-12位表示当前子包的ID号  所以下面分别对这几位赋值*/
        if (sub_package_num > 8)
        {
            return FALSE;
        }
        /*设置属性13-15位分包数*/
        protocal_info_ptr->property &= 0x1fff;
        protocal_info_ptr->property |= ((sub_package_num - 1) << 13);
#if DEBUG
        printf("sub_package_num is:%4x\n",sub_package_num);
#endif
    /*sub_package_num - 1 原因是比如最后一个子包的长度不一定是位DATA_LENGTH_MAX需要单独处理*/
        for (; count < (sub_package_num - 1); count++)
        {
            /*设置数据包长度*/
            protocal_info_ptr->length[0] =
                DATA_LENGTH_MAX + PROTOCAL_LENGTH;
            /* 设置属性的10-12位  表示当前子包的ID号*/
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
           /*将数据按照protocal_info_ptr格式打包 并填写CRC校验码*/
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
            /*统计需要转义的个数*/
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

        /*将最后一个有可能长度不为DATA_LENGTH_MAX的子包单独处理*/
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
    else /*data_send_length <= DATA_LENGTH_MAX  不需要分包的情况 直接发送 属性采用初始化时候默认的即可*/
    {
        /*set the length of tht protocal_info_ptr->length[0]*/
        protocal_info_ptr->length[0] = data_send_length + PROTOCAL_LENGTH;

        /*注意这里需要对property的10-15子包描述位清0 避免上次分包修改protocal_info_ptr结构体
        属性成员的数据10-15位对此次产生影响   因为结构体传递的是指针*/
        protocal_info_ptr->property &= 0x03ff;

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

/*数据包接收读取串口设备文件来获取数据然后传递给data_process处理
data_process 按帧格式接收处理 反转义后校验 然后传递给消息列队 在
tsk_run中处理将子包整合然后根据数据内容做不同相应
参数:data_recv_info_ptr 用来存放接收数据的缓存区 */
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
        /* 从串口中读取数据*/
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

        /* 如果接受到数据*/
        if ((0 == len) && (pos < BUFFER_SIZE) &&(1 == data_new_flag))
        {
            /*start to process data*/
#if DEBUG
            printf("data_start_pos is%2d pos is %3d\n", data_start_pos, pos);
#endif

/*data_proces函数将data_recv_buf_ptr接受到的的数据包根据HEAD和TAIL格式分组，如
果数据包的数据包含3.5组，则将前三组反转义，然后CRC校验（对数据帧去HEAD和TAIL后校
验），最后根据数据和长度构成mes_que结构体传递给消息列队在tsk_run中进一步处理。剩
下的0.5则返回0.5组HEAD的位置返回给 data_proc_pos，下次接受完整后在传入处理。
如果刚好是整数组则直接返回数据结束位置*/
            data_proc_pos = data_process(data_recv_buf_ptr, data_start_pos, pos);
            if (data_proc_pos > 0)
            {
                data_start_pos = data_proc_pos;
            }
            else
            {
                /*这里可以改成data_Start_pos = 0; pos = 0;当完整接受后可以从头接受数据减少进入BUFFER_SIZE == pos的IF中*/
                //data_start_pos = pos = 0;
                /*这里是如果数据刚分成整数组则将下次从pos也就是当前数据包末尾位置开始
               接收，不采用上一种方法是因为调试的时候这样方便统计和查看接受数据的个数*/
                data_start_pos = pos;
            }
            printf("data_proc_pos is %3d\n", data_proc_pos);
            data_new_flag = 0;
        }

        /*如果到达了缓存区末尾则将还没有处理完的数据拷贝到缓存区开头  这里改成循环列队的结构
        效率会高一些*/
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

/*1判断消息列队是否生成如果没有则生成。2创建一个线程*/
pthread_t tsk_thread_create(void *(*start_routine)(void *),void *arg)
{
    INT8 ret = 0;
    /*create a msg_que*/
    msg_que msg_que_info;
    INT16 msg_que_id;
    key_t key;
    key = (key_t)MSG_QUE_KEY;
    /*判断消息列队是否生成了 没有则生成 MSG_QUE_KEY键值由ftok得到*/
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

/*用来删除消息列队以及在列队中的所有数据*/
U8 tsk_thread_delete(void)
{
    INT16 msg_que_id = 0;
    /*由键值得到消息列队的ID*/
    msg_que_id = msgget(MSG_QUE_KEY, IPC_EXCL);
    if (msg_que_id < 0)
    {
        printf("msg que is not exist!\n");
        return TRUE;
    }
    else
    {
        /*IPC_RMID用来删除消息列队以及在列队中的所有数据 立即生效*/
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

    rev_data.port_fd = port_fd; // 初始化接收数据结构体的port口

    MSG_QUE_KEY = ftok("./",6); // MSG_QUE_KEY是全局变量 ftok第一个参数为项目名，第二个参数大于0即可

    tsk_thread_create((void *(*)(void *))data_recv, (void *)&rev_data);
    tsk_thread_create((void *(*)(void *))tsk_run, (void *)&port_fd);
    while(!g_exit_flag )
    {
        printf("input command:\n");
        /*fgets 读一行数据如果数据超过了data数据的大小则会读sizeof data -1 最后一位会给'\0'*/
        fgets(data, sizeof(data) , stdin);
        scom.property = PROPERTY_INIT;  //这里根据数据类型来修改属性   10-15子包描述位留给data_Send函数自己填写
        /*因为fgets读取会将最后的换行符也读入所以my_strlen(data) - 1*/
        data_send(port_fd,&scom,data,my_strlen(data)-1);
    }
    close_port(port_fd);
    tsk_thread_delete();
}
/*测试的时候只需要用一个USB转串口接电脑USB0，然后USB转串口TX和RX相连*/
