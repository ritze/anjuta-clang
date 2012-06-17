/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    plugin.c
    Copyright (C) 2000 Naba Kumar

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <config.h>
#include <ctype.h>
#include <stdlib.h>
#include <libanjuta/anjuta-shell.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/interfaces/ianjuta-editor-assist.h>
#include <libanjuta/interfaces/ianjuta-editor-cell.h>
#include <libanjuta/interfaces/ianjuta-document.h>
#include <libanjuta/interfaces/ianjuta-document-manager.h>
#include <libanjuta/interfaces/ianjuta-language.h>
#include <libanjuta/interfaces/ianjuta-parser.h>
#include <libanjuta/interfaces/ianjuta-preferences.h>

#include "plugin.h"

#define ICON_FILE "anjuta-parser-engine-plugin.png"

static gpointer parent_class;

/* Enable/Disable language-support */
static void
install_support (ParserEnginePlugin *parser_plugin)
{
    IAnjutaLanguage* lang_manager =
        anjuta_shell_get_interface (ANJUTA_PLUGIN (parser_plugin)->shell,
                                    IAnjutaLanguage, NULL);
	
    if (!lang_manager)
        return;

	if (parser_plugin->support_installed)
        return;

    parser_plugin->current_language =
        ianjuta_language_get_name_from_editor (lang_manager,
                                               IANJUTA_EDITOR_LANGUAGE (parser_plugin->current_editor), NULL);

    DEBUG_PRINT("Parser support installed for: %s",
                parser_plugin->current_language);
/*	
	if (parser_plugin->current_language &&
	    (g_str_equal (parser_plugin->current_language, "C" )
	     || g_str_equal (parser_plugin->current_language, "C++")
	     || g_str_equal (parser_plugin->current_language, "Vala")
		 //JS
		 //python
		 //...
		 ))
	{
		ParserEngineAssist *assist;
	
		g_assert (parser_plugin->assist == NULL);
	
		assist = parser_engine_assist_new (IANJUTA_EDITOR (parser_plugin->current_editor),
					anjuta_shell_get_interface (
							anjuta_plugin_get_shell (ANJUTA_PLUGIN (parser_plugin)),
				    		IAnjutaSymbolManager, NULL),
		            parser_plugin->settings);
		
		//TODO: only load assist, if it's available for c/c++
		if (!assist)
			return;
	
		parser_plugin->assist = assist;
	}
	else
		return;
*/
	parser_plugin->support_installed = TRUE;
}

static void
uninstall_support (ParserEnginePlugin *parser_plugin)
{
    if (!parser_plugin->support_installed)
        return;
	
    parser_plugin->support_installed = FALSE;
}

static void
on_editor_language_changed (IAnjutaEditor *editor,
                            const gchar *new_language,
                            ParserEnginePlugin *plugin)
{
    uninstall_support (plugin);
    install_support (plugin);
}

static void
on_value_added_current_editor (AnjutaPlugin *plugin, const gchar *name,
                               const GValue *value, gpointer data)
{
    ParserEnginePlugin *parser_plugin;
    IAnjutaDocument* doc = IANJUTA_DOCUMENT(g_value_get_object (value));
    parser_plugin = ANJUTA_PLUGIN_PARSER_ENGINE (plugin);
	
    if (IANJUTA_IS_EDITOR(doc))
        parser_plugin->current_editor = G_OBJECT(doc);
    else
    {
        parser_plugin->current_editor = NULL;
        return;
    }
	
    if (IANJUTA_IS_EDITOR(parser_plugin->current_editor))
        install_support (parser_plugin);
	
    g_signal_connect (parser_plugin->current_editor, "language-changed",
                      G_CALLBACK (on_editor_language_changed),
                      plugin);
}

static void
on_value_removed_current_editor (AnjutaPlugin *plugin, const gchar *name,
                                 gpointer data)
{
    ParserEnginePlugin *parser_plugin;
    parser_plugin = ANJUTA_PLUGIN_PARSER_ENGINE (plugin);
	
    if (parser_plugin->current_editor)
        g_signal_handlers_disconnect_by_func (parser_plugin->current_editor,
                                          G_CALLBACK (on_editor_language_changed),
                                          plugin);
	
    if (IANJUTA_IS_EDITOR(parser_plugin->current_editor))
        uninstall_support (parser_plugin);
	
    parser_plugin->current_editor = NULL;
}

static gboolean
parser_engine_plugin_activate_plugin (AnjutaPlugin *plugin)
{
    ParserEnginePlugin *parser_plugin;

    parser_plugin = ANJUTA_PLUGIN_PARSER_ENGINE (plugin);

    DEBUG_PRINT ("%s", "AnjutaParserEnginePlugin: Activating plugin ...");
	
    parser_plugin->editor_watch_id =
        anjuta_plugin_add_watch (plugin,
                                 IANJUTA_DOCUMENT_MANAGER_CURRENT_DOCUMENT,
                                 on_value_added_current_editor,
                                 on_value_removed_current_editor,
                                 plugin);

    return TRUE;
}

static gboolean
parser_engine_plugin_deactivate_plugin (AnjutaPlugin *plugin)
{
    ParserEnginePlugin *parser_plugin;
    parser_plugin = ANJUTA_PLUGIN_PARSER_ENGINE (plugin);

    anjuta_plugin_remove_watch (plugin,
                                parser_plugin->editor_watch_id,
                                TRUE);

    DEBUG_PRINT ("%s", "AnjutaParserEnginePlugin: Deactivated plugin.");
    return TRUE;
}

static void
parser_engine_plugin_finalize (GObject *obj)
{
    /* Finalization codes here */
    G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
parser_engine_plugin_dispose (GObject *obj)
{
	/* Disposition codes */
    G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
parser_engine_plugin_instance_init (GObject *obj)
{
    ParserEnginePlugin *plugin = ANJUTA_PLUGIN_PARSER_ENGINE (obj);
    plugin->current_editor = NULL;
    plugin->current_language = NULL;
	plugin->editor_watch_id = 0;
}

static void
parser_engine_plugin_class_init (GObjectClass *klass)
{
    AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    plugin_class->activate = parser_engine_plugin_activate_plugin;
    plugin_class->deactivate = parser_engine_plugin_deactivate_plugin;
    klass->finalize = parser_engine_plugin_finalize;
    klass->dispose = parser_engine_plugin_dispose;
}

/* support methods */

/**
 * parser_engine_is_character:
 * @ch: character to check
 * @context_characters: language-specific context characters
 *                      the end is marked with a '0' character
 *
 * Returns: if the current character seperates a scope
 */
static gboolean
parser_engine_is_character (gchar ch,
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

#define SCOPE_BRACE_JUMP_LIMIT 50

/**
 * parser_engine_get_scope_context
 * @editor: current editor
 * @iter: Current cursor position
 * @scope_context_characters: language-specific context characters
 *                            the end is marked with a '0' character
 *
 * Find the scope context for calltips
 */
static gchar*
parser_engine_get_scope_context (IAnjutaEditor* editor,
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
		if (parser_engine_is_character (ch, scope_context_characters))
		{
			scope_chars_found = TRUE;
		}
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
 * ianjuta_parser_assist_proposal_new:
 * @symbol: IAnjutaSymbol to create the proposal for
 *
 * Creates a new IAnjutaEditorAssistProposal for symbol
 *
 * Returns: a newly allocated IAnjutaEditorAssistProposal
 */
static IAnjutaEditorAssistProposal*
parser_engine_proposal_new (IAnjutaSymbol* symbol)
{
	IAnjutaEditorAssistProposal* proposal = g_new0 (IAnjutaEditorAssistProposal, 1);
	IAnjutaSymbolType type = ianjuta_symbol_get_sym_type (symbol, NULL);
	IAnjutaParserProposalData* data = g_new0 (IAnjutaParserProposalData, 1);

	data->name = g_strdup (ianjuta_symbol_get_string (symbol, IANJUTA_SYMBOL_FIELD_NAME, NULL));
	switch (type)
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
	proposal->data = data;
	/* Icons are lifetime object of the symbol-db so we can cast here */
	proposal->icon = (GdkPixbuf*) ianjuta_symbol_get_icon (symbol, NULL);
	return proposal;
}

//TODO: language-specific settings: only used in parser-cxx/assist.c
static GList*
iparser_create_calltips (IAnjutaParser* self,
                         IAnjutaIterable* iter,
                         GList* merge,
                         GError** e)
{
	GList* tips = merge;
	if (iter)
	{
		do
		{
			IAnjutaSymbol* symbol = IANJUTA_SYMBOL (iter);
			const gchar* name = ianjuta_symbol_get_string (symbol, IANJUTA_SYMBOL_FIELD_NAME, NULL);
			if (name != NULL)
			{
				const gchar* args = ianjuta_symbol_get_string (symbol, IANJUTA_SYMBOL_FIELD_SIGNATURE, NULL);
				const gchar* rettype = ianjuta_symbol_get_string (symbol, IANJUTA_SYMBOL_FIELD_RETURNTYPE, NULL);
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
//equal part 1 START
				gchar** argv;
				if (!args)
					args = "()";
				
				argv = g_strsplit (args, ",", -1);
				print_args = g_strjoinv (separator, argv);
				gchar* tip = g_strdup_printf ("%s %s %s", rettype, name, print_args);
				
				if (!g_list_find_custom (tips, tip, (GCompareFunc) strcmp))
					tips = g_list_append (tips, tip);
				
				g_strfreev (argv);
				g_free (print_args);
				g_free (separator);
				g_free (white_name);
//equal part 1 END
			}
			else
				break;
		}
		while (ianjuta_iterable_next (iter, NULL));
	}
	return tips;
}

static GList*
iparser_create_completion_from_symbols (IAnjutaParser* self,
                                        IAnjutaIterable* symbols,
                                        GError** e)
{
	GList* list = NULL;

	if (!symbols)
		return NULL;
	do
	{
		IAnjutaSymbol* symbol = IANJUTA_SYMBOL (symbols);
		IAnjutaEditorAssistProposal* proposal = parser_engine_proposal_new (symbol);	

		list = g_list_append (list, proposal);
	}
	while (ianjuta_iterable_next (symbols, NULL));

	return list;
}

#define BRACE_SEARCH_LIMIT 500

static gchar*
iparser_get_calltip_context (IAnjutaParser* self,
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
	
	context = parser_engine_get_scope_context (IANJUTA_EDITOR (itip),
	                                           iter, scope_context_characters);

	/* Point iter to the first character of the scope to align calltip correctly */
	ianjuta_iterable_next (iter, NULL);
	
	return context;
}

static gchar*
iparser_get_pre_word (IAnjutaParser* self,
                      IAnjutaEditor* editor,
                      IAnjutaIterable *iter,
                      IAnjutaIterable** start_iter,
                      const gchar* word_characters,
                      GError** e)
{
g_warning ("iparser_get_pre_word works");
	IAnjutaIterable *end = ianjuta_iterable_clone (iter, NULL);
	IAnjutaIterable *begin = ianjuta_iterable_clone (iter, NULL);
	gchar ch, *preword_chars = NULL;
	gboolean out_of_range = FALSE;
	gboolean preword_found = FALSE;
	
	/* Cursor points after the current characters, move back */
	ianjuta_iterable_previous (begin, NULL);
	
	ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (begin), 0, NULL);
	
	while (ch && parser_engine_is_character (ch, word_characters))
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
g_warning ("iparser_get_pre_word 1");
		g_object_unref (begin);
g_warning ("iparser_get_pre_word 2");
		*start_iter = NULL;
	}
	
g_warning ("iparser_get_pre_word 3");
	g_object_unref (end);
g_warning ("iparser_get_pre_word 4");
	return preword_chars;
}

static void
iparser_iface_init (IAnjutaParserIface* iface)
{
	iface->create_calltips = iparser_create_calltips;
	iface->create_completion_from_symbols = iparser_create_completion_from_symbols;
	iface->get_calltip_context = iparser_get_calltip_context;
	iface->get_pre_word = iparser_get_pre_word;
}

ANJUTA_PLUGIN_BEGIN (ParserEnginePlugin, parser_engine_plugin);
ANJUTA_PLUGIN_ADD_INTERFACE (iparser, IANJUTA_TYPE_PARSER);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (ParserEnginePlugin, parser_engine_plugin);
