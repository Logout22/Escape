/**
 * $Id$
 * Copyright (C) 2008 - 2009 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <sys/common.h>
#include <sys/task/proc.h>
#include <sys/task/event.h>
#include <sys/task/signals.h>
#include <sys/vfs/vfs.h>
#include <sys/vfs/node.h>
#include <sys/vfs/real.h>
#include <sys/vfs/request.h>
#include <sys/util.h>
#include <sys/video.h>
#include <esc/fsinterface.h>
#include <esc/messages.h>
#include <string.h>
#include <assert.h>
#include <errors.h>

/* A small note to this: we can use one channel (identified by the node) in parallel because
 * we expect all drivers to handle the requests in FIFO order. That means, if we order the
 * requests (append new ones at the end), we can simply choose the first with the specified node
 * when looking for a channel that should receive the response.
 * As soon as a request should no longer receive messages, vfs/driver and vfs/real remove the
 * request from the list via vfs_req_remove(). This HAS TO be done by the one that sends the
 * response! Later, when the client got the response, it free's the request.
 */

#define REQ_COUNT			1024
#define HANDLER_COUNT		32

static fReqHandler handler[HANDLER_COUNT] = {NULL};

static sRequest requests[REQ_COUNT];
static sRequest *reqFreeList;
static sRequest *reqUsedList;
static sRequest *reqUsedEnd;

void vfs_req_init(void) {
	size_t i;
	/* init requests */
	requests->next = NULL;
	reqFreeList = requests;
	for(i = 1; i < REQ_COUNT; i++) {
		requests[i].next = reqFreeList;
		reqFreeList = requests + i;
	}
	reqUsedList = NULL;
	reqUsedEnd = NULL;
}

bool vfs_req_setHandler(tMsgId id,fReqHandler f) {
	if(id >= HANDLER_COUNT || handler[id] != NULL)
		return false;
	handler[id] = f;
	return true;
}

void vfs_req_sendMsg(tMsgId id,sVFSNode *node,const void *data,size_t size) {
	assert(node != NULL);
	if(id < HANDLER_COUNT && handler[id])
		handler[id](node,data,size);
}

sRequest *vfs_req_get(sVFSNode *node,void *buffer,size_t size) {
	sThread *t = thread_getRunning();
	sRequest *req = NULL;
	assert(node != NULL);

	if(reqFreeList == NULL)
		return NULL;

	req = reqFreeList;
	reqFreeList = req->next;

	req->tid = t->tid;
	req->node = node;
	req->state = REQ_STATE_WAITING;
	req->val1 = 0;
	req->val2 = 0;
	req->data = buffer;
	req->dsize = size;
	req->count = 0;
	req->next = NULL;
	if(!reqUsedEnd)
		reqUsedList = req;
	else
		reqUsedEnd->next = req;
	reqUsedEnd = req;
	return req;
}

void vfs_req_waitForReply(sRequest *req,bool allowSigs) {
	/* wait */
	ev_wait(req->tid,EVI_REQ_REPLY,(tEvObj)req->node);
	if(allowSigs)
		thread_switch();
	else
		thread_switchNoSigs();
	/* if we waked up and the request is not finished, the driver probably died or we received
	 * a signal (if allowSigs is true) */
	if(req->state != REQ_STATE_FINISHED) {
		/* indicate an error */
		req->count = (allowSigs && sig_hasSignalFor(req->tid)) ? ERR_INTERRUPTED : ERR_DRIVER_DIED;
	}
}

sRequest *vfs_req_getByNode(const sVFSNode *node) {
	sRequest *req = reqUsedList;
	while(req != NULL) {
		if(req->node == node) {
			/* the thread may have been terminated... */
			if(thread_getById(req->tid) == NULL) {
				vfs_req_free(req);
				return NULL;
			}
			return req;
		}
		req = req->next;
	}
	return NULL;
}

void vfs_req_remove(sRequest *r) {
	sRequest *req = reqUsedList,*p = NULL;
	while(req != NULL) {
		if(req == r) {
			if(p)
				p->next = req->next;
			else
				reqUsedList = req->next;
			if(req == reqUsedEnd)
				reqUsedEnd = p;
			return;
		}
		p = req;
		req = req->next;
	}
}

void vfs_req_free(sRequest *r) {
	if(r) {
		vfs_req_remove(r);
		r->next = reqFreeList;
		reqFreeList = r;
	}
}

#if DEBUGGING

void vfs_req_dbg_printAll(void) {
	vid_printf("Active requests:\n");
	sRequest *req = reqUsedList;
	while(req != NULL) {
		vfs_req_dbg_print(req);
		req = req->next;
	}
}

void vfs_req_dbg_print(sRequest *r) {
	vid_printf("\tRequest with %p (%s):\n",r->node,vfs_node_getPath(vfs_node_getNo(r->node)));
	vid_printf("\t\ttid: %d\n",r->tid);
	vid_printf("\t\tstate: %u\n",r->state);
	vid_printf("\t\tval1: %u\n",r->val1);
	vid_printf("\t\tval2: %u\n",r->val2);
	vid_printf("\t\tdata: %p\n",r->data);
	vid_printf("\t\tdsize: %Su\n",r->dsize);
	vid_printf("\t\tcount: %Su\n",r->count);
}

#endif