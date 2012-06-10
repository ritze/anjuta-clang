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

//struct _ParserClangParserPriv {
	CXTranslationUnit translation_unit;
//}

void
parser_clang_parser_init (const gchar* full_file_path)
{
	g_warning ("initiate new translation unit instance for %s", full_file_path);
	
	CXIndex index = clang_createIndex (0, 0);
	//TODO: set num_unsaved_files and unsaved_files...
	unsigned num_unsaved_files = 0;
	struct CXUnsavedFile * unsaved_files = NULL;
	
	translation_unit = clang_createTranslationUnitFromSourceFile (index, full_file_path, 0, NULL, num_unsaved_files, unsaved_files);
	clang_disposeIndex (index);
}

void
parser_clang_parser_deinit ()
{
	g_warning ("deinitiate translation unit");
	
	if (translation_unit) {
		clang_disposeTranslationUnit (translation_unit);
		translation_unit = NULL;
	}
}

IAnjutaIterable *
parser_clang_parser_process_expression (const gchar *stmt,
                                        const gchar *above_text,
                                        const gchar *full_file_path,
                                        gulong linenum)
{
	g_warning ("process expression...");
	return NULL;
}

static void
parser_clang_parser_parse ()
{
	//some test code
	unsigned n;
	unsigned i;
	for (i = 0, n = clang_getNumDiagnostics (translation_unit); i != n; ++i) {
		CXDiagnostic diag = clang_getDiagnostic (translation_unit, i);
		CXString string = clang_formatDiagnostic (diag,
								clang_defaultDiagnosticDisplayOptions());
		g_warning ("%s", clang_getCString (string));
		clang_disposeString (string);
	}
}