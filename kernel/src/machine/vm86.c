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
#include <task/proc.h>
#include <task/thread.h>
#include <task/sched.h>
#include <task/signals.h>
#include <mem/kheap.h>
#include <mem/text.h>
#include <mem/paging.h>
#include <machine/vm86.h>
#include <util.h>
#include <video.h>
#include <string.h>
#include <assert.h>
#include <errors.h>

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

#define DBGVM86(fmt...)	/*vid_printf(fmt)*/

static u16 vm86_popw(sIntrptStackFrame *stack);
static u32 vm86_popl(sIntrptStackFrame *stack);
static void vm86_pushw(sIntrptStackFrame *stack,u16 word);
static void vm86_pushl(sIntrptStackFrame *stack,u32 l);
static void vm86_copyResult(sIntrptStackFrame* stack,sVM86Info *info);
static sVM86Info *vm86_createInfo(tPid pid,sVM86Regs *regs);
static void vm86_destroyInfo(sVM86Info *info);

s32 vm86_int(u16 interrupt,sVM86Regs *regs,sVM86Memarea *areas,u16 areaCount) {
	u32 frameNos[(1024 * K) / PAGE_SIZE];
	sIntrptStackFrame *istack;
	sVM86Info *info;
	u32 *ivt;
	u32 *mframeNos;
	sThread *t;
	sProc *p;
	s32 res;
	u32 i;
	tTid tid;
	tPid pid;

	for(i = 0; i < areaCount; i++) {
		if(BYTES_2_PAGES(areas[i].size) > VM86_MAX_MEMPAGES)
			return ERR_INVALID_ARGS;
	}
	if(interrupt >= VM86_IVT_SIZE)
		return ERR_INVALID_ARGS;
	t = thread_getRunning();
	pid = proc_getFreePid();
	if(pid == INVALID_PID)
		return ERR_NO_FREE_PROCS;

	/* store information in calling process */
	tid = t->tid;
	info = vm86_createInfo(pid,regs);
	if(info == NULL)
		return ERR_NOT_ENOUGH_MEM;
	/* store in calling process */
	t->proc->vm86Info = info;

	mframeNos = (u32*)kheap_alloc(areaCount * VM86_MAX_MEMPAGES * sizeof(u32));
	if(mframeNos == NULL) {
		vm86_destroyInfo(info);
		return ERR_NOT_ENOUGH_MEM;
	}

	/* create child */
	/* Note that it is really necessary to set wether we're a VM86-task or not BEFORE we get
	 * chosen by the scheduler the first time. Otherwise the scheduler can't set the right
	 * value for tss.esp0 and we will get a wrong stack-layout on the next interrupt */
	res = proc_clone(pid,true);
	if(res < 0) {
		kheap_free(mframeNos);
		vm86_destroyInfo(info);
		t->proc->vm86Info = NULL;
		return res;
	}
	/* parent */
	if(res == 0) {
		/* first block the calling thread and then do a switch */
		/* we'll wakeup the thread as soon as the child is done with the interrupt */
		sched_setBlocked(thread_getRunning());
		thread_switch();
		/* everything is finished :) */
		memcpy(regs,&info->regs,sizeof(sVM86Regs));
		/* kill vm86-process */
		proc_kill(proc_getByPid(info->vm86Pid));
		vm86_destroyInfo(info);
		/* mark as destroyed */
		p = proc_getRunning();
		p->vm86Info = NULL;
		return 0;
	}

	/* child */
	t = thread_getRunning();
	p = t->proc;
	p->vm86Caller = tid;

	/* first, collect the frame-numbers for the mapping (before unmapping the area) */
	for(i = 0; i < areaCount; i++)
		paging_getFrameNos(mframeNos + i * VM86_MAX_MEMPAGES,(u32)areas[i].src,areas[i].size);

	/* free the current text; free frames if text_free() returns true */
	paging_unmap(0,p->textPages,text_free(p->text,p->pid),false);
	/* ensure that we don't have a text-usage anymore */
	p->text = NULL;
	/* remove process-data */
	proc_changeSize(-p->dataPages,CHG_DATA);
	/* Note that we HAVE TO do it behind the proc_changeSize() call since the data-pages are
	 * still behind the text-pages, no matter if we've already unmapped the text-pages or not,
	 * and proc_changeSize() trusts p->textPages */
	p->textPages = 0;

	/* Now map the first MiB of physical memory to 0x00000000 and the first 64 KiB to 0x00100000,
	 * too. Because in real-mode it occurs an address-wraparound at 1 MiB. In VM86-mode it doesn't
	 * therefore we have to emulate it. We do that by simply mapping the same to >= 1MiB. */
	for(i = 0; i < ARRAY_SIZE(frameNos); i++)
		frameNos[i] = i;
	paging_map(0x00000000,frameNos,ARRAY_SIZE(frameNos),PG_NOFREE | PG_WRITABLE,true);
	paging_map(0x00100000,frameNos,(64 * K) / PAGE_SIZE,PG_NOFREE | PG_WRITABLE,true);

	/* map the specified areas to the frames of the parent, so that the BIOS can write
	 * it directly to the buffer of the calling process */
	for(i = 0; i < areaCount; i++) {
		u32 pages = BYTES_2_PAGES(areas[i].size);
		paging_map(areas[i].dst,mframeNos + i * VM86_MAX_MEMPAGES,pages,PG_WRITABLE,true);
	}
	kheap_free(mframeNos);

	/* now, remove the stack because we don't need it anymore (we needed it before to access
	 * the areas!) */
	proc_changeSize(-p->stackPages,CHG_STACK);

	/* TODO we need no io-map here, right? */
	kheap_free(p->ioMap);
	p->ioMap = NULL;

	/* give it a name */
	strcpy(p->command,"VM86");

	istack = intrpt_getCurStack();
	/* set stack-pointer (in an unsed area) */
	istack->uss = 0x9000;
	istack->uesp = 0x0FFC;
	/* enable VM flag */
	istack->eflags |= 1 << 17;
	/* set entrypoint */
	ivt = (u32*)0;
	istack->eip = ivt[interrupt] & 0xFFFF;
	istack->cs = ivt[interrupt] >> 16;
	/* segment registers */
	istack->vm86ds = info->regs.ds;
	istack->vm86es = info->regs.es;
	istack->vm86fs = 0;
	istack->vm86gs = 0;
	/* general purpose registers */
	istack->eax = info->regs.ax;
	istack->ebx = info->regs.bx;
	istack->ecx = info->regs.cx;
	istack->edx = info->regs.dx;
	istack->esi = info->regs.si;
	istack->edi = info->regs.di;
	return 0;
}

bool vm86_handleGPF(sIntrptStackFrame *stack) {
	u8 *ops = (u8*)(stack->eip + (stack->cs << 4));
	u8 opCode;
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
			u16 *sp;
			u32 *ivt,intno = *ops;
			stack->uesp -= sizeof(u16) * 3;
			sp = (u16*)(stack->uesp + (stack->uss << 4));
			/* save eflags and ip on stack */
			sp[0] = (u16)stack->eflags;
			sp[1] = (u16)stack->cs;
			sp[2] = (u16)stack->eip + 2;
			/* set new ip */
			ivt = (u32*)0;
			assert(intno < VM86_IVT_SIZE);
			DBGVM86("[VM86] int 0x%x @ %x:%x -> %x:%x\n",intno,stack->cs,stack->eip + 2,
					ivt[intno] >> 16,ivt[intno] & 0xFFFF);
			stack->eip = ivt[intno] & 0xFFFF;
			stack->cs = ivt[intno] >> 16;
		}
		break;
		case X86OP_IRET: {
			u32 newip,newcs,newflags;
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
				sProc *p = proc_getRunning();
				sThread *caller = thread_getById(p->vm86Caller);
				if(caller != NULL) {
					vm86_copyResult(stack,caller->proc->vm86Info);
					sched_setReady(caller);
				}
				/* commit suicide and do a switch */
				proc_terminate(p,0,SIG_COUNT);
				thread_switch();
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
				vm86_pushw(stack,(u16)stack->eflags);
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
			util_outWord(stack->edx,stack->eax);
			stack->eip++;
			break;
		case X86OP_OUTB:
			DBGVM86("[VM86] outb (0x%x -> 0x%x)\n",stack->eax,stack->edx);
			util_outByte(stack->edx,stack->eax);
			stack->eip++;
			break;
		case X86OP_INW:
            stack->eax = util_inWord(stack->edx);
			DBGVM86("[VM86] inw (0x%x <- 0x%x)\n",stack->eax,stack->edx);
			stack->eip++;
			break;
		case X86OP_INB:
			stack->eax = (stack->eax & 0xFF00) + util_inByte(stack->edx);
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
			util_panic("Invalid opcode (0x%x) @ 0x%x",opCode,(u32)(ops - 1));
			break;
	}
	return true;
}

static u16 vm86_popw(sIntrptStackFrame *stack) {
	u16 *sp = (u16*)(stack->uesp + (stack->uss << 4));
	stack->uesp += sizeof(u16);
	return *sp;
}

static u32 vm86_popl(sIntrptStackFrame *stack) {
	u32 *sp = (u32*)(stack->uesp + (stack->uss << 4));
	stack->uesp += sizeof(u32);
	return *sp;
}

static void vm86_pushw(sIntrptStackFrame *stack,u16 word) {
	stack->uesp -= sizeof(u16);
	*((u16*)(stack->uesp + (stack->uss << 4))) = word;
}

static void vm86_pushl(sIntrptStackFrame *stack,u32 l) {
	stack->uesp -= sizeof(u32);
	*((u32*)(stack->uesp + (stack->uss << 4))) = l;
}

static void vm86_copyResult(sIntrptStackFrame* stack,sVM86Info *info) {
	info->regs.ax = stack->eax;
	info->regs.bx = stack->ebx;
	info->regs.cx = stack->ecx;
	info->regs.dx = stack->edx;
	info->regs.si = stack->esi;
	info->regs.di = stack->edi;
	info->regs.ds = stack->vm86ds;
	info->regs.es = stack->vm86es;
}

static sVM86Info *vm86_createInfo(tPid pid,sVM86Regs *regs) {
	sVM86Info *info = kheap_alloc(sizeof(sVM86Info));
	if(info == NULL)
		return NULL;
	info->vm86Pid = pid;
	memcpy(&info->regs,regs,sizeof(sVM86Regs));
	return info;
}

static void vm86_destroyInfo(sVM86Info *info) {
	kheap_free(info);
}