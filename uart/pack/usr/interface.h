#ifndef _INTERFACE_H_
#define _INTERFACE_H_
#include "uart.h"
#include "protocal.h"
#include<sys/ipc.h>
#include<sys/msg.h>
#include<stdio.h>
#include <signal.h>        //signal()
#include <sys/time.h>    //struct itimerval, setitimer()

typedef unsigned char U8;
typedef char INT8;
typedef unsigned int U32;
typedef int INT32;
typedef unsigned short int U16;
typedef short int INT16;





#if 0
#define DEBUG
#endif

#if 0
#define RESULT
#endif

#define REV_BUFFER_SIZE 96
extern volatile int MSG_QUE_KEY; //KEY VALUE
extern volatile int MSG_QUE_ID;  //msg_id

#define DEV_PORT0  "/dev/ttyS0"
#define DEV_PORT1 "/dev/ttyS1"

#ifdef __cplusplus
extern "C"
{
#endif
//extern INT8 Read_MSG_QUE(void);
extern INT8 Read_MSG_QUE(rev_pthread_param * rev);//interface with UI
extern int My_Strlen(const char * src);


#ifdef __cplusplus
}
#endif


#endif


