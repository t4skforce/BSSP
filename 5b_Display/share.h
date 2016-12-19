/*
 * share.h
 *
 *  Created on: Dec 16, 2016
 *      Author: bssp
 */

#ifndef SHARE_H_
#define SHARE_H_

#define MAX_MSG_LEN 256

#define SHM_NAME "/is131415"
#define SNAMEWRITE "/is131415_write"
#define SNAMEREAD "/is131415_read"

typedef struct {
	char msg[MAX_MSG_LEN];
} shm_for_msg_t;

#endif /* SHARE_H_ */
