/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    execute.c
    Copyright (C) 2008 Sébastien Granjoux

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

/*
 * Run and Stop program
 *---------------------------------------------------------------------------*/

#include <config.h>

#include "execute.h"

#include <libanjuta/anjuta-debug.h>
#include <libanjuta/interfaces/ianjuta-terminal.h>

#include <libgnomevfs/gnome-vfs-utils.h>

#include <signal.h>

/* Constants
 *---------------------------------------------------------------------------*/

#define PREF_USE_SB "build.use_scratchbox"
#define PREF_SB_PATH "build.scratchbox.path"


/*----------------------------------------------------------------------------
 * Type definitions
 */

struct _RunProgramChild
{
	GPid pid;
	guint source;
	gboolean use_signal;
	gboolean terminated;
};

/* Private functions
 *---------------------------------------------------------------------------*/

static void
on_child_terminated (GPid pid, gint status, gpointer user_data);

static void
run_plugin_child_free (RunProgramPlugin *plugin, GPid pid)
{
	GList *child;
	
	for (child = g_list_first (plugin->child); child != NULL; child = g_list_next (child))
	{
		if (((RunProgramChild *)child->data)->pid == pid)
		{
			if (((RunProgramChild *)child->data)->use_signal)
			{
				g_return_if_fail (plugin->child_exited_connection > 0);
				plugin->child_exited_connection--;
				if (plugin->child_exited_connection == 0)
				{
					IAnjutaTerminal *term;
		
					term = anjuta_shell_get_interface (ANJUTA_PLUGIN (plugin)->shell,
													   IAnjutaTerminal, NULL);
					g_signal_handlers_disconnect_by_func (term, on_child_terminated, plugin);
				}
			}
			else if (((RunProgramChild *)child->data)->source)
			{
				g_source_remove (((RunProgramChild *)child->data)->source);
			}
			g_free (child->data);
			plugin->child = g_list_delete_link (plugin->child, child);
			break;
		}
	}

	run_plugin_update_menu_sensitivity (plugin);
}

static void
on_child_terminated (GPid pid, gint status, gpointer user_data)
{
	RunProgramPlugin *plugin = (RunProgramPlugin *)user_data;
	
	run_plugin_child_free (plugin, pid);
}


static void
on_child_terminated_signal (IAnjutaTerminal *term, GPid pid, gint status, gpointer user_data)
{
	on_child_terminated (pid, status, user_data);
}

static gboolean
get_local_executable_and_directory (RunProgramPlugin *plugin, gchar **local, gchar **dir)
{
	gchar *target = NULL;
	gchar *dir_uri = NULL;
	const gchar *err_msg;
	
	anjuta_shell_get (ANJUTA_PLUGIN (plugin)->shell,
	 				  RUN_PROGRAM_DIR, G_TYPE_STRING, &dir_uri,
					  RUN_PROGRAM_URI, G_TYPE_STRING, &target, NULL);
	
	if (target == NULL)
	{
		g_free (target);
		g_free (dir_uri);
		return FALSE;
	}
	else if ((*local = gnome_vfs_get_local_path_from_uri (target)) == NULL)
	{
		/* Only local program are supported */
		err_msg = _("Program '%s' is not a local file");
	}
	else if ((dir_uri != NULL) && ((*dir = gnome_vfs_get_local_path_from_uri (dir_uri)) == NULL))
	{
		gchar *tmp;	
		/* Only local directory are supported */
		err_msg = _("Program directory '%s' is not local");
		tmp = target;
		target = dir_uri;
		dir_uri = tmp;
		g_free (*local);
	}
	else
	{
		g_free (target);
		g_free (dir_uri);
		target = *local;	

		if (g_file_test (*local, G_FILE_TEST_EXISTS) == FALSE)
		{
			err_msg = _("Program '%s' does not exists");
		}
		else if (g_file_test (*local, G_FILE_TEST_IS_EXECUTABLE) == FALSE)
		{
			err_msg = _("Program '%s' does not have execution permission");
		}
		else
		{
			/* Return local executable */
			return TRUE;
		}
	}
			
	anjuta_util_dialog_error (GTK_WINDOW (ANJUTA_PLUGIN (plugin)->shell), err_msg, target);
	g_free (target);
	g_free (dir_uri);
	
	return FALSE;
}

static GPid
execute_with_terminal (RunProgramPlugin *plugin,
					   const gchar *dir, const gchar *cmd, gchar **env)
{
	IAnjutaTerminal *term;
	GPid pid = 0;
		
	term = anjuta_shell_get_interface (ANJUTA_PLUGIN (plugin)->shell,
										   IAnjutaTerminal, NULL);
	if (term)
	{
		gchar* launcher_path = g_find_program_in_path("anjuta_launcher");
		gchar *new_cmd;
		RunProgramChild *child;
	
		if (launcher_path != NULL)
		{
			new_cmd = g_strconcat ("anjuta_launcher ", cmd, NULL);
			g_free (launcher_path);
		}
		else
		{
			DEBUG_PRINT("Missing anjuta_launcher");
			new_cmd = g_strdup (cmd);
		}

		if (plugin->child_exited_connection == 0)
		{
			g_signal_connect (term, "child-exited", G_CALLBACK (on_child_terminated_signal), plugin);
		}
		plugin->child_exited_connection++;
		child = g_new0 (RunProgramChild, 1);
		child->use_signal = TRUE;
		plugin->child = g_list_prepend (plugin->child, child);
		
		pid = ianjuta_terminal_execute_command (term, dir, new_cmd, env, NULL);
		child->pid = pid;
		g_free (new_cmd);
	}
	
	return pid;
}

static GPid
execute_without_terminal (RunProgramPlugin *plugin,
					   const gchar *dir, gchar *cmd, gchar **env)
{
	char *user_shell;
	char * argv[4];
	GPid pid;
	RunProgramChild *child;
	gsize len;
	gchar **new_env;
	gchar **old_env;
	gchar **p;	
	gint i;

	/* Create environment variable array */
	old_env = g_listenv();
	len = old_env ? g_strv_length (old_env) : 0;
	len += env ? g_strv_length (env) : 0;
	len ++;	
	new_env = g_new (char *, len);
	
	/* Remove some environment variables, Move other in new_env */
	i = 0;
	for (p = old_env; *p; p++)
	{
		const gchar *value;
		
		value = g_getenv (*p);
		if (env != NULL)
		{
			gchar **q;
		
			for (q = env; *q; q++)
			{
				gsize len;
				
				len = strlen (*p);
				if ((strlen (*q) > len + 1) &&
					(strncmp (*q, *p, len) == 0) &&
					(*q[len] == '='))
				{
					value = *q + len + 1;
					break;
				}
			}
		}
		
		new_env[i++] = g_strconcat (*p, "=", value, NULL);
	}
	g_strfreev (old_env);
	
	/* Add new user variable */
	for (p = env; *p; p++)
	{
		new_env[i++] = g_strdup (*p);
	}
	new_env[i] = NULL;
	
	/* Run user program using in a shell */
	user_shell = gnome_util_user_shell ();
	argv[0] = user_shell;
	argv[1] = "-c";
	argv[2] = cmd;
	argv[3] = NULL;

	child = g_new0 (RunProgramChild, 1);
	plugin->child = g_list_prepend (plugin->child, child);
	
	if (g_spawn_async_with_pipes (dir, argv, new_env, G_SPAWN_DO_NOT_REAP_CHILD,
									NULL, NULL, &pid, NULL, NULL, NULL, NULL))
	{
		child->pid = pid;
		child->source = g_child_watch_add (pid, on_child_terminated, plugin);
	}
	else
	{
		pid = 0;
	}
	
	g_free (user_shell);
	g_strfreev (new_env);
	
	return pid;
}

/* Public functions
 *---------------------------------------------------------------------------*/

gboolean
run_plugin_run_program (RunProgramPlugin *plugin)
{
	gchar *target;
	gchar *dir = NULL;
	gchar *args = NULL;
	gchar **env = NULL;
	gchar *cmd;
	gboolean run_in_terminal = 0;
	AnjutaPreferences *prefs;
	GPid pid;
	
	if (!get_local_executable_and_directory (plugin, &target, &dir))
		return FALSE;
	
	/* Get other parameters from shell */
	anjuta_shell_get (ANJUTA_PLUGIN (plugin)->shell,
					RUN_PROGRAM_ARGS, G_TYPE_STRING, &args,
					RUN_PROGRAM_ENV, G_TYPE_STRV, &env,
					RUN_PROGRAM_NEED_TERM, G_TYPE_BOOLEAN, &run_in_terminal,
					NULL);
	
	/* TODO: Check if target is up to date */

	if (dir == NULL)
		dir = g_path_get_dirname (target);
	
	if (args && strlen (args) > 0)
		cmd = g_strconcat (target, " ", args, NULL);
	else
		cmd = g_strdup (target);
	g_free (args);
	g_free (target);

	/* Take care of scratchbox */
	prefs = anjuta_shell_get_preferences (ANJUTA_PLUGIN(plugin)->shell, NULL);
	if (anjuta_preferences_get_int (prefs , PREF_USE_SB))
	{
		const gchar* sb_path = anjuta_preferences_get(prefs, PREF_SB_PATH);
		/* we need to skip the /scratchbox/users part, maybe could be done more clever */
		const gchar* real_dir = strstr(dir, "/home");
		gchar* oldcmd = cmd;
		gchar* olddir = dir;
		
		cmd = g_strdup_printf("%s/login -d %s \"%s\"", sb_path,
									  real_dir, oldcmd);
		g_free(oldcmd);
		dir = g_strdup(real_dir);
		g_free (olddir);
	}
	
	if (run_in_terminal)
	{
		pid = execute_with_terminal (plugin, dir, cmd, env);
		if (!pid)
		{
			pid = execute_without_terminal (plugin, dir, cmd, env);
		}
	}
	else
	{
		pid = execute_without_terminal (plugin, dir, cmd, env);
	}
	
	if (pid == 0)
	{
		anjuta_util_dialog_error (GTK_WINDOW (ANJUTA_PLUGIN (plugin)->shell),
								  "Unable to execute %s", cmd);
	}
	run_plugin_update_menu_sensitivity (plugin);
	
	g_free (dir);
	g_strfreev (env);
	g_free (cmd);

	return TRUE;
}

gboolean
run_plugin_kill_program (RunProgramPlugin *plugin, gboolean terminate)
{
	if (plugin->child != NULL)
	{
		RunProgramChild *child = (RunProgramChild *)plugin->child->data;
		
		if (!child->terminated && terminate)
		{
			kill (child->pid, SIGTERM);
			child->terminated = TRUE;
		}
		else
		{
			kill (child->pid, SIGKILL);
			run_plugin_child_free (plugin, child->pid);
		}
	}
	
	return TRUE;
}

void
run_free_all_children (RunProgramPlugin *plugin)
{
	GList *child;
	IAnjutaTerminal *term;

	/* Remove terminal child-exited handle */
	term = anjuta_shell_get_interface (ANJUTA_PLUGIN (plugin)->shell,
										IAnjutaTerminal, NULL);
	g_signal_handlers_disconnect_by_func (term, on_child_terminated, plugin);
	plugin->child_exited_connection = 0;

	/* Remove all child-exited source */
	for (child = g_list_first (plugin->child); child != NULL; child = g_list_next (child))
	{
		if (!((RunProgramChild *)child->data)->use_signal)
		{
			g_source_remove (((RunProgramChild *)child->data)->source);
		}
		g_free (child->data);
	}
	g_list_free (plugin->child);
	plugin->child = NULL;
}