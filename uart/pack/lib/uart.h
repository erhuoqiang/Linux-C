#ifndef _UART_H_
#define _UART_H_

#include<stdlib.h>
#include<stdio.h>
#include<fcntl.h>          /*File control 文件控制*/
#include<unistd.h>
#include<string.h>
#include<termio.h>         /*配置串口的接口函数*/
#include<sys/types.h>
#include<sys/stat.h>


typedef unsigned char U8;
typedef char INT8;
typedef unsigned int U32;
typedef int INT32;
typedef unsigned short int U16;
typedef short int INT16;






#ifdef __cplusplus
extern "C"
{
#endif
/*串口配置*/
extern INT8 Open_Port(const char * DEV);
extern INT8 Close_Port(INT8 port_fd);
extern U8 Serial_Init(INT8 port_fd, U32 baud_rate, U8 data_bits, U8 parity, U8 stop_bit);


#ifdef __cplusplus
}
#endif



#endif


