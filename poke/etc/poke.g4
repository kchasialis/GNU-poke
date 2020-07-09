/* -*- mode: antlr -*- */
/* poke.g4 - LL Poke grammar for Antlr4  */

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

grammar poke;

/*** Lexer rules ***/

fragment IS :  ([uU]|[uU]?[lLBhHnN]|[lLBhHnN][uU]) ;
fragment HEXCST : '0'[xX][0-9a-fA-F][0-9a-fA-F_]* ;
fragment BINCST : '0'[bB][01][01_]* ;
fragment OCTCST : '0'[oO][0-7_]* ;
fragment DECCST : [0-9][0-9_]* ;
fragment CST : HEXCST | BINCST | OCTCST | DECCST ;
fragment NUMBER : CST IS? ;
fragment L : [a-zA-Z] ;
fragment D : [0-9] ;

ATTR: L(L|D)*
    ;

UNIT: '#'(HEXCST|BINCST|OCTCST|DECCST)
    ;

IDENTIFIER: L(L|D)*
    ;

TYPENAME: L(L|D)*
    ;

INTEGER:
      '__FILE__'
    | '__LINE__'
    | NUMBER
    ;

CHAR: .
    | '\\' ([ntr]|[0-7]|[0-7][0-7]|[0-7][0-7][0-7])
    ;

STRING: '"' ([^"]|(.|'\n'))* '"'
    ;

/*** Parser rules ***/

program:
        /* Empty */
	| program_elem_list
    ;

program_elem_list:
        program_elem
    | program_elem_list program_elem
	;

program_elem:
        declaration
    | stmt
    ;

/*
 * Identifiers.
 */

identifier:
        TYPENAME
    | IDENTIFIER
    ;

/*
 * Expressions.
 */

expression:
        primary
    | unary_operator expression /* %prec UNARY */
	| 'sizeof' '(' simple_type_specifier ')' /* %prec HYPERUNARY */
    | expression ATTR
    | expression '+' expression
    | expression '-' expression
    | expression '*' expression
    | expression '/' expression
	| expression '/^' expression
    | expression '%' expression
    | expression '<<.' expression
    | expression '.>>' expression
    | expression '==' expression
	| expression '!=' expression
    | expression '<' expression
    | expression '>' expression
    | expression '<=' expression
	| expression '>=' expression
    | expression '|' expression
    | expression '^' expression
	| expression '&' expression
    | expression '&&' expression
	| expression '||' expression
	| expression 'in' expression
	| expression 'as' simple_type_specifier
	| expression 'isa' simple_type_specifier
    | TYPENAME '{' struct_field_list opt_comma '}'
    | UNIT
    | expression UNIT
	| struct
    /* The next rule is bconc, but we cannot use the bconc rule
       because that introduces indirect left-recursivity that cannot
       be handled by antlr4.  */
	| expression '::' expression
    | map
    ;

bconc:
        expression '::' expression
    ;

map:
        simple_type_specifier '@' expression /* %prec THEN */
	| simple_type_specifier '@' expression ':' expression /* %prec 'else' */
    ;

unary_operator: '-' | '+' | '~'	| '!' | 'unmap'	;

primary:
      IDENTIFIER
	| INTEGER
    | CHAR
    | STRING
    | '(' expression ')'
    | array
    | primary '.' identifier
    | primary '[' expression ']' /* %prec '.'*/
    | primary '[' expression ':' expression ']' /* %prec '.' */
    | primary '[' ':' ']' /* %prec '.' */
    | primary '[' ':' expression ']' /* %prec '.' */
	| primary '[' expression ':' ']' /* %prec '.' */
    | primary '(' funcall_arg_list ')'  /*%prec '.' */
	;

funcall_arg_list:
        /* Empty */
	| funcall_arg
    | funcall_arg_list ',' funcall_arg
    ;

funcall_arg:
        expression
    ;

opt_comma:
        /* Empty */
	| ','
    ;

struct:
        'struct' '{' struct_field_list opt_comma '}'
	;

struct_field_list:
        /* Empty */
    | struct_field
    | struct_field_list ',' struct_field
    ;

struct_field:
        expression
    | identifier '=' expression
    ;

array:
        '[' array_initializer_list opt_comma ']'
	;

array_initializer_list:
        array_initializer
    | array_initializer_list ',' array_initializer
    ;

array_initializer:
        expression
    | '.' '[' INTEGER ']' '=' expression
    ;

/*
 * Functions.
 */

function_specifier:
        '(' function_arg_list ')' simple_type_specifier ':' comp_stmt
	| simple_type_specifier ':' comp_stmt
    ;

function_arg_list:
        function_arg
    | function_arg ',' function_arg_list
    ;

function_arg:
        simple_type_specifier identifier function_arg_initial
	| identifier '...'
    ;

function_arg_initial:
        /* Empty */
	| '=' expression
    ;

/*
 * Types.
 */

type_specifier:
        simple_type_specifier
    | struct_type_specifier
    | function_type_specifier
    ;

simple_type_specifier:
        TYPENAME
    | 'any'
	| 'void'
    | 'string'
	| integral_type_specifier
	| offset_type_specifier
    | simple_type_specifier '[' ']'
	| simple_type_specifier '[' expression ']'
    ;

integral_type_specifier:
        integral_type_sign INTEGER '>'
	;

integral_type_sign:
      'int<'
    | 'uint<'
	;

offset_type_specifier:
      'offset<' simple_type_specifier ',' IDENTIFIER '>'
    | 'offset<' simple_type_specifier ',' simple_type_specifier '>'
    | 'offset<' simple_type_specifier ',' INTEGER '>'
	;

function_type_specifier
    : '(' function_type_arg_list ')' simple_type_specifier
    | '(' ')' simple_type_specifier
	;

function_type_arg_list
	: function_type_arg
    | function_type_arg ',' function_type_arg_list
	;

function_type_arg:
        simple_type_specifier
    | simple_type_specifier '?'
    | '...'
	;

struct_type_specifier:
        struct_type_pinned struct_or_union '{' '}'
    | struct_type_pinned struct_or_union '{' struct_type_elem_list '}'
    ;

struct_or_union:
        'struct'
	| 'union'
    ;

struct_type_pinned:
        /* Empty */
	| 'pinned'
    ;

struct_type_elem_list:
        struct_type_field
    | declaration
    | struct_type_elem_list declaration
    | struct_type_elem_list struct_type_field
    ;

endianness:
        /* Empty */
	| 'little'
	| 'big'
    ;

struct_type_field:
        endianness type_specifier struct_type_field_identifier
        struct_type_field_constraint struct_type_field_label ';'
    ;

struct_type_field_identifier:
        /* Empty */
	| identifier
	;

struct_type_field_label:
        /* Empty */
    | '@' expression
	;

struct_type_field_constraint:
        /* Empty */
    | ':' expression
    ;

/*
 * Declarations.
 */

declaration:
        'defun' identifier '=' function_specifier
    | 'defvar' identifier '=' expression ';'
    | 'deftype' identifier '=' type_specifier ';'
    ;

/*
 * Statements.
 */

comp_stmt:
        '{' '}'
    | '{' stmt_decl_list '}'
    | builtin
    ;

builtin:
      '__PKL_BUILTIN_RAND__'
	| '__PKL_BUILTIN_GET_ENDIAN__'
	| '__PKL_BUILTIN_SET_ENDIAN__'
    | '__PKL_BUILTIN_GET_IOS__'
    | '__PKL_BUILTIN_SET_IOS__'
	| '__PKL_BUILTIN_OPEN__'
	| '__PKL_BUILTIN_CLOSE__'
	;

stmt_decl_list:
        stmt
    | stmt_decl_list stmt
    | declaration
    | stmt_decl_list declaration
	;

stmt:
        comp_stmt
    | ';'
    | primary '=' expression ';'
	| bconc '=' expression ';'
	| map '=' expression ';'
    | 'if' '(' expression ')' stmt /* %prec THEN */
    | 'if' '(' expression ')' stmt 'else' stmt /* %prec 'else' */
	| 'while' '(' expression ')' stmt
	| 'for' '(' IDENTIFIER 'in' expression ')' stmt
	| 'for' '(' IDENTIFIER 'in' expression 'where' expression ')' stmt
    | 'break' ';'
    | 'return' ';'
    | 'return' expression ';'
    | 'try' stmt 'catch' comp_stmt
	| 'try' stmt 'catch' 'if' expression comp_stmt
	| 'try' stmt 'catch'  '(' function_arg ')' comp_stmt
    | 'try' stmt 'until' expression ';'
	| 'raise' ';'
	| 'raise' expression ';'
    | expression ';'
    | 'print' expression ';'
	| 'printf' STRING print_stmt_arg_list ';'
    | 'printf' '(' STRING print_stmt_arg_list ')' ';'
	| funcall_stmt ';'
    ;

print_stmt_arg_list:
        /* Empty */
    | print_stmt_arg_list ',' expression
	;

funcall_stmt:
        primary funcall_stmt_arg_list
	;

funcall_stmt_arg_list:
        funcall_stmt_arg
    | funcall_stmt_arg_list funcall_stmt_arg
    ;

funcall_stmt_arg:
        ':' IDENTIFIER expression
	;
