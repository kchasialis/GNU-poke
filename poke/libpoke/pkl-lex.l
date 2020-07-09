/* pkl-lex.l - Lexer for Poke.  */

/* Copyright (C) 2019, 2020 Jose E. Marchesi */

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
%option prefix="pkl_tab_"
%option warn nodefault
%option pointer
%option noyywrap
%option reentrant
%option bison-bridge
%option header-file="pkl-lex.h"
%option bison-locations
%option noinput
%option nounput
   /* %option yylineno */
%option extra-type="struct pkl_parser *"

%top {
   /* This code goes at the "top" of the generated file.  */
   #include <config.h>
}

%{
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include "pkl.h"
#include "pkl-ast.h"
#include "pkl-env.h"
#include "pkl-diag.h"
#include "pkl-parser.h"
#define YYSTYPE PKL_TAB_STYPE
#define YYLTYPE pkl_ast_loc
#define YYDEBUG 1
#include <pkl-tab.h>

#define YY_FATAL_ERROR(msg)                             \
  do                                                    \
    {                                                   \
     YYLTYPE *loc = yyget_lloc (yyscanner);             \
     YY_EXTRA_TYPE extra = yyget_extra (yyscanner);     \
     pkl_ice (extra->compiler, extra->ast, *loc, "%s",  \
              msg);                                     \
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

/* Note that the following function assumes that STR is a pointer of a
   string that satisfies the regexp
   ({HEXCST}|{BINCST}|{OCTCST}|{DECCST}) */

int
pkl_lex_get_base  (const char *str, int *offset)
{
 int base = 10;
 *offset = 0;

 if (str[0] == '0')
  {
    if (str[1] == 'x' || str[1] == 'X')
    {
      base = 16;
      *offset = 2;
    }
    else if (str[1] == 'b' || str[1] == 'B')
    {
      base = 2;
      *offset = 2;
    }
    else if (str[1] == 'o' || str[1] == 'O')
    {
      base = 8;
      *offset = 2;
    }
  }

 return base;
}

%}

NEWLINE            \n
BLANK              [ \t]
LETTER             [a-zA-Z]
FIELD_NAME         {LETTER}[a-zA-Z0-9_]*
CHAR                   '(.|\\[ntr\\]|\\[0-7]|\\[0-7][0-7]|\\[0-7][0-7][0-7])'
STRING             \"([^"]|\\(.|\n))*\"
HEXCST                   0[xX][0-9a-fA-F][0-9a-fA-F_]*
BINCST                   0[bB][01][01_]*
OCTCST             0[oO][0-7_]*
DECCST             [0-9][0-9_]*
IS                   ((u|U)|(u|U)?(l|L|B|h|H|n|N)|(l|L|B|h|H|n|N)(u|U))

L [a-zA-Z_]
D [0-9]
A $
S ::

%x C_COMMENT
%x SHEBANG_COMMENT

%%

%{
  /* Handle the support for multiple parsers.  */

  if (yyextra->start_token)
  {
    int t = yyextra->start_token;
    yyextra->start_token = 0;
    return t;
  }
%}

{NEWLINE}        { /* Ignore newlines.  */  }
{BLANK}                { /* Ignore whitespace.  */ }
\f                { /* Ignore form feed ^L characters.  */ }

"/*" { BEGIN(C_COMMENT); }
<C_COMMENT>"*/" { BEGIN(INITIAL); }
<C_COMMENT>.    {  }
<C_COMMENT>\n   {  }

"#!" { BEGIN(SHEBANG_COMMENT); }
<SHEBANG_COMMENT>"!#" { BEGIN(INITIAL); }
<SHEBANG_COMMENT>.    {  }
<SHEBANG_COMMENT>\n   {  }

"//"[^\n]*        { /* Partial-line comment.  */ }

"__LINE__"        {
  pkl_ast_node type
    = pkl_ast_make_integral_type (yyextra->ast, 64, 0);
  yylval->ast = pkl_ast_make_integer (yyextra->ast,
                                      yylloc->first_line);
  PKL_AST_TYPE (yylval->ast) = ASTREF (type);

  return INTEGER;
}

"__FILE__"        {
  pkl_ast_node type
    = pkl_ast_make_string_type (yyextra->ast);

  yylval->ast = pkl_ast_make_string (yyextra->ast,
                                     yyextra->filename
                                     ? yyextra->filename
                                     : "<stdin>");
  PKL_AST_TYPE (yylval->ast) = ASTREF (type);
  return STR;
 }

"pinned"        { return PINNED; }
"struct"        { return STRUCT; }
"union"                { return UNION; }
"else"                { return ELSE; }
"while"                { return WHILE; }
"until"                { return UNTIL; }
"for"                { return FOR; }
"in"                { return IN; }
"where"                { return WHERE; }
"if"                { return IF; }
"sizeof"        { return SIZEOF; }
"defun"                { return DEFUN; }
"method"        { return METHOD; }
"deftype"        { return DEFTYPE; }
"defvar"        { return DEFVAR; }
"defunit"        { return DEFUNIT; }
"break"                { return BREAK; }
"return"        { return RETURN; }
"string"        { return STRING; }
"as"                { return AS; }
"try"                { return TRY; }
"catch"                { return CATCH; }
"raise"                { return RAISE; }
"void"                { return VOID; }
"any"                { return ANY; }
"print"                { return PRINT; }
"printf"        { return PRINTF; }
"isa"                { return ISA; }
"unmap"                { return UNMAP; }
"big"                { return BIG; }
"little"        { return LITTLE; }
"load"                { return LOAD; }
"__PKL_BUILTIN_RAND__" {
   if (yyextra->bootstrapped) REJECT; return BUILTIN_RAND; }
"__PKL_BUILTIN_GET_ENDIAN__" {
   if (yyextra->bootstrapped) REJECT; return BUILTIN_GET_ENDIAN; }
"__PKL_BUILTIN_SET_ENDIAN__" {
   if (yyextra->bootstrapped) REJECT; return BUILTIN_SET_ENDIAN; }
"__PKL_BUILTIN_GET_IOS__" {
   if (yyextra->bootstrapped) REJECT; return BUILTIN_GET_IOS; }
"__PKL_BUILTIN_SET_IOS__" {
   if (yyextra->bootstrapped) REJECT; return BUILTIN_SET_IOS; }
"__PKL_BUILTIN_OPEN__" {
   if (yyextra->bootstrapped) REJECT; return BUILTIN_OPEN; }
"__PKL_BUILTIN_CLOSE__" {
   if (yyextra->bootstrapped) REJECT; return BUILTIN_CLOSE; }
"__PKL_BUILTIN_IOSIZE__" {
   if (yyextra->bootstrapped) REJECT; return BUILTIN_IOSIZE; }
"__PKL_BUILTIN_GETENV__" {
   if (yyextra->bootstrapped) REJECT; return BUILTIN_GETENV; }

"uint<"         { return UINTCONSTR; }
"int<"          { return INTCONSTR; }
"offset<"       { return OFFSETCONSTR; }

"..."                { return THREEDOTS; }

"*="                { return MULA; }
"/="                { return DIVA; }
"%="                { return MODA; }
"+="                { return ADDA; }
"-="                { return SUBA; }
"<-="                { return SLA; }
"->="                { return SRA; }
"&="                { return BANDA; }
"|="                { return IORA; }
"^="                { return XORA; }
":::"                { return BCONC; }

"||"                { return OR; }
"&&"                { return AND; }
"=="                { return EQ; }
"!="                { return NE; }
"<="                { return LE; }
">="                { return GE; }
"<<."                { return SL; }
".>>"                { return SR; }
"++"                { return INC; }
"--"                { return DEC; }

"["                { return '['; }
"]"                { return ']'; }
"("                { return '('; }
")"                { return ')'; }
"{"                { return '{'; }
"}"                { return '}'; }
","                { return ','; }
"="                { return '='; }
"?"                { return '?'; }
";"                { return ';'; }
":"                { return ':'; }
"|"                { return '|'; }
"^"                { return '^'; }
"&"                { return '&'; }
"<"                { return '<'; }
">"                { return '>'; }
"+"                { return '+'; }
"-"                { return '-'; }
"**"                { return POW; }
"*"                { return '*'; }
"/^"                { return CEILDIV; }
"/"                { return '/'; }
"%"                { return '%'; }
"!"                { return '!'; }
"~"                { return '~'; }
"."                { return '.'; }
"@"                { return '@'; }

{A}({L}|{D})({L}|{D}|({S}({L}|{D})))* {
  char *errmsg;
  pkl_alien_token_handler_fn cb
    = pkl_alien_token_fn (yyextra->compiler);

  if (pkl_lexical_cuckolding_p (yyextra->compiler)
      && cb != NULL)
    {
      char *id = (*cb) (yytext + 1, &errmsg);

      if (id)
        {
          yylval->ast = pkl_ast_make_identifier (yyextra->ast,
                                                 id);
          free (id);
          return IDENTIFIER;
        }

      /* Error from alien handler.  */
      yyextra->alien_errmsg = errmsg;
      return ALIEN;
    }

  /* Lexical cuckolding is not enabled.  */
  yylloc->last_column -= strlen (yytext);
  REJECT;
}

'{L}({L}|{D})* {
  yylval->ast = pkl_ast_make_identifier (yyextra->ast, yytext + 1);
  return ATTR;
}

#({HEXCST}|{BINCST}|{OCTCST}|{DECCST}) {
  char *end;
  int base, offset;
  uint64_t value;
  pkl_ast_node type
     = pkl_ast_make_integral_type (yyextra->ast, 64, 0);

  base = pkl_lex_get_base (yytext + 1, &offset);
  value = strtoll (yytext + 1 + offset, &end, base);
  yylval->ast = pkl_ast_make_integer (yyextra->ast, value);
  PKL_AST_TYPE (yylval->ast) = ASTREF (type);

  return UNIT;
}

#{L}({L}|{D})* {
   pkl_ast_node decl
      = pkl_env_lookup (yyextra->env, PKL_ENV_NS_UNITS,
                        yytext + 1, NULL, NULL);

   if (decl)
     yylval->ast = PKL_AST_DECL_INITIAL (decl);
   else
     {
       decl = pkl_env_lookup (yyextra->env, PKL_ENV_NS_MAIN,
                              yytext + 1, NULL, NULL);

       if (decl && PKL_AST_DECL_KIND (decl) == PKL_AST_DECL_KIND_TYPE)
         yylval->ast = PKL_AST_DECL_INITIAL (decl);
       else
         yylval->ast = NULL;
     }

   return UNIT;
}

{L}({L}|{D})* {
   pkl_ast_node decl
     = pkl_env_lookup (yyextra->env, PKL_ENV_NS_MAIN,
                       yytext, NULL, NULL);

   yylval->ast = pkl_ast_make_identifier (yyextra->ast, yytext);

   if (decl && PKL_AST_DECL_KIND (decl) == PKL_AST_DECL_KIND_TYPE)
     return TYPENAME;
   else
     return IDENTIFIER;
}

({HEXCST}|{BINCST}|{OCTCST}|{DECCST}){IS}? {
  uint64_t value;
  int base, offset, signed_p, width;
  char *p, *end;
  pkl_ast_node type;

  /* First of all, strip any '_' character from yytext.  */
  for (p = yytext; *p != '\0'; p++)
  {
    char *tmp;

    while (*p == '_')
     for (tmp = p; *tmp != '\0'; tmp++)
       *tmp = *(tmp + 1);
  }

  base = pkl_lex_get_base (yytext, &offset);

  /* strtoull can fail here only in case of overflow.  */
  errno = 0;
  value = strtoull (yytext + offset, &end, base);
  if (value == ULLONG_MAX && errno == ERANGE)
        return INTEGER_OVERFLOW;

  signed_p = 1;
  if (*end == 'u' || *end == 'U'
      || (*end != '\0' && (*(end + 1) == 'u' || *(end + 1) == 'U')))
    signed_p = 0;

  width = 0;
  if (*end == 'l' || *end == 'L'
      || ((*end != '\0') && ((*(end + 1) == 'l' || *(end + 1) == 'L'))))
    width = 64;
  else  if (*end == 'h' || *end == 'H'
      || ((*end != '\0') && ((*(end + 1) == 'h' || *(end + 1) == 'H'))))
    width = 16;
  else  if (*end == 'B' || ((*end != '\0') && *(end + 1) == 'B'))
    width = 8;
  else if (*end == 'n' || *end == 'N'
      || ((*end != '\0') && ((*(end + 1) == 'n' || *(end + 1) == 'N'))))
    width = 4;

  /* If not specified with the 'l' or 'L' suffix, the type of the
     integer constant is the smallest signed or unsigned integer
     capable of holding it, starting with 32 bits, in steps of power
     of two and up to 64 bits.  But note these are positive values! */

  if (width == 0)
  {
    if (value >= 0x0000000080000000 && value <= 0x00000000ffffffff)
      width = signed_p ? 64 : 32;
    else if (value & 0xffffffff80000000)
      width = 64;
    else
      width = 32;
  }

  type = pkl_ast_make_integral_type (yyextra->ast, width, signed_p);

  yylval->ast = pkl_ast_make_integer (yyextra->ast, value);
  PKL_AST_TYPE (yylval->ast) = ASTREF (type);

  return INTEGER;
}

{CHAR} {
   uint8_t value;
   pkl_ast_node type;

   if (yytext[1] == '\\')
    {
      if (yytext[2] >= '0' && yytext[2] <= '9')
        {
          char *end;
          uint64_t val64;

          /* strtoll can't fail in this context.  */
          val64 = strtoll (yytext + 2, &end, 8);
          value = (uint8_t) val64;
        }
      else
        switch (yytext[2])
          {
          case 'n':
            value = '\n';
            break;
          case 't':
            value = '\t';
            break;
          case 'r':
            value = '\r';
            break;
          case '\\':
            value = '\\';
            break;
          default:
            /* To avoid compiler warnings.  */
            value = 0;
            break;
          }
    }
   else
     value = yytext[1];

   type = pkl_ast_make_integral_type (yyextra->ast, 8, 0);
   PKL_AST_TYPE_COMPLETE (type) = PKL_AST_TYPE_COMPLETE_YES;

   yylval->ast = pkl_ast_make_integer (yyextra->ast, value);
   PKL_AST_TYPE (yylval->ast) = ASTREF (type);

   return CHAR;
}

{STRING} {
   pkl_ast_node type;

   /* Strip the quoting characters */
   yytext[strlen (yytext) - 1] = 0;

   type = pkl_ast_make_string_type (yyextra->ast);

   yylval->ast = pkl_ast_make_string (yyextra->ast, yytext + 1);
   PKL_AST_TYPE (yylval->ast) = ASTREF (type);

   return STR;
}

. { return ERR; }

%%

/*
Local variables:
mode:c
End:
*/
