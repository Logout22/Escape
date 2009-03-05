/**
 * @version		$Id$
 * @author		Nils Asmussen <nils@script-solution.de>
 * @copyright	2008 Nils Asmussen
 */

#include "../h/common.h"
#include "../h/vfs.h"
#include "../h/proc.h"
#include "../h/kheap.h"
#include <video.h>
#include <string.h>

#include "tvfs.h"
#include <test.h>

/* forward declarations */
static void test_vfs(void);
static void test_vfs_readFileSystem(void);
static void test_vfs_readFileProcess0(void);
static void test_vfs_createProcess(void);
static void test_vfs_createService(void);
static void test_vfs_resolvePath(void);
static bool test_vfs_resolvePathCpy(cstring a,cstring b);
static void test_vfs_getPath(void);

/* public vfs-node for test-purposes */
#define MAX_NAME_LEN 59
typedef struct {
	tVFSNodeNo nodeNo;
	u8 name[MAX_NAME_LEN + 1];
} sVFSNodePub;

/* public process-data for test-purposes */
typedef struct {
	u8 state;
	tPid pid;
	tPid parentPid;
	u32 textPages;
	u32 dataPages;
	u32 stackPages;
} sProcPub;

/* our test-module */
sTestModule tModVFS = {
	"VFS",
	&test_vfs
};

static void test_vfs(void) {
	test_vfs_resolvePath();
	test_vfs_readFileSystem();
	test_vfs_readFileProcess0();
	test_vfs_createProcess();
	test_vfs_createService();
	test_vfs_getPath();
}

static void test_vfs_readFileSystem(void) {
	tVFSNodeNo nodeNo,procNode,servNode;
	tFD fd;
	s32 res;
	sVFSNodePub node;
	u32 oldHeap,oldGFT,newHeap,newGFT;

	oldHeap = kheap_getFreeMem();
	oldGFT = vfs_dbg_getGFTEntryCount();

	test_caseStart("Testing vfs_readFile() for /system");

	/* resolve path */
	if(vfs_resolvePath("/system",&nodeNo) < 0) {
		test_caseFailed("Unable to resolve path '/system'!");
		return;
	}

	/* open */
	if(vfs_openFile(VFS_READ,nodeNo,&fd) < 0) {
		test_caseFailed("Unable to open '/system'!");
		return;
	}

	/* skip "." and ".." */
	vfs_readFile(fd,(u8*)&node,sizeof(sVFSNodePub));
	vfs_readFile(fd,(u8*)&node,sizeof(sVFSNodePub));

	/* read "processes" */
	if((res = vfs_readFile(fd,(u8*)&node,sizeof(sVFSNodePub))) != sizeof(sVFSNodePub)) {
		vfs_closeFile(fd);
		test_caseFailed("Unable to read with fd=%d! Expected %d bytes, read %d",
				fd,sizeof(sVFSNodePub),res);
		return;
	}

	/* check data */
	vfs_resolvePath("/system/processes",&procNode);
	if(strcmp((cstring)node.name,"processes") != 0) {
		vfs_closeFile(fd);
		test_caseFailed("Node-name='%s', expected 'system'",node.name);
		return;
	}
	if(node.nodeNo != procNode) {
		vfs_closeFile(fd);
		test_caseFailed("nodeNo=%d, expected %d",node.nodeNo);
		return;
	}

	/* read "services" */
	if((res = vfs_readFile(fd,(u8*)&node,sizeof(sVFSNodePub))) != sizeof(sVFSNodePub)) {
		vfs_closeFile(fd);
		test_caseFailed("Unable to read with fd=%d! Expected %d bytes, read %d",
				fd,sizeof(sVFSNodePub),res);
		return;
	}

	/* check data */
	vfs_resolvePath("/system/services",&servNode);
	if(strcmp((cstring)node.name,"services") != 0) {
		vfs_closeFile(fd);
		test_caseFailed("Node-name='%s', expected 'system'",node.name);
		return;
	}
	if(node.nodeNo != servNode) {
		vfs_closeFile(fd);
		test_caseFailed("nodeNo=%d, expected %d",node.nodeNo);
		return;
	}

	/* close */
	vfs_closeFile(fd);

	newHeap = kheap_getFreeMem();
	newGFT = vfs_dbg_getGFTEntryCount();
	if(oldHeap != newHeap || oldGFT != newGFT) {
		test_caseFailed("oldHeap=%d, newHeap=%d, oldGFT=%d, newGFT=%d",oldHeap,newHeap,oldGFT,newGFT);
		return;
	}

	test_caseSucceded();
}

static void test_vfs_readFileProcess0(void) {
	tVFSNodeNo nodeNo;
	tFD fd;
	s32 res;
	sProcPub proc;
	sProc *p0 = proc_getByPid(0);
	u32 oldHeap,oldGFT,newHeap,newGFT;

	oldHeap = kheap_getFreeMem();
	oldGFT = vfs_dbg_getGFTEntryCount();

	test_caseStart("Testing vfs_readFile() for /system/processes/0");

	/* resolve path */
	if(vfs_resolvePath("/system/processes/0",&nodeNo) < 0) {
		test_caseFailed("Unable to resolve path '/system/processes/0'!");
		return;
	}

	/* open */
	if(vfs_openFile(VFS_READ,nodeNo,&fd) < 0) {
		test_caseFailed("Unable to open '/system/processes/0'!");
		return;
	}

	/* read */
	if((res = vfs_readFile(fd,(u8*)&proc,sizeof(sProcPub))) != sizeof(sProcPub)) {
		vfs_closeFile(fd);
		test_caseFailed("Unable to read with fd=%d! Expected %d bytes, read %d",
				fd,sizeof(sProcPub),res);
		return;
	}

	/* check data */
	if(!test_assertInt(proc.textPages,p0->textPages)) {vfs_closeFile(fd); return;}
	if(!test_assertInt(proc.dataPages,p0->dataPages)) {vfs_closeFile(fd); return;}
	if(!test_assertInt(proc.stackPages,p0->stackPages)) {vfs_closeFile(fd); return;}
	if(!test_assertInt(proc.pid,p0->pid)) {vfs_closeFile(fd); return;}
	if(!test_assertInt(proc.parentPid,p0->parentPid)) {vfs_closeFile(fd); return;}
	if(!test_assertInt(proc.state,p0->state)) {vfs_closeFile(fd); return;}

	/* close */
	vfs_closeFile(fd);

	newHeap = kheap_getFreeMem();
	newGFT = vfs_dbg_getGFTEntryCount();
	if(oldHeap != newHeap || oldGFT != newGFT) {
		test_caseFailed("oldHeap=%d, newHeap=%d, oldGFT=%d, newGFT=%d",oldHeap,newHeap,oldGFT,newGFT);
		return;
	}

	test_caseSucceded();
}

static s32 dummyReadHandler(sVFSNode *node,u8 *buffer,u32 offset,u32 count) {
	return 0;
}

static void test_vfs_createProcess(void) {
	u32 oldHeap,newHeap;

	test_caseStart("Testing vfs_createProcess()");

	oldHeap = kheap_getFreeMem();

	/* create */
	if(!vfs_createProcess(123,&dummyReadHandler)) {
		test_caseFailed("Unable to create node with process id 123");
		return;
	}
	/* try another create with same pid */
	if(vfs_createProcess(123,&dummyReadHandler)) {
		test_caseFailed("Created another node with pid 123");
		return;
	}
	/* remove */
	vfs_removeProcess(123);

	/* check mem-usage */
	newHeap = kheap_getFreeMem();
	if(oldHeap != newHeap) {
		test_caseFailed("oldHeap=%d, newHeap=%d",oldHeap,newHeap);
		return;
	}

	test_caseSucceded();
}

static void test_vfs_createService(void) {
	u32 oldHeap,newHeap;
	sProc *p = proc_getRunning();

	test_caseStart("Testing vfs_createService()");

	oldHeap = kheap_getFreeMem();

	if(!test_assertInt(vfs_createService(p->pid,"test"),0)) return;
	if(!test_assertInt(vfs_createService(p->pid,"test2"),ERR_PROC_DUP_SERVICE)) return;
	if(!test_assertInt(vfs_createService((p + 1)->pid,"test"),ERR_SERVICE_EXISTS)) return;
	if(!test_assertInt(vfs_createService((p + 1)->pid,""),ERR_INV_SERVICE_NAME)) return;
	if(!test_assertInt(vfs_createService((p + 1)->pid,"abc.def"),ERR_INV_SERVICE_NAME)) return;
	if(!test_assertInt(vfs_createService((p + 1)->pid,"test2"),0)) return;
	vfs_removeService(p);
	vfs_removeService(p + 1);

	/* check mem-usage */
	newHeap = kheap_getFreeMem();
	if(oldHeap != newHeap) {
		test_caseFailed("oldHeap=%d, newHeap=%d",oldHeap,newHeap);
		return;
	}

	test_caseSucceded();
}

static void test_vfs_resolvePath(void) {
	test_caseStart("Testing vfs_resolvePath()");

	if(!test_vfs_resolvePathCpy("/","")) return;
	if(!test_vfs_resolvePathCpy("//","")) return;
	if(!test_vfs_resolvePathCpy("///","")) return;
	if(!test_vfs_resolvePathCpy("/..","")) return;
	if(!test_vfs_resolvePathCpy("/../..","")) return;
	if(!test_vfs_resolvePathCpy("/./.","")) return;
	if(!test_vfs_resolvePathCpy("/system/..","")) return;
	if(!test_vfs_resolvePathCpy("/system/.","system")) return;
	if(!test_vfs_resolvePathCpy("/system/./","system")) return;
	if(!test_vfs_resolvePathCpy("/system/./.","system")) return;
	if(!test_vfs_resolvePathCpy("/////system/./././.","system")) return;
	if(!test_vfs_resolvePathCpy("/../system/../system/./","system")) return;
	if(!test_vfs_resolvePathCpy("//..//..//..","")) return;
	/* TODO test as soon as relative paths are possible
	if(!test_vfs_resolvePathCpy("system/.","system")) return;
	if(!test_vfs_resolvePathCpy("system/./","system")) return;
	if(!test_vfs_resolvePathCpy("system/./.","system")) return;
	if(!test_vfs_resolvePathCpy("../system/../system/./","system")) return; */

	test_caseSucceded();
}

static bool test_vfs_resolvePathCpy(cstring a,cstring b) {
	static s8 c[100];
	s32 err;
	tVFSNodeNo no;
	sVFSNode *node;
	strncpy(c,b,99);
	if((err = vfs_resolvePath(a,&no)) != 0)
		return err;

	node = vfs_getNode(no);
	return test_assertStr(node->name,c);
}

static void test_vfs_getPath(void) {
	tVFSNodeNo no;
	sVFSNode *node;

	test_caseStart("Testing vfs_getPath()");

	vfs_resolvePath("/",&no);
	node = vfs_getNode(no);
	test_assertStr(vfs_getPath(node),(string)"/");

	vfs_resolvePath("/system",&no);
	node = vfs_getNode(no);
	test_assertStr(vfs_getPath(node),(string)"/system");

	vfs_resolvePath("/system/processes",&no);
	node = vfs_getNode(no);
	test_assertStr(vfs_getPath(node),(string)"/system/processes");

	vfs_resolvePath("/system/processes/0",&no);
	node = vfs_getNode(no);
	test_assertStr(vfs_getPath(node),(string)"/system/processes/0");

	test_caseSucceded();
}
