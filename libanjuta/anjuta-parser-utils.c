/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-parser-utils.c
 * Copyright (C) Naba Kumar  <naba@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <libanjuta/anjuta-utils.h>
#include <libanjuta/anjuta-parser-utils.h>
#include <libanjuta/interfaces/ianjuta-editor-cell.h>

#define SCOPE_BRACE_JUMP_LIMIT 50
#define BRACE_SEARCH_LIMIT 500

/**
 * anjuta_parser_util_is_character:
 * @ch: character to check
 * @context_characters: language-specific context characters
 *                      the end is marked with a '0' character
 *
 * Returns: if the current character seperates a scope
 */
static gboolean
anjuta_parser_util_is_character (gchar ch, const gchar* characters)
{
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
 * anjuta_parser_util_get_scope_context
 * @editor: current editor
 * @iter: Current cursor position
 * @scope_context_characters: language-specific context characters
 *                            the end is marked with a '0' character
 *
 * Find the scope context for calltips
 */
static gchar*
anjuta_parser_util_get_scope_context (IAnjutaEditor* editor, IAnjutaIterable *iter,
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
		if (anjuta_parser_util_is_character (ch, scope_context_characters))
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

/**
 * anjuta_parser_util_get_calltip_context:
 * @itip: whether a tooltip is crrently shown
 * @iter: current cursor position
 * @scope_context_characters: language-specific context characters
 *                            the end is marked with a '0' character
 *
 * Searches for a calltip context
 *
 * Returns: name of the method to show a calltip for or NULL
 */
gchar*
anjuta_parser_util_get_calltip_context (IAnjutaEditorTip* itip, IAnjutaIterable* iter,
                                        const gchar* scope_context_characters)
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
	
	context = anjuta_parser_util_get_scope_context (IANJUTA_EDITOR (itip),
	                                                iter, scope_context_characters);

	/* Point iter to the first character of the scope to align calltip correctly */
	ianjuta_iterable_next (iter, NULL);
	
	return context;
}

/**
 * anjuta_parser_util_get_pre_word:
 * @editor: Editor object
 * @iter: current cursor position
 * @start_iter: return location for the start_iter (if a preword was found)
 *
 * Search for the current typed word
 *
 * Returns: The current word (needs to be freed) or NULL if no word was found
 */
gchar*
anjuta_parser_util_get_pre_word (IAnjutaEditor* editor, IAnjutaIterable *iter,
                                 IAnjutaIterable** start_iter, const gchar *word_characters)
{
	IAnjutaIterable *end = ianjuta_iterable_clone (iter, NULL);
	IAnjutaIterable *begin = ianjuta_iterable_clone (iter, NULL);
	gchar ch, *preword_chars = NULL;
	gboolean out_of_range = FALSE;
	gboolean preword_found = FALSE;
	
	/* Cursor points after the current characters, move back */
	ianjuta_iterable_previous (begin, NULL);
	
	ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (begin), 0, NULL);
	
	while (ch && anjuta_parser_util_is_character (ch, word_characters))
	{
		preword_found = TRUE;
		if (!ianjuta_iterable_previous (begin, NULL))
		{
			out_of_range = TRUE;
			break;
		}
		ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (begin), 0, NULL);
	}
	
	if (preword_found)
	{
		if (!out_of_range)
			ianjuta_iterable_next (begin, NULL);
		preword_chars = ianjuta_editor_get_text (editor, begin, end, NULL);
		*start_iter = begin;
	}
	else
	{
		g_object_unref (begin);
		*start_iter = NULL;
	}
	
	g_object_unref (end);
	return preword_chars;
}
