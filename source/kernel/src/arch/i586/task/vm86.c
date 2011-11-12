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
#include <sys/arch/i586/task/vm86.h>
#include <sys/arch/i586/task/ioports.h>
#include <sys/arch/i586/ports.h>
#include <sys/arch/i586/gdt.h>
#include <sys/task/proc.h>
#include <sys/task/thread.h>
#include <sys/task/sched.h>
#include <sys/task/signals.h>
#include <sys/task/event.h>
#include <sys/mem/cache.h>
#include <sys/mem/paging.h>
#include <sys/mem/vmm.h>
#include <sys/video.h>
#include <sys/mutex.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define X86OP_INT			0xCD
#define X86OP_IRET			0xCF
#define X86OP_PUSHF			0x9C
#define X86OP_POPF			0x9D
#define X86OP_OUTW			0xEF
#define X86OP_OUTB			0xEE
#define X86OP_INW			0xED
#define X86OP_INB			0xEC
#define X86OP_STI			0xFA
#define X86OP_CLI			0xFB

#define X86OP_DATA32		0x66
#define X86OP_ADDR32		0x67
#define X86OP_CS			0x2E
#define X86OP_DS			0x3E
#define X86OP_ES			0x26
#define X86OP_SS			0x36
#define X86OP_GS			0x65
#define X86OP_FS			0x64
#define X86OP_REPNZ			0xF2
#define X86OP_REP			0xF3

#define VM86_IVT_SIZE		256
#define VM86_MAX_MEMPAGES	2

#define DBGVM86(fmt...)		/*vid_printf(fmt)*/

static uint16_t vm86_popw(sIntrptStackFrame *stack);
static uint32_t vm86_popl(sIntrptStackFrame *stack);
static void vm86_pushw(sIntrptStackFrame *stack,uint16_t word);
static void vm86_pushl(sIntrptStackFrame *stack,uint32_t l);
static void vm86_start(void);
static void vm86_stop(sIntrptStackFrame *stack);
static void vm86_finish(void);
static void vm86_copyRegResult(sIntrptStackFrame* stack);
static int vm86_storeAreaResult(void);
static void vm86_copyAreaResult(void);
static bool vm86_copyInfo(uint16_t interrupt,USER const sVM86Regs *regs,
		USER const sVM86Memarea *area);
static void vm86_clearInfo(void);

static frameno_t frameNos[(1024 * K) / PAGE_SIZE];
static tid_t vm86Tid = INVALID_TID;
static volatile tid_t caller = INVALID_TID;
static sVM86Info info;
static int vm86Res = -1;
static mutex_t vm86Lock;

int vm86_create(void) {
	sProc *p;
	sThread *t;
	size_t i,frameCount;
	int res;

	/* create child */
	/* Note that it is really necessary to set whether we're a VM86-task or not BEFORE we get
	 * chosen by the scheduler the first time. Otherwise the scheduler can't set the right
	 * value for tss.esp0 and we will get a wrong stack-layout on the next interrupt */
	res = proc_clone(P_VM86);
	if(res < 0)
		return res;
	/* parent */
	if(res != 0)
		return 0;

	t = thread_getRunning();
	p = t->proc;
	vm86Tid = t->tid;

	/* remove all regions */
	proc_removeRegions(p->pid,true);

	/* Now map the first MiB of physical memory to 0x00000000 and the first 64 KiB to 0x00100000,
	 * too. Because in real-mode it occurs an address-wraparound at 1 MiB. In VM86-mode it doesn't
	 * therefore we have to emulate it. We do that by simply mapping the same to >= 1MiB. */
	frameCount = (1024 * K) / PAGE_SIZE;
	for(i = 0; i < frameCount; i++)
		frameNos[i] = i;
	paging_map(0x00000000,frameNos,frameCount,PG_PRESENT | PG_WRITABLE);
	paging_map(0x00100000,frameNos,(64 * K) / PAGE_SIZE,PG_PRESENT | PG_WRITABLE);

	/* Give the vm86-task permission for all ports. As it seems vmware expects that if they
	 * have used the 32-bit-data-prefix once (at least for inw) it takes effect for the
	 * following instructions, too!? By giving the task the permission to perform port I/O
	 * directly we prevent this problem :) */
	/* FIXME but there has to be a better way.. */
	if(p->archAttr.ioMap == NULL)
		p->archAttr.ioMap = (uint8_t*)cache_alloc(IO_MAP_SIZE / 8);
	/* note that we HAVE TO request all ports (even the reserved ones); otherwise it doesn't work
	 * everywhere (e.g. my notebook needs it) */
	if(p->archAttr.ioMap != NULL)
		memset(p->archAttr.ioMap,0x00,IO_MAP_SIZE / 8);

	/* give it a name */
	proc_setCommand(p,"VM86");

	/* block us; we get waked up as soon as someone wants to use us */
	ev_block(t);
	thread_switch();

	/* ok, we're back again... */
	vm86_start();
	/* never reached */
	return 0;
}

int vm86_int(uint16_t interrupt,USER sVM86Regs *regs,USER const sVM86Memarea *area) {
	sThread *t;
	sThread *vm86t;
	if(area && BYTES_2_PAGES(area->size) > VM86_MAX_MEMPAGES)
		return -EINVAL;
	if(interrupt >= VM86_IVT_SIZE)
		return -EINVAL;
	t = thread_getRunning();

	/* check whether there still is a vm86-task */
	vm86t = thread_getById(vm86Tid);
	if(vm86t == NULL || !(vm86t->proc->flags & P_VM86))
		return -ESRCH;

	/* ensure that only one thread at a time can use the vm86-task */
	mutex_aquire(t,&vm86Lock);
	/* store information in calling process */
	caller = t->tid;
	if(!vm86_copyInfo(interrupt,regs,area)) {
		vm86_finish();
		return -ENOMEM;
	}

	/* reserve frames for vm86-thread */
	if(info.area) {
		if(!thread_reserveFrames(vm86t,BYTES_2_PAGES(info.area->size))) {
			vm86_finish();
			return -ENOMEM;
		}
	}

	/* make vm86 ready */
	ev_unblock(vm86t);

	/* block the calling thread and then do a switch */
	/* we'll wakeup the thread as soon as the vm86-task is done with the interrupt */
	ev_block(t);
	thread_switch();

	/* everything is finished :) */
	if(vm86Res == 0) {
		thread_addCallback(t,vm86_finish);
		memcpy(regs,&info.regs,sizeof(sVM86Regs));
		vm86_copyAreaResult();
		thread_remCallback(t,vm86_finish);
	}

	/* mark as done */
	vm86_finish();
	return vm86Res;
}

void vm86_handleGPF(sIntrptStackFrame *stack) {
	uint8_t *ops = (uint8_t*)(stack->eip + (stack->cs << 4));
	uint8_t opCode;
	bool data32 = false;
	bool pref_done = false;
	do {
		switch((opCode = *ops)) {
			case X86OP_DATA32:
				data32 = true;
				break;
			case X86OP_ADDR32: break;
			case X86OP_CS: break;
			case X86OP_DS: break;
			case X86OP_ES: break;
			case X86OP_SS: break;
			case X86OP_GS: break;
			case X86OP_FS: break;
			case X86OP_REPNZ: break;
			case X86OP_REP: break;
			default:
				pref_done = true;
				break;
		}
		ops++;
	}
	while(!pref_done);

	switch(opCode) {
		case X86OP_INT: {
			uint16_t *sp;
			volatile uint32_t *ivt; /* has to be volatile to prevent llvm from optimizing it away */
			uint32_t intno = *ops;
			stack->uesp -= sizeof(uint16_t) * 3;
			sp = (uint16_t*)(stack->uesp + (stack->uss << 4));
			/* save eflags and ip on stack */
			sp[0] = (uint16_t)stack->eflags;
			sp[1] = (uint16_t)stack->cs;
			sp[2] = (uint16_t)stack->eip + 2;
			/* set new ip */
			ivt = (uint32_t*)0;
			assert(intno < VM86_IVT_SIZE);
			DBGVM86("[VM86] int 0x%x @ %x:%x -> %x:%x\n",intno,stack->cs,stack->eip + 2,
					ivt[intno] >> 16,ivt[intno] & 0xFFFF);
			stack->eip = ivt[intno] & 0xFFFF;
			stack->cs = ivt[intno] >> 16;
		}
		break;
		case X86OP_IRET: {
			uint32_t newip,newcs,newflags;
			if(data32) {
				newflags = vm86_popl(stack);
				newcs = vm86_popl(stack);
				newip = vm86_popl(stack);
			}
			else {
				newflags = (stack->eflags & 0xFFFF0000) | vm86_popw(stack);
				newcs = vm86_popw(stack);
				newip = vm86_popw(stack);
			}
			DBGVM86("[VM86] iret -> (%x:%x,0x%x)\n",newcs,newip,newflags);
			/* eip = cs = 0 means we're done */
			if(newip == 0 && newcs == 0) {
				DBGVM86("[VM86] done\n");
				vm86_stop(stack);
				/* don't continue here */
				return;
			}
            stack->eip = newip;
            stack->cs = newcs;
            /* don't disable interrupts */
            stack->eflags = newflags | (1 << 9);
		}
		break;
		case X86OP_PUSHF:
			/* save eflags */
			DBGVM86("[VM86] pushf (0x%x)\n",stack->eflags);
			if(data32)
				vm86_pushl(stack,stack->eflags);
			else
				vm86_pushw(stack,(uint16_t)stack->eflags);
			stack->eip++;
			break;
		case X86OP_POPF:
			/* restore eflags (don't disable interrupts) */
			if(data32)
				stack->eflags = vm86_popl(stack) | (1 << 9);
			else
				stack->eflags = (stack->eflags & 0xFFFF0000) | vm86_popw(stack) | (1 << 9);
			DBGVM86("[VM86] popf (0x%x)\n",stack->eflags);
			stack->eip++;
			break;
		case X86OP_OUTW:
			DBGVM86("[VM86] outw (0x%x -> 0x%x)\n",stack->eax,stack->edx);
			if(data32)
				ports_outDWord(stack->edx,stack->eax);
			else
				ports_outWord(stack->edx,stack->eax);
			stack->eip++;
			break;
		case X86OP_OUTB:
			DBGVM86("[VM86] outb (0x%x -> 0x%x)\n",stack->eax,stack->edx);
			ports_outByte(stack->edx,stack->eax);
			stack->eip++;
			break;
		case X86OP_INW:
			if(data32)
				stack->eax = ports_inDWord(stack->edx);
			else
				stack->eax = ports_inWord(stack->edx);
			DBGVM86("[VM86] inw (0x%x <- 0x%x)\n",stack->eax,stack->edx);
			stack->eip++;
			break;
		case X86OP_INB:
			stack->eax = (stack->eax & 0xFF00) + ports_inByte(stack->edx);
			DBGVM86("[VM86] inb (0x%x <- 0x%x)\n",stack->eax,stack->edx);
			stack->eip++;
			break;
		case X86OP_STI:
			stack->eip++;
			break;
		case X86OP_CLI:
			stack->eip++;
			break;
		default:
			util_panic("Invalid opcode (0x%x) @ 0x%x",opCode,(uintptr_t)(ops - 1));
			break;
	}
}

static uint16_t vm86_popw(sIntrptStackFrame *stack) {
	uint16_t *sp = (uint16_t*)(stack->uesp + (stack->uss << 4));
	stack->uesp += sizeof(uint16_t);
	return *sp;
}

static uint32_t vm86_popl(sIntrptStackFrame *stack) {
	uint32_t *sp = (uint32_t*)(stack->uesp + (stack->uss << 4));
	stack->uesp += sizeof(uint32_t);
	return *sp;
}

static void vm86_pushw(sIntrptStackFrame *stack,uint16_t word) {
	stack->uesp -= sizeof(uint16_t);
	*((uint16_t*)(stack->uesp + (stack->uss << 4))) = word;
}

static void vm86_pushl(sIntrptStackFrame *stack,uint32_t l) {
	stack->uesp -= sizeof(uint32_t);
	*((uint32_t*)(stack->uesp + (stack->uss << 4))) = l;
}

static void vm86_start(void) {
	volatile uint32_t *ivt; /* has to be volatile to prevent llvm from optimizing it away */
	sIntrptStackFrame *istack;
	sThread *t = thread_getRunning();
	assert(caller != INVALID_TID);

	istack = thread_getIntrptStack(t);

	/* copy the area to vm86; important: don't let the bios overwrite itself. therefore
	 * we map other frames to that area. */
	if(info.area) {
		/* can't fail */
		assert(paging_map(info.area->dst,NULL,BYTES_2_PAGES(info.area->size),
				PG_PRESENT | PG_WRITABLE) >= 0);
		memcpy((void*)info.area->dst,info.copies[0],info.area->size);
	}

	/* set stack-pointer (in an unsed area) */
	istack->uss = 0x9000;
	istack->uesp = 0x0FFC;
	/* enable VM flag */
	istack->eflags |= 1 << 17;
	/* set entrypoint */
	ivt = (uint32_t*)0;
	istack->eip = ivt[info.interrupt] & 0xFFFF;
	istack->cs = ivt[info.interrupt] >> 16;
	/* segment registers */
	istack->vm86ds = info.regs.ds;
	istack->vm86es = info.regs.es;
	istack->vm86fs = 0;
	istack->vm86gs = 0;
	/* general purpose registers */
	istack->eax = info.regs.ax;
	istack->ebx = info.regs.bx;
	istack->ecx = info.regs.cx;
	istack->edx = info.regs.dx;
	istack->esi = info.regs.si;
	istack->edi = info.regs.di;
}

static void vm86_stop(sIntrptStackFrame *stack) {
	sThread *t = thread_getRunning();
	sThread *ct = thread_getById(caller);
	vm86Res = 0;
	if(ct != NULL) {
		vm86_copyRegResult(stack);
		vm86Res = vm86_storeAreaResult();
		ev_unblock(ct);
	}

	/* block us and do a switch */
	ev_block(t);
	thread_switch();

	/* lets start with a new request :) */
	vm86_start();
}

static void vm86_finish(void) {
	sThread *t = thread_getById(caller);
	if(info.area)
		thread_discardFrames(thread_getById(vm86Tid));
	vm86_clearInfo();
	caller = INVALID_TID;
	mutex_release(t,&vm86Lock);
}

static void vm86_copyRegResult(sIntrptStackFrame *stack) {
	info.regs.ax = stack->eax;
	info.regs.bx = stack->ebx;
	info.regs.cx = stack->ecx;
	info.regs.dx = stack->edx;
	info.regs.si = stack->esi;
	info.regs.di = stack->edi;
	info.regs.ds = stack->vm86ds;
	info.regs.es = stack->vm86es;
}

static int vm86_storeAreaResult(void) {
	size_t i;
	if(info.area) {
		uintptr_t start = info.area->dst / PAGE_SIZE;
		size_t pages = BYTES_2_PAGES(info.area->size);
		/* copy the result to heap; we'll copy it later to the calling process */
		memcpy(info.copies[0],(void*)info.area->dst,info.area->size);
		for(i = 0; i < info.area->ptrCount; i++) {
			uintptr_t rmAddr = *(uintptr_t*)((uintptr_t)info.copies[0] + info.area->ptr[i].offset);
			uintptr_t virt = ((rmAddr & 0xFFFF0000) >> 12) | (rmAddr & 0xFFFF);
			memcpy((void*)info.copies[i + 1],(void*)virt,info.area->ptr[i].size);
		}
		/* undo mapping */
		paging_unmap(info.area->dst,pages,true);
		for(i = 0; i < pages; i++)
			frameNos[start + i] = start + i;
		assert(paging_map(info.area->dst,frameNos + start,pages,PG_PRESENT | PG_WRITABLE) == 0);
	}
	return 0;
}

static void vm86_copyAreaResult(void) {
	size_t i;
	if(info.area) {
		memcpy(info.area->src,info.copies[0],info.area->size);
		if(info.area->ptrCount > 0) {
			for(i = 0; i < info.area->ptrCount; i++)
				memcpy((void*)info.area->ptr[i].result,info.copies[i + 1],info.area->ptr[i].size);
		}
	}
}

static bool vm86_copyInfo(uint16_t interrupt,USER const sVM86Regs *regs,
		USER const sVM86Memarea *area) {
	info.interrupt = interrupt;
	memcpy(&info.regs,regs,sizeof(sVM86Regs));
	info.copies = NULL;
	info.area = NULL;
	if(area) {
		size_t i;
		/* copy area */
		info.area = (sVM86Memarea*)cache_alloc(sizeof(sVM86Memarea));
		if(info.area == NULL)
			return false;
		memcpy(info.area,area,sizeof(sVM86Memarea));
		/* copy ptrs */
		info.area->ptr = NULL;
		if(info.area->ptrCount > 0) {
			info.area->ptr = cache_alloc(sizeof(sVM86AreaPtr) * info.area->ptrCount);
			if(!info.area->ptr) {
				vm86_clearInfo();
				return false;
			}
			memcpy(info.area->ptr,area->ptr,sizeof(sVM86AreaPtr) * info.area->ptrCount);
		}
		/* create buffers for the data-exchange */
		info.copies = (void**)cache_calloc(1 + area->ptrCount,sizeof(void*));
		info.copies[0] = cache_alloc(info.area->size);
		if(!info.copies[0]) {
			vm86_clearInfo();
			return false;
		}
		memcpy(info.copies[0],area->src,area->size);
		for(i = 0; i < area->ptrCount; i++) {
			void *copy = cache_alloc(area->ptr[i].size);
			if(!copy) {
				vm86_clearInfo();
				return false;
			}
			info.copies[i + 1] = copy;
		}
	}
	return true;
}

static void vm86_clearInfo(void) {
	size_t i;
	if(info.area) {
		for(i = 0; i <= info.area->ptrCount; i++)
			cache_free(info.copies[i]);
		cache_free(info.area->ptr);
		cache_free(info.copies);
		cache_free(info.area);
	}
}