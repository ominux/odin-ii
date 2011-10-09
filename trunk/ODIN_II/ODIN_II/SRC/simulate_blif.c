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
//#define NULL 0;

//extern struct global_args;

double wall_time() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (1000000*tv.tv_sec+tv.tv_usec)/1.0e6;
}

/*
 * Performs simulation. 
 */ 
void simulate_netlist(netlist_t *netlist)
{
	printf("Beginning simulation.\n"); fflush(stdout);

	int num_test_vectors = global_args.num_test_vectors;
	char *input_vector_file = global_args.sim_vector_input_file;
	// Existing output vectors to check against.
	char *output_vector_file = global_args.sim_vector_output_file;

	char **additional_pins = 0;
	int num_additional_pins = 0;

	// Parse the list of additional pins passed via the -p option.
	if (global_args.sim_additional_pins)
	{
		char *pin_list = strdup(global_args.sim_additional_pins);
		char *token    = strtok(pin_list, ",");
		int count = 0;
		while (token)
		{
			additional_pins = realloc(additional_pins,sizeof(char *) * (count +1));
			additional_pins[count++] = strdup(token);
			token = strtok(NULL, ",");
		}
		free(pin_list);
		num_additional_pins = count;
	}

	// Create and verify the lines.
	int input_lines_size;
	line_t **input_lines = create_input_test_vector_lines(&input_lines_size, netlist);
	if (!verify_lines(input_lines,  input_lines_size))
		error_message(SIMULATION_ERROR, 0, -1, "Input lines could not be assigned.");

	int output_lines_size;
	line_t **output_lines = create_output_test_vector_lines(&output_lines_size, netlist);
	if (!verify_lines(output_lines, output_lines_size))
		error_message(SIMULATION_ERROR, 0, -1, "Output lines could not be assigned.");

	FILE *out = fopen(OUTPUT_VECTOR_FILE_NAME, "w");
	if (!out) error_message(SIMULATION_ERROR, -1, -1, "Could not open output vector file.");

	FILE *modelsim_out = fopen("test.do", "w");
	if (!modelsim_out) error_message(SIMULATION_ERROR, -1, -1, "Could not open modelsim output file.");
	fprintf(modelsim_out, "add wave *\n");
	fprintf(modelsim_out, "force clk 1 0, 0 50 -repeat 100\n");

	FILE *in  = NULL;
	if (input_vector_file)
	{
		printf("Simulating input vector file.\n"); fflush(stdout);
		in = fopen(input_vector_file, "r");
		if (!in) error_message(SIMULATION_ERROR, -1, -1, "Could not open vector input file: %s", input_vector_file);

		// Count the test vectors in the file.
		num_test_vectors = 0;
		char buffer[BUFFER_MAX_SIZE];
		while (fgets(buffer, BUFFER_MAX_SIZE, in))
			num_test_vectors++;
		if (num_test_vectors) // Don't count the headers.
			num_test_vectors--;
		rewind(in);

		// Read the vector headers and check to make sure they match the lines.
		if (!verify_test_vector_headers(in, input_lines, input_lines_size))
			error_message(SIMULATION_ERROR, 0, -1, "Invalid vector header format in %s.", input_vector_file);
	}
	else
	{
		printf("Simulating using new vectors.\n"); fflush(stdout);
		in  = fopen( INPUT_VECTOR_FILE_NAME, "w");
		if (!in)  error_message(SIMULATION_ERROR, -1, -1, "Could not open input vector file.");
	}

	if (num_test_vectors)
	{
		double simulation_time = 0;
		stages *stages = 0;

		// Simulation is done in "waves" of SIM_WAVE_LENGTH cycles at a time.
		int  num_waves = ceil(num_test_vectors / (double)SIM_WAVE_LENGTH);
		int  wave;
		for (wave = 0; wave < num_waves; wave++)
		{
			int cycle_offset = SIM_WAVE_LENGTH * wave;
			int wave_length  = (wave < (num_waves-1))?SIM_WAVE_LENGTH:(num_test_vectors - cycle_offset);

			// Assign vectors to lines, either by reading or generating it.
			if (input_vector_file)
			{
				int cycle = cycle_offset;
				char buffer[BUFFER_MAX_SIZE];
				while (fgets(buffer, BUFFER_MAX_SIZE, in) && cycle < cycle_offset + wave_length)
				{
					test_vector *v = parse_test_vector(buffer);
					store_test_vector_in_lines(v, input_lines, input_lines_size, cycle++);
					free_test_vector(v);
				}
			}
			else
			{
				int cycle;
				for (cycle = cycle_offset; cycle < cycle_offset + wave_length; cycle++)
				{
					test_vector *v = generate_random_test_vector(input_lines, input_lines_size, cycle);
					store_test_vector_in_lines(v, input_lines, input_lines_size, cycle);
					free_test_vector(v);
				}
				write_wave_to_file(input_lines, input_lines_size, in, modelsim_out, INPUT, cycle_offset, wave_length);
			}

			printf("%6d/%d",wave+1,num_waves);

			double time = wall_time();

			// Perform simulation
			int cycle;
			for (cycle = cycle_offset; cycle < cycle_offset + wave_length; cycle++)
			{
				if (!cycle)
				{	// The first cycle produces the stages, and adds additional lines as specified by the -p option.
					stages = simulate_first_cycle(netlist, cycle, additional_pins, num_additional_pins, &output_lines, &output_lines_size);
					// Make sure the output lines are still OK after adding custom lines.
					if (!verify_lines(output_lines, output_lines_size))
						error_message(SIMULATION_ERROR, -1, -1, "Problem detected with the output lines after the first cycle.");
				}
				else
				{
					simulate_cycle(netlist, cycle, stages);
				}
			}

			simulation_time += wall_time() - time;

			// Write the result of this wave to the output vector file.
			write_wave_to_file(output_lines, output_lines_size, out, modelsim_out, OUTPUT, cycle_offset, wave_length);

			int wave_cols = wave_length+10;
			int display_cols = 80;
			if (!((wave+1) % (display_cols/wave_cols)))
				printf("\n");
		}

		fclose(out);

		printf("\n");

		// If a second output vector file was given via the -T option, verify that it matches.
		if (output_vector_file)
			if (verify_output_vectors(output_vector_file, num_test_vectors))
				printf("Vectors match\n");

		printf("Number of nodes: %d\n", stages->num_nodes);
		printf("Simulation time: %fs\n", simulation_time);

		fprintf(modelsim_out, "run %d\n", (num_test_vectors*100) + 100);

		free_stages(stages);
	}
	else
	{
		printf("No vectors to simulate.\n");
	}

	while (num_additional_pins--)
		free(additional_pins[num_additional_pins]);
	free(additional_pins);

	free_lines(output_lines, output_lines_size);
	free_lines(input_lines, input_lines_size);

	fclose(modelsim_out);
	fclose(in);
}

/*
 * This simulates a single cycle using the stages generated
 * during the first cycle. Simulates in parallel if OpenMP is enabled.
 */
void simulate_cycle(netlist_t *netlist, int cycle, stages *s)
{
	int i;
	for(i = 0; i < s->count; i++)
	{
		int j;
		#ifdef _OPENMP
		if (s->counts[i] < 150)
		#endif
		{
			for (j = 0; j < s->counts[i]; j++)
				compute_and_store_value(s->stages[i][j], cycle);

		}
		#ifdef _OPENMP
		else
		{
			#pragma omp parallel for
			for (j = 0; j < s->counts[i]; j++)
				compute_and_store_value(s->stages[i][j], cycle);
		}
		#endif
	}

	printf("."); fflush(stdout);
}

/*
 * Simulates the first cycle by traversing the netlist and returns
 * the nodes organised into parallelizable stages. Also adds lines to
 * custom pins and nodes as requested via the -p option.
 */
stages *simulate_first_cycle
(
		netlist_t *netlist,
		int cycle,
		char **additional_pins, int num_additional_pins,
		line_t ***lines, int *num_lines
)
{
	queue_t *queue = create_queue();
	// Enqueue top input nodes
	int i;
	for (i = 0; i < netlist->num_top_input_nodes; i++)
		enqueue_node_if_ready(queue,netlist->top_input_nodes[i],cycle);

	// Enqueue constant nodes.
	nnode_t *constant_nodes[] = {netlist->gnd_node, netlist->vcc_node, netlist->pad_node};
	int num_constant_nodes = 3;
	for (i = 0; i < num_constant_nodes; i++)
		enqueue_node_if_ready(queue,constant_nodes[i],cycle);

	nnode_t **ordered_nodes = 0;
	int   num_ordered_nodes = 0;

	nnode_t *node;
	while ((node = queue->remove(queue)))
	{
		compute_and_store_value(node, cycle);

		// Add custom pins to the lines as found.
		// TODO: Should be factored into a separate function.
		if (num_additional_pins)
		{
			int add = FALSE;
			int j, k;
			char *port_name = 0;
			for (j = 0; j < node->num_output_pins; j++)
			{
				if (node->output_pins[j]->name)
				{
					for (k = 0; k < num_additional_pins; k++)
					{
						if (strstr(node->output_pins[j]->name,additional_pins[k]))
						{
							add = TRUE;
							break;
						}
					}
					free(port_name);
					if (add) break;
				}
			}

			if (!add && node->name && strlen(node->name) && strchr(node->name, '^'))
			{
				for (k = 0; k < num_additional_pins; k++)
				{
					if (strstr(node->name,additional_pins[k]))
					{
						add = TRUE;
						break;
					}
				}
			}

			if (add)
			{
				int single_pin = strchr(additional_pins[k], '~')?1:0;

				port_name = strdup(strchr(node->name, '^') + 1);
				char *tilde = strchr(port_name, '~');
				if (tilde && !single_pin)
					*tilde = '\0';

				if (find_portname_in_lines(port_name, *lines, *num_lines) == -1)
				{
					line_t *line = create_line(port_name);
					(*lines) = realloc((*lines), sizeof(line_t*) * ((*num_lines)+1));
					(*lines)[(*num_lines)++] = line;
				}
				assign_node_to_line(node, *lines, *num_lines, OUTPUT, single_pin);
				free(port_name);
			}
		}

		// Enqueue child nodes which are ready, not already queued, and not already complete.
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
		ordered_nodes = realloc(ordered_nodes, sizeof(nnode_t *) * (num_ordered_nodes + 1));
		ordered_nodes[num_ordered_nodes++] = node;
	}
	queue->destroy(queue);

	// Reorganise the ordered nodes into stages for parallel computation.
	stages *s = stage_ordered_nodes(ordered_nodes, num_ordered_nodes);
	free(ordered_nodes);

	printf("*"); fflush(stdout);

	return s;
}

/*
 * Puts the ordered nodes in stages which can be computed in parallel.
 */
stages *stage_ordered_nodes(nnode_t **ordered_nodes, int num_ordered_nodes) {
	stages *s = malloc(sizeof(s));
	s->stages = calloc(1,sizeof(nnode_t**));
	s->counts = calloc(1,sizeof(int));
	s->count  = 1;

	const int index_table_size = (num_ordered_nodes/100)+10;

	// Hash tables index the nodes in the current stage, as well as their children.
	hashtable_t *stage_children = create_hashtable(index_table_size);
	hashtable_t *stage_nodes    = create_hashtable(index_table_size);

	int i;
	for (i = 0; i < num_ordered_nodes; i++)
	{
		nnode_t* node = ordered_nodes[i];
		int stage = s->count-1;

		// Get the node's children for dependency checks.
		int num_children;
		nnode_t **children = get_children_of(node, &num_children);

		// Determine if the node is a child of any node in the current stage.
		int is_child_of_stage = stage_children->get(stage_children, node, sizeof(nnode_t*))?1:0;

		// Determine if any node in the current stage is a child of this node.
		int is_stage_child_of = FALSE;
		int j;
		if (!is_child_of_stage)
			for (j = 0; j < num_children; j++)
				if ((is_stage_child_of = stage_nodes->get(stage_nodes, children[j], sizeof(nnode_t*))?1:0))
					break;

		// Start a new stage if this node is related to any node in the current stage.
		if (is_child_of_stage || is_stage_child_of)
		{
			s->stages = realloc(s->stages, sizeof(nnode_t**) * (s->count+1));
			s->counts = realloc(s->counts, sizeof(int)       * (s->count+1));
			stage = s->count++;
			s->stages[stage] = 0;
			s->counts[stage] = 0;

			stage_children->destroy(stage_children);
			stage_nodes   ->destroy(stage_nodes);

			stage_children = create_hashtable(index_table_size);
			stage_nodes    = create_hashtable(index_table_size);
		}

		// Add the node to the current stage.
		s->stages[stage] = realloc(s->stages[stage],sizeof(nnode_t*) * (s->counts[stage]+1));
		s->stages[stage][s->counts[stage]++] = node;

		// Index the node.
		stage_nodes->add(stage_nodes, node, sizeof(nnode_t*), node);

		// Index its children.
		for (j = 0; j < num_children; j++)
			stage_children->add(stage_children, children[j], sizeof(nnode_t*), children[j]);

		free(children);
	}
	stage_children->destroy(stage_children);
	stage_nodes   ->destroy(stage_nodes);

	// Add the total number of nodes to the stages structure.
	s->num_nodes = num_ordered_nodes;

	return s;
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
		if (node->output_pins[i] && node->output_pins[i]->cycle < cycle)
			return FALSE;			

	return TRUE;
}

/*
 * Checks to see if the node is ready to be simulated for the given cycle.
 */
inline int is_node_ready(nnode_t* node, int cycle)
{
	if (node->type == FF_NODE)
		cycle--;

	int i;
	for (i = 0; i < node->num_input_pins; i++)
		if (node->input_pins[i]->cycle < cycle)
			return FALSE;

	return TRUE;
}

/*
 * Gets the children of the given node. Returns the number of children via the num_children parameter. 
 */ 
nnode_t **get_children_of(nnode_t *node, int *num_children)
{
	nnode_t **children = 0;
	int count = 0;
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
				{
					children = realloc(children, sizeof(nnode_t*) * (count + 1));
					children[count++] = fanout_pin->node;
				}
			}
		}
	}
	*num_children = count;
	return children; 
}

/*
 * Sets the pin to the given value for the given cycle. Does not
 * propagate the value to the connected net.
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
		{	// < 010 1
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			signed char pin0 = get_pin_value(node->input_pins[0],cycle);
			signed char pin1 = get_pin_value(node->input_pins[1],cycle);
			signed char pin2 = get_pin_value(node->input_pins[2],cycle);

			if      (pin0  < 0 || pin1  < 0 || pin2  < 0) update_pin_value(node->output_pins[0], -1, cycle);
			else if (pin0 == 0 && pin1 == 1 && pin2 == 0) update_pin_value(node->output_pins[0],  1, cycle);
			else                                          update_pin_value(node->output_pins[0],  0, cycle);
			return;
		}
		case GT:
		{	// > 100 1
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			signed char pin0 = get_pin_value(node->input_pins[0],cycle);
			signed char pin1 = get_pin_value(node->input_pins[1],cycle);
			signed char pin2 = get_pin_value(node->input_pins[2],cycle);

			if      (pin0  < 0 || pin1  < 0 || pin2  < 0) update_pin_value(node->output_pins[0], -1, cycle);
			else if (pin0 == 1 && pin1 == 0 && pin2 == 0) update_pin_value(node->output_pins[0],  1, cycle);
			else                                          update_pin_value(node->output_pins[0],  0, cycle);

			return;
		}
		case ADDER_FUNC:
		{	// 001 1\n010 1\n100 1\n111 1
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			signed char pin0 = get_pin_value(node->input_pins[0],cycle);
			signed char pin1 = get_pin_value(node->input_pins[1],cycle);
			signed char pin2 = get_pin_value(node->input_pins[2],cycle);

			if (
					   (pin0 == 0 && pin1 == 0 && pin2 == 1)
					|| (pin0 == 0 && pin1 == 1 && pin2 == 0)
					|| (pin0 == 1 && pin1 == 0 && pin2 == 0)
					|| (pin0 == 1 && pin1 == 1 && pin2 == 1)
			)
				update_pin_value(node->output_pins[0], 1, cycle);
			else if (pin0 < 0 || pin1 < 0 || pin2 < 0)
				update_pin_value(node->output_pins[0], -1, cycle);
			else
				update_pin_value(node->output_pins[0], 0, cycle);

			return;
		}
		case CARRY_FUNC:
		{	// 011 1\n100 1\n110 1\n111 1
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);
			
			signed char pin0 = get_pin_value(node->input_pins[0],cycle);
			signed char pin1 = get_pin_value(node->input_pins[1],cycle);
			signed char pin2 = get_pin_value(node->input_pins[2],cycle);

			if (
				   (pin0 == 1 && (pin1 == 1 || pin2 == 1))
				|| (pin1 == 1 && pin2 == 1)
			)
				update_pin_value(node->output_pins[0], 1, cycle);

			else if (pin0 < 0 || pin1 < 0 || pin2 < 0)
				update_pin_value(node->output_pins[0], -1, cycle);

			else
				update_pin_value(node->output_pins[0], 0, cycle);

			return;
		}
		case BITWISE_NOT:
		{
			oassert(node->num_input_pins == 1);
			oassert(node->num_output_pins == 1);

			signed char pin = get_pin_value(node->input_pins[0], cycle);

			if      (pin  < 0) update_pin_value(node->output_pins[0], -1, cycle);
			else if (pin == 1) update_pin_value(node->output_pins[0],  0, cycle);
			else               update_pin_value(node->output_pins[0],  1, cycle);
			return;
		}
		case LOGICAL_AND:
		{	// &&
			oassert(node->num_output_pins == 1);
			for (i = 0; i < node->num_input_pins; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin <  0) { unknown = TRUE; }
				if (pin == 0) { update_pin_value(node->output_pins[0],  0, cycle); return; }
			}
			if (unknown) update_pin_value(node->output_pins[0], -1, cycle);
			else         update_pin_value(node->output_pins[0],  1, cycle);
			return;
		}
		case LOGICAL_OR:
		{	// ||
			oassert(node->num_output_pins == 1);
			for (i = 0; i < node->num_input_pins; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin <  0) { unknown = TRUE; }
				if (pin == 1) { update_pin_value(node->output_pins[0],  1, cycle); return; }
			}
			if (unknown) update_pin_value(node->output_pins[0], -1, cycle);
			else         update_pin_value(node->output_pins[0],  0, cycle);
			return;
		}
		case LOGICAL_NAND:
		{	// !&&
			oassert(node->num_output_pins == 1);
			for (i = 0; i < node->num_input_pins; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin <  0) { unknown = TRUE; }
				if (pin == 0) { update_pin_value(node->output_pins[0],  1, cycle); return; }
			}
			if (unknown) update_pin_value(node->output_pins[0], -1, cycle);
			else         update_pin_value(node->output_pins[0],  0, cycle);
			return;
		}
		case LOGICAL_NOT: // !
		case LOGICAL_NOR:
		{	// !|
			oassert(node->num_output_pins == 1);
			for (i = 0; i < node->num_input_pins; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin <  0) { unknown = TRUE; }
				if (pin == 1) { update_pin_value(node->output_pins[0],  0, cycle); return; }
			}
			if (unknown) update_pin_value(node->output_pins[0], -1, cycle);
			else         update_pin_value(node->output_pins[0], 1, cycle);
			return;
		}
		case LOGICAL_EQUAL:	// ==
		case LOGICAL_XOR:
		{	// ^
			oassert(node->num_output_pins == 1);
			int ones = 0;
			for (i = 0; i < node->num_input_pins; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin <  0) { update_pin_value(node->output_pins[0], -1, cycle); return; }
				if (pin == 1) { ones++; }
			}
			if ((ones % 2) == 1) update_pin_value(node->output_pins[0], 1, cycle);
			else                 update_pin_value(node->output_pins[0], 0, cycle);
			return;
		}
		case NOT_EQUAL:			// !=
		case LOGICAL_XNOR:
		{	// !^
			oassert(node->num_output_pins == 1);
			int ones = 0;
			for (i = 0; i < node->num_input_pins; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin <  0) { update_pin_value(node->output_pins[0], -1, cycle); return; }
				if (pin == 1) { ones++; }
			}
			if ((ones % 2) == 1) update_pin_value(node->output_pins[0], 0, cycle);
			else                 update_pin_value(node->output_pins[0], 1, cycle);
			return;
		}
		case MUX_2: // May still be incorrect for 3 valued logic.
		{
			oassert(node->num_output_pins == 1);
			oassert(node->num_input_port_sizes >= 2);
			oassert(node->input_port_sizes[0] == node->input_port_sizes[1]);

			for (i = 0; i < node->input_port_sizes[0]; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin < 0)
				{
					unknown = TRUE;
				}

				if (pin == 1 && pin == get_pin_value(node->input_pins[i+node->input_port_sizes[0]],cycle)) {
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
			compute_memory_node(node,cycle);
			return;
		}
		case HARD_IP:
		{
			oassert(node->input_port_sizes[0] > 0);
			oassert(node->output_port_sizes[0] > 0);
			
			compute_hard_ip_node(node,cycle);
			return;
		}
		case MULTIPLY:
		{
			oassert(node->num_input_port_sizes >= 2);
			oassert(node->num_output_port_sizes == 1);
				
			compute_multiply_node(node,cycle);
			return;
		}
		case GENERIC :
		{
			compute_generic_node(node,cycle);
			return;
		}
		case INPUT_NODE:  return; 
		case OUTPUT_NODE: return;
		case PAD_NODE:
		{
			for (i = 0; i < node->num_output_pins; i++)
				update_pin_value(node->output_pins[i], 0, cycle);

			return;
		}
		case CLOCK_NODE:
		{
			for (i = 0; i < node->num_output_pins; i++)
				update_pin_value(node->output_pins[i], cycle % 2, cycle);

			return;
		}
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

void compute_memory_node(nnode_t *node, int cycle)
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

		int i;
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
			instantiate_memory(node, &(node->memory_data), data_width, addr_width);

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

		int i;
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
			instantiate_memory(node, &(node->memory_data), data_width2, addr_width2);

		compute_memory(data1, out1, data_width1, addr1, addr_width1, we1, clock, cycle, node->memory_data);
		compute_memory(data2, out2, data_width2, addr2, addr_width2, we2, clock, cycle, node->memory_data);
	}
}

// TODO: Needs to be verified.
void compute_hard_ip_node(nnode_t *node, int cycle)
{
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

	int i;
	for (i = 0; i < node->num_input_pins; i++)
		input_pins[i] = get_pin_value(node->input_pins[i],cycle);

	(node->simulate_block_cycle)(cycle, node->num_input_pins, input_pins, node->num_output_pins, output_pins);

	for (i = 0; i < node->num_output_pins; i++)
		update_pin_value(node->output_pins[i], output_pins[i], cycle);

	free(input_pins);
	free(output_pins);
}

// TODO: Needs to be verified.
void compute_multiply_node(nnode_t *node, int cycle)
{
	int *a = malloc(sizeof(int)*node->input_port_sizes[0]);
	int *b = malloc(sizeof(int)*node->input_port_sizes[1]);

	int i;
	int unknown = FALSE;

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
			update_pin_value(node->output_pins[i], -1, cycle);
	}

	int *result = multiply_arrays(a, node->input_port_sizes[0], b, node->input_port_sizes[1]);

	for (i = 0; i < node->num_output_pins; i++)
		update_pin_value(node->output_pins[i], result[i], cycle);

	free(result);
	free(a);
	free(b);
}

// TODO: Needs to be verified.
void compute_generic_node(nnode_t *node, int cycle)
{
	int line_count_bitmap = node->bit_map_line_count;
	char **bit_map = node->bit_map;

	int lut_size  = 0;
	while (bit_map[0][lut_size] != 0)
		lut_size++;

	int found = 0;
	int i;
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
				break;
		}

		if (j == lut_size) found = TRUE;
	}

	if (found) update_pin_value(node->output_pins[0], 1, cycle);
	else       update_pin_value(node->output_pins[0], 0, cycle);
}

/*
 * Takes two arrays of integers (1's and 0's) and returns an array
 * of integers (1's and 0's) that represent their product. The
 * length of the returned array is twice that of the two parameters.
 *
 * This array will need to be freed later!
 */
// TODO: Needs to be verified.
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
		while (result[i] > 1)
		{
			result[i] -= 2;
			result[i+1]++;
		}
	}
	return result;
}

/*
 * Computes memory.
 */
// TODO: Needs to be verified.
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
// TODO: Needs to be verified.
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

			long long int addr_val = strtol(addr, 0, 10);
			long long int data_val = strtol(data, 0, 16);

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

/*
 * Searches for a line with the given name in the lines. Returns the id
 * or -1 if no such line was found.
 */
int find_portname_in_lines(char* port_name, line_t **lines, int count)
{
	int found = FALSE;
	int j;
	for (j = 0; j < count; j++)
	{
		if (!strcmp(lines[j]->name, port_name))
		{
			found = TRUE;
			break;
		}
	}

	if (!found) return -1;
	else        return  j;
}

/*
 * Assigns the given node to its corresponding line in the given array of line.
 * Assumes the line has already been created.
 */
void assign_node_to_line(nnode_t *node, line_t **lines, int lines_size, int type, int single_pin)
{
	if (!node->num_output_pins)
	{
		npin_t *pin = allocate_npin();
		allocate_more_node_output_pins(node, 1);
		add_a_output_pin_to_node_spot_idx(node, pin, 0);
	}

	// Grab the portion of the name ater the ^
	char *port_name = strdup(strchr(node->name, '^') + 1);
	// Find out if there is a ~
	char *tilde = strchr(port_name, '~');
	/*
	 * If there's a ~, this is a multi-pin port. Unless we want to forcibly
	 * treat it as a single pin, get the pin number, and truncate the
	 * port name by placing null in the position of the tilde.
	 */
	int pin_number;
	if (tilde && !single_pin) {
		pin_number = atoi(tilde+1);
		*tilde = '\0';
	}
	// Otherwise, pretend there is no tilde.
	else {
		tilde = 0;
	}

	int j = find_portname_in_lines(port_name, lines, lines_size);

	if (!tilde)
	{	// Treat the pin as a lone single pin.
		if (j == -1)
		{
			if (!(node->type == GND_NODE || node->type == VCC_NODE || node->type == PAD_NODE || node->type == CLOCK_NODE))
				warning_message(SIMULATION_ERROR, -1, -1, "Could not map single-bit top-level input node '%s' to input vector", node->name);
		} 
		else
		{
			lines[j]->number_of_pins = 1;
			lines[j]->pins = malloc(sizeof(npin_t *));
			lines[j]->pins[0] = node->output_pins[0];
			lines[j]->type = type;
		}		
	}
	else
	{	// Treat the pin as part of a multi-pin port.
		if (j == -1)
		{
			warning_message(SIMULATION_ERROR, -1, -1, "Could not map multi-bit top-level input node '%s' to input vector", node->name);			
		} 
		else
		{
			lines[j]->pins = realloc(lines[j]->pins, sizeof(npin_t*)* (lines[j]->number_of_pins + 1));
			lines[j]->pins[lines[j]->number_of_pins++] = node->output_pins[0];
			lines[j]->type = type;
		}
	}
	free(port_name);
}

/*
 * Given a netlist, this function maps the top_input_nodes
 * to a line_t* each. It stores them in an array and returns it,
 * storing the array size in the *lines_size pointer.
 */
line_t **create_input_test_vector_lines(int *lines_size, netlist_t *netlist)
{
	line_t **lines = 0;
	int count = 0;
	int i; 	
	for (i = 0; i < netlist->num_top_input_nodes; i++)
	{
		nnode_t *node = netlist->top_input_nodes[i];
		char *port_name = strdup(strchr(node->name, '^') + 1);
		char *tilde = strchr(port_name, '~');

		if (tilde) *tilde = '\0';

		if (strcmp(port_name, RESET_PORT_NAME) && node->type != CLOCK_NODE)
		{
			if (find_portname_in_lines(port_name, lines, count) == -1)
			{
				line_t *line = create_line(port_name);
				lines = realloc(lines, sizeof(line_t*)*(count+1));
				lines[count++] = line;
			}
			assign_node_to_line(node, lines, count, INPUT,0);
		}
		free(port_name);
	}
	*lines_size = count;
	return lines;
}

/*
 * Given a netlist, this function maps the top_output_nodes
 * to a line_t* each. It stores them in an array and returns it,
 * storing the array size in the *lines_size pointer.
 */
line_t **create_output_test_vector_lines(int *lines_size, netlist_t *netlist)
{
	line_t **lines = 0;
	int count = 0;
	int i; 
	for (i = 0; i < netlist->num_top_output_nodes; i++)
	{
		nnode_t *node = netlist->top_output_nodes[i];
		char *port_name = strdup(strchr(node->name, '^') + 1);
		char *tilde = strchr(port_name, '~');

		if (tilde) *tilde = '\0';

		if (find_portname_in_lines(port_name, lines, count) == -1)
		{
			line_t *line = create_line(port_name);
			lines = realloc(lines, sizeof(line_t*)*(count+1));
			lines[count++] = line;
		}
		assign_node_to_line(node, lines, count, OUTPUT,0);

		free(port_name);
	}
	*lines_size = count;
	return lines;
}

/*
 * Writes the lines[] elements' names to the files followed by a newline at the very end of
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
	fflush(file);
}

/*
 * Parses the first line of the given file pointer and compares it to the
 * given nodes for identity. If there is any difference, a warning is printed,
 * and FALSE is returned. If there are no differences, the file pointer is left
 * at the start of the first line after the header, and TRUE is returned.
 */
int verify_test_vector_headers(FILE *in, line_t **lines, int lines_size)
{
	rewind(in);
	char buffer [BUFFER_MAX_SIZE];
	int current_line = 0;
	int buffer_length = 0;

	buffer[0] = '\0';
	do
	{
		char next = fgetc(in);

		if (next == EOF)
		{
			warning_message(SIMULATION_ERROR, 0, -1, "Hit end of file.");

			return FALSE;
		}
		else if (next == ' ' || next == '\t' || next == '\n')
		{
			if (buffer_length)
			{
				if(strcmp(lines[current_line]->name,buffer))
				{
					warning_message(SIMULATION_ERROR, 0, -1, "Vector header mismatch: \"%s\" conflicts with \"%s\". Given vectors probably don't belong to this circuit.", lines[current_line]->name,buffer);
					return FALSE;
				}
				else
				{
					buffer_length = 0;
					current_line++;
				}
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

	return TRUE;
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
				warning_message(SIMULATION_ERROR, 0, -1, "A line %d:(%s) has a NULL pin. This may cause a segfault. This can be caused when registers are declared as reg [X:Y], where Y is greater than zero.", j, lines[i]->name);
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
	line->pins = 0;
	line->type = -1;
	line->name = malloc(sizeof(char)*(strlen(name)+1));

	strcpy(line->name, name);

	return line;
}

/*
 * Stores the given test vector in the given lines, with some sanity checking to ensure that it
 * has a compatible geometry.
 */
void store_test_vector_in_lines(test_vector *v, line_t **lines, int num_lines, int cycle)
{
	if (num_lines < v->count) error_message(SIMULATION_ERROR, 0, -1, "Fewer lines (%d) than values (%d).", num_lines, v->count);
	if (num_lines > v->count) error_message(SIMULATION_ERROR, 0, -1, "More lines (%d) than values (%d).", num_lines, v->count);

	int l;
	for (l = 0; l < v->count; l++)
	{
		line_t *line = lines[l];

		if (line->number_of_pins < 1)
		{
			warning_message(SIMULATION_ERROR, -1, -1, "Found a line '%s' with no pins.", line->name);
		}
		else
		{
			int i;
			for (i = 0; i < v->counts[l] && i < line->number_of_pins; i++)
			{
				update_pin_value(line->pins[i],  v->values[l][i], cycle);
			}

			for (; i < line->number_of_pins; i++)
				update_pin_value(line->pins[i], 0, cycle);
		}
	}
}

/*
 * Compares two test vectors for numerical and geometric identity. Returns FALSE if
 * they are found to be different, and TRUE otherwise.
 */
int compare_test_vectors(test_vector *v1, test_vector *v2)
{
	if (v1->count != v2->count)
	{
		warning_message(SIMULATION_ERROR, 0, -1, "Vector lengths differ.");
		return FALSE;
	}

	int l;
	for (l = 0; l < v1->count; l++)
	{
		int i;
		for (i = 0; i < v1->counts[l] && i < v2->counts[l]; i++)
		{
			if (v1->values[l][i] != v2->values[l][i])
				return FALSE;
		}

		if (v1->counts[l] != v2->counts[l])
		{
			test_vector *v = v1->counts[l] < v2->counts[l] ? v2 : v1;

			int j;
			for (j = i; j < v->counts[l]; j++)
			{
				if (v->values[l][j] != 0) return FALSE;
			}
		}
	}
	return TRUE;
}

/*
 * Parses the given line from a test vector file into a
 * test_vector data structure.
 */
test_vector *parse_test_vector(char *buffer)
{
	buffer = strdup(buffer);
	test_vector *v = malloc(sizeof(test_vector));
	v->values = 0;
	v->counts = 0;
	v->count = 0;

	int length = strlen(buffer);

	if(buffer[length-2] == '\r' || buffer[length-2] == '\n') buffer[length-2] = '\0';
	if(buffer[length-1] == '\r' || buffer[length-1] == '\n') buffer[length-1] = '\0';

	const char *delim = " \t";
	char *token = strtok(buffer, delim);
	while (token)
	{
		v->values = realloc(v->values, sizeof(signed char **) * (v->count + 1));
		v->counts = realloc(v->counts, sizeof(int) * (v->count + 1));
		v->values[v->count] = 0;
		v->counts[v->count] = 0;

		if (strlen(token) == 1)
		{   // Only 1 pin.
			signed char value = -1;
			if      (token[0] == '0') value = 0;
			else if (token[0] == '1') value = 1;

			v->values[v->count] = realloc(v->values[v->count], sizeof(signed char) * (v->counts[v->count] + 1));
			v->values[v->count][v->counts[v->count]++] = value;
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

				for (k = 0; k < 4; k++)
				{
					signed char bit = (value & (1 << k)) > 0 ? 1 : 0;
					v->values[v->count] = realloc(v->values[v->count], sizeof(signed char) * (v->counts[v->count] + 1));
					v->values[v->count][v->counts[v->count]++] = bit;
				}
			}
		}
		else
		{
			int i;
			for (i = strlen(token) - 1; i >= 0; i--)
			{
				signed char value = -1;
				if      (token[i] == '0') value = 0;
				else if (token[i] == '1') value = 1;

				v->values[v->count] = realloc(v->values[v->count], sizeof(signed char) * (v->counts[v->count] + 1));
				v->values[v->count][v->counts[v->count]++] = value;
			}
		}
		v->count++;
		token = strtok(NULL, delim);
	}
	free(buffer);
	return v;
}

/*
 * Generates a "random" test_vector structure based on the geometry of the given lines.
 *
 * If you want better randomness, call srand at some point.
 */
test_vector *generate_random_test_vector(line_t **lines, int lines_size, int cycle)
{
	test_vector *v = malloc(sizeof(test_vector));
	v->values = 0;
	v->counts = 0;
	v->count = 0;

	int i;
	for (i = 0; i < lines_size; i++)
	{
		v->values = realloc(v->values, sizeof(signed char **) * (v->count + 1));
		v->counts = realloc(v->counts, sizeof(int) * (v->count + 1));
		v->values[v->count] = 0;
		v->counts[v->count] = 0;

		if (lines[i]->type == CLOCK_NODE)
		{
			v->values[v->count] = realloc(v->values[v->count], sizeof(signed char) * (v->counts[v->count] + 1));
			v->values[v->count][v->counts[v->count]++] = cycle % 2;
		}
		else if (!strcmp(lines[i]->name, RESET_PORT_NAME) || lines[i]->type == GND_NODE || lines[i]->type == PAD_NODE)
		{
			v->values[v->count] = realloc(v->values[v->count], sizeof(signed char) * (v->counts[v->count] + 1));
			v->values[v->count][v->counts[v->count]++] = (cycle == 0) ? 1 : 0;
		}
		else if (lines[i]->type == VCC_NODE)
		{
			v->values[v->count] = realloc(v->values[v->count], sizeof(signed char) * (v->counts[v->count] + 1));
			v->values[v->count][v->counts[v->count]++] = 1;
		}
		else
		{
			int j;
			for (j = 0; j < lines[i]->number_of_pins; j++)
			{
				v->values[v->count] = realloc(v->values[v->count], sizeof(signed char) * (v->counts[v->count] + 1));
				v->values[v->count][v->counts[v->count]++] = rand() % 2;
			}
		}
		v->count++;
	}
	return v;
}

/*
 * Writes a wave of vectors to the given file. Writes the headers
 * prior to cycle 0.
 */ 
void write_wave_to_file(line_t **lines, int lines_size, FILE* file, FILE *modelsim_out, int type, int cycle_offset, int wave_length)
{
	if (!cycle_offset)
		write_vector_headers(file, lines, lines_size);

	int cycle;
	for (cycle = cycle_offset; cycle < (cycle_offset + wave_length); cycle++)
		write_vector_to_file(lines, lines_size, file, modelsim_out, type, cycle);
}

/*
 * Writes all line values in lines[] such that line->type == type to the
 * file specified by the file parameter.
 *
 * TODO: Factor modelsim output and disentangle it from vector output.
 */
void write_vector_to_file(line_t **lines, int lines_size, FILE *file, FILE *modelsim_out, int type, int cycle)
{
	int first = TRUE;

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
				nnode_t *node = lines[i]->pins[0]->node;

				if (type == INPUT || !node->input_pins) pin = lines[i]->pins[0];
				else                                    pin = node->input_pins[lines[i]->pins[0]->pin_node_idx];

				if (get_pin_value(pin,cycle) < 0) fprintf(file, "x");
				else                              fprintf(file, "%d", get_pin_value(pin,cycle));

				if (type == INPUT && modelsim_out)
					fprintf(modelsim_out, "force %s %d %d\n", lines[i]->name,get_pin_value(pin,cycle), cycle * 100);
			}
			else
			{
				int j;
				int value = 0;
				int unknown = FALSE;

				nnode_t *node = lines[i]->pins[0]->node;

				if (type == INPUT || !node->input_pins)
				{
					for (j = 0; j < lines[i]->number_of_pins; j++)
						if (get_pin_value(lines[i]->pins[j],cycle) < 0)
							unknown = TRUE;
				} 
				else
				{
					for (j = 0; j < lines[i]->number_of_pins; j++)
						if (get_pin_value(node->input_pins[lines[i]->pins[j]->pin_node_idx],cycle) < 0)
							unknown = TRUE;
				}

				if (unknown)
				{
					for (j = lines[i]->number_of_pins - 1; j >= 0 ; j--)
					{
						npin_t *pin;
						nnode_t *node = lines[i]->pins[j]->node;

						if (type == INPUT || !node->input_pins) pin = lines[i]->pins[j];
						else                                    pin = node->input_pins[lines[i]->pins[j]->pin_node_idx];

						if (get_pin_value(pin,cycle) < 0) fprintf(file, "x");
						else                              fprintf(file, "%d", get_pin_value(pin,cycle));

						if (type == INPUT)
							warning_message(SIMULATION_ERROR, -1, -1, "Tried to write an unknown value to the modelsim script. It's likely unreliable. \n");
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
						nnode_t *node = lines[i]->pins[j]->node;

						if (type == INPUT || !node->input_pins) pin = lines[i]->pins[j];
						else                                    pin = node->input_pins[lines[i]->pins[j]->pin_node_idx];

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
						fprintf(modelsim_out, " %d\n", cycle * 100);
				}
			}
		}
	}
	fprintf(file, "\n");
}

/*
 * Verify that the given output vector file matches is identical (numerically)
 * to the one written by the current simulation. This is done by parsing each
 * file vector by vector and comparing them. Also verifies that the headers are identical.
 *
 * Prints appropriate warning messages when differences are found.
 *
 * Returns false if the files differ and true if they are identical, with the exception of
 * number format.
 */
int verify_output_vectors(char* output_vector_file, int num_test_vectors)
{
	int error = FALSE;

	// The filename cannot be the same as our default output file.
	if (!strcmp(output_vector_file,OUTPUT_VECTOR_FILE_NAME))
	{
		error = TRUE;
		warning_message(SIMULATION_ERROR,0,-1,
				"Vector file \"%s\" given for verification "
				"is the same as the default output file \"%s\". "
				"Ignoring.", output_vector_file, OUTPUT_VECTOR_FILE_NAME);
	}
	else
	{
		// The file being verified against.
		FILE *existing_out = fopen(output_vector_file, "r");
		if (!existing_out) error_message(SIMULATION_ERROR, -1, -1, "Could not open vector output file: %s", output_vector_file);

		// Our current output vectors. (Just produced.)
		FILE *current_out  = fopen(OUTPUT_VECTOR_FILE_NAME, "r");
		if (!current_out) error_message(SIMULATION_ERROR, -1, -1, "Could not open output vector file.");

		int cycle;
		char buffer1[BUFFER_MAX_SIZE];
		char buffer2[BUFFER_MAX_SIZE];
		// Start at cycle -1 to check the headers.
		for (cycle = -1; cycle < num_test_vectors; cycle++)
		{
			if (!fgets(buffer1, BUFFER_MAX_SIZE, existing_out))
			{
				error = TRUE;
				warning_message(SIMULATION_ERROR, 0, -1,"Too few vectors in %s \n", output_vector_file);
				break;
			}
			else if (!fgets(buffer2, BUFFER_MAX_SIZE, current_out))
			{
				error = TRUE;
				warning_message(SIMULATION_ERROR, 0, -1,"Simulation produced fewer than %d vectors.\n", num_test_vectors);
				break;
			}
			// The headers differ.
			else if ((cycle == -1) && strcmp(buffer1,buffer2))
			{
				error = TRUE;
				warning_message(SIMULATION_ERROR, 0, -1, "Vector headers do not match: \n"
						"\t%s"
						"in %s does not match\n"
						"\t%s"
						"in %s.\n\n",
						buffer2, OUTPUT_VECTOR_FILE_NAME, buffer1, output_vector_file
				);
				break;
			}
			else
			{
				// Parse both vectors.
				test_vector *v1 = parse_test_vector(buffer1);
				test_vector *v2 = parse_test_vector(buffer2);

				// Compare them and print an appropreate message if they differ.
				if (!compare_test_vectors(v1,v2))
				{
					error = TRUE;
					warning_message(SIMULATION_ERROR, 0, -1, "Cycle %d mismatch: \n"
							"\t%s"
							"in %s does not match\n"
							"\t%s"
							"in %s.\n\n",
							cycle, buffer2, OUTPUT_VECTOR_FILE_NAME, buffer1, output_vector_file
					);
				}
				free_test_vector(v1);
				free_test_vector(v2);
			}
		}

		// If the file we're checking against is longer than the current output, print an appropreate warning.
		if (!error && fgets(buffer1, BUFFER_MAX_SIZE, existing_out))
		{
			warning_message(SIMULATION_ERROR, 0, -1,"%s contains more vectors than %s.\n", output_vector_file, OUTPUT_VECTOR_FILE_NAME);
			error = TRUE;
		}

		fclose(existing_out);
		fclose(current_out);
	}
	return !error;
}

/*
 * Free each element in lines[] and the array itself
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
 * Free stages.
 */
void free_stages(stages *s)
{
	while (s->count--)
		free(s->stages[s->count]);

	free(s->counts);
	free(s);
}

/*
 * Free the given test_vector.
 */
void free_test_vector(test_vector* v)
{
	while (v->count--)
		free(v->values[v->count]);

	free(v->counts);
	free(v);
}

