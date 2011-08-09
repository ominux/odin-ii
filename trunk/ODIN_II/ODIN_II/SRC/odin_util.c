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
#include "types.h"
#include "globals.h"
#include "errors.h"
#include "odin_util.h"

/*--------------------------------------------------------------------------
 * (function: make_signal_name)
// return signal_name-bit
 *------------------------------------------------------------------------*/
char *make_signal_name(char *signal_name, int bit)
{
	char *return_string;

	oassert(signal_name != NULL);
	if (bit == -1)
		return strdup(signal_name);

	return_string = strdup(signal_name);
	return_string = (char*)realloc(return_string, sizeof(char)*(strlen(return_string)+1+10+1));
	sprintf(return_string, "%s-%d", return_string, bit);
	return return_string;	
}

/*---------------------------------------------------------------------------------------------
 * (function: make_full_ref_name)
// {previous_string}.module_name+instance_name
// {previous_string}.module_name+instance_name^signal_name
// {previous_string}.module_name+instance_name^signal_name~bit
 *-------------------------------------------------------------------------------------------*/
char *make_full_ref_name(char *previous, char *module_name, char *module_instance_name, char *signal_name, int bit)
{
	char *return_string;
	return_string = (char*)malloc(sizeof(char)*1);
	return_string[0] = '\0';

	if (previous != NULL)
	{
		return_string = (char*)realloc(return_string, sizeof(char)*(strlen(previous)+1+1));
		sprintf(return_string, "%s", previous);
	}
	if (module_name != NULL)
	{
		return_string = (char*)realloc(return_string, sizeof(char)*(strlen(return_string)+1+strlen(module_name)+1+strlen(module_instance_name)+1));
		sprintf(return_string, "%s.%s+%s", return_string, module_name, module_instance_name);
	}
	if ((signal_name != NULL) && ((previous != NULL) || ((module_name != NULL))))
	{
		return_string = (char*)realloc(return_string, sizeof(char)*(strlen(return_string)+1+strlen(signal_name)+1));
		strcat(return_string, "^");
		strcat(return_string, signal_name);
	}
	else if (signal_name != NULL) 
	{
		return_string = (char*)realloc(return_string, sizeof(char)*(strlen(return_string)+1+strlen(signal_name)+1));
		sprintf(return_string, "%s", signal_name);
	}
	if (bit != -1)
	{
		oassert(signal_name != NULL);
		return_string = (char*)realloc(return_string, sizeof(char)*(strlen(return_string)+1+10+1));
		sprintf(return_string, "%s~%d", return_string, bit);
	}
	return return_string;	
}

/*---------------------------------------------------------------------------------------------
 * (function: twos_complement)
 * Changes a bit string to its twos complement value
 *-------------------------------------------------------------------------------------------*/
char *twos_complement(char *str)
{
	int length = strlen(str) - 1;
	int i;
	int flag = 0;

	for (i = length; i >= 0; i--)
	{
		if (flag == 0)
			str[i] = str[i];
		else
			str[i] = (str[i] == '1') ? '0' : '1';
		if ((str[i] == '1') && (flag == 0))
			flag = 1;
	}
	return str;
}

/*---------------------------------------------------------------------------------------------
 * (function: convert_int_to_bit_string)
 * Outputs a string msb to lsb.  For example, 3 becomes "011"
 *-------------------------------------------------------------------------------------------*/
char *convert_long_to_bit_string(long long orig_long, int num_bits)
{
	int i;
	char *return_val = (char*)malloc(sizeof(char)*(num_bits+1));
	int mask = 1;

	for (i = num_bits-1; i >= 0; i--)
	{
		if((mask & orig_long) > 0)
		{
			return_val[i] = '1';	
		}
		else
		{
			return_val[i] = '0';	
		}
		mask = mask << 1;
	}
	return_val[num_bits] = '\0';
	
	return return_val;
}

/*---------------------------------------------------------------------------------------------
 * (function: convert_dec_string_of_size_to_int)
 *-------------------------------------------------------------------------------------------*/
long long convert_dec_string_of_size_to_long(char *orig_string, int size)
{
	int i;
	long long return_value = 0;
	long long current_base_value = 1;
	char temp[2];

	if (strlen(orig_string) > 19)
	{
		/* greater than our bit capacity so not a constant 64 bits */
		return -1;
	}

	for (i = strlen(orig_string)-1; i > -1; i--)
	{
		if (isdigit(orig_string[i]))
		{
			sprintf(temp, "%c", orig_string[i]);
			return_value += (long long)(atoi(temp) * current_base_value);
		}
		else
		{
			error_message(PARSE_ERROR, -1, -1, "This suspected decimal number (%s) is not\n", orig_string);
		}
		current_base_value *= 10;
	}
	
	return return_value;
}

/*---------------------------------------------------------------------------------------------
 * (function: convert_hex_string_of_size_to_int)
 *-------------------------------------------------------------------------------------------*/
long long convert_hex_string_of_size_to_long(char *orig_string, int size)
{
	int i;
	long long return_value = 0;
	long long current_base_value = 1;
	char temp[2];

	if (strlen(orig_string) > 16)
	{
		/* greater than our bit capacity so not a constant */
		return -1;
	}

	for (i = strlen(orig_string)-1; i > -1; i--)
	{
		if (isdigit(orig_string[i]))
		{
			sprintf(temp, "%c", orig_string[i]);
			return_value += (long long)(atoi(temp) * current_base_value);
		}
		else if ((orig_string[i] == 'a') || (orig_string[i] == 'A'))
		{
			return_value += (long long)(10 * current_base_value);
		}
		else if ((orig_string[i] == 'b') || (orig_string[i] == 'B'))
		{
			return_value += (long long)(11 * current_base_value);
		}
		else if ((orig_string[i] == 'c') || (orig_string[i] == 'C'))
		{
			return_value += (long long)(11 * current_base_value);
		}
		else if ((orig_string[i] == 'd') || (orig_string[i] == 'D'))
		{
			return_value += (long long)(11 * current_base_value);
		}
		else if ((orig_string[i] == 'e') || (orig_string[i] == 'E'))
		{
			return_value += (long long)(11 * current_base_value);
		}
		else if ((orig_string[i] == 'f') || (orig_string[i] == 'F'))
		{
			return_value += (long long)(11 * current_base_value);
		}
		else
		{
			error_message(PARSE_ERROR, -1, -1, "This suspected hex number (%s) is not\n", orig_string);
		}
		current_base_value *= 16;
	}
	
	return return_value;
}

/*---------------------------------------------------------------------------------------------
 * (function: convert_oct_string_of_size_to_int)
 *-------------------------------------------------------------------------------------------*/
long long convert_oct_string_of_size_to_long(char *orig_string, int size)
{
	int i;
	long long return_value = 0;
	long long current_base_value = 1;
	char temp[2];

	if (strlen(orig_string) > 21)
	{
		/* greater than our bit capacity so not a constant */
		return -1;
	}

	for (i = strlen(orig_string)-1; i > -1; i--)
	{
		if (isdigit(orig_string[i]))
		{
			oassert(atoi(temp) < 8);
			sprintf(temp, "%c", orig_string[i]);
			return_value += (long long)(atoi(temp) * current_base_value);
		}
		else
		{
			error_message(PARSE_ERROR, -1, -1, "This suspected oct number (%s) is not\n", orig_string);
		}
		current_base_value *= 8;
	}
	
	return return_value;
}

/*---------------------------------------------------------------------------------------------
 * (function: convert_binary_string_of_size_to_int)
 *-------------------------------------------------------------------------------------------*/
long long convert_binary_string_of_size_to_long(char *orig_string, int size)
{
	int i;
	long long return_value = 0;
	long long current_base_value = 1;
	char temp[2];

	if (strlen(orig_string) > 63)
	{
		/* greater than our bit capacity so not a constant */
		return -1;
	}

	for (i = strlen(orig_string)-1; i > -1; i--)
	{
		if ((tolower(orig_string[i]) == 'x') || (tolower(orig_string[i]) == 'z'))
		{
			/* this can't be converted to a decimal value */
			return -1;
		}
		else if (isdigit(orig_string[i]))
		{
			sprintf(temp, "%c", orig_string[i]);
			oassert(atoi(temp) < 2);
			return_value += (long long)(atoi(temp) * current_base_value);
		}
		else
		{
			error_message(PARSE_ERROR, -1, -1, "This suspected binary number (%s) is not\n", orig_string);
		}
		current_base_value *= 2;
	}
	
	return return_value;
}

/*---------------------------------------------------------------------------------------------
 * (function: my_power)
 *      My own simple power function
 *-------------------------------------------------------------------------------------------*/
long long int my_power(long long int x, long long int y)
{
	int i;
	long long int value;

	if (y == 0)
	{
		return 1;
	}

	value = x;

	for (i = 1; i < y; i++)
	{
		value *= x;
	}

	return value;
}

/*---------------------------------------------------------------------------------------------
 *  (function: make_simple_name )
 *-------------------------------------------------------------------------------------------*/
char *make_string_based_on_id(nnode_t *node)
{
	char *return_string;

	return_string = (char*)malloc(sizeof(char)*(20+2)); // any unique id greater than 20 characters means trouble

	sprintf(return_string, "n%ld", node->unique_id);

	return return_string;
}

/*---------------------------------------------------------------------------------------------
 *  (function: make_simple_name )
 *-------------------------------------------------------------------------------------------*/
char *make_simple_name(char *input, char *flatten_string, char flatten_char)
{
	int i;
	int j;
	char *return_string = NULL;
	oassert(input != NULL);

	return_string = (char*)malloc(sizeof(char)*(strlen(input)+1));

	for (i = 0; i < strlen(input); i++)
	{ 
		return_string[i] = input[i];
		for (j = 0; j < strlen(flatten_string); j++)
		{
			if (input[i] == flatten_string[j])
			{
				return_string[i] = flatten_char;
				break;
			}
		}
	}

	return_string[strlen(input)] = '\0';	

	return return_string;
}

/*-----------------------------------------------------------------------
 *  * (function: my_malloc_struct )
 *   *-----------------------------------------------------------------*/
void *my_malloc_struct(int bytes_to_alloc)
{
        void *allocated = NULL;
        static long int m_id = 0;

	// ways to stop the execution at the point when a specific structure is built
	//oassert(m_id != 1777);

        allocated = malloc(bytes_to_alloc);
        if(allocated == NULL) 
        {
                fprintf(stderr,"MEMORY FAILURE\n"); 
                oassert (0);
        }

	/* mark the unique_id */
        *((long int*)allocated) = m_id; 

        m_id++;

        return(allocated);
}
/*---------------------------------------------------------------------------------------------
 * (function: pow2 )
 *-------------------------------------------------------------------------------------------*/
long long int pow2(int to_the_power)
{
	int i;
	long long int return_val = 1;

	for (i = 0; i < to_the_power; i++)
	{
		return_val = return_val << 1;
	}

	return return_val;
}
