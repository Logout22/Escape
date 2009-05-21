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

#include <common.h>
#include <mm.h>
#include <multiboot.h>
#include <proc.h>
#include <vfs.h>
#include <vfsnode.h>
#include <vfsinfo.h>
#include <util.h>
#include <kheap.h>
#include <assert.h>
#include <string.h>

/* VFS-directory-entry (equal to the direntry of ext2) */
typedef struct {
	tVFSNodeNo nodeNo;
	u16 recLen;
	u16 nameLen;
	/* name follows (up to 255 bytes) */
} __attribute__((packed)) sVFSDirEntry;

/**
 * The read-callback for the proc-read-handler
 */
static void vfsinfo_procReadCallback(sVFSNode *node,void *buffer);

/**
 * The read-handler for the mem-usage-node
 */
static s32 vfsinfo_memUsageReadHandler(tTid tid,sVFSNode *node,u8 *buffer,u32 offset,u32 count);

/**
 * The read-callback for the VFS memusage-read-handler
 */
static void vfsinfo_memUsageReadCallback(sVFSNode *node,void *buffer);

void vfsinfo_init(void) {
	tVFSNodeNo nodeNo;
	sVFSNode *sysNode;
	vfsn_resolvePath("system:/",&nodeNo);
	sysNode = vfsn_getNode(nodeNo);

	vfsn_createInfo(KERNEL_TID,vfsn_getNode(nodeNo),(char*)"memusage",vfsinfo_memUsageReadHandler);
}

s32 vfsinfo_procReadHandler(tTid tid,sVFSNode *node,u8 *buffer,u32 offset,u32 count) {
	/* don't use the cache here to prevent that one process occupies it for all others */
	/* (if the process doesn't call close() the cache will not be invalidated and therefore
	 * other processes might miss changes) */
	return vfs_readHelper(tid,node,buffer,offset,count,
			18 * 9 + 6 * 10 + 2 * 16 + MAX_PROC_NAME_LEN + 1,
			vfsinfo_procReadCallback);
}

static void vfsinfo_procReadCallback(sVFSNode *node,void *buffer) {
	char *str = (char*)buffer;
	sProc *p = proc_getByPid(atoi(node->name));
	u32 *uptr,*kptr;
	UNUSED(node);

	/* TODO */
	/*uptr = (u32*)&p->ucycleCount;
	kptr = (u32*)&p->kcycleCount;*/
	util_sprintf(
		str,
		"%-16s%u\n"
		"%-16s%u\n"
		"%-16s%s\n"
		/*"%-16s%u\n"*/
		"%-16s%u\n"
		"%-16s%u\n"
		"%-16s%u\n"
		/*"%-16s%08x%08x\n"
		"%-16s%08x%08x\n"*/
		,
		"Pid:",p->pid,
		"ParentPid:",p->parentPid,
		"Command:",p->command,
		/*"State:",p->state,*/
		"TextPages:",p->textPages,
		"DataPages:",p->dataPages,
		"StackPages:",p->stackPages/*,
		"UCPUCycles:",*(uptr + 1),*uptr,
		"KCPUCycles:",*(kptr + 1),*kptr*/
	);
}

static s32 vfsinfo_memUsageReadHandler(tTid tid,sVFSNode *node,u8 *buffer,u32 offset,u32 count) {
	return vfs_readHelper(tid,node,buffer,offset,count,(11 + 10 + 1) * 4 + 1,
			vfsinfo_memUsageReadCallback);
}

static void vfsinfo_memUsageReadCallback(sVFSNode *node,void *buffer) {
	char *str = (char*)buffer;
	UNUSED(node);

	u32 free = mm_getFreeFrmCount(MM_DEF | MM_DMA) << PAGE_SIZE_SHIFT;
	u32 total = mboot_getUsableMemCount();
	util_sprintf(
		str,
		"%-11s%10u\n"
		"%-11s%10u\n"
		"%-11s%10u\n"
		"%-11s%10u\n"
		,
		"Total:",total,
		"Used:",total - free,
		"Free:",free,
		"KHeapSize:",kheap_getUsedMem()
	);
}

s32 vfsinfo_dirReadHandler(tTid tid,sVFSNode *node,u8 *buffer,u32 offset,u32 count) {
	s32 byteCount;

	UNUSED(tid);
	vassert(node != NULL,"node == NULL");
	vassert(buffer != NULL,"buffer == NULL");

	/* not cached yet? */
	if(node->data.def.cache == NULL) {
		/* we need the number of bytes first */
		byteCount = 0;
		sVFSNode *n = NODE_FIRST_CHILD(node);
		while(n != NULL) {
			byteCount += sizeof(sVFSDirEntry) + strlen(n->name);
			n = n->next;
		}

		vassert((u32)byteCount < (u32)0xFFFF,"Overflow of size and pos detected");

		node->data.def.size = byteCount;
		node->data.def.pos = byteCount;
		if(byteCount > 0) {
			/* now allocate mem on the heap and copy all data into it */
			u8 *childs = (u8*)kheap_alloc(byteCount);
			if(childs == NULL) {
				node->data.def.size = 0;
				node->data.def.pos = 0;
			}
			else {
				u16 len;
				sVFSDirEntry *dirEntry = (sVFSDirEntry*)childs;
				node->data.def.cache = childs;
				n = NODE_FIRST_CHILD(node);
				while(n != NULL) {
					len = strlen(n->name);
					dirEntry->nodeNo = NADDR_TO_VNNO(n);
					dirEntry->nameLen = len;
					dirEntry->recLen = sizeof(sVFSDirEntry) + len;
					memcpy(dirEntry + 1,n->name,len);
					dirEntry = (sVFSDirEntry*)((u8*)dirEntry + dirEntry->recLen);
					n = n->next;
				}
			}
		}
	}

	if(offset > node->data.def.size)
		offset = node->data.def.size;
	byteCount = MIN(node->data.def.size - offset,count);
	if(byteCount > 0) {
		/* simply copy the data to the buffer */
		memcpy(buffer,(u8*)node->data.def.cache + offset,byteCount);
	}

	return byteCount;
}
