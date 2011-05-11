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
     vDELAY_ID = 260,
     vALWAYS = 261,
     vAND = 262,
     vASSIGN = 263,
     vBEGIN = 264,
     vCASE = 265,
     vDEFAULT = 266,
     vDEFINE = 267,
     vELSE = 268,
     vEND = 269,
     vENDCASE = 270,
     vENDMODULE = 271,
     vIF = 272,
     vINOUT = 273,
     vINPUT = 274,
     vMODULE = 275,
     vNAND = 276,
     vNEGEDGE = 277,
     vNOR = 278,
     vNOT = 279,
     vOR = 280,
     vOUTPUT = 281,
     vPARAMETER = 282,
     vPOSEDGE = 283,
     vREG = 284,
     vWIRE = 285,
     vXNOR = 286,
     vXOR = 287,
     vDEFPARAM = 288,
     voANDAND = 289,
     voOROR = 290,
     voLTE = 291,
     voGTE = 292,
     voSLEFT = 293,
     voSRIGHT = 294,
     voEQUAL = 295,
     voNOTEQUAL = 296,
     voCASEEQUAL = 297,
     voCASENOTEQUAL = 298,
     voXNOR = 299,
     voNAND = 300,
     voNOR = 301,
     vNOT_SUPPORT = 302,
     LOWER_THAN_ELSE = 303
   };
#endif
/* Tokens.  */
#define vSYMBOL_ID 258
#define vNUMBER_ID 259
#define vDELAY_ID 260
#define vALWAYS 261
#define vAND 262
#define vASSIGN 263
#define vBEGIN 264
#define vCASE 265
#define vDEFAULT 266
#define vDEFINE 267
#define vELSE 268
#define vEND 269
#define vENDCASE 270
#define vENDMODULE 271
#define vIF 272
#define vINOUT 273
#define vINPUT 274
#define vMODULE 275
#define vNAND 276
#define vNEGEDGE 277
#define vNOR 278
#define vNOT 279
#define vOR 280
#define vOUTPUT 281
#define vPARAMETER 282
#define vPOSEDGE 283
#define vREG 284
#define vWIRE 285
#define vXNOR 286
#define vXOR 287
#define vDEFPARAM 288
#define voANDAND 289
#define voOROR 290
#define voLTE 291
#define voGTE 292
#define voSLEFT 293
#define voSRIGHT 294
#define voEQUAL 295
#define voNOTEQUAL 296
#define voCASEEQUAL 297
#define voCASENOTEQUAL 298
#define voXNOR 299
#define voNAND 300
#define voNOR 301
#define vNOT_SUPPORT 302
#define LOWER_THAN_ELSE 303




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 61 "SRC/verilog_bison.y"
{
	char *id_name;
	char *num_value;
	ast_node_t *node;
}
/* Line 1529 of yacc.c.  */
#line 151 "SRC/verilog_bison.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

