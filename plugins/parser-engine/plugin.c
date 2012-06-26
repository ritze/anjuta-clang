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

struct _ParserEnginePluginPriv {
	/* Autocompletion */
	IAnjutaIterable* start_iter;
	IAnjutaEditorTip* itip;
}

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
	//TODO: necessary?
	//assist->priv->iassist = IANJUTA_EDITOR_ASSIST (ieditor);
	ianjuta_editor_assist_add (IANJUTA_EDITOR_ASSIST (parser_plugin->current_editor),
	                           IANJUTA_PROVIDER (parser_plugin), NULL);
	parser_plugin->support_installed = TRUE;
}

static void
uninstall_support (ParserEnginePlugin *parser_plugin)
{
    if (!parser_plugin->support_installed)
        return;
	
	ianjuta_editor_assist_remove (IANJUTA_EDITOR_ASSIST (parser_plugin->current_editor),
	                              IANJUTA_PROVIDER (parser_plugin), NULL);
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
    	parser_plugin->priv->itip = IANJUTA_EDITOR_TIP (parser_plugin->current_editor);
    else
    {
        parser_plugin->current_editor = NULL;
        parser_plugin->priv->itip = NULL;
        return;
    }
	
    if (IANJUTA_IS_EDITOR (parser_plugin->current_editor))
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

//TODO: language-specific setting: only used in parser-cxx
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

static IAnjutaIterable*
iparser_find_next_brace (IAnjutaParser* self,
                         IAnjutaIterable* iter,
                         GError** e)
{
	IAnjutaIterable* current_iter = ianjuta_iterable_clone (iter, NULL);
	gchar ch;
	do
	{
		ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (current_iter), 0, NULL);
		if (ch == '(')
			return current_iter;
	}
	while (g_ascii_isspace (ch) && ianjuta_iterable_next (current_iter, NULL));
	
	g_object_unref (current_iter);
	return NULL;
}

static gboolean
iparser_find_whitespace (IAnjutaParser* self,
                         IAnjutaIterable* iter,
                         GError** e)
{
	gchar ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (iter), 0, NULL);
	if (g_ascii_isspace (ch) && ch != '\n' && iparser_find_next_brace(self, iter, NULL))
		return TRUE;
	else
		return FALSE;
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
		g_object_unref (begin);
		*start_iter = NULL;
	}
	
	g_object_unref (end);
	return preword_chars;
}

static void
iparser_set_start_iter (IAnjutaParser* self,
						IAnjutaIterable* start_iter,
						GError** e)
{
	ParserEnginePlugin *parser_plugin = ANJUTA_PLUGIN_PARSER_ENGINE (self);
	parser_plugin->priv->start_iter = ianjuta_iterable_clone (start_iter, NULL);
g_warning ("iparser_set_start_iter: %s", parser_plugin->priv->start_iter);
}

static void
iparser_iface_init (IAnjutaParserIface* iface)
{
	iface->create_calltips = iparser_create_calltips;
	iface->create_completion_from_symbols = iparser_create_completion_from_symbols;
	iface->find_next_brace = iparser_find_next_brace;
	iface->find_whitespace = iparser_find_whitespace;
	iface->get_calltip_context = iparser_get_calltip_context;
	iface->get_pre_word = iparser_get_pre_word;
	iface->set_start_iter = iparser_set_start_iter;
}

/**
 * iprovider_activate:
 * @self: IAnjutaProvider object
 * @iter: cursor position when proposal was activated
 * @data: Data assigned to the completion object
 * @e: Error population
 *
 * Called from the provider when the user activated a proposal
 */
static void
iprovider_activate (IAnjutaProvider* self,
                    IAnjutaIterable* iter,
                    gpointer data,
                    GError** e)
{
	ParserEnginePlugin *parser = ANJUTA_PLUGIN_PARSER_ENGINE (self);
	IAnjutaParserProposalData *prop_data;
	GString *assistance;
	IAnjutaEditor *te;
	gboolean add_space_after_func = FALSE;
	gboolean add_brace_after_func = FALSE;
	gboolean add_closebrace_after_func = FALSE;
	
	g_return_if_fail (data != NULL);
	prop_data = data;
	assistance = g_string_new (prop_data->name);
	
	//TODO: adapt JS and Vala
	if (prop_data->is_func)
	{
		IAnjutaIterable* next_brace = iparser_find_next_brace (parser, iter, NULL);
		//TODO:
		add_space_after_func =
			g_settings_get_boolean (assist->priv->settings,
			                        PREF_AUTOCOMPLETE_SPACE_AFTER_FUNC);
		add_brace_after_func =
			g_settings_get_boolean (assist->priv->settings,
			                        PREF_AUTOCOMPLETE_BRACE_AFTER_FUNC);
		add_closebrace_after_func =
			g_settings_get_boolean (assist->priv->settings,
			                        PREF_AUTOCOMPLETE_CLOSEBRACE_AFTER_FUNC);

		if (add_space_after_func
			&& !iparser_find_whitespace (parser, iter, NULL))
			g_string_append (assistance, " ");
		if (add_brace_after_func && !next_brace)
			g_string_append (assistance, "(");
		else
			g_object_unref (next_brace);
	}
	
	te = IANJUTA_EDITOR (parser_plugin->current_editor);
		
	ianjuta_document_begin_undo_action (IANJUTA_DOCUMENT (te), NULL);
	
	if (ianjuta_iterable_compare (iter, parser->priv->start_iter, NULL) != 0)
	{
		ianjuta_editor_selection_set (IANJUTA_EDITOR_SELECTION (te),
									  parser->priv->start_iter, iter, FALSE, NULL);
		ianjuta_editor_selection_replace (IANJUTA_EDITOR_SELECTION (te),
										  assistance->str, -1, NULL);
	}
	else
	{
		ianjuta_editor_insert (te, iter, assistance->str, -1, NULL);
	}
	
	if (add_brace_after_func && add_closebrace_after_func)
	{
		IAnjutaIterable *next_brace;
		IAnjutaIterable *pos = ianjuta_iterable_clone (iter, NULL);

		ianjuta_iterable_set_position (pos,
									   ianjuta_iterable_get_position (assist->priv->start_iter, NULL)
									   + strlen (assistance->str),
									   NULL);
		next_brace = iparser_find_next_brace (parser, pos, NULL);
		if (!next_brace)
			ianjuta_editor_insert (te, pos, ")", -1, NULL);
		else
		{
			pos = next_brace;
			ianjuta_iterable_next (pos, NULL);
		}
		
		ianjuta_editor_goto_position (te, pos, NULL);

		ianjuta_iterable_previous (pos, NULL);
		if (!prop_data->has_para)
		{
			pos = ianjuta_editor_get_position (te, NULL);
			ianjuta_iterable_next (pos, NULL);
			ianjuta_editor_goto_position (te, pos, NULL);
		}
		
		g_object_unref (pos);
	}

	ianjuta_document_end_undo_action (IANJUTA_DOCUMENT (te), NULL);

	/* Show calltip if we completed function */
	if (add_brace_after_func)
	{
		/* Check for calltip */
		if (parser->priv->itip && 
			//TODO:
		    g_settings_get_boolean (assist->priv->settings,
		                            PREF_CALLTIP_ENABLE))	
		    //TODO: adapt
		    //CXX-Parser: parser_cxx_assist_calltip (ParserCxxAssist *assist)
		    //Python: python_assist_calltip (PythonAssist *assist)
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
			LANG_assist_calltip (assist);

	}
	g_string_free (assistance, TRUE);
}

/**
 * iprovider_get_start_iter:
 * @self: IAnjutaProvider object
 * @e: Error population
 *
 * Returns: the iter where the autocompletion starts
 */
static IAnjutaIterable*
iprovider_get_start_iter (IAnjutaProvider* self,
                          GError** e)
{ 
	ParserEnginePlugin *parser_plugin = ANJUTA_PLUGIN_PARSER_ENGINE (self);
g_warning ("iprovider_get_start_iter: %s", parser_plugin->priv->start_iter);
	return parser_plugin->priv->start_iter;
}

/**
 * iprovider_get_name:
 * @self: IAnjutaProvider object
 * @e: Error population
 *
 * Returns: the provider name
 */
static const gchar*
iprovider_get_name (IAnjutaProvider* self,
                    GError** e)
{
	ParserEnginePlugin *parser_plugin = ANJUTA_PLUGIN_PARSER_ENGINE (self);
g_warning ("iprovider_get_name: %s", parser_plugin->current_language);
	return parser_plugin->current_language;
}

//TODO: works this?
static void python_assist_iface_init(IAnjutaProviderIface* iface)
{
	iface->activate = iprovider_activate;
	iface->get_name = iprovider_get_name;
	iface->get_start_iter = iprovider_get_start_iter;
	iface->populate = iprovider_populate;
}

ANJUTA_PLUGIN_BEGIN (ParserEnginePlugin, parser_engine_plugin);
ANJUTA_PLUGIN_ADD_INTERFACE (iparser, IANJUTA_TYPE_PARSER);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (ParserEnginePlugin, parser_engine_plugin);
