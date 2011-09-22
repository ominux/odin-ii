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
#include "simulate_blif.h"

/* 
 * Simulates the netlist with the test_vector_file_name as input.
 * This simulates the input values in the test vector file, and
 * ensures the simulated values match the output values in the file.
 */
void simulate_blif (char *test_vector_file_name, netlist_t *netlist)
{
	printf("Beginning simulation.\n"); fflush(stdout);
	simulate_netlist(0, test_vector_file_name, netlist);
}

/*
 * Simulates the netlist with newly generated, random test vectors.
 * This will simulate num_test_vectors cycles of the circuit, storing
 * the input and output values in INPUT_TEST_VECTORS_FILE_NAME and
 * OUTPUT_TEST_VECTORS_FILE_NAME, respectively.
 */
void simulate_new_vectors (int num_test_vectors, netlist_t *netlist)
{
	printf("Beginning simulation.\n"); fflush(stdout);
	simulate_netlist(num_test_vectors, NULL, netlist);
}

/*
 * Performs simulation. 
 */ 
void simulate_netlist(int num_test_vectors, char *test_vector_file_name, netlist_t *netlist)
{
	int input_lines_size;
	line_t **input_lines;
	int output_lines_size;
	line_t **output_lines = NULL;

	FILE *in;
	FILE *out = NULL;
	FILE *modelsim_out = NULL;

	if (test_vector_file_name)
	{
		in = fopen(test_vector_file_name, "r");
		if (!in) error_message(SIMULATION_ERROR, -1, -1, "Could not open vector input file: %s", test_vector_file_name);

		input_lines = read_test_vector_headers(in, &input_lines_size, netlist->num_top_input_nodes + netlist->num_top_output_nodes);
		if (!input_lines) error_message(SIMULATION_ERROR, -1, -1, "Invalid vector file format: %s", test_vector_file_name);

		int i;
		for (i = 0; i < netlist->num_top_input_nodes; i++)
			assign_node_to_line(netlist->top_input_nodes[i], input_lines, input_lines_size, INPUT);

		for (i = 0; i < netlist->num_top_output_nodes; i++)
			assign_node_to_line(netlist->top_output_nodes[i], input_lines, input_lines_size, OUTPUT);

		int lines_ok = verify_lines(input_lines, input_lines_size);
		if (!lines_ok) error_message(SIMULATION_ERROR, -1, -1, "Lines could not be assigned.");

		// Count the test vectors in the file.
		num_test_vectors = 0;
		char buffer[BUFFER_MAX_SIZE];
		while (fgets(buffer, BUFFER_MAX_SIZE, in))
			num_test_vectors++;

		rewind(in);
		// Skip the vector headers.
		if (!fgets(buffer, BUFFER_MAX_SIZE, in)) error_message(SIMULATION_ERROR, -1, -1, "Failed to skip headers.");; 
	}
	else
	{
		modelsim_out = fopen("test.do", "w");

		if (!modelsim_out) error_message(SIMULATION_ERROR, -1, -1, "Could not open modelsim output file.");
		fprintf(modelsim_out, "force clock 1 0, 0 50 -repeat 100\n");

		out = fopen(OUTPUT_VECTOR_FILE_NAME, "w");
		if (!out) error_message(SIMULATION_ERROR, -1, -1, "Could not open output vector file.");

		in  = fopen( INPUT_VECTOR_FILE_NAME, "w");
		if (!in)  error_message(SIMULATION_ERROR, -1, -1, "Could not open input vector file.");

		input_lines  = create_input_test_vector_lines(&input_lines_size, netlist);
		output_lines = create_output_test_vector_lines(&output_lines_size, netlist);

		int lines_ok = verify_lines(input_lines,  input_lines_size) && verify_lines(output_lines, output_lines_size);
		if (!lines_ok) error_message(SIMULATION_ERROR, -1, -1, "Lines could not be assigned.");
	}
	
	if (num_test_vectors)
	{
		nnode_t **ordered_nodes = 0;
		int   num_ordered_nodes = 0;
		int   num_waves         = ceil(num_test_vectors / (double)SIM_WAVE_LENGTH);		
		int wave;
		for (wave = 0; wave < num_waves; wave++)
		{
			int cycle_offset = SIM_WAVE_LENGTH * wave;
			int wave_length  = (wave < (num_waves-1))?SIM_WAVE_LENGTH:(num_test_vectors - cycle_offset);

			if (test_vector_file_name)
			{
				int cycle = cycle_offset;
				char buffer[BUFFER_MAX_SIZE];
				while (fgets(buffer, BUFFER_MAX_SIZE, in))
					assign_input_vector_to_lines(input_lines, buffer, cycle++);
			}
			else
			{
				int i;
				for (i = 0; i < wave_length; i++)
					assign_random_vector_to_input_lines(input_lines, input_lines_size, cycle_offset+i);

				write_all_vectors_to_file(input_lines, input_lines_size, in, modelsim_out, INPUT, cycle_offset, wave_length);
			}

			printf("%6d/%d",wave+1,num_waves);
			
			int cycle;
			for (cycle = cycle_offset; cycle < cycle_offset + wave_length; cycle++)
			{
				simulate_cycle(netlist, cycle, &ordered_nodes, &num_ordered_nodes);
				printf("."); fflush(stdout);
			}

			if (test_vector_file_name)
			{
				int error = FALSE; 
				for (cycle = cycle_offset; cycle < cycle_offset + wave_length; cycle++)
				{
					if (!verify_output_vectors(netlist, input_lines, input_lines_size, cycle)) 
						error = TRUE; 
				}
				if (!error) printf("Error\n"); 
				else        printf("OK   \n");
			}
			else
			{
				int wave_cols = wave_length+10; 
				int display_cols = 80; 
				if (!((wave+1) % (display_cols/wave_cols))) 
					printf("\n");

				write_all_vectors_to_file(output_lines, output_lines_size, out, modelsim_out, OUTPUT, cycle_offset, wave_length);
			}
		}
		free(ordered_nodes);
		ordered_nodes = 0;
		num_ordered_nodes = 0;
	}

	if (!test_vector_file_name)
	{
		fclose(out);
		fclose(modelsim_out);		
		free_lines(output_lines, output_lines_size);		
	}
	free_lines(input_lines, input_lines_size);
	fclose(in);
}

/*
 * This simulates a single cycle. 
 */
void simulate_cycle(netlist_t *netlist, int cycle, nnode_t ***ordered_nodes, int *num_ordered_nodes)
{
	if (cycle)
	{
		int i; 				
		for(i = 0; i < *num_ordered_nodes; i++)
			compute_and_store_value((*ordered_nodes)[i], cycle);

	}
	else
	{
		queue_t *queue = create_queue();
		int i;
		for (i = 0; i < netlist->num_top_input_nodes; i++)
			enqueue_node_if_ready(queue,netlist->top_input_nodes[i],cycle);

		nnode_t *constant_nodes[] = {netlist->gnd_node, netlist->vcc_node, netlist->pad_node};
		int num_constant_nodes = 3;
		for (i = 0; i < num_constant_nodes; i++)
			enqueue_node_if_ready(queue,constant_nodes[i],cycle);
		
		nnode_t *node;
		while ((node = queue->remove(queue)))
		{
			compute_and_store_value(node, cycle);

			int num_children; 
			nnode_t **children = get_children_of(node, &num_children); 
			for (i = 0; i < num_children; i++)
			{
				nnode_t* node = children[i];
				if (
					   !node->in_queue
					&& is_node_ready(node, cycle)
					&& !is_node_complete(node, cycle)
				)
				{
					node->in_queue = TRUE;
					queue->add(queue,node);
				}
			}
			free(children); 

			node->in_queue = FALSE;

			// Add the node to the ordered nodes array.
			*ordered_nodes = realloc(*ordered_nodes, sizeof(nnode_t *) * ((*num_ordered_nodes) + 1));
			(*ordered_nodes)[(*num_ordered_nodes)++] = node;
		}
		queue->destroy(queue);
	}	
}

/*
 * Enqueues the node in the given queue if is_node_ready returns TRUE.
 */
int enqueue_node_if_ready(queue_t* queue, nnode_t* node, int cycle)
{
	if (is_node_ready(node, cycle))
	{
		node->in_queue = TRUE;
		queue->add(queue, node);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/*
 * Determines if the given node has been simulated for the given cycle.
 */
inline int is_node_complete(nnode_t* node, int cycle)
{
	int i;
	for (i = 0; i < node->num_output_pins; i++)
	{
		if (node->output_pins[i] && node->output_pins[i]->cycle < cycle)
			return FALSE;			

	}
	return TRUE;
}

/*
 * Checks to see if the node is ready to be simulated for the given cycle.
 */
inline int is_node_ready(nnode_t* node, int cycle)
{
	if (node->type == FF_NODE) cycle--;

	int i;
	for (i = 0; i < node->num_input_pins; i++)
	{
		if (node->input_pins[i]->cycle < cycle)
			return FALSE;
	}
	return TRUE;
}

/*
 * Gets the children of the given node. Returns the number of children via the num_children parameter. 
 */ 
nnode_t **get_children_of(nnode_t *node, int *num_children)
{
	queue_t *queue = create_queue(); 	
	int i; 
	for (i = 0; i < node->num_output_pins; i++)
	{
		nnet_t *net = node->output_pins[i]->net;
		if (net)
		{
			int j; 
			for (j = 0; j < net->num_fanout_pins; j++)
			{
				npin_t *fanout_pin = net->fanout_pins[j];
				if (fanout_pin && fanout_pin->type == INPUT && fanout_pin->node)
					queue->add(queue,fanout_pin->node);
			}
		}
	}

	*num_children = queue->count; 
	nnode_t **children = (nnode_t **)queue->remove_all(queue); 
	queue->destroy(queue); 
	return children; 
}

/*
 * Sets the pin to the given value for the given cycle. Does not
 * propogate the value to the connected net. 
 * 
 * CAUTION: Use update_pin_value to update pins. This function will not update
 *          the connected net. 
 */
inline void set_pin(npin_t *pin, int value, int cycle)
{
	pin->values[get_values_offset(cycle)] = value;
	pin->cycle = cycle;
}

/*
 * Updates the value of a pin and its cycle. Pins should be updated using 
 * only this function. 
 */
inline void update_pin_value(npin_t *pin, int value, int cycle)
{
	set_pin(pin,value,cycle); 

	if (pin->net)
	{		
		int i;
		for (i = 0; i < pin->net->num_fanout_pins; i++)
		{
			npin_t *fanout_pin = pin->net->fanout_pins[i];
			if (fanout_pin)
				set_pin(fanout_pin,value,cycle);
		}
	}
}

/*
 * Gets the value of a pin. Pins should be checked using this function only. 
 */ 
inline signed char get_pin_value(npin_t *pin, int cycle)
{
	if (cycle < 0) return -1;

	return pin->values[get_values_offset(cycle)];
}

/*
 * Calculates the index in the values array for the given cycle. 
 */ 
inline int get_values_offset(int cycle)
{
	if (cycle < 0) return SIM_WAVE_LENGTH-1;

	return (((cycle) + (SIM_WAVE_LENGTH+1)) % (SIM_WAVE_LENGTH+1));
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
	int i;
	int unknown = FALSE;

	/*
	 * ODIN doesn't list clocks as type CLOCK_NODE, and we don't pick them
	 * up when we're assigning top-level inputs to lines[], so we
	 * need to check them here.
	 */
	if (!strcmp(node->name, "top^"CLOCK_PORT_NAME_1) || !strcmp(node->name, "top^"CLOCK_PORT_NAME_2))
	{
		for (i = 0; i < node->num_output_pins; i++)
			update_pin_value(node->output_pins[i], cycle % 2, cycle);

		return;
	}
	
	if (!strcmp(node->name, RESET_PORT_NAME))
	{
		for (i = 0; i < node->num_output_pins; i++)
			update_pin_value(node->output_pins[i], 0, cycle);
	}	

	/*
	 * The behaviour defined in these case statements reflect
	 * the logic in the output_blif.c file.
	 */
	switch(node->type)
	{
		case LT:
		{				// < 010 1
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			if (
				   get_pin_value(node->input_pins[0],cycle) < 0 
				|| get_pin_value(node->input_pins[1],cycle) < 0 
				|| get_pin_value(node->input_pins[2],cycle) < 0
			)
			{
				update_pin_value(node->output_pins[0], -1, cycle);
			}
			else if (
				   get_pin_value(node->input_pins[0],cycle) == 0 
				&& get_pin_value(node->input_pins[1],cycle) == 1 
				&& get_pin_value(node->input_pins[2],cycle) == 0
			)
			{
				update_pin_value(node->output_pins[0], 1, cycle);			
			}
			else
			{
				update_pin_value(node->output_pins[0], 0, cycle);
			}
			return;
		}
		case GT:
		{				// > 100 1
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			if (
				   get_pin_value(node->input_pins[0],cycle) < 0 
				|| get_pin_value(node->input_pins[1],cycle) < 0 
				|| get_pin_value(node->input_pins[2],cycle) < 0
			)
			{
				update_pin_value(node->output_pins[0], -1, cycle);				
			}
			else if (
				   get_pin_value(node->input_pins[0],cycle) == 1 
				&& get_pin_value(node->input_pins[1],cycle) == 0 
				&& get_pin_value(node->input_pins[2],cycle) == 0
			)
			{
				update_pin_value(node->output_pins[0], 1, cycle);
			}
			else
			{
				update_pin_value(node->output_pins[0], 0, cycle);
			}
			return;
		}
		case ADDER_FUNC:
		{		// 001 1\n010 1\n100 1\n111 1
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			if (
				   get_pin_value(node->input_pins[0],cycle) < 0 
				|| get_pin_value(node->input_pins[1],cycle) < 0 
				|| get_pin_value(node->input_pins[2],cycle) < 0
			)
			{
				update_pin_value(node->output_pins[0], -1, cycle);
			}
			else if (
				(
					   get_pin_value(node->input_pins[0],cycle) == 0 
					&& get_pin_value(node->input_pins[1],cycle) == 0
					&& get_pin_value(node->input_pins[2],cycle) == 1
				)
				|| (
					   get_pin_value(node->input_pins[0],cycle) == 0 
					&& get_pin_value(node->input_pins[1],cycle) == 1 
					&& get_pin_value(node->input_pins[2],cycle) == 0
				) 
				|| (
					   get_pin_value(node->input_pins[0],cycle) == 1 
					&& get_pin_value(node->input_pins[1],cycle) == 0 
					&& get_pin_value(node->input_pins[2],cycle) == 0
				)
				|| (
					   get_pin_value(node->input_pins[0],cycle) == 1 
					&& get_pin_value(node->input_pins[1],cycle) == 1 
					&& get_pin_value(node->input_pins[2],cycle) == 1
				)
			)
			{
				update_pin_value(node->output_pins[0], 1, cycle);			
			}
			else
			{
				update_pin_value(node->output_pins[0], 0, cycle);
			}
			return;
		}
		case CARRY_FUNC:
		{		// 011 1\n100 1\n110 1\n111 1
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);
			
			if (
				(
					   get_pin_value(node->input_pins[0],cycle) == 1 
					&& get_pin_value(node->input_pins[1],cycle) == 0 
					&& get_pin_value(node->input_pins[2],cycle) == 0
				) 
				|| (
					   get_pin_value(node->input_pins[0],cycle) == 1 
					&& get_pin_value(node->input_pins[1],cycle) == 1 
					&& get_pin_value(node->input_pins[2],cycle) == 0
				) 
				|| (
					   get_pin_value(node->input_pins[1],cycle) == 1 
					&& get_pin_value(node->input_pins[2],cycle) == 1
				)
			)
			{
				update_pin_value(node->output_pins[0], 1, cycle);				
			}
			else if (
				   get_pin_value(node->input_pins[0],cycle) < 0 
				|| get_pin_value(node->input_pins[1],cycle) < 0 
				|| get_pin_value(node->input_pins[2],cycle) < 0
			)
			{
				update_pin_value(node->output_pins[0], -1, cycle);				
			}
			else
			{
				update_pin_value(node->output_pins[0], 0, cycle);
			}
			return;
		}
		case BITWISE_NOT:
		{
			oassert(node->num_input_pins == 1);
			oassert(node->num_output_pins == 1);

			if (get_pin_value(node->input_pins[0],cycle) < 0)
				update_pin_value(node->output_pins[0], -1, cycle);
			else if (get_pin_value(node->input_pins[0],cycle) == 1)
				update_pin_value(node->output_pins[0], 0, cycle);
			else
				update_pin_value(node->output_pins[0], 1, cycle);

			return;
		}
		case LOGICAL_AND:
		{		// &&
			oassert(node->num_output_pins == 1);

			for (i = 0; i < node->num_input_pins; i++)
			{
				if (get_pin_value(node->input_pins[i],cycle) < 0)
				{
					update_pin_value(node->output_pins[0], -1, cycle);
					return;
				}

				if (get_pin_value(node->input_pins[i],cycle) == 0)
				{
					update_pin_value(node->output_pins[0], 0, cycle);
					return;
				}
			}
			update_pin_value(node->output_pins[0], 1, cycle);
			return;
		}
		case LOGICAL_OR:
		{		// ||
			oassert(node->num_output_pins == 1);

			for (i = 0; i < node->num_input_pins; i++)
			{
				if (get_pin_value(node->input_pins[i],cycle) < 0)
				{
					unknown = TRUE;
				}

				if (get_pin_value(node->input_pins[i],cycle) == 1)
				{
					update_pin_value(node->output_pins[0], 1, cycle);
					return;
				}
			}

			if (unknown) update_pin_value(node->output_pins[0], -1, cycle);
			else         update_pin_value(node->output_pins[0], 0, cycle);
			return;
		}
		case LOGICAL_NAND:
		{		// !&&
			oassert(node->num_output_pins == 1);

			int retVal = 0;
			for (i = 0; i < node->num_input_pins; i++)
			{
				if (get_pin_value(node->input_pins[i],cycle) < 0)
				{
					update_pin_value(node->output_pins[0], -1, cycle);
					return;
				}

				if (get_pin_value(node->input_pins[i],cycle) == 0)
				{
					retVal = 1;
				}
			}

			update_pin_value(node->output_pins[0], retVal, cycle);
			return;
		}
		case LOGICAL_NOT:		// !
		case LOGICAL_NOR:
		{		// !|
			oassert(node->num_output_pins == 1);

			for (i = 0; i < node->num_input_pins; i++)
			{
				if (get_pin_value(node->input_pins[i],cycle) < 0)
				{
					unknown = TRUE;
				}

				if (get_pin_value(node->input_pins[i],cycle) == 1)
				{
					update_pin_value(node->output_pins[0], 0, cycle);
					return;
				}
			}

			if (unknown) update_pin_value(node->output_pins[0], -1, cycle);
			else         update_pin_value(node->output_pins[0], 1, cycle);
			return;
		}
		case LOGICAL_EQUAL:		// ==
		case LOGICAL_XOR:
		{		// ^
			oassert(node->num_output_pins == 1);

			long long ones = 0;
			for (i = 0; i < node->num_input_pins; i++)
			{
				if (get_pin_value(node->input_pins[i],cycle)  < 0) unknown = TRUE;
				if (get_pin_value(node->input_pins[i],cycle) == 1) ones++;
			}

			if (unknown)
			{
				update_pin_value(node->output_pins[0], -1, cycle);
			}
			else
			{
				if (ones % 2 == 1) update_pin_value(node->output_pins[0], 1, cycle);
				else               update_pin_value(node->output_pins[0], 0, cycle);
			}
			return;
		}
		case NOT_EQUAL:			// !=
		case LOGICAL_XNOR:
		{		// !^
			oassert(node->num_output_pins == 1);
			
			long long ones = 0;
			for (i = 0; i < node->num_input_pins; i++)
			{
				if (get_pin_value(node->input_pins[i],cycle)  < 0) unknown = TRUE;
				if (get_pin_value(node->input_pins[i],cycle) == 1) ones++;
			}

			if (unknown)
			{
				update_pin_value(node->output_pins[0], -1, cycle);
			}
			else
			{
				if (ones % 2 == 0) update_pin_value(node->output_pins[0], 1, cycle);
				else               update_pin_value(node->output_pins[0], 0, cycle);
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
				if (get_pin_value(node->input_pins[i],cycle) < 0)
				{
					unknown = TRUE;
				}

				if (
					   get_pin_value(node->input_pins[i],cycle) == 1 
					&& get_pin_value(node->input_pins[i],cycle) == get_pin_value(node->input_pins[i+node->input_port_sizes[0]],cycle)
				) {
					update_pin_value(node->output_pins[0], 1, cycle);
					return;
				}
			}

			if (unknown) update_pin_value(node->output_pins[0], -1, cycle);
			else         update_pin_value(node->output_pins[0], 0, cycle);

			return;
		}
		case FF_NODE:
		{
			oassert(node->num_output_pins == 1);
			oassert(node->num_input_pins == 2);

			update_pin_value(node->output_pins[0], get_pin_value(node->input_pins[0],cycle-1), cycle);			
			return;
		}
		case MEMORY:
		{
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

			oassert(strcmp(node->related_ast_node->children[0]->types.identifier, SINGLE_PORT_MEMORY_NAME) == 0
				|| strcmp(node->related_ast_node->children[0]->types.identifier, DUAL_PORT_MEMORY_NAME) == 0);
			
			if (strcmp(node->related_ast_node->children[0]->types.identifier, SINGLE_PORT_MEMORY_NAME) == 0)
			{
				int we = 0;
				int clock = 0;
				int data_width = 0;
				int addr_width = 0;
				npin_t **addr = NULL;
				npin_t **data = NULL;
				npin_t **out = NULL;

				for (i = 0; i < node->num_input_pins; i++)
				{
					if (strcmp(node->input_pins[i]->mapping, we_name) == 0)
					{
						we = get_pin_value(node->input_pins[i],cycle);
					}
					else if (strcmp(node->input_pins[i]->mapping, addr_name) == 0)
					{
						if (!addr) addr = &node->input_pins[i];
						addr_width++;
					}
					else if (strcmp(node->input_pins[i]->mapping, data_name) == 0)
					{
						if (!data) data = &node->input_pins[i];
						data_width++;
					}
					else if (strcmp(node->input_pins[i]->mapping, clock_name) == 0)
					{
						clock = get_pin_value(node->input_pins[i],cycle);
					}
				}
				out = node->output_pins;

				if (node->type == MEMORY && !node->memory_data)
				{
					instantiate_memory(node, &(node->memory_data), data_width, addr_width);
				}

				compute_memory(data, out, data_width, addr, addr_width, we, clock, cycle, node->memory_data);
			}
			else
			{
				int clock = 0;

				int we1 = 0;
				int data_width1 = 0;
				int addr_width1 = 0;
				int we2 = 0;
				int data_width2 = 0;
				int addr_width2 = 0;
				
				npin_t **addr1 = NULL;
				npin_t **data1 = NULL;
				npin_t **out1 = NULL;
				npin_t **addr2 = NULL;
				npin_t **data2 = NULL;
				npin_t **out2 = NULL;


				for (i = 0; i < node->num_input_pins; i++)
				{
					if (strcmp(node->input_pins[i]->mapping, we_name1) == 0)
					{
						we1 = get_pin_value(node->input_pins[i],cycle);
					}
					else if (strcmp(node->input_pins[i]->mapping, we_name2) == 0)
					{
						we2 = get_pin_value(node->input_pins[i],cycle);
					}
					else if (strcmp(node->input_pins[i]->mapping, addr_name1) == 0)
					{
						if (!addr1) addr1 = &node->input_pins[i];
						addr_width1++;
					}
					else if (strcmp(node->input_pins[i]->mapping, addr_name2) == 0)
					{
						if (!addr2) addr2 = &node->input_pins[i];
						addr_width2++;
					}
					else if (strcmp(node->input_pins[i]->mapping, data_name1) == 0)
					{
						if (!data1) data1 = &node->input_pins[i];
						data_width1++;
					}
					else if (strcmp(node->input_pins[i]->mapping, data_name2) == 0)
					{
						if (!data2) data2 = &node->input_pins[i];
						data_width2++;
					}
					else if (strcmp(node->input_pins[i]->mapping, clock_name) == 0)
					{
						clock = get_pin_value(node->input_pins[i],cycle);
					}
				}

				for (i = 0; i < node->num_output_pins; i++)
				{
					if (strcmp(node->output_pins[i]->mapping, out_name1) == 0)
					{
						if (!out1) out1 = &node->output_pins[i];
					}
					else if (strcmp(node->output_pins[i]->mapping, out_name2) == 0)
					{
						if (!out2) out2 = &node->output_pins[i];
					}
				}			 

				if (!node->memory_data)
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
			oassert(node->input_port_sizes[0] > 0);
			oassert(node->output_port_sizes[0] > 0);
			
			int *input_pins = malloc(sizeof(int)*node->num_input_pins);
			int *output_pins = malloc(sizeof(int)*node->num_output_pins);
			
			if (!node->simulate_block_cycle)
			{
				char *filename = malloc(sizeof(char)*strlen(node->name));
				
				if (!index(node->name, '.')) error_message(SIMULATION_ERROR, -1, -1, "Couldn't extract the name of a shared library for hard-block simulation");
								
				snprintf(filename, sizeof(char)*strlen(node->name), "%s.so", index(node->name, '.')+1);
				
				void *handle = dlopen(filename, RTLD_LAZY);
				
				if (!handle) error_message(SIMULATION_ERROR, -1, -1, "Couldn't open a shared library for hard-block simulation: %s", dlerror());
				
				dlerror();

				void (*func_pointer)(int, int, int*, int, int*) = (void(*)(int, int, int*, int, int*))dlsym(handle, "simulate_block_cycle");

				char *error = dlerror();
				if (error) error_message(SIMULATION_ERROR, -1, -1, "Couldn't load a shared library method for hard-block simulation: %s", error);				

				node->simulate_block_cycle = func_pointer;

				free(filename);
			}
			
			for (i = 0; i < node->num_input_pins; i++)
			{
				input_pins[i] = get_pin_value(node->input_pins[i],cycle);
			}
			
			(node->simulate_block_cycle)(cycle, node->num_input_pins, input_pins, node->num_output_pins, output_pins);
						
			for (i = 0; i < node->num_output_pins; i++)
			{
				update_pin_value(node->output_pins[i], output_pins[i], cycle);
			}
			
			free(input_pins);
			free(output_pins);
			return;
		}
		case MULTIPLY:
		{
			oassert(node->num_input_port_sizes >= 2);
			oassert(node->num_output_port_sizes == 1);
				
			int *a = malloc(sizeof(int)*node->input_port_sizes[0]);
			int *b = malloc(sizeof(int)*node->input_port_sizes[1]);

			for (i = 0; i < node->input_port_sizes[0]; i++)
			{
				a[i] = get_pin_value(node->input_pins[i],cycle);	
				if (a[i] < 0) unknown = TRUE;
			}

			for (i = 0; i < node->input_port_sizes[1]; i++)
			{
				b[i] = get_pin_value(node->input_pins[node->input_port_sizes[0] + i],cycle);
				if (b[i] < 0) unknown = TRUE;
			}

			if (unknown)
			{
				for (i = 0; i < node->num_output_pins; i++)
				{
					update_pin_value(node->output_pins[i], -1, cycle);
				}
			}

			int *result = multiply_arrays(a, node->input_port_sizes[0], b, node->input_port_sizes[1]);
			
			for (i = 0; i < node->num_output_pins; i++)
			{
				update_pin_value(node->output_pins[i], result[i], cycle);
			}			
			free(result);
			free(a);
			free(b);
			return;
		}
		case GENERIC :
		{
			int line_count_bitmap = node->bit_map_line_count;
			char **bit_map = node->bit_map;

			int lut_size  = 0;
			while (bit_map[0][lut_size] != 0)
				lut_size++;

			int found = 0;
			for (i = 0; i < line_count_bitmap && (!found); i++)
			{
				int j;
				for (j = 0; j < lut_size; j++)
				{
					if (get_pin_value(node->input_pins[j],cycle) < 0)
					{
						update_pin_value(node->output_pins[0], -1, cycle);
						return;
					}

					if ((bit_map[i][j] != '-') && (bit_map[i][j]-'0' != get_pin_value(node->input_pins[j],cycle)))
					{
						break;
					}
				}

				if (j == lut_size) found = TRUE;
			}	
					
			if (found) update_pin_value(node->output_pins[0], 1, cycle);
			else       update_pin_value(node->output_pins[0], 0, cycle);
			return;
		}
		case INPUT_NODE:  return; 
		case OUTPUT_NODE: return;
		case PAD_NODE:
		{
			for (i = 0; i < node->num_output_pins; i++)
			{
				update_pin_value(node->output_pins[i], 0, cycle);
			}
			return;
		}
		case CLOCK_NODE:  return;
		case GND_NODE:
		{
			update_pin_value(node->output_pins[0], 0, cycle);
			return;
		}
		case VCC_NODE:
		{
			update_pin_value(node->output_pins[0], 1, cycle);
			return;
		}

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
		{
			error_message(SIMULATION_ERROR, -1, -1, "Node should have been converted to softer version: %s", node->name);
			return;
		}
	}
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
	oassert(token != NULL);
	oassert(strlen(token) != 0);

	if (line->number_of_pins < 1)
	{
		warning_message(SIMULATION_ERROR, -1, -1, "Found a line '%s' with no pins.", line->name);		
	}	
	else if (strlen(token) == 1)
	{ // Only 1 pin.
		if      (token[0] == '0') update_pin_value(line->pins[0],  0, cycle);
		else if (token[0] == '1') update_pin_value(line->pins[0],  1, cycle);
		else                      update_pin_value(line->pins[0], -1, cycle);		
	} 
	else if (token[0] == '0' && (token[1] == 'x' || token[1] == 'X'))
	{
		token += 2;
		int token_length = strlen(token);
		
		int i = 0;
		int j = token_length - 1;
		while(i < j)
		{
			char temp = token[i];
			token[i] = token [j];
			token[j] = temp;
			i++; 
			j--;
		}

		int k = 0; 
		for (i = 0; i < token_length; i++)
		{
			char temp[] = {token[i],'\0'};
			int value = strtol(temp, NULL, 16);

			for (k = 0; k < 4 && k + (i*4) < line->number_of_pins; k++)
			{
				int pin_value = (value & (1 << k)) > 0 ? 1 : 0;
				update_pin_value(line->pins[k + (i*4)], pin_value, cycle);
			}
		}

		for ( ; k + (i*4) < line->number_of_pins; k++)
			update_pin_value(line->pins[k + (i*4)], get_pin_value(line->pins[k + (i*4)],cycle), cycle);
	}
	else
	{
		int i, j; 
		for (i = strlen(token) - 1, j = 0; i >= 0 && j < line->number_of_pins; i--, j++)
		{
			if (token[i] == '0')      update_pin_value(line->pins[j],  0, cycle);
			else if (token[i] == '1') update_pin_value(line->pins[j],  1, cycle);
			else                      update_pin_value(line->pins[j], -1, cycle);
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
	char *port_name = malloc(sizeof(char)*strlen(node->name));
	strcpy(port_name, strchr(node->name, '^') + 1);

	char *tilde = strchr(port_name, '~');
	if (!tilde)
	{
		int found = FALSE;		
		int j;
		for (j = 0; j < lines_size; j++)
		{
			if (strcmp(lines[j]->name, port_name) == 0)
			{
				found = TRUE;
				break;
			}
		}

		if (!found)
		{
			if (	
				   !(strcmp(port_name, CLOCK_PORT_NAME_1) == 0 || strcmp(port_name, CLOCK_PORT_NAME_2) == 0) 				
				&& ! (node->type == GND_NODE || node->type == VCC_NODE || node->type == PAD_NODE || node->type == CLOCK_NODE)
			)
			{
				warning_message(SIMULATION_ERROR, -1, -1, "Could not map single-bit top-level input node '%s' to input vector", node->name);
			}			
		} 
		else
		{
			if (!node->num_output_pins)
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
		}		
	}
	else
	{
		int pin_number = atoi(tilde+1);	
		*tilde = '\0';	

		int found = FALSE;
		int j;
		for (j = 0; j < lines_size; j++)
		{
			if (!strcmp(lines[j]->name, port_name))
			{
				found = TRUE;
				break;
			}
		}

		if (!found)
		{
			warning_message(SIMULATION_ERROR, -1, -1, "Could not map multi-bit top-level input node '%s' to input vector", node->name);			
		} 
		else
		{
			if (lines[j]->max_number_of_pins < 0)
			{
				lines[j]->max_number_of_pins = 8;
				lines[j]->pins = malloc(sizeof(npin_t*)*lines[j]->max_number_of_pins);
			}

			if (lines[j]->max_number_of_pins <= pin_number)
			{
				while (lines[j]->max_number_of_pins <= pin_number) 
					lines[j]->max_number_of_pins += 64;

				lines[j]->pins = realloc(lines[j]->pins, sizeof(npin_t*)*lines[j]->max_number_of_pins);
			}

			if (!node->num_output_pins)
			{
				npin_t *pin = allocate_npin();
				allocate_more_node_output_pins(node, 1);
				add_a_output_pin_to_node_spot_idx(node, pin, 0);
			}

			lines[j]->pins[pin_number] = node->output_pins[0];
			lines[j]->type = type;
			lines[j]->number_of_pins++;
		}
	}
	free(port_name);
}

/*
 * Given a netlist, this function maps the top_input_nodes and
 * top_output_nodes to a line_t* each. It stores them in an array
 * and returns it, storing the array size in the *lines_size
 * pointer.
 */
line_t **create_input_test_vector_lines(int *lines_size, netlist_t *netlist)
{
	line_t **lines = malloc(sizeof(line_t*)*netlist->num_top_input_nodes);	
	int current_line = 0;

	int i; 	
	for (i = 0; i < netlist->num_top_input_nodes; i++)
	{
		char *port_name = malloc(sizeof(char)*(strlen(netlist->top_input_nodes[i]->name)+1));
		strcpy(port_name, strchr(netlist->top_input_nodes[i]->name, '^') + 1);
				
		char *tilde = strchr(port_name, '~');
		if (!tilde)
		{
			if (!(strcmp(port_name, CLOCK_PORT_NAME_1) == 0 || strcmp(port_name, CLOCK_PORT_NAME_2) == 0))
			{
				line_t * line = create_line(port_name); 
				line->number_of_pins = 1;
				line->max_number_of_pins = 1;
				line->pins = malloc(sizeof(npin_t *));
				line->pins[0] = netlist->top_input_nodes[i]->output_pins[0];
				line->type = INPUT;			
				lines[current_line++] = line;	
			}	
		}
		else
		{
			int pin_number = atoi(tilde+1);
			*tilde = '\0';

			if (!(strcmp(port_name, CLOCK_PORT_NAME_1) == 0 || strcmp(port_name, CLOCK_PORT_NAME_2) == 0 || strcmp(port_name, RESET_PORT_NAME) == 0))
			{
				int found = FALSE;
				int j; 
				for (j = 0; j < current_line && !found; j++)
				{
					if (strcmp(lines[j]->name, port_name) == 0)
					{
						found = TRUE;
						break; 
					}
				}

				if (!found) lines[current_line++] = create_line(port_name); 

				if (lines[j]->max_number_of_pins < 0)
				{
					lines[j]->max_number_of_pins = 8;
					lines[j]->pins = malloc(sizeof(npin_t*)*lines[j]->max_number_of_pins);
				}

				if (lines[j]->max_number_of_pins <= pin_number)
				{
					while (lines[j]->max_number_of_pins <= pin_number)
						lines[j]->max_number_of_pins += 64;

					lines[j]->pins = realloc(lines[j]->pins, sizeof(npin_t*)*lines[j]->max_number_of_pins);
				}

				lines[j]->pins[pin_number] = netlist->top_input_nodes[i]->output_pins[0];
				lines[j]->type = INPUT;
				lines[j]->number_of_pins++;
			}
		}
		free(port_name);
	}
	*lines_size = current_line;
	lines = realloc(lines, sizeof(line_t*)*(*lines_size));			
	return lines;
}

line_t **create_output_test_vector_lines(int *lines_size, netlist_t *netlist)
{
	line_t **lines = malloc(sizeof(line_t*)*netlist->num_top_output_nodes);	
	int current_line = 0; 
	
	int i; 
	for (i = 0; i < netlist->num_top_output_nodes; i++)
	{
		char *port_name = malloc(sizeof(char)*strlen(netlist->top_output_nodes[i]->name));
		strcpy(port_name, strchr(netlist->top_output_nodes[i]->name, '^') + 1);

		char *tilde = strchr(port_name, '~'); 
		if (!tilde)
		{
			if (netlist->top_output_nodes[i]->num_output_pins == 0)
			{
				npin_t *pin = allocate_npin();
				allocate_more_node_output_pins(netlist->top_output_nodes[i], 1);
				add_a_output_pin_to_node_spot_idx(netlist->top_output_nodes[i], pin, 0);
			}

			line_t * line = create_line(port_name); 						
			line->number_of_pins = 1;
			line->max_number_of_pins = 1;
			line->pins = malloc(sizeof(npin_t *));
			line->pins[0] = netlist->top_output_nodes[i]->output_pins[0];
			line->type = OUTPUT; 
			lines[current_line++] = line;
		}
		else
		{
			int pin_number = atoi(tilde+1);
			*tilde = '\0';
			int found = FALSE;

			int j; 
			for (j = 0; j < current_line && !found; j++)
			{
				if (strcmp(lines[j]->name, port_name) == 0)
				{
					found = TRUE;
					break; 
				}
			}

			if (!found) lines[current_line++] = create_line(port_name);

			if (lines[j]->max_number_of_pins < 0)
			{
				lines[j]->max_number_of_pins = 8;
				lines[j]->pins = malloc(sizeof(npin_t*)*lines[j]->max_number_of_pins);
			}

			if (lines[j]->max_number_of_pins <= pin_number)
			{
				while (lines[j]->max_number_of_pins <= pin_number)
					lines[j]->max_number_of_pins += 64;

				lines[j]->pins = realloc(lines[j]->pins, sizeof(npin_t*)*lines[j]->max_number_of_pins);
			}

			if (!netlist->top_output_nodes[i]->num_output_pins)
			{
				npin_t *pin = allocate_npin();
				allocate_more_node_output_pins(netlist->top_output_nodes[i], 1);
				add_a_output_pin_to_node_spot_idx(netlist->top_output_nodes[i], pin, 0);
			}

			lines[j]->pins[pin_number] = netlist->top_output_nodes[i]->output_pins[0];
			lines[j]->type = OUTPUT;
			lines[j]->number_of_pins++;
		}
		free(port_name);
	}
	*lines_size = current_line;
	lines = realloc(lines, sizeof(line_t*)*(*lines_size));	
	return lines;
}

/*
 * Writes the lines[] elements' names to the files
 * followed by a newline at the very end of
 * each file
 */
void write_vector_headers(FILE *file, line_t **lines, int lines_size)
{
	int first = TRUE;

	int i;
	for (i = 0; i < lines_size; i++)
	{
		if (!first) fprintf(file, " ");
		else        first = FALSE; 

		fprintf(file, "%s", lines[i]->name);
	}

	fprintf(file, "\n");
}

/*
 * Reads in headers from a file and assigns them to elements in the
 * array of line_t * it returns.
 */
line_t** read_test_vector_headers(FILE *in, int *lines_size, int max_lines_size)
{
	char buffer [BUFFER_MAX_SIZE];
	int current_line = 0;
	int buffer_length = 0;
	line_t **lines = malloc(sizeof(line_t*)*max_lines_size);

	buffer[0] = '\0';
	do
	{
		char next = fgetc(in);

		if (next == EOF)
		{
			return NULL;
		}
		else if (next == ' ' || next == '\t' || next == '\n')
		{
			if (buffer_length)
			{
				lines[current_line++] = create_line(buffer);
				buffer_length = 0;			
			}

			if (next == '\n')
				break;
		}
		else
		{
			buffer[buffer_length++] = next;			
			buffer[buffer_length] = '\0';
		}		
	} while (1); 
	*lines_size = current_line;
	return lines;
}

/* 
 * Verifies that no lines are null. 
 */
int verify_lines (line_t **lines, int lines_size)
{
	int i; 
	for (i = 0; i < lines_size; i++)
	{
		int j;
		for (j = 0; j < lines[i]->number_of_pins; j++)
		{
			if (!lines[i]->pins[j])
			{
				warning_message(SIMULATION_ERROR, -1, -1, "A line has a NULL pin. This may cause a segfault. This can be caused when registers are declared as reg [X:Y], where Y is greater than zero.");
				return FALSE; 
			}
		}
	}
	return TRUE; 
}

/*
 * allocates memory for and instantiates a line_t struct
 */
line_t *create_line(char *name)
{
	line_t *line = malloc(sizeof(line_t));

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
	int length = strlen(buffer);
	const char *delim = " \t";
	
	if(buffer[length-2] == '\r' || buffer[length-2] == '\n') buffer[length-2] = '\0';
	if(buffer[length-1] == '\r' || buffer[length-1] == '\n') buffer[length-1] = '\0';

	int line_count = 0;	
	char *token = strtok(buffer, delim);
	while (token)
	{
		store_value_in_line(token, lines[line_count++], cycle);
		token = strtok(NULL, delim);
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
		if (lines[i]->type != OUTPUT)
		{
			if (strcmp(lines[i]->name, CLOCK_PORT_NAME_1) == 0 || strcmp(lines[i]->name, CLOCK_PORT_NAME_2) == 0)
			{
				update_pin_value(lines[i]->pins[0], cycle%2, cycle);			
			}
			else if (strcmp(lines[i]->name, RESET_PORT_NAME) == 0 || lines[i]->type == GND_NODE || lines[i]->type == PAD_NODE)
			{
				if (cycle == 0) update_pin_value(lines[i]->pins[0], 1, cycle);
				else            update_pin_value(lines[i]->pins[0], 0, cycle);
	
			}
			else if (lines[i]->type == VCC_NODE)
			{
				update_pin_value(lines[i]->pins[0], 1, cycle);			
			}
			else
			{
				int j;
				for (j = 0; j < lines[i]->number_of_pins; j++)
				{
					int r = rand();
					update_pin_value(lines[i]->pins[j], r % 2, cycle);
				}
			}
		}
	}
}

/*
 * Writes a wave of vectors to the given file. 
 */ 
void write_all_vectors_to_file(line_t **lines, int lines_size, FILE* file, FILE *modelsim_out, int type, int cycle_offset, int wave_length)
{
	if (!cycle_offset)
		write_vector_headers(file, lines, lines_size);

	int cycle;
	for (cycle = cycle_offset; cycle < (cycle_offset + wave_length); cycle++)
		write_vectors_to_file(lines, lines_size, file, modelsim_out, type, cycle);
}


/*
 * Writes all line values in lines[] such that line->type == type to the
 * file specified by the file parameter.
 */
void write_vectors_to_file(line_t **lines, int lines_size, FILE *file, FILE *modelsim_out, int type, int cycle)
{
	int first = TRUE;

	if (type == INPUT && modelsim_out)
		fprintf(modelsim_out, "run %d\n", cycle*101);

	int i; 
	for (i = 0; i < lines_size; i++)
	{
		if (lines[i]->type == type)
		{
			if (first) first = FALSE;
			else       fprintf(file, " ");

			if (lines[i]->number_of_pins == 1)
			{
				npin_t *pin;

				if (type == INPUT) pin = lines[i]->pins[0];
				else               pin = lines[i]->pins[0]->node->input_pins[lines[i]->pins[0]->pin_node_idx];

				if (get_pin_value(pin,cycle) < 0) fprintf(file, "x");
				else                              fprintf(file, "%d", get_pin_value(pin,cycle));

				if (type == INPUT && modelsim_out)
					fprintf(modelsim_out, "force %s %d %d\n", lines[i]->name,get_pin_value(pin,cycle), cycle * 100 + 95);
			}
			else
			{
				int j;
				int value = 0;
				int unknown = FALSE;

				if (type == INPUT)
				{
					for (j = 0; j < lines[i]->number_of_pins; j++)
						if (get_pin_value(lines[i]->pins[j],cycle) < 0)
							unknown = TRUE;
				} 
				else
				{
					for (j = 0; j < lines[i]->number_of_pins; j++)
						if (get_pin_value(lines[i]->pins[j]->node->input_pins[lines[i]->pins[j]->pin_node_idx],cycle) < 0)
							unknown = TRUE;
				}

				if (unknown)
				{
					for (j = lines[i]->number_of_pins - 1; j >= 0 ; j--)
					{
						npin_t *pin;
						if (type == INPUT) pin = lines[i]->pins[j];
						else               pin = lines[i]->pins[j]->node->input_pins[lines[i]->pins[j]->pin_node_idx];

						if (get_pin_value(pin,cycle) < 0) fprintf(file, "x");
						else                              fprintf(file, "%d", get_pin_value(pin,cycle));

						if (type == INPUT) warning_message(SIMULATION_ERROR, -1, -1, "Tried to write an unknown value to the modelsim script. It's likely unreliable. \n");
					}					
				}
				else
				{
					fprintf(file, "0x");

					if (type == INPUT && modelsim_out)
						fprintf(modelsim_out, "force %s 16#", lines[i]->name);

					for (j = lines[i]->number_of_pins - 1; j >= 0; j--)
					{
						npin_t *pin;

						if (type == INPUT) pin = lines[i]->pins[j];
						else               pin = lines[i]->pins[j]->node->input_pins[lines[i]->pins[j]->pin_node_idx];

						if (get_pin_value(pin,cycle) > 0)
							value += my_power(2, j % 4);

						if (j % 4 == 0)
						{
							fprintf(file, "%X", value);

							if (type == INPUT && modelsim_out)
								fprintf(modelsim_out, "%X", value);

							value = 0;
						}
					}

					if (type == INPUT && modelsim_out)
						fprintf(modelsim_out, " %d\n", cycle * 100 + 95);
				}
			}
		}
	}
	fprintf(file, "\n");
}

/*
 * Checks that each member line of lines[] such that line->type == OUTPUT
 * has corresponding values stored in input_pins[0] and output_pins[0].
 */
int verify_output_vectors(netlist_t *netlist, line_t **lines, int lines_size, int cycle)
{
	int problems = FALSE;
	int i;
	for (i = 0; i < lines_size; i++)
	{
		if (lines[i]->type != INPUT)
		{
			int j; 
			for (j = 0; j < lines[i]->number_of_pins; j++)
			{
				npin_t *output_pin = lines[i]->pins[j];
				npin_t *input_pin = output_pin->node->input_pins[output_pin->pin_node_idx];

				if (get_pin_value(input_pin,cycle) != get_pin_value(output_pin,cycle))
				{
					fprintf(stderr, "Simulation Value mismatch at node %s. Expected %d but encountered %d on cycle %d.\n",
							output_pin->node->name,
							get_pin_value(output_pin,cycle),
							get_pin_value(input_pin,cycle),
							cycle);
					problems = TRUE;
				}

				if (input_pin->cycle != output_pin->cycle || input_pin->cycle != cycle)
				{
					fprintf(stderr, "Simulation cycle mismatch at node %s. Expected cycle %d but encountered cycle %d on actual cycle %d.\n",
							output_pin->node->name,
							output_pin->cycle,
							input_pin->cycle,
							cycle);
					problems = TRUE;
				}
			}
		}
	}	
	return !problems;
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
	int result_size = a_length + b_length;
	int *result = calloc(sizeof(int), result_size);
	
	int i;
	for (i = 0; i < a_length; i++)
	{
		if (a[i] == 1)
		{
			int j; 
			for (j = 0; j < b_length; j++)
				result[i+j] += b[j];
		}
	}
	for (i = 0; i < result_size; i++)
	{
		while (result[i] > 1) {
			result[i] -= 2;
			result[i+1]++;
		}
	}
	return result;
}

/*
 * Computes memory. 
 */
void compute_memory(
	npin_t **inputs, 
	npin_t **outputs, 
	int data_width, 
	npin_t **addr, 
	int addr_width, 
	int we, 
	int clock,
	int cycle,
	int *data
)
{
	if (!clock)
	{
		int i; 
		for (i = 0; i < data_width; i++)
			update_pin_value(outputs[i], get_pin_value(outputs[i],cycle), cycle);
	}
	else
	{
		int address = 0;

		int i;
		for (i = 0; i < addr_width; i++) 
			address += 1 << (addr_width - 1 - i); 

		if (we)
		{
			for (i = 0; i < data_width; i++)
			{
				int write_address = i + (address * data_width);
				data[write_address] = get_pin_value(inputs[i],cycle);
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
}

/*
 * Initializes memory using a memory information file (mif). If not 
 * file is found, it is initialized to 0's. 
 */
void instantiate_memory(nnode_t *node, int **memory, int data_width, int addr_width)
{
	char *filename = node->name;
	char *input = (char *)malloc(sizeof(char)*BUFFER_MAX_SIZE);
	long long int max_address = my_power(2, addr_width);

	*memory = (int *)malloc(sizeof(int)*max_address*data_width);
	memset(*memory, 0, sizeof(int)*max_address*data_width);

	filename = strrchr(filename, '+') + 1;
	strcat(filename, ".mif");
	if (!filename) error_message(SIMULATION_ERROR, -1, -1, "Couldn't parse node name");
	
	FILE *mif = fopen(filename, "r");
	if (!mif)
	{
		warning_message(SIMULATION_ERROR, -1, -1, "Couldn't open MIF file %s",filename);
		return;
	}

	while (fgets(input, BUFFER_MAX_SIZE, mif))
		if (strcmp(input, "Content\n") == 0)
			break;

	while (fgets(input, BUFFER_MAX_SIZE, mif))
	{
		char *addr = (char *)malloc(sizeof(char)*BUFFER_MAX_SIZE);
		char *data = (char *)malloc(sizeof(char)*BUFFER_MAX_SIZE);

		if (!(strcmp(input, "Begin\n") == 0 || strcmp(input, "End;") == 0 || strcmp(input, "End;\n") == 0))
		{
			char *colon = strchr(input, ':');

			strncpy(addr, input, (colon-input));
			colon += 2;

			char *semicolon = strchr(input, ';');
			strncpy(data, colon, (semicolon-colon));

			long long int addr_val = strtol(addr, NULL, 10);
			long long int data_val = strtol(data, NULL, 16);
			
			int i;
			for (i = 0; i < data_width; i++)
			{
				int mask = (1 << ((data_width - 1) - i));
				int val = (mask & data_val) > 0 ? 1 : 0;
				int write_address = i + (addr_val * data_width);

				data[write_address] = val;
			}
		}
	}
	fclose(mif);
}
