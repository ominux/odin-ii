/*
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

/* This is the functionality that reads in a blif file (likely from ABC) and puts it intoo the OdinII netlist format.
   This has been derived from the blif reading in t-vpack */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "util.h"
#include "globals.h"
#include "read_blif.h"
#include "netlist_utils.h"
#include "errors.h"

static FILE *blif;
int linenum;

char *blif_one_string = "ONE_VCC_CNS";
char *blif_zero_string = "ZERO_GND_ZERO";
char *blif_pad_string = "ZERO_PAD_ZERO";

static fpos_t first_end_pos; /* pointer to the first .end */

static void read_tokens ( char *buffer, int pass, int *done, int lut_size, netlist_t *netlist);
nnode_t * add_node_and_driver (netlist_t *netlist, int lut_size);
void add_node_inputs (netlist_t *netlist);
static void add_latch_node_and_driver (netlist_t *netlist);
static void add_latch_inputs (netlist_t *netlist);
static void add_io_node_and_driver (short io_type, netlist_t *netlist);
static void dum_parse ( char *buf); 
void convert_function_for_lut(nnode_t *node, char *input, netlist_t *netlist, int lut_size);

/*---------------------------------------------------------------------------------------------
 * (function: read_blif)
 *-------------------------------------------------------------------------------------------*/
netlist_t *
read_blif (
    char *blif_file,
    int lut_size
    )
{
	netlist_t *netlist = allocate_netlist();
	char buffer[BUFSIZE];
	int pass, done;
	npin_t *new_pin;

	blif = my_fopen (blif_file, "r", 0);

	/* create the constant nets */
	/* ZERO */
	netlist->zero_net = allocate_nnet();
	netlist->gnd_node = allocate_nnode();
	netlist->gnd_node->type = GND_NODE;
	allocate_more_node_output_pins(netlist->gnd_node, 1);
	add_output_port_information(netlist->gnd_node, 1);
	new_pin = allocate_npin();
	add_a_output_pin_to_node_spot_idx(netlist->gnd_node, new_pin, 0);
	add_a_driver_pin_to_net(netlist->zero_net, new_pin);
	netlist->gnd_node->name = blif_zero_string;
	netlist->zero_net->name = blif_zero_string;
	/* ONE */
	netlist->one_net = allocate_nnet();
	netlist->vcc_node = allocate_nnode();
	netlist->vcc_node->type = VCC_NODE;
	allocate_more_node_output_pins(netlist->vcc_node, 1);
	add_output_port_information(netlist->vcc_node, 1);
	new_pin = allocate_npin();
	add_a_output_pin_to_node_spot_idx(netlist->vcc_node, new_pin, 0);
	add_a_driver_pin_to_net(netlist->one_net, new_pin);
	netlist->vcc_node->name = blif_one_string;
	netlist->one_net->name = blif_one_string;

	/* PAD */
	netlist->pad_net = allocate_nnet();
	netlist->pad_node = allocate_nnode();
	netlist->pad_node->type = PAD_NODE;
	allocate_more_node_output_pins(netlist->pad_node, 1);
	add_output_port_information(netlist->pad_node, 1);
	new_pin = allocate_npin();
	add_a_output_pin_to_node_spot_idx(netlist->pad_node, new_pin, 0);
	add_a_driver_pin_to_net(netlist->pad_net, new_pin);
	netlist->gnd_node->name = blif_pad_string;
	netlist->zero_net->name = blif_pad_string;

	/* Two passes.  First we create the nodes and drivers.  Second, we hook them up */
	for (pass = 1; pass <= 2; pass++)
	{
		linenum = 0;
		done = 0;
		while ((my_fgets (buffer, BUFSIZE, blif) != NULL) && !done)
		{
			read_tokens (buffer, pass, &done, lut_size, netlist);
		}
		rewind (blif);	/* Start at beginning of file again */
	}

	fclose (blif);

	return netlist;
}

/*---------------------------------------------------------------------------------------------
 * (function: read_tokens)
 *-------------------------------------------------------------------------------------------*/
#define TOKENS " \t\n"
static void
read_tokens (
    char *buffer,
    int pass,
    int *done, 
    int lut_size, 
    netlist_t *netlist)
{

	/* Figures out which, if any token is at the start of this line and *
	 * takes the appropriate action.                                    */
	char *ptr;
	static nnode_t *current_node = NULL;
	short static last_was_names = FALSE;

	ptr = my_strtok (buffer, TOKENS, blif, buffer);

	if ((last_was_names == TRUE) || ((ptr != NULL) && ((ptr[0] == '0') || (ptr[0] == '1') || (ptr[0] == '-'))))
	{
		/* IF - this is a function then we'll convert to function */
		if (pass == 1)
		{
			convert_function_for_lut(current_node, ptr, netlist, lut_size);
		}
		else
		{
			dum_parse(buffer);
		}
	}

	if (ptr == NULL)
		return;

	last_was_names = FALSE;

	if (strcmp (ptr, ".names") == 0)
	{
		/* convert the header of a function in 2 passes.  Pass 1 create the drivers, Pass 2 hookup the inputs to the drivers */
		if (pass == 1)
		{
			current_node = add_node_and_driver(netlist, lut_size);
			last_was_names = TRUE;
		}
		else if (pass == 2)
		{
			add_node_inputs(netlist);
		}
		else
		{
			dum_parse (buffer);
		}
		return;
	}

	if (strcmp (ptr, ".latch") == 0)
	{
		/* create latches with same passes as functions */
		if (pass == 1)
		{
			add_latch_node_and_driver (netlist);
		}
		else if (pass == 2)
		{
			add_latch_inputs (netlist);
		}
		else
		{
			dum_parse (buffer);
		}
		return;
	}

	if (strcmp (ptr, ".inputs") == 0)
	{
		/* Create an input node */
		if (pass == 1)
		{
			add_io_node_and_driver (INPUT_NODE, netlist);
		}
		else
		{
			dum_parse (buffer);
		}
		return;
	}

	if (strcmp (ptr, ".outputs") == 0)
	{
		/* Create output nodes */
		if (pass == 1)
		{
			add_io_node_and_driver(OUTPUT_NODE, netlist);
		}
		else
		{
			dum_parse (buffer);
		}
		return;
	}

	if (strcmp (ptr, ".end") == 0)
	{
		/* PAJ - record the spot of the file end since this is where we move to to start looking for the subckts */
		if (fgetpos (blif, &first_end_pos) != 0)
		{
			printf ("Error in file pointer read - read_blif.c\n");
			exit (-1);
		}
		*done = 1;		/* PAJ - end since there are other .end lower for subckt */
		return;
	}

	if (strcmp (ptr, ".model") == 0)
	{
#if 0
		ptr = my_strtok (NULL, TOKENS, blif, buffer);

		if (doall && pass == 4)
		{			/* Only bother on main second pass. */
			if (ptr != NULL)
			{
				model = (char *) my_malloc ((strlen (ptr) + 1) * sizeof (char));
				strcpy (model, ptr);
			}
			else
			{
				model = (char *) my_malloc (sizeof (char));
				model[0] = '\0';
			}
			model_lines++;	/* For error checking only */
		}
		return;
#endif
	}

	if (strcmp (ptr, ".subckt") == 0)
	{
		oassert(FALSE);
#if 0
		if (pass == 1)		// PAJ - pass 1 since we depend on the block being indexed and not changed...if lower in the list then compress_net kills this assumption
		{
			add_subckt (doall);
		}
#endif
	}
}

/*---------------------------------------------------------------------------------------------
 * (function: dum_parse)
 *-------------------------------------------------------------------------------------------*/
static void
dum_parse (
    char *buf)
{
	/* Continue parsing to the end of this (possibly continued) line. */
	while (my_strtok (NULL, TOKENS, blif, buf) != NULL)
		;
}

/*---------------------------------------------------------------------------------------------
 * (function: add_node_and_driver)
 *-------------------------------------------------------------------------------------------*/
nnode_t *
add_node_and_driver (netlist_t *netlist, int lut_size)
{
	char *ptr_traverse; 
	char buf[BUFSIZE];
	char *output_name; 
	long sc_spot;
	nnode_t *new_node = allocate_nnode();
	npin_t *node_output_pin;
	nnet_t *new_net;
	int input_count;

	/* create node */
	new_node->related_ast_node = NULL;
	new_node->type = BLIF_FUNCTION;

	/* allocate the pins needed */
	allocate_more_node_output_pins(new_node, 1);
	add_output_port_information(new_node, 1);

	/* Count # nets connecting */
	input_count = 0;
	while ((ptr_traverse = my_strtok (NULL, TOKENS, blif, buf)) != NULL)
	{
		input_count++;
		output_name = ptr_traverse;
	}

	if (input_count-1 > 0)
	{
		/* allocate input pins.  Is 1 less than i where the last i is the output name */
		allocate_more_node_input_pins(new_node, input_count-1);
		add_input_port_information(new_node, 1);
	}
	
	/* create the unique name for this gate */
	new_node->name = make_full_ref_name(NULL, NULL, NULL, output_name, -1);

	/* add the node to the netlist */
	add_node_to_netlist(netlist, new_node, -1);

	/* make the list of function outputs */
	new_node->associated_function = (short*)calloc(sizeof(short), pow2(lut_size));

	/* add the pin */
	node_output_pin = allocate_npin();
	add_a_output_pin_to_node_spot_idx(new_node, node_output_pin, 0);
	
	/* now add the net and store it in the string hash */
	new_net = allocate_nnet();
	new_net->name = new_node->name;
	add_a_driver_pin_to_net(new_net, node_output_pin);

	/* add to the driver hash */
	if ((sc_spot = sc_lookup_string(netlist->out_pins_sc, new_node->name)) != -1)
	{
		/* Special case that the driver drives an output pin, then hook it up.  Note that we don't test for multiple drivers to an output */
		nnode_t *output_node = (nnode_t*)netlist->out_pins_sc->data[sc_spot];
		npin_t *node_input_pin;

		/* hookup this output net to that node */
		node_input_pin = allocate_npin();
		/* add to the driver net */
		add_a_fanout_pin_to_net(new_net, node_input_pin);
		/* add pin to node */
		add_a_input_pin_to_node_spot_idx(output_node, node_input_pin, 0);

		/* add to the driver hash */
		sc_spot = sc_add_string(netlist->nets_sc, new_node->name);
		if (netlist->nets_sc->data[sc_spot] != NULL)
		{
			error_message(BLIF_ERROR, linenum, -1, "Two blif outputs with the same name (%s)\n", new_node->name);
		}
		netlist->nets_sc->data[sc_spot] = (void*)new_net;
	}
	else
	{
		/* add to the driver hash */
		sc_spot = sc_add_string(netlist->nets_sc, new_node->name);
		if (netlist->nets_sc->data[sc_spot] != NULL)
		{
			error_message(BLIF_ERROR, linenum, -1, "Two blif outputs with the same name (%s)\n", new_node->name);
		}
		netlist->nets_sc->data[sc_spot] = (void*)new_net;
	}

	return new_node;
}

/*---------------------------------------------------------------------------------------------
 * (function: convert_function_for_lut)
 *-------------------------------------------------------------------------------------------*/
void convert_function_for_lut(nnode_t *node, char *input, netlist_t *netlist, int lut_size)
{
	int i, j, k;
	char *output_value;
	char buf[BUFSIZE];

	if ((input[0] == '0') || (input[0] == '1') || (input[0] == '-'))
	{
		/* get next line */
		output_value = my_strtok (NULL, TOKENS, blif, buf);

		if (output_value == NULL)
		{
			npin_t *node_input_pin;

			/* hookup this output net to that node */
			node_input_pin = allocate_npin();
			allocate_more_node_input_pins(node, 1);
			/* add pin to node */
			add_a_input_pin_to_node_spot_idx(node, node_input_pin, 0);

			if (input[0] == '0')
			{
				/* add to the driver net */
				add_a_fanout_pin_to_net(netlist->zero_net, node_input_pin);
			}
			else if (input[0] == '1')
			{
				/* value in function */
				for (i = 0; i < pow2(lut_size); i++)
				{
					node->associated_function[i] = 1;
				}

				/* add to the driver net */
				add_a_fanout_pin_to_net(netlist->one_net, node_input_pin);
			}
		}
		else
		{
			int num_inputs = strlen(input);
			long long int running_val;
			long long int *index_skip_list = NULL;
			long long int num_index_skip_list;
			long long int start_index = 0;
			long long int this_index;
			int current_function_num_entries;
	
			oassert(num_inputs < sizeof (long long int)*8);
			oassert(num_inputs == node->num_input_pins);
			oassert(strlen(output_value) == 1);
		
			if (output_value[0] == '0')
				return;
	
			index_skip_list = (long long int*)malloc(sizeof(long long int)*num_inputs);
			num_index_skip_list = 0;
	
			running_val = 1;
	
			/* find the starting index, and all the wildcard characters */
			for (i = num_inputs-1; i >= 0; i--)
			{
				if (input[i] == '1')
				{
					start_index += running_val;
				}
				else if (input[i] == '-')
				{
					index_skip_list[num_index_skip_list] = running_val; // records the collumn value
					num_index_skip_list ++;
				}
				running_val = running_val << 1; 
			}
	
			/* go through each of the possible combinations */
			for (i = 0; i < pow2(num_index_skip_list); i++)
			{
				this_index = start_index;
	
				/* like going through a truth table of all possibilities */
				for (j = 0; j < num_index_skip_list; j++)
				{
					long long int place = 1 << j; //index_skip_list[j];
	
					if ((i & place) > 0) // if this bit is high
					{
						this_index += index_skip_list[j];
					}
				}
	
				oassert(this_index < pow2(num_inputs));
				current_function_num_entries = pow2(num_inputs);
				for (k = 0; k < pow2(lut_size-(num_inputs)); k++)
				{
					node->associated_function[this_index+k*current_function_num_entries] = 1;
				}
			}
			free(index_skip_list);
		}
	}
	else 
	{
		npin_t *node_input_pin;

		/* hookup this output net to that node */
		node_input_pin = allocate_npin();
		allocate_more_node_input_pins(node, 1);
		/* add pin to node */
		add_a_input_pin_to_node_spot_idx(node, node_input_pin, 0);

		add_a_fanout_pin_to_net(netlist->zero_net, node_input_pin);
	}

}

/*---------------------------------------------------------------------------------------------
 * (function: add_node_inputs)
 *-------------------------------------------------------------------------------------------*/
void
add_node_inputs (netlist_t *netlist)
{
	int i;
	char *ptr_traverse; 
	char buf[BUFSIZE];
	char *input_name = NULL; 
	char *output_name; 
	long sc_spot;
	npin_t **node_input_pin = NULL;
	int num_input_pins = 0;
	nnet_t *driver_net;
	nnode_t *this_node;
	int input_count;

	/* now that we have the node, add the inputs */
	input_count = -1; /* start at -1 since we're delayed by one */
	while ((ptr_traverse = my_strtok (NULL, TOKENS, blif, buf)) != NULL)
	{
		if (input_name != NULL)
		{
			if ((sc_spot = sc_lookup_string(netlist->nets_sc, input_name)) == -1)
			{
				error_message(BLIF_ERROR, linenum, -1, "Blif input does not exist (%s)\n", input_name);
			}
			driver_net = (nnet_t*)netlist->nets_sc->data[sc_spot];

			/* add the input pin to the driver net */
			node_input_pin = (npin_t**)realloc(node_input_pin, sizeof(npin_t*)*(num_input_pins+1));
			num_input_pins++;
			node_input_pin[input_count] = allocate_npin();
			node_input_pin[input_count]->name = input_name;
			/* add to the driver net */
			add_a_fanout_pin_to_net(driver_net, node_input_pin[input_count]);
		}

		output_name = ptr_traverse;
		input_name = ptr_traverse;
		input_count++;
	}

	/* now that we know the node name, find it */
	if ((sc_spot = sc_lookup_string(netlist->nets_sc, output_name)) == -1)
	{
		error_message(BLIF_ERROR, linenum, -1, "Blif output node does not exist (%s)\n", output_name);
	}
	this_node = ((nnet_t*)netlist->nets_sc->data[sc_spot])->driver_pin->node;

	/* hookup all the input pins into this net now */
	for (i = 0; i < input_count; i++)
	{
		add_a_input_pin_to_node_spot_idx(this_node, node_input_pin[i], i);
	} 

	free(node_input_pin);
}

/*---------------------------------------------------------------------------------------------
 * (function: add_latch_node_and_driver)
 *-------------------------------------------------------------------------------------------*/
static void add_latch_node_and_driver (netlist_t *netlist)
{
	 /* .latch <input> <output> <type (latch on)> <control (clock)> <init_val> *
	 * The latch pins are in .nets 0 to 2 in the order: Q D CLOCK.            */
	char *ptr, buf[BUFSIZE], saved_names[6][BUFSIZE];
	int i;
	long sc_spot;
	nnode_t *new_node = allocate_nnode();
	npin_t *node_output_pin;
	nnet_t *new_net;

	/* Count # parameters, making sure we don't go over 6 (avoids memory corr.) */
	/* Note that we can't rely on the tokens being around unless we copy them.  */

	for (i = 0; i < 6; i++)
	{
		ptr = my_strtok (NULL, TOKENS, blif, buf);
		if (ptr == NULL)
			break;
		strcpy (saved_names[i], ptr);
	}

	if (i != 5)
	{
		error_message (BLIF_ERROR, linenum, -1, "Error:  .latch does not have 5 parameters.\ncheck the netlist, line %d.\n", linenum);
		exit (1);
	}

	/* create node */
	new_node->related_ast_node = NULL;
	new_node->type = FF_NODE;

	/* allocate the pins needed */
	allocate_more_node_output_pins(new_node, 1);
	add_output_port_information(new_node, 1);

	/* allocate input pins.  Is 1 less than i where the last i is the output name */
	allocate_more_node_input_pins(new_node, 2);
	add_input_port_information(new_node, 1);
	
	/* create the unique name for this gate.  indx=1 is the output name */
	new_node->name = make_full_ref_name(NULL, NULL, NULL, saved_names[1], -1);

	/* add the node to the netlist */
	add_node_to_netlist(netlist, new_node, -1);

	/* add the pin */
	node_output_pin = allocate_npin();
	add_a_output_pin_to_node_spot_idx(new_node, node_output_pin, 0);
	
	/* now add the net and store it in the string hash */
	new_net = allocate_nnet();
	new_net->name = new_node->name;
	add_a_driver_pin_to_net(new_net, node_output_pin);

	/* add to the driver hash */
	if ((sc_spot = sc_lookup_string(netlist->out_pins_sc, new_node->name)) != -1)
	{
		nnode_t *output_node = (nnode_t*)netlist->out_pins_sc->data[sc_spot];
		npin_t *node_input_pin;

		/* hookup this output net to that node */
		node_input_pin = allocate_npin();
		/* add to the driver net */
		add_a_fanout_pin_to_net(new_net, node_input_pin);
		/* add pin to node */
		add_a_input_pin_to_node_spot_idx(output_node, node_input_pin, 0);

		/* add to the driver hash */
		sc_spot = sc_add_string(netlist->nets_sc, new_node->name);
		if (netlist->nets_sc->data[sc_spot] != NULL)
		{
			error_message(BLIF_ERROR, linenum, -1, "Two blif outputs with the same name (%s)\n", new_node->name);
		}
		netlist->nets_sc->data[sc_spot] = (void*)new_net;
	}
	else
	{
		/* add to the driver hash */
		sc_spot = sc_add_string(netlist->nets_sc, new_node->name);
		if (netlist->nets_sc->data[sc_spot] != NULL)
		{
			error_message(BLIF_ERROR, linenum, -1, "Two blif outputs with the same name (%s)\n", new_node->name);
		}
		netlist->nets_sc->data[sc_spot] = (void*)new_net;
	}
}

/*---------------------------------------------------------------------------------------------
 * (function: add_latch_inputs)
 *-------------------------------------------------------------------------------------------*/
static void add_latch_inputs (netlist_t *netlist)
{
	 /* .latch <input> <output> <type (latch on)> <control (clock)> <init_val> *
	 * The latch pins are in .nets 0 to 2 in the order: Q D CLOCK.            */
	char *ptr, buf[BUFSIZE], saved_names[6][BUFSIZE];
	int i;
	long sc_spot;
	npin_t *node_input_pin = NULL;
	npin_t *node_clock_pin = NULL;
	nnet_t *driver_net;
	nnode_t *this_node;

	/* Count # parameters, making sure we don't go over 6 (avoids memory corr.) */
	/* Note that we can't rely on the tokens being around unless we copy them.  */

	for (i = 0; i < 6; i++)
	{
		ptr = my_strtok (NULL, TOKENS, blif, buf);
		if (ptr == NULL)
			break;
		strcpy (saved_names[i], ptr);
	}

	if (i != 5)
	{
		error_message (BLIF_ERROR, linenum, -1, "Error:  .latch does not have 5 parameters.\ncheck the netlist, line %d.\n", linenum);
		exit (1);
	}

	/* ADD INPUT */
	if ((sc_spot = sc_lookup_string(netlist->nets_sc, saved_names[0])) == -1)
	{
		error_message(BLIF_ERROR, linenum, -1, "Blif input does not exist (%s)\n", saved_names[0]);
	}
	driver_net = (nnet_t*)netlist->nets_sc->data[sc_spot];

	/* add the input pin to the driver net */
	node_input_pin = allocate_npin();
	/* add to the driver net */
	add_a_fanout_pin_to_net(driver_net, node_input_pin);

	/* ADD CLOCK */
	if ((sc_spot = sc_lookup_string(netlist->nets_sc, saved_names[3])) == -1)
	{
		error_message(BLIF_ERROR, linenum, -1, "Blif input does not exist (%s)\n", saved_names[3]);
	}
	driver_net = (nnet_t*)netlist->nets_sc->data[sc_spot];

	/* add the input pin to the driver net */
	node_clock_pin = allocate_npin();
	/* add to the driver net */
	add_a_fanout_pin_to_net(driver_net, node_clock_pin);

	/* now that we know the node name, find it */
	if ((sc_spot = sc_lookup_string(netlist->nets_sc, saved_names[1])) == -1)
	{
		error_message(BLIF_ERROR, linenum, -1, "Blif output node does not exist (%s)\n", saved_names[0]);
	}
	this_node = ((nnet_t*)netlist->nets_sc->data[sc_spot])->driver_pin->node;

	/* hookup all the input pins into this net now */
	add_a_input_pin_to_node_spot_idx(this_node, node_input_pin, 0);
	/* clocks on input pin 1 */
	add_a_input_pin_to_node_spot_idx(this_node, node_clock_pin, 1);
}

/*---------------------------------------------------------------------------------------------
 * (function: add_io_node_and_driver )
 *-------------------------------------------------------------------------------------------*/
static void
add_io_node_and_driver (
	short io_type,
	netlist_t *netlist
)
{
	char *ptr; 
	char buf[BUFSIZE];
	long sc_spot;
	nnode_t *new_node;
	npin_t *node_output_pin;
	nnet_t *new_net;

	while (1)
	{
		/* go through each of the io pins */
		ptr = my_strtok (NULL, TOKENS, blif, buf);

		if (ptr == NULL)
			return;

		/* create a new node for this input */
		new_node = allocate_nnode();
		/* create node */
		new_node->related_ast_node = NULL;
		new_node->type = io_type;

		if (io_type == INPUT_NODE)
		{
			/* create the unique name for this gate */
			new_node->name = make_full_ref_name(NULL, NULL, NULL, ptr, -1);

			/* add the node to the netlist */
			add_node_to_netlist(netlist, new_node, -1);

			/* add the output pin */
			allocate_more_node_output_pins(new_node, 1);
			add_output_port_information(new_node, 1);

			/* add the pin */
			node_output_pin = allocate_npin();
			add_a_output_pin_to_node_spot_idx(new_node, node_output_pin, 0);

			/* now add the net and store it in the string hash */
			new_net = allocate_nnet();
			new_net->name = new_node->name;
			add_a_driver_pin_to_net(new_net, node_output_pin);

			/* add to the driver hash */
			sc_spot = sc_add_string(netlist->nets_sc, new_node->name);
			if (netlist->nets_sc->data[sc_spot] != NULL)
			{
				error_message(BLIF_ERROR, linenum, -1, "Two blif outputs with the same name (%s)\n", new_node->name);
			}
			netlist->nets_sc->data[sc_spot] = (void*)new_net;
		}
		else if (io_type == OUTPUT_NODE)
		{
			char *output_name = (char*)malloc(sizeof(char)*(strlen(ptr)+1+4));
			sprintf(output_name, "out:%s", ptr);
			/* create the unique name for this gate */
			new_node->name = make_full_ref_name(NULL, NULL, NULL, output_name, -1);

			/* add the node to the netlist */
			add_node_to_netlist(netlist, new_node, -1);

			/* allocate input pins.  Is 1 less than i where the last i is the output name */
			allocate_more_node_input_pins(new_node, 1);
			add_input_port_information(new_node, 1);

			/* add to the driver hash */
			sc_spot = sc_add_string(netlist->out_pins_sc, ptr);//new_node->name);
			if (netlist->out_pins_sc->data[sc_spot] != NULL)
			{
				error_message(BLIF_ERROR, linenum, -1, "Two outputs pins with the same name (%s)\n", ptr);
			}
			netlist->out_pins_sc->data[sc_spot] = (void*)new_node;
		}
	}
}
