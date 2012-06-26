/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cpp-java-assist.c
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
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/anjuta-utils.h>
#include <libanjuta/interfaces/ianjuta-file.h>
#include <libanjuta/interfaces/ianjuta-editor-cell.h>
#include <libanjuta/interfaces/ianjuta-editor-selection.h>
#include <libanjuta/interfaces/ianjuta-editor-assist.h>
#include <libanjuta/interfaces/ianjuta-editor-tip.h>
#include <libanjuta/interfaces/ianjuta-provider.h>
#include <libanjuta/interfaces/ianjuta-document.h>
#include <libanjuta/interfaces/ianjuta-symbol-manager.h>
#include "assist.h"
#include "engine-parser.h"

#define PREF_AUTOCOMPLETE_ENABLE "completion-enable"
#define PREF_AUTOCOMPLETE_SPACE_AFTER_FUNC "completion-space-after-func"
#define PREF_AUTOCOMPLETE_BRACE_AFTER_FUNC "completion-brace-after-func"
#define PREF_AUTOCOMPLETE_CLOSEBRACE_AFTER_FUNC "completion-closebrace-after-func"
#define PREF_CALLTIP_ENABLE "calltip-enable"
//TODO:
#define BRACE_SEARCH_LIMIT 500
#define SCOPE_CONTEXT_CHARACTERS "_.:>-0"
#define WORD_CHARACTER "_0"

G_DEFINE_TYPE_WITH_CODE (ParserCxxAssist, parser_cxx_assist, G_TYPE_OBJECT)

struct _ParserCxxAssistPriv {
	GSettings* settings;
	IAnjutaEditorAssist* iassist;
	IAnjutaEditorTip* itip;
	IAnjutaParser* parser;
	
	const gchar* editor_filename;

	/* Calltips */
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

/**
 * parser_cxx_assist_proposal_new:
 * @symbol: IAnjutaSymbol to create the proposal for
 *
 * Creates a new IAnjutaEditorAssistProposal for symbol
 *
 * Returns: a newly allocated IAnjutaEditorAssistProposal
 */
static IAnjutaEditorAssistProposal*
parser_cxx_assist_proposal_new (IAnjutaSymbol* symbol)
{
	IAnjutaEditorAssistProposal* proposal = g_new0 (IAnjutaEditorAssistProposal, 1);
	IAnjutaParserProposalData* data = g_new0 (IAnjutaParserProposalData, 1);
	
	data->name = g_strdup (ianjuta_symbol_get_string (symbol, IANJUTA_SYMBOL_FIELD_NAME, NULL));
	data->type = ianjuta_symbol_get_sym_type (symbol, NULL);
	switch (data->type)
	{
		case IANJUTA_SYMBOL_TYPE_PROTOTYPE:
		case IANJUTA_SYMBOL_TYPE_FUNCTION:
		case IANJUTA_SYMBOL_TYPE_METHOD:
		case IANJUTA_SYMBOL_TYPE_MACRO_WITH_ARG:
			proposal->label = g_strdup_printf ("%s()", data->name);
			data->is_func = TRUE;
			break;
		default:
			proposal->label = g_strdup (data->name);
			data->is_func = FALSE;
	}
	data->has_para = false;
	if (data->is_func)
	{
		const gchar* signature = ianjuta_symbol_get_string (symbol,
		                                                    IANJUTA_SYMBOL_FIELD_SIGNATURE,
		                                                    NULL);
		if (!g_strcmp0 (signature, "(void)") || !g_strcmp0 (signature, "()"))
			data->has_para = true;
	}
	
	proposal->data = data;
	/* Icons are lifetime object of the symbol-db so we can cast here */
	proposal->icon = (GdkPixbuf*) ianjuta_symbol_get_icon (symbol, NULL);
	return proposal;
}

/**
 * parser_cxx_assist_proposal_free:
 * @proposal: the proposal to free
 * 
 * Frees the proposal
 */
static void
parser_cxx_assist_proposal_free (IAnjutaEditorAssistProposal* proposal)
{
	IAnjutaParserProposalData* data = proposal->data;
	g_free (data->name);
	g_free (data);
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
	IAnjutaParserProposalData* prop_data = proposal->data;
	
	return prop_data->name;
}

/**
 * parser_cxx_assist_create_completion_from_symbols:
 * @symbols: Symbol iteration
 *
 * Create a list of IAnjutaEditorAssistProposals from a list of symbols
 *
 * Returns: a newly allocated GList of newly allocated proposals. Free
 * with cpp_java_assist_proposal_free()
 */
static GList*
parser_cxx_assist_create_completion_from_symbols (IAnjutaIterable* symbols)
{
	GList* list = NULL;

	if (!symbols)
		return NULL;
	do
	{
		IAnjutaSymbol* symbol = IANJUTA_SYMBOL (symbols);
		IAnjutaEditorAssistProposal* proposal = parser_cxx_assist_proposal_new (symbol);	

		list = g_list_append (list, proposal);
	}
	while (ianjuta_iterable_next (symbols, NULL));

	return list;
}

/**
 * parser_cxx_assist_update_pre_word:
 * @assist: self
 * @pre_word: new pre_word
 *
 * Updates the current pre_word
 */
static void
parser_cxx_assist_update_pre_word (ParserCxxAssist* assist, const gchar* pre_word)
{
	g_free (assist->priv->pre_word);
	if (pre_word == NULL)
		pre_word = "";
	assist->priv->pre_word = g_strdup (pre_word);
}

/**
 * parser_cxx_assist_is_expression_separator:
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
parser_cxx_assist_is_expression_separator (gchar c, gboolean skip_braces, IAnjutaIterable* iter)
{
	IAnjutaEditorAttribute attrib = ianjuta_editor_cell_get_attribute (IANJUTA_EDITOR_CELL(iter),
	                                                                   NULL);
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
 * parser_cxx_assist_parse_expression:
 * @assist: self,
 * @iter: current cursor position
 * @start_iter: return location for the start of the completion
 * 
 * Returns: An iter of a list of IAnjutaSymbols or NULL
 */
static IAnjutaIterable*
parser_cxx_assist_parse_expression (ParserCxxAssist* assist, IAnjutaIterable* iter, IAnjutaIterable** start_iter)
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
		gchar ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL(cur_pos), 0, NULL);
		
		if (parser_cxx_assist_is_expression_separator(ch, FALSE, iter)) {
			break;
		}

		if (ch == '.' || (op_start && ch == '-') || (ref_start && ch == ':'))
		{
			/* Found an operator, get the statement and the pre_word */
			IAnjutaIterable* pre_word_start = ianjuta_iterable_clone (cur_pos, NULL);
			IAnjutaIterable* pre_word_end = ianjuta_iterable_clone (iter, NULL);
			IAnjutaIterable* stmt_end = ianjuta_iterable_clone (pre_word_start, NULL);

			
			/* we need to pass to the parser all the statement included the last operator,
			 * being it "." or "->" or "::"
			 * Increase the end bound of the statement.
			 */
			ianjuta_iterable_next (stmt_end, NULL);
			if (op_start == TRUE || ref_start == TRUE)
				ianjuta_iterable_next (stmt_end, NULL);
				
			
			/* Move one character forward so we have the start of the pre_word and
			 * not the last operator */
			ianjuta_iterable_next (pre_word_start, NULL);
			/* If this is a two character operator, skip the second character */
			if (op_start)
			{
				ianjuta_iterable_next (pre_word_start, NULL);
			}
			
			parser_cxx_assist_update_pre_word (assist, 
			                                 ianjuta_editor_get_text (editor,
			                                                          pre_word_start, pre_word_end, NULL));

			/* Try to get the name of the variable */
			while (ianjuta_iterable_previous (cur_pos, NULL))
			{
				gchar word_ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL(cur_pos), 0, NULL);
				
				if (parser_cxx_assist_is_expression_separator(word_ch, FALSE, cur_pos)) 
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
		gint lineno;
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
		
		lineno = ianjuta_editor_get_lineno (editor, NULL);

		/* the parser works even for the "Gtk::" like expressions, so it shouldn't be 
		 * created a specific case to handle this.
		 */
		res = engine_parser_process_expression (stmt,
		                                        above_text,
		                                        assist->priv->editor_filename,
		                                        lineno);
		g_free (stmt);
	}
	g_object_unref (cur_pos);
	return res;
}

/** 
 * parser_cxx_assist_create_completion_cache:
 * @assist: self
 *
 * Create a new completion_cache object
 */
static void
parser_cxx_assist_create_completion_cache (ParserCxxAssist* assist)
{
	g_assert (assist->priv->completion_cache == NULL);
	assist->priv->completion_cache = 
		g_completion_new (anjuta_proposal_completion_func);
}

/**
 * parser_cxx_assist_cancel_queries:
 * @assist: self
 *
 * Abort all async operations
 */
static void
parser_cxx_assist_cancel_queries (ParserCxxAssist* assist)
{
	ianjuta_symbol_query_cancel (assist->priv->ac_query_file, NULL);
	ianjuta_symbol_query_cancel (assist->priv->ac_query_project, NULL);
	ianjuta_symbol_query_cancel (assist->priv->ac_query_system, NULL);
	assist->priv->async_file_id = 0;
	assist->priv->async_project_id = 0;
	assist->priv->async_system_id = 0;
}

/**
 * parser_cxx_assist_clear_completion_cache:
 * @assist: self
 *
 * Clear the completion cache, aborting all async operations
 */
static void
parser_cxx_assist_clear_completion_cache (ParserCxxAssist* assist)
{
	parser_cxx_assist_cancel_queries (assist);
	if (assist->priv->completion_cache)
	{	
		g_list_foreach (assist->priv->completion_cache->items, (GFunc) parser_cxx_assist_proposal_free, NULL);
		g_completion_free (assist->priv->completion_cache);
	}
	assist->priv->completion_cache = NULL;
	assist->priv->member_completion = FALSE;
	assist->priv->autocompletion = FALSE;
}

/**
 * parser_cxx_assist_populate_real:
 * @assist: self
 * @finished: TRUE if no more proposals are expected, FALSE otherwise
 *
 * Really invokes the completion interfaces and adds completions. Might be called
 * from an async context
 */
static void
parser_cxx_assist_populate_real (ParserCxxAssist* assist, gboolean finished)
{
	g_assert (assist->priv->pre_word != NULL);
	gchar* prefix;
	GList* proposals = g_completion_complete (assist->priv->completion_cache,
	                                          assist->priv->pre_word,
	                                          &prefix);
	if (g_list_length (proposals) == 1)
	{
		IAnjutaEditorAssistProposal* proposal = proposals->data;
		IAnjutaParserProposalData* data = proposal->data;
		if (g_str_equal (assist->priv->pre_word, data->name))
		{
			ianjuta_editor_assist_proposals (assist->priv->iassist,
			                                 IANJUTA_PROVIDER(assist),
			                                 NULL, finished, NULL);
			return;
		}
	}

	ianjuta_editor_assist_proposals (assist->priv->iassist,
	                                 IANJUTA_PROVIDER(assist),
	                                 proposals, finished, NULL);
}

/**
 * parser_cxx_assist_create_member_completion_cache
 * @assist: self
 * @cursor: Current cursor position
 * 
 * Create the completion_cache for member completion if possible
 *
 * Returns: TRUE if a completion cache was build, FALSE otherwise
 */
static gboolean
parser_cxx_assist_create_member_completion_cache (ParserCxxAssist* assist, IAnjutaIterable* cursor)
{
	IAnjutaIterable* symbol = NULL;
	IAnjutaIterable* start_iter = NULL;
	symbol = parser_cxx_assist_parse_expression (assist, cursor, &start_iter);

	if (symbol)
	{
		gint retval = FALSE;
		/* Query symbol children */
		IAnjutaIterable *children = 
			ianjuta_symbol_query_search_members (assist->priv->query_members,
			                                    IANJUTA_SYMBOL(symbol),
			                                    NULL);
		if (children)
		{
			GList* proposals = parser_cxx_assist_create_completion_from_symbols (children);
			parser_cxx_assist_create_completion_cache (assist);
			g_completion_add_items (assist->priv->completion_cache, proposals);

			ianjuta_parser_set_start_iter (assist->priv->parser, start_iter, NULL);

			parser_cxx_assist_populate_real (assist, TRUE);
			g_list_free (proposals);
			g_object_unref (children);
			retval = TRUE;
		}
		g_object_unref (symbol);
		return retval;
	}
	else if (start_iter)
		g_object_unref (start_iter);
	return FALSE;
}

/**
 * on_symbol_search_complete:
 * @search_id: id of this search
 * @symbols: the returned symbols
 * @assist: self
 *
 * Called by the async search method when it found symbols
 */
static void
on_symbol_search_complete (IAnjutaSymbolQuery *query, IAnjutaIterable* symbols,
						   ParserCxxAssist* assist)
{
	GList* proposals;
	proposals = parser_cxx_assist_create_completion_from_symbols (symbols);

	if (query == assist->priv->ac_query_file)
		assist->priv->async_file_id = 0;
	else if (query == assist->priv->ac_query_project)
		assist->priv->async_project_id = 0;
	else if (query == assist->priv->ac_query_system)
		assist->priv->async_system_id = 0;
	else
		g_assert_not_reached ();
	
	g_completion_add_items (assist->priv->completion_cache, proposals);
	gboolean running = assist->priv->async_system_id || assist->priv->async_file_id ||
		assist->priv->async_project_id;
	if (!running)
		parser_cxx_assist_populate_real (assist, TRUE);
	g_list_free (proposals);
}

/**
 * parser_cxx_assist_create_autocompletion_cache:
 * @assist: self
 * @cursor: Current cursor position
 * 
 * Create completion cache for autocompletion. This is done async.
 *
 * Returns: TRUE if a preword was detected, FALSE otherwise
 */ 
static gboolean
parser_cxx_assist_create_autocompletion_cache (ParserCxxAssist* assist, IAnjutaIterable* cursor)
{
	IAnjutaIterable* start_iter;
	gchar* pre_word = ianjuta_parser_get_pre_word (
                          assist->priv->parser, IANJUTA_EDITOR (assist->priv->iassist),
	                      cursor, &start_iter, WORD_CHARACTER, NULL);
	if (!pre_word || strlen (pre_word) <= 3)
	{
		if (start_iter)
			g_object_unref (start_iter);
		return FALSE;
	}
	else
	{
		gchar *pattern = g_strconcat (pre_word, "%", NULL);
		
		parser_cxx_assist_create_completion_cache (assist);
		parser_cxx_assist_update_pre_word (assist, pre_word);

		if (IANJUTA_IS_FILE (assist->priv->iassist))
		{
			GFile *file = ianjuta_file_get_file (IANJUTA_FILE (assist->priv->iassist), NULL);
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
		ianjuta_symbol_query_search (assist->priv->ac_query_project, pattern, NULL);
		assist->priv->async_system_id = 1;
		ianjuta_symbol_query_search (assist->priv->ac_query_system, pattern, NULL);
		g_free (pre_word);
		g_free (pattern);
		
		ianjuta_parser_set_start_iter (assist->priv->parser, start_iter, NULL);
		
		return TRUE;
	}
}

/**
 * on_calltip_search_complete:
 * @search_id: id of this search
 * @symbols: the returned symbols
 * @assist: self
 *
 * Called by the async search method when it found calltips
 */
//TODO: Move assist->priv->tips and assist->priv->calltip_iter to parser-engine
static void
on_calltip_search_complete (IAnjutaSymbolQuery *query, IAnjutaIterable* symbols,
							ParserCxxAssist* assist)
{
	assist->priv->tips = ianjuta_parser_create_calltips (assist->priv->parser, symbols, assist->priv->tips, NULL);
	if (query == assist->priv->calltip_query_file)
		assist->priv->async_calltip_file = 0;
	else if (query == assist->priv->calltip_query_project)
		assist->priv->async_calltip_project = 0;
	else if (query == assist->priv->calltip_query_system)
		assist->priv->async_calltip_system = 0;
	else
		g_assert_not_reached ();
	gboolean running = assist->priv->async_calltip_system || assist->priv->async_calltip_file ||
		assist->priv->async_calltip_project;

	DEBUG_PRINT ("Calltip search finished with %d items", g_list_length (assist->priv->tips));
	
	if (!running && assist->priv->tips)
	{
		ianjuta_editor_tip_show (IANJUTA_EDITOR_TIP(assist->priv->itip), assist->priv->tips,
		                         assist->priv->calltip_iter,
		                         NULL);
	}
}

/**
 * parser_cxx_assist_query_calltip:
 * @assist: self
 * @call_context: name of method/function
 *
 * Starts an async query for the calltip
 */
static void
parser_cxx_assist_query_calltip (ParserCxxAssist *assist, const gchar *call_context)
{	
	ParserCxxAssistPriv* priv = assist->priv;
	
	/* Search file */
	if (IANJUTA_IS_FILE (assist->priv->itip))
	{
		GFile *file = ianjuta_file_get_file (IANJUTA_FILE (priv->itip), NULL);

		if (file != NULL)
		{
			priv->async_calltip_file = 1;
			ianjuta_symbol_query_search_file (assist->priv->calltip_query_file,
				                                          call_context, file,
				                                          NULL);
			g_object_unref (file);
		}
	}

	/* Search Project */
	priv->async_calltip_project = 1;
	ianjuta_symbol_query_search (assist->priv->calltip_query_project,
		                                             call_context, NULL);
	
	/* Search system */
	assist->priv->async_calltip_system = 1;
	ianjuta_symbol_query_search (assist->priv->calltip_query_system,
		                                            call_context, NULL);
}

/**
 * parser_cxx_assist_clear_calltip_context:
 * @assist: self
 *
 * Clears the calltip context and brings it back into a save state
 */
//TODO: Move assist->priv->tips, assist->priv->calltip_iter and assist->priv->calltip_context to parser-engine
static void
parser_cxx_assist_clear_calltip_context (ParserCxxAssist* assist)
{
	ianjuta_symbol_query_cancel (assist->priv->calltip_query_file, NULL);
	ianjuta_symbol_query_cancel (assist->priv->calltip_query_project, NULL);
	ianjuta_symbol_query_cancel (assist->priv->calltip_query_system, NULL);

	assist->priv->async_calltip_file = 0;
	assist->priv->async_calltip_project = 0;
	assist->priv->async_calltip_system = 0;
	
	g_free (assist->priv->calltip_context);
	assist->priv->calltip_context = NULL;
	
	g_list_foreach (assist->priv->tips, (GFunc) g_free, NULL);
	g_list_free (assist->priv->tips);
	assist->priv->tips = NULL;

	if (assist->priv->calltip_iter)
		g_object_unref (assist->priv->calltip_iter);
	assist->priv->calltip_iter = NULL;
}

/**
 * parser_cxx_assist_cancelled:
 * @iassist: IAnjutaEditorAssist that emitted the signal
 * @assist: ParserCxxAssist object
 *
 * Stop any autocompletion queries when the cancelled signal was received
 */
static void
parser_cxx_assist_cancelled (IAnjutaEditorAssist* iassist, ParserCxxAssist* assist)
{
	parser_cxx_assist_cancel_queries (assist);
}

/**
 * parser_cxx_assist_none:
 * @self: IAnjutaProvider object
 * @assist: ParserCxxAssist object
 *
 * Indicate that there is nothing to autocomplete
 */
static void
parser_cxx_assist_none (IAnjutaProvider* self,
                        ParserCxxAssist* assist)
{
	ianjuta_editor_assist_proposals (assist->priv->iassist,
	                                 self,
	                                 NULL, TRUE, NULL);
}


/**
 * parser_cxx_assist_populate:
 * @self: IAnjutaProvider object
 * @cursor: Iter at current cursor position (after current character)
 * @e: Error population
 */
static void
parser_cxx_assist_populate (IAnjutaProvider* self, IAnjutaIterable* cursor, GError** e)
{
	ParserCxxAssist* assist = PARSER_CXX_ASSIST (self);
	
	/* Check if we actually want autocompletion at all */
	if (!g_settings_get_boolean (assist->priv->settings,
	                             PREF_AUTOCOMPLETE_ENABLE))
	{
		parser_cxx_assist_none (self, assist);
		return;
	}
	
	/* Check if this is a valid text region for completion */
	IAnjutaEditorAttribute attrib = ianjuta_editor_cell_get_attribute (IANJUTA_EDITOR_CELL(cursor),
	                                                                   NULL);
	if (attrib == IANJUTA_EDITOR_STRING ||
	    attrib == IANJUTA_EDITOR_COMMENT)
	{
		parser_cxx_assist_none (self, assist);
		return;
	}

	/* Check for calltip */
	if (assist->priv->itip && 
	    g_settings_get_boolean (assist->priv->settings,
	                            PREF_CALLTIP_ENABLE))
	{	
		//TODO: use parser_engine_calltip (parser)
		parser_cxx_assist_calltip (assist);
			
	}
	
	/* Check if completion was in progress */
	if (assist->priv->member_completion || assist->priv->autocompletion)
	{
		IAnjutaIterable* start_iter = NULL;
		g_assert (assist->priv->completion_cache != NULL);
		gchar* pre_word = ianjuta_parser_get_pre_word (
		              	      assist->priv->parser, IANJUTA_EDITOR (assist->priv->iassist),
		                      cursor, &start_iter, WORD_CHARACTER, NULL);
		if (pre_word && g_str_has_prefix (pre_word, assist->priv->pre_word))
		{
			/* Great, we just continue the current completion */
			g_object_unref (assist->priv->start_iter);
			assist->priv->start_iter = start_iter;

			parser_cxx_assist_update_pre_word (assist, pre_word);
			parser_cxx_assist_populate_real (assist, TRUE);
			g_free (pre_word);
			return;
		}			
		g_free (pre_word);
	}

	parser_cxx_assist_clear_completion_cache (assist);
	
	/* Check for member completion */
	if (parser_cxx_assist_create_member_completion_cache (assist, cursor))
	{
		assist->priv->member_completion = TRUE;
		return;
	}
	else if (parser_cxx_assist_create_autocompletion_cache (assist, cursor))
	{
		assist->priv->autocompletion = TRUE;
		return;
	}		
	/* Nothing to propose */
	if (assist->priv->start_iter)
	{
		g_object_unref (assist->priv->start_iter);
		assist->priv->start_iter = NULL;
	}
	parser_cxx_assist_none (self, assist);
}

/**
 * parser_cxx_assist_install:
 * @self: IAnjutaProvider object
 * @ieditor: Editor to install support for
 * @iparser: Parser to install support for
 *
 * Returns: Registers provider for editor
 */
static void
parser_cxx_assist_install (ParserCxxAssist *assist, IAnjutaEditor *ieditor, IAnjutaParser *iparser)
{
	g_return_if_fail (assist->priv->iassist == NULL);

	if (IANJUTA_IS_EDITOR_ASSIST (ieditor))
	{
		assist->priv->iassist = IANJUTA_EDITOR_ASSIST (ieditor);
		g_signal_connect (ieditor, "cancelled", G_CALLBACK (parser_cxx_assist_cancelled), assist);
	}
	else
		assist->priv->iassist = NULL;

	if (IANJUTA_IS_EDITOR_TIP (ieditor))
		assist->priv->itip = IANJUTA_EDITOR_TIP (ieditor);
	else
		assist->priv->itip = NULL;

	if (IANJUTA_IS_PARSER (iparser))
		assist->priv->parser = IANJUTA_PARSER (iparser);
	else
		assist->priv->parser = NULL;
		
	if (IANJUTA_IS_FILE (assist->priv->iassist))
	{
		GFile *file = ianjuta_file_get_file (IANJUTA_FILE (assist->priv->iassist), NULL);
		if (file != NULL)
		{
			assist->priv->editor_filename = g_file_get_path (file);
			g_object_unref (file);
		}
	}
}

/**
 * parser_cxx_assist_uninstall:
 * @self: IAnjutaProvider object
 *
 * Returns: Unregisters provider
 */
static void
parser_cxx_assist_uninstall (ParserCxxAssist *assist)
{
	g_return_if_fail (assist->priv->iassist != NULL);
	
	g_signal_handlers_disconnect_by_func (assist->priv->iassist,
	                                      parser_cxx_assist_cancelled, assist);
	assist->priv->iassist = NULL;
}

static void
parser_cxx_assist_init (ParserCxxAssist *assist)
{
	assist->priv = g_new0 (ParserCxxAssistPriv, 1);
}

static void
parser_cxx_assist_finalize (GObject *object)
{
	ParserCxxAssist *assist = PARSER_CXX_ASSIST (object);
	ParserCxxAssistPriv* priv = assist->priv;
	
	parser_cxx_assist_uninstall (assist);
	parser_cxx_assist_clear_completion_cache (assist);
	parser_cxx_assist_clear_calltip_context (assist);


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

	engine_parser_deinit ();
	
	g_free (assist->priv);
	G_OBJECT_CLASS (parser_cxx_assist_parent_class)->finalize (object);
}

static void
parser_cxx_assist_class_init (ParserCxxAssistClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = parser_cxx_assist_finalize;
}

ParserCxxAssist *
parser_cxx_assist_new (IAnjutaEditor *ieditor,
                       IAnjutaParser *iparser,
                       IAnjutaSymbolManager *isymbol_manager,
                       GSettings* settings)
{
	ParserCxxAssist *assist;
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
		IANJUTA_SYMBOL_FIELD_ACCESS
	};

	if (!IANJUTA_IS_EDITOR_ASSIST (ieditor) && !IANJUTA_IS_EDITOR_TIP (ieditor))
	{
		/* No assistance is available with the current editor */
		return NULL;
	}
	assist = g_object_new (TYPE_PARSER_CXX_ASSIST, NULL);
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
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PRIVATE, NULL);
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
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PUBLIC, NULL);
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
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PUBLIC, NULL);
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
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PRIVATE, NULL);
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
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PUBLIC, NULL);
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
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PUBLIC, NULL);
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
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PRIVATE, NULL);
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
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PUBLIC, NULL);
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
	                                     IANJUTA_SYMBOL_QUERY_SEARCH_FS_PUBLIC, NULL);

	/* Install support */
	parser_cxx_assist_install (assist, ieditor, iparser);

	engine_parser_init (isymbol_manager);	
	
	return assist;
}
