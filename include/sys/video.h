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

#ifndef VIDEO_H_
#define VIDEO_H_

#include <sys/common.h>
#include <stdarg.h>

typedef enum {BLACK,BLUE,GREEN,CYAN,RED,MARGENTA,ORANGE,WHITE,GRAY,LIGHTBLUE} eColor;

#define VID_COLS				80
#define VID_ROWS				25

#define TARGET_SCREEN			1
#define TARGET_LOG				2

/**
 * Inits the video-stuff
 */
void vid_init(void);

/**
 * Sets the targets of the printing
 *
 * @param targets the targets (TARGET_*)
 */
void vid_setTargets(u8 targets);

/**
 * Clears the screen
 */
void vid_clearScreen(void);

/**
 * Prints the given character to the current position on the screen
 *
 * @param c the character
 */
void vid_putchar(char c);

/**
 * Formatted output to the video-screen
 *
 * @param fmt the format
 */
void vid_printf(const char *fmt,...);

/**
 * Same as vid_printf, but with the va_list as argument
 *
 * @param fmt the format
 * @param ap the argument-list
 */
void vid_vprintf(const char *fmt,va_list ap);

#endif
