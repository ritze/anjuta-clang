/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) Eran Ifrah (Main file for CodeLite www.codelite.org/ )
 * Copyright (C) Massimo Cora' 2009 <maxcvs@email.it> (Customizations for Anjuta)
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

#ifndef lint
static char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define yyclearin (yychar=(-1))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING (yyerrflag!=0)
#define yyparse cl_expr_parse
#define yylex cl_expr_lex
#define yyerror cl_expr_error
#define yychar cl_expr_char
#define yyval cl_expr_val
#define yylval cl_expr_lval
#define yydebug cl_expr_debug
#define yynerrs cl_expr_nerrs
#define yyerrflag cl_expr_errflag
#define yyss cl_expr_ss
#define yyssp cl_expr_ssp
#define yyvs cl_expr_vs
#define yyvsp cl_expr_vsp
#define yylhs cl_expr_lhs
#define yylen cl_expr_len
#define yydefred cl_expr_defred
#define yydgoto cl_expr_dgoto
#define yysindex cl_expr_sindex
#define yyrindex cl_expr_rindex
#define yygindex cl_expr_gindex
#define yytable cl_expr_table
#define yycheck cl_expr_check
#define yyname cl_expr_name
#define yyrule cl_expr_rule
#define YYPREFIX "cl_expr_"
   
/* Copyright Eran Ifrah(c)*/
/*************** Includes and Defines *****************************/
#include <string>
#include <vector>
#include <stdio.h>
#include <map>
#include "expression-result.h"

#define YYSTYPE std::string
#define YYDEBUG 0        /* get the pretty debugging code to compile*/

void cl_expr_error(char *string);

static ExpressionResult result;

/*---------------------------------------------*/
/* externs defined in the lexer*/
/*---------------------------------------------*/
extern char *cl_expr_text;
extern int cl_expr_lex();
extern int cl_expr_parse();
extern int cl_expr_lineno;
extern std::vector<std::string> currentScope;
extern bool setExprLexerInput(const std::string &in);
extern void cl_expr_lex_clean();

/*************** Standard ytab.c continues here *********************/
#define LE_AUTO 257
#define LE_DOUBLE 258
#define LE_INT 259
#define LE_STRUCT 260
#define LE_BREAK 261
#define LE_ELSE 262
#define LE_LONG 263
#define LE_SWITCH 264
#define LE_CASE 265
#define LE_ENUM 266
#define LE_REGISTER 267
#define LE_TYPEDEF 268
#define LE_CHAR 269
#define LE_EXTERN 270
#define LE_RETURN 271
#define LE_UNION 272
#define LE_CONST 273
#define LE_FLOAT 274
#define LE_SHORT 275
#define LE_UNSIGNED 276
#define LE_CONTINUE 277
#define LE_FOR 278
#define LE_SIGNED 279
#define LE_VOID 280
#define LE_DEFAULT 281
#define LE_GOTO 282
#define LE_SIZEOF 283
#define LE_VOLATILE 284
#define LE_DO 285
#define LE_IF 286
#define LE_STATIC 287
#define LE_WHILE 288
#define LE_NEW 289
#define LE_DELETE 290
#define LE_THIS 291
#define LE_OPERATOR 292
#define LE_CLASS 293
#define LE_PUBLIC 294
#define LE_PROTECTED 295
#define LE_PRIVATE 296
#define LE_VIRTUAL 297
#define LE_FRIEND 298
#define LE_INLINE 299
#define LE_OVERLOAD 300
#define LE_TEMPLATE 301
#define LE_TYPENAME 302
#define LE_THROW 303
#define LE_CATCH 304
#define LE_IDENTIFIER 305
#define LE_STRINGliteral 306
#define LE_FLOATINGconstant 307
#define LE_INTEGERconstant 308
#define LE_CHARACTERconstant 309
#define LE_OCTALconstant 310
#define LE_HEXconstant 311
#define LE_POUNDPOUND 312
#define LE_CComment 313
#define LE_CPPComment 314
#define LE_NAMESPACE 315
#define LE_USING 316
#define LE_TYPEDEFname 317
#define LE_ARROW 318
#define LE_ICR 319
#define LE_DECR 320
#define LE_LS 321
#define LE_RS 322
#define LE_LE 323
#define LE_GE 324
#define LE_EQ 325
#define LE_NE 326
#define LE_ANDAND 327
#define LE_OROR 328
#define LE_ELLIPSIS 329
#define LE_CLCL 330
#define LE_DOTstar 331
#define LE_ARROWstar 332
#define LE_MULTassign 333
#define LE_DIVassign 334
#define LE_MODassign 335
#define LE_PLUSassign 336
#define LE_MINUSassign 337
#define LE_LSassign 338
#define LE_RSassign 339
#define LE_ANDassign 340
#define LE_ERassign 341
#define LE_ORassign 342
#define LE_MACRO 343
#define LE_DYNAMIC_CAST 344
#define LE_STATIC_CAST 345
#define LE_CONST_CAST 346
#define LE_REINTERPRET_CAST 347
#define YYERRCODE 256
short cl_expr_lhs[] = {                                        -1,
    0,    0,    3,    1,    1,    4,    4,    5,    5,    5,
    5,    5,    5,    5,    5,    5,    6,    6,    6,    7,
    7,    7,    2,    2,    2,    2,    2,    2,    2,   13,
   14,   14,   15,   15,   11,   11,   11,   11,   17,   17,
   18,   18,    9,   10,   10,   10,   12,   12,    8,    8,
   19,   16,   16,
};
short cl_expr_len[] = {                                         2,
    0,    2,    0,    2,    1,    0,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    0,    1,    3,    4,
    4,    7,    6,    2,    3,    3,    6,    5,    8,    2,
    0,    3,    0,    1,    1,    1,    1,    1,    0,    1,
    0,    2,    2,    0,    1,    1,    6,    3,    0,    2,
    2,    0,    1,
};
short cl_expr_defred[] = {                                      1,
    0,    5,    2,    0,   45,   46,    4,    0,   24,   35,
   36,   37,   38,    0,    0,    0,    0,   49,    0,    0,
   25,    0,   26,    0,    0,   50,   49,    0,    0,   41,
   53,   30,   51,    0,    0,    0,   41,    0,   48,    0,
    0,    7,   49,    0,   18,   34,   28,    0,    0,    0,
   42,   40,   43,   27,    0,    0,   32,   23,    0,   41,
   13,    8,   11,    9,   12,   10,   15,   14,   16,    0,
   41,   19,   29,   47,    0,   20,   21,    0,   41,   22,
};
short cl_expr_dgoto[] = {                                       1,
    3,    7,    4,   43,   71,   44,   45,   19,   39,    8,
   17,   20,   23,   35,   47,   32,   53,   40,   26,
};
short cl_expr_sindex[] = {                                      0,
 -240,    0,    0,   15,    0,    0,    0,  -17,    0,    0,
    0,    0,    0,   -5, -247, -287,  -23,    0, -251,   21,
    0,  -35,    0,  -91,   17,    0,    0,   34,  -58,    0,
    0,    0,    0, -195,   42,   22,    0, -195,    0,  -11,
 -226,    0,    0,  -24,    0,    0,    0,   43, -226,  -22,
    0,    0,    0,    0, -178, -195,    0,    0,   49,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  -57,
    0,    0,    0,    0, -195,    0,    0,    8,    0,    0,
};
short cl_expr_rindex[] = {                                      0,
  -25,    0,    0,  -21,    0,    0,    0, -213,    0,    0,
    0,    0,    0, -213,    0,    0,    0,    0,    0,    0,
    0,    1,    0,    5,    9,    0,    0,    0,  -12,    0,
    0,    0,    0,  -38,   13,    0,    0,  -38,    0,  -34,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0, -170,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  -30,
    0,    0,    0,    0,  -38,    0,    0,    0,    0,    0,
};
short cl_expr_gindex[] = {                                      0,
    0,    0,    0,    0,    0,  -27,   38,   -4,   16,    0,
    0,   39,   20,    0,    0,    0,    0,    0,    0,
};
#define YYTABLESIZE 360
short cl_expr_table[] = {                                      31,
   52,   38,   75,   16,   52,   17,   39,   41,   31,   39,
   50,   41,   33,   41,    3,    2,    3,   24,   44,   56,
   44,   56,   14,   17,   15,   41,   52,   39,   41,   41,
   51,   41,    3,    3,   18,   25,   27,   57,   55,   60,
   52,   52,   52,   21,   52,   41,   52,   78,   31,   41,
   31,   56,   49,   29,   33,   31,   28,   22,   52,   52,
   54,   30,   52,   52,   52,   36,   31,   31,   59,   79,
   33,   33,    6,    5,   37,   74,   34,   42,   22,   61,
   62,   46,   58,   48,   63,   76,   77,    6,    6,   73,
   64,   49,    6,   72,   80,   65,   66,   67,    6,    0,
   68,   69,    0,    6,    6,    6,    0,    0,    6,    6,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   70,    0,    0,    0,
    0,    0,    0,    0,    6,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    6,
    6,    0,    0,    0,    6,    0,    0,    0,    0,    0,
    6,    0,    0,    0,    0,    6,    6,    6,   33,    0,
    6,    6,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,   52,    0,    0,    0,
   52,    0,    0,    0,   31,    3,    6,    0,   33,   44,
   39,   33,   33,    9,    0,    0,    0,    0,    0,    3,
    0,    0,    0,   44,    0,    0,    0,    0,    0,    0,
    0,   52,    0,    0,    0,   52,    0,    0,    0,   31,
    0,    0,    0,   33,    0,   52,    0,    0,    0,   52,
    0,    0,    0,   31,    0,    0,    0,   33,    3,    3,
    3,    3,   44,   44,   44,   44,   10,   11,   12,   13,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   52,   52,   52,   52,   52,   52,
   52,   52,   31,   31,   31,   31,   33,   33,   33,   33,
};
short cl_expr_check[] = {                                      91,
    0,   60,   60,    8,    0,   44,   41,   38,    0,   44,
   38,   42,    0,   44,   40,  256,   42,  305,   40,   44,
   42,   44,   40,   62,   42,   38,   38,   62,   41,   42,
   42,   62,   58,   59,   40,   16,   60,   62,   43,   62,
   40,   41,   42,  291,   40,   30,   42,   75,   40,   62,
   42,   44,   37,  305,   42,   91,   18,  305,   58,   59,
   41,   41,   58,   59,   60,   27,   58,   59,   49,   62,
   58,   59,   58,   59,   41,   60,   60,  273,  305,  258,
  259,   40,   40,   62,  263,   70,   71,  258,  259,   41,
  269,  305,  263,   56,   79,  274,  275,  276,  269,   -1,
  279,  280,   -1,  274,  275,  276,   -1,   -1,  279,  280,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  305,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  305,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  258,
  259,   -1,   -1,   -1,  263,   -1,   -1,   -1,   -1,   -1,
  269,   -1,   -1,   -1,   -1,  274,  275,  276,  330,   -1,
  279,  280,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,  256,   -1,   -1,   -1,
  256,   -1,   -1,   -1,  256,  291,  305,   -1,  256,  291,
  305,  330,  330,  291,   -1,   -1,   -1,   -1,   -1,  305,
   -1,   -1,   -1,  305,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,  291,   -1,   -1,   -1,  291,   -1,   -1,   -1,  291,
   -1,   -1,   -1,  291,   -1,  305,   -1,   -1,   -1,  305,
   -1,   -1,   -1,  305,   -1,   -1,   -1,  305,  344,  345,
  346,  347,  344,  345,  346,  347,  344,  345,  346,  347,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  344,  345,  346,  347,  344,  345,
  346,  347,  344,  345,  346,  347,  344,  345,  346,  347,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 1
#endif
#define YYMAXTOKEN 347
#if YYDEBUG
char *cl_expr_name[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,"'&'",0,"'('","')'","'*'",0,"','",0,0,0,0,0,0,0,0,0,0,0,0,0,"':'","';'",
"'<'",0,"'>'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'['",0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,"LE_AUTO","LE_DOUBLE","LE_INT","LE_STRUCT","LE_BREAK","LE_ELSE","LE_LONG",
"LE_SWITCH","LE_CASE","LE_ENUM","LE_REGISTER","LE_TYPEDEF","LE_CHAR",
"LE_EXTERN","LE_RETURN","LE_UNION","LE_CONST","LE_FLOAT","LE_SHORT",
"LE_UNSIGNED","LE_CONTINUE","LE_FOR","LE_SIGNED","LE_VOID","LE_DEFAULT",
"LE_GOTO","LE_SIZEOF","LE_VOLATILE","LE_DO","LE_IF","LE_STATIC","LE_WHILE",
"LE_NEW","LE_DELETE","LE_THIS","LE_OPERATOR","LE_CLASS","LE_PUBLIC",
"LE_PROTECTED","LE_PRIVATE","LE_VIRTUAL","LE_FRIEND","LE_INLINE","LE_OVERLOAD",
"LE_TEMPLATE","LE_TYPENAME","LE_THROW","LE_CATCH","LE_IDENTIFIER",
"LE_STRINGliteral","LE_FLOATINGconstant","LE_INTEGERconstant",
"LE_CHARACTERconstant","LE_OCTALconstant","LE_HEXconstant","LE_POUNDPOUND",
"LE_CComment","LE_CPPComment","LE_NAMESPACE","LE_USING","LE_TYPEDEFname",
"LE_ARROW","LE_ICR","LE_DECR","LE_LS","LE_RS","LE_LE","LE_GE","LE_EQ","LE_NE",
"LE_ANDAND","LE_OROR","LE_ELLIPSIS","LE_CLCL","LE_DOTstar","LE_ARROWstar",
"LE_MULTassign","LE_DIVassign","LE_MODassign","LE_PLUSassign","LE_MINUSassign",
"LE_LSassign","LE_RSassign","LE_ANDassign","LE_ERassign","LE_ORassign",
"LE_MACRO","LE_DYNAMIC_CAST","LE_STATIC_CAST","LE_CONST_CAST",
"LE_REINTERPRET_CAST",
};
char *cl_expr_rule[] = {
"$accept : translation_unit",
"translation_unit :",
"translation_unit : translation_unit primary_expr",
"$$1 :",
"primary_expr : $$1 simple_expr",
"primary_expr : error",
"const_spec :",
"const_spec : LE_CONST",
"basic_type_name : LE_INT",
"basic_type_name : LE_CHAR",
"basic_type_name : LE_SHORT",
"basic_type_name : LE_LONG",
"basic_type_name : LE_FLOAT",
"basic_type_name : LE_DOUBLE",
"basic_type_name : LE_SIGNED",
"basic_type_name : LE_UNSIGNED",
"basic_type_name : LE_VOID",
"parameter_list :",
"parameter_list : template_parameter",
"parameter_list : parameter_list ',' template_parameter",
"template_parameter : const_spec nested_scope_specifier LE_IDENTIFIER special_star_amp",
"template_parameter : const_spec nested_scope_specifier basic_type_name special_star_amp",
"template_parameter : const_spec nested_scope_specifier LE_IDENTIFIER '<' parameter_list '>' special_star_amp",
"simple_expr : stmnt_starter special_cast '<' cast_type '>' '('",
"simple_expr : stmnt_starter LE_THIS",
"simple_expr : stmnt_starter '*' LE_THIS",
"simple_expr : stmnt_starter '*' identifier_name",
"simple_expr : stmnt_starter '(' cast_type ')' special_star_amp identifier_name",
"simple_expr : stmnt_starter nested_scope_specifier identifier_name optional_template_init_list optinal_postifx",
"simple_expr : stmnt_starter '(' '(' cast_type ')' special_star_amp identifier_name ')'",
"identifier_name : LE_IDENTIFIER array_brackets",
"optional_template_init_list :",
"optional_template_init_list : '<' parameter_list '>'",
"optinal_postifx :",
"optinal_postifx : '('",
"special_cast : LE_DYNAMIC_CAST",
"special_cast : LE_STATIC_CAST",
"special_cast : LE_CONST_CAST",
"special_cast : LE_REINTERPRET_CAST",
"amp_item :",
"amp_item : '&'",
"star_list :",
"star_list : star_list '*'",
"special_star_amp : star_list amp_item",
"stmnt_starter :",
"stmnt_starter : ';'",
"stmnt_starter : ':'",
"cast_type : nested_scope_specifier LE_IDENTIFIER '<' parameter_list '>' special_star_amp",
"cast_type : nested_scope_specifier LE_IDENTIFIER special_star_amp",
"nested_scope_specifier :",
"nested_scope_specifier : nested_scope_specifier scope_specifier",
"scope_specifier : LE_IDENTIFIER LE_CLCL",
"array_brackets :",
"array_brackets : '['",
};
#endif
#ifndef YYSTYPE
typedef int YYSTYPE;
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 500
#define YYMAXDEPTH 500
#endif
#endif
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short yyss[YYSTACKSIZE];
YYSTYPE yyvs[YYSTACKSIZE];
#define yystacksize YYSTACKSIZE
void yyerror(char *s) {}

void expr_consumBracketsContent(char openBrace)
{
	char closeBrace;
	
	switch(openBrace) {
	case '(': closeBrace = ')'; break;
	case '[': closeBrace = ']'; break;
	case '<': closeBrace = '>'; break;
	case '{': closeBrace = '}'; break;
	default:
		openBrace = '(';
		closeBrace = ')';
		break;
	}
	
	int depth = 1;
	while(depth > 0)
	{
		int ch = cl_expr_lex();
		if(ch == 0){
			break;
		}
		
		if(ch == closeBrace)
		{
			depth--;
			continue;
		}
		else if(ch == openBrace)
		{
			depth ++ ;
			continue;
		}
	}
}

void expr_FuncArgList()
{
	int depth = 1;
	while(depth > 0)
	{
		int ch = cl_expr_lex();
		//printf("ch=%d\n", ch);
		//fflush(stdout);
		if(ch ==0){
			break;
		}
		
		if(ch == ')')
		{
			depth--;
			continue;
		}
		else if(ch == '(')
		{
			depth ++ ;
			continue;
		}
	}
}

void expr_consumeTemplateDecl()
{
	int depth = 1;
	while(depth > 0)
	{
		int ch = cl_expr_lex();
		//printf("ch=%d\n", ch);
		fflush(stdout);
		if(ch ==0){
			break;
		}
		
		if(ch == '>')
		{
			depth--;
			continue;
		}
		else if(ch == '<')
		{
			depth ++ ;
			continue;
		}
	}
}

void expr_syncParser(){
	//dont do anything, a hook to allow us to implement some
	//nice error recovery if needed
}

// return the scope name at the end of the input string
ExpressionResult &parse_expression(const std::string &in)
{
	result.reset();
	//provide the lexer with new input
	if( !setExprLexerInput(in) ){
		return result;
	}
	
	//printf("parsing...\n");
	cl_expr_parse();
	//do the lexer cleanup
	cl_expr_lex_clean();
	
	return result;
}
#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse()
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register char *yys;
	
    extern char *getenv(const char *name);

    if (yys = getenv("YYDEBUG"))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }

#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if (yyn = yydefred[yystate]) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yyss + yystacksize - 1)
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#ifdef lint
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#ifdef lint
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yyss + yystacksize - 1)
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 3:
{result.reset();}
break;
case 5:
{ 
								yyclearin;	/*clear lookahead token*/
								yyerrok;
								/*fprintf(stderr, "CodeLite: syntax error, unexpected token '%s' found at line %d \n", cl_expr_text, cl_expr_lineno);*/
								/*fflush(stderr);*/
								expr_syncParser();
						}
break;
case 6:
{yyval = ""; }
break;
case 7:
{ yyval = yyvsp[0]; }
break;
case 8:
{ yyval = yyvsp[0]; }
break;
case 9:
{ yyval = yyvsp[0]; }
break;
case 10:
{ yyval = yyvsp[0]; }
break;
case 11:
{ yyval = yyvsp[0]; }
break;
case 12:
{ yyval = yyvsp[0]; }
break;
case 13:
{ yyval = yyvsp[0]; }
break;
case 14:
{ yyval = yyvsp[0]; }
break;
case 15:
{ yyval = yyvsp[0]; }
break;
case 16:
{ yyval = yyvsp[0]; }
break;
case 17:
{yyval = "";}
break;
case 18:
{yyval = yyvsp[0];}
break;
case 19:
{yyval = yyvsp[-2] + yyvsp[-1] + yyvsp[0];}
break;
case 20:
{yyval = yyvsp[-3] + " " + yyvsp[-2] + " " + yyvsp[-1] +yyvsp[0];}
break;
case 21:
{yyval = yyvsp[-3] + " " + yyvsp[-2] + " " + yyvsp[-1] +yyvsp[0];}
break;
case 22:
{yyval = yyvsp[-6] + " " + yyvsp[-5] + " " + yyvsp[-4] +yyvsp[-3] + yyvsp[-2] + yyvsp[-1];}
break;
case 23:
{
						expr_FuncArgList(); 
						yyval = yyvsp[-2];
						result.m_isaType = true;
						result.m_name = yyvsp[-2];
						result.m_isFunc = false;
						/*printf("Rule 1\n");	*/
					}
break;
case 24:
{
						yyval = yyvsp[0];
						result.m_isaType = false;
						result.m_name = yyval;
						result.m_isFunc = false;
						result.m_isThis = true;
						result.m_isPtr = true;
						/*printf("Rule 2\n");	*/
					}
break;
case 25:
{
						yyval = yyvsp[0];
						result.m_isaType = false;
						result.m_name = yyval;
						result.m_isFunc = false;
						result.m_isThis = true;
						/*printf("Rule 3\n");		*/
					}
break;
case 26:
{
						yyval = yyvsp[0];
						result.m_isaType = false;
						result.m_name = yyval;
						result.m_isFunc = false;
						result.m_isThis = false;
						result.m_isPtr = false;
						/*printf("Rule 4\n");		*/
					}
break;
case 27:
{
						yyval = yyvsp[-3];
						result.m_isaType = true;
						result.m_name = yyval;
						result.m_isFunc = false;
						result.m_isThis = false;
						/*printf("Rule 5\n");		*/
					}
break;
case 28:
{
						result.m_isaType = false;
						result.m_name = yyvsp[-2];
						result.m_isThis = false;
						yyvsp[-3].erase(yyvsp[-3].find_last_not_of(":")+1);
						result.m_scope = yyvsp[-3];
						result.m_isTemplate = yyvsp[-1].empty() ? false : true;
						result.m_templateInitList = yyvsp[-1];
						/*printf("Rule 6\n");		*/
					}
break;
case 29:
{
						yyval = yyvsp[-4];
						result.m_isaType = true;
						result.m_name = yyval;
						result.m_isFunc = false;
						result.m_isThis = false;
						/*printf("Rule 7\n");		*/
					}
break;
case 30:
{yyval = yyvsp[-1];}
break;
case 31:
{yyval = "";}
break;
case 32:
{yyval = yyvsp[-2] + yyvsp[-1] + yyvsp[0];}
break;
case 33:
{yyval = "";}
break;
case 34:
{
						yyval = yyvsp[0]; 
						expr_FuncArgList();
						result.m_isFunc = true;
					}
break;
case 35:
{yyval = yyvsp[0];}
break;
case 36:
{yyval = yyvsp[0];}
break;
case 37:
{yyval = yyvsp[0];}
break;
case 38:
{yyval = yyvsp[0];}
break;
case 39:
{yyval = ""; }
break;
case 40:
{ yyval = yyvsp[0]; }
break;
case 41:
{yyval = ""; }
break;
case 42:
{yyval = yyvsp[-1] + yyvsp[0];}
break;
case 43:
{ yyval = yyvsp[-1] + yyvsp[0]; }
break;
case 44:
{yyval = "";}
break;
case 45:
{ yyval = ";";}
break;
case 46:
{ yyval = ":";}
break;
case 47:
{
					yyval = yyvsp[-5] + yyvsp[-4]; 
					yyvsp[-5].erase(yyvsp[-5].find_last_not_of(":")+1);
					result.m_scope = yyvsp[-5]; 
					result.m_name = yyvsp[-4];
					result.m_isPtr = (yyvsp[0].find("*") != (size_t)-1);;
					result.m_isTemplate = true;
					result.m_templateInitList = yyvsp[-3] + yyvsp[-2] + yyvsp[-1];
				}
break;
case 48:
{
					yyval = yyvsp[-2] + yyvsp[-1]; 
					yyvsp[-2].erase(yyvsp[-2].find_last_not_of(":")+1);
					result.m_scope = yyvsp[-2]; 
					result.m_name = yyvsp[-1];
					result.m_isPtr = (yyvsp[0].find("*") != (size_t)-1);;
				}
break;
case 49:
{yyval = "";}
break;
case 50:
{	yyval = yyvsp[-1] + yyvsp[0];}
break;
case 51:
{yyval = yyvsp[-1]+ yyvsp[0];}
break;
case 52:
{ yyval = ""; }
break;
case 53:
{ expr_consumBracketsContent('['); yyval = "[]";}
break;
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yyss + yystacksize - 1)
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
