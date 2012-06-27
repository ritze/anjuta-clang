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
#include <libanjuta/interfaces/ianjuta-symbol-manager.h>
#include <libanjuta/anjuta-utils.h>

#include "utils.h"

#define SCOPE_BRACE_JUMP_LIMIT 50
#define BRACE_SEARCH_LIMIT 500

/**
 * parser_engine_is_character:
 * @ch: character to check
 * @context_characters: language-specific context characters
 *                      the end is marked with a '0' character
 *
 * Returns: if the current character seperates a scope
 */
static gboolean
parser_engine_utils_is_character (gchar ch,
                                  const gchar* characters)
{
g_warning ("parser_engine_is_scope_context_character works");
	int i;
	
	if (g_ascii_isspace (ch))
		return FALSE;
	if (g_ascii_isalnum (ch))
		return TRUE;
	for (i = 0; characters[i] != '0'; i++)
	{
		if (ch == characters[i])
		{
			return TRUE;
		}
	}
	
	return FALSE;
}

/**
 * parser_engine_utils_get_scope_context
 * @editor: current editor
 * @iter: Current cursor position
 * @scope_context_characters: language-specific context characters
 *                            the end is marked with a '0' character
 *
 * Find the scope context for calltips
 */
static gchar*
parser_engine_utils_get_scope_context (IAnjutaEditor* editor,
                                       IAnjutaIterable *iter,
                                       const gchar* scope_context_characters)
{
	IAnjutaIterable* end;	
	gchar ch, *scope_chars = NULL;
	gboolean out_of_range = FALSE;
	gboolean scope_chars_found = FALSE;
	
	end = ianjuta_iterable_clone (iter, NULL);
	ianjuta_iterable_next (end, NULL);
	
	ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (iter), 0, NULL);
	
	while (ch)
	{
		if (parser_engine_utils_is_character (ch, scope_context_characters))
			scope_chars_found = TRUE;
		else if (ch == ')')
		{
			if (!anjuta_util_jump_to_matching_brace (iter, ch, SCOPE_BRACE_JUMP_LIMIT))
			{
				out_of_range = TRUE;
				break;
			}
		}
		else
			break;
		if (!ianjuta_iterable_previous (iter, NULL))
		{
			out_of_range = TRUE;
			break;
		}		
		ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (iter), 0, NULL);
	}
	if (scope_chars_found)
	{
		IAnjutaIterable* begin;
		begin = ianjuta_iterable_clone (iter, NULL);
		if (!out_of_range)
			ianjuta_iterable_next (begin, NULL);
		scope_chars = ianjuta_editor_get_text (editor, begin, end, NULL);
		g_object_unref (begin);
	}
	g_object_unref (end);
	return scope_chars;
}

static gchar*
iparser_utils_get_calltip_context (IAnjutaParser* self,
                                   IAnjutaEditorTip* itip,
                                   IAnjutaIterable* iter,
                                   const gchar* scope_context_characters,
                                   GError** e)
{
	gchar ch;
	gchar *context = NULL;
	
	ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (iter), 0, NULL);
	if (ch == ')')
	{
		if (!anjuta_util_jump_to_matching_brace (iter, ')', -1))
			return NULL;
		if (!ianjuta_iterable_previous (iter, NULL))
			return NULL;
	}
	if (ch != '(')
	{
		if (!anjuta_util_jump_to_matching_brace (iter, ')',
												   BRACE_SEARCH_LIMIT))
			return NULL;
	}
	
	/* Skip white spaces */
	while (ianjuta_iterable_previous (iter, NULL)
		&& g_ascii_isspace (ianjuta_editor_cell_get_char
								(IANJUTA_EDITOR_CELL (iter), 0, NULL)));
	
	context = parser_engine_utils_get_scope_context (IANJUTA_EDITOR (itip),
	                                                 iter, scope_context_characters);

	/* Point iter to the first character of the scope to align calltip correctly */
	ianjuta_iterable_next (iter, NULL);
	
	return context;
}
