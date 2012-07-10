/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    plugin.h
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

#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#include <libanjuta/anjuta-plugin.h>
#include "provider.h"

extern GType parser_engine_plugin_get_type (GTypeModule *module);
#define ANJUTA_TYPE_PLUGIN_PARSER_ENGINE         (parser_engine_plugin_get_type (NULL))
#define ANJUTA_PLUGIN_PARSER_ENGINE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), ANJUTA_TYPE_PLUGIN_PARSER_ENGINE, ParserEnginePlugin))
#define ANJUTA_PLUGIN_PARSER_ENGINE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), ANJUTA_TYPE_PLUGIN_PARSER_ENGINE, ParserEnginePluginClass))
#define ANJUTA_IS_PLUGIN_PARSER_ENGINE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ANJUTA_TYPE_PLUGIN_PARSER_ENGINE))
#define ANJUTA_IS_PLUGIN_PARSER_ENGINE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), ANJUTA_TYPE_PLUGIN_PARSER_ENGINE))
#define ANJUTA_PLUGIN_PARSER_ENGINE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), ANJUTA_TYPE_PLUGIN_PARSER_ENGINE, ParserEnginePluginClass))

typedef struct _ParserEnginePluginClass ParserEnginePluginClass;
typedef struct _ParserEnginePlugin ParserEnginePlugin;

struct _ParserEnginePlugin {
	AnjutaPlugin parent;

	//TODO: which one is really needed?
	gint editor_watch_id;
	gboolean support_installed;
	GList *support_plugins;
	GObject *current_editor;
	const gchar *current_language;
	
	/* Provider */
	ParserProvider *provider;
};

struct _ParserEnginePluginClass {
	AnjutaPluginClass parent_class;
};

#endif
