/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * parser-clang-assist.c
 * Copyright (C)  2007 Naba Kumar  <naba@gnome.org>
 *                     Johannes Schmid  <jhs@gnome.org>
 * 
 * anjuta is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * anjuta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with anjuta.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <clang-c/Index.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-language-provider.h>
#include <libanjuta/anjuta-launcher.h>
#include <libanjuta/anjuta-pkg-config.h>
#include <libanjuta/anjuta-utils.h>
#include <libanjuta/interfaces/ianjuta-file.h>
#include <libanjuta/interfaces/ianjuta-editor-cell.h>
#include <libanjuta/interfaces/ianjuta-editor-assist.h>
#include <libanjuta/interfaces/ianjuta-editor-tip.h>
#include <libanjuta/interfaces/ianjuta-language-provider.h>
#include <libanjuta/interfaces/ianjuta-project-manager.h>
#include <libanjuta/interfaces/ianjuta-symbol-manager.h>
#include "parser-clang-assist.h"

#define BRACE_SEARCH_LIMIT 500
#define SCOPE_CONTEXT_CHARACTERS "_.:>-0"
#define WORD_CHARACTER "_0"
#define MIN_CODECOMPLETE 4

//TODO: Delete the following two lines
#define DEBUG 1
//#define DEBUG_PRINT g_warning

#ifdef DEBUG
	#define DISPLAY_DIAGNOSTICS 1
#else
	#define DISPLAY_DIAGNOSTICS 0
#endif

static void iprovider_iface_init(IAnjutaProviderIface* iface);
static void ilanguage_provider_iface_init(IAnjutaLanguageProviderIface* iface);

G_DEFINE_TYPE_WITH_CODE (ParserClangAssist,
                         parser_clang_assist,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IANJUTA_TYPE_PROVIDER,
			                                    iprovider_iface_init)
                         G_IMPLEMENT_INTERFACE (IANJUTA_TYPE_LANGUAGE_PROVIDER,
			                                    ilanguage_provider_iface_init))

struct _ParserClangAssistPriv {
	GSettings* settings;
	IAnjutaEditorAssist* iassist;
	IAnjutaEditorTip* itip;
	IAnjutaProjectManager* imanager;
	AnjutaLanguageProvider* lang_prov;
	AnjutaLauncher* autocomplete_launcher;
	AnjutaLauncher* calltip_launcher;
	
	const gchar* editor_filename;
	const gchar* project_root;

	/* Clang */
	CXIndex clang_index;
	CXTranslationUnit clang_tu;
	CXFile clang_file;
	GList* clang_include_dirs;
	
	/* Context */
	gchar* context_cache;
	CXCursor context_cursor_cache;
	
	/* Calltips */
	gchar* calltip_context;
	IAnjutaIterable* calltip_iter;
	GList* tips;
	
	gint async_calltip_file;
	gint async_calltip_system;
	gint async_calltip_project;

	IAnjutaSymbolQuery *calltip_query_file;
	IAnjutaSymbolQuery *calltip_query_system;
	IAnjutaSymbolQuery *calltip_query_project;

	/* Autocompletion */
	GCompletion *completion_cache;
	gchar* pre_word;
	gboolean member_completion;
	gboolean autocompletion;

	gint async_file_id;
	gint async_system_id;
	gint async_project_id;

	IAnjutaSymbolQuery *ac_query_file;
	IAnjutaSymbolQuery *ac_query_system;
	IAnjutaSymbolQuery *ac_query_project;	

	/* Member autocompletion */
	IAnjutaSymbolQuery *query_members;

	/* Sync query */
	IAnjutaSymbolQuery *sync_query_file;
	IAnjutaSymbolQuery *sync_query_system;
	IAnjutaSymbolQuery *sync_query_project;
	
};

static GList*
parser_clang_assist_get_include_dirs (ParserClangAssist* assist)
{
	gchar *cmd;
	gchar *out;
	GList* include_dirs = NULL;
	GList* pkgs = ianjuta_project_manager_get_packages (assist->priv->imanager,
	                                                    NULL);
	
	/* Add system include dirs */
	cmd = g_strdup_printf ("gcc -v -x c++ /dev/null -fsyntax-only");
	
	if (g_spawn_command_line_sync (cmd, NULL, &out, NULL, NULL))
	{
		gchar **flags;
		flags = g_strsplit (out, "\n", -1);
		
		if (flags != NULL)
		{
			gchar **flag;
			for (flag = flags; *flag != NULL; flag++)
			{
				          //TODO: Backup: "\\ /\.*include"
				if (g_regex_match_simple ("\\ /\.*include", *flag, 0,
				                          G_REGEX_MATCH_ANCHORED))
				{
					include_dirs = g_list_append (include_dirs,
					                              g_strdup (*flag + 1));
				}
			}
			g_strfreev (flags);
		}
		else
			DEBUG_PRINT ("Couldn't find include folders for the C++ compiler");
		g_free (out);
	}
	g_free (cmd);
	
	/* Add project specific include dirs */
	GList* pkg;
	for (pkg = pkgs; pkg != NULL; pkg = g_list_next (pkg))
	{
		GList* dirs = anjuta_pkg_config_get_directories (pkg->data, FALSE,
		                                                 FALSE, NULL);
		include_dirs = g_list_concat (include_dirs, dirs);
	}
	
	/* Add project root path */
	include_dirs = g_list_append (include_dirs, g_strdup_printf ("%s",
	                                               assist->priv->project_root));
	
#ifdef DEBUG
	gchar *debug_text = "Include folders:\n";
	GList* include_dir = include_dirs;
	for (; include_dir != NULL; include_dir = g_list_next (include_dir))
		debug_text = g_strdup_printf ("%s  %s\n", debug_text,
		                              (gchar*) include_dir->data);
	DEBUG_PRINT (debug_text);
#endif
	
	return include_dirs;
}

static void
parser_clang_assist_clang_init (ParserClangAssist* assist,
                                gint numUnsaved,
                                struct CXUnsavedFile *unsaved)
{
	const gchar* path = assist->priv->editor_filename;
	
	DEBUG_PRINT ("Initiate new translation unit instance for %s", path);
	
	if (!assist->priv->clang_include_dirs)
		assist->priv->clang_include_dirs =
		                        parser_clang_assist_get_include_dirs (assist);
	GList *include_dirs = assist->priv->clang_include_dirs;
		
	guint argc = g_list_length (include_dirs);
	const gchar* argv[argc];

	guint i;
	for (i = 0; i < argc; i++, include_dirs = g_list_next (include_dirs))
		argv[i] = g_strdup_printf ("-I%s", (gchar*) include_dirs->data);
	
	assist->priv->clang_index = clang_createIndex (0, DISPLAY_DIAGNOSTICS);
	assist->priv->clang_tu = clang_parseTranslationUnit (
	                            assist->priv->clang_index, path, argv, argc,
	                            unsaved, numUnsaved, CXTranslationUnit_None);
	assist->priv->clang_file = clang_getFile (assist->priv->clang_tu, path);
}

static void
parser_clang_assist_clang_deinit (ParserClangAssist* assist)
{
	DEBUG_PRINT ("Deinitiate translation unit instance for %s",
	             assist->priv->editor_filename);
	
	if (assist->priv->clang_tu)
		clang_disposeTranslationUnit (assist->priv->clang_tu);
	assist->priv->clang_tu = NULL;
	
	if (assist->priv->clang_index)
		clang_disposeIndex (assist->priv->clang_index);
	assist->priv->clang_index = NULL;
	
	assist->priv->clang_file = NULL;
}

static struct CXUnsavedFile *
parser_clang_assist_get_unsaved (const gchar *filename, gchar *unsaved_content)
{
	struct CXUnsavedFile *unsaved = NULL;
	
	if (unsaved_content)
	{
		DEBUG_PRINT ("Create CXUnsavedFile");
		unsaved = g_new0 (struct CXUnsavedFile, 2);
		unsaved[0].Filename = filename;
		unsaved[0].Contents = unsaved_content;
		unsaved[0].Length = strlen (unsaved_content);
	}
	
	return unsaved;
}

//TODO: Do this async...
static void
parser_clang_assist_parse (ParserClangAssist* assist, gchar *unsaved_content)
{
	gint unsaved_num = 0;
	gint error = 0;
	struct CXUnsavedFile *unsaved = parser_clang_assist_get_unsaved (
	                            assist->priv->editor_filename, unsaved_content);
	if (unsaved_content)
	{
		DEBUG_PRINT ("Parse code with unsaved content.");
		unsaved_num = 1;
	}
	
	if (assist->priv->clang_tu)
	{
		guint options = clang_defaultReparseOptions (assist->priv->clang_tu);
		//TODO: Crash source is here:
		error = clang_reparseTranslationUnit (assist->priv->clang_tu,
		                                      unsaved_num, unsaved, options);
		assist->priv->clang_file = clang_getFile (assist->priv->clang_tu,
	                                              assist->priv->editor_filename);
	}
	else
	{
		parser_clang_assist_clang_init (assist, unsaved_num, unsaved);
		if (!assist->priv->clang_tu)
			error = 1;
	}
	g_free (unsaved_content);
	g_free (unsaved);
	
	if (error != 0)
	{
		DEBUG_PRINT ("Could not parse! Abort here...");
		parser_clang_assist_clang_deinit (assist);
	}
}

//TODO: Delete this function
static gchar*
parser_clang_assist_get_definition (ParserClangAssist* assist,
                                    gint line,
                                    gint column)
{
	gchar* locationString;
	gchar* definitionString;
	
	if (line < 0 || column < 0)
	{
		g_warning ("Negative line or column!");
		return NULL;
	}
	
	CXSourceLocation location = clang_getLocation (assist->priv->clang_tu,
	                                               assist->priv->clang_file,
	                                               line, column);
	CXCursor cursor = clang_getCursor (assist->priv->clang_tu, location);
	
	CXString cursorSpelling = clang_getCursorDisplayName (cursor);
	locationString = g_strdup_printf ("Cursor: %s", clang_getCString (cursorSpelling));
	clang_disposeString (cursorSpelling);
	
	g_warning ("%s", locationString);
	
	//TEST
	cursor = clang_getCursorReferenced (cursor);
	//END OF TEST
	
	if (clang_Cursor_isNull (cursor))
	{
		g_warning ("Cursor to the definition is null!");
		return NULL;
	}
	
	CXSourceLocation definitionLocation = clang_getCursorLocation (cursor);
	CXCursor definitionCursor = clang_getCursor (assist->priv->clang_tu, definitionLocation);
	CXString definitionCursorSpelling = clang_getCursorSpelling (definitionCursor);
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
	
	definitionString = g_strdup_printf ("Definition: Cursor location: %u, %u (%s)",
								        definitionLine,
	                                    definitionColumn,
	                                    clang_getCString (clang_getFileName(definitionFile)));

	g_warning ("%s", definitionString);
	
	return g_strjoin (locationString, "\n", definitionString, NULL);
}

/**
 * parser_clang_assist_proposal_new:
 * @cursor: CXCursor to create the proposal for
 *
 * Creates a new IAnjutaEditorAssistProposal for symbol
 *
 * Returns: a newly allocated IAnjutaEditorAssistProposal
 */
static IAnjutaEditorAssistProposal*
parser_clang_assist_proposal_new (CXCursor cursor)
{
	CXString cursorSpelling = clang_getCursorSpelling (cursor);
	IAnjutaEditorAssistProposal* proposal = g_new0 (IAnjutaEditorAssistProposal,
	                                                1);
	AnjutaLanguageProposalData* data = anjuta_language_proposal_data_new (
	                                (gchar*) clang_getCString (cursorSpelling));

	data->info = NULL;
	data->is_func = FALSE;
	data->has_para = FALSE;
	
	if (clang_Cursor_getNumArguments (cursor) > -1)
		data->is_func = TRUE;
	
	if (clang_Cursor_getNumArguments (cursor) > 0)
		data->has_para = TRUE;
	
	switch (clang_getCursorKind (cursor))
	{
		case CXCursor_FunctionDecl:
g_warning ("IANJUTA_SYMBOL_TYPE_FUNCTION");
			data->type = IANJUTA_SYMBOL_TYPE_FUNCTION;
			break;
		case CXCursor_MacroInstantiation:
g_warning ("IANJUTA_SYMBOL_TYPE_MACRO_WITH_ARG");
			if (data->has_para)
				data->type = IANJUTA_SYMBOL_TYPE_MACRO_WITH_ARG;
			break;
		case CXCursor_VarDecl:
g_warning ("IANJUTA_SYMBOL_TYPE_VARIABLE");
			data->type = IANJUTA_SYMBOL_TYPE_VARIABLE;
			break;
		default:
g_warning ("IANJUTA_SYMBOL_TYPE ==> DEFAULT");
	}
	
	if (data->is_func)
		proposal->label = g_strdup_printf ("%s()", data->name);
	else
		proposal->label = g_strdup (data->name);
	proposal->data = data;
	
	clang_disposeString (cursorSpelling);
	
	return proposal;
}

/**
 * parser_clang_assist_proposal_free:
 * @proposal: the proposal to free
 * 
 * Frees the proposal
 */
static void
parser_clang_assist_proposal_free (IAnjutaEditorAssistProposal* proposal)
{
	AnjutaLanguageProposalData* data = proposal->data;
	anjuta_language_proposal_data_free (data);
	g_free (proposal->label);
	g_free (proposal);
}

/**
 * anjuta_propsal_completion_func:
 * @data: an IAnjutaEditorAssistProposal
 *
 * Returns: the name of the completion func
 */
static gchar*
anjuta_proposal_completion_func (gpointer data)
{
	IAnjutaEditorAssistProposal* proposal = data;
	AnjutaLanguageProposalData* prop_data = proposal->data;
	
	return prop_data->name;
}

/**
 * parser_clang_assist_create_completion_from_cursor:
 * @cursor: CXCursor to the definition or reference
 *
 * Create a list of IAnjutaEditorAssistProposals from a list of symbols
 *
 * Returns: a newly allocated GList of newly allocated proposals. Free
 * with cpp_java_assist_proposal_free()
 */
static GList*
parser_clang_assist_create_completion_from_cursor (ParserClangAssist* assist,
                                                   CXCursor cursor)
{
//DEBUG:
	CXSourceLocation definitionLocation = clang_getCursorLocation (cursor);
	CXCursor definitionCursor = clang_getCursor (assist->priv->clang_tu, definitionLocation);
	CXString definitionCursorSpelling = clang_getCursorSpelling (definitionCursor);
	g_warning ("Definition Cursor: %s", clang_getCString (definitionCursorSpelling));
	clang_disposeString (definitionCursorSpelling);
//End of DEBUG

	if (clang_Cursor_isNull (cursor))
		return NULL;
	
	guint line;
	guint column;
	guint options;
	gint unsaved_num = 0;
	const gchar* filename;
	gchar* unsaved_content;
	struct CXUnsavedFile *unsaved;
	CXSourceLocation location;
	GList* list = NULL;
	
	filename = assist->priv->editor_filename;
	//TODO: Incaid cast?
	unsaved_content = ianjuta_editor_get_text_all (
	                            IANJUTA_EDITOR (assist->priv->iassist), NULL);
	unsaved = parser_clang_assist_get_unsaved (filename, unsaved_content);
	if (unsaved_content)
		unsaved_num = 1;
	location = clang_getCursorLocation (cursor);
	clang_getSpellingLocation (location, NULL, &line, &column, NULL);
	options = clang_defaultReparseOptions (assist->priv->clang_tu);
	
	//TODO:
	CXCodeCompleteResults *results = clang_codeCompleteAt (
	                                     assist->priv->clang_tu, filename, line,
	                                     column, unsaved, unsaved_num, options);
	
//DEBUG	
	gchar *text = g_strdup_printf ("Completion:\n");
	guint n = clang_getNumDiagnostics(assist->priv->clang_tu);
	guint i;
	for (i = 0; i != n; ++i) {
		CXDiagnostic diag = clang_getDiagnostic(assist->priv->clang_tu, i);
		CXString line = clang_formatDiagnostic(
		                        diag, clang_defaultDiagnosticDisplayOptions());
		text = g_strdup_printf ("%s\n%s", text, clang_getCString(line));
		clang_disposeString(line);
	}
	g_warning ("%s", text);
	g_free (text);
//End of DEBUG
	
	
	
	
	
//	do {
		//IAnjutaEditorAssistProposal* proposal = parser_clang_assist_proposal_new (cursor);	

//		list = g_list_append (list, proposal);
//	}
//	while (ianjuta_iterable_next (symbols, NULL));
	
	clang_disposeCodeCompleteResults (results);
	
	return list;
}

/**
 * parser_clang_assist_update_pre_word:
 * @assist: self
 * @pre_word: new pre_word
 *
 * Updates the current pre_word
 */
static void
parser_clang_assist_update_pre_word (ParserClangAssist* assist,
                                     const gchar* pre_word)
{
	g_free (assist->priv->pre_word);
	if (pre_word == NULL)
		pre_word = "";
	assist->priv->pre_word = g_strdup (pre_word);
}

/**
 * parser_clang_assist_is_expression_separator:
 * @c: character to check
 * @skip_braces: whether to skip closing braces
 * @iter: current cursor position
 *
 * Checks if a character seperates a C/C++ expression. It can skip brances
 * because they might not really end the expression
 *
 * Returns: TRUE if the characters seperates an expression, FALSE otherwise
 */
static gboolean
parser_clang_assist_is_expression_separator (gchar c,
                                             gboolean skip_braces,
                                             IAnjutaIterable* iter)
{
	IAnjutaEditorAttribute attrib = ianjuta_editor_cell_get_attribute (
	                                        IANJUTA_EDITOR_CELL(iter), NULL);
	int i;
	const gchar separators[] = {',', ';', '\n', '\r', '\t', '(',
	                          '{', '}', '=', '<', '\v', '!',
	                          '&', '%', '*', '[', ']', '?', '/',
	                          '+', 0};
	
	if (attrib == IANJUTA_EDITOR_STRING ||
	    attrib == IANJUTA_EDITOR_COMMENT)
	{
		return FALSE;
	}
	
	if (c == ')' && skip_braces)
	{
		anjuta_util_jump_to_matching_brace (iter, c, BRACE_SEARCH_LIMIT);
		return TRUE;
	}
	else if (c == ')' && !skip_braces)
		return FALSE;
	
	for (i = 0; separators[i] != 0; i++)
	{
		if (separators[i] == c)
			return TRUE;
	}

	return FALSE;
}

/**
 * parser_clang_assist_parse_expression:
 * @assist: self,
 * @iter: current cursor position
 * @start_iter: return location for the start of the completion
 * 
 * Returns: An iter of a list of IAnjutaSymbols or NULL
 */
static IAnjutaIterable*
parser_clang_assist_parse_expression (ParserClangAssist* assist, IAnjutaIterable* iter, IAnjutaIterable** start_iter)
{
	IAnjutaEditor* editor = IANJUTA_EDITOR (assist->priv->iassist);
	IAnjutaIterable* res = NULL;
	IAnjutaIterable* cur_pos = ianjuta_iterable_clone (iter, NULL);
	gboolean op_start = FALSE;
	gboolean ref_start = FALSE;
	gchar* stmt = NULL;
	
	/* Cursor points after the current characters, move back */
	ianjuta_iterable_previous (cur_pos, NULL);
	
	/* Search for a operator in the current line */
	do 
	{
		gchar ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL(cur_pos),
		                                         0, NULL);
		
		if (parser_clang_assist_is_expression_separator(ch, FALSE, iter)) {
			break;
		}

		if (ch == '.' || (op_start && ch == '-') || (ref_start && ch == ':'))
		{
			/* Found an operator, get the statement and the pre_word */
			IAnjutaIterable* pre_word_start = ianjuta_iterable_clone (cur_pos,
			                                                          NULL);
			IAnjutaIterable* pre_word_end = ianjuta_iterable_clone (iter, NULL);
			IAnjutaIterable* stmt_end = ianjuta_iterable_clone (pre_word_start,
			                                                    NULL);

			
			/* we need to pass to the parser all the statement included the last
			 * operator, being it "." or "->" or "::"
			 * Increase the end bound of the statement.
			 */
			ianjuta_iterable_next (stmt_end, NULL);
			if (op_start == TRUE || ref_start == TRUE)
				ianjuta_iterable_next (stmt_end, NULL);
				
			
			/* Move one character forward so we have the start of the pre_word
			 * and not the last operator */
			ianjuta_iterable_next (pre_word_start, NULL);
			/* If this is a two character operator, skip the second character */
			if (op_start)
			{
				ianjuta_iterable_next (pre_word_start, NULL);
			}
			
			parser_clang_assist_update_pre_word (assist, ianjuta_editor_get_text (
			                                                   editor,
			                                                   pre_word_start,
			                                                   pre_word_end,
			                                                   NULL));

			/* Try to get the name of the variable */
			while (ianjuta_iterable_previous (cur_pos, NULL))
			{
				gchar word_ch = ianjuta_editor_cell_get_char (
				                        IANJUTA_EDITOR_CELL(cur_pos), 0, NULL);
				
				if (parser_clang_assist_is_expression_separator(word_ch, FALSE,
				                                              cur_pos)) 
					break;				
			}
			ianjuta_iterable_next (cur_pos, NULL);
			stmt = ianjuta_editor_get_text (editor,
			                                cur_pos, stmt_end, NULL);
			*start_iter = pre_word_start;
			g_object_unref (stmt_end);
			g_object_unref (pre_word_end);
			break;
		}
		else if (ch == '>')
			op_start = TRUE;
		else if (ch == ':')
			ref_start = TRUE;
		else
		{
			op_start = FALSE;
			ref_start = FALSE;
		}
	}
	while (ianjuta_iterable_previous (cur_pos, NULL));

	if (stmt)
	{
		gchar *above_text;
		IAnjutaIterable* start;
		
		if (!assist->priv->editor_filename)
		{
			g_free (stmt);
			return NULL;
		}
		
		start = ianjuta_editor_get_start_position (editor, NULL);
		above_text = ianjuta_editor_get_text (editor, start, iter, NULL);
		g_object_unref (start);
		
		g_free (stmt);
	}
	g_object_unref (cur_pos);
	return res;
}

/** 
 * parser_clang_assist_create_completion_cache:
 * @assist: self
 *
 * Create a new completion_cache object
 */
static void
parser_clang_assist_create_completion_cache (ParserClangAssist* assist)
{
	g_assert (assist->priv->completion_cache == NULL);
	assist->priv->completion_cache = 
		g_completion_new (anjuta_proposal_completion_func);
}

/**
 * parser_clang_assist_cancel_queries:
 * @assist: self
 *
 * Abort all async operations
 */
static void
parser_clang_assist_cancel_queries (ParserClangAssist* assist)
{
	ianjuta_symbol_query_cancel (assist->priv->ac_query_file, NULL);
	ianjuta_symbol_query_cancel (assist->priv->ac_query_project, NULL);
	ianjuta_symbol_query_cancel (assist->priv->ac_query_system, NULL);
	assist->priv->async_file_id = 0;
	assist->priv->async_project_id = 0;
	assist->priv->async_system_id = 0;
}

/**
 * parser_clang_assist_clear_completion_cache:
 * @assist: self
 *
 * Clear the completion cache, aborting all async operations
 */
static void
parser_clang_assist_clear_completion_cache (ParserClangAssist* assist)
{
	parser_clang_assist_cancel_queries (assist);
	if (assist->priv->completion_cache)
	{	
		g_list_foreach (assist->priv->completion_cache->items,
		                        (GFunc) parser_clang_assist_proposal_free, NULL);
		g_completion_free (assist->priv->completion_cache);
	}
	assist->priv->completion_cache = NULL;
	assist->priv->member_completion = FALSE;
	assist->priv->autocompletion = FALSE;
}

/**
 * parser_clang_assist_populate_real:
 * @assist: self
 * @finished: TRUE if no more proposals are expected, FALSE otherwise
 *
 * Really invokes the completion interfaces and adds completions.
 * Might be called from an async context
 */
static void
parser_clang_assist_populate_real (ParserClangAssist* assist, gboolean finished)
{
	g_assert (assist->priv->pre_word != NULL);
	GList* proposals = g_completion_complete (assist->priv->completion_cache,
	                                          assist->priv->pre_word,
	                                          NULL);
	ianjuta_editor_assist_proposals (assist->priv->iassist,
	                                 IANJUTA_PROVIDER(assist), proposals,
	                                 assist->priv->pre_word, finished, NULL);
}

/**
 * parser_clang_assist_create_member_completion_cache
 * @assist: self
 * @cursor: Current cursor position
 * 
 * Create the completion_cache for member completion if possible
 *
 * Returns: the iter where a completion cache was build, NULL otherwise
 */
static IAnjutaIterable*
parser_clang_assist_create_member_completion_cache (ParserClangAssist* assist,
                                                    IAnjutaIterable* cursor)
{
	IAnjutaIterable* symbol = NULL;
	IAnjutaIterable* start_iter = NULL;
//TODO: Use the black magic of clang
//	symbol = parser_clang_assist_parse_expression (assist, cursor, &start_iter);

	if (symbol)
	{
		/* Query symbol children */
		CXCursor children =  clang_getNullCursor ();/*ianjuta_symbol_query_search_members (
		            assist->priv->query_members, IANJUTA_SYMBOL (symbol), NULL);*/
		
		g_object_unref (symbol);
		if (!clang_Cursor_isNull (children))
		{
			GList* proposals =
			    parser_clang_assist_create_completion_from_cursor (assist,
			                                                       children);
			parser_clang_assist_create_completion_cache (assist);
			g_completion_add_items (assist->priv->completion_cache, proposals);

			parser_clang_assist_populate_real (assist, TRUE);
			g_list_free (proposals);
			return start_iter;
		}
	}
	else if (start_iter)
		g_object_unref (start_iter);
	return NULL;
}

/**
 * TODO
 * on_symbol_search_complete:
 * @search_id: id of this search
 * @symbols: the returned symbols
 * @assist: self
 *
 * Called by the async search method when it found symbols
 */
static void
on_symbol_search_complete (CXCursor cursor, ParserClangAssist* assist)
{
	GList* proposals;
	proposals = parser_clang_assist_create_completion_from_cursor (assist,
	                                                               cursor);
	
	
	g_completion_add_items (assist->priv->completion_cache, proposals);
	gboolean running = assist->priv->async_system_id
	                       || assist->priv->async_file_id
	                       || assist->priv->async_project_id;
	if (!running)
		parser_clang_assist_populate_real (assist, TRUE);
	g_list_free (proposals);
}

/**
 * parser_clang_assist_create_autocompletion_cache:
 * @assist: self
 * @cursor: Current cursor position
 * 
 * Create completion cache for autocompletion. This is done async.
 *
 * Returns: the iter where a preword was detected, NULL otherwise
 */ 
static IAnjutaIterable*
parser_clang_assist_create_autocompletion_cache (ParserClangAssist* assist,
                                                 IAnjutaIterable* cursor)
{
	IAnjutaIterable* start_iter;
	gchar* pre_word = anjuta_language_provider_get_pre_word (
	                                  assist->priv->lang_prov,
                                      IANJUTA_EDITOR (assist->priv->iassist),
									  cursor, &start_iter, WORD_CHARACTER);
	if (!pre_word || strlen (pre_word) < MIN_CODECOMPLETE)
	{
		if (start_iter)
			g_object_unref (start_iter);
		return NULL;
	}
	else
	{
		gchar *pattern = g_strconcat (pre_word, "%", NULL);
		
		parser_clang_assist_create_completion_cache (assist);
		parser_clang_assist_update_pre_word (assist, pre_word);

		if (IANJUTA_IS_FILE (assist->priv->iassist))
		{
			GFile *file = ianjuta_file_get_file (
			                      IANJUTA_FILE (assist->priv->iassist), NULL);
			if (file != NULL)
			{
				assist->priv->async_file_id = 1;
				ianjuta_symbol_query_search_file (assist->priv->ac_query_file,
												  pattern, file, NULL);
				g_object_unref (file);
			}
		}
		/* This will avoid duplicates of FUNCTION and PROTOTYPE */
		assist->priv->async_project_id = 1;
		ianjuta_symbol_query_search (assist->priv->ac_query_project, pattern,
		                             NULL);
		assist->priv->async_system_id = 1;
		ianjuta_symbol_query_search (assist->priv->ac_query_system, pattern,
		                             NULL);
		g_free (pre_word);
		g_free (pattern);
		
		return start_iter;
	}
}

/**
 * parser_clang_assist_get_calltip_context:
 * @self: Self
 * @iter: current cursor position
 * @e: Error propagation
 *
 * Searches for a calltip context
 *
 * Returns: name of the method to show a calltip for or NULL
 */
static gchar*
parser_clang_assist_get_calltip_context (IAnjutaLanguageProvider *self,
                                         IAnjutaIterable *iter,
                                         GError** e)
{
	ParserClangAssist* assist = PARSER_CLANG_ASSIST (self);
	gchar* calltip_context;
	
	//TODO:
	guint offset = ianjuta_iterable_get_position (iter, NULL)/*  + 1*/;
	
	gchar* unsavedContent = ianjuta_editor_get_text_all (
	                            IANJUTA_EDITOR (assist->priv->iassist), NULL);
	parser_clang_assist_parse (assist, unsavedContent);
	
	//TODO: Crash:
	CXSourceLocation location = clang_getLocationForOffset (
	               assist->priv->clang_tu, assist->priv->clang_file, offset);
/* TODO: Test code
	gint line = ianjuta_editor_get_lineno (IANJUTA_EDITOR (assist->priv->iassist), NULL);
	gint column = ianjuta_editor_get_column (IANJUTA_EDITOR (assist->priv->iassist), NULL);
	guint offset = ianjuta_editor_get_offset (IANJUTA_EDITOR (assist->priv->iassist), NULL);
	g_warning ("clang_getLocation ()");
	CXSourceLocation location = clang_getLocation (assist->priv->clang_tu, assist->priv->clang_file, line, column);
	g_warning ("clang_getLocationForOffset ()");
	CXSourceLocation location2 = clang_getLocationForOffset (assist->priv->clang_tu, assist->priv->clang_file, offset2);
	g_warning ("done");
*/
	
	CXCursor cursor = clang_getCursor (assist->priv->clang_tu, location);
	CXString cursor_spelling = clang_getCursorSpelling (cursor);
	calltip_context = g_strdup_printf ("%s", clang_getCString (cursor_spelling));
	clang_disposeString (cursor_spelling);
	
	/* Abort, if calltip_context is empty */
	if (*calltip_context == '\0')
		return NULL;
	
	assist->priv->context_cache = calltip_context;
	assist->priv->context_cursor_cache = cursor;
	return calltip_context;
}

/**
 * parser_clang_assist_create_calltips:
 * @iter: List of symbols
 * @merge: list of calltips to merge or NULL
 *
 * Create a list of Calltips (string) from a list of symbols
 *
 * A newly allocated GList* with newly allocated strings
 */
static GList*
parser_clang_assist_create_calltips (IAnjutaIterable* iter, GList* merge)
{
	GList* tips = merge;
	if (iter)
	{
		do
		{
			IAnjutaSymbol* symbol = IANJUTA_SYMBOL (iter);
			const gchar* name = ianjuta_symbol_get_string (
			                            symbol,IANJUTA_SYMBOL_FIELD_NAME, NULL);
			if (name != NULL)
			{
				const gchar* args = ianjuta_symbol_get_string (
				                                symbol,
				                                IANJUTA_SYMBOL_FIELD_SIGNATURE,
				                                NULL);
				const gchar* rettype = ianjuta_symbol_get_string (
				                               symbol,
				                               IANJUTA_SYMBOL_FIELD_RETURNTYPE,
				                               NULL);
				gchar* print_args;
				gchar* separator;
				gchar* white_name;
				gint white_count = 0;

				if (!rettype)
					rettype = "";
				else
					white_count += strlen(rettype) + 1;
				
				white_count += strlen(name) + 1;
				
				white_name = g_strnfill (white_count, ' ');
				separator = g_strjoin (NULL, ", \n", white_name, NULL);
				gchar** argv;
				if (!args)
					args = "()";
				
				argv = g_strsplit (args, ",", -1);
				print_args = g_strjoinv (separator, argv);
				gchar* tip = g_strdup_printf ("%s %s %s", rettype, name,
				                              print_args);
				
				if (!g_list_find_custom (tips, tip, (GCompareFunc) strcmp))
					tips = g_list_append (tips, tip);
				
				g_strfreev (argv);
				g_free (print_args);
				g_free (separator);
				g_free (white_name);
			}
			else
				break;
		}
		while (ianjuta_iterable_next (iter, NULL));
	}
	return tips;
}

/**
 * on_calltip_search_complete:
 * @search_id: id of this search
 * @symbols: the returned symbols
 * @assist: self
 *
 * Called by the async search method when it found calltips
 */
static void
on_calltip_search_complete (IAnjutaSymbolQuery *query, IAnjutaIterable* symbols,
							ParserClangAssist* assist)
{
	assist->priv->tips = parser_clang_assist_create_calltips (symbols,
	                                                        assist->priv->tips);
	if (query == assist->priv->calltip_query_file)
		assist->priv->async_calltip_file = 0;
	else if (query == assist->priv->calltip_query_project)
		assist->priv->async_calltip_project = 0;
	else if (query == assist->priv->calltip_query_system)
		assist->priv->async_calltip_system = 0;
	else
		g_assert_not_reached ();
	gboolean running = assist->priv->async_calltip_system
	                       || assist->priv->async_calltip_file
	                       || assist->priv->async_calltip_project;

	DEBUG_PRINT ("Calltip search finished with %d items",
	             g_list_length (assist->priv->tips));
	
	if (!running && assist->priv->tips)
	{
		ianjuta_editor_tip_show (IANJUTA_EDITOR_TIP(assist->priv->itip),
		                         assist->priv->tips, assist->priv->calltip_iter,
		                         NULL);
	}
}

/**
 * parser_clang_assist_query_calltip:
 * @assist: Self
 *
 * Starts an async query for the calltip
 */
static void
parser_clang_assist_query_calltip (ParserClangAssist* assist)
{
	if (clang_Cursor_isNull (assist->priv->context_cursor_cache))
	{
		DEBUG_PRINT ("Context is not available in the cache! Abort calltip query here...");
		return;
	}
	
	//TODO: Do this async
	CXCursor cursor = clang_getCursorReferenced (assist->priv->context_cursor_cache);
	
	if (clang_Cursor_isNull (cursor))
	{
g_warning ("Cursor to the definition is null!");
		return;
	}
	
	CXSourceLocation definitionLocation = clang_getCursorLocation (cursor);
	CXCursor definitionCursor = clang_getCursor (assist->priv->clang_tu, definitionLocation);
// Debug:
CXString definitionCursorSpelling = clang_getCursorSpelling (definitionCursor);
g_warning ("Definition Cursor: %s", clang_getCString (definitionCursorSpelling));
clang_disposeString (definitionCursorSpelling);
	
	
	on_symbol_search_complete (definitionCursor, assist);
}

/**
 * parser_clang_assist_create_calltip_context:
 * @assist: self
 * @call_context: The context (method/function name)
 * @position: iter where to show calltips
 *
 * Create the calltip context
 */
static void
parser_clang_assist_create_calltip_context (ParserClangAssist* assist,
                                            const gchar* call_context,
                                            IAnjutaIterable* position)
{
	assist->priv->calltip_context = g_strdup (call_context);
	assist->priv->calltip_iter = position;
}

/**
 * parser_clang_assist_clear_calltip_context:
 * @self: Self
 * @e: Error propagation
 *
 * Clears the calltip context and brings it back into a save state
 */
static void
parser_clang_assist_clear_calltip_context (ParserClangAssist* assist)
{
	ianjuta_symbol_query_cancel (assist->priv->calltip_query_file, NULL);
	ianjuta_symbol_query_cancel (assist->priv->calltip_query_project, NULL);
	ianjuta_symbol_query_cancel (assist->priv->calltip_query_system, NULL);

	assist->priv->async_calltip_file = 0;
	assist->priv->async_calltip_project = 0;
	assist->priv->async_calltip_system = 0;
	
	g_list_foreach (assist->priv->tips, (GFunc) g_free, NULL);
	g_list_free (assist->priv->tips);
	assist->priv->tips = NULL;
	
	g_free (assist->priv->calltip_context);
	assist->priv->calltip_context = NULL;
	
	if (assist->priv->calltip_iter)
		g_object_unref (assist->priv->calltip_iter);
	assist->priv->calltip_iter = NULL;
}

/**
 * parser_clang_assist_cancelled:
 * @iassist: IAnjutaEditorAssist that emitted the signal
 * @assist: ParserClangAssist object
 *
 * Stop any autocompletion queries when the cancelled signal was received
 */
static void
parser_clang_assist_cancelled (IAnjutaEditorAssist* iassist,
                               ParserClangAssist* assist)
{
	parser_clang_assist_cancel_queries (assist);
}

static GList*
parser_clang_assist_get_calltip_cache (IAnjutaLanguageProvider* self,
                                       gchar* call_context,
                                       GError** e)
{
	ParserClangAssist* assist = PARSER_CLANG_ASSIST (self);
	if (!g_strcmp0 (call_context, assist->priv->calltip_context))
	{
		DEBUG_PRINT ("Calltip was found in the cache.");
		return assist->priv->tips;
	}
	else
	{
		DEBUG_PRINT ("Calltip is not available in the cache!");
		return NULL;
	}
}

static void
parser_clang_assist_new_calltip (IAnjutaLanguageProvider* self,
                                 gchar* call_context,
                                 IAnjutaIterable* cursor,
                                 GError** e)
{
g_warning ("parser_clang_assist_new_calltip (call_context = %s)", call_context);
	ParserClangAssist* assist = PARSER_CLANG_ASSIST (self);
	
	if (clang_Cursor_isNull (assist->priv->context_cursor_cache)
	    || g_strcmp0 (call_context, assist->priv->context_cache))
	{
		DEBUG_PRINT ("Context is not available in the cache! Abort new calltip here...");
g_warning ("Context is not available in the cache! Abort new calltip here...");
		//TODO: Get cursor
		return;
/*		assist->priv->context_cache = call_context;
		assist->priv->context_cursor_cache =
		                    clang_getCursor (assist->priv->clang_tu, location);
*/	}
	
	parser_clang_assist_clear_calltip_context (assist);
	parser_clang_assist_create_calltip_context (assist, call_context, cursor);
	parser_clang_assist_query_calltip (assist);
}

static IAnjutaIterable*
parser_clang_assist_populate_completions (IAnjutaLanguageProvider* self,
                                          IAnjutaIterable* cursor,
                                          GError** e)
{
	ParserClangAssist* assist = PARSER_CLANG_ASSIST (self);
	IAnjutaIterable* start_iter = NULL;
	
	/* Check if completion was in progress */
	if (assist->priv->member_completion || assist->priv->autocompletion)
	{
		g_assert (assist->priv->completion_cache != NULL);
		gchar* pre_word = anjuta_language_provider_get_pre_word (
		                              assist->priv->lang_prov,
		                              IANJUTA_EDITOR (assist->priv->iassist),
		                              cursor, &start_iter, WORD_CHARACTER);
		DEBUG_PRINT ("Preword: %s", pre_word);
		if (pre_word && g_str_has_prefix (pre_word, assist->priv->pre_word))
		{
			DEBUG_PRINT ("Continue autocomplete for %s", pre_word);
			
			/* Great, we just continue the current completion */			
			parser_clang_assist_update_pre_word (assist, pre_word);
			parser_clang_assist_populate_real (assist, TRUE);
			g_free (pre_word);
			return start_iter;
		}			
		g_free (pre_word);
	}
	
	parser_clang_assist_clear_completion_cache (assist);
	
	/* Check for member completion */
	start_iter = parser_clang_assist_create_member_completion_cache (assist,
	                                                               cursor);
	if (start_iter)
		assist->priv->member_completion = TRUE;
	else
	{
		start_iter = parser_clang_assist_create_autocompletion_cache (assist,
		                                                            cursor);
		if (start_iter)
			assist->priv->autocompletion = TRUE;
	}
	
	return start_iter;
}

/**
 * parser_clang_assist_install:
 * @assist: ParserClangAssist object
 * @imanager: Project manager to install support for
 * @ieditor: Editor to install support for
 * @iparser: Parser to install support for
 * @project_root: Path to the project root
 *
 * Returns: Registers provider for editor
 */
static void
parser_clang_assist_install (ParserClangAssist *assist,
                             IAnjutaProjectManager *imanager,
                             IAnjutaEditor *ieditor,
                             const gchar *project_root)
{
	g_return_if_fail (assist->priv->iassist == NULL);
	assist->priv->project_root = project_root;
	
	if (IANJUTA_IS_PROJECT_MANAGER (imanager))
		assist->priv->imanager = IANJUTA_PROJECT_MANAGER (imanager);
	else
		assist->priv->imanager = NULL;
	
	if (IANJUTA_IS_EDITOR_ASSIST (ieditor))
	{
		assist->priv->iassist = IANJUTA_EDITOR_ASSIST (ieditor);
		ianjuta_editor_assist_add (IANJUTA_EDITOR_ASSIST (ieditor),
		                           IANJUTA_PROVIDER(assist), NULL);
		g_signal_connect (ieditor, "cancelled",
		                  G_CALLBACK (parser_clang_assist_cancelled), assist);
	}
	else
		assist->priv->iassist = NULL;

	if (IANJUTA_IS_EDITOR_TIP (ieditor))
		assist->priv->itip = IANJUTA_EDITOR_TIP (ieditor);
	else
		assist->priv->itip = NULL;
		
	if (IANJUTA_IS_FILE (assist->priv->iassist))
	{
		GFile *file = ianjuta_file_get_file (
		                          IANJUTA_FILE (assist->priv->iassist), NULL);
		if (file != NULL)
		{
			assist->priv->editor_filename = g_file_get_path (file);
			g_object_unref (file);
		}
	}
}

/**
 * parser_clang_assist_uninstall:
 * @self: ParserClangAssist object
 *
 * Returns: Unregisters provider
 */
static void
parser_clang_assist_uninstall (ParserClangAssist *assist)
{
	g_return_if_fail (assist->priv->iassist != NULL);
	
	g_signal_handlers_disconnect_by_func (assist->priv->iassist,
	                                      parser_clang_assist_cancelled, assist);
	ianjuta_editor_assist_remove (assist->priv->iassist, IANJUTA_PROVIDER(assist), NULL);
	assist->priv->iassist = NULL;
}

static void
parser_clang_assist_init (ParserClangAssist *assist)
{
	assist->priv = g_new0 (ParserClangAssistPriv, 1);
}

static void
parser_clang_assist_finalize (GObject *object)
{
	ParserClangAssist *assist = PARSER_CLANG_ASSIST (object);
	ParserClangAssistPriv* priv = assist->priv;

	parser_clang_assist_clang_deinit (assist);
	parser_clang_assist_uninstall (assist);
	parser_clang_assist_clear_completion_cache (assist);
	parser_clang_assist_clear_calltip_context (assist);
	
	if (priv->clang_include_dirs)
		anjuta_util_glist_strings_free (priv->clang_include_dirs);
	priv->clang_include_dirs = NULL;
		
	if (priv->calltip_query_file)
		g_object_unref (priv->calltip_query_file);
	priv->calltip_query_file = NULL;
		                
	if (priv->calltip_query_system)
		g_object_unref (priv->calltip_query_system);
	priv->calltip_query_system = NULL;
	
	if (priv->calltip_query_project)
		g_object_unref (priv->calltip_query_project);
	priv->calltip_query_project = NULL;
	
	if (priv->ac_query_file)
		g_object_unref (priv->ac_query_file);
	priv->ac_query_file = NULL;
	
	if (priv->ac_query_system)
		g_object_unref (priv->ac_query_system);
	priv->ac_query_system = NULL;
		                
	if (priv->ac_query_project)
		g_object_unref (priv->ac_query_project);
	priv->ac_query_project = NULL;
		                
	if (priv->query_members)
		g_object_unref (priv->query_members);
	priv->query_members = NULL; 

	if (priv->sync_query_file)
		g_object_unref (priv->sync_query_file);
	priv->sync_query_file = NULL;

	if (priv->sync_query_system)
		g_object_unref (priv->sync_query_system);
	priv->sync_query_system = NULL;

	if (priv->sync_query_project)
		g_object_unref (priv->sync_query_project);
	priv->sync_query_project = NULL;
	
	g_free (assist->priv);
	G_OBJECT_CLASS (parser_clang_assist_parent_class)->finalize (object);
}

static void
parser_clang_assist_class_init (ParserClangAssistClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = parser_clang_assist_finalize;
}

//TODO: Remove isymbol_manager parameter
ParserClangAssist *
parser_clang_assist_new (IAnjutaEditor *ieditor,
                         IAnjutaProjectManager *imanager,
                         IAnjutaSymbolManager *isymbol_manager,
                         GSettings* settings,
                         const gchar *project_root)
{
	ParserClangAssist *assist;
	static IAnjutaSymbolField calltip_fields[] = {
		IANJUTA_SYMBOL_FIELD_ID,
		IANJUTA_SYMBOL_FIELD_NAME,
		IANJUTA_SYMBOL_FIELD_RETURNTYPE,
		IANJUTA_SYMBOL_FIELD_SIGNATURE
	};
	static IAnjutaSymbolField ac_fields[] = {
		IANJUTA_SYMBOL_FIELD_ID,
		IANJUTA_SYMBOL_FIELD_NAME,
		IANJUTA_SYMBOL_FIELD_KIND,
		IANJUTA_SYMBOL_FIELD_TYPE,
		IANJUTA_SYMBOL_FIELD_ACCESS,
		IANJUTA_SYMBOL_FIELD_SIGNATURE
	};

	if (!IANJUTA_IS_EDITOR_ASSIST (ieditor) && !IANJUTA_IS_EDITOR_TIP (ieditor))
	{
		/* No assistance is available with the current editor */
		return NULL;
	}
	assist = g_object_new (TYPE_PARSER_CLANG_ASSIST, NULL);
	assist->priv->settings = settings;
	
	/* Create call tip queries */
	/* Calltip in file */
	assist->priv->calltip_query_file =
		ianjuta_symbol_manager_create_query (isymbol_manager,
		                                     IANJUTA_SYMBOL_QUERY_SEARCH_FILE,
		                                     IANJUTA_SYMBOL_QUERY_DB_PROJECT,
		                                     NULL);
	ianjuta_symbol_query_set_fields (assist->priv->calltip_query_file,
	                                 G_N_ELEMENTS (calltip_fields),
	                                 calltip_fields, NULL);
	ianjuta_symbol_query_set_filters (assist->priv->calltip_query_file,
	                                  IANJUTA_SYMBOL_TYPE_PROTOTYPE |
	                                  IANJUTA_SYMBOL_TYPE_FUNCTION |
	                                  IANJUTA_SYMBOL_TYPE_METHOD |
	                                  IANJUTA_SYMBOL_TYPE_MACRO_WITH_ARG,
	                                  TRUE, NULL);
	ianjuta_symbol_query_set_file_scope (assist->priv->calltip_query_file,
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PRIVATE,
	                                     NULL);
	ianjuta_symbol_query_set_mode (assist->priv->calltip_query_file,
	                               IANJUTA_SYMBOL_QUERY_MODE_ASYNC, NULL);
	g_signal_connect (assist->priv->calltip_query_file, "async-result",
	                  G_CALLBACK (on_calltip_search_complete), assist);
	/* Calltip in project */
	assist->priv->calltip_query_project =
		ianjuta_symbol_manager_create_query (isymbol_manager,
		                                     IANJUTA_SYMBOL_QUERY_SEARCH,
		                                     IANJUTA_SYMBOL_QUERY_DB_PROJECT,
		                                     NULL);
	ianjuta_symbol_query_set_fields (assist->priv->calltip_query_project,
	                                 G_N_ELEMENTS (calltip_fields),
	                                 calltip_fields, NULL);
	ianjuta_symbol_query_set_filters (assist->priv->calltip_query_project,
	                                  IANJUTA_SYMBOL_TYPE_PROTOTYPE |
	                                  IANJUTA_SYMBOL_TYPE_METHOD |
	                                  IANJUTA_SYMBOL_TYPE_MACRO_WITH_ARG,
	                                  TRUE, NULL);
	ianjuta_symbol_query_set_file_scope (assist->priv->calltip_query_project,
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PUBLIC,
	                                     NULL);
	ianjuta_symbol_query_set_mode (assist->priv->calltip_query_project,
	                               IANJUTA_SYMBOL_QUERY_MODE_ASYNC, NULL);
	g_signal_connect (assist->priv->calltip_query_project, "async-result",
	                  G_CALLBACK (on_calltip_search_complete), assist);
	/* Calltip in system */
	assist->priv->calltip_query_system =
		ianjuta_symbol_manager_create_query (isymbol_manager,
		                                     IANJUTA_SYMBOL_QUERY_SEARCH,
		                                     IANJUTA_SYMBOL_QUERY_DB_SYSTEM,
		                                     NULL);
	ianjuta_symbol_query_set_fields (assist->priv->calltip_query_system,
	                                 G_N_ELEMENTS (calltip_fields),
	                                 calltip_fields, NULL);
	ianjuta_symbol_query_set_filters (assist->priv->calltip_query_system,
	                                  IANJUTA_SYMBOL_TYPE_PROTOTYPE |
	                                  IANJUTA_SYMBOL_TYPE_METHOD |
	                                  IANJUTA_SYMBOL_TYPE_MACRO_WITH_ARG,
	                                  TRUE, NULL);
	ianjuta_symbol_query_set_file_scope (assist->priv->calltip_query_system,
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PUBLIC,
	                                     NULL);
	ianjuta_symbol_query_set_mode (assist->priv->calltip_query_system,
	                               IANJUTA_SYMBOL_QUERY_MODE_ASYNC, NULL);
	g_signal_connect (assist->priv->calltip_query_system, "async-result",
	                  G_CALLBACK (on_calltip_search_complete), assist);

	/* Create autocomplete queries */
	/* AC in file */
	assist->priv->ac_query_file =
		ianjuta_symbol_manager_create_query (isymbol_manager,
		                                     IANJUTA_SYMBOL_QUERY_SEARCH_FILE,
		                                     IANJUTA_SYMBOL_QUERY_DB_PROJECT,
		                                     NULL);
	ianjuta_symbol_query_set_group_by (assist->priv->ac_query_file,
	                                   IANJUTA_SYMBOL_FIELD_NAME, NULL);
	ianjuta_symbol_query_set_fields (assist->priv->ac_query_file,
	                                 G_N_ELEMENTS (ac_fields),
	                                 ac_fields, NULL);
	ianjuta_symbol_query_set_file_scope (assist->priv->ac_query_file,
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PRIVATE,
	                                     NULL);
	ianjuta_symbol_query_set_mode (assist->priv->ac_query_file,
	                               IANJUTA_SYMBOL_QUERY_MODE_ASYNC, NULL);
	g_signal_connect (assist->priv->ac_query_file, "async-result",
	                  G_CALLBACK (on_symbol_search_complete), assist);
	/* AC in project */
	assist->priv->ac_query_project =
		ianjuta_symbol_manager_create_query (isymbol_manager,
		                                     IANJUTA_SYMBOL_QUERY_SEARCH,
		                                     IANJUTA_SYMBOL_QUERY_DB_PROJECT,
		                                     NULL);
	ianjuta_symbol_query_set_group_by (assist->priv->ac_query_project,
	                                   IANJUTA_SYMBOL_FIELD_NAME, NULL);
	ianjuta_symbol_query_set_fields (assist->priv->ac_query_project,
	                                 G_N_ELEMENTS (ac_fields),
	                                 ac_fields, NULL);
	ianjuta_symbol_query_set_file_scope (assist->priv->ac_query_project,
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PUBLIC,
	                                     NULL);
	ianjuta_symbol_query_set_mode (assist->priv->ac_query_project,
	                               IANJUTA_SYMBOL_QUERY_MODE_ASYNC, NULL);
	g_signal_connect (assist->priv->ac_query_project, "async-result",
	                  G_CALLBACK (on_symbol_search_complete), assist);
	/* AC in system */
	assist->priv->ac_query_system =
		ianjuta_symbol_manager_create_query (isymbol_manager,
		                                     IANJUTA_SYMBOL_QUERY_SEARCH,
		                                     IANJUTA_SYMBOL_QUERY_DB_SYSTEM,
		                                     NULL);
	ianjuta_symbol_query_set_group_by (assist->priv->ac_query_system,
	                                   IANJUTA_SYMBOL_FIELD_NAME, NULL);
	ianjuta_symbol_query_set_fields (assist->priv->ac_query_system,
	                                 G_N_ELEMENTS (ac_fields),
	                                 ac_fields, NULL);
	ianjuta_symbol_query_set_file_scope (assist->priv->ac_query_system,
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PUBLIC,
	                                     NULL);
	ianjuta_symbol_query_set_mode (assist->priv->ac_query_system,
	                               IANJUTA_SYMBOL_QUERY_MODE_ASYNC, NULL);
	g_signal_connect (assist->priv->ac_query_system, "async-result",
	                  G_CALLBACK (on_symbol_search_complete), assist);

	/* Members autocompletion */
	assist->priv->query_members =
		ianjuta_symbol_manager_create_query (isymbol_manager,
		                                     IANJUTA_SYMBOL_QUERY_SEARCH_MEMBERS,
		                                     IANJUTA_SYMBOL_QUERY_DB_PROJECT,
		                                     NULL);
	ianjuta_symbol_query_set_fields (assist->priv->query_members,
	                                 G_N_ELEMENTS (ac_fields),
	                                 ac_fields, NULL);

	/* Create sync queries */
	/* Sync query in file */
	assist->priv->sync_query_file =
		ianjuta_symbol_manager_create_query (isymbol_manager,
		                                     IANJUTA_SYMBOL_QUERY_SEARCH_FILE,
		                                     IANJUTA_SYMBOL_QUERY_DB_PROJECT,
		                                     NULL);
	ianjuta_symbol_query_set_fields (assist->priv->sync_query_file,
	                                 G_N_ELEMENTS (calltip_fields),
	                                 calltip_fields, NULL);
	ianjuta_symbol_query_set_filters (assist->priv->sync_query_file,
	                                  IANJUTA_SYMBOL_TYPE_PROTOTYPE |
	                                  IANJUTA_SYMBOL_TYPE_FUNCTION |
	                                  IANJUTA_SYMBOL_TYPE_METHOD |
	                                  IANJUTA_SYMBOL_TYPE_MACRO_WITH_ARG,
	                                  TRUE, NULL);
	ianjuta_symbol_query_set_file_scope (assist->priv->sync_query_file,
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PRIVATE,
	                                     NULL);
	/* Sync query in project */
	assist->priv->sync_query_project =
		ianjuta_symbol_manager_create_query (isymbol_manager,
		                                     IANJUTA_SYMBOL_QUERY_SEARCH,
		                                     IANJUTA_SYMBOL_QUERY_DB_PROJECT,
		                                     NULL);
	ianjuta_symbol_query_set_fields (assist->priv->sync_query_project,
	                                 G_N_ELEMENTS (calltip_fields),
	                                 calltip_fields, NULL);
	ianjuta_symbol_query_set_filters (assist->priv->sync_query_project,
	                                  IANJUTA_SYMBOL_TYPE_PROTOTYPE |
	                                  IANJUTA_SYMBOL_TYPE_METHOD |
	                                  IANJUTA_SYMBOL_TYPE_MACRO_WITH_ARG,
	                                  TRUE, NULL);
	ianjuta_symbol_query_set_file_scope (assist->priv->sync_query_project,
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PUBLIC,
	                                     NULL);
	/* Sync query in system */
	assist->priv->sync_query_system =
		ianjuta_symbol_manager_create_query (isymbol_manager,
		                                     IANJUTA_SYMBOL_QUERY_SEARCH,
		                                     IANJUTA_SYMBOL_QUERY_DB_SYSTEM,
		                                     NULL);
	ianjuta_symbol_query_set_fields (assist->priv->sync_query_system,
	                                 G_N_ELEMENTS (calltip_fields),
	                                 calltip_fields, NULL);
	ianjuta_symbol_query_set_filters (assist->priv->sync_query_system,
	                                  IANJUTA_SYMBOL_TYPE_PROTOTYPE |
	                                  IANJUTA_SYMBOL_TYPE_METHOD |
	                                  IANJUTA_SYMBOL_TYPE_MACRO_WITH_ARG,
	                                  TRUE, NULL);
	ianjuta_symbol_query_set_file_scope (assist->priv->sync_query_system,
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PUBLIC,
	                                     NULL);

	/* Install support */
	parser_clang_assist_install (assist, imanager, ieditor, project_root);
	assist->priv->lang_prov = g_object_new (ANJUTA_TYPE_LANGUAGE_PROVIDER, NULL);
	anjuta_language_provider_install (assist->priv->lang_prov, ieditor, settings);
	parser_clang_assist_parse (assist,  NULL);
	
	return assist;
}

static void
parser_clang_assist_activate (IAnjutaProvider* self,
                              IAnjutaIterable* iter,
                              gpointer data,
                              GError** e)
{
	ParserClangAssist* assist = PARSER_CLANG_ASSIST (self);
	anjuta_language_provider_activate (assist->priv->lang_prov, self, iter,
	                                   data);
}

static void
parser_clang_assist_populate (IAnjutaProvider* self,
                              IAnjutaIterable* cursor,
                              GError** e)
{
	ParserClangAssist* assist = PARSER_CLANG_ASSIST (self);
	anjuta_language_provider_populate (assist->priv->lang_prov, self, cursor);
}

static const gchar*
parser_clang_assist_get_name (IAnjutaProvider* self,
                              GError** e)
{
	return _("C/C++");
}

static IAnjutaIterable*
parser_clang_assist_get_start_iter (IAnjutaProvider* self,
                                    GError** e)
{
	ParserClangAssist* assist = PARSER_CLANG_ASSIST (self);
	return anjuta_language_provider_get_start_iter (assist->priv->lang_prov);
}

static void
iprovider_iface_init (IAnjutaProviderIface* iface)
{
	iface->activate            = parser_clang_assist_activate;
	iface->populate            = parser_clang_assist_populate;
	iface->get_name            = parser_clang_assist_get_name;
	iface->get_start_iter      = parser_clang_assist_get_start_iter;
}

static void
ilanguage_provider_iface_init (IAnjutaLanguageProviderIface* iface)
{
	iface->get_calltip_cache   = parser_clang_assist_get_calltip_cache;
	iface->get_calltip_context = parser_clang_assist_get_calltip_context;
	iface->new_calltip         = parser_clang_assist_new_calltip;
	iface->populate_completions   = parser_clang_assist_populate_completions;
}
