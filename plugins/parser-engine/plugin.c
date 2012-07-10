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
#include <libanjuta/interfaces/ianjuta-calltip-provider.h>
#include <libanjuta/interfaces/ianjuta-document.h>
#include <libanjuta/interfaces/ianjuta-document-manager.h>
#include <libanjuta/interfaces/ianjuta-language.h>

#include "plugin.h"

#define ICON_FILE "anjuta-parser-engine-plugin.png"

static gpointer parent_class;

static GList*
load_new_support_plugins (ParserEnginePlugin* parser_plugin, GList* new_plugin_ids,
						  AnjutaPluginManager* plugin_manager)
{
	GList* node;
	GList* needed_plugins = NULL;
	for (node = new_plugin_ids; node != NULL; node = g_list_next (node))
	{
		gchar* plugin_id = node->data;
		GObject* new_plugin = anjuta_plugin_manager_get_plugin_by_id (plugin_manager,
																	  plugin_id);
		GList* item = g_list_find (parser_plugin->support_plugins, new_plugin);
		if (!item)
			DEBUG_PRINT ("Loading plugin: %s", plugin_id);
		needed_plugins = g_list_append (needed_plugins, new_plugin);
	}
	return needed_plugins;
}

static void
unload_unused_support_plugins (ParserEnginePlugin* parser_plugin,
							   GList* needed_plugins)
{
	GList* plugins = g_list_copy (parser_plugin->support_plugins);
	GList* node;
	DEBUG_PRINT ("Unloading plugins");
	for (node = plugins; node != NULL; node = g_list_next (node))
	{
		AnjutaPlugin* plugin = ANJUTA_PLUGIN (node->data);
		DEBUG_PRINT ("Checking plugin: %p", plugin);
		if (g_list_find (needed_plugins, plugin) == NULL)
		{
			DEBUG_PRINT ("%s", "Unloading plugin");
			anjuta_plugin_deactivate (plugin);
		}
	}
	g_list_free (plugins);
}

/* Enable/Disable language-support */
static void
install_support (ParserEnginePlugin *parser_plugin)
{
    IAnjutaLanguage* lang_manager = anjuta_shell_get_interface (
                    anjuta_plugin_get_shell (ANJUTA_PLUGIN (parser_plugin)),
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

	if (parser_plugin->current_language &&
	    (g_str_equal (parser_plugin->current_language, "C" ) ||
	     g_str_equal (parser_plugin->current_language, "C++") ||
	     g_str_equal (parser_plugin->current_language, "Vala") ||
	     g_str_equal (parser_plugin->current_language, "Python") ||
	     g_str_equal (parser_plugin->current_language, "JavaScript")))
	{
		/* Load current language editor support plugins */
		AnjutaPluginManager *plugin_manager;
		GList *new_support_plugins, *support_plugin_descs, *needed_plugins, *node;
		
		plugin_manager = anjuta_shell_get_plugin_manager (
		                     anjuta_plugin_get_shell (ANJUTA_PLUGIN (parser_plugin)), NULL);
		support_plugin_descs = anjuta_plugin_manager_query (plugin_manager,
		                                                    "Anjuta Plugin",
		                                                    "Interfaces",
		                                                    "IAnjutaCalltipProvider",
		                                                    "Language Support",
		                                                    "Languages",
		                                                    parser_plugin->current_language,
		                                                    NULL);
		
		//TODO: edit for parser-clang
		new_support_plugins = NULL;
		for (node = support_plugin_descs; node != NULL; node = g_list_next (node))
		{
			gchar *plugin_id;

			AnjutaPluginDescription *desc = node->data;

			anjuta_plugin_description_get_string (desc, "Anjuta Plugin", "Location",
			                                      &plugin_id);

			new_support_plugins = g_list_append (new_support_plugins, plugin_id);
		}
		g_list_free (support_plugin_descs);

		/* Load new plugins */
		needed_plugins =
			load_new_support_plugins (parser_plugin, new_support_plugins,
			                          plugin_manager);

		/* Unload unused plugins */
		unload_unused_support_plugins (parser_plugin, needed_plugins);

		/* Update list */
		g_list_free (parser_plugin->support_plugins);
		parser_plugin->support_plugins = needed_plugins;

		anjuta_util_glist_strings_free (new_support_plugins);
		
		
		ParserProvider *provider;
		
		g_assert (parser_plugin->provider == NULL);
		
		provider = parser_provider_new (IANJUTA_EDITOR (parser_plugin->current_editor),
					    anjuta_shell_get_interface (
				                   anjuta_plugin_get_shell (ANJUTA_PLUGIN (parser_plugin)),
				                   IAnjutaCalltipProvider, NULL),
                        parser_plugin->current_language);
		
		if (!provider)
			return;
	
		parser_plugin->provider = provider;
	}
	else
		return;
	                           
	parser_plugin->support_installed = TRUE;
}

static void
uninstall_support (ParserEnginePlugin *parser_plugin)
{
    if (!parser_plugin->support_installed)
        return;
        
	if (parser_plugin->provider)
    {
        g_object_unref (parser_plugin->provider);
        parser_plugin->provider = NULL;
    }
    
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
    IAnjutaDocument* doc;
    parser_plugin = ANJUTA_PLUGIN_PARSER_ENGINE (plugin);
    doc = IANJUTA_DOCUMENT (g_value_get_object (value));
	
    if (IANJUTA_IS_EDITOR(doc))
        parser_plugin->current_editor = G_OBJECT(doc);
    else
    {
        parser_plugin->current_editor = NULL;
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
    plugin->provider = NULL;
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

static void
iprovider_assist_iface_init (IAnjutaProviderAssistIface* iface)
{
	iface->proposals = iprovider_assist_proposals;
}

ANJUTA_PLUGIN_BEGIN (ParserEnginePlugin, parser_engine_plugin);
ANJUTA_PLUGIN_ADD_INTERFACE (iprovider_assist, IANJUTA_TYPE_PROVIDER_ASSIST);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (ParserEnginePlugin, parser_engine_plugin);
