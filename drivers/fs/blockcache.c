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

#include <esc/common.h>
#include <esc/heap.h>
#include <esc/debug.h>
#include <assert.h>
#include "blockcache.h"

/* for statistics */
static u32 cacheHits = 0;
static u32 cacheMisses = 0;

/**
 * Requests the given block and reads it from disk if desired
 */
static sCBlock *bcache_doRequest(sBlockCache *c,u32 blockNo,bool read);
/**
 * Fetches a block-cache-entry
 */
static sCBlock *bcache_getBlock(sBlockCache *c);

void bcache_init(sBlockCache *c) {
	u32 i;
	sCBlock *bentry;
	c->usedBlocks = NULL;
	c->freeBlocks = NULL;
	c->oldestBlock = NULL;
	c->blockCache = (sCBlock*)malloc(c->blockCacheSize * sizeof(sCBlock));
	vassert(c->blockCache != NULL,"Unable to alloc mem for blockcache");
	bentry = c->blockCache;
	for(i = 0; i < c->blockCacheSize; i++) {
		bentry->blockNo = 0;
		bentry->buffer = NULL;
		bentry->dirty = false;
		bentry->prev = (i < c->blockCacheSize - 1) ? bentry + 1 : NULL;
		bentry->next = c->freeBlocks;
		c->freeBlocks = bentry;
		bentry++;
	}
}

void bcache_destroy(sBlockCache *c) {
	c->usedBlocks = NULL;
	c->freeBlocks = NULL;
	c->oldestBlock = NULL;
	free(c->blockCache);
}

void bcache_flush(sBlockCache *c) {
	sCBlock *bentry = c->usedBlocks;
	while(bentry != NULL) {
		if(bentry->dirty) {
			vassert(c->write != NULL,"Block %d dirty, but no write-function",bentry->blockNo);
			c->write(c->handle,bentry->buffer,bentry->blockNo,1);
			bentry->dirty = false;
		}
		bentry = bentry->next;
	}
}

sCBlock *bcache_create(sBlockCache *c,u32 blockNo) {
	return bcache_doRequest(c,blockNo,false);
}

sCBlock *bcache_request(sBlockCache *c,u32 blockNo) {
	return bcache_doRequest(c,blockNo,true);
}

static sCBlock *bcache_doRequest(sBlockCache *c,u32 blockNo,bool read) {
	sCBlock *block,*bentry;

	/* search for the block. perhaps it's already in cache */
	bentry = c->usedBlocks;
	while(bentry != NULL) {
		if(bentry->blockNo == blockNo) {
			/* remove from list and put at the beginning of the usedlist because it was
			 * used most recently */
			if(bentry->prev != NULL) {
				/* update oldest */
				if(c->oldestBlock == bentry)
					c->oldestBlock = bentry->prev;
				/* remove */
				bentry->prev->next = bentry->next;
				bentry->next->prev = bentry->prev;
				/* put at the beginning */
				bentry->prev = NULL;
				bentry->next = c->usedBlocks;
				bentry->next->prev = bentry;
				c->usedBlocks = bentry;
			}
			cacheHits++;
			return bentry;
		}
		bentry = bentry->next;
	}

	/* init cached block */
	block = bcache_getBlock(c);
	if(block->buffer == NULL) {
		block->buffer = (u8*)malloc(c->blockSize);
		if(block->buffer == NULL)
			return NULL;
	}
	block->blockNo = blockNo;
	block->dirty = false;

	/* now read from disk */
	if(read && !c->read(c->handle,block->buffer,blockNo,1)) {
		block->blockNo = 0;
		return NULL;
	}

	cacheMisses++;
	return block;
}

static sCBlock *bcache_getBlock(sBlockCache *c) {
	sCBlock *block = c->freeBlocks;
	if(block != NULL) {
		/* remove from freelist and put in usedlist */
		c->freeBlocks = block->next;
		block->next->prev = NULL;
		/* block->prev is already NULL */
		block->next = c->usedBlocks;
		block->next->prev = block;
		c->usedBlocks = block;
		/* update oldest */
		if(c->oldestBlock == NULL)
			c->oldestBlock = block;
		return block;
	}

	/* take the oldest one */
	block = c->oldestBlock;
	c->oldestBlock = block->prev;
	c->oldestBlock->next = NULL;
	/* put at beginning of usedlist */
	block->prev = NULL;
	block->next = c->usedBlocks;
	block->next->prev = block;
	c->usedBlocks = block;
	/* if it is dirty we have to write it first to disk */
	if(block->dirty) {
		vassert(c->write != NULL,"Block %d dirty, but no write-function",block->blockNo);
		c->write(c->handle,block->buffer,block->blockNo,1);
	}
	return block;
}

#if DEBUGGING

void bcache_print(sBlockCache *c) {
	u32 i = 0;
	sCBlock *block;
	printf("Used blocks:\n\t");
	block = c->usedBlocks;
	while(block != NULL) {
		if(++i % 8 == 0)
			printf("\n\t");
		printf("%d ",block->blockNo);
		block = block->next;
	}
	printf("\n");
}

void bcache_printStats(void) {
	printf("[BlockCache] Hits: %d, Misses: %d; %d %%\n",cacheHits,cacheMisses,
			(u32)(100 / ((float)(cacheMisses + cacheHits) / cacheHits)));
}

#endif