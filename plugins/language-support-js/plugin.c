/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    Copyright (C) 2009 Maxim Ermilov   <zaspire@rambler.ru>

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
#include <libanjuta/anjuta-shell.h>
#include <libanjuta/anjuta-session.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/interfaces/ianjuta-calltip-provider.h>
#include <libanjuta/interfaces/ianjuta-document-manager.h>
#include <libanjuta/interfaces/ianjuta-project-manager.h>
#include <libanjuta/interfaces/ianjuta-editor-assist.h>
#include <libanjuta/interfaces/ianjuta-editor-tip.h>
#include <libanjuta/interfaces/ianjuta-language.h>
#include <libanjuta/interfaces/ianjuta-preferences.h>
#include <ctype.h>

#include <glib.h>
#include "util.h"
#include "plugin.h"
#include "prefs.h"
#include "code-completion.h"

#include "gi-symbol.h"

#define PREFS_BUILDER ANJUTA_GLADE_DIR"/anjuta-language-javascript.ui"
#define ICON_FILE "anjuta-language-cpp-java-plugin.png"
#define UI_FILE ANJUTA_UI_DIR"/anjuta-language-javascript.xml"

#define JSDIRS_LISTSTORE "jsdirs_liststore"
#define JSDIRS_TREEVIEW "jsdirs_treeview"

static gpointer parent_class;

static void
on_value_added_current_editor (AnjutaPlugin *plugin, const gchar *name,
							   const GValue *value, gpointer data);
static void
on_value_removed_current_editor (AnjutaPlugin *plugin, const gchar *name,
								 gpointer data);

/*Export for GtkBuilder*/
G_MODULE_EXPORT void
on_jsdirs_rm_button_clicked (GtkButton *button, gpointer user_data);
G_MODULE_EXPORT void
on_jsdirs_add_button_clicked (GtkButton *button, gpointer user_data);

static gboolean
js_support_plugin_activate (AnjutaPlugin *plugin)
{
	JSLang *js_support_plugin;

	DEBUG_PRINT ("%s", "JSLang: Activating JSLang plugin ...");
	js_support_plugin = (JSLang*) plugin;
	js_support_plugin->editor_watch_id =
		anjuta_plugin_add_watch (plugin, IANJUTA_DOCUMENT_MANAGER_CURRENT_DOCUMENT,
					 on_value_added_current_editor,
					 on_value_removed_current_editor,
					 plugin);

	js_support_plugin->prefs = g_settings_new (JS_SUPPORT_SCHEMA);
	return TRUE;
}

static gboolean
js_support_plugin_deactivate (AnjutaPlugin *plugin)
{
	JSLang *js_support_plugin;

	DEBUG_PRINT ("%s", "JSLang: Dectivating JSLang plugin ...");
	js_support_plugin = (JSLang*) plugin;
	anjuta_plugin_remove_watch (plugin, js_support_plugin->editor_watch_id, TRUE);
	return TRUE;
}

static void
js_support_plugin_finalize (GObject *obj)
{
	JSLang *self = (JSLang*)obj;

	g_object_unref (self->symbol);
	self->symbol = NULL;

	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
js_support_plugin_dispose (GObject *obj)
{
	JSLang *self = (JSLang*)obj;

	g_assert (self != NULL);

	g_object_unref (self->symbol);
	self->symbol = NULL;

	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
js_support_plugin_instance_init (GObject *obj)
{
	JSLang *plugin = (JSLang*)obj;
	plugin->prefs = NULL;
	plugin->symbol = NULL;
}

static void
js_support_plugin_class_init (GObjectClass *klass)
{
	AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	plugin_class->activate = js_support_plugin_activate;
	plugin_class->deactivate = js_support_plugin_deactivate;
	klass->finalize = js_support_plugin_finalize;
	klass->dispose = js_support_plugin_dispose;
}

static void
install_support (JSLang *plugin)
{
	const gchar *lang;
	IAnjutaLanguage* lang_manager;

	setPlugin (plugin);

	if (!IANJUTA_IS_EDITOR (plugin->current_editor))
		return;
	lang_manager = anjuta_shell_get_interface (ANJUTA_PLUGIN (plugin)->shell,
							IAnjutaLanguage, NULL);
	if (!lang_manager)
		return;
	lang = ianjuta_language_get_name_from_editor (lang_manager,
													   IANJUTA_EDITOR_LANGUAGE (plugin->current_editor), NULL);
	if (!lang || !g_str_equal (lang, "JavaScript"))
		return;

	DEBUG_PRINT ("%s", "JSLang: Install support");
}

static void
uninstall_support (JSLang *plugin)
{
	DEBUG_PRINT ("%s", "JSLang: Uninstall support");
}

static void
on_value_added_current_editor (AnjutaPlugin *plugin, const gchar *name,
							   const GValue *value, gpointer data)
{
	JSLang *js_support_plugin;
	IAnjutaDocument* doc = IANJUTA_DOCUMENT(g_value_get_object (value));

	DEBUG_PRINT ("%s", "JSLang: Add editor");

	js_support_plugin = (JSLang*) plugin;
	if (IANJUTA_IS_EDITOR(doc))
		js_support_plugin->current_editor = G_OBJECT(doc);
	else
	{
		js_support_plugin->current_editor = NULL;
		return;
	}
	install_support (js_support_plugin);
}

static void
on_value_removed_current_editor (AnjutaPlugin *plugin, const gchar *name,
								 gpointer data)
{
	JSLang *js_support_plugin;

	DEBUG_PRINT ("%s", "JSLang: Remove editor");

	js_support_plugin = (JSLang*) plugin;
	if (IANJUTA_IS_EDITOR(js_support_plugin->current_editor))
		uninstall_support (js_support_plugin);
	js_support_plugin->current_editor = NULL;
}

static void
jsdirs_save (GtkTreeModel *list_store)
{
	GtkTreeIter iter;
	const gchar *project_root = NULL;
	anjuta_shell_get (ANJUTA_PLUGIN (getPlugin ())->shell,
					  IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI,
					  G_TYPE_STRING, &project_root, NULL);

	GFile *tmp = g_file_new_for_uri (project_root);
	AnjutaSession *session = anjuta_session_new (g_file_get_path (tmp));
	g_object_unref (tmp);

	GList *dirs = NULL;
	if (!gtk_tree_model_iter_children (list_store, &iter, NULL))
		return;
	do
	{
		gchar *dir;
		gtk_tree_model_get (list_store, &iter, 0, &dir, -1);

		g_assert (dir != NULL);

		dirs = g_list_append (dirs, dir);
	} while (gtk_tree_model_iter_next (list_store, &iter));
	anjuta_session_set_string_list (session, "options", "js_dirs", dirs);
	anjuta_session_sync (session);
}

G_MODULE_EXPORT void
on_jsdirs_rm_button_clicked (GtkButton *button, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeView *tree = GTK_TREE_VIEW (user_data);
	GtkTreeModel *list_store = gtk_tree_view_get_model (tree);
	GtkTreeSelection *selection = gtk_tree_view_get_selection (tree);

	if (!gtk_tree_selection_get_selected (selection, &list_store, &iter))
		return;
	gtk_list_store_remove (GTK_LIST_STORE (list_store), &iter);
	jsdirs_save (list_store);
}

G_MODULE_EXPORT void
on_jsdirs_add_button_clicked (GtkButton *button, gpointer user_data)
{
	GtkWidget *dialog;

	g_assert (user_data != NULL);

	GtkTreeView *tree = GTK_TREE_VIEW (user_data);
	GtkListStore *list_store = GTK_LIST_STORE (gtk_tree_view_get_model (tree));

	g_assert (list_store != NULL);

	dialog = gtk_file_chooser_dialog_new ("Choose directory",
				      NULL,
				      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		if (filename)
		{
			GtkTreeIter iter;
			gtk_list_store_append (list_store, &iter);
			gtk_list_store_set (list_store, &iter, 0, filename, -1);
			g_free (filename);
		}
		jsdirs_save (GTK_TREE_MODEL (list_store));
	}
	gtk_widget_destroy (dialog);
}

static void
jsdirs_init_treeview (GtkBuilder* bxml)
{
	const gchar *project_root = NULL;
	GtkTreeIter iter;
	GtkListStore *list_store = GTK_LIST_STORE (gtk_builder_get_object (bxml, JSDIRS_LISTSTORE));
	if (!list_store)
		return;

	anjuta_shell_get (ANJUTA_PLUGIN (getPlugin ())->shell,
					  IANJUTA_PROJECT_MANAGER_PROJECT_ROOT_URI,
					  G_TYPE_STRING, &project_root, NULL);

	GFile *tmp = g_file_new_for_uri (project_root);
	AnjutaSession *session = anjuta_session_new (g_file_get_path (tmp));
	g_object_unref (tmp);
	GList* dir_list = anjuta_session_get_string_list (session, "options", "js_dirs");
	GList *i;
	gtk_list_store_clear (list_store);

	for (i = dir_list; i; i = g_list_next (i))
	{
		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter, 0, i->data, -1);
	}
	if (!dir_list)
	{
		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter, 0, ".", -1);
	}
}

static void
ipreferences_merge (IAnjutaPreferences* ipref, AnjutaPreferences* prefs,
					GError** e)
{
	GError* error = NULL;
	GtkBuilder* bxml = gtk_builder_new ();

	/* Add preferences */
	if (!gtk_builder_add_from_file (bxml, PREFS_BUILDER, &error))
	{
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}
	GtkTreeView *tree = GTK_TREE_VIEW (gtk_builder_get_object (bxml, JSDIRS_TREEVIEW));

	gtk_builder_connect_signals (bxml, tree);
	jsdirs_init_treeview (bxml);
	anjuta_preferences_add_from_builder (prefs,
								 bxml, NULL, "vbox1", _("JavaScript"),
								 ICON_FILE);
	g_object_unref (bxml);
}

static void
ipreferences_unmerge (IAnjutaPreferences* ipref, AnjutaPreferences* prefs,
					  GError** e)
{
	anjuta_preferences_remove_page(prefs, _("JavaScript"));
}

static void
ipreferences_iface_init (IAnjutaPreferencesIface* iface)
{
	iface->merge = ipreferences_merge;
	iface->unmerge = ipreferences_unmerge;
}

static void 
iprovider_populate (IAnjutaProvider *obj, IAnjutaIterable* iter, GError **err)
{
	static GList *trash = NULL;
	JSLang *plugin = (JSLang*)obj;

	if (plugin->start_iter) {
		g_object_unref (plugin->start_iter);
	}

        ianjuta_parser_set_start_iter (assist->priv->parser, iter, NULL);

	if (!plugin->current_editor)
		return;
	gint depth;
	GList *suggestions = NULL;
	gchar *str = code_completion_get_str (IANJUTA_EDITOR (plugin->current_editor), FALSE);

	if (trash)
	{
		g_list_foreach (trash, (GFunc)g_free, NULL);
		g_list_free (trash);
		trash = NULL;
	}

	if (!str)
		return;

	g_assert (plugin->prefs);

	if (strlen (str) < g_settings_get_int (plugin->prefs, MIN_CODECOMPLETE))
	{
		ianjuta_editor_assist_proposals ( IANJUTA_EDITOR_ASSIST (plugin->current_editor), obj,  NULL, NULL,  TRUE, NULL);
		return;
	}

	gchar *file = file_completion (IANJUTA_EDITOR (plugin->current_editor), &depth);
	gint i;
	DEBUG_PRINT ("JSLang: Auto complete for %s (TMFILE=%s)", str, file);
	for (i = strlen (str) - 1; i; i--)
	{
		if (str[i] == '.')
			break;
	}
	if (i > 0)
	{
		suggestions = code_completion_get_list (plugin, file, g_strndup (str, i), depth);
	}
	else
		suggestions = code_completion_get_list (plugin, file, NULL, depth);
	if (suggestions)
	{
		GList *nsuggest = NULL;
                gint k;
		if (i > 0)
		{
			suggestions = filter_list (suggestions, str + i + 1);
			k = strlen (str + i + 1);
		} else
		{
			suggestions = filter_list (suggestions, str);
			k = strlen (str);
		}
		GList *i;
		for (; k > 0; k--)
			ianjuta_iterable_previous (plugin->start_iter, NULL);

		for (i = suggestions; i; i = g_list_next(i)) {
			IAnjutaEditorAssistProposal* proposal = g_new0(IAnjutaEditorAssistProposal, 1);
			IAnjutaParserProposalData* prop_data;

			if (!i->data)
				continue;
			
			//TODO: Or "prop_data->name = i->data;" ?
			prop_data->name = i->data->name;
			prop_data->is_func = code_completion_is_symbol_func (plugin, i->data);
			//TODO: not implemented yet
			prop_data->has_para = TRUE;
			//TODO: is this the right way?
			prop_data->info = i->data;
			
			proposal->label = i->data->name;
//			proposal->data = i->data;
			proposal->data = prop_data;
			nsuggest = g_list_prepend (nsuggest, proposal);
		}
		ianjuta_editor_assist_proposals ( IANJUTA_EDITOR_ASSIST (plugin->current_editor), obj,  nsuggest, NULL, TRUE, NULL);
		g_list_free (nsuggest);
        trash = suggestions;
		return;
	}
	ianjuta_editor_assist_proposals ( IANJUTA_EDITOR_ASSIST (plugin->current_editor), obj,  NULL, NULL, TRUE, NULL);
}

ANJUTA_PLUGIN_BEGIN (JSLang, js_support_plugin);
ANJUTA_PLUGIN_ADD_INTERFACE(ipreferences, IANJUTA_TYPE_PREFERENCES);
ANJUTA_PLUGIN_ADD_INTERFACE(iparser_calltip, IANJUTA_TYPE_CALLTIP_PROVIDER);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (JSLang, js_support_plugin);
