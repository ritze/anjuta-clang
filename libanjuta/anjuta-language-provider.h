/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-language-provider.h
 * Copyright (C) Naba Kumar  <naba@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef _ANJUTA_LANGUAGE_PROVIDER_H_
#define _ANJUTA_LANGUAGE_PROVIDER_H_

#include <glib.h>
#include <glib-object.h>
#include <libanjuta/interfaces/ianjuta-editor.h>
#include <libanjuta/interfaces/ianjuta-editor-tip.h>
#include <libanjuta/interfaces/ianjuta-iterable.h>
#include <libanjuta/interfaces/ianjuta-provider.h>

G_BEGIN_DECLS

#define ANJUTA_TYPE_LANGUAGE_PROVIDER             (anjuta_language_provider_get_type ())
#define ANJUTA_LANGUAGE_PROVIDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), ANJUTA_TYPE_LANGUAGE_PROVIDER, AnjutaLanguageProvider))
#define ANJUTA_LANGUAGE_PROVIDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), ANJUTA_TYPE_LANGUAGE_PROVIDER, AnjutaLanguageProviderClass))
#define ANJUTA_IS_LANGUAGE_PROVIDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ANJUTA_TYPE_LANGUAGE_PROVIDER))
#define ANJUTA_IS_LANGUAGE_PROVIDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), ANJUTA_TYPE_LANGUAGE_PROVIDER))
#define ANJUTA_LANGUAGE_PROVIDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), ANJUTA_TYPE_LANGUAGE_PROVIDER, AnjutaLanguageProviderClass))

typedef struct _AnjutaLanguageProviderClass AnjutaLanguageProviderClass;
typedef struct _AnjutaLanguageProvider AnjutaLanguageProvider;
typedef struct _AnjutaLanguageProviderPriv AnjutaLanguageProviderPriv;
 
struct _AnjutaLanguageProviderClass
{
	GObjectClass parent_class;
};

struct _AnjutaLanguageProvider
{
	GObject parent_instance;
	
	AnjutaLanguageProviderPriv *priv;
};

GType anjuta_language_provider_get_type (void) G_GNUC_CONST;

AnjutaLanguageProvider* 
anjuta_language_provider_new					(IAnjutaEditor *ieditor,
                                                 GSettings* settings);

gchar*
anjuta_language_provider_get_pre_word			(IAnjutaEditor* editor,
                                                 IAnjutaIterable *iter,
                                                 IAnjutaIterable** start_iter,
                                                 const gchar *word_characters);

gchar*
anjuta_language_provider_get_calltip_context	(IAnjutaEditorTip* itip,
                                                 IAnjutaIterable* iter,
                                                 const gchar* scope_context_ch);

void
anjuta_language_provider_activate				(AnjutaLanguageProvider* lang_prov,
                                                 IAnjutaProvider* iprov,
                                                 IAnjutaIterable* iter,
                                                 gpointer data);

void
anjuta_language_provider_populate				(AnjutaLanguageProvider* lang_prov,
                                                 IAnjutaProvider* provider,
                                                 IAnjutaIterable* cursor);

IAnjutaIterable*
anjuta_language_provider_get_start_iter			(AnjutaLanguageProvider* lang_prov);

G_END_DECLS

#endif
