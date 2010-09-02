/*
Copyright (c) 2009 Peter Andrew Jamieson (jamieson.peter@gmail.com)

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/ 
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include "globals.h"
#include "types.h"
#include "errors.h"
#include "ast_util.h"
#include "odin_util.h"
#include "util.h"

/*---------------------------------------------------------------------------
 * (function: create_node_w_type)
 *-------------------------------------------------------------------------*/
ast_node_t* create_node_w_type(short id, int line_number, int file_number)
{
	static long unique_count = 0;

//	oassert(unique_count != 198);

	ast_node_t* new_node;

	new_node = (ast_node_t*)calloc(1, sizeof(ast_node_t));
	oassert(new_node != NULL);
	new_node->type = id;
	
	new_node->children = NULL;
	new_node->num_children = 0;

	new_node->line_number = line_number;
	new_node->file_number = file_number;
	new_node->unique_count = unique_count++;

	new_node->far_tag = 0;
	new_node->high_number = 0;

	return new_node;
}

/*---------------------------------------------------------------------------------------------
 * (function: free_child_in_tree)
 * frees all children below too
 *-------------------------------------------------------------------------------------------*/
void free_child_in_tree(ast_node_t *from, int idx_removal)
{
	ast_node_t *child = from->children[idx_removal];
	int i;
	
	if ((child == NULL) || (child->shared_node == TRUE))
		return;
	
	/* free all it's children .... and so on recursively */
	for (i = 0; i < child->num_children; i++)
	{
		free_child_in_tree(child->children[i], i);	
	}
	
	if (child->children != NULL)
		free(child->children);

	switch(child->type)
	{
		case IDENTIFIERS:
			if (child->types.identifier != NULL)
				free(child->types.identifier);
			break;
		case NUMBERS:
			if (child->types.number.number != NULL)
				free(child->types.number.number);
			break;
	}

	free(child);	
	from->children[idx_removal] = NULL;
}

/*---------------------------------------------------------------------------------------------
 * (function: free_ast_node)
 *-------------------------------------------------------------------------------------------*/
void free_ast_node(ast_node_t *child)
{
	int i;
	
	if ((child == NULL) || (child->shared_node == TRUE))
		return;
	
	/* free all it's children .... and so on recursively */
	for (i = 0; i < child->num_children; i++)
	{
		free_child_in_tree(child, i);	
	}
	
	if (child->children != NULL)
		free(child->children);

	switch(child->type)
	{
		case IDENTIFIERS:
			if (child->types.identifier != NULL)
				free(child->types.identifier);
			break;
		case NUMBERS:
			if (child->types.number.number != NULL)
				free(child->types.number.number);
			break;
	}

	free(child);	
}

/*---------------------------------------------------------------------------------------------
 * (function: free_ast_node_only)
 *-------------------------------------------------------------------------------------------*/
void free_ast_node_only(ast_node_t *child)
{
	if (child->children != NULL)
		free(child->children);

	switch(child->type)
	{
		case IDENTIFIERS:
			if (child->types.identifier != NULL)
				free(child->types.identifier);
			break;
		case NUMBERS:
			if (child->types.number.number != NULL)
				free(child->types.number.number);
			break;
	}

	free(child);	
}

/*---------------------------------------------------------------------------------------------
 * (function: create_tree_node_id)
 *-------------------------------------------------------------------------------------------*/
ast_node_t* create_tree_node_id(char* string, int line_number, int file_number)
{
	ast_node_t* new_node = create_node_w_type(IDENTIFIERS, line_number, current_parse_file);
	new_node->types.identifier = string;

	return new_node;
}

/*---------------------------------------------------------------------------------------------
 * (function: *create_tree_node_long_long_number)
 *-------------------------------------------------------------------------------------------*/
ast_node_t *create_tree_node_long_long_number(long long number, int line_number, int file_number)
{
	ast_node_t* new_node = create_node_w_type(NUMBERS, line_number, current_parse_file);
	new_node->types.number.base = LONG_LONG;
	new_node->types.number.value = number;

	return new_node;
}

/*---------------------------------------------------------------------------------------------
 * (function: create_tree_node_number)
 *-------------------------------------------------------------------------------------------*/
ast_node_t *create_tree_node_number(char* number, int line_number, int file_number)
{
	ast_node_t* new_node = create_node_w_type(NUMBERS, line_number, current_parse_file);
	char *string_pointer = number;
	int index_string_pointer = 0;
	char *temp_string;
	short flag_constant_decimal = FALSE;

	/* find the ' character if it's a base */
	for (string_pointer=number; *string_pointer; string_pointer++) 
	{
		if (*string_pointer == '\'') 
		{
			break;
		}
		index_string_pointer++;
	}

	if (index_string_pointer == strlen(number))
	{
		flag_constant_decimal = TRUE;
		/* this is base d */
		new_node->types.number.base = DEC;
		/* reset to the front */
		string_pointer = number;

		/* size is the rest of the string */
		new_node->types.number.size = strlen((string_pointer));
		/* assign the remainder of the number to a string */
		new_node->types.number.number = strdup((string_pointer));
	}	
	else
	{
		/* there is a base in the form: number[bhod]'number */
		switch(tolower(*(string_pointer+1)))
		{
			case 'd':
				new_node->types.number.base = DEC;
				break;
			case 'h':
				new_node->types.number.base = HEX;
				break;
			case 'o':
				new_node->types.number.base = OCT;
				break;
			case 'b':
				new_node->types.number.base = BIN;
				break;
			default:
				printf("Not a number\n");
				oassert(FALSE);
		}

		/* check if the size matches the design specified size */
		temp_string = strdup(number); 
		temp_string[index_string_pointer] = '\0';
		/* size is the rest of the string */
		new_node->types.number.size = atoi(temp_string); 
		free(temp_string);
	
		/* move to the digits */
		string_pointer += 2;

		/* assign the remainder of the number to a string */
		new_node->types.number.number = strdup((string_pointer));
	}

	/* check for decimal numbers without the formal 2'd... format */
	if (flag_constant_decimal == FALSE)
	{
		/* size describes how may bits */
		new_node->types.number.binary_size = new_node->types.number.size;
	}
	else
	{
		/* size is for a constant that needs */
		if (strcmp(new_node->types.number.number, "0") != 0)
		{
			new_node->types.number.binary_size = ceil((log(convert_dec_string_of_size_to_long(new_node->types.number.number, new_node->types.number.size)+1))/log(2));
		}
		else
		{
			new_node->types.number.binary_size = 1;
		}
	}
	/* add in the values for all the numbers */
	switch (new_node->types.number.base)
	{
		case(DEC):
			new_node->types.number.value = convert_dec_string_of_size_to_long(new_node->types.number.number, new_node->types.number.size);
			new_node->types.number.binary_string = convert_long_to_bit_string(new_node->types.number.value, new_node->types.number.binary_size);
			break;
		case(HEX):
			new_node->types.number.value = convert_hex_string_of_size_to_long(new_node->types.number.number, strlen(new_node->types.number.number));
			new_node->types.number.binary_string = convert_long_to_bit_string(new_node->types.number.value, new_node->types.number.binary_size);
			break;
		case(OCT):
			new_node->types.number.value = convert_oct_string_of_size_to_long(new_node->types.number.number, strlen(new_node->types.number.number)); 
			new_node->types.number.binary_string = convert_long_to_bit_string(new_node->types.number.value, new_node->types.number.binary_size);
			break;
		case(BIN):
			new_node->types.number.value = convert_binary_string_of_size_to_long(new_node->types.number.number,  strlen(new_node->types.number.number));// a -1 is in case of x or z's
			new_node->types.number.binary_string = convert_long_to_bit_string(new_node->types.number.value, new_node->types.number.binary_size);
			break;
	}

	return new_node;
}

/*---------------------------------------------------------------------------
 * (function: allocate_children_to_node)
 *-------------------------------------------------------------------------*/
void allocate_children_to_node(ast_node_t* node, int num_children, ...) 
{
	va_list ap;
	int i;

	/* allocate space for the children */
	node->children = (ast_node_t**)malloc(sizeof(ast_node_t*)*num_children);
	node->num_children = num_children;
	
	/* set the virtual arguments */
	va_start(ap, num_children);

	for (i = 0; i < num_children; i++)
	{
		node->children[i] = va_arg(ap, ast_node_t*);
	}
}

/*---------------------------------------------------------------------------------------------
 * (function: add_child_to_node)
 *-------------------------------------------------------------------------------------------*/
void add_child_to_node(ast_node_t* node, ast_node_t *child) 
{
	/* allocate space for the children */
	node->children = (ast_node_t**)realloc(node->children, sizeof(ast_node_t*)*(node->num_children+1));
	node->num_children ++;
	node->children[node->num_children-1] = child;
}

/*---------------------------------------------------------------------------------------------
 * (function: get_range)
 *-------------------------------------------------------------------------------------------*/
int get_range(ast_node_t* first_node) 
{
	long temp_value;
	/* look at the first item to see if it has a range */
	if (first_node->children[1] != NULL)
	{
		/* IF the first element in the list has a second element...that is the range */
		oassert(first_node->children[2] != NULL); // the third element should be a value
		oassert((first_node->children[1]->type == NUMBERS) && (first_node->children[2]->type == NUMBERS)); // should be numbers
		if(first_node->children[1]->types.number.value < first_node->children[2]->types.number.value)
		{
			/* swap them around */
			temp_value = first_node->children[1]->types.number.value;
			first_node->children[1]->types.number.value = first_node->children[2]->types.number.value;
			first_node->children[2]->types.number.value = temp_value;
		}

		return abs(first_node->children[1]->types.number.value - first_node->children[2]->types.number.value + 1); // 1:0 is 2 spots
	}
	return -1; // indicates no range
}

/*---------------------------------------------------------------------------------------------
 * (function: make_concat_into_list_of_strings)
 * 	0th idx will be the MSbit
 *-------------------------------------------------------------------------------------------*/
void make_concat_into_list_of_strings(ast_node_t *concat_top)
{
	int i, j; 

	concat_top->types.concat.num_bit_strings = 0;
	concat_top->types.concat.bit_strings = NULL;

	/* recursively do all embedded concats */
	for (i = 0; i < concat_top->num_children; i++)
	{
		if (concat_top->children[i]->type == CONCATENATE)
		{
			make_concat_into_list_of_strings(concat_top->children[i]);
		}
	}
		
	for (i = 0; i < concat_top->num_children; i++)
	{
		if (concat_top->children[i]->type == IDENTIFIERS)
		{
			char *temp_string = make_full_ref_name(NULL, NULL, NULL, concat_top->children[i]->types.identifier, -1);
			long sc_spot;

			if ((sc_spot = sc_lookup_string(local_symbol_table_sc, temp_string)) == -1)
			{
				error_message(NETLIST_ERROR, concat_top->line_number, concat_top->file_number, "Missing declaration of this symbol %s\n", temp_string);
			}
			free(temp_string);

			if (((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[1] == NULL)
			{
				concat_top->types.concat.num_bit_strings ++;
				concat_top->types.concat.bit_strings = (char**)realloc(concat_top->types.concat.bit_strings, sizeof(char*)*(concat_top->types.concat.num_bit_strings));
				concat_top->types.concat.bit_strings[concat_top->types.concat.num_bit_strings-1] = get_name_of_pin_at_bit(concat_top->children[i], -1);
			}
			else if (((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[3] == NULL)
			{
				/* reverse thorugh the range since highest bit in index will be lower in the string indx */
				for (j = ((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[1]->types.number.value - ((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[2]->types.number.value; j >= 0; j--)
				{
					concat_top->types.concat.num_bit_strings ++;
					concat_top->types.concat.bit_strings = (char**)realloc(concat_top->types.concat.bit_strings, sizeof(char*)*(concat_top->types.concat.num_bit_strings));
					concat_top->types.concat.bit_strings[concat_top->types.concat.num_bit_strings-1] = get_name_of_pin_at_bit(concat_top->children[i], j);
				}
			}
			else if (((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[3] != NULL)
			{
				oassert(FALSE);	
			}
	
		}
		else if (concat_top->children[i]->type == ARRAY_REF)
		{
			concat_top->types.concat.num_bit_strings ++;
			concat_top->types.concat.bit_strings = (char**)realloc(concat_top->types.concat.bit_strings, sizeof(char*)*(concat_top->types.concat.num_bit_strings));
			concat_top->types.concat.bit_strings[concat_top->types.concat.num_bit_strings-1] = get_name_of_pin_at_bit(concat_top->children[i], 0);
		}
		else if (concat_top->children[i]->type == RANGE_REF)
		{
			oassert(concat_top->children[i]->children[1]->types.number.value >= concat_top->children[i]->children[2]->types.number.value);
			/* reverse thorugh the range since highest bit in index will be lower in the string indx */
			for (j = concat_top->children[i]->children[1]->types.number.value - concat_top->children[i]->children[2]->types.number.value; j >= 0; j--)
			{
				concat_top->types.concat.num_bit_strings ++;
				concat_top->types.concat.bit_strings = (char**)realloc(concat_top->types.concat.bit_strings, sizeof(char*)*(concat_top->types.concat.num_bit_strings));
				concat_top->types.concat.bit_strings[concat_top->types.concat.num_bit_strings-1] = get_name_of_pin_at_bit(concat_top->children[i], ((concat_top->children[i]->children[1]->types.number.value - concat_top->children[i]->children[2]->types.number.value))-j);
			}
		}
		else if (concat_top->children[i]->type == NUMBERS)
		{
			if(concat_top->children[i]->types.number.base == DEC)
			{
				error_message(NETLIST_ERROR, concat_top->line_number, concat_top->file_number, "Concatenation can't include decimal numbers due to conflict on bits\n");
			}

			/* forward through list since 0th bit of a number (get_name_of_pin) is the msb */
			for (j = 0; j < concat_top->children[i]->types.number.binary_size; j++)
			{
				concat_top->types.concat.num_bit_strings ++;
				concat_top->types.concat.bit_strings = (char**)realloc(concat_top->types.concat.bit_strings, sizeof(char*)*(concat_top->types.concat.num_bit_strings));
				concat_top->types.concat.bit_strings[concat_top->types.concat.num_bit_strings-1] = get_name_of_pin_at_bit(concat_top->children[i], j);
			}
		}
		else if (concat_top->children[i]->type == CONCATENATE)
		{
			/* forward through list since we build concatenate list in idx order of MSB at index 0 and LSB at index list_size */
			for (j = 0; j < concat_top->children[i]->types.concat.num_bit_strings; j++)
			{
				concat_top->types.concat.num_bit_strings ++;
				concat_top->types.concat.bit_strings = (char**)realloc(concat_top->types.concat.bit_strings, sizeof(char*)*(concat_top->types.concat.num_bit_strings));
				concat_top->types.concat.bit_strings[concat_top->types.concat.num_bit_strings-1] = get_name_of_pin_at_bit(concat_top->children[i], j);
			}
		}
	}
}

/*---------------------------------------------------------------------------------------------
 * (function: get_name of_port_at_bit)
 * 	Assume module connections can be one of: Array entry, Concat, Signal, Array range reference
 *-------------------------------------------------------------------------------------------*/
char *get_name_of_var_declare_at_bit(ast_node_t *var_declare, int bit)
{
	char *return_string; 

	/* calculate the port details */
	if (var_declare->children[1] == NULL)
	{
		oassert(bit == 0);
		return_string = make_full_ref_name(NULL, NULL, NULL, var_declare->children[0]->types.identifier, -1);
	}
	else if (var_declare->children[3] == NULL)
	{
		return_string = make_full_ref_name(NULL, NULL, NULL, var_declare->children[0]->types.identifier, var_declare->children[2]->types.number.value+bit);
	}
	else if (var_declare->children[3] != NULL)
	{
		/* MEMORY output */
		oassert(FALSE);
	}
	
	return return_string;
}

/*---------------------------------------------------------------------------------------------
 * (function: get_name of_port_at_bit)
 * 	Assume module connections can be one of: Array entry, Concat, Signal, Array range reference
 *-------------------------------------------------------------------------------------------*/
char *get_name_of_pin_at_bit(ast_node_t *var_node, int bit)
{
	char *return_string; 

	if (var_node->type == ARRAY_REF)
	{
		return_string = make_full_ref_name(NULL, NULL, NULL, var_node->children[0]->types.identifier, (int)var_node->children[1]->types.number.value);
	}
	else if (var_node->type == RANGE_REF)
	{
		oassert((var_node->children[2]->types.number.value+bit <= var_node->children[1]->types.number.value) && (var_node->children[2]->types.number.value+bit >= var_node->children[2]->types.number.value));
		return_string = make_full_ref_name(NULL, NULL, NULL, var_node->children[0]->types.identifier, var_node->children[2]->types.number.value+bit);
	}
	else if ((var_node->type == IDENTIFIERS) && (bit == -1))
	{
		return_string = make_full_ref_name(NULL, NULL, NULL, var_node->types.identifier, -1);
	}
	else if (var_node->type == IDENTIFIERS)
	{
		long sc_spot;
		int pin_index;

		if ((sc_spot = sc_lookup_string(local_symbol_table_sc, var_node->types.identifier)) == -1)
		{
			error_message(NETLIST_ERROR, var_node->line_number, var_node->file_number, "Missing declaration of this symbol %s\n", var_node->types.identifier);
		}

		if (((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[1] == NULL)
		{
			pin_index = bit;
		}
		else if (((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[3] == NULL)
		{
			pin_index = ((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[2]->types.number.value + bit; 
		}
		else
			oassert(FALSE);

		return_string = make_full_ref_name(NULL, NULL, NULL, var_node->types.identifier, pin_index);
	}
	else if (var_node->type == NUMBERS)
	{
		if (bit == -1)
			bit = 0;

		oassert(bit < var_node->types.number.binary_size);
		if (var_node->types.number.binary_string[var_node->types.number.binary_size-bit-1] == '1')
		{
			return_string = (char*)malloc(sizeof(char)*11+1); // ONE_VCC_CNS	
			sprintf(return_string, "ONE_VCC_CNS");
		}
		else if (var_node->types.number.binary_string[var_node->types.number.binary_size-bit-1] == '0')
		{
			return_string = (char*)malloc(sizeof(char)*13+1); // ZERO_GND_ZERO	
			sprintf(return_string, "ZERO_GND_ZERO");
		}
		else
		{
			oassert(FALSE);
		}
	}
	else if (var_node->type == CONCATENATE)
	{
		if (var_node->types.concat.num_bit_strings == 0)
		{
			oassert(FALSE);
		}
		else
		{
			if (var_node->types.concat.num_bit_strings == -1)
			{
				/* If this hasn't been made into a string list then do it */
				make_concat_into_list_of_strings(var_node);
			}

			return_string = (char*)malloc(sizeof(char)*strlen(var_node->types.concat.bit_strings[bit])+1);
			sprintf(return_string, "%s", var_node->types.concat.bit_strings[bit]);
		}
	}
	else
	{
		oassert(FALSE);
	}
	
	return return_string;
}

/*---------------------------------------------------------------------------------------------
 * (function: get_name_of_pins
 * 	Assume module connections can be one of: Array entry, Concat, Signal, Array range reference
 * 	Return a list of strings
 *-------------------------------------------------------------------------------------------*/
char_list_t *get_name_of_pins(ast_node_t *var_node)
{
	char **return_string; 
	char_list_t *return_list = (char_list_t*)malloc(sizeof(char_list_t));

	int i;
	int width;

	if (var_node->type == ARRAY_REF)
	{
		width = 1;
		return_string = (char**)malloc(sizeof(char*));
		return_string[0] = make_full_ref_name(NULL, NULL, NULL, var_node->children[0]->types.identifier, (int)var_node->children[1]->types.number.value);
	}
	else if (var_node->type == RANGE_REF)
	{
		width = (var_node->children[1]->types.number.value - var_node->children[2]->types.number.value+1);
		return_string = (char**)malloc(sizeof(char*)*width);
		for (i = 0; i < width; i++)
		{
			return_string[i] = make_full_ref_name(NULL, NULL, NULL, var_node->children[0]->types.identifier, var_node->children[2]->types.number.value+i);
		}
	}
	else if (var_node->type == IDENTIFIERS)
	{
		/* need to look in the symbol table for details about this identifier (i.e. is it a port) */
		long sc_spot;
		char *temp_string = make_full_ref_name(NULL, NULL, NULL, var_node->types.identifier, -1);

		if ((sc_spot = sc_lookup_string(local_symbol_table_sc, temp_string)) == -1)
		{
			error_message(NETLIST_ERROR, var_node->line_number, var_node->file_number, "Missing declaration of this symbol %s\n", temp_string);
		}
		free(temp_string);

		if (((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[1] == NULL)
		{
			width = 1;
			return_string = (char**)malloc(sizeof(char*)*width);
			return_string[0] = make_full_ref_name(NULL, NULL, NULL, var_node->types.identifier, -1);
		}
		else if (((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[3] == NULL)
		{
			int index = 0;
			width = ((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[1]->types.number.value - ((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[2]->types.number.value + 1;
			return_string = (char**)malloc(sizeof(char*)*width);
			for (i = 0; i < width; i++)
//			for (i = 0; i < ((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[2]->types.number.value; i < ((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[1]->types.number.value+1; i++)
			{
				return_string[index] = make_full_ref_name(NULL, NULL, NULL, var_node->types.identifier, i+((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[2]->types.number.value);
				index++;
			}
		}
		else if (((ast_node_t*)local_symbol_table_sc->data[sc_spot])->children[3] != NULL)
		{
			oassert(FALSE);	
		}
	}
	else if (var_node->type == NUMBERS)
	{
		width = var_node->types.number.binary_size;
		return_string = (char**)malloc(sizeof(char*)*width);
		for (i = 0; i < width; i++)
		{
			/* strings are msb is 0th index in string, reverse access */
			if (var_node->types.number.binary_string[var_node->types.number.binary_size-i-1] == '1')
			{
				return_string[i] = (char*)malloc(sizeof(char)*11+1); // ONE_VCC_CNS	
				sprintf(return_string[i], "ONE_VCC_CNS");
			}
			else if (var_node->types.number.binary_string[var_node->types.number.binary_size-i-1] == '0')
			{
				return_string[i] = (char*)malloc(sizeof(char)*13+1); // ZERO_GND_ZERO	
				sprintf(return_string[i], "ZERO_GND_ZERO");
			}
			else
			{
				oassert(FALSE);
			}
		}
	}
	else if (var_node->type == CONCATENATE)
	{
		if (var_node->types.concat.num_bit_strings == 0)
		{
			oassert(FALSE);
		}
		else
		{
			if (var_node->types.concat.num_bit_strings == -1)
			{
				/* If this hasn't been made into a string list then do it */
				make_concat_into_list_of_strings(var_node);
			}

			width = var_node->types.concat.num_bit_strings;
			return_string = (char**)malloc(sizeof(char*)*width);
			for (i = 0; i < width; i++) // 0th bit is MSB so need to access reverse
			{
				return_string[i] = (char*)malloc(sizeof(char)*strlen(var_node->types.concat.bit_strings[var_node->types.concat.num_bit_strings-i-1])+1);
				sprintf(return_string[i], "%s", var_node->types.concat.bit_strings[var_node->types.concat.num_bit_strings-i-1]);
			}
		}
	}
	else
	{
		oassert(FALSE);
	}
	
	return_list->strings = return_string;
	return_list->num_strings = width;

	return return_list;
}

/*---------------------------------------------------------------------------------------------
 * (function: get_name_of_pins_with_prefix
 *-------------------------------------------------------------------------------------------*/
char_list_t *get_name_of_pins_with_prefix(ast_node_t *var_node, char *instance_name_prefix)
{
	int i;
	char_list_t *return_list;

	/* get the list */
	return_list = get_name_of_pins(var_node);

	for (i = 0; i < return_list->num_strings; i++)
	{
		return_list->strings[i] = make_full_ref_name(instance_name_prefix, NULL, NULL, return_list->strings[i], -1);
	}

	return return_list;
}
