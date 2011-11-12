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

#include <esc/common.h>
#include <esc/thread.h>
#include <esc/debug.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>

#define MAX_EXIT_FUNCS		32

typedef void (*fRegFrameInfo)(void *callback);
typedef void (*fConstr)(void);
typedef struct {
	void (*f)(void*);
	void *p;
	void *d;
} sGlobalObj;

/**
 * Assembler routines
 */
extern int _startthread(fThreadEntry entryPoint,void *arg);
extern void sigRetFunc(void);
/**
 * Inits the c-library
 */
void __libc_init(void);
/**
 * Will be called by gcc at the beginning for every global object to register the
 * destructor of the object
 */
int __cxa_atexit(void (*f)(void *),void *p,void *d);
/**
 * We'll call this function in exit() to call all destructors registered by *atexit()
 */
void __cxa_finalize(void *d);

static tULock threadLock = 0;
static size_t threadCount = 1;
static tULock exitLock = 0;
static size_t exitFuncCount = 0;
static sGlobalObj exitFuncs[MAX_EXIT_FUNCS];

int startthread(fThreadEntry entryPoint,void *arg) {
	int res;
	locku(&threadLock);
	res = _startthread(entryPoint,arg);
	if(res >= 0)
		threadCount++;
	unlocku(&threadLock);
	return res;
}

int __cxa_atexit(void (*f)(void *),void *p,void *d) {
	locku(&exitLock);
	if(exitFuncCount >= MAX_EXIT_FUNCS) {
		unlocku(&exitLock);
		return -ENOMEM;
	}

	exitFuncs[exitFuncCount].f = f;
	exitFuncs[exitFuncCount].p = p;
	exitFuncs[exitFuncCount].d = d;
	exitFuncCount++;
	unlocku(&exitLock);
	return 0;
}

void __cxa_finalize(A_UNUSED void *d) {
	locku(&threadLock);
	/* if we're the last thread, call the exit-functions */
	if(--threadCount == 0) {
		ssize_t i;
		for(i = exitFuncCount - 1; i >= 0; i--)
			exitFuncs[i].f(exitFuncs[i].p);
	}
	unlocku(&threadLock);
}

void __libc_init(void) {
	/* tell kernel address of sigRetFunc */
	signal(SIG_RET,(fSignal)&sigRetFunc);
}