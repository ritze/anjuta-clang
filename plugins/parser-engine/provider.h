/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C)  2007 Naba Kumar  <naba@gnome.org>
 * 
 * anjuta is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * anjuta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with anjuta.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _PARSER_PROVIDER_H_
#define _PARSER_PROVIDER_H_

#include <glib-object.h>
#include <libanjuta/anjuta-preferences.h>
#include <libanjuta/interfaces/ianjuta-calltip-provider.h>
#include <libanjuta/interfaces/ianjuta-editor-assist.h>
#include <libanjuta/interfaces/ianjuta-parser.h>

G_BEGIN_DECLS

#define TYPE_PARSER_PROVIDER             (parser_provider_get_type ())
#define PARSER_PROVIDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_PARSER_PROVIDER, ParserProvider))
#define PARSER_PROVIDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_PARSER_PROVIDER, ParserProviderClass))
#define IS_PARSER_PROVIDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_PARSER_PROVIDER))
#define IS_PARSER_PROVIDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_PARSER_PROVIDER))
#define PARSER_PROVIDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_PARSER_PROVIDER, ParserProviderClass))

typedef struct _ParserProviderClass ParserProviderClass;
typedef struct _ParserProvider ParserProvider;
typedef struct _ParserProviderContext ParserProviderContext;
typedef struct _ParserProviderPriv ParserProviderPriv;

struct _ParserProviderContext {
	GCompletion* completion;
	GList* tips;
	gint position;
};

struct _ParserProviderClass
{
	GObjectClass parent_class;
};

struct _ParserProvider
{
	GObject parent_instance;
	ParserProviderPriv *priv;
};

GType parser_provider_get_type (void) G_GNUC_CONST;
ParserProvider *parser_provider_new (IAnjutaEditor *ieditor,
                                     IAnjutaParser *iparser,
                                     IAnjutaCalltipProvider *icalltip_provider,
                                     const gchar *language);

G_END_DECLS

#endif /* _PARSER_PROVIDER_H_ */
