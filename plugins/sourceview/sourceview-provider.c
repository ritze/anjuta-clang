/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) Johannes Schmid 2009 <jhs@gnome.org>
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

#include "sourceview-provider.h"
#include "sourceview-cell.h"
#include "sourceview-private.h"
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/interfaces/ianjuta-document.h>
#include <libanjuta/interfaces/ianjuta-editor-assist.h>
#include <libanjuta/interfaces/ianjuta-editor-cell.h>
#include <libanjuta/interfaces/ianjuta-editor-selection.h>
#include <libanjuta/interfaces/ianjuta-editor-tip.h>

static void
sourceview_provider_iface_init (GtkSourceCompletionProviderIface* provider);

G_DEFINE_TYPE_WITH_CODE (SourceviewProvider,
			 sourceview_provider,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
			                        sourceview_provider_iface_init))

struct _SourceviewProviderPriv {
	/* Autocompletion */
	IAnjutaIterable* start_iter;
	
	/* Calltips */
	gchar* calltip_context;
	GList* tips;
	IAnjutaIterable* calltip_iter;
};

static void
on_context_cancelled (GtkSourceCompletionContext* context, SourceviewProvider* provider)
{
	g_signal_handlers_disconnect_by_func (context, on_context_cancelled, provider);

	g_signal_emit_by_name (provider->sv, "cancelled");
	
	provider->cancelled = TRUE;
	provider->context = NULL;
}

static void
sourceview_provider_populate (GtkSourceCompletionProvider* provider, GtkSourceCompletionContext* context)
{
	SourceviewProvider* prov = SOURCEVIEW_PROVIDER(provider);
	GtkTextIter iter;
	SourceviewCell* cell;
	gtk_source_completion_context_get_iter(context, &iter);
	cell = sourceview_cell_new (&iter, GTK_TEXT_VIEW(prov->sv->priv->view));
	prov->context = context;
	prov->cancelled = FALSE;
	g_signal_connect (context, "cancelled", G_CALLBACK(on_context_cancelled), prov);
	ianjuta_provider_populate(prov->iprov, IANJUTA_ITERABLE(cell), NULL);
	g_object_unref (cell);
}

static gchar*
sourceview_provider_get_name (GtkSourceCompletionProvider* provider)
{
	SourceviewProvider* prov = SOURCEVIEW_PROVIDER(provider);
	return g_strdup (ianjuta_provider_get_name (prov->iprov, NULL));
}


static gboolean
sourceview_provider_get_start_iter (GtkSourceCompletionProvider* provider, 
                                    GtkSourceCompletionContext* context,
                                    GtkSourceCompletionProposal* proposal, GtkTextIter* iter)
{
	SourceviewProvider* prov = SOURCEVIEW_PROVIDER(provider);
	IAnjutaIterable* iiter = ianjuta_provider_get_start_iter (prov->iprov, NULL);
	if (iiter)
	{
		SourceviewCell* cell = SOURCEVIEW_CELL(iiter);
		GtkTextIter source_iter;
		sourceview_cell_get_iter(cell, &source_iter);
		*iter = source_iter;
		return TRUE;
	}
	else
		return FALSE;
}

static gboolean
sourceview_provider_activate_proposal (GtkSourceCompletionProvider* provider,
                                       GtkSourceCompletionProposal* proposal,
                                       GtkTextIter* iter)
{
	SourceviewProvider* prov = SOURCEVIEW_PROVIDER (provider);
	SourceviewCell* cell = sourceview_cell_new (iter, GTK_TEXT_VIEW(prov->sv->priv->view));
	gpointer data = g_object_get_data (G_OBJECT(proposal), "__data");

	ianjuta_provider_activate(prov->iprov, IANJUTA_ITERABLE(cell), data, NULL);
	
	g_object_unref (cell);
	return TRUE;
}

static gint
sourceview_provider_get_priority (GtkSourceCompletionProvider* provider)
{
	/* Always the highest priority */
	return G_MAXINT32;
}

static void
sourceview_provider_iface_init (GtkSourceCompletionProviderIface* provider)
{
	provider->get_name = sourceview_provider_get_name;
	provider->populate = sourceview_provider_populate;
	provider->get_start_iter = sourceview_provider_get_start_iter;
	provider->activate_proposal = sourceview_provider_activate_proposal;
	provider->get_priority = sourceview_provider_get_priority;
}

//------ START --------
static void
sourceview_provider_clear_calltip_context (SourceviewProvider* provider)
{
g_warning ("sourceview_provider_clear_calltip_context");
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
 * sourceview_provider_create_calltip_context:
 * @provider: self
 * @call_context: The context (method/function name)
 * @position: iter where to show calltips
 *
 * Create the calltip context
 */
static void
sourceview_provider_create_calltip_context (SourceviewProvider* provider,
                                            const gchar* call_context,
                                            IAnjutaIterable* position)
{
g_warning ("sourceview_provider_create_calltip_context");
	provider->priv->calltip_context = g_strdup (call_context);
	provider->priv->calltip_iter = position;
}

/**
 * sourceview_provider_calltip:
 * @provider: self
 * 
 * Creates a calltip if there is something to show a tip for
 * Calltips are queried async
 *
 * Returns: TRUE if a calltips was queried, FALSE otherwise
 */
static gboolean
sourceview_provider_calltip (SourceviewProvider* provider)
{
g_warning ("sourceview_provider_calltip");
	IAnjutaEditor *editor;
	IAnjutaEditorTip *tip;
	IAnjutaIterable *iter;
	gchar *call_context;
	
	editor = ianjuta_language_provider_get_editor (
	                                 provider->iprov, NULL);
	tip = IANJUTA_EDITOR_TIP (editor);
	
	iter = ianjuta_editor_get_position (editor, NULL);
	ianjuta_iterable_previous (iter, NULL);
	
	call_context = ianjuta_language_provider_get_context (
	                           provider->iprov, iter, NULL);
	if (call_context)
	{
		DEBUG_PRINT ("Searching calltip for: %s", call_context);
		if (provider->priv->calltip_context
		    && g_str_equal (call_context, provider->priv->calltip_context))
		{
			/* Continue tip */
			if (provider->priv->tips)
			{
				if (!ianjuta_editor_tip_visible (tip, NULL))
				{
					ianjuta_editor_tip_show (tip, provider->priv->tips,
					                         provider->priv->calltip_iter,
					                         NULL);
				}
			}
		}
		else
		{
			/* New tip */
			if (ianjuta_editor_tip_visible (tip, NULL))
				ianjuta_editor_tip_cancel (tip, NULL);
				
			ianjuta_language_provider_clear_context (
			                        provider->iprov, NULL);
			sourceview_provider_clear_calltip_context (provider);
			sourceview_provider_create_calltip_context (provider, call_context,
			                                            iter);
			ianjuta_language_provider_query (provider->iprov,
			                                 call_context, NULL);
		}
		
		g_free (call_context);
		return TRUE;
	}
	else
	{
		if (ianjuta_editor_tip_visible (tip, NULL))
			ianjuta_editor_tip_cancel (tip, NULL);
		ianjuta_language_provider_clear_context (provider->iprov, NULL);
		
		g_object_unref (iter);
		return FALSE;
	}
}

/**
 * sourceview_provider_find_next_brace:
 * @iter: Iter to start searching at
 *
 * Returns: The position of the brace, if the next non-whitespace character is a opening brace,
 * NULL otherwise
 */
static IAnjutaIterable*
sourceview_provider_find_next_brace (IAnjutaIterable* iter)
{
g_warning ("sourceview_provider_find_next_brace");
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
 * sourceview_provider_find_whitespace:
 * @iter: Iter to start searching at
 *
 * Returns: TRUE if the next character is a whitespace character,
 * FALSE otherwise
 */
static gboolean
sourceview_provider_find_whitespace (IAnjutaIterable* iter)
{
g_warning ("sourceview_provider_find_whitespace");
	gchar ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (iter), 0, NULL);
	if (g_ascii_isspace (ch) && ch != '\n' && sourceview_provider_find_next_brace (iter))
		return TRUE;
	else
		return FALSE;
}

/**
 * sourceview_provider_none:
 * @provider: self
 * @parser: ParserProvider object
 *
 * Indicate that there is nothing to autocomplete
 */
static void
sourceview_provider_none (SourceviewProvider* provider)
{
g_warning ("sourceview_provider_none");
	IAnjutaEditor *editor = ianjuta_language_provider_get_editor (
	                                provider->iprov, NULL);
	
	if (IANJUTA_IS_EDITOR_ASSIST (editor))
		ianjuta_editor_assist_proposals (IANJUTA_EDITOR_ASSIST (editor),
		                                 iprov, NULL, NULL, TRUE, NULL);
}

static void
sourceview_provider_activate (IAnjutaProvider* iprov,
                              IAnjutaIterable* iter,
                              gpointer data,
                              GError** e)
{
g_warning ("sourceview_provider_activate");
	SourceviewProvider* provider = LANGUAGE_MANAGER (iprov);
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
		IAnjutaIterable* next_brace = sourceview_provider_find_next_brace (iter);
		add_space_after_func = ianjuta_language_provider_get_boolean (
		        provider->iprov,
			    IANJUTA_LANGUAGE_PROVIDER_PREF_AUTOCOMPLETE_SPACE_AFTER_FUNC,
			    NULL);
		add_brace_after_func = ianjuta_language_provider_get_boolean (
		        provider->iprov,
		        IANJUTA_LANGUAGE_PROVIDER_PREF_AUTOCOMPLETE_BRACE_AFTER_FUNC,
		        NULL);
		add_closebrace_after_func = ianjuta_language_provider_get_boolean (
		        provider->iprov,
		        IANJUTA_LANGUAGE_PROVIDER_PREF_AUTOCOMPLETE_CLOSEBRACE_AFTER_FUNC,
		        NULL);

		if (add_space_after_func
			&& !sourceview_provider_find_whitespace (iter))
			g_string_append (assistance, " ");
		if (add_brace_after_func && !next_brace)
			g_string_append (assistance, "(");
		else
			g_object_unref (next_brace);
	}
	
	editor = ianjuta_language_provider_get_editor (provider->iprov, NULL);
		
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
		next_brace = sourceview_provider_find_next_brace (pos);
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
		    ianjuta_language_provider_get_boolean (provider->iprov,
		                          IANJUTA_LANGUAGE_PROVIDER_PREF_CALLTIP_ENABLE,
		                          NULL))
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
			sourceview_provider_calltip (provider);

	}
	g_string_free (assistance, TRUE);
}

//TODO:
static void
sourceview_provider_populate (IAnjutaProvider* iprov, IAnjutaIterable* cursor, GError** e)
{
g_warning ("sourceview_provider_populate");
	SourceviewProvider* provider = SOURCEVIEW_PROVIDER (iprov);
	IAnjutaIterable *start_iter;

	/* Check if we actually want autocompletion at all */
	if (!ianjuta_language_provider_get_boolean (provider->iprov,
	                       IANJUTA_LANGUAGE_PROVIDER_PREF_AUTOCOMPLETE_ENABLE,
	                       NULL))
	{
		sourceview_provider_none (provider);
		return;
	}
	
	/* Check if this is a valid text region for completion */
	IAnjutaEditorAttribute attrib = ianjuta_editor_cell_get_attribute (
	                                        IANJUTA_EDITOR_CELL(cursor), NULL);
	if (attrib == IANJUTA_EDITOR_COMMENT /*TODO: || attrib == IANJUTA_EDITOR_STRING*/)
	{
		sourceview_provider_none (provider);
		return;
	}

	/* Check for calltip */
	IAnjutaEditor *editor = ianjuta_language_provider_get_editor (provider->iprov, NULL);
	if (IANJUTA_IS_EDITOR_TIP (editor) && 
	    ianjuta_language_provider_get_boolean (provider->iprov,
	                              IANJUTA_LANGUAGE_PROVIDER_PREF_CALLTIP_ENABLE,
	                              NULL))
	{	
		sourceview_provider_calltip (provider);
	}
	
	/* Execute language-specific part */
	start_iter = ianjuta_language_provider_populate (provider->iprov, cursor,
	                                                 NULL);
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
	sourceview_provider_none (provider);
}

static IAnjutaIterable*
sourceview_provider_get_start_iter (IAnjutaProvider* iprov, GError** e)
{
g_warning ("iprovider_get_start_iter");
	SourceviewProvider* provider = SOURCEVIEW_PROVIDER (iprov);
g_warning ("iprovider_get_start_iter: works");
	return provider->priv->start_iter;
}

static const gchar*
sourceview_provider_get_name (IAnjutaProvider* iprov, GError** e)
{
	SourceviewProvider* provider = SOURCEVIEW_PROVIDER (iprov);
	return ianjuta_language_provider_get_name (provider->iprov, e);
}
//------  END  --------

static void
sourceview_provider_init (SourceviewProvider *object)
{
	object->priv = g_new0 (SourceviewProviderPriv, 1);
	object->context = NULL;
}

static void
sourceview_provider_finalize (GObject *obj)
{
	SourceviewProvider* prov = OURCEVIEW_PROVIDER (obj);
	sourceview_provider_clear_calltip_context (prov);
	g_free (provider->priv);
	
	/* Finalization codes here */
	//TODO:
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
sourceview_provider_dispose (GObject* obj)
{

}

static void
sourceview_provider_class_init (SourceviewProviderClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	//TODO:
	object_class->finalize = sourceview_provider_finalize;
	object_class->dispose = sourceview_provider_dispose;
}

GtkSourceCompletionProvider* sourceview_provider_new (Sourceview* sv,
                                                      IAnjutaProvider* iprov)
{
	GObject* obj = g_object_new (SOURCEVIEW_TYPE_PROVIDER, NULL);
	SourceviewProvider* prov = SOURCEVIEW_PROVIDER(obj);
	prov->sv = sv;
	prov->iprov = iprov;
	return GTK_SOURCE_COMPLETION_PROVIDER(obj);
}

