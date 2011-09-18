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
#include <stdlib.h>
#include <test.h>

#include "tests/theap.h"
#include "tests/tfileio.h"
#include "tests/tdir.h"
#include "tests/tenv.h"
#include "tests/tsyscalls.h"

int main(void) {
	test_register(&tModHeap);
	test_register(&tModFileio);
	test_register(&tModDir);
	test_register(&tModEnv);
	test_register(&tModSyscalls);
	test_start();
	return EXIT_SUCCESS;
}