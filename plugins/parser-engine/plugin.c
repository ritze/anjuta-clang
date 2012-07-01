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
		ParserProvider *provider;
	
		g_assert (parser_plugin->provider == NULL);
		
//Debug code
		IAnjutaCalltipProvider *calltip_provider = anjuta_shell_get_interface (
				                   anjuta_plugin_get_shell (ANJUTA_PLUGIN (parser_plugin)),
				                   IAnjutaCalltipProvider, NULL);
				                   
		if (calltip_provider && IANJUTA_IS_CALLTIP_PROVIDER (calltip_provider))
			g_warning("IAnjutaCalltipProvider: set!");
		else
			g_warning("IAnjutaCalltipProvider: not set!");
//End of debug code

		provider = parser_provider_new (IANJUTA_EDITOR (parser_plugin->current_editor),
					    calltip_provider,
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

ANJUTA_PLUGIN_BEGIN (ParserEnginePlugin, parser_engine_plugin);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (ParserEnginePlugin, parser_engine_plugin);
