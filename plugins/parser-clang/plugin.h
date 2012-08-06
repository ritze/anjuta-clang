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
#include "parser-clang-assist.h"

extern GType parser_clang_plugin_get_type (GTypeModule *module);
#define ANJUTA_TYPE_PLUGIN_PARSER_CLANG         (parser_clang_plugin_get_type (NULL))
#define ANJUTA_PLUGIN_PARSER_CLANG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), ANJUTA_TYPE_PLUGIN_PARSER_CLANG, ParserClangPlugin))
#define ANJUTA_PLUGIN_PARSER_CLANG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), ANJUTA_TYPE_PLUGIN_PARSER_CLANG, ParserClangPluginClass))
#define ANJUTA_IS_PLUGIN_PARSER_CLANG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), ANJUTA_TYPE_PLUGIN_PARSER_CLANG))
#define ANJUTA_IS_PLUGIN_PARSER_CLANG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), ANJUTA_TYPE_PLUGIN_PARSER_CLANG))
#define ANJUTA_PLUGIN_PARSER_CLANG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), ANJUTA_TYPE_PLUGIN_PARSER_CLANG, ParserClangPluginClass))

typedef struct _ParserClangPlugin ParserClangPlugin;
typedef struct _ParserClangPluginClass ParserClangPluginClass;

struct _ParserClangPlugin {
	AnjutaPlugin parent;
	
	GSettings* settings;
	GSettings* editor_settings;
	gboolean support_installed;
	GObject *current_editor;
	const gchar *current_language;
	gchar *project_root_directory;
	
	/* Watches */
	gint project_root_watch_id;
	gint editor_watch_id;

	/* Assist */
	ParserClangAssist *assist;
	
	/* Preferences */
	GtkBuilder* bxml;
};

struct _ParserClangPluginClass {
	AnjutaPluginClass parent_class;
};

#endif
