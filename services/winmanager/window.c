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
#include <esc/rect.h>
#include <esc/mem.h>
#include <esc/fileio.h>
#include <esc/heap.h>
#include <esc/proc.h>
#include <esc/service.h>
#include <esc/debug.h>
#include <esc/io.h>
#include <stdlib.h>
#include <sllist.h>
#include <string.h>
#include "window.h"

static void win_repaint(sRectangle *r,sWindow *win,s16 z);
static void win_sendActive(tWinId id,bool isActive,tCoord mouseX,tCoord mouseY);
static void win_sendRepaint(tCoord x,tCoord y,tSize width,tSize height,tWinId id);
static void win_getRepaintRegions(sSLList *list,tWinId id,sWindow *win,s16 z,sRectangle *r);
static void win_clearRegion(u8 *mem,tCoord x,tCoord y,tSize width,tSize height);
static void win_notifyVesa(tCoord x,tCoord y,tSize width,tSize height);

static tFD vesa;
static tServ servId;
static tSize screenWidth;
static tSize screenHeight;
static u8 colorDepth;

static sMsg msg;	/* TODO we already have a msg in winmain.c */
static u8 *shmem;
static u16 activeWindow = WINDOW_COUNT;
static sWindow windows[WINDOW_COUNT];

bool win_init(tServ sid) {
	tMsgId mid;
	tWinId i;

	servId = sid;

	/* mark windows unused */
	for(i = 0; i < WINDOW_COUNT; i++)
		windows[i].id = WINID_UNSED;

	vesa = open("/services/vesa",IO_WRITE);
	if(vesa < 0)
		error("Unable to open /services/vesa");

	/* request screen infos from vesa */
	if(send(vesa,MSG_VESA_GETMODE_REQ,&msg,sizeof(msg.args)) < 0)
		error("Unable to send get-mode-request to vesa");
	if(receive(vesa,&mid,&msg,sizeof(msg)) < 0)
		error("Unable to read the get-mode-response from vesa");

	/* store */
	screenWidth = (tSize)msg.args.arg1;
	screenHeight = (tSize)msg.args.arg2;
	colorDepth = (u8)msg.args.arg3;

	shmem = (u8*)joinSharedMem("vesa");
	if(shmem == NULL)
		error("Unable to join shared memory 'vesa'");

	return true;
}

tCoord win_getScreenWidth(void) {
	return screenWidth;
}

tCoord win_getScreenHeight(void) {
	return screenHeight;
}

void win_setCursor(tCoord x,tCoord y) {
	msg.args.arg1 = x;
	msg.args.arg2 = y;
	send(vesa,MSG_VESA_CURSOR,&msg,sizeof(msg.args));
}

tWinId win_create(u16 x,u16 y,u16 width,u16 height,tPid owner,u8 style) {
	tWinId i;
	for(i = 0; i < WINDOW_COUNT; i++) {
		if(windows[i].id == WINID_UNSED) {
			windows[i].id = i;
			windows[i].x = x;
			windows[i].y = y;
			/* TODO determine z */
			windows[i].z = i;
			windows[i].width = width;
			windows[i].height = height;
			windows[i].owner = owner;
			windows[i].style = style;
			/* TODO what keymap to set? */
			windows[i].keymap = 1;
			return i;
		}
	}
	return WINID_UNSED;
}

void win_destroyWinsOf(tTid tid,tCoord mouseX,tCoord mouseY) {
	tWinId id;
	for(id = 0; id < WINDOW_COUNT; id++) {
		if(windows[id].id != WINID_UNSED && windows[id].owner == tid)
			win_destroy(id,mouseX,mouseY);
	}
}

void win_destroy(tWinId id,tCoord mouseX,tCoord mouseY) {
	sRectangle *old;
	/* mark unused */
	windows[id].id = WINID_UNSED;

	/* repaint window-area */
	old = (sRectangle*)malloc(sizeof(sRectangle));
	old->x = windows[id].x;
	old->y = windows[id].y;
	old->width = windows[id].width;
	old->height = windows[id].height;
	old->window = WINDOW_COUNT;
	win_repaint(old,NULL,-1);

	/* set highest window active */
	if(activeWindow == id) {
		tWinId i,winId = WINID_UNSED;
		s16 maxz = -1;
		sWindow *w = windows;
		for(i = 0; i < WINDOW_COUNT; i++) {
			if(w->id != WINID_UNSED && w->z > maxz) {
				winId = i;
				maxz = w->z;
			}
			w++;
		}
		if(i != WINID_UNSED)
			win_setActive(i,false,mouseX,mouseY);
	}
}

sWindow *win_get(tWinId id) {
	if(id >= WINDOW_COUNT || windows[id].id == WINID_UNSED)
		return NULL;
	return windows + id;
}

bool win_exists(tWinId id) {
	return id < WINDOW_COUNT && windows[id].id != WINID_UNSED;
}

sWindow *win_getAt(tCoord x,tCoord y) {
	tWinId i;
	s16 maxz = -1;
	tWinId winId = WINDOW_COUNT;
	sWindow *w = windows;
	for(i = 0; i < WINDOW_COUNT; i++) {
		if(w->id != WINID_UNSED && w->z > maxz &&
				x >= w->x && x < w->x + w->width &&
				y >= w->y && y < w->y + w->height) {
			winId = i;
			maxz = w->z;
		}
		w++;
	}
	if(winId != WINDOW_COUNT)
		return windows + winId;
	return NULL;
}

sWindow *win_getActive(void) {
	if(activeWindow != WINDOW_COUNT)
		return windows + activeWindow;
	return NULL;
}

void win_setActive(tWinId id,bool repaint,tCoord mouseX,tCoord mouseY) {
	tWinId i;
	s16 curz = windows[id].z;
	s16 maxz = 0;
	sWindow *w = windows;
	if(id != WINDOW_COUNT) {
		for(i = 0; i < WINDOW_COUNT; i++) {
			if(w->id != WINID_UNSED && w->z > curz && w->style != WIN_STYLE_POPUP) {
				if(w->z > maxz)
					maxz = w->z;
				w->z--;
			}
			w++;
		}
		if(maxz > 0)
			windows[id].z = maxz;
	}

	if(id != activeWindow) {
		if(activeWindow != WINDOW_COUNT)
			win_sendActive(activeWindow,false,mouseX,mouseY);

		activeWindow = id;
		if(repaint && activeWindow != WINDOW_COUNT) {
			sRectangle *new;
			win_sendActive(activeWindow,true,mouseX,mouseY);

			new = (sRectangle*)malloc(sizeof(sRectangle));
			new->x = windows[activeWindow].x;
			new->y = windows[activeWindow].y;
			new->width = windows[activeWindow].width;
			new->height = windows[activeWindow].height;
			win_repaint(new,windows + activeWindow,windows[activeWindow].z);
		}
	}
}

void win_moveTo(tWinId window,tCoord x,tCoord y) {
	u32 i,count;
	sRectangle *rects;
	sRectangle *old = (sRectangle*)malloc(sizeof(sRectangle));
	sRectangle *new = (sRectangle*)malloc(sizeof(sRectangle));

	/* save old position */
	old->x = windows[window].x;
	old->y = windows[window].y;
	old->width = windows[window].width;
	old->height = windows[window].height;

	/* create rectangle for new position */
	new->x = windows[window].x = x;
	new->y = windows[window].y = y;
	new->width = old->width;
	new->height = old->height;

	/* clear old position */
	rects = rectSplit(old,new,&count);
	if(count > 0) {
		/* if there is an intersection, use the splitted parts */
		for(i = 0; i < count; i++) {
			/* repaintWindow() will free one rect, but we've allocated multiple rects at once :/ */
			sRectangle *tmp = (sRectangle*)malloc(sizeof(sRectangle));
			memcpy(tmp,rects + i,sizeof(sRectangle));
			tmp->window = WINDOW_COUNT;
			win_repaint(tmp,NULL,-1);
		}
		free(rects);
		free(old);
	}
	else {
		/* no intersection, so use the whole old rectangle */
		old->window = WINDOW_COUNT;
		win_repaint(old,NULL,-1);
	}

	/* repaint new position */
	win_repaint(new,windows + window,windows[window].z);
}

void win_update(tWinId window,tCoord x,tCoord y,tSize width,tSize height) {
	sWindow *win = windows + window;
	sRectangle *r = (sRectangle*)malloc(sizeof(sRectangle));
	r->x = win->x + x;
	r->y = win->y + y;
	r->width = width;
	r->height = height;
	win_repaint(r,win,win->z);
}

static void win_repaint(sRectangle *r,sWindow *win,s16 z) {
	sRectangle *rect;
	sSLNode *n;
	sSLList *list = sll_create();
	if(list == NULL) {
		printe("Unable to create list");
		exit(EXIT_FAILURE);
	}

	win_getRepaintRegions(list,0,win,z,r);
	for(n = sll_begin(list); n != NULL; n = n->next) {
		rect = (sRectangle*)n->data;

		/* ignore invalid values */
		/* FIXME where do they come from? */
		if(rect->x + rect->width > screenWidth || rect->y + rect->height > screenHeight ||
				rect->width > screenWidth || rect->height > screenHeight)
			continue;

		/* if it doesn't belong to a window, we have to clear it */
		if(rect->window == WINDOW_COUNT)
			win_clearRegion(shmem,rect->x,rect->y,rect->width,rect->height);
		/* send the window a repaint-request */
		else
			win_sendRepaint(rect->x,rect->y,rect->width,rect->height,rect->window);
	}

	/* free mem, too */
	sll_destroy(list,true);
}

static void win_sendActive(tWinId id,bool isActive,tCoord mouseX,tCoord mouseY) {
	tFD aWin = getClientThread(servId,windows[id].owner);
	if(aWin >= 0) {
		msg.args.arg1 = id;
		msg.args.arg2 = isActive;
		msg.args.arg3 = mouseX;
		msg.args.arg4 = mouseY;
		send(aWin,MSG_WIN_SET_ACTIVE,&msg,sizeof(msg.args));
		close(aWin);
	}
}

static void win_sendRepaint(tCoord x,tCoord y,tSize width,tSize height,tWinId id) {
	tFD aWin = getClientThread(servId,windows[id].owner);
	if(aWin >= 0) {
		msg.args.arg1 = x - windows[id].x;
		msg.args.arg2 = y - windows[id].y;
		msg.args.arg3 = width;
		msg.args.arg4 = height;
		msg.args.arg5 = id;
		send(aWin,MSG_WIN_UPDATE,&msg,sizeof(msg.args));
		close(aWin);
	}
}

static void win_getRepaintRegions(sSLList *list,tWinId id,sWindow *win,s16 z,sRectangle *r) {
	sRectangle *rects;
	sRectangle wr;
	sRectangle *inter;
	sWindow *w;
	u32 count;
	bool intersect;
	for(; id < WINDOW_COUNT; id++) {
		w = windows + id;
		/* skip unused, ourself and rects behind ourself */
		if((win && w->id == win->id) || w->id == WINID_UNSED || w->z < z)
			continue;

		/* build window-rect */
		wr.x = w->x;
		wr.y = w->y;
		wr.width = w->width;
		wr.height = w->height;
		/* split r by the rectangle */
		rects = rectSplit(r,&wr,&count);

		/* the window has to repaint the intersection, if there is any */
		inter = (sRectangle*)malloc(sizeof(sRectangle));
		if(inter == NULL) {
			printe("Unable to allocate mem");
			exit(EXIT_FAILURE);
		}
		intersect = rectIntersect(&wr,r,inter);
		if(intersect)
			win_getRepaintRegions(list,id + 1,w,w->z,inter);
		else
			free(inter);

		if(rects) {
			/* split all by all other windows */
			while(count-- > 0)
				win_getRepaintRegions(list,id + 1,win,z,rects + count);
		}

		/* if we made a recursive call we can leave here */
		if(rects || intersect) {
			/* forget old rect and stop here */
			free(r);
			return;
		}
	}

	/* no split, so its on top */
	if(win)
		r->window = win->id;
	else
		r->window = WINDOW_COUNT;
	sll_append(list,r);
}

static void win_clearRegion(u8 *mem,tCoord x,tCoord y,tSize width,tSize height) {
	tCoord ysave = y;
	tCoord maxy = y + height;
	u32 count = width * PIXEL_SIZE;
	mem += (y * screenWidth + x) * PIXEL_SIZE;
	while(y <= maxy) {
		memclear(mem,count);
		mem += screenWidth * PIXEL_SIZE;
		y++;
	}

	win_notifyVesa(x,ysave,width,height);
}

static void win_notifyVesa(tCoord x,tCoord y,tSize width,tSize height) {
	msg.args.arg1 = x;
	msg.args.arg2 = y;
	msg.args.arg3 = width;
	msg.args.arg4 = height;
	send(vesa,MSG_VESA_UPDATE,&msg,sizeof(msg.args));
}


#if DEBUGGING

void win_dbg_print(void) {
	tWinId i;
	sWindow *w = windows;
	debugf("Windows:\n");
	for(i = 0; i < WINDOW_COUNT; i++) {
		if(w->id != WINID_UNSED) {
			debugf("\t[%d] @(%d,%d), size=(%d,%d), z=%d, owner=%d, style=%d\n",
					i,w->x,w->y,w->width,w->height,w->z,w->owner,w->style);
		}
		w++;
	}
}

#endif