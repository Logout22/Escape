/**
 * $Id$
 * Copyright (C) 2008 - 2011 Nils Asmussen
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
#include <sys/task/thread.h>
#include <sys/task/signals.h>
#include <sys/task/sched.h>
#include <sys/task/elf.h>
#include <sys/task/lock.h>
#include <sys/task/env.h>
#include <sys/task/event.h>
#include <sys/task/uenv.h>
#include <sys/task/groups.h>
#include <sys/task/terminator.h>
#include <sys/task/fd.h>
#include <sys/mem/paging.h>
#include <sys/mem/pmem.h>
#include <sys/mem/cache.h>
#include <sys/mem/vmm.h>
#include <sys/mem/cow.h>
#include <sys/mem/sllnodes.h>
#include <sys/intrpt.h>
#include <sys/vfs/vfs.h>
#include <sys/vfs/node.h>
#include <sys/vfs/fsmsgs.h>
#include <sys/spinlock.h>
#include <sys/mutex.h>
#include <sys/util.h>
#include <sys/syscalls.h>
#include <sys/video.h>
#include <sys/debug.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>

#define DEBUG_CREATIONS					0
/* the max. size we'll allow for exec()-arguments */
#define EXEC_MAX_ARGSIZE				(2 * K)

/* we have to put the first one here because we need a process before we can allocate
 * something on the kernel-heap. sounds strange, but the kheap-init stuff uses paging and
 * this wants to have the current process. So, I guess the cleanest way is to simply put
 * the first process in the data-area (we can't free the first one anyway) */
Proc ProcBase::first;
/* our processes */
sSLList ProcBase::procs;
Proc *ProcBase::pidToProc[MAX_PROC_COUNT];
pid_t ProcBase::nextPid = 1;
mutex_t ProcBase::procLock;
mutex_t ProcBase::childLock;

void ProcBase::init() {
	/* init the first process */
	Proc *p = &first;

	p->pid = 0;
	p->parentPid = 0;
	p->ruid = ROOT_UID;
	p->euid = ROOT_UID;
	p->suid = ROOT_UID;
	p->rgid = ROOT_GID;
	p->egid = ROOT_GID;
	p->sgid = ROOT_GID;
	p->groups = NULL;
	p->ownFrames = 0;
	p->sharedFrames = 0;
	p->swapped = 0;
	p->sigRetAddr = 0;
	p->flags = 0;
	p->entryPoint = 0;
	sll_init(&p->fsChans,slln_allocNode,slln_freeNode);
	p->env = NULL;
	p->stats.input = 0;
	p->stats.output = 0;
	p->stats.peakOwnFrames = 0;
	p->stats.peakSharedFrames = 0;
	p->stats.swapCount = 0;
	p->stats.totalRuntime = 0;
	p->stats.totalSyscalls = 0;
	p->stats.totalScheds = 0;
	p->stats.exitCode = 0;
	p->stats.exitSignal = SIG_COUNT;
	memclear(p->locks,sizeof(p->locks));
	p->command = strdup("initloader");
	/* create nodes in vfs */
	p->threadDir = vfs_createProcess(p->pid);
	if(p->threadDir < 0)
		util_panic("Not enough mem for init process");

	/* init fds */
	fd_init(p);

	/* create first thread */
	sll_init(&p->threads,slln_allocNode,slln_freeNode);
	if(!sll_append(&p->threads,Thread::init(p)))
		util_panic("Unable to append the initial thread");

	/* init region-stuff */
	p->freeStackAddr = 0;
	p->dataAddr = 0;
	vmfree_init(&p->freemap,FREE_AREA_BEGIN,FREE_AREA_END - FREE_AREA_BEGIN);
	vmreg_addTree(p->pid,&p->regtree);

	/* add to procs */
	sll_init(&procs,slln_allocNode,slln_freeNode);
	if(!add(p))
		util_panic("Unable to add init-process");
}

void ProcBase::setCommand(const char *cmd,int argc,const char *args) {
	int i;
	size_t cmdlen = strlen(cmd);
	size_t len = cmdlen + 1;
	const char *curargs = args;
	char *curdst;
	if(command)
		cache_free((char*)command);

	/* store flag for the fs driver; TODO this is an ugly hack */
	if(strstr(cmd,"/dev/fs") != NULL)
		flags |= P_FS;

	/* determine total length */
	for(i = 0; i < argc; ++i) {
		size_t curlen = strlen(curargs) + 1;
		if(i > 0)
			len += curlen;
		curargs += curlen;
	}

	/* copy cmd and arguments into buffer */
	command = (char*)cache_alloc(len);
	curdst = (char*)command;
	memcpy(curdst,cmd,cmdlen);
	curdst += cmdlen;
	for(i = 0; i < argc; ++i) {
		size_t curlen = strlen(args);
		if(i > 0) {
			if(i == 1)
				*curdst++ = ' ';
			memcpy(curdst,args,curlen);
			curdst += curlen;
			if(i < argc - 1)
				*curdst++ = ' ';
		}
		args += curlen + 1;
	}
	*curdst = '\0';
}

void ProcBase::getMemUsageOf(pid_t pid,size_t *own,size_t *shared,size_t *swapped) {
	Proc *p = request(pid,PLOCK_REGIONS);
	if(p) {
		vmm_getMemUsageOf(p,own,shared,swapped);
		*own = *own + paging_getPTableCount(p->getPageDir()) + p->getKMemUsage();
		release(p,PLOCK_REGIONS);
	}
}

void ProcBase::getMemUsage(size_t *dataShared,size_t *dataOwn,size_t *dataReal) {
	size_t pages,ownMem = 0,shMem = 0;
	float dReal = 0;
	sSLNode *n;
	mutex_aquire(&procLock);
	for(n = sll_begin(&procs); n != NULL; n = n->next) {
		size_t pown,psh,pswap;
		Proc *p = (Proc*)n->data;
		getMemUsageOf(p->pid,&pown,&psh,&pswap);
		ownMem += pown;
		shMem += psh;
		dReal += vmm_getMemUsage(p->pid,&pages);
	}
	mutex_release(&procLock);
	*dataOwn = ownMem * PAGE_SIZE;
	*dataShared = shMem * PAGE_SIZE;
	*dataReal = (size_t)(dReal + cow_getFrmCount()) * PAGE_SIZE;
}

int ProcBase::clone(uint8_t flags) {
	int newPid,res = 0;
	Proc *p,*cur;
	Thread *nt,*curThread = Thread::getRunning();
	assert((flags & P_ZOMBIE) == 0);
	cur = request(curThread->getProc()->pid,PLOCK_PROG);
	if(!cur) {
		res = -ESRCH;
		goto errorReqProc;
	}
	/* don't allow cloning when the process should die */
	if(cur->flags & (P_ZOMBIE | P_PREZOMBIE)) {
		res = -EDESTROYED;
		goto errorCur;
	}

	p = (Proc*)cache_alloc(sizeof(Proc));
	if(!p) {
		res = -ENOMEM;
		goto errorCur;
	}

	/* clone page-dir */
	if((res = paging_cloneKernelspace(p->getPageDir(),curThread->getTid())) < 0)
		goto errorProc;

	/* set basic attributes */
	p->parentPid = cur->pid;
	memclear(p->locks,sizeof(p->locks));
	p->ruid = cur->ruid;
	p->euid = cur->euid;
	p->suid = cur->suid;
	p->rgid = cur->rgid;
	p->egid = cur->egid;
	p->sgid = cur->sgid;
	p->sigRetAddr = cur->sigRetAddr;
	p->flags = 0;
	p->entryPoint = cur->entryPoint;
	sll_init(&p->fsChans,slln_allocNode,slln_freeNode);
	p->env = NULL;
	p->flags = flags;
	p->stats.input = 0;
	p->stats.output = 0;
	p->stats.peakOwnFrames = 0;
	p->stats.peakSharedFrames = 0;
	p->stats.swapCount = 0;
	p->stats.totalRuntime = 0;
	p->stats.totalSyscalls = 0;
	p->stats.totalScheds = 0;
	p->stats.exitCode = 0;
	p->stats.exitSignal = SIG_COUNT;
	/* give the process the same name (may be changed by exec) */
	p->command = strdup(cur->command);
	if(p->command == NULL) {
		res = -ENOMEM;
		goto errorPdir;
	}

	/* determine pid; ensure that nobody can get this pid, too */
	mutex_aquire(&procLock);
	newPid = getFreePid();
	if(newPid < 0) {
		mutex_release(&procLock);
		res = -ENOPROCS;
		goto errorCmd;
	}

	/* add to processes */
	p->pid = newPid;
	if(!add(p)) {
		mutex_release(&procLock);
		res = -ENOMEM;
		goto errorCmd;
	}
	mutex_release(&procLock);

	/* create the VFS node */
	p->threadDir = vfs_createProcess(p->pid);
	if(p->threadDir < 0) {
		res = p->threadDir;
		goto errorAdd;
	}

	/* join group of parent */
	p->groups = NULL;
	groups_join(p,cur);

	/* clone regions */
	p->freeStackAddr = 0;
	vmfree_init(&p->freemap,FREE_AREA_BEGIN,FREE_AREA_END - FREE_AREA_BEGIN);
	if((res = vmm_cloneAll(p->pid)) < 0)
		goto errorVFS;

	/* clone current thread */
	if((res = Thread::create(curThread,&nt,p,0,true)) < 0)
		goto errorRegs;
	sll_init(&p->threads,slln_allocNode,slln_freeNode);
	if(!sll_append(&p->threads,nt)) {
		res = -ENOMEM;
		goto errorThread;
	}

	if((res = cloneArch(p,cur) < 0))
		goto errorThreadAppend;

	/* inherit file-descriptors */
	fd_clone(p);

	res = Thread::finishClone(curThread,nt);
	if(res == 1) {
		/* child */
		return 0;
	}
	/* parent */
	Event::unblock(nt);

#if DEBUG_CREATIONS
#ifdef __eco32__
	debugf("Thread %d (proc %d:%s): %x\n",nt->getTid(),nt->getProc()->pid,nt->getProc()->command,
			nt->archAttr.kstackFrame);
#endif
#ifdef __mmix__
	debugf("Thread %d (proc %d:%s)\n",nt->getTid(),nt->getProc()->pid,nt->getProc()->command);
#endif
#endif

	release(cur,PLOCK_PROG);
	/* if we had reserved too many, free them now */
	curThread->discardFrames();
	return p->pid;

errorThreadAppend:
	sll_removeFirstWith(&p->threads,nt);
errorThread:
	nt->kill();
errorRegs:
	doRemoveRegions(p,true);
errorVFS:
	groups_leave(p->pid);
	vfs_removeProcess(p->pid);
errorAdd:
	remove(p);
errorCmd:
	cache_free((void*)p->command);
errorPdir:
	paging_destroyPDir(p->getPageDir());
errorProc:
	cache_free(p);
errorCur:
	release(cur,PLOCK_PROG);
errorReqProc:
	return res;
}

int ProcBase::startThread(uintptr_t entryPoint,uint8_t flags,const void *arg) {
	uintptr_t tlsStart,tlsEnd;
	size_t pageCount;
	Thread *t = Thread::getRunning();
	Proc *p;
	Thread *nt;
	int res;
	/* don't allow new threads when the process should die */
	if(t->getProc()->flags & (P_ZOMBIE | P_PREZOMBIE))
		return -EDESTROYED;

	/* reserve frames for new stack- and tls-region */
	pageCount = Thread::getThreadFrmCnt();
	if(t->getTLSRange(&tlsStart,&tlsEnd))
		pageCount += BYTES_2_PAGES(tlsEnd - tlsStart);
	t->reserveFrames(pageCount);

	p = request(t->getProc()->pid,PLOCK_PROG);
	if(!p) {
		res = -ESRCH;
		goto errorReqProc;
	}

	if((res = Thread::create(t,&nt,p,flags,false)) < 0)
		goto error;

	/* append thread */
	if(!sll_append(&p->threads,nt)) {
		nt->kill();
		res = -ENOMEM;
		goto error;
	}

	Thread::finishThreadStart(t,nt,arg,entryPoint);

	/* mark ready (idle is always blocked because we choose it explicitly when no other can run) */
	if(nt->getFlags() & T_IDLE)
		Event::block(nt);
	else
		Event::unblock(nt);

#if DEBUG_CREATIONS
#ifdef __eco32__
	debugf("Thread %d (proc %d:%s): %x\n",nt->getTid(),nt->getProc()->pid,nt->getProc()->command,
			nt->archAttr.kstackFrame);
#endif
#ifdef __mmix__
	debugf("Thread %d (proc %d:%s)\n",nt->getTid(),nt->getProc()->pid,nt->getProc()->command);
#endif
#endif

	release(p,PLOCK_PROG);
	t->discardFrames();
	return nt->getTid();

error:
	release(p,PLOCK_PROG);
errorReqProc:
	t->discardFrames();
	return res;
}

int ProcBase::exec(const char *path,USER const char *const *args,const void *code,size_t size) {
	char *argBuffer;
	ELF::StartupInfo info;
	Thread *t = Thread::getRunning();
	Proc *p = request(t->getProc()->pid,PLOCK_PROG);
	size_t argSize;
	int argc,fd = -1;
	if(!p)
		return -ESRCH;
	/* don't allow exec when the process should die */
	if(p->flags & (P_ZOMBIE | P_PREZOMBIE)) {
		release(p,PLOCK_PROG);
		return -EDESTROYED;
	}
	/* we can't do an exec if we have multiple threads (init can do that, because the threads are
	 * "kernel-threads") */
	if(p->pid != 0 && sll_length(&p->threads) > 1) {
		release(p,PLOCK_PROG);
		return -EINVAL;
	}

	if(!code) {
		/* resolve path; require a path in real fs */
		inode_t nodeNo;
		if(vfs_node_resolvePath(path,&nodeNo,NULL,VFS_READ) != -EREALPATH) {
			release(p,PLOCK_PROG);
			return -EINVAL;
		}
	}

	argc = 0;
	argBuffer = NULL;
	if(args != NULL) {
		argc = buildArgs(args,&argBuffer,&argSize,!code);
		if(argc < 0) {
			release(p,PLOCK_PROG);
			return argc;
		}
	}
	Thread::addHeapAlloc(argBuffer);

	/* remove all except stack */
	doRemoveRegions(p,false);

	/* load program */
	if(code) {
		if(ELF::loadFromMem(code,size,&info) < 0)
			goto error;
	}
	else {
		if(ELF::loadFromFile(path,&info) < 0)
			goto error;
	}

	/* if its the dynamic linker, we need to give it the file-descriptor for the program to load */
	/* we need to do this here without lock, because vfs_openPath will perform a context-switch */
	if(info.linkerEntry != info.progEntry) {
		sFile *file;
		if(vfs_openPath(p->pid,VFS_READ,path,&file) < 0)
			goto error;
		fd = fd_assoc(file);
		if(fd < 0) {
			vfs_closeFile(p->pid,file);
			goto error;
		}
	}

	/* copy path so that we can identify the process */
	p->setCommand(path,argc,argBuffer);
	/* reset stats */
	p->stats.input = 0;
	p->stats.output = 0;
	p->stats.peakOwnFrames = p->ownFrames;
	p->stats.peakSharedFrames = p->sharedFrames;
	p->stats.swapCount = 0;
	p->stats.totalRuntime = 0;
	p->stats.totalSyscalls = 0;
	p->stats.totalScheds = 0;

#if DEBUG_CREATIONS
#ifdef __eco32__
	debugf("EXEC: proc %d:%s\n",p->pid,p->command);
#endif
#ifdef __mmix__
	debugf("EXEC: proc %d:%s\n",p->pid,p->command);
#endif
#endif

	/* make process ready */
	/* the entry-point is the one of the process, since threads don't start with the dl again */
	p->entryPoint = info.progEntry;
	release(p,PLOCK_PROG);
	/* for starting use the linker-entry, which will be progEntry if no dl is present */
	if(!uenv_setupProc(argc,argBuffer,argSize,&info,info.linkerEntry,fd))
		goto errorNoRel;
	Thread::remHeapAlloc(argBuffer);
	cache_free(argBuffer);
	return 0;

error:
	release(p,PLOCK_PROG);
errorNoRel:
	Thread::remHeapAlloc(argBuffer);
	cache_free(argBuffer);
	terminate(p->pid,1,SIG_COUNT);
	Thread::switchAway();
	util_panic("We should not reach this!");
	/* not reached */
	return 0;
}

void ProcBase::join(tid_t tid) {
	Thread *t = Thread::getRunning();
	/* wait until this thread doesn't exist anymore or there are no other threads than ourself */
	Proc *p = request(t->getProc()->pid,PLOCK_PROG);
	while((tid == 0 && sll_length(&t->getProc()->threads) > 1) ||
			(tid != 0 && Thread::getById(tid) != NULL)) {
		Event::wait(t,EVI_THREAD_DIED,(evobj_t)t->getProc());
		release(p,PLOCK_PROG);

		Thread::switchNoSigs();

		request(t->getProc()->pid,PLOCK_PROG);
	}
	release(p,PLOCK_PROG);
}

void ProcBase::exit(int exitCode) {
	Thread *t = Thread::getRunning();
	Proc *p = request(t->getProc()->pid,PLOCK_PROG);
	if(p) {
		p->stats.exitCode = exitCode;
		t->terminate();
		release(p,PLOCK_PROG);
	}
}

void ProcBase::segFault() {
	Thread *t = Thread::getRunning();
	addSignalFor(t->getProc()->pid,SIG_SEGFAULT);
	/* make sure that next time this exception occurs, the process is killed immediatly. otherwise
	 * we might get in an endless-loop */
	sig_unsetHandler(t->getTid(),SIG_SEGFAULT);
}

void ProcBase::addSignalFor(pid_t pid,int signal) {
	Proc *p = request(pid,PLOCK_PROG);
	if(p) {
		bool sent = false;
		sSLNode *n;
		/* don't send a signal to processes that are dying */
		if(p->flags & (P_PREZOMBIE | P_ZOMBIE)) {
			release(p,PLOCK_PROG);
			return;
		}

		for(n = sll_begin(&p->threads); n != NULL; n = n->next) {
			Thread *pt = (Thread*)n->data;
			if(sig_addSignalFor(pt->getTid(),signal))
				sent = true;
		}
		release(p,PLOCK_PROG);

		/* no handler and fatal? terminate proc! */
		if(!sent && sig_isFatal(signal)) {
			terminate(pid,1,signal);
			if(pid == getRunning())
				Thread::switchAway();
		}
	}
}

void ProcBase::terminate(pid_t pid,int exitCode,int signal) {
	sSLNode *tn;
	Proc *p = request(pid,PLOCK_PROG);
	if(!p)
		return;
	/* don't terminate processes twice */
	if(p->flags & (P_PREZOMBIE | P_ZOMBIE)) {
		release(p,PLOCK_PROG);
		return;
	}

	if(pid == 0)
		util_panic("You can't terminate the initial process");

	/* print information to log */
	if(signal != SIG_COUNT || exitCode != 0) {
		vid_printf("Process %d:%s terminated by signal %d, exitCode %d\n",
				p->pid,p->command,signal,exitCode);
	}

	/* store exit-conditions */
	p->stats.exitCode = exitCode;
	p->stats.exitSignal = signal;

	/* terminate all threads */
	for(tn = sll_begin(&p->threads); tn != NULL; tn = tn->next) {
		Thread *pt = (Thread*)tn->data;
		pt->terminate();
	}
	p->flags |= P_PREZOMBIE;
	release(p,PLOCK_PROG);
}

void ProcBase::killThread(tid_t tid) {
	Thread *t = Thread::getById(tid);
	Proc *p = request(t->getProc()->pid,PLOCK_PROG);
	assert(p != NULL);
	p->stats.totalRuntime += t->getStats().runtime;
	p->stats.totalSyscalls += t->getStats().syscalls;
	p->stats.totalScheds += t->getStats().schedCount;
	t->kill();
	sll_removeFirstWith(&p->threads,t);
	if(sll_length(&p->threads) == 0) {
		p->stats.exitCode = 0;
		doDestroy(p);
	}
	release(p,PLOCK_PROG);
}

void ProcBase::destroy(pid_t pid) {
	sSLNode *n;
	Proc *p = request(pid,PLOCK_PROG);
	if(!p)
		return;
	for(n = sll_begin(&p->threads); n != NULL; ) {
		sSLNode *next = n->next;
		Thread *pt = (Thread*)n->data;
		pt->kill();
		sll_removeFirstWith(&p->threads,pt);
		n = next;
	}
	p->stats.exitCode = 0;
	doDestroy(p);
	release(p,PLOCK_PROG);
}

void ProcBase::doDestroy(Proc *p) {
	/* release resources */
	fd_destroy(p);
	groups_leave(p->pid);
	env_removeFor(p->pid);
	doRemoveRegions(p,true);
	vmfree_destroy(&p->freemap);
	vmreg_remTree(&p->regtree);
	paging_destroyPDir(p->getPageDir());
	lock_releaseAll(p->pid);
	terminateArch(p);
	sll_clear(&p->threads,false);

	/* we are a zombie now, so notify parent that he can get our exit-state */
	p->flags &= ~P_PREZOMBIE;
	p->flags |= P_ZOMBIE;
	/* ensure that the parent-wakeup doesn't get lost */
	mutex_aquire(&childLock);
	notifyProcDied(p->parentPid);
	mutex_release(&childLock);
}

void ProcBase::kill(pid_t pid) {
	sSLNode *n;
	Proc *p = request(pid,PLOCK_PROG);
	if(!p)
		return;

	/* give childs the ppid 0 */
	mutex_aquire(&childLock);
	mutex_aquire(&procLock);
	for(n = sll_begin(&procs); n != NULL; n = n->next) {
		Proc *cp = (Proc*)n->data;
		if(cp->parentPid == p->pid) {
			cp->parentPid = 0;
			/* if this process has already died, the parent can't wait for it since its dying
			 * right now. therefore notify init of it */
			if(cp->flags & P_ZOMBIE)
				notifyProcDied(0);
		}
	}
	mutex_release(&procLock);
	mutex_release(&childLock);

	/* free the last resources and remove us from vfs */
	cache_free((char*)p->command);
	vfs_removeProcess(p->pid);

	/* remove and free */
	remove(p);
	release(p,PLOCK_PROG);
	cache_free(p);
}

void ProcBase::notifyProcDied(pid_t parent) {
	addSignalFor(parent,SIG_CHILD_TERM);
	Event::wakeup(EVI_CHILD_DIED,(evobj_t)getByPid(parent));
}

int ProcBase::waitChild(USER ExitState *state) {
	Thread *t = Thread::getRunning();
	Proc *p = t->getProc();
	int res;
	/* check if there is already a dead child-proc */
	mutex_aquire(&childLock);
	res = getExitState(p->pid,state);
	if(res < 0) {
		/* wait for child */
		Event::wait(t,EVI_CHILD_DIED,(evobj_t)p);
		mutex_release(&childLock);
		Thread::switchAway();
		/* stop waiting for event; maybe we have been waked up for another reason */
		Event::removeThread(t);
		/* don't continue here if we were interrupted by a signal */
		if(sig_hasSignalFor(t->getTid()))
			return -EINTR;
		res = getExitState(p->pid,state);
		if(res < 0)
			return res;
	}
	else
		mutex_release(&childLock);

	/* finally kill the process */
	kill(res);
	return 0;
}

int ProcBase::getExitState(pid_t ppid,USER ExitState *state) {
	sSLNode *n;
	mutex_aquire(&procLock);
	for(n = sll_begin(&procs); n != NULL; n = n->next) {
		Proc *p = (Proc*)n->data;
		if(p->parentPid == ppid && (p->flags & P_ZOMBIE)) {
			/* avoid deadlock; at other places we aquire the PLOCK_PROG first and procLock afterwards */
			mutex_release(&procLock);
			p = request(p->pid,PLOCK_PROG);
			if(state) {
				state->pid = p->pid;
				state->exitCode = p->stats.exitCode;
				state->signal = p->stats.exitSignal;
				state->runtime = p->stats.totalRuntime;
				state->schedCount = p->stats.totalScheds;
				state->syscalls = p->stats.totalSyscalls;
				state->ownFrames = p->stats.peakOwnFrames;
				state->sharedFrames = p->stats.peakSharedFrames;
				state->swapped = p->stats.swapCount;
				release(p,PLOCK_PROG);
				return p->pid;
			}
			release(p,PLOCK_PROG);
			return p->pid;
		}
	}
	mutex_release(&procLock);
	return -ECHILD;
}

void ProcBase::doRemoveRegions(Proc *p,bool remStack) {
	sSLNode *n;
	/* unset TLS-region (and stack-region, if needed) from all threads; do this first because as
	 * soon as vmm_removeAll is finished, somebody might use the stack-region-number to get the
	 * region-range, which is not possible anymore, because the region is already gone. */
	for(n = sll_begin(&p->threads); n != NULL; n = n->next) {
		Thread *t = (Thread*)n->data;
		t->removeRegions(remStack);
	}
	vmm_removeAll(p->pid,remStack);
}

void ProcBase::printAll() {
	sSLNode *n;
	for(n = sll_begin(&procs); n != NULL; n = n->next) {
		Proc *p = (Proc*)n->data;
		p->print();
	}
}

void ProcBase::printAllRegions() {
	sSLNode *n;
	for(n = sll_begin(&procs); n != NULL; n = n->next) {
		Proc *p = (Proc*)n->data;
		vmm_print(p->pid);
		vid_printf("\n");
	}
}

void ProcBase::printAllPDs(uint parts,bool regions) {
	sSLNode *n;
	for(n = sll_begin(&procs); n != NULL; n = n->next) {
		Proc *p = (Proc*)n->data;
		size_t own,shared,swapped;
		getMemUsageOf(p->pid,&own,&shared,&swapped);
		vid_printf("Process %d (%s) (%ld own, %ld sh, %ld sw):\n",
				p->pid,p->command,own,shared,swapped);
		if(regions)
			vmm_print(p->pid);
		paging_printPDir(p->getPageDir(),parts);
		vid_printf("\n");
	}
}

void ProcBase::print() {
	size_t own,shared,swap;
	sSLNode *n;
	vid_printf("Proc %d:\n",pid);
	vid_printf("\tppid=%d, cmd=%s, entry=%#Px\n",parentPid,command,entryPoint);
	vid_printf("\tOwner: ruid=%u, euid=%u, suid=%u\n",ruid,euid,suid);
	vid_printf("\tGroup: rgid=%u, egid=%u, sgid=%u\n",rgid,egid,sgid);
	vid_printf("\tGroups: ");
	groups_print(pid);
	vid_printf("\n");
	getMemUsageOf(pid,&own,&shared,&swap);
	vid_printf("\tFrames: own=%lu, shared=%lu, swapped=%lu\n",own,shared,swap);
	vid_printf("\tExitInfo: code=%u, signal=%u\n",stats.exitCode,stats.exitSignal);
	vid_printf("\tMemPeak: own=%lu, shared=%lu, swapped=%lu\n",
	           stats.peakOwnFrames,stats.peakSharedFrames,stats.swapCount);
	vid_printf("\tRunStats: runtime=%lu, scheds=%lu, syscalls=%lu\n",
	           stats.totalRuntime,stats.totalScheds,stats.totalSyscalls);
	prf_pushIndent();
	vmfree_print(&freemap);
	prf_popIndent();
	vid_printf("\tRegions:\n");
	vid_printf("\t\tDataRegion: %p\n",dataAddr);
	vid_printf("\t\tFreeStack: %p\n",freeStackAddr);
	vmm_printShort(pid,"\t\t");
	prf_pushIndent();
	env_printAllOf(pid);
	fd_print(static_cast<Proc*>(this));
	vfs_fsmsgs_printFSChans(static_cast<Proc*>(this));
	prf_popIndent();
	vid_printf("\tThreads:\n");
	for(n = sll_begin(&threads); n != NULL; n = n->next) {
		vid_printf("\t\t");
		((Thread*)n->data)->printShort();
		vid_printf("\n");
	}
	vid_printf("\n");
}

int ProcBase::buildArgs(USER const char *const *args,char **argBuffer,size_t *size,bool fromUser) {
	const char *const *arg;
	char *bufPos;
	int argc = 0;
	size_t remaining = EXEC_MAX_ARGSIZE;
	size_t len;

	/* alloc space for the arguments */
	*argBuffer = (char*)cache_alloc(EXEC_MAX_ARGSIZE);
	if(*argBuffer == NULL)
		return -ENOMEM;

	/* count args and copy them on the kernel-heap */
	/* note that we have to create a copy since we don't know where the args are. Maybe
	 * they are on the user-stack at the position we want to copy them for the
	 * new process... */
	Thread::addHeapAlloc(*argBuffer);
	bufPos = *argBuffer;
	arg = args;
	while(1) {
		/* check if it is a valid pointer */
		if(fromUser && !paging_isInUserSpace((uintptr_t)arg,sizeof(char*)))
			goto error;
		/* end of list? */
		if(*arg == NULL)
			break;

		/* check whether the string is readable */
		if(fromUser && !sysc_isStrInUserSpace(*arg,&len))
			goto error;
		else
			len = strlen(*arg);
		/* ensure that the argument is not longer than the left space */
		if(len >= remaining)
			goto error;

		/* copy to heap */
		memcpy(bufPos,*arg,len + 1);
		bufPos += len + 1;
		remaining -= len + 1;
		arg++;
		argc++;
	}
	Thread::remHeapAlloc(*argBuffer);
	/* store args-size and return argc */
	*size = EXEC_MAX_ARGSIZE - remaining;
	return argc;

error:
	Thread::remHeapAlloc(*argBuffer);
	cache_free(*argBuffer);
	return -EFAULT;
}

pid_t ProcBase::getFreePid() {
	size_t count = 0;
	while(count < MAX_PROC_COUNT) {
		/* 0 is always present */
		if(nextPid >= MAX_PROC_COUNT)
			nextPid = 1;
		if(pidToProc[nextPid++] == NULL)
			return nextPid - 1;
		count++;
	}
	return INVALID_PID;
}

bool ProcBase::add(Proc *p) {
	if(sll_append(&procs,p)) {
		pidToProc[p->pid] = p;
		return true;
	}
	return false;
}

void ProcBase::remove(Proc *p) {
	mutex_aquire(&procLock);
	sll_removeFirstWith(&procs,p);
	pidToProc[p->pid] = NULL;
	mutex_release(&procLock);
}


#if DEBUGGING

#define PROF_PROC_COUNT		128

static time_t proctimes[PROF_PROC_COUNT];

void ProcBase::dbg_startProf() {
	sSLNode *n,*m;
	Thread *t;
	for(n = sll_begin(&procs); n != NULL; n = n->next) {
		const Proc *p = (const Proc*)n->data;
		assert(p->pid < PROF_PROC_COUNT);
		proctimes[p->pid] = 0;
		for(m = sll_begin(&p->threads); m != NULL; m = m->next) {
			t = (Thread*)m->data;
			proctimes[p->pid] += t->stats.runtime;
		}
	}
}

void ProcBase::dbg_stopProf() {
	sSLNode *n,*m;
	Thread *t;
	for(n = sll_begin(&procs); n != NULL; n = n->next) {
		const Proc *p = (const Proc*)n->data;
		time_t curtime = 0;
		assert(p->pid < PROF_PROC_COUNT);
		for(m = sll_begin(&p->threads); m != NULL; m = m->next) {
			t = (Thread*)m->data;
			curtime += t->stats.runtime;
		}
		curtime -= proctimes[p->pid];
		if(curtime > 0)
			vid_printf("Process %3d (%18s): t=%08x\n",p->pid,p->command,curtime);
	}
}

#endif