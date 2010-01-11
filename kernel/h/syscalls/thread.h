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

#ifndef SYSCALLS_THREAD_H_
#define SYSCALLS_THREAD_H_

#include <machine/intrpt.h>

/**
 * Returns the tid of the current thread
 *
 * @return tTid the thread-id
 */
void sysc_gettid(sIntrptStackFrame *stack);

/**
 * @return u32 the number of threads of the current process
 */
void sysc_getThreadCount(sIntrptStackFrame *stack);

/**
 * Starts a new thread
 *
 * @param entryPoint the entry-point
 * @param char** arguments
 * @return tTid 0 for the new thread, the new thread-id for the current thread
 */
void sysc_startThread(sIntrptStackFrame *stack);

#endif /* SYSCALLS_THREAD_H_ */