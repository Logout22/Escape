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

#ifndef FUNCTIONSTMT_H_
#define FUNCTIONSTMT_H_

#include <esc/common.h>
#include "node.h"
#include "stmtlist.h"
#include "../exec/env.h"

typedef struct {
	char *name;
	sASTNode *stmts;
	sEnv *env;
} sFunctionStmt;

/**
 * Creates a function-statement-node
 *
 * @param name the function-name (will NOT be cloned)
 * @param stmts the statement-list
 * @return the created node
 */
sASTNode *ast_createFunctionStmt(char *name,sASTNode *stmts);

/**
 * Executes the given node(-tree)
 *
 * @param e the environment
 * @param n the node
 * @return the value
 */
sValue *ast_execFunctionStmt(sEnv *e,sFunctionStmt *n);

/**
 * Calls the given function and executes it in a new (sub-)environment
 *
 * @param n the function
 * @return the result
 */
s32 ast_callFunction(sFunctionStmt *n);

/**
 * Prints this function-statement
 *
 * @param s the list
 * @param layer the layer
 */
void ast_printFunctionStmt(sFunctionStmt *n,u32 layer);

/**
 * Does nothing
 *
 * @param n the statement
 */
void ast_destroyFunctionStmt(sFunctionStmt *n);

/**
 * Destroys the given function-statement
 *
 * @param n the statement
 */
void ast_killFunctionStmt(sFunctionStmt *n);

#endif /* FUNCTIONSTMT_H_ */
