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


#define DATA_LENGTH_MAX 8
#define PROTOCAL_LENGTH 9
#define BUFFER_SIZE 72
#define BAUDRATE 115200

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
	if ((NUsLL == package_data_ptr) || (0 == data_length)|| (*package_data_ptr != HEAD))
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
                *package_data_ptr++ =  *buffer_ptr++;  //如果不是前面三种情况则说明传输过程中该位或者后一位出现的错误
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


/*define the flag to cancel the thread*/  
U8 g_exit_flag = 0;  
 

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
  
/*数据属性初始化值*/
#define PROPERTY_INIT 0x11
#define PROPERTY_0x01 0x51
#define PROPERTY_0x02 0x91
#define PROPERTY_0x03 0xd1
#define PROPERTY_0x04 0x111
#define PROPERTY_0x05 0x151
#define PROPERTY_OTHER 0x155
 
#define MSG_QUE_KEY 1024  
/*define the struct of the mes_que*/  
#define MSG_TYPE 17  
 
/*消息列队接收一帧接受解析的数据msg_que类型，并执行数据对应指令*/
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
