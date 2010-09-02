/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     vSYMBOL_ID = 258,
     vNUMBER_ID = 259,
     vALWAYS = 260,
     vAND = 261,
     vASSIGN = 262,
     vBEGIN = 263,
     vCASE = 264,
     vDEFAULT = 265,
     vDEFINE = 266,
     vELSE = 267,
     vEND = 268,
     vENDCASE = 269,
     vENDMODULE = 270,
     vIF = 271,
     vINOUT = 272,
     vINPUT = 273,
     vMODULE = 274,
     vNAND = 275,
     vNEGEDGE = 276,
     vNOR = 277,
     vNOT = 278,
     vOR = 279,
     vOUTPUT = 280,
     vPARAMETER = 281,
     vPOSEDGE = 282,
     vREG = 283,
     vWIRE = 284,
     vXNOR = 285,
     vXOR = 286,
     vDEFPARAM = 287,
     voANDAND = 288,
     voOROR = 289,
     voLTE = 290,
     voGTE = 291,
     voSLEFT = 292,
     voSRIGHT = 293,
     voEQUAL = 294,
     voNOTEQUAL = 295,
     voCASEEQUAL = 296,
     voCASENOTEQUAL = 297,
     voXNOR = 298,
     voNAND = 299,
     voNOR = 300,
     vNOT_SUPPORT = 301,
     LOWER_THAN_ELSE = 302
   };
#endif
/* Tokens.  */
#define vSYMBOL_ID 258
#define vNUMBER_ID 259
#define vALWAYS 260
#define vAND 261
#define vASSIGN 262
#define vBEGIN 263
#define vCASE 264
#define vDEFAULT 265
#define vDEFINE 266
#define vELSE 267
#define vEND 268
#define vENDCASE 269
#define vENDMODULE 270
#define vIF 271
#define vINOUT 272
#define vINPUT 273
#define vMODULE 274
#define vNAND 275
#define vNEGEDGE 276
#define vNOR 277
#define vNOT 278
#define vOR 279
#define vOUTPUT 280
#define vPARAMETER 281
#define vPOSEDGE 282
#define vREG 283
#define vWIRE 284
#define vXNOR 285
#define vXOR 286
#define vDEFPARAM 287
#define voANDAND 288
#define voOROR 289
#define voLTE 290
#define voGTE 291
#define voSLEFT 292
#define voSRIGHT 293
#define voEQUAL 294
#define voNOTEQUAL 295
#define voCASEEQUAL 296
#define voCASENOTEQUAL 297
#define voXNOR 298
#define voNAND 299
#define voNOR 300
#define vNOT_SUPPORT 301
#define LOWER_THAN_ELSE 302




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 61 "SRC/verilog_bison.y"
{
	char *id_name;
	char *num_value;
	ast_node_t *node;
}
/* Line 1489 of yacc.c.  */
#line 149 "SRC/verilog_bison.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

