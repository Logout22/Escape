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
#include <mem/paging.h>
#include <mem/pmem.h>
#include <video.h>
#include <test.h>
#include "tpaging.h"

/* forward declarations */
static void test_paging(void);
static bool test_paging_cycle(u32 addr,u32 count);
static void test_paging_allocate(u32 addr,u32 count);
static void test_paging_access(u32 addr,u32 count);
static void test_paging_free(u32 addr,u32 count);

/* our test-module */
sTestModule tModPaging = {
	"Paging",
	&test_paging
};

#define TEST_MAX_FRAMES 1024*3
u32 frames[TEST_MAX_FRAMES];

static void test_paging(void) {
	u32 x,y;
	u32 addr[] = {0x0,0xB0000000,0xA0000000,0x4000,0x1234};
	u32 count[] = {0,1,50,1024,1025,2048,2051};

	for(y = 0; y < ARRAY_SIZE(addr); y++) {
		for(x = 0; x < ARRAY_SIZE(count); x++) {
			test_paging_cycle(addr[y],count[x]);
		}
	}
}

static bool test_paging_cycle(u32 addr,u32 count) {
	u32 oldFF, newFF, oldPC, newPC;

	test_caseStart("Mapping %d pages to 0x%08x",count,addr);

	oldPC = paging_dbg_getPageCount();
	oldFF = mm_getFreeFrmCount(MM_DMA | MM_DEF);

	test_paging_allocate(addr,count);
	test_paging_access(addr,count);
	test_paging_free(addr,count);

	newPC = paging_dbg_getPageCount();
	newFF = mm_getFreeFrmCount(MM_DMA | MM_DEF);

	if(oldFF != newFF || oldPC != newPC) {
		test_caseFailed("oldPC=%d, oldFF=%d, newPC=%d, newFF=%d",oldPC,oldFF,newPC,newFF);
		return false;
	}

	test_caseSucceded();

	return true;
}

static void test_paging_allocate(u32 addr,u32 count) {
	mm_allocateFrames(MM_DEF,frames,count);
	paging_map(addr,frames,count,PG_WRITABLE,false);
}

static void test_paging_access(u32 addr,u32 count) {
	u32 i;
	/* make page-aligned */
	addr &= ~(PAGE_SIZE - 1);
	for(i = 0; i < count; i++) {
		/* write to the first word */
		*(u32*)addr = 0xDEADBEEF;
		/* write to the last word */
		*(u32*)(addr + PAGE_SIZE - sizeof(u32)) = 0xDEADBEEF;
		addr += PAGE_SIZE;
	}
}

static void test_paging_free(u32 addr,u32 count) {
	paging_unmap(addr,count,false,false);
	if(count > 0) {
		/* unmap & remove affected page-tables */
		paging_unmapPageTables(ADDR_TO_PDINDEX(addr),
				PAGES_TO_PTS((addr - (addr & ~(PAGE_SIZE * PT_ENTRY_COUNT - 1))) / PAGE_SIZE + count));
	}
	mm_freeFrames(MM_DEF,frames,count);
}