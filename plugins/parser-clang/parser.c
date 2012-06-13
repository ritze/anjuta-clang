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
	const gchar* path;
	CXTranslationUnit translation_unit;
	CXFile file;
//}

static GList*
parser_clang_parser_get_diagnostics ()
{
	GList *list = NULL;
	
	guint numDiagnostics = clang_getNumDiagnostics (translation_unit);
	guint i;
	
	for (i = 0; i != numDiagnostics; ++i)
		list = g_list_append (list, clang_getDiagnostic (translation_unit, i));
	
	return list;
}

static void
parser_clang_parser_diag (CXDiagnostic *diag)
{
	CXString string = clang_formatDiagnostic (diag,clang_defaultDiagnosticDisplayOptions());
	g_warning ("%s", clang_getCString (string));
	clang_disposeString (string);
}

static void
parser_clang_parser_parse (const gchar *unsavedContent)
{
	//some test code
	if (unsavedContent) {
		g_warning ("reparse...");
		
		struct CXUnsavedFile *unsaved = NULL;	
		unsaved = g_malloc (2 * sizeof (struct CXUnsavedFile));
		unsaved[0].Filename = path;
		unsaved[0].Contents = unsavedContent;
		//TODO: works this really?
		unsaved[0].Length = g_strv_length ((gchar**) unsavedContent);
		unsaved[1].Filename = NULL;
		unsaved[1].Contents = NULL;
		unsaved[1].Length = 0;

		gint error = clang_reparseTranslationUnit (translation_unit, 1,
						unsaved, clang_defaultReparseOptions (translation_unit));

		//TODO: act accordingly...
		if (error)
		{
			g_warning ("Could not reparse! abort here...");
			return;
		}

		g_free (unsaved);
	}
	
	GList *diags = parser_clang_parser_get_diagnostics ();

	g_list_foreach (diags, (GFunc) parser_clang_parser_diag, NULL);
}

gchar*
parser_clang_parser_get_definition (gint line, gint column)
{
	gchar* locationString;
	gchar* definitionString;
	
	if (line < 0 || column < 0)
	{
		g_warning ("Negative line or column!");
		return NULL;
	}
	
	CXSourceLocation location = clang_getLocation (translation_unit, file, line,
	                                               column);
	CXCursor cursor = clang_getCursor (translation_unit, location);

	CXString cursorSpelling = clang_getCursorDisplayName (cursor);
	locationString = g_strjoin ("Cursor: ", clang_getCString (cursorSpelling), NULL);
	clang_disposeString (cursorSpelling);
	
	g_warning ("%s", locationString);
	
	//TODO: Why is definition mostly null?		
	CXCursor definition = clang_getCursorDefinition (cursor);

	if (clang_Cursor_isNull (definition))
	{
		g_warning ("Cursor to the definition is null!");
		return NULL;
	}



	
	CXSourceLocation definitionLocation = clang_getCursorLocation (definition);

	//get definition
	CXCursor definitionCursor = clang_getCursor (translation_unit, definitionLocation);
	CXString definitionCursorSpelling = clang_getCursorDisplayName (definitionCursor);
	g_warning ("Definition Cursor: %s", clang_getCString (definitionCursorSpelling));
	clang_disposeString (definitionCursorSpelling);




	
	CXFile definitionFile;
	guint definitionLine;
	guint definitionColumn;
	guint definitionOffset;
	
	clang_getInstantiationLocation (definitionLocation,
	                                &definitionFile,
	                                &definitionLine,
	                                &definitionColumn,
	                                &definitionOffset);
	
	definitionString = g_strjoin ("Definition: Cursor location: ",
	                              clang_getFileName(definitionFile),
								  definitionLine, definitionColumn, NULL);

	g_warning ("%s", definitionString);
	
	return g_strjoin (locationString, "\n", definitionString, NULL);
	//return definitionOffset;
}

void
parser_clang_parser_init (const gchar* full_file_path)
{
	g_warning ("initiate new translation unit instance for %s", full_file_path);

	path = full_file_path;
	CXIndex index = clang_createIndex (0, 0);
	
	translation_unit = clang_createTranslationUnitFromSourceFile (index, path,
	                                                              0, NULL,
	                                                              0, NULL);
	file = clang_getFile (translation_unit, path);
	
	clang_disposeIndex (index);

	if (!translation_unit)
	{
		g_warning ("Could not parse file.");
		return;
	}
	
	//even more test code
//	parser_clang_parser_parse (NULL);
	
//	g_warning ("83, 20: GList *diags = parser_clang_parser_get_diagnostics ();");
//	parser_clang_parser_get_definition (83, 20);
//	g_warning ("89,  0: parser_clang_parser_get_calltip (guint line, guint column);");
//	parser_clang_parser_get_definition (89, 0);
}

void
parser_clang_parser_deinit ()
{
	g_warning ("deinitiate translation unit");
	
	if (translation_unit) {
		clang_disposeTranslationUnit (translation_unit);
		translation_unit = NULL;
	}
	
	file = NULL;
	path = NULL;
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