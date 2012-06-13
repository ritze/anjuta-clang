/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) Moritz Luedecke 2012 <ritze@skweez.net>
 * 
 * anjuta is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * anjuta is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//#ifndef _PARSER_CLANG_PARSER_H_
//#define _PARSER_CLANG_PARSER_H_

#include <glib-object.h>
#include <libanjuta/interfaces/ianjuta-symbol-manager.h>
/*
G_BEGIN_DECLS

#define TYPE_PARSER_CLANG_PARSER             (parser_clang_parser_get_type ())
#define PARSER_CLANG_PARSER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_PARSER_CLANG_PARSER, ParserClangParser))
#define PARSER_CLANG_PARSER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_PARSER_CLANG_PARSER, ParserClangParserClass))
#define IS_PARSER_CLANG_PARSER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_PARSER_CLANG_PARSER))
#define IS_PARSER_CLANG_PARSER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_PARSER_CLANG_PARSER))
#define PARSER_CLANG_PARSER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_PARSER_CLANG_PARSER, ParserClangParserClass))

typedef struct _ParserClangParserClass ParserClangParserClass;
typedef struct _ParserClangParser ParserClangParser;
typedef struct _ParserClangParserContext ParserClangParserContext;
typedef struct _ParserClangParserPriv ParserClangParserPriv;

struct _ParserClangParserContext {
};

struct _ParserClangParserClass
{
	GObjectClass parent_class;
};

struct _ParserClangParser
{
	GObject parent_instance;
	ParserClangParserPriv *priv;
};
*/
void
parser_clang_parser_init (const gchar *full_file_path);

void
parser_clang_parser_deinit (void);
	
/**
 * The function parse the C++ statement, try to get the type of objects to be
 * completed and returns an iterator with those symbols.
 * @param stmt A statement like "((FooKlass*) B).", "foo_klass1->get_foo_klass2 ()->",
 * "foo_klass1->member2->", etc.
 * @above_text Text of the buffer/file before the statement up to the first byte.
 * @param full_file_path The full path to the file. This is for engine scanning purposes.
 * @param linenum The line number where the statement is.
 *	 
 * @return IAnjutaIterable * with the actual completions symbols.
 */
IAnjutaIterable *
parser_clang_parser_process_expression (const gchar *stmt,
                                        const gchar *above_text,
                                        const gchar *full_file_path,
                                        gulong linenum);

/**
 * @return NULL, if nothing found...
 * 
 */
gchar*
parser_clang_parser_get_definition (gint line, gint column);


//G_END_DECLS

//#endif /* _PARSER_CLANG_PARSER_H_ */