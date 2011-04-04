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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <dlfcn.h>
#include "sim_block.h"
#include "types.h"
#include "globals.h"
#include "errors.h"
#include "netlist_utils.h"
#include "odin_util.h"
#include "outputs.h"
#include "util.h"
#include "multipliers.h"
#include "hard_blocks.h"
#include "simulate_blif.h"
#include "types.h"
#include "queue.h"

#define BUFFER_MAX_SIZE 1024

//#define DEBUG_SIMULATOR

#define CLOCK_PORT_NAME_1 "clock"
#define CLOCK_PORT_NAME_2 "clk"
#define RESET_PORT_NAME "reset_n"

#define SINGLE_PORT_MEMORY_NAME "single_port_ram"
#define DUAL_PORT_MEMORY_NAME "dual_port_ram"

void simulate_cycle(netlist_t *netlist, int cycle);
void store_value_in_line(char *token, line_t *line, int cycle);
void assign_node_to_line(nnode_t *node, line_t **lines, int lines_size, int type);
line_t** read_test_vector_headers(FILE *out, int *lines_size, int max_lines_size);
long int get_value_from_string(char *token);
int is_node_top_level(netlist_t *netlist, nnode_t *node);
void compute_and_store_value(nnode_t *node, int cycle);
void free_lines(line_t **lines, int lines_size);
void assign_input_vector_to_lines(line_t **lines, char *buffer, int cycle);
void assign_random_vector_to_input_lines(line_t **lines, int lines_size, int cycle);
void write_vectors_to_file(line_t **lines, int lines_size, FILE *file, int type, int cycle);
line_t **create_test_vector_lines(int *lines_size, netlist_t *netlist);
int verify_output_vectors(netlist_t *netlist, line_t **lines, int lines_size, int cycle);
line_t *create_line(char *name);
int has_node_been_computed_for_cycle(nnode_t *node, int cycle);
void write_vector_headers(FILE *iv, FILE *outv, line_t **lines, int lines_size);

void set_constant_pin_values(netlist_t *netlist, int cycle);
void update_pin_value(npin_t *pin, int value, int cycle);

int *multiply_arrays(int *a, int a_length, int *b, int b_length);
void compute_memory(npin_t **inputs, npin_t **outputs, 
		int data_width, npin_t **addr, int addr_width, int we, int clock, int cycle, int *data);

void instantiate_memory(nnode_t *node, int **memory, int data_width, int addr_width);

void free_blocks();

simulation_type sim_type;
int sim_result;
FILE *modelsim_out;

queue_t *blocks;

/* preprocessing
 * Simulates the netlist with the test_vector_file_name as input.
 * This simulates the input values in the test vector file, and
 * ensures the simulated values match the output values in the file.
 */
void simulate_blif (char *test_vector_file_name, netlist_t *netlist)
{
	FILE *in;
	char buffer[BUFFER_MAX_SIZE];
	int cycle, i, lines_size;
	line_t **lines;

	sim_type = TEST_EXISTING_VECTORS;
	sim_result = TRUE;		//we're going to assume that everything goes OK for now
	modelsim_out = NULL;
	blocks = create_queue();

	//open our file
	in = fopen(test_vector_file_name, "r");
	if (in == NULL)
	{
		error_message(SIMULATION_ERROR, -1, -1, "Could not open input vectors file %s\n", test_vector_file_name);
	}

	//lines is an array of line_t structs that store a name and an array of pins
	lines = read_test_vector_headers(in, &lines_size, netlist->num_top_input_nodes + netlist->num_top_output_nodes);

	//for each top input node, map it to a line
	for (i = 0; i < netlist->num_top_input_nodes; i++)
		assign_node_to_line(netlist->top_input_nodes[i], lines, lines_size, INPUT);
	//for each top output node, map it to a line
	for (i = 0; i < netlist->num_top_output_nodes; i++)
		assign_node_to_line(netlist->top_output_nodes[i], lines, lines_size, OUTPUT);

	for (i = 0; i < lines_size; i++)
	{
		int j;
		for (j = 0; j < lines[i]->number_of_pins; j++)
		{
			if (NULL == lines[i]->pins[j])
			{
				warning_message(SIMULATION_ERROR, -1, -1, "A line has a NULL pin. This may cause a segfault. This can be caused when registers are declared as reg [X:Y], where Y is greater than zero.");
			}
		}
	}

	//get a line from our input vectors file and do stuff with it
	cycle = 0;
	while (fgets(buffer, BUFFER_MAX_SIZE, in) != NULL)
	{
		//continues while we can still read lines from the test vector file
#ifdef DEBUG_SIMULATOR
		printf("Cycle: %d\nRead in from file: %s", cycle, buffer);
#endif

		//assigns the test values for the input lines ONLY
		assign_input_vector_to_lines(lines, buffer, cycle);

#ifdef DEBUG_SIMULATOR
		int m,n;
		for (m = 0; m < lines_size; m++)
		{
			printf("Line %s pin values %d through 0: ", lines[m]->name, lines[m]->number_of_pins-1);
			for (n = lines[m]->number_of_pins -1; n >= 0; n--)
			{
				printf("%d", lines[m]->pins[n]->sim_state->value);
			}
			printf("\n");
		}
#endif

		simulate_cycle(netlist, cycle);
		//checks that the output test values match what we simulated
		verify_output_vectors(netlist, lines, lines_size, cycle);

		cycle++;
	}

	//if something went wrong, let the user know. They'll have to search the output
	//for specifics
	if (!sim_result)
	{
		printf("\nSimulation Error\n");
		fprintf(stderr, "Simulation produced invalid data against the test vectors. Please see stderr output for details.");
	}

	fclose(in);
	free_lines(lines, lines_size);
	free_blocks();
}

/*
 * Simulates the netlist with newly generated, random test vectors.
 * This will simulate num_test_vectors cycles of the circuit, storing
 * the input and output values in INPUT_TEST_VECTORS_FILE_NAME and
 * OUTPUT_TEST_VECTORS_FILE_NAME, respectively.
 */
void simulate_new_vectors (int num_test_vectors, netlist_t *netlist)
{
	FILE *iv, *outv;
	int cycle, lines_size;
	line_t **lines;
	int i;

	sim_type = GENERATE_VECTORS;

	blocks = create_queue();

	//open the input and output vector files
	iv = fopen(INPUT_VECTOR_FILE_NAME, "w");
	if (NULL == iv)
	{
		error_message(SIMULATION_ERROR, -1, -1, "Could not write to input vectors file %s\n", INPUT_VECTOR_FILE_NAME);
	}
	outv = fopen(OUTPUT_VECTOR_FILE_NAME, "w");
	if (NULL == outv)
	{
		error_message(SIMULATION_ERROR, -1, -1, "Could not write to output vectors file %s\n", OUTPUT_VECTOR_FILE_NAME);
	}
	modelsim_out = fopen("test.do", "w");
	if (NULL == modelsim_out)
	{
		error_message(SIMULATION_ERROR, -1, -1, "Could not write to modelsim output file\n");
	}

	fprintf(modelsim_out, "force clock 1 0, 0 50 -repeat 100\n");

	//lines will be an array representing the I/O lines of our netlist
	lines = create_test_vector_lines(&lines_size, netlist);

	for (i = 0; i < lines_size; i++)
	{
		int j;
		for (j = 0; j < lines[i]->number_of_pins; j++)
		{
			if (NULL == lines[i]->pins[j])
			{
				warning_message(SIMULATION_ERROR, -1, -1, "A line has a NULL pin. This may cause a segfault.This can be caused when registers are declared as reg [X:Y], where Y is greater than zero.");
			}
		}
	}

	//write the headers (names of I/O lines) to vector files
	//this is the same format that we're expecting to read in to verify
	//these later
	write_vector_headers(iv, outv, lines, lines_size);

	//used to generate random test vectors
	srand((unsigned)(time(0)));
	
	for (cycle = 0; cycle < num_test_vectors; cycle++)
	{
#ifdef DEBUG_SIMULATOR
		int i;
#endif
		
		assign_random_vector_to_input_lines(lines, lines_size, cycle);

#ifdef DEBUG_SIMULATOR
		printf("Cycle: %d\n", cycle);
		for (i = 0; i < lines_size; i++)
		{
			int j;

			if (lines[i]->type == OUTPUT)
				continue;

			printf("Values for line %s from %d to 0: ", lines[i]->name, lines[i]->number_of_pins);

			for (j = 0; j < lines[i]->number_of_pins; j++)
			{
				printf("%d", lines[i]->pins[j]->sim_state->value);
			}

			printf("\n");
		}
#endif

		write_vectors_to_file(lines, lines_size, iv, INPUT, cycle);
		simulate_cycle(netlist, cycle);
		write_vectors_to_file(lines, lines_size, outv, OUTPUT, cycle);
	}

	fprintf(modelsim_out, "run %d\n", cycle*101);

	fclose(iv);
	fclose(outv);
	fclose(modelsim_out);
	free_lines(lines, lines_size);
	free_blocks();
}

int has_node_been_computed_for_cycle(nnode_t *node, int cycle)
{
	int i;

	for (i = 0; i < node->num_output_pins; i++)
	{
		if (node->output_pins[i] != NULL)
			if (node->output_pins[i]->sim_state->cycle < cycle)
				return FALSE;
	}
	return TRUE;
}

/*
 * This simulates a single cycle in the netlist
 */
void simulate_cycle(netlist_t *netlist, int cycle)
{
	int i;
	queue_t *q;
	q = create_queue();

#ifndef DEBUG_SIMULATOR
	printf(".");
#endif

	//we're going to assign out constant pin values and enqueue those nodes
	set_constant_pin_values(netlist, cycle);
	enqueue_item(q, (void *)netlist->gnd_node);
	enqueue_item(q, (void *)netlist->pad_node);
	enqueue_item(q, (void *)netlist->vcc_node);
	//we're assuming that the top_input_nodes have had their values updated.
	for (i = 0; i < netlist->num_top_input_nodes; i++)
	{
		enqueue_item(q, (void *)netlist->top_input_nodes[i]);
	}


	for (i = 0; i < netlist->gnd_node->num_output_pins; i++)
	{
		int j;
		if (netlist->gnd_node->output_pins[i] == NULL ||
				netlist->gnd_node->output_pins[i]->net == NULL)
			continue;
		for (j = 0; j < netlist->gnd_node->output_pins[i]->net->num_fanout_pins; j++)
		{
			if (netlist->gnd_node->output_pins[i]->net->fanout_pins[j] == NULL ||
					netlist->gnd_node->output_pins[i]->net->fanout_pins[j]->node == NULL)
				continue;
			enqueue_item(q, (void *)netlist->gnd_node->output_pins[i]->net->fanout_pins[j]->node);
		}
	}

	for (i = 0; i < netlist->vcc_node->num_output_pins; i++)
	{
		int j;
		if (netlist->vcc_node->output_pins[i] == NULL ||
				netlist->vcc_node->output_pins[i]->net == NULL)
			continue;
		for (j = 0; j < netlist->vcc_node->output_pins[i]->net->num_fanout_pins; j++)
		{
			if (netlist->vcc_node->output_pins[i]->net->fanout_pins[j] == NULL ||
					netlist->vcc_node->output_pins[i]->net->fanout_pins[j]->node == NULL)
				continue;
			enqueue_item(q, (void *)netlist->vcc_node->output_pins[i]->net->fanout_pins[j]->node);
		}
	}
	
	
	//We're going to go through all of our flip flops before the rest of our circuit in order to 
	//update their values first
	for (i = 0; i < netlist->num_ff_nodes; i++)
	{
		nnode_t *node;
		
		node = netlist->ff_nodes[i];
		
		oassert(node->num_output_pins == 1);
		oassert(node->num_input_pins == 2);
		//If we get our inpt directly from input pins, then ouput that input's current (updated) value
		if (is_node_top_level(netlist, node->input_pins[0]->net->driver_pin->node))
		{
			update_pin_value(node->output_pins[0], node->input_pins[0]->sim_state->	value, cycle);
		}
		//If we get our input from another node, then output that inpput's previous value
		else
		{
			update_pin_value(node->output_pins[0], node->input_pins[0]->sim_state->prev_value, cycle);
		}
		
		/*
		Here's the problem: we're updating the input pins and we need, on the subsequent cycle,
		to output that value. Right now, we're outputing that input node's _previous_ value.
		We're doing this because values that come directly from input pins need to be handled.
		What we need to do is actually check if the node is directly under an input pin.
		*/
	}

	while (!is_empty(q))
	{
		nnode_t *node;
		int try_again_later, already_calculated;

		try_again_later = FALSE;
		already_calculated = FALSE;

		node = (nnode_t *)dequeue_item(q);

		if (NULL == node)
		{
			warning_message(SIMULATION_ERROR, -1, -1, "Dequeued NULL node\n");
		}

		//Check if we've already calculated it's new value
		for (i = 0; i < node->num_output_pins; i++)
		{
			if (NULL != node->output_pins[i])
				if (node->output_pins[i]->sim_state->cycle >= cycle)
					already_calculated = TRUE;
		}

		if (already_calculated
				&& !is_node_top_level(netlist, node)
				&& node->type != FF_NODE)
		{
				continue;
		}

		//Check if we're ready to calculate it's new value
		for (i = 0; i < node->num_input_pins; i++)
		{
			//have we computed everything we need to for the inputs?
			if (node->input_pins[i]->sim_state->cycle < cycle)
			{
				if (node->input_pins[i]->net->driver_pin->node->type == GND_NODE 
					||	strcmp(node->input_pins[i]->net->driver_pin->node->name, RESET_PORT_NAME) == 0)
				{
					update_pin_value(node->input_pins[i], node->input_pins[i]->sim_state->value, cycle);
				}
				else if (node->input_pins[i]->net->driver_pin->node->type == VCC_NODE ||
						node->input_pins[i]->net->driver_pin->node->type == PAD_NODE)
				{
					update_pin_value(node->input_pins[i], node->input_pins[i]->sim_state->value, cycle);
				}
				else
				{
					try_again_later = TRUE;
					//enqueue_item(q, node->input_pins[i]->net->driver_pin->node);
#ifdef DEBUG_SIMULATOR
					printf("%s still needs %s to be calculated. Re-enqueing. \n", node->name, node->input_pins[i]->net->driver_pin->node->name);
					printf("queue size: %d \n", q->count);
#endif
				}

			}
		}

		/*
		if (try_again_later == TRUE &&
						node->type == FF_NODE)
		{
			//check and see if the clock value is updated
			if (node->input_pins[1]->sim_state->cycle == cycle)
				try_again_later = FALSE;
		}
		*/

		//On the first clock cycle, let memories be computed so that output values stabilize
		if (try_again_later == TRUE &&
						node->type == MEMORY && cycle == 0)
		{
			try_again_later = FALSE;
		}

		if (try_again_later == TRUE)
		{
			enqueue_item(q, node);
			continue;
		}

		compute_and_store_value(node, cycle);

		//enqueue each child node for computation
		for (i = 0; i < node->num_output_pins; i++)
		{
			int j;
			nnet_t *net;

			//follow the output pins to their net, and follow its fanout pins to the nodes
			net = node->output_pins[i]->net;
			if (NULL == net)
			{
				//this happens because top-level output nodes have outpins pins w/o nets
				continue;
			}

			for (j = 0; j < net->num_fanout_pins; j++)
			{
				if (NULL == net->fanout_pins[j])
					continue;
				//both of these subsequent checks are absolutely necessary, because either is possible
				if (net->fanout_pins[j]->type == INPUT)
					if (net->fanout_pins[j]->node != NULL)
						enqueue_item(q, net->fanout_pins[j]->node);
			}
		}
	}

	//free our memory
	destroy_queue(q);
}

/*
 * Given a node, this function will simulate that node's new outputs,
 * and updates those pins.
 *
 * This function assumes that the input to the nodes has been updated to
 * reflect the values of the current cycle. As such, it doesn't check
 * to make sure this is the case.
 */
void compute_and_store_value(nnode_t *node, int cycle)
{
	int i, unknown;

	unknown = FALSE;

	char *temp1 = malloc(sizeof(char)*(strlen(node->name) + 4));
	char *temp2 = malloc(sizeof(char)*(strlen(node->name) + 4));

	sprintf(temp1, "top^%s", CLOCK_PORT_NAME_1);
	sprintf(temp2, "top^%s", CLOCK_PORT_NAME_2);

	if (strcmp(node->name, temp1) == 0 || strcmp(node->name, temp2) == 0)
	{
		/*
		 * ODIN doesn't list clocks as type CLOCK_NODE, and we don't pick them
		 * up when we're assigning top-level inputs to lines[], so we
		 * need to check them here.
		 */
		for (i = 0; i < node->num_output_pins; i++)
			update_pin_value(node->output_pins[i], cycle % 2, cycle);
		free(temp1);
		free(temp2);
		return;
	}
	
	if (strcmp(node->name, RESET_PORT_NAME) == 0)
	{
		for (i = 0; i < node->num_output_pins; i++)
		{
			update_pin_value(node->output_pins[i], 0, cycle);
		}
	}

	free(temp1);
	free(temp2);
#ifdef DEBUG_SIMULATOR
	if (node->type == FF_NODE)
	{
		printf("*** Computing a Flip Flop %s on cycle %d\n", node->name, cycle);
	}
#endif

	/*
	 * The behaviour defined in these case statements reflect
	 * the logic in the output_blif.c file.
	 */
	switch(node->type)
	{
		case LT:				// < 010 1
		{
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			if (node->input_pins[0]->sim_state->value < 0 ||
					node->input_pins[1]->sim_state->value < 0 ||
					node->input_pins[2]->sim_state->value < 0)
			{
				update_pin_value(node->output_pins[0], -1, cycle);
				return;
			}

			if (node->input_pins[0]->sim_state->value == 0 &&
					node->input_pins[1]->sim_state->value == 1 &&
					node->input_pins[2]->sim_state->value == 0)
			{
				update_pin_value(node->output_pins[0], 1, cycle);
				return;
			}
			update_pin_value(node->output_pins[0], 0, cycle);
			return;
		}
		case GT:				// > 100 1
		{
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			if (node->input_pins[0]->sim_state->value < 0 ||
					node->input_pins[1]->sim_state->value < 0 ||
					node->input_pins[2]->sim_state->value < 0)
			{
				update_pin_value(node->output_pins[0], -1, cycle);
				return;
			}

			if (node->input_pins[0]->sim_state->value == 1 &&
					node->input_pins[1]->sim_state->value == 0 &&
					node->input_pins[2]->sim_state->value == 0)
			{
				update_pin_value(node->output_pins[0], 1, cycle);
				return;
			}
			update_pin_value(node->output_pins[0], 0, cycle);
			return;
		}
		case ADDER_FUNC:		// 001 1\n010 1\n100 1\n111 1
		{
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			if (node->input_pins[0]->sim_state->value < 0 ||
					node->input_pins[1]->sim_state->value < 0 ||
					node->input_pins[2]->sim_state->value < 0)
			{
				update_pin_value(node->output_pins[0], -1, cycle);
				return;
			}

			if ((node->input_pins[0]->sim_state->value == 0 &&
					node->input_pins[1]->sim_state->value == 0 &&
					node->input_pins[2]->sim_state->value == 1) ||
				(node->input_pins[0]->sim_state->value == 0 &&
					node->input_pins[1]->sim_state->value == 1 &&
					node->input_pins[2]->sim_state->value == 0) ||
				(node->input_pins[0]->sim_state->value == 1 &&
					node->input_pins[1]->sim_state->value == 0 &&
					node->input_pins[2]->sim_state->value == 0) ||
				(node->input_pins[0]->sim_state->value == 1 &&
					node->input_pins[1]->sim_state->value == 1 &&
					node->input_pins[2]->sim_state->value == 1))
			{
				update_pin_value(node->output_pins[0], 1, cycle);
				return;
			}
			update_pin_value(node->output_pins[0], 0, cycle);
			return;
		}
		case CARRY_FUNC:		// 011 1\n100 1\n110 1\n111 1
		{
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			if ((node->input_pins[0]->sim_state->value == 1 &&
					node->input_pins[1]->sim_state->value == 0 &&
					node->input_pins[2]->sim_state->value == 0) ||
				(node->input_pins[0]->sim_state->value == 1 &&
					node->input_pins[1]->sim_state->value == 1 &&
					node->input_pins[2]->sim_state->value == 0) ||
				(node->input_pins[1]->sim_state->value == 1 &&
					node->input_pins[2]->sim_state->value == 1))
			{
				update_pin_value(node->output_pins[0], 1, cycle);
				return;
			}

			if (node->input_pins[0]->sim_state->value < 0 ||
					node->input_pins[1]->sim_state->value < 0 ||
					node->input_pins[2]->sim_state->value < 0)
			{
				update_pin_value(node->output_pins[0], -1, cycle);
				return;
			}

			update_pin_value(node->output_pins[0], 0, cycle);
			return;
		}
		case BITWISE_NOT:		//
		{
			oassert(node->num_input_pins == 1);
			oassert(node->num_output_pins == 1);
			if (node->input_pins[0]->sim_state->value < 0)
				update_pin_value(node->output_pins[0], -1, cycle);
			else if (node->input_pins[0]->sim_state->value == 1)
				update_pin_value(node->output_pins[0], 0, cycle);
			else
				update_pin_value(node->output_pins[0], 1, cycle);
			return;
		}
		case LOGICAL_AND:		// &&
		{
			oassert(node->num_output_pins == 1);

			for (i = 0; i < node->num_input_pins; i++)
			{
				if (node->input_pins[i]->sim_state->value < 0)
				{
					update_pin_value(node->output_pins[0], -1, cycle);
					return;
				}
				if (node->input_pins[i]->sim_state->value == 0)
				{
					update_pin_value(node->output_pins[0], 0, cycle);
					return;
				}
			}
			update_pin_value(node->output_pins[0], 1, cycle);
			return;
		}
		case LOGICAL_OR:		// ||
		{
			oassert(node->num_output_pins == 1);
			for (i = 0; i < node->num_input_pins; i++)
			{
				if (node->input_pins[i]->sim_state->value < 0)
					unknown = TRUE;
				if (node->input_pins[i]->sim_state->value == 1)
				{
					update_pin_value(node->output_pins[0], 1, cycle);
					return;
				}
			}

			if (unknown)
				update_pin_value(node->output_pins[0], -1, cycle);
			else
				update_pin_value(node->output_pins[0], 0, cycle);
			return;
		}
		case LOGICAL_NAND:		// !&&
		{
			int retVal;
			oassert(node->num_output_pins == 1);

			retVal = 0;
			for (i = 0; i < node->num_input_pins; i++)
			{
				if (node->input_pins[i]->sim_state->value < 0)
				{
					update_pin_value(node->output_pins[0], -1, cycle);
					return;
				}
				if (node->input_pins[i]->sim_state->value == 0)
				{
					retVal = 1;
				}
			}

			update_pin_value(node->output_pins[0], retVal, cycle);
			return;
		}
		case LOGICAL_NOT:		// !
		case LOGICAL_NOR:		// !|
		{
			oassert(node->num_output_pins == 1);

			for (i = 0; i < node->num_input_pins; i++)
			{
				if (node->input_pins[i]->sim_state->value < 0)
				{
					update_pin_value(node->output_pins[0], -1, cycle);
					return;
				}
				if (node->input_pins[i]->sim_state->value == 1)
				{
					update_pin_value(node->output_pins[0], 0, cycle);
					return;
				}
			}
			update_pin_value(node->output_pins[0], 1, cycle);
			return;
		}
		case LOGICAL_EQUAL:		// ==
		case LOGICAL_XOR:		// ^
		{
			long long ones;

			oassert(node->num_output_pins == 1);

			ones = 0;
			for (i = 0; i < node->num_input_pins; i++)
			{
				if (node->input_pins[i]->sim_state->value < 0)
					unknown = TRUE;
				if (node->input_pins[i]->sim_state->value == 1)
					ones++;
			}
			if (unknown)
				update_pin_value(node->output_pins[0], -1, cycle);
			else
			{
				if (ones % 2 == 1)
					update_pin_value(node->output_pins[0], 1, cycle);
				else
					update_pin_value(node->output_pins[0], 0, cycle);
			}
			return;
		}
		case NOT_EQUAL:			// !=
		case LOGICAL_XNOR:		// !^
		{
			long long ones;

			oassert(node->num_output_pins == 1);

			ones = 0;
			for (i = 0; i < node->num_input_pins; i++)
			{
				if (node->input_pins[i]->sim_state->value < 0)
					unknown = TRUE;
				if (node->input_pins[i]->sim_state->value == 1)
					ones++;
			}

			if (unknown)
				update_pin_value(node->output_pins[0], -1, cycle);
			else
			{
				if (ones % 2 == 0)
					update_pin_value(node->output_pins[0], 1, cycle);
				else
					update_pin_value(node->output_pins[0], 0, cycle);
			}
			return;
		}
		case MUX_2:
		{
			oassert(node->num_output_pins == 1);
			oassert(node->num_input_port_sizes >= 2);
			oassert(node->input_port_sizes[0] == node->input_port_sizes[1]);

			for (i = 0; i < node->input_port_sizes[0]; i++)
			{
				if (node->input_pins[i]->sim_state->value < 0)
					unknown = TRUE;
				if (node->input_pins[i]->sim_state->value == 1 &&
						node->input_pins[i]->sim_state->value ==
						node->input_pins[i+node->input_port_sizes[0]]->sim_state->value)
				{
					update_pin_value(node->output_pins[0], 1, cycle);
					return;
				}
			}

			if (unknown)
				update_pin_value(node->output_pins[0], -1, cycle);
			else
				update_pin_value(node->output_pins[0], 0, cycle);

			return;
		}
		case FF_NODE:
		{
			oassert(node->num_output_pins == 1);
			oassert(node->num_input_pins == 2);
			
			
			/*
			if (node->input_pins[1]->sim_state->value % 2 == 1)	//rising edge of clock
				update_pin_value(node->output_pins[0], node->input_pins[0]->sim_state->value, cycle);
			else//falling edge of clock
			{
				int prev;

				prev = node->output_pins[0]->sim_state->prev_value;
				
				update_pin_value(node->output_pins[0], node->output_pins[0]->sim_state->value, cycle);
				node->output_pins[0]->sim_state->prev_value = prev;
			}
			*/
			return;
		}
		case MEMORY:
		{
			int i;

			char *clock_name = "clk";

			char *we_name = "we";
			char *addr_name = "addr";
			char *data_name = "data";

			char *we_name1 = "we1";
			char *addr_name1 = "addr1";
			char *data_name1 = "data1";
			char *we_name2 = "we2";
			char *addr_name2 = "addr2";
			char *data_name2 = "data2";
			char *out_name1 = "out1";
			char *out_name2 = "out2";

			oassert(strcmp(node->related_ast_node->children[0]->types.identifier, 
						SINGLE_PORT_MEMORY_NAME) == 0 ||
					strcmp(node->related_ast_node->children[0]->types.identifier, 
						DUAL_PORT_MEMORY_NAME) == 0);
			
			if (strcmp(node->related_ast_node->children[0]->types.identifier, 
						SINGLE_PORT_MEMORY_NAME) == 0)
			{
				int we;
				int clock;
				int data_width = 0;
				int addr_width = 0;
				npin_t **addr = NULL;
				npin_t **data = NULL;
				npin_t **out = NULL;
				
				for (i = 0; i < node->num_input_pins; i++)
				{
					if (strcmp(node->input_pins[i]->mapping, we_name) == 0)
						we = node->input_pins[i]->sim_state->value;
					else if (strcmp(node->input_pins[i]->mapping, addr_name) == 0)
					{
						if (addr == NULL)
							addr = &node->input_pins[i];
						addr_width++;
					}
					else if (strcmp(node->input_pins[i]->mapping, data_name) == 0)
					{
						if (data == NULL)
							data = &node->input_pins[i];
						data_width++;
					}
					else if (strcmp(node->input_pins[i]->mapping, clock_name) == 0)
						clock = node->input_pins[i]->sim_state->value;
				}
				out = node->output_pins;

				if (node->type == MEMORY &&
						node->memory_data == NULL)
				{
					instantiate_memory(node, &(node->memory_data), data_width, addr_width);
				}

				compute_memory(data, out, data_width, addr, addr_width, 
						we, clock, cycle, node->memory_data);
			}
			else
			{
				int clock;
				int we1;
				int data_width1 = 0;
				int addr_width1 = 0;
				npin_t **addr1 = NULL;
				npin_t **data1 = NULL;
				npin_t **out1 = NULL;
				int we2;
				int data_width2 = 0;
				int addr_width2 = 0;
				npin_t **addr2 = NULL;
				npin_t **data2 = NULL;
				npin_t **out2 = NULL;

				for (i = 0; i < node->num_input_pins; i++)
				{
					if (strcmp(node->input_pins[i]->mapping, we_name1) == 0)
						we1 = node->input_pins[i]->sim_state->value;
					else if (strcmp(node->input_pins[i]->mapping, we_name2) == 0)
						we2 = node->input_pins[i]->sim_state->value;
					else if (strcmp(node->input_pins[i]->mapping, addr_name1) == 0)
					{
						if (addr1 == NULL)
							addr1 = &node->input_pins[i];
						addr_width1++;
					}
					else if (strcmp(node->input_pins[i]->mapping, addr_name2) == 0)
					{
						if (addr2 == NULL)
							addr2 = &node->input_pins[i];
						addr_width2++;
					}
					else if (strcmp(node->input_pins[i]->mapping, data_name1) == 0)
					{
						if (data1 == NULL)
							data1 = &node->input_pins[i];
						data_width1++;
					}
					else if (strcmp(node->input_pins[i]->mapping, data_name2) == 0)
					{
						if (data2 == NULL)
							data2 = &node->input_pins[i];
						data_width2++;
					}
					else if (strcmp(node->input_pins[i]->mapping, clock_name) == 0)
						clock = node->input_pins[i]->sim_state->value;
				}
				for (i = 0; i < node->num_output_pins; i++)
				{
					if (strcmp(node->output_pins[i]->mapping, out_name1) == 0)
					{
						if (out1 == NULL)
							out1 = &node->output_pins[i];
					}
					else if (strcmp(node->output_pins[i]->mapping, out_name2) == 0)
					{
						if (out2 == NULL)
							out2 = &node->output_pins[i];
					}
				}

				if (node->memory_data == NULL)
				{
					instantiate_memory(node, &(node->memory_data), data_width2, addr_width2);
				}

				compute_memory(data1, out1, data_width1, addr1, addr_width1, we1, clock, cycle, node->memory_data);
				compute_memory(data2, out2, data_width2, addr2, addr_width2, we2, clock, cycle, node->memory_data);
			}

			return;
		}
		case HARD_IP:
		{
			int k;
			int *input_pins = malloc(sizeof(int)*node->num_input_pins);
			int *output_pins = malloc(sizeof(int)*node->num_output_pins);
			
			oassert(node->input_port_sizes[0] > 0);
			oassert(node->output_port_sizes[0] > 0);

			if (node->simulate_block_cycle == NULL)
			{
				void *handle;
				char *error;
				void (*func_pointer)(int, int, int*, int, int*);
				char *filename = malloc(sizeof(char)*strlen(node->name));
				
				if (index(node->name, '.') == NULL)
					error_message(SIMULATION_ERROR, -1, -1, "Couldn't extract the name of a shared library for hard-block simulation");
				
				//we're extracting "hardblocktype+instancename.so" from "module.hardblocktype+instancename"
				snprintf(filename, sizeof(char)*strlen(node->name), "%s.so", index(node->name, '.')+1);
				
				handle = dlopen(filename, RTLD_LAZY);
				if (!handle)
				{
					error_message(SIMULATION_ERROR, -1, -1, "Couldn't open a shared library for hard-block simulation: %s", dlerror());
				}
				dlerror();//clear any existing errors
				func_pointer = (void(*)(int, int, int*, int, int*))dlsym(handle, "simulate_block_cycle");
				if ((error = dlerror()) != NULL)
				{
					error_message(SIMULATION_ERROR, -1, -1, "Couldn't load a shared library method for hard-block simulation: %s", error);
				}

				node->simulate_block_cycle = func_pointer;
				enqueue_item(blocks, handle);
				free(filename);
			}
			
			//extract values of input pins into int array
			for (k = 0; k < node->num_input_pins; k++)
			{
				input_pins[k] = node->input_pins[k]->sim_state->value;
			}
			
			//invoke hardblock simulation call
			(node->simulate_block_cycle)(cycle, node->num_input_pins, input_pins, node->num_output_pins, output_pins);
			
			//extra values of output array into output pins.
			for (k = 0; k < node->num_output_pins; k++)
			{
				update_pin_value(node->output_pins[k], output_pins[k], cycle);
			}
			
			free(input_pins);
			free(output_pins);

			return;
		}
		case MULTIPLY:
		{
			int *a, *b, *result;

			oassert(node->num_input_port_sizes >= 2);
			oassert(node->num_output_port_sizes == 1);

			a = malloc(sizeof(int)*node->input_port_sizes[0]);
			b = malloc(sizeof(int)*node->input_port_sizes[1]);

			//get our input arrays ready
			for (i = 0; i < node->input_port_sizes[0]; i++)
			{
				a[i] = node->input_pins[i]->sim_state->value;
				if (a[i] < 0)
					unknown = TRUE;
			}
			for (i = 0; i < node->input_port_sizes[1]; i++)
			{
				b[i] = node->input_pins[node->input_port_sizes[0] + i]->sim_state->value;
				if (b[i] < 0)
					unknown = TRUE;
			}

			if (unknown)
			{
				for (i = 0; i < node->num_output_pins; i++)
				{
					update_pin_value(node->output_pins[i], -1, cycle);
				}
			}

			//multiply our values
			result = multiply_arrays(a, node->input_port_sizes[0], b, node->input_port_sizes[1]);

			//get our output values from result[]
			for (i = 0; i < node->num_output_pins; i++)
			{
				update_pin_value(node->output_pins[i], result[i], cycle);
			}

			//free memory
			free(result);
			free(a);
			free(b);
			return;
		}
		case INPUT_NODE:
		case OUTPUT_NODE:
			return;
		case PAD_NODE:
		case CLOCK_NODE:
		case GND_NODE:
		case VCC_NODE:
			return;
		/* These should have already been converted to softer versions. */
		case BITWISE_AND:
		case BITWISE_NAND:
		case BITWISE_NOR:
		case BITWISE_XNOR:
		case BITWISE_XOR:
		case BITWISE_OR:
		case BUF_NODE:
		case MULTI_PORT_MUX:
		case SL:
		case SR:
		case CASE_EQUAL:
		case CASE_NOT_EQUAL:
		case DIVIDE:
		case MODULO:
		case GTE:
		case LTE:
		case ADD:
		case MINUS:
		default:
			/* these nodes should have been converted to softer versions */
			error_message(SIMULATION_ERROR, -1, -1, "Node should have been converted to softer version: %s", node->name);
	}
}

int is_node_top_level(netlist_t *netlist, nnode_t *node)
{
	int i;

	for (i = 0; i < netlist->num_top_input_nodes; i++)
		if (node == netlist->top_input_nodes[i])
			return TRUE;
	for (i = 0; i < netlist->num_top_output_nodes; i++)
		if (node == netlist->top_output_nodes[i])
			return TRUE;
	return FALSE;
}

/*
 * Given a line_t* reference and a string token, this will update the
 * line's pins to the value reflected in the token.
 *
 * If the hex or binary string isn't long enough for all the pins,
 * this function will update their cycle only (ie: the retain their
 * previous value).
 */
void store_value_in_line(char *token, line_t *line, int cycle)
{
	int i, j, k;
	oassert(token != NULL);
	oassert(strlen(token) != 0);

	//This shouldn't happen because lines are mapped *from* the top_input/output_nodes
	if (line->number_of_pins < 1)
	{
		warning_message(SIMULATION_ERROR, -1, -1, "Found a line '%s' with no pins.", line->name);
		return;
	}

	if (strlen(token) == 1)
	{
		/*
		 * means that there is only one pin, so we can assume the only input is either 0 or 1
		 */
		if (token[0] == '0')
			update_pin_value(line->pins[0], 0, cycle);
		else if (token[0] == '1')
			update_pin_value(line->pins[0], 1, cycle);
		else
			update_pin_value(line->pins[0], -1, cycle);

		return;
	}

	/*
	 * means that we have more than one pin
	 * token may be a string of 0's and 1's or a hex value
	 */
	if (token[0] == '0' && (token[1] == 'x' || token[1] == 'X'))
	{	//this is a hex string
		int token_length;

		token += 2;	//gets rid of the 0x prefix
		token_length = strlen(token);

		//we're reversing the token, so "0x9A6" becomes "6A9"
		//this makes processing this value and storing it easier
		i = 0, j = token_length - 1;
		while(i < j)
		{
			char temp;
			temp = token[i];
			token[i] = token [j];
			token[j] = temp;
			i++; j--;
		}

		//from the least-significant character in the hex string to the most significant
		for (i = 0; i < token_length; i++)
		{
			int value;
			char temp[2];

			//store our character in it's own 'string'
			temp[0] = token[i];
			temp[1] = '\0';

			value = strtol(temp, NULL, 16);

			//value is a int between 0 and 15 whose binary representation is the vaue for 4 pins
			for (k = 0; k < 4 && k + (i*4) < line->number_of_pins; k++)
			{
				int pin_value;

				pin_value = (value & (1 << k)) > 0 ? 1 : 0;
				update_pin_value(line->pins[k + (i*4)], pin_value, cycle);
			}
		}

		//this ensures every pin that wasn't updated already is updated, for this cycle
		for (; k + (i*4) < line->number_of_pins; k++)
			update_pin_value(line->pins[k + (i*4)], line->pins[k + (i*4)]->sim_state->value, cycle);
	}
	else
	{	//this is a string of 1's and 0's
		for (i = strlen(token) - 1, j = 0; i >= 0 && j < line->number_of_pins; i--, j++)
		{
			if (token[i] == '0')
				update_pin_value(line->pins[j], 0, cycle);
			else if (token[i] == '1')
				update_pin_value(line->pins[j], 1, cycle);
			else
				update_pin_value(line->pins[j], -1, cycle);
		}
		for (; j < line->number_of_pins; j++)
			update_pin_value(line->pins[j], -1, cycle);
	}
}

/*
 * Given a node, this function finds a corresponding entry in the lines array.
 * If not found, it will create a reference for it.
 */
void assign_node_to_line(nnode_t *node, line_t **lines, int lines_size, int type)
{
	int j, found;
	int pin_number;		//only used in multi-pin
	char *port_name;
	char *tilde;

	//this is larger than we need, but we'll free it later
	port_name = malloc(sizeof(char)*strlen(node->name));

	//copy the port name from the node's name, starting at the node's hat character
	strcpy(port_name, strchr(node->name, '^') + 1);
	//find the tilde, or NULL if it's not present
	tilde = strchr(port_name, '~');

	if (tilde == FALSE)		//single-bit width input
	{
		found = FALSE;
		//look for a line that's already there.
		//These are guaranteed to be here because we map lines from top level nodes.
		//this shouldn't be necessary, but it can't hurt
		for (j = 0; j < lines_size; j++)
		{
			if (strcmp(lines[j]->name, port_name) == 0)
			{
				found = TRUE;
				break;
			}
		}

		if (found == FALSE)
		{
			 if (strcmp(port_name, CLOCK_PORT_NAME_1) == 0 ||
					strcmp(port_name, CLOCK_PORT_NAME_2) == 0)
			 {
				 return;
			 }
//			if (strcmp(port_name, RESET_PORT_NAME) == 0)

			if (node->type == GND_NODE ||
				node->type == VCC_NODE ||
				node->type == PAD_NODE ||
				node->type == CLOCK_NODE)
				return;
				
			warning_message(SIMULATION_ERROR, -1, -1, "Could not map single-bit top-level input node '%s' to input vector", node->name);
			return;
		}
		//j is equal to the index of the correct line

		//we may need to allocate pins if this is a top_output_nodes[] member
		if (node->num_output_pins == 0)
		{
			npin_t *pin = allocate_npin();
			allocate_more_node_output_pins(node, 1);
			add_a_output_pin_to_node_spot_idx(node, pin, 0);
		}

		lines[j]->number_of_pins = 1;
		lines[j]->max_number_of_pins = 1;
		lines[j]->pins = malloc(sizeof(npin_t *));
		lines[j]->pins[0] = node->output_pins[0];
		lines[j]->type = type;
#ifdef DEBUG_SIMULATOR
		printf("connecting %s only pin to %s\n", port_name, node->name);
#endif
		return;
	}
	//multi-bit width input
	pin_number = atoi(tilde+1);	//the string directly after the ~
	*tilde = '\0';	//change the tilde to \0 so we can use string library functions on port_name

	found = FALSE;
	for (j = 0; j < lines_size; j++)
	{
		if (strcmp(lines[j]->name, port_name) == 0)
		{
			found = TRUE;
			break;
		}
	}
	if (found == FALSE)
	{
		warning_message(SIMULATION_ERROR, -1, -1, "Could not map multi-bit top-level input node '%s' to input vector", node->name);
		return;
	}
	//j is equal to the index of the correct line

	//expand out number of pins in this line_t* if necessary
	if (lines[j]->max_number_of_pins < 0)
	{
		// this is the first time we've added a pin
		lines[j]->max_number_of_pins = 8;
		lines[j]->pins = malloc(sizeof(npin_t*)*lines[j]->max_number_of_pins);
	}
	if (lines[j]->max_number_of_pins <= pin_number)
	{
		//if we run out, then increase them by 64. Why not.
		while (lines[j]->max_number_of_pins <= pin_number)
			lines[j]->max_number_of_pins += 64;
		lines[j]->pins = realloc(lines[j]->pins, sizeof(npin_t*)*lines[j]->max_number_of_pins);
	}

#ifdef DEBUG_SIMULATOR
	printf("connecting %s pin %d to %s\n", port_name, pin_number, node->name);
#endif

	/*
	 * always assign to output pin; if it's an input line, then this is
	 * where it should go. if it's an output line, then we'll compare
	 * that node's input pins to output pins to verify the simulation
	 */
	if (node->num_output_pins == 0)
	{
		//an output node only has input pins, so add one.
		npin_t *pin = allocate_npin();
		allocate_more_node_output_pins(node, 1);
		add_a_output_pin_to_node_spot_idx(node, pin, 0);
	}

	lines[j]->pins[pin_number] = node->output_pins[0];
	lines[j]->type = type;
	lines[j]->number_of_pins++;
}

/*
 * Given a netlist, this function maps the top_input_nodes and
 * top_output_nodes to a line_t* each. It stores them in an array
 * and returns it, storing the array size in the *lines_size
 * pointer.
 */
line_t **create_test_vector_lines(int *lines_size, netlist_t *netlist)
{
	line_t **lines;
	int i, j, current_line, pin_number, found;
	char *port_name, *tilde;

	//start with no lines
	current_line = 0;
	//malloc the absolute largest array possible (each top-level node is one pin)
	lines = malloc(sizeof(line_t*)*(netlist->num_top_input_nodes + netlist->num_top_output_nodes));

	//used when freeing memory
	port_name = NULL;

	for (i = 0; i < netlist->num_top_input_nodes; i++)
	{
		//make sure to free memory!
		if (NULL != port_name)
			free(port_name);

		//this is larger than we need, but we'll free the rest anyways
		port_name = malloc(sizeof(char)*strlen(netlist->top_input_nodes[i]->name));
		strcpy(port_name, strchr(netlist->top_input_nodes[i]->name, '^') + 1);

		//look for a ~
		tilde = strchr(port_name, '~');
		if (NULL == tilde)	//single-bit
		{
			//skip the clock and reset port - not sure why netlist->clock_node doesn't map correctly
			if (strcmp(port_name, CLOCK_PORT_NAME_1) == 0 ||
					strcmp(port_name, CLOCK_PORT_NAME_2) == 0)// ||
//					strcmp(port_name, RESET_PORT_NAME) == 0)
				continue;

			//create the port
			lines[current_line] = create_line(port_name);
			lines[current_line]->number_of_pins = 1;
			lines[current_line]->max_number_of_pins = 1;
			lines[current_line]->pins = malloc(sizeof(npin_t *));
			lines[current_line]->pins[0] = netlist->top_input_nodes[i]->output_pins[0];
			lines[current_line]->type = INPUT;
#ifdef DEBUG_SIMULATOR
			printf("connecting %s only pin to %s\n", lines[current_line]->name, netlist->top_input_nodes[i]->name);
#endif
			current_line++;
			continue;
		}
		//means we have multiple pins. The node->name is "top^port_name~pin#", so
		//take the string from one after the tilde to get the pin number. Then,
		//we can change the ~ to a '\0', letting use use the port_name as it's own
		//string. This doesn't change the node's reference, since we copied
		//our own port name

		pin_number = atoi(tilde+1);
		*tilde = '\0';

		if (strcmp(port_name, CLOCK_PORT_NAME_1) == 0 ||
				strcmp(port_name, CLOCK_PORT_NAME_2) == 0 ||
				strcmp(port_name, RESET_PORT_NAME) == 0)
			continue;

		//we need to look for a port that already exists that has the name we're
		//looking for. If we can't find it, it means we're the first
		found = FALSE;
		for (j = 0; j < current_line && found == FALSE; j++)
		{
			if (strcmp(lines[j]->name, port_name) == 0)
				found = TRUE;
		}

		if (found == FALSE)
		{
			lines[current_line] = create_line(port_name);
			current_line++;
		}
		else j--;	//if we found it, then j is equal to one more than the correct reference

		//this is done the first time
		if (lines[j]->max_number_of_pins < 0)
		{
			lines[j]->max_number_of_pins = 8;
			lines[j]->pins = malloc(sizeof(npin_t*)*lines[j]->max_number_of_pins);
		}

		//expand out number of pins in this line_t* if necessary
		//this is necessary because we might find pin 9 before, say,
		//pin 2. So we keep a max number of pins, and hope that the
		//the array is eventually filled out
		if (lines[j]->max_number_of_pins <= pin_number)
		{
			while (lines[j]->max_number_of_pins <= pin_number)
				lines[j]->max_number_of_pins += 64;
			lines[j]->pins = realloc(lines[j]->pins, sizeof(npin_t*)*lines[j]->max_number_of_pins);
		}

		//we're going to assume that a node has only one pin
		lines[j]->pins[pin_number] = netlist->top_input_nodes[i]->output_pins[0];
		lines[j]->type = INPUT;
		lines[j]->number_of_pins++;

#ifdef DEBUG_SIMULATOR
		printf("connecting %s pin %d to %s\n", port_name, pin_number, netlist->top_input_nodes[i]->name);
#endif
	}

	//this loop is almost identical to the one above it, with a few keep differences
	//pointed out in the comments
	for (i = 0; i < netlist->num_top_output_nodes; i++)
	{
		if (port_name != NULL)
			free(port_name);

		port_name = malloc(sizeof(char)*strlen(netlist->top_output_nodes[i]->name));
		strcpy(port_name, strchr(netlist->top_output_nodes[i]->name, '^') + 1);

		tilde = strchr(port_name, '~');
		if (NULL == tilde)	//single-bit
		{
			if (netlist->top_output_nodes[i]->num_output_pins == 0)
			{
				npin_t *pin = allocate_npin();
				allocate_more_node_output_pins(netlist->top_output_nodes[i], 1);
				add_a_output_pin_to_node_spot_idx(netlist->top_output_nodes[i], pin, 0);
			}

			lines[current_line] = create_line(port_name);
			lines[current_line]->number_of_pins = 1;
			lines[current_line]->max_number_of_pins = 1;
			lines[current_line]->pins = malloc(sizeof(npin_t *));
			lines[current_line]->pins[0] = netlist->top_output_nodes[i]->output_pins[0];
			lines[current_line]->type = OUTPUT; //difference: a different type of pin
#ifdef DEBUG_SIMULATOR
			printf("connecting %s only pin to %s\n", lines[current_line]->name, netlist->top_output_nodes[i]->name);
#endif
			current_line++;
			continue;
		}

		pin_number = atoi(tilde+1);
		*tilde = '\0';
		found = FALSE;
		for (j = 0; j < current_line && found == FALSE; j++)
		{
			if (strcmp(lines[j]->name, port_name) == 0)
				found = TRUE;
		}
		if (found == FALSE)
		{
			lines[current_line] = create_line(port_name);
			current_line++;
		}
		else j--;


		if (lines[j]->max_number_of_pins < 0)
		{
			lines[j]->max_number_of_pins = 8;
			lines[j]->pins = malloc(sizeof(npin_t*)*lines[j]->max_number_of_pins);
		}

		//expand out number of pins in this line_t* if necessary
		if (lines[j]->max_number_of_pins <= pin_number)
		{
			while (lines[j]->max_number_of_pins <= pin_number)
				lines[j]->max_number_of_pins += 64;
			lines[j]->pins = realloc(lines[j]->pins, sizeof(npin_t*)*lines[j]->max_number_of_pins);
		}

		//difference:
		//top-level output nodes don't have output pins; they only have an input pin.
		//we're going to add output pins here. Then, in order to verify test vector
		//simulation, we'll store the expected value in the output pin, simulate,
		//and compare the input and output pins to see if they match.
		if (netlist->top_output_nodes[i]->num_output_pins == 0)
		{
			npin_t *pin = allocate_npin();
			allocate_more_node_output_pins(netlist->top_output_nodes[i], 1);
			add_a_output_pin_to_node_spot_idx(netlist->top_output_nodes[i], pin, 0);
		}

		lines[j]->pins[pin_number] = netlist->top_output_nodes[i]->output_pins[0];
		lines[j]->type = OUTPUT;
		lines[j]->number_of_pins++;
#ifdef DEBUG_SIMULATOR
		printf("connecting %s pin %d to %s\n", port_name, pin_number, netlist->top_output_nodes[i]->name);
#endif
	}
	if (port_name != NULL)
		free(port_name);

	*lines_size = current_line;
	return lines;
}

/*
 * Writes the lines[] elements' names to the files, depending on the
 * line type of each element, followed by a newline at the very end of
 * each file
 */
void write_vector_headers(FILE *iv, FILE *outv, line_t **lines, int lines_size)
{
	int i, first_in, first_out;

	first_in = TRUE;
	first_out = TRUE;

	//the first time, we print out the name; subsequently, we print
	//out a space, then the name, so the output files can be re-read
	for (i = 0; i < lines_size; i++)
	{
		if (lines[i]->type == INPUT)
		{
			if (first_in == TRUE)
			{
				fprintf(iv, "%s", lines[i]->name);
				first_in = FALSE;
				continue;
			}
			fprintf(iv, " %s", lines[i]->name);
		}
		else	//lines[i]->type == OUTPUT
		{
			if (first_out == TRUE)
			{
				fprintf(outv, "%s", lines[i]->name);
				first_out = FALSE;
				continue;
			}
			fprintf(outv, " %s", lines[i]->name);
		}
	}
	fprintf(iv, "\n");
	fprintf(outv, "\n");
}

/*
 * Reads in headers from a file and assigns them to elements in the
 * array of line_t * it returns.
 */
line_t** read_test_vector_headers(FILE *in, int *lines_size, int max_lines_size)
{
	char buffer [BUFFER_MAX_SIZE];
	char next;
	int buffer_length;
	int current_line;
	line_t **lines;

	lines = malloc(sizeof(line_t*)*max_lines_size);

	current_line = 0;
	buffer[0] = '\0';
	buffer_length = 0;

	//get our next character
	next = fgetc(in);
	while (TRUE)
	{
		//if the next character is a space, tab or newline, then it's the end of our old token
		if (next == ' ' || next == '\t' || next == '\n')
		{
			//if we haven't been reading in a token, we're either at the end of the
			//line or we are in a space between tokens
			if (buffer_length == 0)
			{
				//newline means we've read this vector
				if (next == '\n')
					break;
				continue;
			}

			//if we get here, it's because we finished reading in a token
			lines[current_line] = create_line(buffer);

			current_line++;
			buffer_length = 0;

			if (next == '\n')
				break;
		}
		else
		{
			buffer[buffer_length] = next;
			buffer_length++;
			buffer[buffer_length] = '\0';
		}
		next = fgetc(in);
	}
	*lines_size = current_line;
	return lines;
}

/*
 * allocates memory for and instantiates a line_t struct
 */
line_t *create_line(char *name)
{
	line_t *line;

	line = malloc(sizeof(line_t));
	line->number_of_pins = 0;
	line->max_number_of_pins = -1;
	line->pins = NULL;
	line->type = -1;
	line->name = malloc(sizeof(char)*(strlen(name)+1));
	strcpy(line->name, name);

	return line;
}

/*
 * frees each element in lines[] and the array itself
 */
void free_lines(line_t **lines, int lines_size)
{
	int i;
	for (i = 0; i < lines_size; i++)
	{
		free(lines[i]->name);
		free(lines[i]->pins);
		free(lines[i]);
	}
	free(lines);
}

/*
 * Given a test vector {1..n}, assigns the ith vector's value to lines[i].
 *
 * NOTE: The use of strtok() on buffer means THIS WILL MODIFY its contents
 */
void assign_input_vector_to_lines(line_t **lines, char *buffer, int cycle)
{
	const char *delim;
	int length, line_count;
	char *token;

	delim = " \t";
	length = strlen(buffer);

	//handle this. Not sure if we need to be so careful, but it can't hurt.
	if(buffer[length-2] == '\r' || buffer[length-2] == '\n')
		buffer[length-2] = '\0';
	if(buffer[length-1] == '\r' || buffer[length-1] == '\n')
		buffer[length-1] = '\0';

	line_count = 0;
	token = strtok(buffer, delim);

	while (token != NULL)
	{
		store_value_in_line(token, lines[line_count], cycle);

		token = strtok(NULL, delim);
		line_count++;
	}
}

/*
 * Assigns a random 0 or 1 to each pin of each line in lines[] which
 * has line->type == INPUT.
 *
 * NOTE: This presupposes that the randomizer is seeded appropriately.
 */
void assign_random_vector_to_input_lines(line_t **lines, int lines_size, int cycle)
{
	int i;

	for (i = 0; i < lines_size; i++)
	{
		int j;

		if (lines[i]->type == OUTPUT)
			continue;
		if (strcmp(lines[i]->name, CLOCK_PORT_NAME_1) == 0 ||
				strcmp(lines[i]->name, CLOCK_PORT_NAME_2) == 0)
		{
			update_pin_value(lines[i]->pins[0], cycle%2, cycle);
			continue;
		}
		if (strcmp(lines[i]->name, RESET_PORT_NAME) == 0 ||
				lines[i]->type == GND_NODE ||
				lines[i]->type == PAD_NODE)
		{
			if (cycle == 0)
				update_pin_value(lines[i]->pins[0], 1, cycle);
			else
				update_pin_value(lines[i]->pins[0], 0, cycle);
			continue;
		}
		if (lines[i]->type == VCC_NODE)
		{
			update_pin_value(lines[i]->pins[0], 1, cycle);
			continue;
		}

		for (j = 0; j < lines[i]->number_of_pins; j++)
		{
			int r = rand();
			update_pin_value(lines[i]->pins[j], r % 2, cycle);
		}
	}
}

/*
 * Writes all line values in lines[] such that line->type == type to the
 * file specified by the file parameter.
 */
void write_vectors_to_file(line_t **lines, int lines_size, FILE *file, int type, int cycle)
{
	int i, first;

	first = TRUE;

	for (i = 0; i < lines_size; i++)
	{
		if (lines[i]->type != type)
			continue;

		//the first vector is printed alone, the rest are prefixed by a space
		if (first)
			first = FALSE;
		else
			fprintf(file, " ");

		if (lines[i]->number_of_pins == 1)	//single-bit
		{
			npin_t *pin;

			//we make this distinction because sometimes extra padding, say,
			//with mutlipliers, will overwrite values on OUTPUT nodes. This gets
			//the true value from the net that drives this pin
			if (type == INPUT)
				pin = lines[i]->pins[0];
			else //type == OUTPUT
				pin = lines[i]->pins[0]->node->input_pins[lines[i]->pins[0]->pin_node_idx];

			if (pin->sim_state->value < 0)
				fprintf(file, "x");
			else
				fprintf(file, "%d", pin->sim_state->value);
			if (type == INPUT && NULL != modelsim_out)
			{
				fprintf(modelsim_out, "force %s %d %d\n", lines[i]->name,pin->sim_state->value, cycle * 100 + 95);
			}
		}
		else //multi-bit
		{
			int j, value, unknown;

			value = 0;
			unknown = FALSE;

			if (type == INPUT)
			{
				//check to see if we have any unknown values
				for (j = 0; j < lines[i]->number_of_pins; j++)
					if (lines[i]->pins[j]->sim_state->value < 0)
						unknown = TRUE;
			}
			else
			{
				for (j = 0; j < lines[i]->number_of_pins; j++)
				{
					if (lines[i]->pins[j]->node->input_pins[lines[i]->pins[j]->pin_node_idx]->sim_state->value < 0)
						unknown = TRUE;
				}
			}

			if (unknown)
			{
				/*
				 * Since we have at least one 'unknown' bit, we can't print this out in hex.
				 */
				for (j = lines[i]->number_of_pins - 1; j >= 0 ; j--)
				{
					npin_t *pin;

					//see above comment
					if (type == INPUT)
						pin = lines[i]->pins[j];
					else //type == OUTPUT
						pin = lines[i]->pins[j]->node->input_pins[lines[i]->pins[j]->pin_node_idx];

					if (pin->sim_state->value < 0)
						fprintf(file, "x");
					else
						fprintf(file, "%d", pin->sim_state->value);
					if (type == INPUT)
						warning_message(SIMULATION_ERROR, -1, -1, "Tried to write an unknown value to the modelsim script. It's likely unreliable. \n");
				}
				continue;
			}

			//we're in hex
			fprintf(file, "0x");

			if (type == INPUT && NULL != modelsim_out)
			{
				fprintf(modelsim_out, "force %s 16#", lines[i]->name);
			}

			//start at MSB-side of hex string
			for (j = lines[i]->number_of_pins - 1; j >= 0; j--)
			{
				npin_t *pin;

				//we make this distinction because sometimes extra padding, say,
				//with mutlipliers, will overwrite values on OUTPUT nodes. This gets
				//the true value from the net that drives this pin
				if (type == INPUT)
					pin = lines[i]->pins[j];
				else //type == OUTPUT
					pin = lines[i]->pins[j]->node->input_pins[lines[i]->pins[j]->pin_node_idx];

				//if we're a one, shift over by our place in the int
				if (pin->sim_state->value > 0)
					value += my_power(2, j % 4);

				//at each fourth bit (counting from the LSB-side of the array!!!
				//this is the opposite of our list traversal!!!), reset our value,
				//since 4 bits = hex character
				if (j % 4 == 0)
				{
					fprintf(file, "%X", value);
					if (type == INPUT && NULL != modelsim_out)
					{
						fprintf(modelsim_out, "%X", value);
					}
					value = 0;
				}
			}
			if (type == INPUT && NULL != modelsim_out)
			{
				fprintf(modelsim_out, " %d\n", cycle * 100 + 95);
			}
		}
	}

	fprintf(file, "\n");
}

/*
 * Checks that each member line of lines[] such that line->type == OUTPUT
 * has corresponding values stored in input_pins[0] and output_pins[0].
 *
 * This blows up a message to the user and sets a global flag if
 * things go wrong
 */
int verify_output_vectors(netlist_t *netlist, line_t **lines, int lines_size, int cycle)
{
	int i, j;
	int problems = FALSE;

	for (i = 0; i < lines_size; i++)
	{
		if (lines[i]->type == INPUT)
			continue;

		//typically, we'll only have one pin, but I want to check
		for (j = 0; j < lines[i]->number_of_pins; j++)
		{
			npin_t *input_pin, *output_pin;

			output_pin = lines[i]->pins[j];
			input_pin = output_pin->node->input_pins[output_pin->pin_node_idx];

			//check if we have the same value
			if (input_pin->sim_state->value != output_pin->sim_state->value)
			{
				fprintf(stderr, "Simulation Value mismatch at node %s. Expected %d but encountered %d on cycle %d.\n",
						output_pin->node->name,
						output_pin->sim_state->value,
						input_pin->sim_state->value,
						cycle);
				problems = TRUE;
			}

			//check if we're all at the same cycle
			if (input_pin->sim_state->cycle != output_pin->sim_state->cycle ||
					input_pin->sim_state->cycle != cycle)
			{
				fprintf(stderr, "Simulation cycle mismatch at node %s. Expected cycle %d but encountered cycle %d on actual cycle %d.\n",
						output_pin->node->name,
						output_pin->sim_state->cycle,
						input_pin->sim_state->cycle,
						cycle);
				problems = TRUE;
			}
		}
	}

	//update gobal flag
	if (problems)
		sim_result = FALSE;

	return !problems;
}

/*
 * These set the constant pins' (gnd, vcc, and pad) values,
 * as well as updates their cycle.
 */
void set_constant_pin_values(netlist_t *netlist, int cycle)
{
	int i;
	for (i = 0; i < netlist->num_clocks; i++)
	{
		update_pin_value(netlist->clocks[i]->output_pins[0], cycle%2, cycle);
	}
	for (i = 0; i < netlist->gnd_node->num_input_pins; i++)
	{
		update_pin_value(netlist->gnd_node->input_pins[i], 0, cycle);
	}
	for (i = 0; i < netlist->gnd_node->num_output_pins; i++)
	{
		update_pin_value(netlist->gnd_node->output_pins[i], 0, cycle);
	}

	for (i = 0; i < netlist->vcc_node->num_input_pins; i++)
	{
		update_pin_value(netlist->vcc_node->input_pins[i], 1, cycle);
	}
	for (i = 0; i < netlist->vcc_node->num_output_pins; i++)
	{
		update_pin_value(netlist->vcc_node->output_pins[i], 1, cycle);
	}

	for (i = 0; i < netlist->pad_node->num_input_pins; i++)
	{
		update_pin_value(netlist->pad_node->input_pins[i], 0, cycle);
	}
	for (i = 0; i < netlist->pad_node->num_output_pins; i++)
	{
		update_pin_value(netlist->pad_node->output_pins[i], 0, cycle);
	}
}

/*
 * updates the value of a pin and  its cycle
 */
void update_pin_value(npin_t *pin, int value, int cycle)
{
	int i;

	oassert(pin != NULL);

	if (pin->sim_state->cycle == cycle)
	{
		oassert(pin->sim_state->value == value);
	}


	//update the sim state of THIS pin
	pin->sim_state->prev_value = pin->sim_state->value;
	pin->sim_state->value = value;
	pin->sim_state->cycle = cycle;

	//if this pin drives a net, we need to update that net's fanout pins
	if (NULL == pin->net)
	{
		//we're updating an output pin of an output node, most likely
		return;
	}
	for (i = 0; i < pin->net->num_fanout_pins; i++)
	{
		npin_t *fanout_pin;

		//sometimes, fanout pins of nets are NULL
		if (NULL == pin->net->fanout_pins[i])
			continue;

		fanout_pin = pin->net->fanout_pins[i];

		fanout_pin->sim_state->prev_value = fanout_pin->sim_state->value;
		fanout_pin->sim_state->value = value;
		fanout_pin->sim_state->cycle = cycle;
	}
}

/*
 * Takes two arrays of integers (1's and 0's) and returns an array
 * of integers (1's and 0's) that represent their product. The
 * length of the returned array is twice that of the two parameters.
 *
 * This array will need to be freed later!
 */
int *multiply_arrays(int *a, int a_length, int *b, int b_length)
{
	int *result, i, j;
	int result_size;

	/*
	 * This is a confusing algorithm to understand; it is so because we could
	 * be multiplying arbitrarily-long bit lengths, so we can't rely on storing
	 * these values in a C data type. This is really just a shift-and-add algorithm
	 *
	 * Instead, we're going to take our two arrays, a and b. For each bit i in a,
	 * if a[i] is a one, then we're going to add b to our result so far, but shifted
	 * over i to the left.
	 *
	 * However, I don't want to deal with the addition of two binary numbers in the
	 * middle of this for loop, so our result might look something like 1212111 for
	 * values of a=1011 and b=1011. So, we need to fix that to be 10100111.
	 *
	 * We can loop through the result array from the LSB (0) to the MSB and, everytime
	 * we encounter a non-binary value, subtract two until it's either zero or one.
	 * Whenever we subtract two from it, add one to the value to our left (since two
	 * at position i is equal to 1 in the position i+1). This essentially puts of
	 * dealing with the carries until the very end; it adds all the carries up, then
	 * deals with them all at once.
	 */

	//initialize our result
	result_size = a_length + b_length;
	result = calloc(sizeof(int), result_size);

	//check for 1's in a
	for (i = 0; i < a_length; i++)
	{
		if (a[i] == 1)
		{
			//for each one we find, add b << i to result
			for (j = 0; j < b_length; j++)
			{
				result[i+j] += b[j];
			}
		}
	}

	//deal with all of our carries
	for (i = 0; i < result_size; i++)
	{
		//keep subtracting until we're binary at position i. We'll deal with position
		//i+1 later, until we deal with everything.
		while (result[i] > 1)
		{
			result[i] -= 2;
			result[i+1]++;
		}
	}

	return result;
}

void compute_memory(npin_t **inputs, 
		npin_t **outputs, 
		int data_width, 
		npin_t **addr, 
		int addr_width, 
		int we, 
		int clock,
		int cycle,
		int *data)
{
	int address;
	int i;

	//if (clock == 0)
	{
		int i; 
		for (i = 0; i < data_width; i++)
		{
			update_pin_value(outputs[i], outputs[i]->sim_state->value, cycle);
		}
		return;
	}
	
	address = 0;
	/*
	 * The idea here is that we have an array of pins representing an address.
	 * The pins are arranged from MSB to LSB. These pins represent a number
	 * which is our address. We're going to iterate over them to get it.
	 */
	for (i = 0; i < addr_width; i++)
	{
		address += 1 << (addr_width - 1 - i);
	}
	if (we)
	{
		for (i = 0; i < data_width; i++)
		{
			int write_address = i + (address * data_width);
			data[write_address] = inputs[i]->sim_state->value;
			update_pin_value(outputs[i], data[write_address], cycle);
		}
	}
	else
	{
		for (i = 0; i < data_width; i++)
		{
			int read_address = i + (address * data_width);
			update_pin_value(outputs[i], data[read_address], cycle);
		}
	}
}

/*
 * This instantiates a node's particular memory array to zeros.
 * We include a reference to the node as well in order to future-
 * proof this in case this method uses memory initialization 
 * files in the future.
 */
void instantiate_memory(nnode_t *node, int **memory, int data_width, int addr_width)
{
	long long int max_address;	
	FILE *mif = NULL;
	char *filename = node->name;
	char *input = malloc(sizeof(char)*BUFFER_MAX_SIZE);
	
	oassert (*memory == NULL);

	max_address = my_power(2, addr_width);

	*memory = malloc (sizeof(int)*max_address*data_width);
	memset(*memory, 0, sizeof(int)*max_address*data_width);
	filename = strrchr(filename, '+') + 1;
	strcat(filename, ".mif");
	if (filename == NULL)
	{
		error_message(SIMULATION_ERROR, -1, -1, "Couldn't parse node name");
	}
	if (!(mif = fopen(filename, "r")))
	{
		char *msg = malloc(sizeof(char)*100);
		strcat(msg, "Couldn't open MIF file ");
		strcat(msg, filename);
		warning_message(SIMULATION_ERROR, -1, -1, msg);
		free(msg);
		return;
	}

	//Advance to content begin
	while (fgets(input, BUFFER_MAX_SIZE, mif))
		if (strcmp(input, "Content\n") == 0) break;
	while (fgets(input, BUFFER_MAX_SIZE, mif))
	{
		int i;
		long long int addr_val, data_val;
		char *colon;
		char *semicolon;
		char *addr = malloc(sizeof(char)*BUFFER_MAX_SIZE);
		char *data = malloc(sizeof(char)*BUFFER_MAX_SIZE);
		if (strcmp(input, "Begin\n") == 0) continue;
		if (strcmp(input, "End;") == 0 ||
			strcmp(input, "End;\n") == 0) continue;
		colon = strchr(input, ':');
		//copy into address string
		strncpy(addr, input, (colon-input));
		colon++; colon++;
		semicolon = strchr(input, ';');
		strncpy(data, colon, (semicolon-colon));
		addr_val = strtol(addr, NULL, 10);
		data_val = strtol(data, NULL, 16);
		//printf("%d: %lld\n", addr_val, data_val);
		for (i = 0; i < data_width; i++)
		{
			//extract binary value for position i out of data_val
			int mask = (1 << ((data_width - 1) - i));
			int val = (mask & data_val) > 0 ? 1 : 0;
			int write_address = i + (addr_val * data_width);
			data[write_address] = val;
			//printf("Addr: %d; i: %d; val: %d; (mask: %d)\n", addr_val, i, val, mask);
		}
	}
	
	fclose(mif);
}

void free_blocks()
{
	while (!is_empty(blocks))
	{
		void *handle;

		handle= dequeue_item(blocks);

		dlclose(handle);
	}
}
