/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * provider.c
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
#include <libanjuta/interfaces/ianjuta-calltip-provider.h>
#include <libanjuta/interfaces/ianjuta-document.h>
#include <libanjuta/interfaces/ianjuta-editor-assist.h>
#include <libanjuta/interfaces/ianjuta-editor-cell.h>
#include <libanjuta/interfaces/ianjuta-editor-selection.h>
#include <libanjuta/interfaces/ianjuta-parser.h>
#include "provider.h"

static void iprovider_iface_init (IAnjutaProviderIface* iface);

G_DEFINE_TYPE_WITH_CODE (ParserProvider,
                         parser_provider,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IANJUTA_TYPE_PROVIDER,
                                                iprovider_iface_init));

struct _ParserProviderPriv {
	IAnjutaEditorAssist* iassist;
	IAnjutaCalltipProvider* calltip_provider;
	IAnjutaEditorTip* itip;
	const gchar* current_language;
	
	/* Autocompletion */
	IAnjutaIterable* start_iter;
	
	/* Calltips */
	gchar* calltip_context;
	GList* tips;
	IAnjutaIterable* calltip_iter;
};

/* support methods */

static void
parser_provider_clear_calltip_context (ParserProvider* provider)
{
	g_free (provider->priv->calltip_context);
	provider->priv->calltip_context = NULL;
	
	g_list_foreach (provider->priv->tips, (GFunc) g_free, NULL);
	g_list_free (provider->priv->tips);
	provider->priv->tips = NULL;

	if (provider->priv->calltip_iter)
		g_object_unref (provider->priv->calltip_iter);
	provider->priv->calltip_iter = NULL;
}

/**
 * parser_provider_create_calltip_context:
 * @assist: self
 * @call_context: The context (method/function name)
 * @position: iter where to show calltips
 *
 * Create the calltip context
 */
static void
parser_provider_create_calltip_context (ParserProvider* provider,
                                        const gchar* call_context,
                                        IAnjutaIterable* position)
{
g_warning ("parser_provider_create_calltip_context");
	provider->priv->calltip_context = g_strdup (call_context);
	provider->priv->calltip_iter = position;
}

/**
 * parser_provider_calltip:
 * @parser: self
 * 
 * Creates a calltip if there is something to show a tip for
 * Calltips are queried async
 *
 * Returns: TRUE if a calltips was queried, FALSE otherwise
 */
static gboolean
parser_provider_calltip (ParserProvider* provider)
{
g_warning ("parser_provider_calltip");
	IAnjutaEditor *editor;
	IAnjutaIterable *iter;
	gchar *call_context;
	
	editor = IANJUTA_EDITOR (provider->priv->itip);
	
	iter = ianjuta_editor_get_position (editor, NULL);
	ianjuta_iterable_previous (iter, NULL);
	
	call_context = ianjuta_calltip_provider_get_context (provider->priv->calltip_provider,
	                                                   iter,
	                                                   NULL);
	if (call_context)
	{
		DEBUG_PRINT ("Searching calltip for: %s", call_context);
		if (provider->priv->calltip_context
		    && g_str_equal (call_context, provider->priv->calltip_context))
		{
			/* Continue tip */
			if (provider->priv->tips)
			{
				if (!ianjuta_editor_tip_visible (IANJUTA_EDITOR_TIP (editor), NULL))
				{
					ianjuta_editor_tip_show (IANJUTA_EDITOR_TIP (editor),
					                         provider->priv->tips,
					                         provider->priv->calltip_iter, NULL);
				}
			}
		}
		else
		{
			/* New tip */
			if (ianjuta_editor_tip_visible (IANJUTA_EDITOR_TIP (editor), NULL))
				ianjuta_editor_tip_cancel (IANJUTA_EDITOR_TIP (editor), NULL);
				
			ianjuta_calltip_provider_clear_context (provider->priv->calltip_provider, NULL);
			parser_provider_clear_calltip_context (provider);
			parser_provider_create_calltip_context (provider, call_context, iter);
			ianjuta_calltip_provider_query (provider->priv->calltip_provider, call_context, NULL);
		}
		
		g_free (call_context);
		return TRUE;
	}
	else
	{
		if (ianjuta_editor_tip_visible (IANJUTA_EDITOR_TIP (editor), NULL))
			ianjuta_editor_tip_cancel (IANJUTA_EDITOR_TIP (editor), NULL);
		ianjuta_calltip_provider_clear_context (provider->priv->calltip_provider, NULL);
		
		g_object_unref (iter);
		return FALSE;
	}
}

/**
 * parser_provider_find_next_brace:
 * @iter: Iter to start searching at
 *
 * Returns: The position of the brace, if the next non-whitespace character is a opening brace,
 * NULL otherwise
 */
static IAnjutaIterable*
parser_provider_find_next_brace (IAnjutaIterable* iter)
{
g_warning ("parser_provider_find_next_brace");
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

/**
 * parser_provider_find_whitespace:
 * @iter: Iter to start searching at
 *
 * Returns: TRUE if the next character is a whitespace character,
 * FALSE otherwise
 */
static gboolean
parser_provider_find_whitespace (IAnjutaIterable* iter)
{
g_warning ("parser_provider_find_whitespace");
	gchar ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (iter), 0, NULL);
	if (g_ascii_isspace (ch) && ch != '\n' && parser_provider_find_next_brace (iter))
		return TRUE;
	else
		return FALSE;
}


/**
 * parser_provider_none:
 * @self: IAnjutaProvider object
 * @parser: ParserProvider object
 *
 * Indicate that there is nothing to autocomplete
 */
static void
parser_provider_none (IAnjutaProvider* self,
                      ParserProvider* provider)
{
g_warning ("parser_provider_none");
	ianjuta_editor_assist_proposals (provider->priv->iassist, self, NULL, TRUE, NULL);
}

//TODO: delete this?
static void
on_calltip_search_complete (GList *tips, /*TODO,*/ ParserProvider *provider)
{
g_warning ("on_calltip_search_complete");
	provider->priv->tips = g_list_prepend (tips, NULL);
	ianjuta_editor_tip_show (IANJUTA_EDITOR_TIP(provider->priv->itip), provider->priv->tips,
		                     provider->priv->calltip_iter, NULL);
g_warning ("on_calltip_search_complete: works");
}

/**
 * parser_provider_install:
 * @provider: ParserProvider object
 * @ieditor: Editor to install support for
 * @iparser: Parser to install support for
 * @icalltip_provider: CalltipProvider to install support for
 *
 * Returns: Registers provider for editor
 */
static void
parser_provider_install (ParserProvider *provider,
                         IAnjutaEditor *ieditor,
                         IAnjutaParser *iparser,
                         IAnjutaCalltipProvider *icalltip_provider)
{
g_warning ("parser_provider_install");
	g_return_if_fail (provider->priv->iassist == NULL);

	if (IANJUTA_IS_EDITOR_ASSIST (ieditor))
	{
		provider->priv->iassist = IANJUTA_EDITOR_ASSIST (ieditor);
		ianjuta_editor_assist_add (IANJUTA_EDITOR_ASSIST (ieditor), IANJUTA_PROVIDER (provider), NULL);
	}
	else
		provider->priv->iassist = NULL;

	if (IANJUTA_IS_EDITOR_TIP (ieditor))
		provider->priv->itip = IANJUTA_EDITOR_TIP (ieditor);
	else
		provider->priv->itip = NULL;
		
	if (IANJUTA_IS_CALLTIP_PROVIDER (icalltip_provider))
		provider->priv->calltip_provider = IANJUTA_CALLTIP_PROVIDER (icalltip_provider);
	else
		provider->priv->calltip_provider = NULL;
g_warning ("parser_provider_install: works");
}

/**
 * parser_provider_uninstall:
 * @self: ParserProvider object
 *
 * Returns: Unregisters provider
 */
static void
parser_provider_uninstall (ParserProvider *provider)
{
g_warning ("parser_provider_uninstall");
	g_return_if_fail (provider->priv->iassist != NULL);
	
	ianjuta_editor_assist_remove (provider->priv->iassist, IANJUTA_PROVIDER (provider), NULL);
	provider->priv->iassist = NULL;
g_warning ("parser_provider_uninstall: works");
}

static void
parser_provider_init (ParserProvider *provider)
{
g_warning ("parser_provider_init");
	provider->priv = g_new0 (ParserProviderPriv, 1);
g_warning ("parser_provider_init: works");
}

static void
parser_provider_finalize (GObject *object)
{
g_warning ("parser_provider_finalize");
	ParserProvider *provider = PARSER_PROVIDER (object);
	
	parser_provider_uninstall (provider);
	parser_provider_clear_calltip_context (provider);
	
	g_free (provider->priv);
	G_OBJECT_CLASS (parser_provider_parent_class)->finalize (object);
g_warning ("parser_provider_finalize: works");
}

static void
parser_provider_class_init (ParserProviderClass *klass)
{
g_warning ("parser_provider_class_init");
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = parser_provider_finalize;
g_warning ("parser_provider_class_init: works");
}

ParserProvider *
parser_provider_new (IAnjutaEditor *ieditor,
                     IAnjutaParser *iparser,
                     IAnjutaCalltipProvider *icalltip_provider,
                     const gchar *language)
{
g_warning ("parser_provider_new");
	ParserProvider *provider;
	
	if (!IANJUTA_IS_EDITOR_ASSIST (ieditor) && !IANJUTA_IS_EDITOR_TIP (ieditor))
	{
		/* No assistance is available with the current editor */
		return NULL;
	}
	provider = g_object_new (TYPE_PARSER_PROVIDER, NULL);
	provider->priv->current_language = language;
	
	/* Install support */
	parser_provider_install (provider, ieditor, iparser, icalltip_provider);
	
g_warning ("parser_provider_new: works");
	return provider;
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
g_warning ("iprovider_activate");
	ParserProvider *provider = PARSER_PROVIDER (self);
	IAnjutaCalltipProviderProposalData *prop_data;
	GString *assistance;
	IAnjutaEditor *te;
	gboolean add_space_after_func = FALSE;
	gboolean add_brace_after_func = FALSE;
	gboolean add_closebrace_after_func = FALSE;
	
	g_return_if_fail (data != NULL);
	prop_data = data;
	assistance = g_string_new (prop_data->name);
	
	if (prop_data->is_func)
	{
		IAnjutaIterable* next_brace = parser_provider_find_next_brace (iter);
		add_space_after_func = ianjuta_calltip_provider_get_boolean (
		        provider->priv->calltip_provider,
			    IANJUTA_CALLTIP_PROVIDER_PREF_AUTOCOMPLETE_SPACE_AFTER_FUNC,
			    NULL);
		add_brace_after_func = ianjuta_calltip_provider_get_boolean (
		        provider->priv->calltip_provider,
		        IANJUTA_CALLTIP_PROVIDER_PREF_AUTOCOMPLETE_BRACE_AFTER_FUNC,
		        NULL);
		add_closebrace_after_func = ianjuta_calltip_provider_get_boolean (
		        provider->priv->calltip_provider,
		        IANJUTA_CALLTIP_PROVIDER_PREF_AUTOCOMPLETE_CLOSEBRACE_AFTER_FUNC,
		        NULL);

		if (add_space_after_func
			&& !parser_provider_find_whitespace (iter))
			g_string_append (assistance, " ");
		if (add_brace_after_func && !next_brace)
			g_string_append (assistance, "(");
		else
			g_object_unref (next_brace);
	}
	
	te = IANJUTA_EDITOR (provider->priv->iassist);
		
	ianjuta_document_begin_undo_action (IANJUTA_DOCUMENT (te), NULL);
	
	if (ianjuta_iterable_compare (iter, provider->priv->start_iter, NULL) != 0)
	{
		ianjuta_editor_selection_set (IANJUTA_EDITOR_SELECTION (te),
									  provider->priv->start_iter, iter, FALSE, NULL);
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
									   ianjuta_iterable_get_position (provider->priv->start_iter, NULL)
									   + strlen (assistance->str),
									   NULL);
		next_brace = parser_provider_find_next_brace (pos);
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
		if (provider->priv->itip &&
		    ianjuta_calltip_provider_get_boolean ( provider->priv->calltip_provider,
		            IANJUTA_CALLTIP_PROVIDER_PREF_CALLTIP_ENABLE, NULL))
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
			parser_provider_calltip (provider);

	}
	g_string_free (assistance, TRUE);
}

/**
 * iprovider_populate:
 * @self: IAnjutaProvider object
 * @cursor: Iter at current cursor position (after current character)
 * @e: Error population
 */
static void
iprovider_populate (IAnjutaProvider* self, IAnjutaIterable* cursor, GError** e)
{
g_warning ("iprovider_populate");
	ParserProvider *provider = PARSER_PROVIDER (self);
	IAnjutaIterable *start_iter;
	
	/* Check if we actually want autocompletion at all */
	if (!ianjuta_calltip_provider_get_boolean (provider->priv->calltip_provider,
	             IANJUTA_CALLTIP_PROVIDER_PREF_AUTOCOMPLETE_ENABLE, NULL))
	{
		parser_provider_none (self, provider);
		return;
	}
	
	/* Check if this is a valid text region for completion */
	IAnjutaEditorAttribute attrib = ianjuta_editor_cell_get_attribute (IANJUTA_EDITOR_CELL(cursor),
	                                                                   NULL);
	if (attrib == IANJUTA_EDITOR_COMMENT /*|| attrib == IANJUTA_EDITOR_STRING*/)
	{
		parser_provider_none (self, provider);
		return;
	}

	/* Check for calltip */
	if (provider->priv->itip && 
	    ianjuta_calltip_provider_get_boolean (provider->priv->calltip_provider,
	            IANJUTA_CALLTIP_PROVIDER_PREF_CALLTIP_ENABLE, NULL))
	{	
		parser_provider_calltip (provider);
	}
	
	/* Execute language-specific part */
	start_iter = ianjuta_calltip_provider_populate (provider->priv->calltip_provider,
	                                                cursor, NULL);
	if (start_iter)
	{
		if (provider->priv->start_iter)
			g_object_unref (provider->priv->start_iter);
		provider->priv->start_iter = start_iter;
		return;
	}

	/* Nothing to propose */
	if (provider->priv->start_iter)
	{
		g_object_unref (provider->priv->start_iter);
		provider->priv->start_iter = NULL;
	}
	parser_provider_none (self, provider);
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
g_warning ("iprovider_get_start_iter");
	ParserProvider *provider = PARSER_PROVIDER (self);
g_warning ("iprovider_get_start_iter: works");
	return provider->priv->start_iter;
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
g_warning ("iprovider_get_name");
	ParserProvider *provider = PARSER_PROVIDER (self);
g_warning ("iprovider_get_name: works");
	return provider->priv->current_language;
}

//TODO: Do all functions work?
static void iprovider_iface_init (IAnjutaProviderIface* iface)
{
	iface->activate = iprovider_activate;
	iface->get_name = iprovider_get_name;
	iface->get_start_iter = iprovider_get_start_iter;
	//TODO: integrate in vala and js
	iface->populate = iprovider_populate;
}
