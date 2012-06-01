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
#include <libanjuta/interfaces/ianjuta-parser.h>

#include "plugin.h"
#include "engine-parser.h"

#define PARSER_CXX_PLUGIN(o) (ParserCxxPlugin*) (o)

static gpointer parent_class;

static gboolean
parser_cxx_plugin_activate_plugin (AnjutaPlugin *plugin)
{
    DEBUG_PRINT ("%s", "AnjutaParserCxxPlugin: Activating plugin ...");
    return TRUE;
}

static gboolean
parser_cxx_plugin_deactivate_plugin (AnjutaPlugin *plugin)
{
	DEBUG_PRINT ("%s", "AnjutaParserCxxPlugin: Deactivated plugin.");
    return TRUE;
}

static void
parser_cxx_plugin_finalize (GObject *obj)
{
    /* Finalization codes here */
    G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
parser_cxx_plugin_dispose (GObject *obj)
{
    /* Disposition codes */
    G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
parser_cxx_plugin_instance_init (GObject *obj)
{
    ParserCxxPlugin *plugin = ANJUTA_PLUGIN_PARSER_CXX (obj);
    plugin->current_language = NULL;
}

static void
parser_cxx_plugin_class_init (GObjectClass *klass)
{
    AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);

    plugin_class->activate = parser_cxx_plugin_activate_plugin;
    plugin_class->deactivate = parser_cxx_plugin_deactivate_plugin;
    klass->finalize = parser_cxx_plugin_finalize;
    klass->dispose = parser_cxx_plugin_dispose;
}

static void
iparser_init (IAnjutaParser *iparser, IAnjutaSymbolManager * manager, GError **err)
{
	engine_parser_init (manager);
}

static void
iparser_deinit (IAnjutaParser *iparser, GError **err)
{
	engine_parser_deinit ();
}

static IAnjutaIterable*
iparser_process_expression (IAnjutaParser *iparser, const gchar *stmt, const gchar * above_text, 
                              const gchar * full_file_path, gulong linenum, GError **err)
{
	return engine_parser_process_expression (stmt, above_text, full_file_path, linenum);
}

static void
iparser_iface_init (IAnjutaParserIface* iface)
{
	iface->init = iparser_init;
	iface->deinit = iparser_deinit;
	iface->process_expression = iparser_process_expression;
}

ANJUTA_PLUGIN_BEGIN (ParserCxxPlugin, parser_cxx_plugin);
ANJUTA_PLUGIN_ADD_INTERFACE (iparser, IANJUTA_TYPE_PARSER);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (ParserCxxPlugin, parser_cxx_plugin);
