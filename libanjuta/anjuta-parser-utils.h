/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-parser-utils.h
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
#ifndef _ANJUTA_PARSER_UTILS_H_
#define _ANJUTA_PARSER_UTILS_H_

#include <glib.h>
#include <libanjuta/interfaces/ianjuta-editor.h>
#include <libanjuta/interfaces/ianjuta-editor-tip.h>
#include <libanjuta/interfaces/ianjuta-iterable.h>

G_BEGIN_DECLS

gchar*		anjuta_parser_util_get_pre_word				(IAnjutaEditor* editor,
                                                         IAnjutaIterable *iter,
                                                         IAnjutaIterable** start_iter,
                                                         const gchar *word_characters);
                                     
gchar*		anjuta_parser_util_get_calltip_context		(IAnjutaEditorTip* itip,
                                                         IAnjutaIterable* iter,
                                                         const gchar* scope_context_characters);
                                     
G_END_DECLS

#endif
