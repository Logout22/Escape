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
#include <sys/dbg/console.h>
#include <sys/dbg/cmd/file.h>
#include <sys/dbg/cmd/view.h>
#include <sys/dbg/cmd/log.h>
#include <sys/dbg/cmd/ls.h>
#include <sys/dbg/cmd/mem.h>
#include <sys/dbg/cmd/panic.h>
#include <sys/dbg/cmd/dump.h>
#include <sys/dbg/cmd/step.h>
#include <sys/dbg/kb.h>
#include <sys/mem/cache.h>
#include <sys/task/smp.h>
#include <sys/video.h>
#include <esc/keycodes.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

size_t Console::histWritePos = 0;
size_t Console::histReadPos = 0;
size_t Console::histSize = 0;
char *Console::history[HISTORY_SIZE];
char Console::emptyLine[VID_COLS];
ScreenBackup Console::backup;
Console::Command Console::commands[] = {
	{"help",NULL},
	{"exit",NULL},
	{"file",cons_cmd_file},
	{"dump",cons_cmd_dump},
	{"view",cons_cmd_view},
	{"log",cons_cmd_log},
	{"ls",cons_cmd_ls},
	{"mem",cons_cmd_mem},
	{"panic",cons_cmd_panic},
	{"step",cons_cmd_step},
};

class LinesNaviBackend : public NaviBackend {
public:
	explicit LinesNaviBackend(const Lines *l)
		: NaviBackend(0,l->getLineCount() * BYTES_PER_LINE), lines(l) {
	}

	virtual uint8_t *loadLine(uintptr_t addr) {
		if(addr / BYTES_PER_LINE < lines->getLineCount())
			return (uint8_t*)lines->getLine(addr / BYTES_PER_LINE);
		return NULL;
	}

	virtual const char *getInfo(uintptr_t addr) {
		static char tmp[64];
		sStringBuffer buf;
		size_t start = addr / BYTES_PER_LINE;
		size_t end = MIN(lines->getLineCount(),start + VID_ROWS - 1);

		buf.len = 0;
		buf.dynamic = false;
		buf.size = sizeof(tmp);
		buf.str = tmp;
		prf_sprintf(&buf,"Lines %zu..%zu of %zu",start + 1,end,lines->getLineCount());
		if(!lines->isValid())
			prf_sprintf(&buf," (incomplete)");
		return buf.str;
	}

	virtual bool lineMatches(uintptr_t addr,const char *search,A_UNUSED size_t searchlen) {
		uint8_t *bytes = loadLine(addr);
		return bytes != NULL && *search && strcasestr((char*)bytes,search) != NULL;
	}

	virtual void displayLine(uintptr_t,uint8_t *bytes) {
		if(bytes)
			vid_printf("%s\r",bytes);
		else
			vid_printf("%*s\n",VID_COLS - 1,"");
	}

	virtual uintptr_t gotoAddr(const char *addr) {
		return (strtoul(addr,NULL,10) - 1) * BYTES_PER_LINE;
	}

private:
	const Lines *lines;
};

void Console::start(const char *initialcmd) {
	size_t i,argc;
	int res;
	Command *cmd;
	char **argv;

	/* first, pause all other CPUs to ensure that we're alone */
	SMP::pauseOthers();

	vid_setTargets(TARGET_SCREEN);
	vid_backup(backup.screen,&backup.row,&backup.col);

	vid_clearScreen();
	for(i = 0; i < VID_ROWS - 3; i++)
		vid_printf("\n");
	vid_printf("Welcome to the debugging console!\nType 'help' to get a list of commands!\n\n");

	memset(emptyLine,' ',VID_COLS - 1);
	emptyLine[VID_COLS - 1] = '\0';

	while(true) {
		vid_printf("# ");

		if(initialcmd != NULL) {
			argv = parseLine(initialcmd,&argc);
			initialcmd = NULL;
		}
		else {
			argv = parseLine(readLine(),&argc);
			vid_printf("\n");
		}

		/* repeat last command when no args are given */
		if(argc == 0) {
			if(!histSize)
				continue;
			size_t last = (histReadPos - 1) % histSize;
			if(history[last]) {
				argv = parseLine(history[last],&argc);
				if(argc == 0)
					continue;
			}
			else
				continue;
		}

		if(strcmp(argv[0],"exit") == 0) {
			/* remove this command from history */
			assert(histSize > 0);
			size_t last = (histReadPos - 1) % histSize;
			Cache::free(history[last]);
			history[last] = NULL;
			histReadPos = last;
			histWritePos = last;
			histSize--;
			break;
		}

		if(strcmp(argv[0],"help") == 0) {
			vid_printf("Available commands:\n");
			for(i = 0; i < ARRAY_SIZE(commands); i++)
				vid_printf("	%s\n",commands[i].name);
			vid_printf("\n");
			vid_printf("All commands use a viewer, that supports the following key-strokes:\n");
			vid_printf(" - up/down/pageup/pagedown/home/end: navigate through the data\n");
			vid_printf(" - left/right: to previous/next search result\n");
			vid_printf(" - enter: jump to entered line/address\n");
			vid_printf(" - s: stop searching\n");
			vid_printf(" - esc: quit\n");
			vid_printf("Note also that you can use '\\XX' to enter a character in hex when searching.\n");
		}
		else {
			cmd = getCommand(argv[0]);
			if(cmd) {
				res = cmd->exec(argc,argv);
				if(res == CONS_EXIT)
					break;
				if(res != 0)
					vid_printf("Executing command '%s' failed: %s (%d)\n",argv[0],strerror(-res),res);
			}
			else
				vid_printf("Unknown command '%s'\n",argv[0]);
		}
	}

	vid_restore(backup.screen,backup.row,backup.col);
	vid_setTargets(TARGET_LOG);

	/* now let the other CPUs continue */
	SMP::resumeOthers();
}

void Console::dumpLine(uintptr_t addr,uint8_t *bytes) {
	size_t i;
	vid_printf("%p: ",addr);
	if(bytes) {
		for(i = 0; i < BYTES_PER_LINE; i++)
			vid_printf("%02X ",bytes[i]);
		vid_printf("| ");
		for(i = 0; i < BYTES_PER_LINE; i++) {
			if(isprint(bytes[i]) && bytes[i] < 0x80 && !isspace(bytes[i]))
				vid_printf("%c",bytes[i]);
			else
				vid_printf(".");
		}
	}
	else {
		for(i = 0; i < BYTES_PER_LINE; i++)
			vid_printf("?? ");
		vid_printf("| ");
		for(i = 0; i < BYTES_PER_LINE; i++)
			vid_printf(".");
	}
	vid_printf("\n");
}

void Console::navigation(NaviBackend *backend) {
	/* the maximum has to be BYTES_PER_LINE because cons_display assumes it atm */
	char search[BYTES_PER_LINE + 1] = "";
	char searchClone[BYTES_PER_LINE + 1] = "";
	int searchMode = SEARCH_NONE;
	uintptr_t addr = backend->getStartPos();
	size_t searchPos = 0;
	Keyboard::Event ev;
	bool run = true;
	assert((backend->getMaxPos() & (BYTES_PER_LINE - 1)) == 0);
	while(run) {
		assert((addr & (BYTES_PER_LINE - 1)) == 0);
		convSearch(search,searchClone,searchPos);
		display(backend,search,searchClone,searchMode,&addr);
		searchMode = SEARCH_NONE;
		Keyboard::get(&ev,KEV_PRESS,true);
		switch(ev.keycode) {
			case VK_UP:
				addr = decrAddr(addr,BYTES_PER_LINE);
				break;
			case VK_DOWN:
				addr = incrAddr(backend->getMaxPos(),addr,BYTES_PER_LINE);
				break;
			case VK_PGUP:
				addr = decrAddr(addr,BYTES_PER_LINE * SCROLL_LINES);
				break;
			case VK_PGDOWN:
				addr = incrAddr(backend->getMaxPos(),addr,BYTES_PER_LINE * SCROLL_LINES);
				break;
			case VK_HOME:
				addr = backend->getStartPos();
				break;
			case VK_END:
				if(backend->getMaxPos() < BYTES_PER_LINE * SCROLL_LINES)
					addr = 0;
				else
					addr = backend->getMaxPos() - BYTES_PER_LINE * SCROLL_LINES;
				break;
			case VK_BACKSP:
				if(searchPos > 0)
					search[--searchPos] = '\0';
				break;
			case VK_ENTER:
				if(searchPos > 0) {
					addr = backend->gotoAddr(search);
					searchPos = 0;
					search[0] = '\0';
				}
				break;
			case VK_LEFT:
				if(searchPos > 0)
					searchMode = SEARCH_BACKWARDS;
				break;
			case VK_RIGHT:
				if(searchPos > 0)
					searchMode = SEARCH_FORWARD;
				break;
			case VK_ESC:
			case VK_Q:
				run = false;
				break;
			default:
				if(isprint(ev.character)) {
					if(searchPos < sizeof(search) - 1) {
						search[searchPos++] = tolower(ev.character);
						search[searchPos] = '\0';
					}
				}
				break;
		}
	}
}

void Console::viewLines(const Lines *l) {
	LinesNaviBackend backend(l);
	navigation(&backend);
}

bool Console::multiLineMatches(NaviBackend *backend,uintptr_t addr,const char *search,size_t searchlen) {
	uint8_t *bytes = backend->loadLine(addr);
	if(bytes && searchlen > 0) {
		size_t i;
		for(i = 0; i < BYTES_PER_LINE; i++) {
			size_t len = MIN(searchlen,BYTES_PER_LINE - i);
			if(strncasecmp(search,(char*)bytes + i,len) == 0) {
				if(len < searchlen) {
					uint8_t *nextBytes = backend->loadLine(addr + BYTES_PER_LINE);
					if(nextBytes && strncasecmp(search + len,(char*)nextBytes,searchlen - len) == 0)
						return true;
					bytes = backend->loadLine(addr);
				}
				else
					return true;
			}
		}
	}
	return false;
}

uintptr_t Console::getMaxAddr(uintptr_t end) {
	uintptr_t max = end - BYTES_PER_LINE * SCROLL_LINES;
	return max > end ? 0 : max;
}

uintptr_t Console::incrAddr(uintptr_t end,uintptr_t addr,size_t amount) {
	uintptr_t max = getMaxAddr(end);
	if(addr + amount < addr || addr + amount > max)
		return max;
	return addr + amount;
}

uintptr_t Console::decrAddr(uintptr_t addr,size_t amount) {
	if(addr - amount > addr)
		return 0;
	return addr - amount;
}

void Console::display(NaviBackend *backend,const char *searchInfo,const char *search,
                      int searchMode,uintptr_t *addr) {
	static char states[] = {'|','/','-','\\','|','/','-'};
	const char *info;
	uint y;
	bool found = true;

	uintptr_t startAddr = *addr;
	if(searchMode != SEARCH_NONE) {
		long count = 0;
		Keyboard::Event ev;
		int state = 0;
		size_t searchlen = strlen(search);
		found = false;
		for(; !found && ((searchMode == SEARCH_FORWARD &&
				startAddr < getMaxAddr(backend->getMaxPos())) ||
			  (searchMode == SEARCH_BACKWARDS && startAddr >= BYTES_PER_LINE)); count++) {
			if(count % 100 == 0) {
				vid_goto(VID_ROWS - 1,0);
				if(Keyboard::get(&ev,KEV_PRESS,false) && ev.keycode == VK_S) {
					*addr = startAddr;
					break;
				}
				vid_printf("\033[co;0;7]%c\033[co]",states[state++ % ARRAY_SIZE(states)]);
			}

			startAddr += searchMode == SEARCH_FORWARD ? BYTES_PER_LINE : -BYTES_PER_LINE;
			if(backend->lineMatches(startAddr,search,searchlen)) {
				found = true;
				break;
			}
		}
		if(!found)
			startAddr = *addr;
	}

	if(startAddr > getMaxAddr(backend->getMaxPos()))
		startAddr = ROUND_DN(getMaxAddr(backend->getMaxPos()),(uintptr_t)BYTES_PER_LINE);
	if(found)
		*addr = startAddr;

	info = backend->getInfo(startAddr);

	vid_goto(0,0);
	for(y = 0; y < VID_ROWS - 1; y++) {
		uint8_t *bytes = backend->loadLine(startAddr);
		bool matches = backend->lineMatches(startAddr,search,strlen(search));
		if(matches)
			vid_printf("\033[co;0;2]");
		backend->displayLine(startAddr,bytes);
		if(matches)
			vid_printf("\033[co]");
		startAddr += BYTES_PER_LINE;
	}

	if(found)
		vid_printf("\033[co;0;7]- Search/Goto: %s%|s\033[co]",searchInfo,info);
	else
		vid_printf("\033[co;0;7]- Search/Goto: \033[co;4;7]%s\033[co;0;7]%|s\033[co]",searchInfo,info);
}

uint8_t Console::charToInt(char c) {
	if(c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	if(c >= 'A' && c <= 'F')
		return 10 + c - 'A';
	if(c >= '0' && c <= '9')
		return c - '0';
	return 0;
}

void Console::convSearch(const char *src,char *dst,size_t len) {
	size_t i,j;
	for(i = 0, j = 0; i < len; ++i) {
		if(src[i] == '\\' && len > i + 2) {
			dst[j++] = (charToInt(src[i + 1]) << 4) | charToInt(src[i + 2]);
			i += 2;
		}
		else if(src[i] != '\\')
			dst[j++] = src[i];
	}
	dst[j] = '\0';
}

char **Console::parseLine(const char *line,size_t *argc) {
	static char argvals[MAX_ARG_COUNT][MAX_ARG_LEN];
	static char *args[MAX_ARG_COUNT];
	size_t i = 0,j = 0;
	args[0] = argvals[0];
	while(*line) {
		if(*line == ' ') {
			if(i > 0) {
				if(j + 1 >= MAX_ARG_COUNT)
					break;
				args[j][i] = '\0';
				j++;
				i = 0;
				args[j] = argvals[j];
			}
		}
		else if(i < MAX_ARG_LEN)
			args[j][i++] = *line;
		line++;
	}
	if(i > 0) {
		*argc = j + 1;
		args[j][i] = '\0';
	}
	else
		*argc = j;
	return args;
}

char *Console::readLine(void) {
	static char line[VID_COLS + 1];
	size_t i = 0;
	Keyboard::Event ev;
	while(true) {
		Keyboard::get(&ev,KEV_PRESS,true);
		if(i >= sizeof(line) - 1 || ev.keycode == VK_ENTER)
			break;

		/* emulate exit for ^D on an empty line */
		if(i == 0 && (ev.flags & KE_CTRL) && ev.keycode == VK_D) {
			strcpy(line,"exit");
			i = SSTRLEN("exit");
			break;
		}

		if((ev.keycode == VK_UP || ev.keycode == VK_DOWN) && histSize > 0) {
			if(ev.keycode == VK_UP) {
				if(histReadPos == 0)
					histReadPos = histSize - 1;
				else
					histReadPos = (histReadPos - 1) % histSize;
			}
			else
				histReadPos = (histReadPos + 1) % histSize;
			if(history[histReadPos]) {
				strcpy(line,history[histReadPos]);
				i = strlen(line);
				vid_printf("\r%s\r# %s",emptyLine,line);
			}
		}
		else if(ev.keycode == VK_BACKSP) {
			if(i > 0) {
				line[--i] = '\0';
				vid_printf("\r%s\r# %s",emptyLine,line);
			}
		}
		else if(isprint(ev.character)) {
			vid_printf("%c",ev.character);
			line[i++] = ev.character;
		}
	}
	line[i] = '\0';

	/* add to history */
	if(strlen(line) > 0 && (!history[histReadPos] || strcmp(history[histReadPos],line) != 0)) {
		history[histWritePos] = strdup(line);
		histWritePos = (histWritePos + 1) % HISTORY_SIZE;
		histReadPos = histWritePos;
		histSize = MIN(histSize + 1,HISTORY_SIZE);
	}
	return line;
}

Console::Command *Console::getCommand(const char *name) {
	size_t i;
	for(i = 0; i < ARRAY_SIZE(commands); i++) {
		if(strcmp(commands[i].name,name) == 0)
			return commands + i;
	}
	return NULL;
}
