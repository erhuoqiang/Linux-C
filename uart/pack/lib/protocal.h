#ifndef _PROTOCAL_H_
#define _PROTOCAL_H_

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
#include<time.h>

typedef unsigned char U8;
typedef char INT8;
typedef unsigned int U32;
typedef int INT32;
typedef unsigned short int U16;
typedef short int INT16;

extern volatile int MSG_QUE_KEY; //KEY VALUE
extern volatile int MSG_QUE_ID;  //msg_id

#define HEAD 0xf4     //头标志
#define TAIL 0x4f    //尾标志
#define ESC 0xff   //转义字符
#define ESC_HEAD  0x01 //HEAD 转义成ESC + ESC_HEAD
#define ESC_ESC  0x02  //ESC 转义成ESC + ESC_ESC
#define ESC_TAIL 0x03  //TAIL 转义成ESC + ESC_TAIL

#define MAX_DATA_LENGTH 90  //REV_BUFFER_SIZE - PROTOCAL_LENGTH
#define PROTOCAL_LENGTH 6

/*串口协议帧*/
typedef struct
{
	U8 head[1];
	U8 length[1];  /*length = 一帧数据的长度 =  协议长度+数据长度*/
	U8 property[1];
	U8 data[MAX_DATA_LENGTH];
	U16 crc_fsc;
	U8 tail[1];
}Data_Protocal;

#define REV_BUFFER_SIZE 96
/*REV_BUFFER*/
typedef struct
{
    INT8 port_id;
    ssize_t (*read_data)(int fd,void * buf, size_t data_len );
    U8 buf[REV_BUFFER_SIZE];
}rev_pthread_param;

/*msg struct*/
typedef struct
{
    long msg_type;  //msg struct first member must be long type
    //INT8 data_len;   //consider data kind changge
    INT8 data[REV_BUFFER_SIZE];
}msg_que;

extern volatile int MSG_QUE_KEY; //KEY VALUE
extern volatile int MSG_QUE_ID;  //msg_id


#define CRC_INIT   0xFFFF
#define POLY 0x1021 // 这里省略了一个1 相当于多项式POLY为0x11021

/*PROPERTY TYPE*/
#define PROPERTY_CMD 0x01
#define PROPERTY_DATA 0x02
#define PROPERTY_OTHER 0x03


#ifdef __cplusplus
extern "C"
{
#endif
/*协议相关   lib*/
extern void Data_Protocal_Package(Data_Protocal * package, U8 property, INT8 *buf, U8 data_length);
extern U16 CRC16_Check(U8* data, int num, int crc);
extern INT8 Escape_Character(U8 *package_buf, U8 package_length, U8 *buf);
extern INT8 Reverse_Escape_Character(U8 *rev_buf, U8 *data_package);
extern int Rev_Process(U8 * rev_buf, int data_start_pos, int data_end_pos);

/*接受和发送  lib*/
extern void Data_Rev(rev_pthread_param * rev_param);
extern int Send_data(INT8 port,Data_Protocal * package, ssize_t (*write_data)(int,  const void *, size_t));

/*Create消息队列*/
extern void Create_MSG_QUE(const char * path, int num);

#ifdef __cplusplus
}
#endif



#endif


