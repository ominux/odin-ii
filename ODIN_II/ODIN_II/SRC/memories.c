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
#include "globals.h"
#include "errors.h"
#include "netlist_utils.h"
#include "node_creation_library.h"
#include "hard_blocks.h"
#include "memories.h"
#include "partial_map.h"

t_model *single_port_rams;
t_model *dual_port_rams;

struct s_linked_vptr *sp_memory_list;
struct s_linked_vptr *dp_memory_list;
struct s_linked_vptr *split_list;
struct s_linked_vptr *memory_instances = NULL;
struct s_linked_vptr *memory_port_size_list = NULL;
int split_size = 0;


void pad_dp_memory_width(nnode_t *node, netlist_t *netlist);
void pad_sp_memory_width(nnode_t *node, netlist_t *netlist);
void pad_memory_output_port(nnode_t *node, netlist_t *netlist, t_model *model, char *port_name);
void pad_memory_input_port(nnode_t *node, netlist_t *netlist, t_model *model, char *port_name);

int
get_memory_port_size(char *name)
{
	struct s_linked_vptr *mpl;

	mpl = memory_port_size_list;
	while (mpl != NULL)
	{
		if (strcmp(((t_memory_port_sizes *)mpl->data_vptr)->name, name) == 0)
			return ((t_memory_port_sizes *)mpl->data_vptr)->size;
		mpl = mpl->next;
	}
	return -1;
}


void 
report_memory_distribution()
{
	nnode_t *node;
	struct s_linked_vptr *temp;
	int i, idx;
	int width = 0;
	int depth = 0;
	int total_memory_bits = 0;

	if ((sp_memory_list == NULL) && (dp_memory_list == NULL))
		return;
	printf("\nHard Logical Memory Distribution\n");
	printf("============================\n");

	temp = sp_memory_list;
	
	int total_memory_block_counter = 0;
	int memory_max_width = 0;
	int memory_max_depth = 0;
	
	while (temp != NULL)
	{
		node = (nnode_t *)temp->data_vptr;
		oassert(node != NULL);
		oassert(node->type == MEMORY);

		/* Need to find the addr and data1 ports */
		idx = 0;
		for (i = 0; i < node->num_input_port_sizes; i++)
		{
			if (strcmp("addr", node->input_pins[idx]->mapping) == 0)
			{
				depth = node->input_port_sizes[i];
			}
			else if (strcmp("data", node->input_pins[idx]->mapping) == 0)
			{
				width = node->input_port_sizes[i];
			}
			idx += node->input_port_sizes[i];
		}

		printf("SPRAM: %d width %d depth\n", width, depth);
		total_memory_bits += width * (1 << depth);
		
		total_memory_block_counter++;
		if (width > memory_max_width) {
			memory_max_width = width;
		}
		if (depth > memory_max_depth) {
			memory_max_depth = depth;
		}
		
		temp = temp->next;
	}

	temp = dp_memory_list;
	while (temp != NULL)
	{
		node = (nnode_t *)temp->data_vptr;
		oassert(node != NULL);
		oassert(node->type == MEMORY);

		/* Need to find the addr and data1 ports */
		idx = 0;
		for (i = 0; i < node->num_input_port_sizes; i++)
		{
			if (strcmp("addr", node->input_pins[idx]->mapping) == 0)
			{
				depth = node->input_port_sizes[i];
			} else if (strcmp("addr1", node->input_pins[idx]->mapping) == 0)
			{
				depth = node->input_port_sizes[i];
			}
			else if (strcmp("data1", node->input_pins[idx]->mapping) == 0)
			{
				width = node->input_port_sizes[i];
			}
			idx += node->input_port_sizes[i];
		}

		printf("DPRAM: %d width %d depth\n", width, depth);
		total_memory_bits += width * (1 << depth);
		
		total_memory_block_counter++;
		if (width > memory_max_width) {
			memory_max_width = width;
		}
		if (depth > memory_max_depth) {
			memory_max_depth = depth;
		}
		
		temp = temp->next;
	}
	
	printf("\nTotal Logical Memory Blocks = %d \n", total_memory_block_counter);
	printf("Total Logical Memory bits = %d \n", total_memory_bits);
	printf("Max Memory Width = %d \n", memory_max_width);
	printf("Max Memory Depth = %d \n", memory_max_depth);
	
	printf("\n");
	return;
}

/*-------------------------------------------------------------------------
 * (function: split_sp_memory_depth)
 *
 * This function works to split the depth of a single port memory into 
 *   several smaller memories.
 *------------------------------------------------------------------------
 */
void
split_sp_memory_depth(nnode_t *node)
{
	int data_port = -1;
	int clk_port  = -1;
	int addr_port = -1;
	int we_port = -1;
	int logical_size;
	int i, j;
	int idx;
	int addr_pin_idx = 0;
	int we_pin_idx = 0;
	nnode_t *new_mem_node;
	nnode_t *and_node, *not_node,  *mux_node, *ff_node;
	npin_t *addr_pin = NULL;
	npin_t *we_pin = NULL;
	npin_t *clk_pin = NULL;
	npin_t *tdout_pin;


	oassert(node->type == MEMORY);


	// Find which port is the addr port
	idx = 0;
	for (i = 0; i < node->num_input_port_sizes; i++)
	{
		//printf("%s\n", node->input_pins[idx]->mapping);
		if (strcmp("addr", node->input_pins[idx]->mapping) == 0)
		{
			addr_port = i;
			addr_pin_idx = idx;
			addr_pin = node->input_pins[idx];
		}
		else if (strcmp("data", node->input_pins[idx]->mapping) == 0)
		{
			data_port = i;
		}
		else if (strcmp("we", node->input_pins[idx]->mapping) == 0)
		{
			we_port = i;
			we_pin = node->input_pins[idx];
			we_pin_idx = idx;
		}
		else if (strcmp("clk", node->input_pins[idx]->mapping) == 0)
		{
			clk_port = i;
			clk_pin = node->input_pins[idx];
		}
		idx += node->input_port_sizes[i];
	}
	if (data_port == -1)
	{
		error_message(1, 0, -1, "No \"data\" port on single port RAM");
	}
	if (addr_port == -1)
	{
		error_message(1, 0, -1, "No \"addr\" port on single port RAM");
	}
	if (we_port == -1)
	{
		error_message(1, 0, -1, "No \"we\" port on single port RAM");
	}
	if (clk_port == -1)
	{
		error_message(1, 0, -1, "No \"clk\" port on single port RAM");
	}
	
	// Check that the memory needs to be split
	// Jason Luu HACK: Logical memory depth determination messed up, forced to use this method
	for(i = 0; i < node->input_port_sizes[addr_port]; i++)
	{
		if(strcmp(node->input_pins[addr_pin_idx + i]->name, "top^ZERO_PAD_ZERO") == 0)
			break;
	}
	logical_size = i;
	if (split_size <= 0)
	{
		printf("Unsupported feature! Split size must be a positive number\n");
		exit(1);
	}
	if ((split_size > 0) && (logical_size <= split_size)) {
		sp_memory_list = insert_in_vptr_list(sp_memory_list, node);
		return;
	}

	// Let's remove the address line from the memory
	for (i = addr_pin_idx; i < node->num_input_pins - 1; i++)
	{
		node->input_pins[i] = node->input_pins[i+1];
		node->input_pins[i]->pin_node_idx--;
	}
	node->input_port_sizes[addr_port]--;
	node->input_pins = realloc(node->input_pins, sizeof(npin_t *) * --node->num_input_pins);

	if (we_pin_idx >= addr_pin_idx)
		we_pin_idx--;

	// Create the new memory node
	new_mem_node = allocate_nnode();
	// Append the new name with an __H
	new_mem_node->name = append_string(node->name, "__H");

	{	// Append the old name with an __S
		char *new_name = append_string(node->name, "__S");
		free(node->name);
		node->name = new_name;
	}

	// Copy properties from the original memory node
	new_mem_node->type = node->type;
	new_mem_node->related_ast_node = node->related_ast_node;
	new_mem_node->traverse_visited = node->traverse_visited;

	add_output_port_information(new_mem_node, node->num_output_pins);
	allocate_more_node_output_pins (new_mem_node, node->num_output_pins);

	for (j = 0; j < node->num_input_port_sizes; j++)
		add_input_port_information(new_mem_node, node->input_port_sizes[j]);

	// Copy over the input pins for the new memory, excluding we
	allocate_more_node_input_pins (new_mem_node, node->num_input_pins);
	for (j = 0; j < node->num_input_pins; j++)
	{
		if (j != we_pin_idx)
			add_a_input_pin_to_node_spot_idx(new_mem_node, copy_input_npin(node->input_pins[j]), j);
	}

	and_node = make_2port_gate(LOGICAL_AND, 1, 1, 1, node, node->traverse_visited);
	add_a_input_pin_to_node_spot_idx(and_node, we_pin, 1);
	add_a_input_pin_to_node_spot_idx(and_node, addr_pin, 0);
	connect_nodes(and_node, 0, node, we_pin_idx);
	node->input_pins[we_pin_idx]->mapping = we_pin->mapping;

	not_node = make_not_gate_with_input(copy_input_npin(addr_pin), new_mem_node, new_mem_node->traverse_visited);
	and_node = make_2port_gate(LOGICAL_AND, 1, 1, 1, new_mem_node, new_mem_node->traverse_visited);
	connect_nodes(not_node, 0, and_node, 0);
	add_a_input_pin_to_node_spot_idx(and_node, copy_input_npin(we_pin), 1);
	connect_nodes(and_node, 0, new_mem_node, we_pin_idx);
	new_mem_node->input_pins[we_pin_idx]->mapping = we_pin->mapping;

	ff_node = make_2port_gate(FF_NODE, 1, 1, 1, node, node->traverse_visited);
	add_a_input_pin_to_node_spot_idx(ff_node, copy_input_npin(addr_pin), 0);
	add_a_input_pin_to_node_spot_idx(ff_node, copy_input_npin(clk_pin), 1);

	// Copy over the output pins for the new memory
	for (j = 0; j < node->num_output_pins; j++)
	{
		mux_node = make_2port_gate(MUX_2, 2, 2, 1, node, node->traverse_visited);
		connect_nodes(ff_node, 0, mux_node, 0);

		not_node = make_not_gate(node, node->traverse_visited);
		connect_nodes(ff_node, 0, not_node, 0);
		connect_nodes(not_node, 0, mux_node, 1);

		tdout_pin = node->output_pins[j];
		remap_pin_to_new_node(tdout_pin, mux_node, 0);

		connect_nodes(node, j, mux_node, 2);
		node->output_pins[j]->mapping = tdout_pin->mapping;

		connect_nodes(new_mem_node, j, mux_node, 3);
		new_mem_node->output_pins[j]->mapping = tdout_pin->mapping;

		tdout_pin->mapping = NULL;

		mux_node->output_pins[0]->name = mux_node->name;
	}

	// must recurse on new memory if it's too small
	if (logical_size <= split_size) {
		sp_memory_list = insert_in_vptr_list(sp_memory_list, new_mem_node);
		sp_memory_list = insert_in_vptr_list(sp_memory_list, node);
	} else {
		split_sp_memory_depth(node);
		split_sp_memory_depth(new_mem_node);
	}

	return;
}

/*-------------------------------------------------------------------------
 * (function: split_dp_memory_depth)
 *
 * This function works to split the depth of a dual port memory into 
 *   several smaller memories.
 *------------------------------------------------------------------------
 */
void 
split_dp_memory_depth(nnode_t *node)
{
	int addr1_port = -1;
	int addr2_port = -1;
	int we1_port = -1;
	int we2_port = -1;
	int logical_size;
	int i, j;
	int idx;
	int addr1_pin_idx = 0;
	int we1_pin_idx = 0;
	int addr2_pin_idx = 0;
	int we2_pin_idx = 0;
	nnode_t *new_mem_node;
	nnode_t *and1_node, *not1_node, *ff1_node, *mux1_node;
	nnode_t *and2_node, *not2_node, *ff2_node, *mux2_node;
	npin_t *addr1_pin = NULL;
	npin_t *addr2_pin = NULL;
	npin_t *we1_pin = NULL;
	npin_t *we2_pin = NULL;
	npin_t *twe_pin, *taddr_pin;
	npin_t *clk_pin = NULL;
	npin_t *tdout_pin;

	oassert(node->type == MEMORY);

	/* Find which ports are the addr1 and addr2 ports */
	idx = 0;
	for (i = 0; i < node->num_input_port_sizes; i++)
	{
		if (strcmp("addr1", node->input_pins[idx]->mapping) == 0)
		{
			addr1_port = i;
			addr1_pin_idx = idx;
			addr1_pin = node->input_pins[idx];
		}
		else if (strcmp("addr2", node->input_pins[idx]->mapping) == 0)
		{
			addr2_port = i;
			addr2_pin_idx = idx;
			addr2_pin = node->input_pins[idx];
		}
		else if (strcmp("we1", node->input_pins[idx]->mapping) == 0)
		{
			we1_port = i;
			we1_pin = node->input_pins[idx];
			we1_pin_idx = idx;
		}
		else if (strcmp("we2", node->input_pins[idx]->mapping) == 0)
		{
			we2_port = i;
			we2_pin = node->input_pins[idx];
			we2_pin_idx = idx;
		}
		else if (strcmp("clk", node->input_pins[idx]->mapping) == 0)
		{
			clk_pin = node->input_pins[idx];
		}
		idx += node->input_port_sizes[i];
	}

	if (addr1_port == -1)
	{
		error_message(1, 0, -1, "No \"addr1\" port on dual port RAM");
	}

	
	/* Jason Luu HACK: Logical memory depth determination messed up, forced to use this method */
	for(i = 0; i < node->input_port_sizes[addr1_port]; i++)
	{
		if(strcmp(node->input_pins[addr1_pin_idx + i]->name, "top^ZERO_PAD_ZERO") == 0)
			break;
	}
	logical_size = i;
 	
	/* Check that the memory needs to be split */
	if (logical_size <= split_size) {
		dp_memory_list = insert_in_vptr_list(dp_memory_list, node);
		return;
	} 

	/* Let's remove the address1 line from the memory */
	for (i = addr1_pin_idx; i < node->num_input_pins - 1; i++)
	{
		node->input_pins[i] = node->input_pins[i+1];
		node->input_pins[i]->pin_node_idx--;
	}
	node->input_port_sizes[addr1_port]--;
	node->input_pins = realloc(node->input_pins, sizeof(npin_t *) * --node->num_input_pins);
	if ((we1_port != -1) && (we1_pin_idx >= addr1_pin_idx))
		we1_pin_idx--;
	if ((we2_port != -1) && (we2_pin_idx >= addr1_pin_idx))
		we2_pin_idx--;
	if ((addr2_port != -1) && (addr2_pin_idx >= addr1_pin_idx))
		addr2_pin_idx--;

	/* Let's remove the address2 line from the memory */
	if (addr2_port != -1)
	{
		for (i = addr2_pin_idx; i < node->num_input_pins - 1; i++)
		{
			node->input_pins[i] = node->input_pins[i+1];
			node->input_pins[i]->pin_node_idx--;
		}
		node->input_port_sizes[addr2_port]--;
		node->input_pins = realloc(node->input_pins, sizeof(npin_t *) * --node->num_input_pins);
		if ((we1_port != -1) && (we1_pin_idx >= addr2_pin_idx))
			we1_pin_idx--;
		if ((we2_port != -1) && (we2_pin_idx >= addr2_pin_idx))
			we2_pin_idx--;
		if (addr1_pin_idx >= addr2_pin_idx)
			addr1_pin_idx--;
	}

	/* Create the new memory node */
	new_mem_node = allocate_nnode();

	// Append the new name with an __H
	new_mem_node->name = append_string(node->name, "__H");

	{	// Append the old name with an __S
		char *new_name = append_string(node->name, "__S");
		free(node->name);
		node->name = new_name;
	}

	/* Copy properties from the original memory node */
	new_mem_node->type = node->type;
	new_mem_node->related_ast_node = node->related_ast_node;
	new_mem_node->traverse_visited = node->traverse_visited;

	// Copy over the port sizes for the new memory
	for (j = 0; j < node->num_output_port_sizes; j++)
		add_output_port_information(new_mem_node, node->output_port_sizes[j]);
	for (j = 0; j < node->num_input_port_sizes; j++)
		add_input_port_information (new_mem_node, node->input_port_sizes[j]);

	// allocate space for pins.
	allocate_more_node_output_pins (new_mem_node, node->num_output_pins);
	allocate_more_node_input_pins  (new_mem_node, node->num_input_pins);

	// Copy over the pins for the new memory
	for (j = 0; j < node->num_input_pins; j++)
		add_a_input_pin_to_node_spot_idx(new_mem_node, copy_input_npin(node->input_pins[j]), j);

	if (we1_pin != NULL)
	{
		and1_node = make_2port_gate(LOGICAL_AND, 1, 1, 1, node, node->traverse_visited);
		twe_pin = copy_input_npin(we1_pin);
		add_a_input_pin_to_node_spot_idx(and1_node, twe_pin, 1);
		taddr_pin = copy_input_npin(addr1_pin);
		add_a_input_pin_to_node_spot_idx(and1_node, taddr_pin, 0);
		connect_nodes(and1_node, 0, node, we1_pin_idx);
		node->input_pins[we1_pin_idx]->mapping = we1_pin->mapping;
	}

	if (we2_pin != NULL)
	{
		and2_node = make_2port_gate(LOGICAL_AND, 1, 1, 1, node, node->traverse_visited);
		twe_pin = copy_input_npin(we2_pin);
		add_a_input_pin_to_node_spot_idx(and2_node, twe_pin, 1);
		taddr_pin = copy_input_npin(addr2_pin);
		add_a_input_pin_to_node_spot_idx(and2_node, taddr_pin, 0);
		connect_nodes(and2_node, 0, node, we2_pin_idx);
		node->input_pins[we2_pin_idx]->mapping = we2_pin->mapping;
	}

	if (we1_pin != NULL)
	{
		taddr_pin = copy_input_npin(addr1_pin);
		not1_node = make_not_gate_with_input(taddr_pin, new_mem_node, new_mem_node->traverse_visited);
		and1_node = make_2port_gate(LOGICAL_AND, 1, 1, 1, new_mem_node, new_mem_node->traverse_visited);
		connect_nodes(not1_node, 0, and1_node, 0);
		add_a_input_pin_to_node_spot_idx(and1_node, we1_pin, 1);
		connect_nodes(and1_node, 0, new_mem_node, we1_pin_idx);
		new_mem_node->input_pins[we1_pin_idx]->mapping = we1_pin->mapping;
	}

	if (we2_pin != NULL)
	{
		taddr_pin = copy_input_npin(addr2_pin);
		not2_node = make_not_gate_with_input(taddr_pin, new_mem_node, new_mem_node->traverse_visited);
		and2_node = make_2port_gate(LOGICAL_AND, 1, 1, 1, new_mem_node, new_mem_node->traverse_visited);
		connect_nodes(not2_node, 0, and2_node, 0);
		add_a_input_pin_to_node_spot_idx(and2_node, we2_pin, 1);
		connect_nodes(and2_node, 0, new_mem_node, we2_pin_idx);
		new_mem_node->input_pins[we2_pin_idx]->mapping = we2_pin->mapping;
	}

	if (node->num_output_pins > 0) /* There is an "out1" output */
	{
		ff1_node = make_2port_gate(FF_NODE, 1, 1, 1, node, node->traverse_visited);
		add_a_input_pin_to_node_spot_idx(ff1_node, addr1_pin, 0);
		add_a_input_pin_to_node_spot_idx(ff1_node, copy_input_npin(clk_pin), 1);

		/* Copy over the output pins for the new memory */
		for (j = 0; j < node->output_port_sizes[0]; j++)
		{
			mux1_node = make_2port_gate(MUX_2, 2, 2, 1, node, node->traverse_visited);
			connect_nodes(ff1_node, 0, mux1_node, 0);
			not1_node = make_not_gate(node, node->traverse_visited);
			connect_nodes(ff1_node, 0, not1_node, 0);
			connect_nodes(not1_node, 0, mux1_node, 1);
			tdout_pin = node->output_pins[j];
			remap_pin_to_new_node(tdout_pin, mux1_node, 0);
			connect_nodes(node, j, mux1_node, 2);
			node->output_pins[j]->mapping = tdout_pin->mapping;
			connect_nodes(new_mem_node, j, mux1_node, 3);
			new_mem_node->output_pins[j]->mapping = tdout_pin->mapping;
			tdout_pin->mapping = NULL;

			mux1_node->output_pins[0]->name = mux1_node->name;
		}
	}

	if (node->num_output_pins > node->output_port_sizes[0]) /* There is an "out2" output */
	{
		ff2_node = make_2port_gate(FF_NODE, 1, 1, 1, node, node->traverse_visited);
		add_a_input_pin_to_node_spot_idx(ff2_node, addr2_pin, 0);
		add_a_input_pin_to_node_spot_idx(ff2_node, copy_input_npin(clk_pin), 1);

		/* Copy over the output pins for the new memory */
		for (j = 0; j < node->output_port_sizes[0]; j++)
		{
			mux2_node = make_2port_gate(MUX_2, 2, 2, 1, node, node->traverse_visited);
			connect_nodes(ff2_node, 0, mux2_node, 0);
			not2_node = make_not_gate(node, node->traverse_visited);

			connect_nodes(ff2_node, 0, not2_node, 0);
			connect_nodes(not2_node, 0, mux2_node, 1);

			tdout_pin = node->output_pins[node->output_port_sizes[0] + j];
			remap_pin_to_new_node(tdout_pin, mux2_node, 0);

			connect_nodes(node, node->output_port_sizes[0] + j, mux2_node, 2);
			node->output_pins[node->output_port_sizes[0] + j]->mapping = tdout_pin->mapping;
			connect_nodes(new_mem_node, node->output_port_sizes[0] + j, mux2_node, 3);
			new_mem_node->output_pins[node->output_port_sizes[0] + j]->mapping = tdout_pin->mapping;
			tdout_pin->mapping = NULL;

			mux2_node->output_pins[0]->name = mux2_node->name;
		}
	}

	/* must recurse on new memory if it's too small */
	if (logical_size <= split_size) {
		dp_memory_list = insert_in_vptr_list(dp_memory_list, new_mem_node);
		dp_memory_list = insert_in_vptr_list(dp_memory_list, node);
	} else {
		split_dp_memory_depth(node);
		split_dp_memory_depth(new_mem_node);
	}

	return;
}

/*-------------------------------------------------------------------------
 * (function: split_sp_memory_width)
 *
 * This function works to split the width of a memory into several smaller
 *   memories.
 *------------------------------------------------------------------------
 */
void 
split_sp_memory_width(nnode_t *node)
{
	int data_port;
	int i, j, k, idx, old_idx, diff;
	nnode_t *new_node;

	oassert(node->type == MEMORY);

	/* Find which port is the data port on the input! */
	idx = 0;
	data_port = -1;
	for (i = 0; i < node->num_input_port_sizes; i++)
	{
		if (strcmp("data", node->input_pins[idx]->mapping) == 0)
			data_port = i;
		idx += node->input_port_sizes[i];
	}
	if (data_port == -1)
	{
		error_message(1, 0, -1, "No \"data\" port on single port RAM");
	}

	diff = node->input_port_sizes[data_port];

	/* Need to create a new node for every data bit */
	for (i = 1; i < node->input_port_sizes[data_port]; i++)
	{
		char BUF[10];
		new_node = allocate_nnode();
		sp_memory_list = insert_in_vptr_list(sp_memory_list, new_node);
		new_node->name = (char *)malloc(strlen(node->name) + 10);
		strcpy(new_node->name, node->name);
		strcat(new_node->name, "-");
		sprintf(BUF, "%d", i);
		strcat(new_node->name, BUF);

		/* Copy properties from the original node */
		new_node->type = node->type;
		new_node->related_ast_node = node->related_ast_node;
		new_node->traverse_visited = node->traverse_visited;
		new_node->node_data = NULL;

		new_node->num_input_port_sizes = node->num_input_port_sizes;
		new_node->input_port_sizes = (int *)malloc(node->num_input_port_sizes * sizeof(int));
		for (j = 0; j < node->num_input_port_sizes; j++)
			new_node->input_port_sizes[j] = node->input_port_sizes[j];

		new_node->input_port_sizes[data_port] = 1;
		new_node->num_output_port_sizes = 1;
		new_node->output_port_sizes = (int *)malloc(sizeof(int));
		new_node->output_port_sizes[0] = 1;

		/* Set the number of input pins and pin entires */
		new_node->num_input_pins = node->num_input_pins - diff + 1;
		new_node->input_pins = (npin_t**)malloc(sizeof(void *) * new_node->num_input_pins);

		idx = 0;
		old_idx = 0;
		for (j = 0; j < new_node->num_input_port_sizes; j++)
		{
			if (j == data_port)
			{
				new_node->input_pins[idx] = node->input_pins[idx + i];
				node->input_pins[idx+i] = NULL;
				new_node->input_pins[idx]->node = new_node;
				new_node->input_pins[idx]->pin_node_idx = idx;
				old_idx = old_idx + node->input_port_sizes[data_port];
				idx++;
			}
			else
			{
				for (k = 0; k < new_node->input_port_sizes[j]; k++)
				{
					new_node->input_pins[idx] = copy_input_npin(node->input_pins[old_idx]);
					new_node->input_pins[idx]->pin_node_idx = idx;
					new_node->input_pins[idx]->node = new_node;
					idx++;
					old_idx++;
				}
			}
		}

		/* Set the number of output pins and pin entry */
		new_node->num_output_pins = 1;
		new_node->output_pins = (npin_t **)malloc(sizeof(void*));
		new_node->output_pins[0] = copy_output_npin(node->output_pins[i]);
		add_a_driver_pin_to_net(node->output_pins[i]->net, new_node->output_pins[0]);
		free_npin(node->output_pins[i]);
		node->output_pins[i] = NULL;
		new_node->output_pins[0]->pin_node_idx = 0;
		new_node->output_pins[0]->node = new_node;
	}

	/* Now need to clean up the original to do 1 bit output - first bit */

	/* Name the node to show first bit! */
	{
		char *new_name = append_string(node->name, "-0");
		free(node->name);
		node->name = new_name;
	}

	/* free the additional output pins */
	for (i = 1; i < node->num_output_pins; i++)
		free_npin(node->output_pins[i]);
	node->num_output_pins = 1;
	node->output_pins = realloc(node->output_pins, sizeof(npin_t *) * 1);
	node->output_port_sizes[0] = 1;

	/* Shuffle the input pins on account of removed input pins */
	idx = old_idx = 0;
	node->input_port_sizes[data_port] = 1;
	for (i = 0; i < node->num_input_port_sizes; i++)
	{
		for (j = 0; j < node->input_port_sizes[i]; j++)
		{
			node->input_pins[idx] = node->input_pins[old_idx];
			node->input_pins[idx]->pin_node_idx = idx;
			idx++;
			old_idx++;
		}
		if (i == data_port)
			old_idx = old_idx + diff - 1;
	}
	node->num_input_pins = node->num_input_pins - diff + 1;
	sp_memory_list = insert_in_vptr_list(sp_memory_list, node);

	return;
}



/*-------------------------------------------------------------------------
 * (function: split_dp_memory_width)
 *
 * This function works to split the width of a memory into several smaller
 *   memories.
 *------------------------------------------------------------------------
 */
void 
split_dp_memory_width(nnode_t *node)
{
	int data_port1, data_port2;
	int i, j, k, idx, old_idx, data_diff1, data_diff2;
	nnode_t *new_node;
	char *tmp_name;

	oassert(node->type == MEMORY);

	/* Find which port is the data port on the input! */
	idx = 0;
	data_port1 = -1;
	data_port2 = -1;
	data_diff1 = 0;
	data_diff2 = 0;

	for (i = 0; i < node->num_input_port_sizes; i++)
	{
		if (strcmp("data1", node->input_pins[idx]->mapping) == 0)
		{
			data_port1 = i;
			data_diff1 = node->input_port_sizes[data_port1] - 1;
		}
		if (strcmp("data2", node->input_pins[idx]->mapping) == 0)
		{
			data_port2 = i;
			data_diff2 = node->input_port_sizes[data_port2] - 1;
		}
		idx += node->input_port_sizes[i];
	}

	if (data_port1 == -1)
	{
		error_message(1, 0, -1, "No \"data1\" port on dual port RAM");
		return;
	}

	/* Need to create a new node for every data bit */
	for (i = 1; i < node->input_port_sizes[data_port1]; i++)
	{
		char BUF[10];
		new_node = allocate_nnode();
		dp_memory_list = insert_in_vptr_list(dp_memory_list, new_node);
		new_node->name = (char *)malloc(strlen(node->name) + 10);
		strcpy(new_node->name, node->name);
		strcat(new_node->name, "-");
		sprintf(BUF, "%d", i);
		strcat(new_node->name, BUF);

		/* Copy properties from the original node */
		new_node->type = node->type;
		new_node->related_ast_node = node->related_ast_node;
		new_node->traverse_visited = node->traverse_visited;
		new_node->node_data = NULL;

		new_node->num_input_port_sizes = node->num_input_port_sizes;
		new_node->input_port_sizes = (int *)malloc(node->num_input_port_sizes * sizeof(int));
		for (j = 0; j < node->num_input_port_sizes; j++)
			new_node->input_port_sizes[j] = node->input_port_sizes[j];

		new_node->input_port_sizes[data_port1] = 1;
		if (data_port2 != -1)
			new_node->input_port_sizes[data_port2] = 1;

		if (data_port2 == -1)
		{
			new_node->num_output_port_sizes = 1;
			new_node->output_port_sizes = (int *)malloc(sizeof(int));
		}
		else
		{
			new_node->num_output_port_sizes = 2;
			new_node->output_port_sizes = (int *)malloc(sizeof(int)*2);
			new_node->output_port_sizes[1] = 1;
		}
		new_node->output_port_sizes[0] = 1;

		/* Set the number of input pins and pin entires */
		new_node->num_input_pins = node->num_input_pins - data_diff1 - data_diff2;
		new_node->input_pins = (npin_t**)malloc(sizeof(void *) * new_node->num_input_pins);


		idx = 0;
		old_idx = 0;
		for (j = 0; j < new_node->num_input_port_sizes; j++)
		{
			if (j == data_port1)
			{
				new_node->input_pins[idx] = node->input_pins[old_idx + i];
				node->input_pins[old_idx+i] = NULL;
				new_node->input_pins[idx]->node = new_node;
				new_node->input_pins[idx]->pin_node_idx = idx;
				old_idx = old_idx + node->input_port_sizes[data_port1];
				idx++;
			}
			else if (j == data_port2)
			{
				new_node->input_pins[idx] = node->input_pins[old_idx + i];
				node->input_pins[old_idx+i] = NULL;
				new_node->input_pins[idx]->node = new_node;
				new_node->input_pins[idx]->pin_node_idx = idx;
				old_idx = old_idx + node->input_port_sizes[data_port2];
				idx++;
			}
			else
			{
				for (k = 0; k < new_node->input_port_sizes[j]; k++)
				{
					new_node->input_pins[idx] = copy_input_npin(node->input_pins[old_idx]);
					new_node->input_pins[idx]->pin_node_idx = idx;
					new_node->input_pins[idx]->node = new_node;
					idx++;
					old_idx++;
				}
			}
		}

		/* Set the number of output pins and pin entry */
		if (data_port2 == -1)
		{
			new_node->num_output_pins = 1;
			new_node->output_pins = (npin_t **)malloc(sizeof(void*));
			new_node->output_pins[0] = node->output_pins[i];
			node->output_pins[i] = NULL;
			new_node->output_pins[0]->pin_node_idx = 0;
			new_node->output_pins[0]->node = new_node;
		}
		else
		{
			new_node->num_output_pins = 2;
			new_node->output_pins = (npin_t **)malloc(sizeof(void*)*2);
			new_node->output_pins[0] = node->output_pins[i];
			node->output_pins[i] = NULL;
			new_node->output_pins[0]->pin_node_idx = 0;
			new_node->output_pins[0]->node = new_node;

			new_node->output_pins[1] = node->output_pins[i+data_diff1+1];
			node->output_pins[i+data_diff1+1] = NULL;
			new_node->output_pins[1]->pin_node_idx = 1;
			new_node->output_pins[1]->node = new_node;
		}
	}

	/* Now need to clean up the original to do 1 bit output - first bit */
	/* Name the node to show first bit! */
	tmp_name = (char *)malloc(strlen(node->name) + 3);
	strcpy(tmp_name, node->name);
	strcat(tmp_name, "-0");
	free(node->name);
	node->name = tmp_name;

	/* free the additional output pins */
	if (data_port2 == -1)
	{
		for (i = 1; i < node->num_output_pins; i++)
			free_npin(node->output_pins[i]);

		node->num_output_pins = 1;
		node->output_pins = realloc(node->output_pins, sizeof(npin_t *) * node->num_output_pins);
		node->output_port_sizes[0] = 1;
	}
	else
	{
		for (i = 1; i < (node->num_output_pins/2); i++)
			free_npin(node->output_pins[i]);
		node->output_pins[1] = node->output_pins[data_diff1 + 1];
		node->output_pins[data_diff1 + 1] = NULL;
		node->output_pins[1]->pin_node_idx = 1;
		for (; i < node->num_output_pins; i++)
			free_npin(node->output_pins[i]);

		node->num_output_pins = 2;
		node->output_pins = realloc(node->output_pins, sizeof(npin_t *) * node->num_output_pins);
		node->output_port_sizes[0] = 1;
		node->output_port_sizes[1] = 1;
	}

	/* Shuffle the input pins on account of removed input pins */
	idx = old_idx = 0;
	node->input_port_sizes[data_port1] = 1;
	if (data_port2 != -1)
		node->input_port_sizes[data_port2] = 1;
	for (i = 0; i < node->num_input_port_sizes; i++)
	{
		for (j = 0; j < node->input_port_sizes[i]; j++)
		{
			node->input_pins[idx] = node->input_pins[old_idx];
			node->input_pins[idx]->pin_node_idx = idx;
			idx++;
			old_idx++;
		}
		if (i == data_port1)
			old_idx = old_idx + data_diff1;
		if (i == data_port2)
			old_idx = old_idx + data_diff2;
	}
	node->num_input_pins = node->num_input_pins - data_diff1;
	if (data_port2 != -1)
		node->num_input_pins = node->num_input_pins - data_diff2;

	node->input_pins = realloc(node->input_pins, sizeof(npin_t *) * node->num_input_pins);
	dp_memory_list = insert_in_vptr_list(dp_memory_list, node);

	return;
}

/*
 * Width-splits the given memory up into chunks the of the
 * width specified in the arch file.
 */
void split_sp_memory_to_arch_width(nnode_t *node)
{
	char *port_name = "data";
	t_model *model = single_port_rams;

	int data_port_number = get_input_port_index_from_mapping(node, port_name);

	oassert(data_port_number != -1);

	int data_port_size = node->input_port_sizes[data_port_number];

	// Get the target width from the arch.
	t_model_ports *ports = get_model_port(model->inputs, port_name);
	int target_size  = ports->size;

	int num_memories = ceil((double)data_port_size / (double)target_size);
	if (data_port_size > target_size)
	{
		int i;
		int data_pins_moved = 0;
		int output_pins_moved = 0;
		for (i = 0; i < num_memories; i++)
		{
			nnode_t *new_node = allocate_nnode();
			new_node->name = append_string(node->name, "-%d",i);
			sp_memory_list = insert_in_vptr_list(sp_memory_list, new_node);

			/* Copy properties from the original node */
			new_node->type = node->type;
			new_node->related_ast_node = node->related_ast_node;
			new_node->traverse_visited = node->traverse_visited;
			new_node->node_data = NULL;

			int j;
			for (j = 0; j < node->num_input_port_sizes; j++)
				add_input_port_information(new_node, 0);

			add_output_port_information(new_node, 0);

			int index = 0;
			int old_index = 0;
			for (j = 0; j < node->num_input_port_sizes; j++)
			{
				// Move this node's share of data pins out of the data port of the original node.
				if (j == data_port_number)
				{
					// Skip over data pins we've already moved.
					old_index += data_pins_moved;
					int k;
					for (k = 0; k < target_size && data_pins_moved < data_port_size; k++)
					{
						allocate_more_node_input_pins(new_node, 1);
						new_node->input_port_sizes[j]++;
						remap_pin_to_new_node(node->input_pins[old_index], new_node, index);
						index++;
						old_index++;
						data_pins_moved++;
					}
					int remaining_data_pins = data_port_size - data_pins_moved;
					// Skip over pins we have yet to copy.
					old_index += remaining_data_pins;
				}
				else
				{
					int k;
					for (k = 0; k < node->input_port_sizes[j]; k++)
					{
						allocate_more_node_input_pins(new_node, 1);
						new_node->input_port_sizes[j]++;
						// Copy pins for all but the last memory. the last one get the original pins moved to it.
						if (i < num_memories - 1)
							add_a_input_pin_to_node_spot_idx(new_node, copy_input_npin(node->input_pins[old_index]), index);
						else
							remap_pin_to_new_node(node->input_pins[old_index], new_node, index);
						index++;
						old_index++;
					}
				}
			}

			index = 0;
			old_index = 0;
			old_index += output_pins_moved;

			int k;
			for (k = 0; k < target_size && output_pins_moved < data_port_size; k++)
			{
				allocate_more_node_output_pins(new_node, 1);
				new_node->output_port_sizes[0]++;
				remap_pin_to_new_node(node->output_pins[old_index], new_node, index);
				index++;
				old_index++;
				output_pins_moved++;
			}
		}
		// Free the original node.
		free_nnode(node);
	}
	else
	{
		sp_memory_list = insert_in_vptr_list(sp_memory_list, node);
	}
}

void split_dp_memory_to_arch_width(nnode_t *node)
{
	char *data1_name = "data1";
	char *data2_name = "data2";
	char *out1_name = "out1";
	char *out2_name = "out2";

	t_model *model = dual_port_rams;

	int data1_port_number = get_input_port_index_from_mapping(node, data1_name);
	int data2_port_number = get_input_port_index_from_mapping(node, data2_name);

	int out1_port_number  = get_output_port_index_from_mapping(node, out1_name);
	int out2_port_number  = get_output_port_index_from_mapping(node, out2_name);

	oassert(data1_port_number != -1);
	oassert(data2_port_number != -1);
	oassert(out1_port_number  != -1);
	oassert(out2_port_number  != -1);

	int data1_port_size = node->input_port_sizes[data1_port_number];
	int data2_port_size = node->input_port_sizes[data2_port_number];

	int out1_port_size  = node->output_port_sizes[out1_port_number];
	int out2_port_size  = node->output_port_sizes[out2_port_number];

	oassert(data1_port_size == data2_port_size);
	oassert(out1_port_size  == out2_port_size);
	oassert(data1_port_size == out1_port_size);

	// Get the target width from the arch.
	t_model_ports *ports = get_model_port(model->inputs, data1_name);
	int target_size  = ports->size;

	int num_memories = ceil((double)data1_port_size / (double)target_size);

	if (data1_port_size > target_size)
	{
		int i;
		int data1_pins_moved = 0;
		int data2_pins_moved = 0;
		int out1_pins_moved  = 0;
		int out2_pins_moved  = 0;
		for (i = 0; i < num_memories; i++)
		{
			nnode_t *new_node = allocate_nnode();
			new_node->name = append_string(node->name, "-%d",i);
			dp_memory_list = insert_in_vptr_list(dp_memory_list, new_node);

			/* Copy properties from the original node */
			new_node->type = node->type;
			new_node->related_ast_node = node->related_ast_node;
			new_node->traverse_visited = node->traverse_visited;
			new_node->node_data = NULL;

			int j;
			for (j = 0; j < node->num_input_port_sizes; j++)
				add_input_port_information(new_node, 0);

			int index = 0;
			int old_index = 0;
			for (j = 0; j < node->num_input_port_sizes; j++)
			{
				// Move this node's share of data pins out of the data port of the original node.
				if (j == data1_port_number)
				{
					// Skip over data pins we've already moved.
					old_index += data1_pins_moved;
					int k;
					for (k = 0; k < target_size && data1_pins_moved < data1_port_size; k++)
					{
						allocate_more_node_input_pins(new_node, 1);
						new_node->input_port_sizes[j]++;
						remap_pin_to_new_node(node->input_pins[old_index], new_node, index);
						index++;
						old_index++;
						data1_pins_moved++;
					}
					int remaining_data_pins = data1_port_size - data1_pins_moved;
					// Skip over pins we have yet to copy.
					old_index += remaining_data_pins;
				}
				else if (j == data2_port_number)
				{
					// Skip over data pins we've already moved.
					old_index += data2_pins_moved;
					int k;
					for (k = 0; k < target_size && data2_pins_moved < data2_port_size; k++)
					{
						allocate_more_node_input_pins(new_node, 1);
						new_node->input_port_sizes[j]++;
						remap_pin_to_new_node(node->input_pins[old_index], new_node, index);
						index++;
						old_index++;
						data2_pins_moved++;
					}
					int remaining_data_pins = data2_port_size - data2_pins_moved;
					// Skip over pins we have yet to copy.
					old_index += remaining_data_pins;
				}
				else
				{
					int k;
					for (k = 0; k < node->input_port_sizes[j]; k++)
					{
						allocate_more_node_input_pins(new_node, 1);
						new_node->input_port_sizes[j]++;
						// Copy pins for all but the last memory. the last one get the original pins moved to it.
						if (i < num_memories - 1)
							add_a_input_pin_to_node_spot_idx(new_node, copy_input_npin(node->input_pins[old_index]), index);
						else
							remap_pin_to_new_node(node->input_pins[old_index], new_node, index);
						index++;
						old_index++;
					}
				}
			}

			for (j = 0; j < node->num_output_port_sizes; j++)
				add_output_port_information(new_node, 0);

			index = 0;
			old_index = 0;
			for (j = 0; j < node->num_output_port_sizes; j++)
			{
				// Move this node's share of data pins out of the data port of the original node.
				if (j == out1_port_number)
				{
					// Skip over data pins we've already moved.
					old_index += out1_pins_moved;
					int k;
					for (k = 0; k < target_size && out1_pins_moved < out1_port_size; k++)
					{
						allocate_more_node_output_pins(new_node, 1);
						new_node->output_port_sizes[j]++;
						remap_pin_to_new_node(node->output_pins[old_index], new_node, index);
						index++;
						old_index++;
						out1_pins_moved++;
					}
					int remaining_pins = out1_port_size - out1_pins_moved;
					// Skip over pins we have yet to copy.
					old_index += remaining_pins;
				}
				else if (j == out2_port_number)
				{
					// Skip over data pins we've already moved.
					old_index += out2_pins_moved;
					int k;
					for (k = 0; k < target_size && out2_pins_moved < out2_port_size; k++)
					{
						allocate_more_node_output_pins(new_node, 1);
						new_node->output_port_sizes[j]++;
						remap_pin_to_new_node(node->output_pins[old_index], new_node, index);
						index++;
						old_index++;
						out2_pins_moved++;
					}
					int remaining_pins = out2_port_size - out2_pins_moved;
					// Skip over pins we have yet to copy.
					old_index += remaining_pins;
				}
				else
				{
					oassert(FALSE);
				}
			}
		}
		// Free the original node.
		free_nnode(node);
	}
	else
	{
		// If we're not splitting, put the original memory node back.
		dp_memory_list = insert_in_vptr_list(dp_memory_list, node);
	}

}


/*-------------------------------------------------------------------------
 * (function: iterate_memories)
 *
 * This function will iterate over all of the memory hard blocks that
 *      exist in the netlist and perform a splitting so that they can
 *      be easily packed into hard memory blocks on the FPGA.
 *-----------------------------------------------------------------------*/
void 
iterate_memories(netlist_t *netlist)
{
	nnode_t *node;
	struct s_linked_vptr *temp;

	/* Split it up on depth */
	if (configuration.split_memory_depth)
	{
		temp = sp_memory_list;
		sp_memory_list = NULL;
		while (temp != NULL)
		{
			node = (nnode_t *)temp->data_vptr;
			oassert(node != NULL);
			oassert(node->type == MEMORY);
			temp = delete_in_vptr_list(temp);
			split_sp_memory_depth(node);
		}
		temp = dp_memory_list;
		dp_memory_list = NULL;
		while (temp != NULL)
		{
			node = (nnode_t *)temp->data_vptr;
			oassert(node != NULL);
			oassert(node->type == MEMORY);
			temp = delete_in_vptr_list(temp);
			split_dp_memory_depth(node);
		}
	}

	/* Split memory up on width */
	if (configuration.split_memory_width)
	{
		temp = sp_memory_list;
		sp_memory_list = NULL;
		while (temp != NULL)
		{
			node = (nnode_t *)temp->data_vptr;
			oassert(node != NULL);
			oassert(node->type == MEMORY);
			temp = delete_in_vptr_list(temp);
			split_sp_memory_width(node);
		}

		temp = dp_memory_list;
		dp_memory_list = NULL;
		while (temp != NULL)
		{
			node = (nnode_t *)temp->data_vptr;
			oassert(node != NULL);
			oassert(node->type == MEMORY);
			temp = delete_in_vptr_list(temp);
			split_dp_memory_width(node);
		}
	}
	else
	{
		temp = sp_memory_list;
		sp_memory_list = NULL;
		while (temp != NULL)
		{
			node = (nnode_t *)temp->data_vptr;
			oassert(node != NULL);
			oassert(node->type == MEMORY);
			temp = delete_in_vptr_list(temp);
			split_sp_memory_to_arch_width(node);
		}

		temp = sp_memory_list;
		sp_memory_list = NULL;
		while (temp != NULL)
		{
			node = (nnode_t *)temp->data_vptr;
			oassert(node != NULL);
			oassert(node->type == MEMORY);
			temp = delete_in_vptr_list(temp);
			pad_sp_memory_width(node, netlist);
		}

		temp = dp_memory_list;
		dp_memory_list = NULL;
		while (temp != NULL)
		{
			node = (nnode_t *)temp->data_vptr;
			oassert(node != NULL);
			oassert(node->type == MEMORY);
			temp = delete_in_vptr_list(temp);
			split_dp_memory_to_arch_width(node);
		}

		temp = dp_memory_list;
		dp_memory_list = NULL;
		while (temp != NULL)
		{
			node = (nnode_t *)temp->data_vptr;
			oassert(node != NULL);
			oassert(node->type == MEMORY);
			temp = delete_in_vptr_list(temp);
			pad_dp_memory_width(node, netlist);
		}
	}
	return;
}

/*-------------------------------------------------------------------------
 * (function: clean_memories)
 *
 * Clean up the memory by deleting the list structure of memories
 *      during optimization.
 *-----------------------------------------------------------------------*/
void clean_memories()
{
	while (sp_memory_list != NULL)
		sp_memory_list = delete_in_vptr_list(sp_memory_list);
	while (dp_memory_list != NULL)
		dp_memory_list = delete_in_vptr_list(dp_memory_list);
}

/*
 * Pads the width of a dual port memory to that specified in the arch file.
 */
void pad_dp_memory_width(nnode_t *node, netlist_t *netlist)
{
	oassert(node->type == MEMORY);
	oassert(dual_port_rams != NULL);

	pad_memory_input_port(node, netlist, dual_port_rams, "data1");
	pad_memory_input_port(node, netlist, dual_port_rams, "data2");

	pad_memory_output_port(node, netlist, dual_port_rams, "out1");
	pad_memory_output_port(node, netlist, dual_port_rams, "out2");

	dp_memory_list = insert_in_vptr_list(dp_memory_list, node);
}

/*
 * Pads the width of a single port memory to that specified in the arch file.
 */
void pad_sp_memory_width(nnode_t *node, netlist_t *netlist)
{
	oassert(node->type == MEMORY);
	oassert(single_port_rams != NULL);

	pad_memory_input_port (node, netlist, single_port_rams, "data");

	pad_memory_output_port(node, netlist, single_port_rams, "out");

	sp_memory_list = insert_in_vptr_list(sp_memory_list, node);
}

/*
 * Pads the given output port to the width specified in the given model.
 */
void pad_memory_output_port(nnode_t *node, netlist_t *netlist, t_model *model, char *port_name)
{
	int port_number = get_output_port_index_from_mapping(node, port_name);
	int port_index  = get_output_pin_index_from_mapping (node, port_name);

	int port_size = node->output_port_sizes[port_number];

	t_model_ports *ports = get_model_port(model->outputs, port_name);

	int target_size = ports->size;
	int diff        = target_size - port_size;

	if (diff > 0)
	{
		allocate_more_node_output_pins(node, diff);

		// Shift other pins to the right, if any.
		int i;
		for (i = node->num_output_pins - 1; i >= port_index + target_size; i--)
			move_a_output_pin(node, i - diff, i);

		for (i = port_index + port_size; i < port_index + target_size; i++)
		{
			// Add new pins to the higher order spots.
			npin_t *new_pin = allocate_npin();
			new_pin->mapping = strdup(port_name);
			add_a_output_pin_to_node_spot_idx(node, new_pin, i);
		}
		node->output_port_sizes[port_number] = target_size;
	}
}

/*
 * Pads the given input port to the width specified in the given model.
 */
void pad_memory_input_port(nnode_t *node, netlist_t *netlist, t_model *model, char *port_name)
{
	oassert(node->type == MEMORY);
	oassert(model != NULL);

	int port_number = get_input_port_index_from_mapping(node, port_name);
	int port_index  = get_input_pin_index_from_mapping (node, port_name);

	oassert(port_number != -1);
	oassert(port_index  != -1);

	int port_size = node->input_port_sizes[port_number];

	t_model_ports *ports = get_model_port(model->inputs, port_name);

	int target_size = ports->size;
	int diff        = target_size - port_size;


	// Expand the inputs
	if (diff > 0)
	{
		allocate_more_node_input_pins(node, diff);

		// Shift other pins to the right, if any.
		int i;
		for (i = node->num_input_pins - 1; i >= port_index + target_size; i--)
			move_a_input_pin(node, i - diff, i);

		for (i = port_index + port_size; i < port_index + target_size; i++)
		{
			add_a_input_pin_to_node_spot_idx(node, get_a_pad_pin(netlist), i);
			node->input_pins[i]->mapping = strdup(port_name);
		}

		node->input_port_sizes[port_number] = target_size;
	}
}

#ifdef VPR6

char is_sp_ram(nnode_t *node)
{
	return is_ast_sp_ram(node->related_ast_node);
}

char is_dp_ram(nnode_t *node)
{
	return is_ast_dp_ram(node->related_ast_node);
}

char is_ast_sp_ram(ast_node_t *node)
{
	char *identifier = node->children[0]->types.identifier;
	if (!strcmp(identifier, "single_port_ram"))
		return TRUE;
	else
		return FALSE;
}

char is_ast_dp_ram(ast_node_t *node)
{
	char *identifier = node->children[0]->types.identifier;
	if (!strcmp(identifier, "dual_port_ram"))
		return TRUE;
	else
		return FALSE;
}

sp_ram_signals *get_sp_ram_signals(nnode_t *node)
{
	oassert(is_sp_ram(node));

	ast_node_t *ast_node = node->related_ast_node;
	sp_ram_signals *signals = malloc(sizeof(sp_ram_signals));

	// Separate the input signals according to their mapping.
	signals->addr = init_signal_list();
	signals->data = init_signal_list();
	signals->out = init_signal_list();
	signals->we = NULL;
	signals->clk = NULL;

	int i;
	for (i = 0; i < node->num_input_pins; i++)
	{
		npin_t *pin = node->input_pins[i];
		if (!strcmp(pin->mapping, "addr"))
			add_pin_to_signal_list(signals->addr, pin);
		else if (!strcmp(pin->mapping, "data"))
			add_pin_to_signal_list(signals->data, pin);
		else if (!strcmp(pin->mapping, "we"))
			signals->we = pin;
		else if (!strcmp(pin->mapping, "clk"))
			signals->clk = pin;
		else
			error_message(NETLIST_ERROR, ast_node->line_number, ast_node->file_number,
					"Unexpected input pin mapping \"%s\" on memory node: %s\n",
					pin->mapping, node->name);
	}

	oassert(signals->clk != NULL);
	oassert(signals->we != NULL);
	oassert(signals->addr->count >= 1);
	oassert(signals->data->count >= 1);
	oassert(signals->data->count == node->num_output_pins);

	for (i = 0; i < node->num_output_pins; i++)
	{
		npin_t *pin = node->output_pins[i];
		if (!strcmp(pin->mapping, "out"))
			add_pin_to_signal_list(signals->out, pin);
		else
			error_message(NETLIST_ERROR, ast_node->line_number, ast_node->file_number,
					"Unexpected output pin mapping \"%s\" on memory node: %s\n",
					pin->mapping, node->name);
	}

	oassert(signals->out->count == signals->data->count);

	return signals;
}

void free_sp_ram_signals(sp_ram_signals *signals)
{
	free_signal_list(signals->data);
	free_signal_list(signals->addr);
	free_signal_list(signals->out);

	free(signals);
}

dp_ram_signals *get_dp_ram_signals(nnode_t *node)
{
	oassert(is_dp_ram(node));

	ast_node_t *ast_node = node->related_ast_node;
	dp_ram_signals *signals = malloc(sizeof(dp_ram_signals));

	// Separate the input signals according to their mapping.
	signals->addr1 = init_signal_list();
	signals->addr2 = init_signal_list();
	signals->data1 = init_signal_list();
	signals->data2 = init_signal_list();
	signals->out1  = init_signal_list();
	signals->out2  = init_signal_list();
	signals->we1 = NULL;
	signals->we2 = NULL;
	signals->clk = NULL;

	int i;
	for (i = 0; i < node->num_input_pins; i++)
	{
		npin_t *pin = node->input_pins[i];
		if (!strcmp(pin->mapping, "addr1"))
			add_pin_to_signal_list(signals->addr1, pin);
		else if (!strcmp(pin->mapping, "addr2"))
			add_pin_to_signal_list(signals->addr2, pin);
		else if (!strcmp(pin->mapping, "data1"))
			add_pin_to_signal_list(signals->data1, pin);
		else if (!strcmp(pin->mapping, "data2"))
			add_pin_to_signal_list(signals->data2, pin);
		else if (!strcmp(pin->mapping, "we1"))
			signals->we1 = pin;
		else if (!strcmp(pin->mapping, "we2"))
			signals->we2 = pin;
		else if (!strcmp(pin->mapping, "clk"))
			signals->clk = pin;
		else
			error_message(NETLIST_ERROR, ast_node->line_number, ast_node->file_number,
							"Unexpected input pin mapping \"%s\" on memory node: %s\n",
							pin->mapping, node->name);
	}

	// Sanity checks.
	oassert(signals->clk != NULL);
	oassert(signals->we1 != NULL && signals->we2 != NULL);
	oassert(signals->addr1->count >= 1 && signals->data1->count >= 1);
	oassert(signals->addr2->count >= 1 && signals->data2->count >= 1);
	oassert(signals->addr1->count == signals->addr2->count);
	oassert(signals->data1->count == signals->data2->count);
	oassert(signals->data1->count + signals->data2->count == node->num_output_pins);

	// Separate output signals according to mapping.
	for (i = 0; i < node->num_output_pins; i++)
	{
		npin_t *pin = node->output_pins[i];
		if (!strcmp(pin->mapping, "out1"))
			add_pin_to_signal_list(signals->out1, pin);
		else if (!strcmp(pin->mapping, "out2"))
			add_pin_to_signal_list(signals->out2, pin);
		else
			error_message(NETLIST_ERROR, ast_node->line_number, ast_node->file_number,
							"Unexpected output pin mapping \"%s\" on memory node: %s\n",
							pin->mapping, node->name);
	}

	oassert(signals->out1->count == signals->out2->count);
	oassert(signals->out1->count == signals->data1->count);

	return signals;
}

void free_dp_ram_signals(dp_ram_signals *signals)
{
	free_signal_list(signals->data1);
	free_signal_list(signals->data2);
	free_signal_list(signals->addr1);
	free_signal_list(signals->addr2);
	free_signal_list(signals->out1);
	free_signal_list(signals->out2);

	free(signals);
}

/*
 * Expands the given single port ram block into soft logic.
 */
void instantiate_soft_single_port_ram(nnode_t *node, short mark, netlist_t *netlist)
{
	oassert(is_sp_ram(node));

	sp_ram_signals *signals = get_sp_ram_signals(node);

	// Construct an address decoder.
	signal_list_t *decoder = create_decoder(node, mark, signals->addr);

	// The total number of memory addresses. (2^address_bits)
	int num_addr = decoder->count;

	int i;
	for (i = 0; i < signals->data->count; i++)
	{
		npin_t *data_pin = signals->data->pins[i];

		// The output multiplexer determines which memory cell is connected to the output register.
		nnode_t *output_mux = make_2port_gate(MULTI_PORT_MUX, num_addr, num_addr, 1, node, mark);

		int j;
		for (j = 0; j < num_addr; j++)
		{
			npin_t *address_pin = decoder->pins[j];

			// An AND gate to enable and disable writing.
			nnode_t *and = make_1port_logic_gate(LOGICAL_AND, 2, node, mark);
			if (!i) add_a_input_pin_to_node_spot_idx(and, decoder->pins[j], 0);
			else    add_a_input_pin_to_node_spot_idx(and, copy_input_npin(decoder->pins[j]), 0);

			if (!i && !j) remap_pin_to_new_node(signals->we, and, 1);
			else          add_a_input_pin_to_node_spot_idx(and, copy_input_npin(signals->we), 1);

			nnode_t *not = make_not_gate(node, mark);
			connect_nodes(and, 0, not, 0);

			// A multiplexer switches between accepting incoming data and keeping existing data.
			nnode_t *mux = make_2port_gate(MUX_2, 2, 2, 1, node, mark);
			connect_nodes(and, 0, mux, 0);
			connect_nodes(not,0,mux,1);
			if (!j) remap_pin_to_new_node(data_pin, mux, 2);
			else    add_a_input_pin_to_node_spot_idx(mux, copy_input_npin(data_pin), 2);

			// A flipflop holds the value of each memory cell.
			nnode_t *ff = make_2port_gate(FF_NODE, 1, 1, 1, node, mark);
			connect_nodes(mux, 0, ff, 0);
			if (!i && !j) remap_pin_to_new_node(signals->clk, ff, 1);
			else          add_a_input_pin_to_node_spot_idx(ff, copy_input_npin(signals->clk), 1);

			// The output of the flipflop connects back to the multiplexer (to hold the value.)
			connect_nodes(ff, 0, mux, 3);

			// The flipflop connects to the output multiplexer.
			connect_nodes(ff, 0, output_mux, num_addr + j);

			add_a_input_pin_to_node_spot_idx(output_mux, copy_input_npin(address_pin), j);
		}

		// Add the output register.
		nnode_t *output_ff = make_2port_gate(FF_NODE, 1, 1, 1, node, mark);
		connect_nodes(output_mux, 0, output_ff, 0);
		add_a_input_pin_to_node_spot_idx(output_ff, copy_input_npin(signals->clk), 1);

		// Move the original output pin over to the register.
		npin_t *output_pin = node->output_pins[i];
		output_pin->name = strdup(output_ff->name);
		//output_pin->net->name = strdup(output_ff->name);
		remap_pin_to_new_node(output_pin, output_ff, 0);
		instantiate_multi_port_mux(output_mux, mark, netlist);
	}

	// Free signal lists.
	free_sp_ram_signals(signals);
	free_signal_list(decoder);

	// Free the original hard block memory.
	free_nnode(node);
}

/*
 * Expands the given dual port ram block into soft logic.
 */
void instantiate_soft_dual_port_ram(nnode_t *node, short mark, netlist_t *netlist)
{
	oassert(is_dp_ram(node));

	dp_ram_signals *signals = get_dp_ram_signals(node);

	// Construct the address decoders.
	signal_list_t *decoder1 = create_decoder(node, mark, signals->addr1);
	signal_list_t *decoder2 = create_decoder(node, mark, signals->addr2);

	oassert(decoder1->count == decoder2->count);

	// The total number of memory addresses. (2^address_bits)
	int num_addr = decoder1->count;
	int data_width = signals->data1->count;

	int i;
	for (i = 0; i < data_width; i++)
	{
		npin_t *data1_pin = signals->data1->pins[i];
		npin_t *data2_pin = signals->data2->pins[i];

		// The output multiplexer determines which memory cell is connected to the output register.
		nnode_t *output_mux1 = make_2port_gate(MULTI_PORT_MUX, num_addr, num_addr, 1, node, mark);
		nnode_t *output_mux2 = make_2port_gate(MULTI_PORT_MUX, num_addr, num_addr, 1, node, mark);

		int j;
		for (j = 0; j < num_addr; j++)
		{
			npin_t *addr1_pin = decoder1->pins[j];
			npin_t *addr2_pin = decoder2->pins[j];

			nnode_t *and1 = make_1port_logic_gate(LOGICAL_AND, 2, node, mark);
			if (!i) add_a_input_pin_to_node_spot_idx(and1, addr1_pin, 0);
			else    add_a_input_pin_to_node_spot_idx(and1, copy_input_npin(addr1_pin), 0);

			if (!i && !j) remap_pin_to_new_node(signals->we1, and1, 1);
			else          add_a_input_pin_to_node_spot_idx(and1, copy_input_npin(signals->we1), 1);

			nnode_t *and2 = make_1port_logic_gate(LOGICAL_AND, 2, node, mark);
			if (!i) add_a_input_pin_to_node_spot_idx(and2, addr2_pin, 0);
			else    add_a_input_pin_to_node_spot_idx(and2, copy_input_npin(addr2_pin), 0);

			if (!i && !j) remap_pin_to_new_node(signals->we2, and2, 1);
			else          add_a_input_pin_to_node_spot_idx(and2, copy_input_npin(signals->we2), 1);

			// The data mux selects between the two data lines for this address.
			nnode_t *data_mux = make_2port_gate(MUX_2, 2, 2, 1, node, mark);
			// Port 2 before 1 to mimic the simulator's behaviour when the addresses are the same.
			connect_nodes(and2, 0, data_mux, 0);
			connect_nodes(and1, 0, data_mux, 1);
			if (!j) remap_pin_to_new_node(data2_pin, data_mux, 2);
			else    add_a_input_pin_to_node_spot_idx(data_mux, copy_input_npin(data2_pin), 2);
			if (!j) remap_pin_to_new_node(data1_pin, data_mux, 3);
			else    add_a_input_pin_to_node_spot_idx(data_mux, copy_input_npin(data1_pin), 3);

			// OR, to enable writing to this address when either port selects it for writing.
			nnode_t *or = make_1port_logic_gate(LOGICAL_OR, 2, node, mark);
			connect_nodes(and1, 0, or, 0);
			connect_nodes(and2, 0, or, 1);

			nnode_t *not = make_not_gate(node, mark);
			connect_nodes(or, 0, not, 0);

			// A multiplexer switches between accepting incoming data and keeping existing data.
			nnode_t *mux = make_2port_gate(MUX_2, 2, 2, 1, node, mark);
			connect_nodes(or, 0, mux, 0);
			connect_nodes(not, 0, mux, 1);
			connect_nodes(data_mux, 0, mux, 2);

			// A flipflop holds the value of each memory cell.
			nnode_t *ff = make_2port_gate(FF_NODE, 1, 1, 1, node, mark);
			connect_nodes(mux, 0, ff, 0);
			if (!i && !j) remap_pin_to_new_node(signals->clk, ff, 1);
			else          add_a_input_pin_to_node_spot_idx(ff, copy_input_npin(signals->clk), 1);

			// The output of the flipflop connects back to the multiplexer (to hold the value.)
			connect_nodes(ff, 0, mux, 3);

			// Connect the flipflop to both output muxes.
			connect_nodes(ff, 0, output_mux1, num_addr + j);
			connect_nodes(ff, 0, output_mux2, num_addr + j);

			// Connect address lines to the output muxes for this address.
			add_a_input_pin_to_node_spot_idx(output_mux1, copy_input_npin(addr1_pin), j);
			add_a_input_pin_to_node_spot_idx(output_mux2, copy_input_npin(addr2_pin), j);
		}

		// Add the output registers: for each data bit on each port.
		nnode_t *output_ff1 = make_2port_gate(FF_NODE, 1, 1, 1, node, mark);
		connect_nodes(output_mux1, 0, output_ff1, 0);
		add_a_input_pin_to_node_spot_idx(output_ff1, copy_input_npin(signals->clk), 1);

		nnode_t *output_ff2 = make_2port_gate(FF_NODE, 1, 1, 1, node, mark);
		connect_nodes(output_mux2, 0, output_ff2, 0);
		add_a_input_pin_to_node_spot_idx(output_ff2, copy_input_npin(signals->clk), 1);

		// Move the original outputs pin over to the register and rename them.
		npin_t *out1_pin = signals->out1->pins[i];
		npin_t *out2_pin = signals->out2->pins[i];

		out1_pin->name = strdup(output_ff1->name);
		out2_pin->name = strdup(output_ff2->name);

		remap_pin_to_new_node(out1_pin, output_ff1, 0);
		remap_pin_to_new_node(out2_pin, output_ff2, 0);

		// Convert the output muxes to MUX_2 nodes.
		instantiate_multi_port_mux(output_mux1, mark, netlist);
		instantiate_multi_port_mux(output_mux2, mark, netlist);
	}

	// Free signal lists.
	free_dp_ram_signals(signals);

	free_signal_list(decoder1);
	free_signal_list(decoder2);

	// Free the original hard block memory.
	free_nnode(node);
}

/*
 * Creates an n to 2^n decoder from the input signal list.
 */
signal_list_t *create_decoder(nnode_t *node, short mark, signal_list_t *input_list)
{
	int num_inputs = input_list->count;
	// Number of outputs is 2^num_inputs
	int num_outputs = 1 << num_inputs;

	// Create NOT gates for all inputs and put the outputs in their own signal list.
	signal_list_t *not_gates = init_signal_list();
	int i;
	for (i = 0; i < num_inputs; i++)
	{
		nnode_t *not = make_not_gate(node, mark);
		remap_pin_to_new_node(input_list->pins[i], not, 0);
		npin_t *not_output = allocate_npin();
		add_a_output_pin_to_node_spot_idx(not, not_output, 0);
		nnet_t *net = allocate_nnet();
		add_a_driver_pin_to_net(net, not_output);
		not_output = allocate_npin();
		add_a_fanout_pin_to_net(net, not_output);
		add_pin_to_signal_list(not_gates, not_output);

		npin_t *pin = allocate_npin();
		net = input_list->pins[i]->net;

		add_a_fanout_pin_to_net(net, pin);

		input_list->pins[i] = pin;
	}

	// Create AND gates and assign signals.
	signal_list_t *return_list = init_signal_list();
	for (i = 0; i < num_outputs; i++)
	{
		// Each output is connected to an and gate which is driven by a single permutation of the inputs.
		nnode_t *and = make_1port_logic_gate(LOGICAL_AND, num_inputs, node, mark);

		int j;
		for (j = 0; j < num_inputs; j++)
		{
			// Look at the jth bit of i. If it's 0, take the negated signal.
			int value = (i & (1 << j)) >> j;
			npin_t *pin = value ? input_list->pins[j] : not_gates->pins[j];

			// Use the original not pins on the first iteration and the original input pins on the last.
			if (i > 0 && i < num_outputs - 1) pin = copy_input_npin(pin);

			// Connect the signal to the output and gate.
			add_a_input_pin_to_node_spot_idx(and, pin, j);
		}

		// Add output pin, net, and fanout pin.
		npin_t *output = allocate_npin();
		nnet_t *net = allocate_nnet();
		add_a_output_pin_to_node_spot_idx(and, output, 0);
		add_a_driver_pin_to_net(net, output);
		output = allocate_npin();
		add_a_fanout_pin_to_net(net, output);

		// Add the fanout pin (decoder output) to the return list.
		add_pin_to_signal_list(return_list, output);
	}

	free_signal_list(not_gates);
	return return_list;
}
#endif
