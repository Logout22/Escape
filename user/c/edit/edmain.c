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
#include <esc/cmdargs.h>
#include <esc/proc.h>
#include <esc/fileio.h>
#include <esc/keycodes.h>
#include <esccodes.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include "buffer.h"
#include "display.h"

static void usage(const char *name) {
	fprintf(stderr,"Usage: %s [<file>]\n",name);
	exit(EXIT_FAILURE);
}

int main(int argc,char *argv[]) {
	bool run = true;
	char c;
	s32 cmd,n1,n2,n3;
	if(argc > 2 || isHelpCmd(argc,argv))
		usage(argv[0]);

	buf_open(argc > 1 ? argv[1] : NULL);
	displ_init(buf_getLines());
	displ_update();

	while(run && (c = fscanc(stdin)) != IO_EOF) {
		if(c == '\033') {
			cmd = freadesc(stdin,&n1,&n2,&n3);
			if(cmd != ESCC_KEYCODE)
				continue;

			if(isprint(n1) && !(n3 & (STATE_CTRL | STATE_ALT))) {
				s32 col,row;
				displ_getCurPos(&col,&row);
				buf_insertAt(col,row,n1);
				displ_markDirty(row,1);
				displ_mvCurHor(HOR_MOVE_RIGHT);
			}
			else {
				switch(n2) {
					case VK_X:
						if(n3 & STATE_CTRL)
							run = false;
						break;

					case VK_ENTER: {
						s32 col,row;
						displ_getCurPos(&col,&row);
						buf_newLine(row);
						displ_mvCurVert(1);
						displ_markDirty(row,buf_getLineCount());
					}
					break;

					case VK_DELETE:
					case VK_BACKSP: {
						s32 col,row;
						displ_getCurPos(&col,&row);
						if(n2 == VK_DELETE)
							buf_removeCur(col,row);
						else if(col > 0) {
							buf_removePrev(col,row);
							displ_mvCurHor(HOR_MOVE_LEFT);
						}
						displ_markDirty(row,1);
					}
					break;

					case VK_LEFT:
						displ_mvCurHor(HOR_MOVE_LEFT);
						break;
					case VK_RIGHT:
						displ_mvCurHor(HOR_MOVE_RIGHT);
						break;

					case VK_HOME:
						if(n3 & STATE_CTRL)
							displ_mvCurVert(LONG_MIN);
						displ_mvCurHor(HOR_MOVE_HOME);
						break;
					case VK_END:
						if(n3 & STATE_CTRL)
							displ_mvCurVert(LONG_MAX);
						displ_mvCurHor(HOR_MOVE_END);
						break;

					case VK_UP:
						displ_mvCurVert(-1);
						break;
					case VK_DOWN:
						displ_mvCurVert(1);
						break;

					case VK_PGUP:
						displ_mvCurVertPage(true);
						break;
					case VK_PGDOWN:
						displ_mvCurVertPage(false);
						break;
				}
			}
			displ_update();
		}
	}
	displ_finish();
	return EXIT_SUCCESS;
}