/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-provider-utils.c
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
#include <libanjuta/anjuta-language-provider.h>
#include <libanjuta/interfaces/ianjuta-document.h>
#include <libanjuta/interfaces/ianjuta-editor-assist.h>
#include <libanjuta/interfaces/ianjuta-editor-cell.h>
#include <libanjuta/interfaces/ianjuta-editor-selection.h>
#include <libanjuta/interfaces/ianjuta-editor-tip.h>
#include <libanjuta/interfaces/ianjuta-provider.h>

#define SCOPE_BRACE_JUMP_LIMIT 50
#define BRACE_SEARCH_LIMIT 500

struct _AnjutaLanguageProviderPriv {
	/* Autocompletion */
	IAnjutaIterable* start_iter;
	
	/* Calltips */
	gchar* calltip_context;
	GList* tips;
	IAnjutaIterable* calltip_iter;
};

/**
 * anjuta_language_provider_find_next_brace:
 * @iter: Iter to start searching at
 *
 * Returns: The position of the brace, if the next non-whitespace character is a
 * opening brace, NULL otherwise
 */
static IAnjutaIterable*
anjuta_language_provider_find_next_brace (IAnjutaIterable* iter)
{
	IAnjutaIterable* current_iter = ianjuta_iterable_clone (iter, NULL);
	gchar ch;
	do
	{
		ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (current_iter),
		                                   0, NULL);
		if (ch == '(')
			return current_iter;
	}
	while (g_ascii_isspace (ch) && ianjuta_iterable_next (current_iter, NULL));
	
	g_object_unref (current_iter);
	return NULL;
}

/**
 * anjuta_language_provider_find_whitespace:
 * @iter: Iter to start searching at
 *
 * Returns: TRUE if the next character is a whitespace character,
 * FALSE otherwise
 */
static gboolean
anjuta_language_provider_find_whitespace (IAnjutaIterable* iter)
{
	gchar ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (iter),
	                                         0, NULL);
	if (g_ascii_isspace (ch) && ch != '\n'
	    && anjuta_language_provider_find_next_brace (iter))
	{
		return TRUE;
	}
	else
		return FALSE;
}

/**
 * anjuta_language_provider_is_character:
 * @ch: character to check
 * @context_characters: language-specific context characters
 *                      the end is marked with a '0' character
 *
 * Returns: if the current character seperates a scope
 */
static gboolean
anjuta_language_provider_is_character (gchar ch, const gchar* characters)
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
 * anjuta_language_provider_get_scope_context
 * @editor: current editor
 * @iter: Current cursor position
 * @scope_context_characters: language-specific context characters
 *                            the end is marked with a '0' character
 *
 * Find the scope context for calltips
 */
static gchar*
anjuta_language_provider_get_scope_context (IAnjutaEditor* editor,
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
		if (anjuta_language_provider_is_character (ch, scope_context_characters))
			scope_chars_found = TRUE;
		else if (ch == ')')
		{
			if (!anjuta_util_jump_to_matching_brace (iter, ch,
						                             SCOPE_BRACE_JUMP_LIMIT))
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
 * anjuta_language_provider_get_calltip_context:
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
anjuta_language_provider_get_calltip_context (IAnjutaEditorTip* itip,
		                                      IAnjutaIterable* iter,
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
		if (!anjuta_util_jump_to_matching_brace (iter, ')', BRACE_SEARCH_LIMIT))
			return NULL;
	}
	
	/* Skip white spaces */
	while (ianjuta_iterable_previous (iter, NULL)
		&& g_ascii_isspace (ianjuta_editor_cell_get_char
								(IANJUTA_EDITOR_CELL (iter), 0, NULL)));
	
	context = anjuta_language_provider_get_scope_context (IANJUTA_EDITOR (itip),
			                                          iter,
													  scope_context_characters);

	/* Point iter to the first character of the scope to align calltip correctly */
	ianjuta_iterable_next (iter, NULL);
	
	return context;
}

/**
 * anjuta_language_provider_get_pre_word:
 * @editor: Editor object
 * @iter: current cursor position
 * @start_iter: return location for the start_iter (if a preword was found)
 *
 * Search for the current typed word
 *
 * Returns: The current word (needs to be freed) or NULL if no word was found
 */
gchar*
anjuta_language_provider_get_pre_word (IAnjutaEditor* editor,
                                       IAnjutaIterable *iter,
                                       IAnjutaIterable** start_iter,
                                       const gchar *word_characters)
{
	IAnjutaIterable *end = ianjuta_iterable_clone (iter, NULL);
	IAnjutaIterable *begin = ianjuta_iterable_clone (iter, NULL);
	gchar ch, *preword_chars = NULL;
	gboolean out_of_range = FALSE;
	gboolean preword_found = FALSE;
	
	/* Cursor points after the current characters, move back */
	ianjuta_iterable_previous (begin, NULL);
	
	ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (begin), 0, NULL);
	
	while (ch && anjuta_language_provider_is_character (ch, word_characters))
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

/* Support methods for IAnjutaProvider */

static void
anjuta_language_provider_clear_calltip_context (AnjutaLanguageProvider* lang_prov)
{
g_warning ("anjuta_language_provider_clear_calltip_context");
	g_free (lang_prov->priv->calltip_context);
	lang_prov->priv->calltip_context = NULL;
	
	g_list_foreach (lang_prov->priv->tips, (GFunc) g_free, NULL);
	g_list_free (lang_prov->priv->tips);
	lang_prov->priv->tips = NULL;

	if (lang_prov->priv->calltip_iter)
		g_object_unref (lang_prov->priv->calltip_iter);
	lang_prov->priv->calltip_iter = NULL;
}

/**
 * anjuta_language_provider_create_calltip_context:
 * @lang_prov: self
 * @call_context: The context (method/function name)
 * @position: iter where to show calltips
 *
 * Create the calltip context
 */
static void
anjuta_language_provider_create_calltip_context (AnjutaLanguageProvider* lang_prov,
                                                 const gchar* call_context,
                                                 IAnjutaIterable* position)
{
g_warning ("anjuta_language_provider_create_calltip_context");
	lang_prov->priv->calltip_context = g_strdup (call_context);
	lang_prov->priv->calltip_iter = position;
}

/**
 * anjuta_language_provider_calltip:
 * @lang_prov: self
 * @provider: IAnjutaProvider object
 * 
 * Creates a calltip if there is something to show a tip for
 * Calltips are queried async
 *
 * Returns: TRUE if a calltips was queried, FALSE otherwise
 */
static gboolean
anjuta_language_provider_calltip (AnjutaLanguageProvider lang_prov,
                                  IAnjutaProvider* provider)
{
g_warning ("anjuta_language_provider_calltip");
	IAnjutaEditor *editor;
	IAnjutaEditorTip *tip;
	IAnjutaIterable *iter;
	gchar *call_context;
	
	editor = ianjuta_language_provider_get_editor (provider, NULL);
	tip = IANJUTA_EDITOR_TIP (editor);
	
	iter = ianjuta_editor_get_position (editor, NULL);
	ianjuta_iterable_previous (iter, NULL);
	
	call_context = ianjuta_language_provider_get_context (provider, iter, NULL);
	if (call_context)
	{
		DEBUG_PRINT ("Searching calltip for: %s", call_context);
		if (lang_prov->priv->calltip_context
		    && g_str_equal (call_context, lang_prov->priv->calltip_context))
		{
			/* Continue tip */
			if (lang_prov->priv->tips)
			{
				if (!ianjuta_editor_tip_visible (tip, NULL))
				{
					ianjuta_editor_tip_show (tip, lang_prov->priv->tips,
					                         lang_prov->priv->calltip_iter,
					                         NULL);
				}
			}
		}
		else
		{
			/* New tip */
			if (ianjuta_editor_tip_visible (tip, NULL))
				ianjuta_editor_tip_cancel (tip, NULL);
				
			ianjuta_language_provider_clear_context (provider, NULL);
			anjuta_language_provider_clear_calltip_context (lang_prov);
			anjuta_language_provider_create_calltip_context (lang_prov,
			                                                 call_context, iter);
			ianjuta_language_provider_query (provider, call_context, NULL);
		}
		
		g_free (call_context);
		return TRUE;
	}
	else
	{
		if (ianjuta_editor_tip_visible (tip, NULL))
			ianjuta_editor_tip_cancel (tip, NULL);
		ianjuta_language_provider_clear_context (provider, NULL);
		
		g_object_unref (iter);
		return FALSE;
	}
}

/**
 * anjuta_language_provider_none:
 * @provider: IAnjutaProvider object
 *
 * Indicate that there is nothing to autocomplete
 */
static void
anjuta_language_provider_none (IAnjutaProvider* provider)
{
g_warning ("anjuta_language_provider_none");
	IAnjutaEditor* editor = ianjuta_language_provider_get_editor (provider,
	                                                              NULL);
	
	if (IANJUTA_IS_EDITOR_ASSIST (editor))
		ianjuta_editor_assist_proposals (IANJUTA_EDITOR_ASSIST (editor),
		                                 provider, NULL, NULL, TRUE, NULL);
}

void
anjuta_language_provider_activate (IAnjutaProvider* provider,
                                   IAnjutaIterable* iter,
                                   gpointer data,
                                   GError** e)
{
g_warning ("anjuta_language_provider_activate");
	AnjutaLanguageProvider* lang_prov = AnjutaLanguageProvider (provider);
	IAnjutaLanguageProviderProposalData *prop_data;
	GString *assistance;
	IAnjutaEditor *editor;
	gboolean add_space_after_func = FALSE;
	gboolean add_brace_after_func = FALSE;
	gboolean add_closebrace_after_func = FALSE;
	
	g_return_if_fail (data != NULL);
	prop_data = data;
	assistance = g_string_new (prop_data->name);
	
	if (prop_data->is_func)
	{
		IAnjutaIterable* next_brace = anjuta_language_provider_find_next_brace (iter);
		add_space_after_func = ianjuta_language_provider_get_boolean (provider,
			IANJUTA_LANGUAGE_PROVIDER_PREF_AUTOCOMPLETE_SPACE_AFTER_FUNC, NULL);
		add_brace_after_func = ianjuta_language_provider_get_boolean (provider,
		    IANJUTA_LANGUAGE_PROVIDER_PREF_AUTOCOMPLETE_BRACE_AFTER_FUNC, NULL);
		add_closebrace_after_func = ianjuta_language_provider_get_boolean (provider,
		    IANJUTA_LANGUAGE_PROVIDER_PREF_AUTOCOMPLETE_CLOSEBRACE_AFTER_FUNC, NULL);

		if (add_space_after_func
			&& !anjuta_language_provider_find_whitespace (iter))
			g_string_append (assistance, " ");
		if (add_brace_after_func && !next_brace)
			g_string_append (assistance, "(");
		else
			g_object_unref (next_brace);
	}
	
	editor = ianjuta_language_provider_get_editor (provider, NULL);
		
	ianjuta_document_begin_undo_action (IANJUTA_DOCUMENT (editor), NULL);
	
	if (ianjuta_iterable_compare (iter, provider->priv->start_iter, NULL) != 0)
	{
		ianjuta_editor_selection_set (IANJUTA_EDITOR_SELECTION (editor),
									  provider->priv->start_iter, iter, FALSE,
									  NULL);
		ianjuta_editor_selection_replace (IANJUTA_EDITOR_SELECTION (editor),
										  assistance->str, -1, NULL);
	}
	else
		ianjuta_editor_insert (editor, iter, assistance->str, -1, NULL);
	
	if (add_brace_after_func && add_closebrace_after_func)
	{
		IAnjutaIterable *next_brace;
		IAnjutaIterable *pos = ianjuta_iterable_clone (iter, NULL);

		ianjuta_iterable_set_position (pos,
		                               ianjuta_iterable_get_position (
									           provider->priv->start_iter, NULL)
									   + strlen (assistance->str),
									   NULL);
		next_brace = anjuta_language_provider_find_next_brace (pos);
		if (!next_brace)
			ianjuta_editor_insert (editor, pos, ")", -1, NULL);
		else
		{
			pos = next_brace;
			ianjuta_iterable_next (pos, NULL);
		}
		
		ianjuta_editor_goto_position (editor, pos, NULL);

		ianjuta_iterable_previous (pos, NULL);
		if (!prop_data->has_para)
		{
			pos = ianjuta_editor_get_position (editor, NULL);
			ianjuta_iterable_next (pos, NULL);
			ianjuta_editor_goto_position (editor, pos, NULL);
		}
		
		g_object_unref (pos);
	}

	ianjuta_document_end_undo_action (IANJUTA_DOCUMENT (editor), NULL);

	/* Show calltip if we completed function */
	if (add_brace_after_func)
	{
		/* Check for calltip */
		if (IANJUTA_IS_EDITOR_TIP (editor) &&
		    ianjuta_language_provider_get_boolean (provider,
		                                           PREF_CALLTIP_ENABLE, NULL))
		    //TODO: adapt
		    //Vala: show_call_tip (IAnjuta.EditorTip editor)
		    //TODO: JS  doesn't support calltip yet. I only found this:
		    /*
		    	GList *t = NULL;
				gchar *args = code_completion_get_func_tooltip (plugin, sym);
				t = g_list_append (t, args);
				if (args)
				{
					ianjuta_editor_tip_show (IANJUTA_EDITOR_TIP(plugin->current_editor), t,
							 position, NULL);
					g_free (args);
				}
		    */
			anjuta_language_provider_calltip (provider);

	}
	g_string_free (assistance, TRUE);
}

void
anjuta_language_provider_populate (IAnjutaProvider* provider,
                                   IAnjutaIterable* cursor,
                                   GError** e)
{
g_warning ("anjuta_language_provider_populate");
	AnjutaLanguageProvider* lang_prov;
	IAnjutaEditor *editor;
	IAnjutaIterable *start_iter;
	
	lang_prov = ANJUTA_LANGUAGE_PROVIDER (provider);

	/* Check if we actually want autocompletion at all */
	if (!ianjuta_language_provider_get_boolean (provider,
		            IANJUTA_LANGUAGE_PROVIDER_PREF_AUTOCOMPLETE_ENABLE, NULL))
	{
		anjuta_language_provider_none (provider);
		return;
	}
	
	/* Check if this is a valid text region for completion */
	IAnjutaEditorAttribute attrib = ianjuta_editor_cell_get_attribute (
	                                        IANJUTA_EDITOR_CELL(cursor), NULL);
	if (attrib == IANJUTA_EDITOR_COMMENT /*TODO: || attrib == IANJUTA_EDITOR_STRING*/)
	{
		anjuta_language_provider_none (provider);
		return;
	}

	/* Check for calltip */
	editor = ianjuta_language_provider_get_editor (provider, NULL);
	if (IANJUTA_IS_EDITOR_TIP (editor) && 
	    ianjuta_language_provider_get_boolean (provider, PREF_CALLTIP_ENABLE,
	                                           NULL))
	{	
		anjuta_language_provider_calltip (lang_prov);
	}
	
	/* Execute language-specific part */
	start_iter = ianjuta_language_provider_populate (provider, cursor, NULL);
	if (start_iter)
	{
		if (lang_prov->priv->start_iter)
			g_object_unref (lang_prov->priv->start_iter);
		lang_prov->priv->start_iter = start_iter;
		return;
	}

	/* Nothing to propose */
	if (lang_prov->priv->start_iter)
	{
		g_object_unref (lang_prov->priv->start_iter);
		lang_prov->priv->start_iter = NULL;
	}
	anjuta_language_provider_none (provider);
}

IAnjutaIterable*
anjuta_language_provider_get_start_iter (IAnjutaProvider* provider, GError** e)
{
g_warning ("anjuta_language_provider_language_get_start_iter");
	AnjutaLanguageProvider* lang_prov = ANJUTA_LANGUAGE_PROVIDER (provider);
	return lang_prov->priv->start_iter;
}
