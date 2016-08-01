#include<stdlib.h>
#include<stdio.h>
#include<fcntl.h>          /*File control 文件控制*/
#include<unistd.h>
#include<string.h>
#include<termio.h>         /*配置串口的接口函数*/
#include<sys/types.h>
#include<sys/stat.h>

#include<pthread.h>
#include<sys/ipc.h>
#include<sys/msg.h>

#define TURE 0
#define FALSE -1

typedef unsigned char U8;
typedef char INT8;
typedef unsigned int U32;
typedef int INT32;
typedef unsigned short int U16;
typedef short int INT16;

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

#define MAX_DATA_LENGTH 249  //255 - PROTOCAL_LENGTH
#define PROTOCAL_LENGTH 6
typedef struct
{
	U8 head[1];
	U8 length[1];  /*length = 一帧数据的长度 =  协议长度+数据长度*/
	U8 property[1];
	U8 data[MAX_DATA_LENGTH];
	U16 crc_fsc;
	U8 tail[1];
}Data_Protocal;


U16 CRC16_Check(U8* data, int num, int crc);
INT8 Open_Port(const char * DEV);
INT8 Close_Port(INT8 port_fd);
void Data_Protocal_Package(Data_Protocal * package, U8 property, INT8 *buf, U8 length);
U8 Serial_Init(INT8 port_fd, U32 baud_rate, U8 data_bits, U8 parity, U8 stop_bit);
int My_Strlen(const char * src);
void Data_Protocal_Package(Data_Protocal * package, U8 property, INT8 *buf, U8 data_length);
int Send_data(INT8 port,Data_Protocal * package);
INT8 Escape_Character(U8 *package_buf, U8 package_length, U8 *buf);
INT8 Reverse_Escape_Character(U8 *buffer_ptr, U8 *package_data_ptr,  U8 data_length);

/*
CRC校验函数 根据自己原本根据书本思路写的改进crc赋初始值是为了考虑开头数据多了很多0
并不影响CRC生成校验值的结果这种情况，提高效率的地方是crc = crc ^ (*addr++ << 8);
这里是根据^位与同组值异或的顺序不同不影响结果的原理写的
参数：addr是数据数组首地址，num是数据长度，crc是crc的初始值
*/
#define CRC_INIT   0xFFFF
#define POLY 0x1021 // 这里省略了一个1 相当于多项式POLY为0x11021
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
#define DEV_PORT1 "/dev/ttyS1"
/*PROPERTY TYPE*/
#define PROPERTY_CMD 0x01
#define PROPERTY_DATA 0x02
#define PROPERTY_OTHER 0x03
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
    printf("CRC is %d\n",package->crc_fsc);

    package->tail[0] = TAIL;
}

/*避免数据中出现和HEAD,TAIL和转义字符标志字段同ASCII码的 采用字节填充的方法
package_data_ptr是数据帧，生成的buf是经过转义的数据帧确保标志符
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
    /* tongji geshu*/
    len = package->length[0] - 2;
    for(i = 1; i <= len - 2; i++)  //bu bao kuo CRC16 2byte
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

        printf("Send data success!\n");
    }
    else
    {
        for( i = 0; i < package->length[0] + escape_count; i++)
        {
                printf(" %x ",package_buf[i]);
        }
        printf("\n");
        if( write(port, package_buf,package->length[0]) == -1)
        {
            printf("Send data failed!\n");
            free(send_buf);
            free(package_buf);
            return -1;
        }
        printf("Send data success!\n");
    }

    return 0;
}

/*****************REV PART********************/

/*反转义 HEAD<-ESC 0X01 TAIL<-ESC 0X03 ESC<-ESC 0X02
将未转义的数据包转义给package_data_ptr  data_length这里没有效果可忽略
这里加上是为了以后需要对没有HEAD和TAIL标识符的数据进行反转义*/
INT8 Reverse_Escape_Character(U8 *buffer_ptr, U8 *package_data_ptr,  U8 data_length)
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
        }

    }
    *package_data_ptr = TAIL;
    return 0;
}
#define DEV_PORT0  "/dev/ttyS0"

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



/*strlen by myself*/
int My_Strlen(const char * src)
{
    const char * temp = src;
    if(src == NULL)
        return -1;
    while(*src++);
    return src - temp - 1;
}

/*from baidu*/
int kbhit(void)
{
    struct termios oldt, newt;
    int ch;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if(ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

#define UP 65
#define DOWN 66
#define LEFT 68
#define RIGHT 67
#define BACK 32
#define MENU 109

int main(int argc, char **argv)
{
     INT8  port_fd = 0;
     Data_Protocal package;
     char str[100] = "";
     port_fd = Open_Port(DEV_PORT1);
     Serial_Init(port_fd, 115200, 8, 'N', 1);
     printf("Input 'q' is quit this program\n");
     while(1)
     {
        //scanf("%s",str);
        if(kbhit())
        {
            str[0] = getchar();
	    if( 27 == str[0]  || 91 == str[0] )
		continue;
	    if('q' == str[0])
		break;
	    switch(str[0])
	    {
		case UP: printf("\nUP\n");break;
		case DOWN: printf("\nDOWN\n");break;
		case LEFT: printf("\nLEFT\n");break;
		case RIGHT: printf("\nRIGHT\n");break;
		case BACK: printf("\nBACK\n");break;
		case MENU: printf("\nMENU\n");break;
		default: printf("\nOTHER\n");break;
	     }
	     fflush(stdout);
	    str[1] = 0;
            Data_Protocal_Package(&package, PROPERTY_CMD, str, My_Strlen(str));
            Send_data(port_fd, &package);
        }
     }
}
