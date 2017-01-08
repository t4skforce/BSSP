/*
 * ioctl.h
 *
 *  Created on: Dec 19, 2016
 *      Author: bssp
 */

#ifndef IOCTL_H_
#define IOCTL_H_

#define IOC_MY_TYPE 0xCE

#define IOC_NR_READCNT  5
#define IOC_NR_WRITECNT 6
#define IOC_NR_CLEAR 7
#define IOC_NR_SETBUFFERSIZE 8

#define IOC_READCNT _IO(IOC_MY_TYPE, IOC_NR_READCNT)
#define IOC_WRITECNT _IO(IOC_MY_TYPE, IOC_NR_WRITECNT)
#define IOC_CLEAR _IO(IOC_MY_TYPE, IOC_NR_CLEAR)
#define IOC_SETBUFFERSIZE _IO(IOC_MY_TYPE, IOC_NR_SETBUFFERSIZE)


#endif /* IOCTL_H_ */
