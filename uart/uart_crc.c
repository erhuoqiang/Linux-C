#include<stdlib.h>
#include<math.h>
#include<stdio.h>
#include<fcntl.h>          /*File control*/
#include<unistd.h>         /**/
#include<errno.h>
#include<string.h>
#include<termio.h>         /*provide the common interface*/
#include<sys/types.h>
#include<sys/stat.h>
#include<pthread.h>
#include<sys/ipc.h>
#include<sys/msg.h>

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned long U32;
typedef signed char INT8;
typedef unsigned int UINT16;
typedef signed int INT16;


#define FALSE -1
#define TRUE 0

/*define the device port*/
#define DEV_PORT "/dev/pts/4"

/*define the flag to cancel the thread*/
U8 g_exit_flag = 0;


#define DATA_LENGTH_MAX 8
#define PROTOCAL_LENGTH 9
#define BUFFER_SIZE 72
#define BAUDRATE 115200
/*串口发送的数据帧协议*/
typedef struct
{
	U8 head[1];
	U16 serial_num;
	U8 length[1];  /*length = 一帧数据的长度 =  PROTOCAL_LENGTH+DATA_LENGTH_MAX*/
	U16 property;     
	U8 package_data[DATA_LENGTH_MAX];
	U16 crc_check;
	U8 tail[1];
}scom_protocal; 
 
#define HEAD 0xf4
#define TAIL 0x4f
/*数据属性初始化值*/
#define PROPERTY_INIT 0x11
#define PROPERTY_0x01 0x51
#define PROPERTY_0x02 0x91
#define PROPERTY_0x03 0xd1
#define PROPERTY_0x04 0x111
#define PROPERTY_0x05 0x151
#define PROPERTY_OTHER 0x155

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


U8 escape_character(U8 *package_data_ptr, U8 data_length, U8 *buffer)
{
	if ((NULL == package_data_ptr) | (0 == data_length))
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
		if (0xf4 == package_data_ptr[count])
		{
			*buffer++ = 0xff;
			*buffer++ = 0x01;
		}
		else if (0xff == package_data_ptr[count])
		{
			*buffer++ = 0xff;
			*buffer++ = 0x02;
		}
		else if (0x4f == package_data_ptr[count])
		{
			*buffer++ = 0xff;
			*buffer++ = 0x03;
		}
		else
		{
			*buffer++ = package_data_ptr[count];
		}/*end if*/
	}/*end for*/
	*buffer++ = 0x4f;
	*buffer = '\0';

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
