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
 * Performs simulation. 
 */ 
void simulate_netlist(netlist_t *netlist)
{
	printf("Beginning simulation.\n"); fflush(stdout);

	// Create and verify the lines.
	lines_t *input_lines = create_input_test_vector_lines(netlist);
	if (!verify_lines(input_lines))
		error_message(SIMULATION_ERROR, 0, -1, "Input lines could not be assigned.");

	lines_t *output_lines = create_output_test_vector_lines(netlist);
	if (!verify_lines(output_lines))
		error_message(SIMULATION_ERROR, 0, -1, "Output lines could not be assigned.");

	FILE *out = fopen(OUTPUT_VECTOR_FILE_NAME, "w");
	if (!out)
		error_message(SIMULATION_ERROR, 0, -1, "Could not open output vector file.");

	FILE *modelsim_out = fopen("test.do", "w");
	if (!modelsim_out)
		error_message(SIMULATION_ERROR, 0, -1, "Could not open modelsim output file.");

	FILE *in  = NULL;
	int num_test_vectors;
	// Passed via the -t option.
	char *input_vector_file  = global_args.sim_vector_input_file;

	// Input vectors can either come from a file or be randomly generated.
	if (input_vector_file)
	{
		in = fopen(input_vector_file, "r");
		if (!in)
			error_message(SIMULATION_ERROR, 0, -1, "Could not open vector input file: %s", input_vector_file);

		num_test_vectors = count_test_vectors(in);

		// Read the vector headers and check to make sure they match the lines.
		if (!verify_test_vector_headers(in, input_lines))
			error_message(SIMULATION_ERROR, 0, -1, "Invalid vector header format in %s.", input_vector_file);

		printf("Simulating %d existing vectors from \"%s\".\n", num_test_vectors, input_vector_file); fflush(stdout);
	}
	else
	{
		// Passed via the -g option.
		num_test_vectors = global_args.num_test_vectors;

		in  = fopen( INPUT_VECTOR_FILE_NAME, "w");
		if (!in)
			error_message(SIMULATION_ERROR, 0, -1, "Could not open input vector file.");

		printf("Simulating %d new vectors.\n", num_test_vectors); fflush(stdout);
	}

	if (!num_test_vectors)
	{
		error_message(SIMULATION_ERROR, 0, -1, "No vectors to simulate.");
	}
	else
	{
		printf("\n");

		int       progress_bar_position = -1;
		const int progress_bar_length   = 50;

		double total_time      = 0; // Includes I/O
		double simulation_time = 0; // Does not include I/O

		stages *stages = 0;

		// Simulation is done in "waves" of SIM_WAVE_LENGTH cycles at a time.
		// Every second cycle gets a new vector.
		int  num_cycles = num_test_vectors * 2;
		int  num_waves = ceil(num_cycles / (double)SIM_WAVE_LENGTH);
		int  wave;
		for (wave = 0; wave < num_waves; wave++)
		{
			double wave_start_time = wall_time();

			int cycle_offset = SIM_WAVE_LENGTH * wave;
			int wave_length  = (wave < (num_waves-1))?SIM_WAVE_LENGTH:(num_cycles - cycle_offset);

			// Assign vectors to lines, either by reading or generating them.
			// Every second cycle gets a new vector.
			test_vector *v = 0;
			int cycle;
			for (cycle = cycle_offset; cycle < cycle_offset + wave_length; cycle++)
			{
				if (is_even_cycle(cycle))
				{
					if (input_vector_file)
					{
						char buffer[BUFFER_MAX_SIZE];
						if (!get_next_vector(in, buffer))
							error_message(SIMULATION_ERROR, 0, -1, "Could not read next vector.");

						v = parse_test_vector(buffer);
					}
					else
					{
						v = generate_random_test_vector(input_lines, cycle);
					}
				}

				store_test_vector_in_lines(v, input_lines, cycle);

				if (!is_even_cycle(cycle))
					free_test_vector(v);
			}

			if (!input_vector_file)
				write_wave_to_file(input_lines, in, cycle_offset, wave_length);

			write_wave_to_modelsim_file(netlist, input_lines, modelsim_out, cycle_offset, wave_length);

			double simulation_start_time = wall_time();

			// Perform simulation
			for (cycle = cycle_offset; cycle < cycle_offset + wave_length; cycle++)
			{
				if (!cycle)
				{	// The first cycle produces the stages, and adds additional lines as specified by the -p option.
					additional_pins *p = parse_additional_pins();
					stages = simulate_first_cycle(netlist, cycle, p, output_lines);
					free_additional_pins(p);
					// Make sure the output lines are still OK after adding custom lines.
					if (!verify_lines(output_lines))
						error_message(SIMULATION_ERROR, 0, -1, "Problem detected with the output lines after the first cycle.");
					// Print netlist-specific statistics.
					print_netlist_stats(stages, num_test_vectors);
				}
				else
				{
					simulate_cycle(cycle, stages);
				}
			}

			simulation_time += wall_time() - simulation_start_time;

			// Write the result of this wave to the output vector file.
			write_wave_to_file(output_lines, out, cycle_offset, wave_length);

			total_time += wall_time() - wave_start_time;

			// Delay drawing of the progress bar until the second wave to improve the accuracy of the ETA.
			if ((num_waves == 1) || cycle_offset)
				progress_bar_position = print_progress_bar(cycle/(double)num_cycles, progress_bar_position, progress_bar_length, total_time);
		}

		fflush(out); 

		fprintf(modelsim_out, "run %d\n", (num_test_vectors*100) + 100);

		printf("\n");
		// If a second output vector file was given via the -T option, verify that it matches.
		char *output_vector_file = global_args.sim_vector_output_file;
		if (output_vector_file)
		{
			if (verify_output_vectors(output_vector_file, num_test_vectors))
				printf("Vector file \"%s\" matches output\n", output_vector_file);
			else
				error_message(SIMULATION_ERROR, 0, -1, "Vector files differ.");
			printf("\n");
		}

		// Print statistics.
		print_simulation_stats(stages, num_test_vectors, total_time, simulation_time);

		free_stages(stages);
	}

	free_lines(output_lines);
	free_lines(input_lines);

	fclose(modelsim_out);
	fclose(in);
	fclose(out);
}

/*
 * This simulates a single cycle using the stages generated
 * during the first cycle. Simulates in parallel if OpenMP is enabled.
 */
void simulate_cycle(int cycle, stages *s)
{
	int i;
	for(i = 0; i < s->count; i++)
	{
		int j;
		#ifdef _OPENMP
		if (s->counts[i] < SIM_PARALLEL_THRESHOLD)
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
}

/*
 * Simulates the first cycle by traversing the netlist and returns
 * the nodes organised into parallelizable stages. Also adds lines to
 * custom pins and nodes as requested via the -p option.
 */
stages *simulate_first_cycle(netlist_t *netlist, int cycle, additional_pins *p, lines_t *l)
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

		// Match node for items passed via -p and add to lines if there's a match.
		add_additional_pins_to_lines(node, p, l);

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

	return s;
}

/*
 * Puts the ordered nodes in stages which can be computed in parallel.
 */
stages *stage_ordered_nodes(nnode_t **ordered_nodes, int num_ordered_nodes) {
	stages *s = malloc(sizeof(stages));
	s->stages = calloc(1,sizeof(nnode_t**));
	s->counts = calloc(1,sizeof(int));
	s->count  = 1;
	s->num_connections = 0;
	s->num_nodes = num_ordered_nodes;

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

		// Record the number of children for computing the degree.
		s->num_connections += num_children;

		free(children);
	}
	stage_children->destroy(stage_children);
	stage_nodes   ->destroy(stage_nodes);

	// Record the number of nodes in parallelizable stages (for statistical purposes).
	s->num_parallel_nodes = 0;
	for (i = 0; i < s->count; i++)
		if (s->counts[i] >= SIM_PARALLEL_THRESHOLD)
			s->num_parallel_nodes += s->counts[i];

	return s;
}

/*
 * Given a node, this function will simulate that node's new outputs,
 * and updates those pins.
 */
void compute_and_store_value(nnode_t *node, int cycle)
{
	switch(node->type)
	{
		case FF_NODE:
		{
			oassert(node->num_output_pins == 1);
			oassert(node->num_input_pins  == 2);

			signed char clock_previous = get_pin_value(node->input_pins[1],cycle-1);
			signed char clock_current  = get_pin_value(node->input_pins[1],cycle);

			if (clock_current > clock_previous) // Rising edge.
			{	// Update the flip-flop from the input value of the previous cycle.
				update_pin_value(node->output_pins[0], get_pin_value(node->input_pins[0],cycle-1), cycle);
			}
			else
			{	// Maintain the flip-flop value.
				update_pin_value(node->output_pins[0], get_pin_value(node->output_pins[0],cycle-1), cycle);
			}
			break;
		}
		case LT: // < 010 1
		{
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			signed char pin0 = get_pin_value(node->input_pins[0],cycle);
			signed char pin1 = get_pin_value(node->input_pins[1],cycle);
			signed char pin2 = get_pin_value(node->input_pins[2],cycle);

			if      (pin0  < 0 || pin1  < 0 || pin2  < 0)
				update_pin_value(node->output_pins[0], -1, cycle);
			else if (pin0 == 0 && pin1 == 1 && pin2 == 0)
				update_pin_value(node->output_pins[0],  1, cycle);
			else
				update_pin_value(node->output_pins[0],  0, cycle);

			break;
		}
		case GT: // > 100 1
		{
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			signed char pin0 = get_pin_value(node->input_pins[0],cycle);
			signed char pin1 = get_pin_value(node->input_pins[1],cycle);
			signed char pin2 = get_pin_value(node->input_pins[2],cycle);

			if      (pin0  < 0 || pin1  < 0 || pin2  < 0)
				update_pin_value(node->output_pins[0], -1, cycle);
			else if (pin0 == 1 && pin1 == 0 && pin2 == 0)
				update_pin_value(node->output_pins[0],  1, cycle);
			else
				update_pin_value(node->output_pins[0],  0, cycle);

			break;
		}
		case ADDER_FUNC: // 001 1\n010 1\n100 1\n111 1
		{
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			signed char pin0 = get_pin_value(node->input_pins[0],cycle);
			signed char pin1 = get_pin_value(node->input_pins[1],cycle);
			signed char pin2 = get_pin_value(node->input_pins[2],cycle);

			if (pin0 < 0 || pin1 < 0 || pin2 < 0)
				update_pin_value(node->output_pins[0], -1, cycle);
			else if (
					   (pin0 == 0 && pin1 == 0 && pin2 == 1)
					|| (pin0 == 0 && pin1 == 1 && pin2 == 0)
					|| (pin0 == 1 && pin1 == 0 && pin2 == 0)
					|| (pin0 == 1 && pin1 == 1 && pin2 == 1)
			)
				update_pin_value(node->output_pins[0], 1, cycle);
			else
				update_pin_value(node->output_pins[0], 0, cycle);

			break;
		}
		case CARRY_FUNC: // 011 1\n100 1\n110 1\n111 1
		{
			oassert(node->num_input_port_sizes == 3);
			oassert(node->num_output_port_sizes == 1);

			signed char pin0 = get_pin_value(node->input_pins[0],cycle);
			signed char pin1 = get_pin_value(node->input_pins[1],cycle);
			signed char pin2 = get_pin_value(node->input_pins[2],cycle);

			if (pin0 < 0 || pin1 < 0 || pin2 < 0)
				update_pin_value(node->output_pins[0], -1, cycle);
			else if (
				   (pin0 == 1 && (pin1 == 1 || pin2 == 1))
				|| (pin1 == 1 && pin2 == 1)
			)
				update_pin_value(node->output_pins[0], 1, cycle);
			else
				update_pin_value(node->output_pins[0], 0, cycle);

			break;
		}
		case BITWISE_NOT:
		{
			oassert(node->num_input_pins == 1);
			oassert(node->num_output_pins == 1);

			signed char pin = get_pin_value(node->input_pins[0], cycle);

			if      (pin  < 0)
				update_pin_value(node->output_pins[0], -1, cycle);
			else if (pin == 1)
				update_pin_value(node->output_pins[0],  0, cycle);
			else
				update_pin_value(node->output_pins[0],  1, cycle);

			break;
		}
		case LOGICAL_AND: // &&
		{
			oassert(node->num_output_pins == 1);
			int unknown = FALSE;
			int zero = 0;
			int i;
			for (i = 0; i < node->num_input_pins; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin <  0) { unknown = TRUE; }
				if (pin == 0) { zero    = TRUE; break; }
			}
			if      (zero)    update_pin_value(node->output_pins[0],  0, cycle);
			else if (unknown) update_pin_value(node->output_pins[0], -1, cycle);
			else              update_pin_value(node->output_pins[0],  1, cycle);
			break;
		}
		case LOGICAL_OR:
		{	// ||
			oassert(node->num_output_pins == 1);
			int unknown = FALSE;
			int one = 0;
			int i;
			for (i = 0; i < node->num_input_pins; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin <  0) { unknown = TRUE; }
				if (pin == 1) { one     = TRUE; break; }
			}
			if      (one)     update_pin_value(node->output_pins[0],  1, cycle);
			else if (unknown) update_pin_value(node->output_pins[0], -1, cycle);
			else              update_pin_value(node->output_pins[0],  0, cycle);
			break;
		}
		case LOGICAL_NAND:
		{	// !&&
			oassert(node->num_output_pins == 1);
			int unknown = FALSE;
			int one = 0;
			int i;
			for (i = 0; i < node->num_input_pins; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin <  0) { unknown = TRUE; }
				if (pin == 0) { one     = TRUE; break; }
			}
			if      (one)     update_pin_value(node->output_pins[0],  1, cycle);
			else if (unknown) update_pin_value(node->output_pins[0], -1, cycle);
			else              update_pin_value(node->output_pins[0],  0, cycle);
			break;
		}
		case LOGICAL_NOT: // !
		case LOGICAL_NOR: // !|
		{
			oassert(node->num_output_pins == 1);
			int unknown = FALSE;
			int zero = 0;
			int i;
			for (i = 0; i < node->num_input_pins; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin <  0) { unknown = TRUE; }
				if (pin == 1) { zero    = TRUE; break; }
			}
			if      (zero)    update_pin_value(node->output_pins[0],  0, cycle);
			else if (unknown) update_pin_value(node->output_pins[0], -1, cycle);
			else              update_pin_value(node->output_pins[0],  1, cycle);
			break;
		}
		case NOT_EQUAL:	  // !=
		case LOGICAL_XOR: // ^
		{
			oassert(node->num_output_pins == 1);
			int unknown = FALSE;
			int ones = 0;
			int i;
			for (i = 0; i < node->num_input_pins; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin <  0) { unknown = TRUE; break; }
				if (pin == 1) { ones++; }
			}
			if      (unknown)         update_pin_value(node->output_pins[0], -1, cycle);
			else if ((ones % 2) == 1) update_pin_value(node->output_pins[0],  1, cycle);
			else                      update_pin_value(node->output_pins[0],  0, cycle);
			break;
		}
		case LOGICAL_EQUAL:	// ==
		case LOGICAL_XNOR:  // !^
		{
			oassert(node->num_output_pins == 1);
			int unknown = FALSE;
			int ones = 0;
			int i;
			for (i = 0; i < node->num_input_pins; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin <  0) { unknown = TRUE; break; }
				if (pin == 1) { ones++; }
			}
			if      (unknown)         update_pin_value(node->output_pins[0], -1, cycle);
			else if ((ones % 2) == 1) update_pin_value(node->output_pins[0],  0, cycle);
			else                      update_pin_value(node->output_pins[0],  1, cycle);
			break;
		}
		case MUX_2:
		{
			// May still be incorrect for 3 valued logic.
			// The first port is a bit mask for which bit in the second port should be connected.
			oassert(node->num_output_pins == 1);
			oassert(node->num_input_port_sizes >= 2);
			oassert(node->input_port_sizes[0] == node->input_port_sizes[1]);

			// Figure out which pin is being selected.
			int unknown = FALSE;
			int select = -1;
			int default_select = -1;
			int i;
			for (i = 0; i < node->input_port_sizes[0]; i++)
			{
				signed char pin = get_pin_value(node->input_pins[i], cycle);

				if (pin  < 0)
					unknown = TRUE;
				else if (pin == 1)
					select  = i;

				// If the pin comes from an "else" condition or a case "default" condition,
				// we favour it in the case where there are unknowns.
				if (node->input_pins[i]->is_default)
					default_select = i;
			}

			if (unknown && default_select >= 0)
			{
				unknown = FALSE;
				select = default_select;
			}

			// If any select pin is unknown, we take the value from the previous cycle.
			if (unknown)
			{
				update_pin_value(node->output_pins[0], get_pin_value(node->output_pins[0], cycle-1), cycle);
			}
			// If no selection is made (all 0) we output x.
			else if (select < 0)
			{
				update_pin_value(node->output_pins[0], -1, cycle);
			}
			else
			{
				signed char pin = get_pin_value(node->input_pins[select + node->input_port_sizes[0]],cycle);
				update_pin_value(node->output_pins[0], pin, cycle);
			}
			break;
		}
		case INPUT_NODE:
			break;
		case OUTPUT_NODE:
			oassert(node->num_output_pins == 1);
			oassert(node->num_input_pins  == 1);
			update_pin_value(node->output_pins[0], get_pin_value(node->input_pins[0],cycle), cycle);
			break;
		case PAD_NODE:
			oassert(node->num_output_pins == 1);
			update_pin_value(node->output_pins[0], 0, cycle);
			break;
		case CLOCK_NODE:
			oassert(node->num_output_pins == 1);
			update_pin_value(node->output_pins[0], is_even_cycle(cycle)?0:1, cycle);
			break;
		case GND_NODE:
			oassert(node->num_output_pins == 1);
			update_pin_value(node->output_pins[0], 0, cycle);
			break;
		case VCC_NODE:
			oassert(node->num_output_pins == 1);
			update_pin_value(node->output_pins[0], 1, cycle);
			break;
		case MEMORY:
			compute_memory_node(node,cycle);
			break;
		case HARD_IP:
			oassert(node->input_port_sizes[0] > 0);
			oassert(node->output_port_sizes[0] > 0);
			compute_hard_ip_node(node,cycle);
			break;
		case MULTIPLY:
			oassert(node->num_input_port_sizes >= 2);
			oassert(node->num_output_port_sizes == 1);
			compute_multiply_node(node,cycle);
			break;
		case GENERIC :
			compute_generic_node(node,cycle);
			break;
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
			error_message(SIMULATION_ERROR, 0, -1, "Node should have been converted to softer version: %s", node->name);
			break;
	}

	// Record coverage on any output pins that have changed.
	{
		int i;
		for (i = 0; i < node->num_output_pins; i++)
			if(get_pin_value(node->output_pins[i],cycle-1) != get_pin_value(node->output_pins[i],cycle))
				node->output_pins[i]->coverage++;
	}
}

/*
 * Gets the number of nodes whose output pins have been sufficiently covered.
 */
int get_num_covered_nodes(stages *s)
{
	int covered_nodes = 0;

	int i;
	for(i = 0; i < s->count; i++)
	{
		int j;
		for (j = 0; j < s->counts[i]; j++)
		{	/*
			 * To count as being covered, every pin should resolve, and
			 * make at least one transition from one binary value to another
			 * and back. (That's three transitions total.)
			 */
			nnode_t *node = s->stages[i][j];
			int k;
			int covered = TRUE;
			for (k = 0; k < node->num_output_pins; k++)
			{
				if (node->output_pins[k]->coverage < 3)
				{
					covered = FALSE;
					break;
				}
			}

			if (covered)
				covered_nodes++;
		}
	}
	return covered_nodes;
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
int is_node_complete(nnode_t* node, int cycle)
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
int is_node_ready(nnode_t* node, int cycle)
{
	if (node->type == FF_NODE)
	{	// Flip-flops depend on the input from the previous cycle and the clock from this cycle.
		if
		(
			   (node->input_pins[0]->cycle < cycle-1)
			|| (node->input_pins[1]->cycle < cycle  )
		)
			return FALSE;
	}
	else
	{
		int i;
		for (i = 0; i < node->num_input_pins; i++)
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
 * Updates the value of a pin and its cycle. Pins should be updated using
 * only this function.
 */
void update_pin_value(npin_t *pin, signed char value, int cycle)
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
signed char get_pin_value(npin_t *pin, int cycle)
{
	if (cycle < 0) return -1;

	return pin->values[get_values_offset(cycle)];
}

/*
 * Sets the pin to the given value for the given cycle. Does not
 * propagate the value to the connected net.
 *
 * CAUTION: Use update_pin_value to update pins. This function will not update
 *          the connected net.
 */
inline void set_pin(npin_t *pin, signed char value, int cycle)
{
	pin->values[get_values_offset(cycle)] = value;
	pin->cycle = cycle;
}

/*
 * Calculates the index in the values array for the given cycle.
 */
inline int get_values_offset(int cycle)
{
	return (((cycle) + (SIM_WAVE_LENGTH+1)) % (SIM_WAVE_LENGTH+1));
}

/*
 * Returns FALSE if the cycle is odd.
 */
int is_even_cycle(int cycle)
{
	return !((cycle + 2) % 2);
}

/*
 * Computes the given memory node.
 */
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
		int posedge = 0;
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
				int current_clock = get_pin_value(node->input_pins[i],cycle);
				int previous_clock = get_pin_value(node->input_pins[i],cycle-1);
				posedge = (current_clock > previous_clock) ? 1 : 0;
			}
		}
		out = node->output_pins;

		if (node->type == MEMORY && !node->memory_data)
			instantiate_memory(node, &(node->memory_data), data_width, addr_width);

		compute_memory(data, out, data_width, addr, addr_width, we, posedge, cycle, node->memory_data);
	}
	else
	{
		int posedge = 0;

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
				int current_clock = get_pin_value(node->input_pins[i],cycle);
				int previous_clock = get_pin_value(node->input_pins[i],cycle-1);
				posedge = (current_clock > previous_clock) ? 1 : 0;
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

		compute_memory(data1, out1, data_width1, addr1, addr_width1, we1, posedge, cycle, node->memory_data);
		compute_memory(data2, out2, data_width2, addr2, addr_width2, we2, posedge, cycle, node->memory_data);
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

		if (!index(node->name, '.')) error_message(SIMULATION_ERROR, 0, -1, "Couldn't extract the name of a shared library for hard-block simulation");

		snprintf(filename, sizeof(char)*strlen(node->name), "%s.so", index(node->name, '.')+1);

		void *handle = dlopen(filename, RTLD_LAZY);

		if (!handle) error_message(SIMULATION_ERROR, 0, -1, "Couldn't open a shared library for hard-block simulation: %s", dlerror());

		dlerror();

		void (*func_pointer)(int, int, int*, int, int*) = (void(*)(int, int, int*, int, int*))dlsym(handle, "simulate_block_cycle");

		char *error = dlerror();
		if (error) error_message(SIMULATION_ERROR, 0, -1, "Couldn't load a shared library method for hard-block simulation: %s", error);

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

void compute_multiply_node(nnode_t *node, int cycle)
{
	int i;
	int unknown = FALSE;
	for (i = 0; i < node->input_port_sizes[0] + node->input_port_sizes[1]; i++)
	{
		signed char pin = get_pin_value(node->input_pins[i],cycle);
		if (pin < 0) { unknown = TRUE; break; }
	}

	if (unknown)
	{
		for (i = 0; i < node->num_output_pins; i++)
			update_pin_value(node->output_pins[i], -1, cycle);
	}
	else
	{
		int *a = malloc(sizeof(int)*node->input_port_sizes[0]);
		int *b = malloc(sizeof(int)*node->input_port_sizes[1]);

		for (i = 0; i < node->input_port_sizes[0]; i++)
			a[i] = get_pin_value(node->input_pins[i],cycle);

		for (i = 0; i < node->input_port_sizes[1]; i++)
			b[i] = get_pin_value(node->input_pins[node->input_port_sizes[0] + i],cycle);

		int *result = multiply_arrays(a, node->input_port_sizes[0], b, node->input_port_sizes[1]);

		for (i = 0; i < node->num_output_pins; i++)
			update_pin_value(node->output_pins[i], result[i], cycle);

		free(result);
		free(a);
		free(b);
	}

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
void compute_memory(
	npin_t **inputs,
	npin_t **outputs,
	int data_width,
	npin_t **addr,
	int addr_width,
	int write_enable,
	int posedge,
	int cycle,
	signed char *data
)
{
	long address = 0;
	int i;
	for (i = 0; i < addr_width; i++)
	{	// If any address pins are x's, write x's to the output and return. 
		if (get_pin_value(addr[i],cycle) < 0)
		{
			int j;
			for (j = 0; j < data_width; j++)
				update_pin_value(outputs[j], -1, cycle);

			return;
		}
		address += get_pin_value(addr[i],cycle) << (i);
	}

	// Read (and write if we) data to memory.
	for (i = 0; i < data_width; i++)
	{	// Compute which bit we are addressing.
		long long bit_address = i + (address * data_width);

		// Update the output.
		if (!posedge) update_pin_value(outputs[i], data[bit_address], cycle);
		else          update_pin_value(outputs[i], get_pin_value(outputs[i],cycle-1), cycle);

		// If write is enabled, copy the input to memory.
		if (write_enable && !posedge)
			data[bit_address] = get_pin_value(inputs[i],cycle);
	}
}

/*
 * Initialises memory using a memory information file (mif). If not
 * file is found, it is initialised to x's.
 */
// TODO: This obviously won't work with mif files, as it doesn't even write the values to the memory.
void instantiate_memory(nnode_t *node, signed char **memory, int data_width, int addr_width)
{
	char *filename = node->name;
	char *input = (char *)malloc(sizeof(char)*BUFFER_MAX_SIZE);
	long long int max_address = my_power(2, addr_width);

	*memory = malloc(sizeof(signed char)*max_address*data_width);
	// Initialise the memory to -1.
	int i;
	for (i = 0; i < max_address * data_width; i++)
		(*memory)[i] = -1;

	filename = strrchr(filename, '+') + 1;
	strcat(filename, ".mif");
	if (!filename)
		error_message(SIMULATION_ERROR, 0, -1, "Couldn't parse node name");

	FILE *mif = fopen(filename, "r");
	if (!mif)
	{
		warning_message(SIMULATION_ERROR, 0, -1, "Couldn't open MIF file %s",filename);
		return;
	}

	error_message(SIMULATION_ERROR, 0, -1, "MIF file support is current broken and needs developer attention.");

	while (fgets(input, BUFFER_MAX_SIZE, mif))
		if (strcmp(input, "Content\n") == 0)
			break;

	while (fgets(input, BUFFER_MAX_SIZE, mif))
	{
		char *addr = malloc(sizeof(char)*BUFFER_MAX_SIZE);
		char *data = malloc(sizeof(char)*BUFFER_MAX_SIZE);

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
				signed char val = (mask & data_val) > 0 ? 1 : 0;
				int write_address = i + (addr_val * data_width);
				// TODO: This is obviously incorrect, as it's writing the value to a small char buffer which isn't connected to the memory.
				data[write_address] = val;
			}
		}
	}
	fclose(mif);
}

/*
 * Searches for a line with the given name in the lines. Returns the index
 * or -1 if no such line was found.
 */
int find_portname_in_lines(char* port_name, lines_t *l)
{
	int j;
	for (j = 0; j < l->count; j++)	
		if (!strcmp(l->lines[j]->name, port_name))		
			return  j;
	
	return -1;
}

/*
 * Assigns the given node to its corresponding line in the given array of line.
 * Assumes the line has already been created.
 */
void assign_node_to_line(nnode_t *node, lines_t *l, int type, int single_pin)
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

	int j = find_portname_in_lines(port_name, l);

	if (!tilde)
	{	// Treat the pin as a lone single pin.
		if (j == -1)
		{
			if (!(node->type == GND_NODE || node->type == VCC_NODE || node->type == PAD_NODE || node->type == CLOCK_NODE))
				warning_message(SIMULATION_ERROR, 0, -1, "Could not map single-bit top-level input node '%s' to input vector", node->name);
		} 
		else
		{
			insert_pin_into_line(node->output_pins[0], 0, l->lines[j], type);
		}		
	}
	else
	{	// Treat the pin as part of a multi-pin port.
		if (j == -1)
		{
			warning_message(SIMULATION_ERROR, 0, -1, "Could not map multi-bit top-level input node '%s' to input vector", node->name);
		} 
		else
		{
			insert_pin_into_line(node->output_pins[0], pin_number, l->lines[j], type);
		}
	}
	free(port_name);
}

/*
 * Inserts the given pin according to its pin number into the given line.
 */
void insert_pin_into_line(npin_t *pin, int pin_number, line_t *line, int type)
{
	line->pins        = realloc(line->pins,        sizeof(npin_t*)* (line->number_of_pins + 1));
	line->pin_numbers = realloc(line->pin_numbers, sizeof(npin_t*)* (line->number_of_pins + 1));

	int ascending = 1;

	// Find the proper place to insert this pin, and make room for it.
	int i;
	for (i = 0; i < line->number_of_pins; i++)
	{
		if
		(
				    (ascending && (line->pin_numbers[i] > pin_number))
				|| (!ascending && (line->pin_numbers[i] < pin_number))
		)
		{
			// Move other pins to the right to make room.
			int j;
			for (j = line->number_of_pins; j > i; j--)
			{
				line->pins[j] = line->pins[j-1];
				line->pin_numbers[j] = line->pin_numbers[j-1];
			}
			break;
		}
	}

	line->pins[i] = pin;
	line->pin_numbers[i] = pin_number;
	line->type = type;
	line->number_of_pins++;
}


/*
 * Given a netlist, this function maps the top_input_nodes
 * to a line_t* each. It stores them in a lines_t struct.
 */
lines_t *create_input_test_vector_lines(netlist_t *netlist)
{
	lines_t *l = malloc(sizeof(lines_t));
	l->lines = 0;
	l->count = 0;
	int i; 	
	for (i = 0; i < netlist->num_top_input_nodes; i++)
	{
		nnode_t *node = netlist->top_input_nodes[i];
		char *port_name = strdup(strchr(node->name, '^') + 1);
		char *tilde = strchr(port_name, '~');

		if (tilde) *tilde = '\0';

		if (node->type != CLOCK_NODE)
		{
			if (find_portname_in_lines(port_name, l) == -1)
			{
				line_t *line = create_line(port_name);
				l->lines = realloc(l->lines, sizeof(line_t *)*(l->count + 1));
				l->lines[l->count++] = line;
			}
			assign_node_to_line(node, l, INPUT, 0);
		}
		free(port_name);
	}
	return l;
}

/*
 * Given a netlist, this function maps the top_output_nodes
 * to a line_t* each. It stores them in a lines_t struct.
 */
lines_t *create_output_test_vector_lines(netlist_t *netlist)
{
	lines_t *l = malloc(sizeof(lines_t));
	l->lines = 0;
	l->count = 0;
	int i; 
	for (i = 0; i < netlist->num_top_output_nodes; i++)
	{
		nnode_t *node = netlist->top_output_nodes[i];
		char *port_name = strdup(strchr(node->name, '^') + 1);
		char *tilde = strchr(port_name, '~');

		if (tilde) *tilde = '\0';

		if (find_portname_in_lines(port_name, l) == -1)
		{
			line_t *line = create_line(port_name);
			l->lines = realloc(l->lines, sizeof(line_t *)*(l->count + 1));
			l->lines[l->count++] = line;
		}
		assign_node_to_line(node, l, OUTPUT, 0);
		free(port_name);
	}
	return l;
}

/*
 * Creates a vector file header from the given lines,
 * and writes it to the given file.
 */
void write_vector_headers(FILE *file, lines_t *l)
{
	char* headers = generate_vector_header(l);
	fprintf(file, "%s", headers);
	free(headers);
	fflush(file);
}

/*
 * Parses the first line of the given file and compares it to the
 * given lines for identity. If there is any difference, a warning is printed,
 * and FALSE is returned. If there are no differences, the file pointer is left
 * at the start of the second line, and TRUE is returned.
 */
int verify_test_vector_headers(FILE *in, lines_t *l)
{
	rewind(in);

	int current_line = 0;
	int buffer_length = 0;

	char read_buffer [BUFFER_MAX_SIZE];
	if (!get_next_vector(in, read_buffer))
		error_message(SIMULATION_ERROR, 0, -1, "Failed to read vector headers.");

	char buffer [BUFFER_MAX_SIZE];
	buffer[0] = '\0';
	int i;
	for (i = 0; i < strlen(read_buffer) && i < BUFFER_MAX_SIZE; i++)
	{
		char next = read_buffer[i];

		if (next == EOF)
		{
			warning_message(SIMULATION_ERROR, 0, -1, "Hit end of file.");
			return FALSE;
		}
		else if (next == ' ' || next == '\t' || next == '\n')
		{
			if (buffer_length)
			{
				if(strcmp(l->lines[current_line]->name,buffer))
				{
					char *expected_header = generate_vector_header(l);
					warning_message(SIMULATION_ERROR, 0, -1, "Vector header mismatch: \n "
							"\tFound:    %s "
							"\tExpected: %s", read_buffer, expected_header);
					free(expected_header);
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
	}
	return TRUE;
}

/* 
 * Verifies that no lines have null pins.
 */
int verify_lines (lines_t *l)
{
	int i; 
	for (i = 0; i < l->count; i++)
	{
		int j;
		for (j = 0; j < l->lines[i]->number_of_pins; j++)
		{
			if (!l->lines[i]->pins[j])
			{
				warning_message(SIMULATION_ERROR, 0, -1, "A line %d:(%s) has a NULL pin. "
					"This can be caused when registers are declared "
					"as reg [X:Y], where Y is greater than zero.", j, l->lines[i]->name);
				return FALSE; 
			}
		}
	}
	return TRUE; 
}

/*
 * allocates memory for and initialises a line_t struct
 */
line_t *create_line(char *name)
{
	line_t *line = malloc(sizeof(line_t));

	line->number_of_pins = 0;
	line->pins = 0;
	line->pin_numbers = 0;
	line->type = -1;
	line->name = malloc(sizeof(char)*(strlen(name)+1));

	strcpy(line->name, name);

	return line;
}

/*
 * Generates the appropriate vector headers based on the given lines.
 */
char *generate_vector_header(lines_t *l)
{
	char *header = calloc(BUFFER_MAX_SIZE, sizeof(char *));
	if (l->count)
	{
		int j;
		for (j = 0; j < l->count; j++)
		{
			strcat(header,l->lines[j]->name);
			strcat(header," ");
		}
		header[strlen(header)-1] = '\n';
	}
	else
	{
		header[0] = '\n';
	}
	return header;
}

/*
 * Stores the given test vector in the given lines, with some sanity checking to ensure that it
 * has a compatible geometry.
 */
void store_test_vector_in_lines(test_vector *v, lines_t *l, int cycle)
{
	if (l->count < v->count) 
		error_message(SIMULATION_ERROR, 0, -1, "Fewer lines (%d) than values (%d).", l->count, v->count);
	if (l->count > v->count) 
		error_message(SIMULATION_ERROR, 0, -1, "More lines (%d) than values (%d).", l->count, v->count);

	int i;
	for (i = 0; i < v->count; i++)
	{
		line_t *line = l->lines[i];

		if (line->number_of_pins < 1)
		{
			warning_message(SIMULATION_ERROR, 0, -1, "Found a line '%s' with no pins.", line->name);
		}
		else
		{
			int j;
			for (j = 0; j < v->counts[i] && j < line->number_of_pins; j++)
				update_pin_value(line->pins[j], v->values[i][j], cycle);

			for (; j < line->number_of_pins; j++)
				update_pin_value(line->pins[j], 0, cycle);
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
	{	// Compare bit by bit.
		int i;
		for (i = 0; i < v1->counts[l] && i < v2->counts[l]; i++)
			if (v1->values[l][i] != v2->values[l][i])
				return FALSE;

		/*
		 *  If one value has more bits than the other, they are still
		 *  equivalent as long as the higher order bits of the longer
		 *  one are zero.
		 */
		if (v1->counts[l] != v2->counts[l])
		{
			test_vector *v = v1->counts[l] < v2->counts[l] ? v2 : v1;
			int j;
			for (j = i; j < v->counts[l]; j++)
				if (v->values[l][j] != 0) return FALSE;
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
	v->count  = 0;

	string_trim(buffer,"\r\n");

	const char *delim = " \t";
	char *token = strtok(buffer, delim);
	while (token)
	{
		v->values = realloc(v->values, sizeof(signed char *) * (v->count + 1));
		v->counts = realloc(v->counts, sizeof(int) * (v->count + 1));
		v->values[v->count] = 0;
		v->counts[v->count] = 0;

		if (token[0] == '0' && (token[1] == 'x' || token[1] == 'X'))
		{	// Value is hex.
			token += 2;
			int token_length = strlen(token);
			string_reverse(token, token_length);

			int i;
			for (i = 0; i < token_length; i++)
			{
				char temp[] = {token[i],'\0'};

				int value = strtol(temp, NULL, 16);
				int k;
				for (k = 0; k < 4; k++)
				{
						signed char bit = value % 2;
						value /= 2;
						v->values[v->count] = realloc(v->values[v->count], sizeof(signed char) * (v->counts[v->count] + 1));
						v->values[v->count][v->counts[v->count]++] = bit;
				}
			}
		}
		else
		{	// Value is binary.
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
test_vector *generate_random_test_vector(lines_t *l, int cycle)
{
	test_vector *v = malloc(sizeof(test_vector));
	v->values = 0;
	v->counts = 0;
	v->count = 0;

	int i;
	for (i = 0; i < l->count; i++)
	{
		v->values = realloc(v->values, sizeof(signed char *) * (v->count + 1));
		v->counts = realloc(v->counts, sizeof(int) * (v->count + 1));
		v->values[v->count] = 0;
		v->counts[v->count] = 0;

		int j;
		for (j = 0; j < l->lines[i]->number_of_pins; j++)
		{
			v->values[v->count] = realloc(v->values[v->count], sizeof(signed char) * (v->counts[v->count] + 1));		
			v->values[v->count][v->counts[v->count]++] = (rand() % 2);
			
			// Generate random three valued logic. 
			//v->values[v->count][v->counts[v->count]++] = (rand() % 3) - 1;
		}
		v->count++;
	}
	return v;
}

/*
 * Writes a wave of vectors to the given file. Writes the headers
 * prior to cycle 0.
 */ 
void write_wave_to_file(lines_t *l, FILE* file, int cycle_offset, int wave_length)
{
	if (!cycle_offset)
		write_vector_headers(file, l);

	int cycle;
	for (cycle = cycle_offset + 1; cycle < (cycle_offset + wave_length); cycle += 2)
		write_vector_to_file(l, file, cycle);
}

/*
 * Writes all values in the given lines to a line in the given file
 * for the given cycle.
 */
void write_vector_to_file(lines_t *l, FILE *file, int cycle)
{
	int first = TRUE;
	int i; 
	for (i = 0; i < l->count; i++)
	{
		char buffer[BUFFER_MAX_SIZE];

		if (first) first = FALSE;
		else       fprintf(file, " ");

		int num_pins = l->lines[i]->number_of_pins;

		if (line_has_unknown_pin(l->lines[i], cycle) || num_pins == 1)
		{
			buffer[0] = 0;

			int j;
			int known_values = 0;
			for (j = num_pins - 1; j >= 0 ; j--)
			{
				signed char value = get_line_pin_value(l->lines[i],j,cycle);

				if (value < 0)
				{
					strcat(buffer, "x");
				}
				else
				{	known_values++;
					sprintf(buffer, "%s%d", buffer, value);
				}
			}
			// If there are no known values, print a single capital X.
			// (Only for testing. Breaks machine readability.)
			if (!known_values && num_pins > 1)
				sprintf(buffer, "X");
		}
		else
		{	
			sprintf(buffer, "0X");

			int value = 0;				
			int j;
			for (j = num_pins - 1; j >= 0; j--)
			{
				signed char pin = get_line_pin_value(l->lines[i],j,cycle);
				value += pin << j % 4;
				
				if (!(j % 4))
				{
					sprintf(buffer, "%s%X", buffer, value);
					value = 0;
				}
			}
		}

		// Expand the value to fill to space under the header. (Gets ugly sometimes.)
		//while (strlen(buffer) < strlen(l->lines[i]->name))
		//	strcat(buffer," ");

		fprintf(file,"%s",buffer);
	}
	fprintf(file, "\n");
}

/*
 * Writes a wave of vectors to the given modelsim out file.
 */
void write_wave_to_modelsim_file(netlist_t *netlist, lines_t *l, FILE* modelsim_out, int cycle_offset, int wave_length)
{
	if (!cycle_offset)
	{
		fprintf(modelsim_out, "add wave *\n");

		// Add clocks to the output file.
		int i;
		for (i = 0; i < netlist->num_top_input_nodes; i++)
		{
			nnode_t *node = netlist->top_input_nodes[i];
			if (node->type == CLOCK_NODE)
			{
				char *port_name = strdup(strchr(node->name, '^') + 1);
				fprintf(modelsim_out, "force %s 1 0, 0 50 -repeat 100\n", port_name);
			}
		}
	}

	int cycle;
	for (cycle = cycle_offset; cycle < (cycle_offset + wave_length); cycle += 2)
		write_vector_to_modelsim_file(l, modelsim_out, cycle);
}

/*
 * Writes a vector to the given modelsim out file.
 */
void write_vector_to_modelsim_file(lines_t *l, FILE *modelsim_out, int cycle)
{
	int i;
	for (i = 0; i < l->count; i++)
	{
		if (line_has_unknown_pin(l->lines[i], cycle) || l->lines[i]->number_of_pins == 1)
		{
			fprintf(modelsim_out, "force %s ",l->lines[i]->name);
			int j;

			for (j = l->lines[i]->number_of_pins - 1; j >= 0 ; j--)
			{
				int value = get_line_pin_value(l->lines[i],j,cycle);

				if (value < 0)  fprintf(modelsim_out, "%s", "x");
				else 		fprintf(modelsim_out, "%d", value);
			}
			fprintf(modelsim_out, " %d\n", cycle/2 * 100);
		}
		else
		{
			int value = 0;
			fprintf(modelsim_out, "force %s 16#", l->lines[i]->name);

			int j;
			for (j = l->lines[i]->number_of_pins - 1; j >= 0; j--)
			{
				if (get_line_pin_value(l->lines[i],j,cycle) > 0)
					value += my_power(2, j % 4);

				if (j % 4 == 0)
				{
					fprintf(modelsim_out, "%X", value);
					value = 0;
				}
			}
			fprintf(modelsim_out, " %d\n", cycle/2 * 100);
		}

	}
}

/*
 * Verify that the given output vector file is identical (numerically)
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
		if (!existing_out) error_message(SIMULATION_ERROR, 0, -1, "Could not open vector output file: %s", output_vector_file);

		// Our current output vectors. (Just produced.)
		FILE *current_out  = fopen(OUTPUT_VECTOR_FILE_NAME, "r");
		if (!current_out) error_message(SIMULATION_ERROR, 0, -1, "Could not open output vector file.");

		int cycle;
		char buffer1[BUFFER_MAX_SIZE];
		char buffer2[BUFFER_MAX_SIZE];
		// Start at cycle -1 to check the headers.
		for (cycle = -1; cycle < num_test_vectors; cycle++)
		{
			if (!get_next_vector(existing_out, buffer1))
			{
				error = TRUE;
				warning_message(SIMULATION_ERROR, 0, -1,"Too few vectors in %s \n", output_vector_file);
				break;
			}
			else if (!get_next_vector(current_out, buffer2))
			{
				error = TRUE;
				warning_message(SIMULATION_ERROR, 0, -1,"Simulation produced fewer than %d vectors. \n", num_test_vectors);
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

				// Compare them and print an appropriate message if they differ.
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

		// If the file we're checking against is longer than the current output, print an appropriate warning.
		if (!error && get_next_vector(existing_out, buffer1))
		{
			error = TRUE;
			warning_message(SIMULATION_ERROR, 0, -1,"%s contains more than %d vectors.\n", output_vector_file, num_test_vectors);
		}

		fclose(existing_out);
		fclose(current_out);
	}
	return !error;
}

/*
 * Parses a comma separated list of additional pins
 * passed via -p into an additional_pins struct.
 */
additional_pins *parse_additional_pins()
{
	additional_pins *p = malloc(sizeof(additional_pins));
	p->pins  = 0;
	p->count = 0;

	// Parse the list of additional pins passed via the -p option.
	if (global_args.sim_additional_pins)
	{
		char *pin_list = strdup(global_args.sim_additional_pins);
		char *token    = strtok(pin_list, ",");
		while (token)
		{
			p->pins = realloc(p->pins, sizeof(char *) * (p->count + 1));
			p->pins[p->count++] = strdup(token);
			token = strtok(NULL, ",");
		}
		free(pin_list);
	}
	return p;
}

/*
 * If the given node matches one of the additional_pins (passed via -p),
 * it's added to the lines. (Matches on output pin names, and node name).
 */
void add_additional_pins_to_lines(nnode_t *node, additional_pins *p, lines_t *l)
{
	if (p->count)
	{
		int add = FALSE;
		int j, k = 0;

		// Search the output pin names for each user-defined item.
		for (j = 0; j < node->num_output_pins; j++)
		{
			if (node->output_pins[j]->name)
			{
				for (k = 0; k < p->count; k++)
				{
					if (strstr(node->output_pins[j]->name, p->pins[k]))
					{
						add = TRUE;
						break;
					}
				}
				if (add) break;
			}
		}

		// Search the node name for each user defined item.
		if (!add && node->name && strlen(node->name) && strchr(node->name, '^'))
		{
			for (k = 0; k < p->count; k++)
			{
				if (strstr(node->name, p->pins[k]))
				{
					add = TRUE;
					break;
				}
			}
		}

		if (add)
		{
			int single_pin = strchr(p->pins[k], '~')?1:0;

			if (strchr(node->name, '^'))
			{
				char *port_name = strdup(strchr(node->name, '^') + 1);

				char *tilde = strchr(port_name, '~');
				if (tilde && !single_pin)
					*tilde = '\0';

				if (find_portname_in_lines(port_name, l) == -1)
				{
					line_t *line = create_line(port_name);
					l->lines = realloc(l->lines, sizeof(line_t *)*((l->count)+1));
					l->lines[l->count++] = line;
				}
				assign_node_to_line(node, l, OUTPUT, single_pin);
				free(port_name);
			}
		}
	}
}

/*
 * Trims characters in the given "chars" string
 * from the end of the given string.
 */
void string_trim(char* string, char *chars)
{
	int length;
	while((length = strlen(string)))
	{	int trimmed = FALSE;
		int i;
		for (i = 0; i < strlen(chars); i++)
		{
			if (string[length-1] == chars[i])
			{
				trimmed = TRUE;
				string[length-1] = '\0';
				break;
			}
		}

		if (!trimmed)
			break;
	}
}

/*
 * Returns TRUE if the given line has a pin for
 * the given cycle whose value is -1.
 */
int line_has_unknown_pin(line_t *line, int cycle)
{
	int unknown = FALSE;
	int j;
	for (j = line->number_of_pins - 1; j >= 0; j--)
	{
		if (get_line_pin_value(line, j, cycle) < 0)
		{
			unknown = TRUE;
			break;
		}
	}
	return unknown;
}

/*
 * Gets the value of the pin given pin within the given line
 * for the given cycle.
 */
signed char get_line_pin_value(line_t *line, int pin_num, int cycle)
{
	return get_pin_value(line->pins[pin_num],cycle);
}

/*
 * Returns a value from a test_vectors struct in hex. Works
 * for the values arrays in pins as well.
 */
char *vector_value_to_hex(signed char *value, int length)
{
	char *tmp;
	char *string = malloc(sizeof(char) * (length + 1));
	int j;
	for (j = 0; j < length+1; j++)
	{
		string[j] = value[j] + '0';
		string[j+1] = '\0';
	}

	string_reverse(string,strlen(string));

	char *hex_string = malloc(sizeof(char) * ((length/4 + 1) + 1));

	sprintf(hex_string, "%X ", (unsigned int)strtol(string, &tmp, 2));

	free(string);

	return hex_string;
}

/*
 * Reverses the given string.
 */
void string_reverse(char *string, int length)
{
	int i = 0;
	int j = length - 1;
	while(i < j)
	{
		char temp = string[i];
		string[i] = string [j];
		string[j] = temp;
		i++;
		j--;
	}
}

/*
 * Counts the number of vectors in the given file. 
 */
int count_test_vectors(FILE *in)
{
	rewind(in);

	int count = 0;
	char buffer[BUFFER_MAX_SIZE];
	while (get_next_vector(in, buffer))
		count++;

	if (count) // Don't count the headers.
		count--;

	rewind(in);

	return count;
}

/*
 * A given line is a vector if it contains one or more
 * non-whitespace characters and does not being with a #.
 */
int is_vector(char *buffer)
{
	char *line = strdup(buffer);
	string_trim(line," \t\r\n");

	if (line[0] != '#' && strlen(line))
	{
		free(line);
		return TRUE;
	}
	else
	{
		free(line);
		return FALSE;
	}
}

/*
 * Gets the next line from the given file that
 * passes the is_vector() test and places it in
 * the buffer. Returns TRUE if a vector was found,
 * and FALSE if no vector was found.
 */
int get_next_vector(FILE *file, char *buffer)
{
	while (fgets(buffer, BUFFER_MAX_SIZE, file))
		if (is_vector(buffer))
			return TRUE;
	
	return FALSE;
}

/*
 * Free each element in lines[] and the array itself
 */
void free_lines(lines_t *l)
{
	int i;
	for (i = 0; i < l->count; i++)
	{
		free(l->lines[i]->name);
		free(l->lines[i]->pins);
		free(l->lines[i]);
	}

	free(l->lines);
	free(l);
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

/*
 * Frees additional_pins struct.
 */
void free_additional_pins(additional_pins *p)
{
	while (p->count--)
		free(p->pins[p->count]);

	free(p->pins);
	free(p);
}

/*
 * Prints/updates an ASCII progress bar of length "length" to position length * completion
 * from previous position "position". Updates ETA based on the elapsed time "time".
 * Returns the new position. If the position is unchanged the bar is not redrawn.
 *
 * Call with position = -1 to draw for the first time. Returns the new
 * position, calculated based on completion.
 */
int print_progress_bar(double completion, int position, int length, double time)
{
	if (position == -1 || ((int)(completion * length)) > position)
	{
		printf("%3.0f%%|", completion * (double)100);

		position = completion * length;

		int i;
		for (i = 0; i < position; i++)
			printf("=");

		printf(">");

		for (; i < length; i++)
			printf("-");

		printf("| Remaining: ");

		double remaining_time = time/(double)completion - time;
		print_time(remaining_time);

		printf("    \r");

		if (position == length)
			printf("\n");

		fflush(stdout);
	}
	return position;
}

/*
 * Prints information about the netlist we are simulating.
 */
void print_netlist_stats(stages *stages, int num_vectors)
{
	printf("%s:\n", get_circuit_filename());

	printf("  Nodes:           %d\n",    stages->num_nodes);
	printf("  Connections:     %d\n",    stages->num_connections);
	printf("  Degree:          %3.2f\n", stages->num_connections/(float)stages->num_nodes);
	printf("  Stages:          %d\n",    stages->count);
	#ifdef _OPENMP
	printf("  Parallel nodes:  %d (%4.1f%%)\n", stages->num_parallel_nodes, (stages->num_parallel_nodes/(double)stages->num_nodes) * 100);
	#endif
	printf("\n");

}

/*
 * Prints statistics. (Coverage and times.)
 */
void print_simulation_stats(stages *stages, int num_vectors, double total_time, double simulation_time)
{
	int covered_nodes = get_num_covered_nodes(stages);
	printf("Simulation time:   ");
	print_time(simulation_time);
	printf("\n");
	printf("Elapsed time:      ");
	print_time(total_time);
	printf("\n");
	printf("Coverage:          "
			"%d (%4.1f%%)\n", covered_nodes, (covered_nodes/(double)stages->num_nodes) * 100);
}

/*
 * Prints the time in appropriate units.
 */
void print_time(double time)
{
	if      (time > 24*3600) printf("%.1fd",  time/(24*3600.0));
	else if (time > 3600)    printf("%.1fh",  time/3600.0);
	else if (time > 60)      printf("%.1fm",  time/60.0);
	else if (time > 1)       printf("%.1fs",  time);
	else                     printf("%.1fms", time*1000);
}

/*
 * Gets the current time in seconds.
 */
double wall_time()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (1000000*tv.tv_sec+tv.tv_usec)/1.0e6;
}

/*
 * Gets the name of the file we are simulating as passed by the -b or -V option.
 */
char *get_circuit_filename()
{
	return global_args.verilog_file?global_args.verilog_file:global_args.blif_file;
}
