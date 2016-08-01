#include "uart.h"


/************************UART***********************/
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
#ifdef DEBUG
    printf("Port %d init success!\n",port_fd);
#endif
    return 0;
}
