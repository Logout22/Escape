/**
 * @version		$Id$
 * @author		Nils Asmussen <nils@script-solution.de>
 * @copyright	2008 Nils Asmussen
 */

#include <common.h>
#include <multiboot.h>
#include <paging.h>
#include <proc.h>
#include <elf.h>
#include <video.h>
#include <vfsnode.h>
#include <util.h>
#include <string.h>

#define CHECK_FLAG(flags,bit) (flags & (1 << bit))

sMultiBoot *mb;
static bool loadedMods = false;

void mboot_init(sMultiBoot *mbp) {
	u32 i;
	sModule *mod;

	/* save the multiboot-structure
	 * (change to 0xC...0 since we get the address at 0x0...0 from GRUB) */
	mb = (sMultiBoot*)((u32)mbp | KERNEL_AREA_V_ADDR);

	/* change the address of the pointers in the structure, too */
	mb->cmdLine = (char*)((u32)mb->cmdLine | KERNEL_AREA_V_ADDR);
	mb->modsAddr = (sModule*)((u32)mb->modsAddr | KERNEL_AREA_V_ADDR);
	mb->mmapAddr = (sMemMap*)((u32)mb->mmapAddr | KERNEL_AREA_V_ADDR);
	mod = mb->modsAddr;
	for(i = 0; i < mb->modsCount; i++) {
		mod->modStart |= KERNEL_AREA_V_ADDR;
		mod->modEnd |= KERNEL_AREA_V_ADDR;
		mod->name = (char*)((u32)mod->name | KERNEL_AREA_V_ADDR);
		mod++;
	}
}

void mboot_loadModules(sIntrptStackFrame *stack) {
	u32 i;
	sProc *p;
	char *name;
	char *service;
	tVFSNodeNo nodeNo;
	sModule *mod = mb->modsAddr;

	/* it's not good to do this twice.. */
	if(loadedMods)
		return;

	loadedMods = true;
	for(i = 0; i < mb->modsCount; i++) {
		name = mod->name;
		service = strchr(name,' ') + 1;
		service[-1] = '\0';

		/* clone proc */
		tPid pid = proc_getFreePid();
		if(pid == INVALID_PID)
			util_panic("No free process-slots");

		if(proc_clone(pid)) {
			/* we'll reach this as soon as the scheduler has chosen the created process */
			p = proc_getRunning();
			/* remove data-pages */
			proc_changeSize(-p->dataPages,CHG_DATA);
			/* now load service */
			memcpy(p->command,name,strlen(name) + 1);
			elf_loadprog((u8*)mod->modStart);
			proc_setupIntrptStack(stack,0,NULL,0);
			/* we don't want to continue the loop ;) */
			break;
		}

		/* wait until the service is registered */
		vid_printf("Waiting for '%s'",service);
		while(vfsn_resolvePath(service,&nodeNo) < 0) {
			vid_printf(".");
			proc_switch();
		}
		vid_toLineEnd(strlen("DONE"));
		vid_printf("\033f\x02""DONE\033r\x0");

		mod++;
	}
}

u32 mboot_getUsableMemCount(void) {
	sMemMap *mmap;
	u32 size = 0;
	for(mmap = mb->mmapAddr;
		(u32)mmap < (u32)mb->mmapAddr + mb->mmapLength;
		mmap = (sMemMap*)((u32)mmap + mmap->size + sizeof(mmap->size))) {
		if(mmap != NULL && mmap->type == MMAP_TYPE_AVAILABLE)
			size += mmap->length;
	}
	return size;
}


/* #### TEST/DEBUG FUNCTIONS #### */
#if DEBUGGING

void mboot_dbg_print(void) {
	u32 x;
	sMemMap *mmap;
	vid_printf("MultiBoot-Structure:\n---------------------\nflags=0x%x\n",mb->flags);
	if(CHECK_FLAG(mb->flags,0)) {
		vid_printf("memLower=%d KB, memUpper=%d KB\n",mb->memLower,mb->memUpper);
	}
	if(CHECK_FLAG(mb->flags,1)) {
		vid_printf("biosDriveNumber=%d, part1=%d, part2=%d, part3=%d\n",mb->bootDevice.drive,
			mb->bootDevice.partition1,mb->bootDevice.partition2,mb->bootDevice.partition3);
	}
	if(CHECK_FLAG(mb->flags,2)) {
		vid_printf("cmdLine=%s\n",mb->cmdLine);
	}
	if(CHECK_FLAG(mb->flags,3)) {
		u32 i;
		sModule *mod = mb->modsAddr;
		vid_printf("modsCount=%d:\n",mb->modsCount);
		for(i = 0; i < mb->modsCount; i++) {
			vid_printf("\tname=%s, start=0x%x, end=0x%x\n",mod->name ? mod->name : "<NULL>",
					mod->modStart,mod->modEnd);
			mod++;
		}
	}
	if(CHECK_FLAG(mb->flags,4)) {
		vid_printf("a.out: tabSize=%d, strSize=%d, addr=0x%x\n",mb->syms.aDotOut.tabSize,mb->syms.aDotOut.strSize,
			mb->syms.aDotOut.addr);
	}
	else if(CHECK_FLAG(mb->flags,5)) {
		vid_printf("ELF: num=%d, size=%d, addr=0x%x, shndx=0x%x\n",mb->syms.ELF.num,mb->syms.ELF.size,
			mb->syms.ELF.addr,mb->syms.ELF.shndx);
	}
	if(CHECK_FLAG(mb->flags,6)) {
		vid_printf("mmapLength=%d, mmapAddr=0x%x\n",mb->mmapLength,mb->mmapAddr);
		vid_printf("Available memory:\n");
		x = 0;
		for(mmap = (sMemMap*)mb->mmapAddr;
			(u32)mmap < (u32)mb->mmapAddr + mb->mmapLength;
			mmap = (sMemMap*)((u32)mmap + mmap->size + sizeof(mmap->size))) {
			if(mmap != NULL && mmap->type == MMAP_TYPE_AVAILABLE) {
				vid_printf("\t%d: addr=0x%08x, size=0x%08x, type=0x%08x\n",
						x,(u32)mmap->baseAddr,(u32)mmap->length,mmap->type);
				x++;
			}
		}
	}

	vid_printf("---------------------\n");
}

#endif
