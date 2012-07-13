/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * plugin.c
 * Copyright (C) Johannes Schmid 2007 <jhs@gnome.org>
 * 
 * plugin.c is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * plugin.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with plugin.c.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <config.h>
#include <libanjuta/anjuta-shell.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/interfaces/ianjuta-document.h>
#include <libanjuta/interfaces/ianjuta-editor-assist.h>
#include <libanjuta/interfaces/ianjuta-editor-cell.h>
#include <libanjuta/interfaces/ianjuta-editor-selection.h>
#include <libanjuta/interfaces/ianjuta-editor-tip.h>
#include <libanjuta/interfaces/ianjuta-language.h>
#include <libanjuta/interfaces/ianjuta-language-provider.h>
#include <libanjuta/interfaces/ianjuta-provider.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "plugin.h"
#define LANG_FILE PACKAGE_DATA_DIR"/languages.xml"

#define LANGUAGE_MANAGER(o) (LanguageManager*) (o)

struct _LanguageManagerPriv {
	//TODO:
	IAnjutaLanguageProvider* iprov;
	
	/* Autocompletion */
	IAnjutaIterable* start_iter;
	
	/* Calltips */
	gchar* calltip_context;
	GList* tips;
	IAnjutaIterable* calltip_iter;
};

typedef struct _Language Language;

struct _Language
{
	IAnjutaLanguageId id;
	gchar* name;
	gchar* make_target;
	GList* strings;
	GList* mime_types;
};
	

static gpointer parent_class;
static void language_manager_clear_calltip_context (LanguageManager *language_manager);

static void
language_destroy (gpointer data)
{
	Language* lang = (Language*) data;
	
	g_free(lang->name);
	g_list_foreach(lang->strings, (GFunc) g_free, NULL);
	g_list_foreach(lang->mime_types, (GFunc) g_free, NULL);
	
	g_free(lang);
}

static void
load_languages (LanguageManager* language_manager)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr cur_node;
	
	LIBXML_TEST_VERSION
		
	doc = xmlReadFile (LANG_FILE, NULL, 0);
	if (!doc)
	{
		DEBUG_PRINT("Could not read language xml file %s!", LANG_FILE);
		return;
	}
	root = xmlDocGetRootElement (doc);
	
	if (!g_str_equal (root->name, "languages"))
	{
		DEBUG_PRINT ("%s", "Invalid languages.xml file");
	}
	
	for (cur_node = root->children; cur_node != NULL; cur_node = cur_node->next)
	{
		Language* lang = g_new0(Language, 1);
		gchar* id;
		gchar* mime_types;
		gchar* strings;
		
		if (!g_str_equal (cur_node->name, (const xmlChar*) "language"))
			continue;
		
		id = (gchar*) xmlGetProp (cur_node, (const xmlChar*) "id");
		lang->id = atoi(id);
		lang->name = (gchar*) xmlGetProp(cur_node, (const xmlChar*) "name");
		lang->make_target = (gchar*) xmlGetProp (cur_node, (const xmlChar*) "make-target");
		mime_types = (gchar*) xmlGetProp (cur_node, (const xmlChar*) "mime-types");
		strings = (gchar*) xmlGetProp (cur_node, (const xmlChar*) "strings");
		if (lang->id != 0 && lang->name && mime_types && strings)
		{
			GStrv mime_typesv = g_strsplit(mime_types, ",", -1);
			GStrv stringsv = g_strsplit (strings, ",", -1);
			GStrv type;
			GStrv string;
			
			for (type = mime_typesv; *type != NULL; type++)
			{
				lang->mime_types = g_list_append (lang->mime_types, g_strdup(*type));
			}
			g_strfreev(mime_typesv);

			for (string = stringsv; *string != NULL; string++)
			{
				lang->strings = g_list_append (lang->strings, g_strdup(*string));
			}
			g_strfreev(stringsv);
			
			g_hash_table_insert (language_manager->languages, GINT_TO_POINTER(lang->id), lang);
		}
		else
		{
			g_free(lang);
		}
		g_free (id);
		g_free (mime_types);
		g_free (strings);
	}	
	xmlFreeDoc(doc);

	language_manager->priv->iprov = anjuta_shell_get_interface (
					anjuta_plugin_get_shell (ANJUTA_PLUGIN (language_manager)),
					IAnjutaLanguageProvider, NULL);
}

static gboolean
language_manager_activate (AnjutaPlugin *plugin)
{
	LanguageManager *language_manager;
	
	DEBUG_PRINT ("%s", "LanguageManager: Activating LanguageManager plugin ...");
	language_manager = (LanguageManager*) plugin;
	
	load_languages(language_manager);

	return TRUE;
}

static gboolean
language_manager_deactivate (AnjutaPlugin *plugin)
{	
	DEBUG_PRINT ("%s", "LanguageManager: Dectivating LanguageManager plugin ...");
	
	return TRUE;
}

static void
language_manager_finalize (GObject *obj)
{
	LanguageManager *language_manager = LANGUAGE_MANAGER (obj);
	language_manager_clear_calltip_context (language_manager);
	g_free (language_manager->priv);
	
	/* Finalization codes here */
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
language_manager_dispose (GObject *obj)
{
	/* Disposition codes */
	LanguageManager* lang = LANGUAGE_MANAGER (obj);
	
	g_hash_table_unref (lang->languages);
	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
language_manager_instance_init (GObject *obj)
{
	LanguageManager *plugin = (LanguageManager*)obj;
	plugin->priv = g_new0 (LanguageManagerPriv, 1);
	plugin->languages = g_hash_table_new_full (NULL, NULL,
											   NULL, language_destroy);

}

static void
language_manager_class_init (GObjectClass *klass) 
{
	AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	plugin_class->activate = language_manager_activate;
	plugin_class->deactivate = language_manager_deactivate;
	klass->finalize = language_manager_finalize;
	klass->dispose = language_manager_dispose;
}

typedef struct
{
	LanguageManager* lang;
	const gchar* target;
	IAnjutaLanguageId result_id;
	gboolean found;
} LangData;

static void
language_manager_find_mime_type (gpointer key, Language* language, LangData* data)
{
	if (data->found)
		return;
	if (g_list_find_custom (language->mime_types, data->target, (GCompareFunc) strcmp))
	{
		data->result_id = language->id;
		data->found = TRUE;
	}
}

static void
language_manager_find_string (gpointer key, Language* language, LangData* data)
{
	if (data->found)
		return;
	if (g_list_find_custom (language->strings, data->target, (GCompareFunc) g_ascii_strcasecmp))
	{
		DEBUG_PRINT ("Found language %i: %s", language->id, language->name);
		data->result_id = language->id;
		data->found = TRUE;
	}
}

static IAnjutaLanguageId
ilanguage_get_from_mime_type (IAnjutaLanguage* ilang, const gchar* mime_type, GError** e)
{
	if (!mime_type)
		return 0;
	LanguageManager* lang = LANGUAGE_MANAGER(ilang);
	LangData* data = g_new0(LangData, 1);
	IAnjutaLanguageId ret_id;
	data->target = mime_type;
	g_hash_table_foreach (lang->languages, (GHFunc)language_manager_find_mime_type, data);
	if (data->found)
	{
		ret_id = data->result_id;
	}
	else
	{
		DEBUG_PRINT ("Unknown mime-type = %s", mime_type);
		ret_id = 0;
	}
	g_free(data);
	return ret_id;
}

static IAnjutaLanguageId
ilanguage_get_from_string (IAnjutaLanguage* ilang, const gchar* string, GError** e)
{
	if (!string)
		return 0;
	LanguageManager* lang = LANGUAGE_MANAGER(ilang);
	LangData* data = g_new0(LangData, 1);
	IAnjutaLanguageId ret_id;
	data->target = string;
	g_hash_table_foreach (lang->languages, (GHFunc)language_manager_find_string, data);
	if (data->found)
	{
		ret_id = data->result_id;
	}
	else
	{
		DEBUG_PRINT ("Unknown language string = %s", string);
		ret_id = 0;
	}
	g_free(data);
	return ret_id;
}

static const gchar*
ilanguage_get_name (IAnjutaLanguage* ilang, IAnjutaLanguageId id, GError** e)
{
	LanguageManager* lang = LANGUAGE_MANAGER(ilang);
	Language* language = g_hash_table_lookup (lang->languages,
											   GINT_TO_POINTER(id));
		
	if (language)
	{
		return language->name;
		DEBUG_PRINT ("Found language: %s", language->name);
	}
	else
		return NULL;
}

static GList*
ilanguage_get_strings (IAnjutaLanguage* ilang, IAnjutaLanguageId id, GError** e)
{
	LanguageManager* lang = LANGUAGE_MANAGER(ilang);
	Language* language = g_hash_table_lookup (lang->languages,
											   GINT_TO_POINTER(id));
	if (language)
		return language->strings;
	else
		return NULL;	
}

static const char*
ilanguage_get_make_target (IAnjutaLanguage* ilang, IAnjutaLanguageId id, GError** e)
{
	LanguageManager* lang = LANGUAGE_MANAGER(ilang);
	Language* language = g_hash_table_lookup (lang->languages,
	                                          GINT_TO_POINTER(id));
	if (language)
		return language->make_target;
	else
		return NULL;

}

static IAnjutaLanguageId
ilanguage_get_from_editor (IAnjutaLanguage* ilang, IAnjutaEditorLanguage* editor, GError** e)
{	
	const gchar* language = 
			ianjuta_editor_language_get_language (editor, e);
		
	IAnjutaLanguageId id = 
			ilanguage_get_from_string (ilang, language, e);
	
	return id;
}

static const gchar*
ilanguage_get_name_from_editor (IAnjutaLanguage* ilang, IAnjutaEditorLanguage* editor, GError** e)
{
	return ilanguage_get_name (ilang,
							   ilanguage_get_from_editor (ilang, editor, e), e);
}

static GList*
ilanguage_get_languages (IAnjutaLanguage* ilang, GError** e)
{
	LanguageManager* lang = LANGUAGE_MANAGER(ilang);
	return g_hash_table_get_keys (lang->languages);
}

static GList*
ilanguage_get_mime_types(IAnjutaLanguage* ilang, IAnjutaLanguageId id, GError** e)
{
	LanguageManager* lang = LANGUAGE_MANAGER(ilang);
	Language* language = g_hash_table_lookup (lang->languages,
	                                          GINT_TO_POINTER(id));
	if (language)
	{
		return language->mime_types;
	}
	return NULL;
}

static void
ilanguage_iface_init (IAnjutaLanguageIface* iface)
{
	iface->get_from_mime_type = ilanguage_get_from_mime_type;
	iface->get_from_string = ilanguage_get_from_string;
	iface->get_name = ilanguage_get_name;
	iface->get_strings = ilanguage_get_strings;
	iface->get_mime_types = ilanguage_get_mime_types;
	iface->get_make_target = ilanguage_get_make_target;
	iface->get_from_editor = ilanguage_get_from_editor;
	iface->get_name_from_editor = ilanguage_get_name_from_editor;
	iface->get_languages = ilanguage_get_languages;
};

static void
language_manager_clear_calltip_context (LanguageManager *language_manager)
{
g_warning ("language_manager_clear_calltip_context");
	g_free (language_manager->priv->calltip_context);
	language_manager->priv->calltip_context = NULL;
	
	g_list_foreach (language_manager->priv->tips, (GFunc) g_free, NULL);
	g_list_free (language_manager->priv->tips);
	language_manager->priv->tips = NULL;

	if (language_manager->priv->calltip_iter)
		g_object_unref (language_manager->priv->calltip_iter);
	language_manager->priv->calltip_iter = NULL;
}

/**
 * language_manager_create_calltip_context:
 * @language_manager: self
 * @call_context: The context (method/function name)
 * @position: iter where to show calltips
 *
 * Create the calltip context
 */
static void
language_manager_create_calltip_context (LanguageManager *language_manager,
                                         const gchar* call_context,
                                         IAnjutaIterable* position)
{
g_warning ("language_manager_create_calltip_context");
	language_manager->priv->calltip_context = g_strdup (call_context);
	language_manager->priv->calltip_iter = position;
}

/**
 * language_manager_calltip:
 * @language_manager: self
 * 
 * Creates a calltip if there is something to show a tip for
 * Calltips are queried async
 *
 * Returns: TRUE if a calltips was queried, FALSE otherwise
 */
static gboolean
language_manager_calltip (LanguageManager *language_manager)
{
g_warning ("language_manager_calltip");
	IAnjutaEditor *editor;
	IAnjutaEditorTip *tip;
	IAnjutaIterable *iter;
	gchar *call_context;
	
	editor = ianjuta_language_provider_get_editor (
	                                 language_manager->priv->iprov, NULL);
	tip = IANJUTA_EDITOR_TIP (editor);
	
	iter = ianjuta_editor_get_position (editor, NULL);
	ianjuta_iterable_previous (iter, NULL);
	
	call_context = ianjuta_language_provider_get_context (
	                           language_manager->priv->iprov, iter, NULL);
	if (call_context)
	{
		DEBUG_PRINT ("Searching calltip for: %s", call_context);
		if (language_manager->priv->calltip_context
		    && g_str_equal (call_context, language_manager->priv->calltip_context))
		{
			/* Continue tip */
			if (language_manager->priv->tips)
			{
				if (!ianjuta_editor_tip_visible (tip, NULL))
				{
					ianjuta_editor_tip_show (tip, language_manager->priv->tips,
					                         language_manager->priv->calltip_iter,
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
			                        language_manager->priv->iprov, NULL);
			language_manager_clear_calltip_context (language_manager);
			language_manager_create_calltip_context (language_manager,
			                                         call_context, iter);
			ianjuta_language_provider_query (language_manager->priv->iprov,
			                                 call_context, NULL);
		}
		
		g_free (call_context);
		return TRUE;
	}
	else
	{
		if (ianjuta_editor_tip_visible (tip, NULL))
			ianjuta_editor_tip_cancel (tip, NULL);
		ianjuta_language_provider_clear_context (
		                            language_manager->priv->iprov, NULL);
		
		g_object_unref (iter);
		return FALSE;
	}
}

/**
 * language_manager_find_next_brace:
 * @iter: Iter to start searching at
 *
 * Returns: The position of the brace, if the next non-whitespace character is a opening brace,
 * NULL otherwise
 */
static IAnjutaIterable*
language_manager_find_next_brace (IAnjutaIterable* iter)
{
g_warning ("language_manager_find_next_brace");
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
 * language_manager_find_whitespace:
 * @iter: Iter to start searching at
 *
 * Returns: TRUE if the next character is a whitespace character,
 * FALSE otherwise
 */
static gboolean
language_manager_find_whitespace (IAnjutaIterable* iter)
{
g_warning ("language_manager_find_whitespace");
	gchar ch = ianjuta_editor_cell_get_char (IANJUTA_EDITOR_CELL (iter), 0, NULL);
	if (g_ascii_isspace (ch) && ch != '\n' && language_manager_find_next_brace (iter))
		return TRUE;
	else
		return FALSE;
}

/**
 * language_manager_none:
 * @language_manager: self
 * @parser: ParserProvider object
 *
 * Indicate that there is nothing to autocomplete
 */
static void
language_manager_none (LanguageManager *language_manager, IAnjutaProvider *iprov)
{
g_warning ("language_manager_none");
	IAnjutaEditor *editor = ianjuta_language_provider_get_editor (
	                                language_manager->priv->iprov, NULL);
	
	if (IANJUTA_IS_EDITOR_ASSIST (editor))
		ianjuta_editor_assist_proposals (IANJUTA_EDITOR_ASSIST (editor),
		                                 iprov, NULL, NULL, TRUE, NULL);
}

static void
iprovider_activate (IAnjutaProvider* iprov,
                    IAnjutaIterable* iter,
                    gpointer data,
                    GError** e)
{
g_warning ("language_manager_activate");
	LanguageManager *language_manager = LANGUAGE_MANAGER (iprov);
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
		IAnjutaIterable* next_brace = language_manager_find_next_brace (iter);
		add_space_after_func = ianjuta_language_provider_get_boolean (
		        language_manager->priv->iprov,
			    IANJUTA_LANGUAGE_PROVIDER_PREF_AUTOCOMPLETE_SPACE_AFTER_FUNC,
			    NULL);
		add_brace_after_func = ianjuta_language_provider_get_boolean (
		        language_manager->priv->iprov,
		        IANJUTA_LANGUAGE_PROVIDER_PREF_AUTOCOMPLETE_BRACE_AFTER_FUNC,
		        NULL);
		add_closebrace_after_func = ianjuta_language_provider_get_boolean (
		        language_manager->priv->iprov,
		        IANJUTA_LANGUAGE_PROVIDER_PREF_AUTOCOMPLETE_CLOSEBRACE_AFTER_FUNC,
		        NULL);

		if (add_space_after_func
			&& !language_manager_find_whitespace (iter))
			g_string_append (assistance, " ");
		if (add_brace_after_func && !next_brace)
			g_string_append (assistance, "(");
		else
			g_object_unref (next_brace);
	}
	
	editor = ianjuta_language_provider_get_editor (
	                                 language_manager->priv->iprov, NULL);
		
	ianjuta_document_begin_undo_action (IANJUTA_DOCUMENT (editor), NULL);
	
	if (ianjuta_iterable_compare (iter, language_manager->priv->start_iter, NULL) != 0)
	{
		ianjuta_editor_selection_set (IANJUTA_EDITOR_SELECTION (editor),
									  language_manager->priv->start_iter,
									  iter, FALSE, NULL);
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
									       language_manager->priv->start_iter,
									       NULL)
									   + strlen (assistance->str),
									   NULL);
		next_brace = language_manager_find_next_brace (pos);
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
		    ianjuta_language_provider_get_boolean (
		                          language_manager->priv->iprov,
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
			language_manager_calltip (language_manager);

	}
	g_string_free (assistance, TRUE);
}

static void
iprovider_populate (IAnjutaProvider* iprov, IAnjutaIterable* cursor, GError** e)
{
g_warning ("language_manager_populate");
	LanguageManager *language_manager = LANGUAGE_MANAGER (iprov);
	IAnjutaIterable *start_iter;

	/* Check if we actually want autocompletion at all */
	if (!ianjuta_language_provider_get_boolean (language_manager->priv->iprov,
	                           IANJUTA_LANGUAGE_PROVIDER_PREF_AUTOCOMPLETE_ENABLE,
	                           NULL))
	{
		language_manager_none (language_manager, iprov);
		return;
	}
	
	/* Check if this is a valid text region for completion */
	IAnjutaEditorAttribute attrib = ianjuta_editor_cell_get_attribute (
	                                        IANJUTA_EDITOR_CELL(cursor), NULL);
	if (attrib == IANJUTA_EDITOR_COMMENT /*TODO: || attrib == IANJUTA_EDITOR_STRING*/)
	{
		language_manager_none (language_manager, iprov);
		return;
	}

	/* Check for calltip */
	IAnjutaEditor *editor = ianjuta_language_provider_get_editor (language_manager->priv->iprov, NULL);
	if (IANJUTA_IS_EDITOR_TIP (editor) && 
	    ianjuta_language_provider_get_boolean (language_manager->priv->iprov,
	                              IANJUTA_LANGUAGE_PROVIDER_PREF_CALLTIP_ENABLE,
	                              NULL))
	{	
		language_manager_calltip (language_manager);
	}
	
	/* Execute language-specific part */
	start_iter = ianjuta_language_provider_populate (language_manager->priv->iprov,
	                                                 cursor, NULL);
	if (start_iter)
	{
		if (language_manager->priv->start_iter)
			g_object_unref (language_manager->priv->start_iter);
		language_manager->priv->start_iter = start_iter;
		return;
	}

	/* Nothing to propose */
	if (language_manager->priv->start_iter)
	{
		g_object_unref (language_manager->priv->start_iter);
		language_manager->priv->start_iter = NULL;
	}
	language_manager_none (language_manager, iprov);
}

static IAnjutaIterable*
iprovider_get_start_iter (IAnjutaProvider* iprov, GError** e)
{
g_warning ("iprovider_get_start_iter");
	LanguageManager *language_manager = LANGUAGE_MANAGER (iprov);
g_warning ("iprovider_get_start_iter: works");
	return language_manager->priv->start_iter;
}

static const gchar*
iprovider_get_name (IAnjutaProvider* iprov, GError** e)
{
	LanguageManager *language_manager = LANGUAGE_MANAGER (iprov);
	return ianjuta_language_provider_get_name (language_manager->priv->iprov, e);
}

static void
iprovider_iface_init (IAnjutaProviderIface* iface)
{
	iface->populate = iprovider_populate;
	iface->get_start_iter = iprovider_get_start_iter;
	iface->activate = iprovider_activate;
	iface->get_name = iprovider_get_name;
}

ANJUTA_PLUGIN_BEGIN (LanguageManager, language_manager);
ANJUTA_PLUGIN_ADD_INTERFACE (ilanguage, IANJUTA_TYPE_LANGUAGE);
ANJUTA_PLUGIN_ADD_INTERFACE (iprovider, IANJUTA_TYPE_PROVIDER);
ANJUTA_PLUGIN_END

ANJUTA_SIMPLE_PLUGIN (LanguageManager, language_manager);
