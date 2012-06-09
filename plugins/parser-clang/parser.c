/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) Moritz Luedecke 2012 <ritze@skweez.net>
 * 
 * anjuta is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * anjuta is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <string.h>
#include <clang-c/Index.h>
#include "parser.h"

void
parser_init (gchar* filename)
{
	CXIndex index = clang_createIndex(0, 0);
	CXTranslationUnit tu = clang_parseTranslationUnit(index, 0, filename,
	                                                  1, 0, 0, CXTranslationUnit_None);
	//CXTranslationUnit tu = clang_createTranslationUnitFromSourceFile(index,
	//path, 0, NULL, num_unsaved, unsaved);
	
	unsigned n;
	unsigned i;
	for (i = 0, n = clang_getNumDiagnostics(tu); i != n; ++i) {
		CXDiagnostic diag = clang_getDiagnostic(tu, i);
		CXString string = clang_formatDiagnostic(diag,
								clang_defaultDiagnosticDisplayOptions());
		fprintf(stderr, "%s\n", clang_getCString(string));
		clang_disposeString(string);
	}
	
	clang_disposeTranslationUnit(tu);
	clang_disposeIndex(index);
}