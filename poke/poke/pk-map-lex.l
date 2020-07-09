/* pk-map-lex.l - Lexer for the poke mapfiles parser.  */

/* Copyright (C) 2020 Jose E. Marchesi */

/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Automake needs this.  */
%option outfile="lex.yy.c"
%option prefix="pk_map_tab_"
%option warn nodefault
%option pointer
%option reentrant
%option noyywrap
%option bison-bridge
%option header-file="pk-map-lex.h"
%option bison-locations
%option noinput
%option nounput
%option extra-type="struct pk_map_parser *"

%top {
     /* This code goes at the "top" of the generated file.  */
     #include <config.h>
}

%{
#include "xalloc.h"

#include "pk-utils.h" /* For STREQ */
#include "pk-map-parser.h"
#define YYLTYPE PK_MAP_TAB_LTYPE
#define YYSTYPE PK_MAP_TAB_STYPE
#include "pk-map-tab.h"

#define YY_FATAL_ERROR(msg)                             \
  do                                                    \
    {                                                   \
        /* YYLTYPE *loc = yyget_lloc (yyscanner);*/             \
     /* XXX YY_EXTRA_TYPE extra = yyget_extra (yyscanner); */   \
     /* Use location.  */                               \
     printf ("%s", msg);                                \
     abort ();                                          \
     /* kludge to avoid compiler warning */             \
     void *x = yy_fatal_error; x = x;                   \
    }                                                   \
  while (0)

#define YY_USER_ACTION                                  \
  do                                                    \
    {                                                   \
     int i;                                             \
                                                        \
     yylloc->first_line = yylloc->last_line;            \
     yylloc->first_column = yylloc->last_column;        \
                                                        \
     for (i = 0; yytext[i] != '\0'; i++)                \
       {                                                \
         if (yytext[i] == '\n')                         \
           {                                            \
             yylloc->last_line++;                       \
             yylloc->last_column = 1;                   \
           }                                            \
         else                                           \
           yylloc->last_column++;                       \
       }                                                \
                                                        \
     yyextra->nchars += yyleng;                         \
    } while (0);

static int
strtotag (const char *str)
{
    if (STREQ (str, "name"))
      return TAG_NAME;
    else if (STREQ (str, "type"))
      return TAG_TYPE;
    else if (STREQ (str, "offset"))
      return TAG_OFFSET;
    else if (STREQ (str, "condition"))
      return TAG_CONDITION;

    return TAG_UNKNOWN;
}

%}

%x C_DATA

COMMENT                   ^#[^\n]*\n
NEWLINE            \n
BLANK              [ \t]
TAGNAME                   %[a-zA-Z_][a-zA-Z0-9_]*

%%
                if (!yyextra->once) { BEGIN (C_DATA); yyextra->once = 1; }

<C_DATA>[^%]*        {
        yylval->string = xstrdup (yytext);
        BEGIN (INITIAL);
        return DATA;
}

{NEWLINE}        { /* Ignore newlines.  */  }
{BLANK}                { /* Ignore whitespace.  */ }
{COMMENT}         { /* Comment.  */ }

"%%"                { return SEP; }
"%entry"        { return ENTRY; }
{TAGNAME}        {
        yylval->integer = strtotag (yytext + 1);
        BEGIN (C_DATA);
        return TAG;
}

<*>.                { return ERROR; }
