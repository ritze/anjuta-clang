/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    plugin.c
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

#include <config.h>
#include <libanjuta/anjuta-debug.h>
#include <libanjuta/interfaces/ianjuta-project-backend.h>

#include "plugin.h"
#include "mk-project.h"


#define ICON_FILE "mk-project-plugin-48.png"

/* AnjutaPlugin functions
 *---------------------------------------------------------------------------*/

static gboolean
activate_plugin (AnjutaPlugin *plugin)
{
	DEBUG_PRINT ("MkpPlugin: Activating Anjuta make backend Plugin ...");
	
	return TRUE;
}

static gboolean
deactivate_plugin (AnjutaPlugin *plugin)
{
	DEBUG_PRINT ("MkpPlugin: Deacctivating Anjuta make backend Plugin ...");
	return TRUE;
}


/* IAnjutaProjectBackend implementation
 *---------------------------------------------------------------------------*/

static IAnjutaProject*
iproject_backend_new_project (IAnjutaProjectBackend* backend, GFile *file, GError** err)
{
	IAnjutaProject *project;
	DEBUG_PRINT("create new mkp project");	
	project = (IAnjutaProject *) mkp_project_new(file, err);
		
	return project;
}

static gint
iproject_backend_probe (IAnjutaProjectBackend* backend, GFile *directory, GError** err)
{
	return mkp_project_probe (directory, err);
}

static void
iproject_backend_iface_init(IAnjutaProjectBackendIface *iface)
{
	iface->new_project = iproject_backend_new_project;
	iface->probe = iproject_backend_probe;
}

/* GObject functions
 *---------------------------------------------------------------------------*/

/* Used in dispose and finalize */
static gpointer parent_class;

static void
mkp_plugin_instance_init (GObject *obj)
{
}

/* dispose is used to unref object created with instance_init */

static void
dispose (GObject *obj)
{
	G_OBJECT_CLASS (parent_class)->dispose (obj);
}

/* finalize used to free object created with instance init */

static void
finalize (GObject *obj)
{
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
mkp_plugin_class_init (GObjectClass *klass)
{
	AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	plugin_class->activate = activate_plugin;
	plugin_class->deactivate = deactivate_plugin;
	klass->dispose = dispose;
	klass->finalize = finalize;
}

/* AnjutaPlugin declaration
 *---------------------------------------------------------------------------*/

ANJUTA_PLUGIN_BEGIN (MkpPlugin, mkp_plugin);
ANJUTA_PLUGIN_ADD_INTERFACE (iproject_backend, IANJUTA_TYPE_PROJECT_BACKEND);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (MkpPlugin, mkp_plugin);
