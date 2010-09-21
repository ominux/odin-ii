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
#include "memories.h"

struct s_linked_vptr *sp_memory_list;
struct s_linked_vptr *dp_memory_list;
struct s_linked_vptr *split_list;
struct s_linked_vptr *memory_instances = NULL;
struct s_linked_vptr *memory_port_size_list = NULL;
int split_size = 0;

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
init_memory_distribution()
{
	return;
}

void 
report_memory_distribution()
{
	nnode_t *node;
	struct s_linked_vptr *temp;
	int i, idx, width, depth;

	if ((sp_memory_list == NULL) && (dp_memory_list == NULL))
		return;
	printf("\nHard Logical Memory Distribution\n");
	printf("============================\n");

	temp = sp_memory_list;
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
			}
			else if (strcmp("data1", node->input_pins[idx]->mapping) == 0)
			{
				width = node->input_port_sizes[i];
			}
			idx += node->input_port_sizes[i];
		}

		printf("DPRAM: %d width %d depth\n", width, depth);
		temp = temp->next;
	}
	
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
	int addr_port = -1;
	int we_port = -1;
	int clk_port = -1;
	int i, j, idx, addr_pin_idx, we_pin_idx;
	nnode_t *new_mem_node;
	nnode_t *and_node, *not_node, *ff_node, *mux_node;
	npin_t *addr_pin = NULL;
	npin_t *we_pin = NULL;
	npin_t *twe_pin, *taddr_pin, *clk_pin, *tdout_pin;

	oassert(node->type == MEMORY);

	/* Find which port is the addr port */
	idx = 0;
	for (i = 0; i < node->num_input_port_sizes; i++)
	{
		if (strcmp("addr", node->input_pins[idx]->mapping) == 0)
		{
			addr_port = i;
			addr_pin_idx = idx;
			addr_pin = node->input_pins[idx];
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
	
	/* Check that the memory needs to be split */
	if (node->input_port_sizes[addr_port] <= split_size) {
		sp_memory_list = insert_in_vptr_list(sp_memory_list, node);
		return;
	}

	/* Let's remove the address line from the memory */
	for (i = addr_pin_idx; i < node->num_input_pins - 1; i++)
	{
		node->input_pins[i] = node->input_pins[i+1];
		node->input_pins[i]->pin_node_idx--;
	}
	node->input_port_sizes[addr_port]--;
	node->num_input_pins--;
	if (we_pin_idx >= addr_pin_idx)
		we_pin_idx--;

	/* Create the new memory node */
	new_mem_node = allocate_nnode();
	new_mem_node->name = (char *)malloc(strlen(node->name) + 10);
	strcpy(new_mem_node->name, node->name);
	strcat(new_mem_node->name, "__H");

	/* Copy properties from the original memory node */
	new_mem_node->type = node->type;
	new_mem_node->related_ast_node = node->related_ast_node;
	new_mem_node->traverse_visited = node->traverse_visited;
	new_mem_node->node_data = NULL;
	new_mem_node->num_output_pins = node->num_output_pins;
	new_mem_node->output_pins = (struct npin_t_t **)malloc(node->num_output_pins * sizeof(struct npin_t_t **));
	new_mem_node->output_port_sizes = (int *)malloc(sizeof(int));
	new_mem_node->output_port_sizes[0] = node->output_port_sizes[0];
	for (i = 0; i < new_mem_node->num_output_pins; i++)
		new_mem_node->output_pins[i] = NULL;

	new_mem_node->num_input_port_sizes = node->num_input_port_sizes;
	new_mem_node->input_port_sizes = (int *)malloc(node->num_input_port_sizes * sizeof(int));
	new_mem_node->input_pins = (struct npin_t_t **)malloc(node->num_input_pins * sizeof(struct npin_t_t **));
	new_mem_node->num_input_pins = node->num_input_pins; //- 1;

	/* Copy over the pin sizes for the new memory */
	for (j = 0; j < new_mem_node->num_input_port_sizes; j++)
	{
		new_mem_node->input_port_sizes[j] = node->input_port_sizes[j];
	}

	/* Copy over the pins for the new memory */
	for (j = 0; j < node->num_input_pins; j++)
	{
		new_mem_node->input_pins[j] = copy_input_npin(node->input_pins[j]);
	}

	and_node = make_2port_gate(LOGICAL_AND, 1, 1, 1, node, node->traverse_visited);
	twe_pin = copy_input_npin(we_pin);
	add_a_input_pin_to_node_spot_idx(and_node, twe_pin, 1);
	taddr_pin = copy_input_npin(addr_pin);
	add_a_input_pin_to_node_spot_idx(and_node, taddr_pin, 0);
	connect_nodes(and_node, 0, node, we_pin_idx);
	node->input_pins[we_pin_idx]->mapping = we_pin->mapping;

	taddr_pin = copy_input_npin(addr_pin);
	not_node = make_not_gate_with_input(taddr_pin, new_mem_node, new_mem_node->traverse_visited);
	and_node = make_2port_gate(LOGICAL_AND, 1, 1, 1, new_mem_node, new_mem_node->traverse_visited);
	connect_nodes(not_node, 0, and_node, 0);
	add_a_input_pin_to_node_spot_idx(and_node, we_pin, 1);
	connect_nodes(and_node, 0, new_mem_node, we_pin_idx);
	new_mem_node->input_pins[we_pin_idx]->mapping = we_pin->mapping;

	ff_node = make_2port_gate(FF_NODE, 1, 1, 1, node, node->traverse_visited);
	add_a_input_pin_to_node_spot_idx(ff_node, addr_pin, 0);
	add_a_input_pin_to_node_spot_idx(ff_node, copy_input_npin(clk_pin), 1);

	
	/* Copy over the output pins for the new memory */
	for (j = 0; j < node->num_output_pins; j++)
	{
		mux_node = make_2port_gate(MULTI_PORT_MUX, 2, 2, 1, node, node->traverse_visited);
		connect_nodes(ff_node, 0, mux_node, 0);
		not_node = make_not_gate(node, node->traverse_visited);
		connect_nodes(ff_node, 0, not_node, 0);
		connect_nodes(not_node, 0, mux_node, 1);
		tdout_pin = node->output_pins[j];
		remap_pin_to_new_node(tdout_pin, mux_node, 0);
		connect_nodes(node, j, mux_node, 3);
		node->output_pins[j]->mapping = tdout_pin->mapping;
		connect_nodes(new_mem_node, j, mux_node, 2);
		new_mem_node->output_pins[j]->mapping = tdout_pin->mapping;
		tdout_pin->mapping = NULL;
	}

	/* must recurse on new memory if it's too small */
	if (node->input_port_sizes[addr_port] <= split_size) {
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
	int clk_port = -1;
	int i, j, idx, addr1_pin_idx, we1_pin_idx, addr2_pin_idx, we2_pin_idx;
	nnode_t *new_mem_node;
	nnode_t *and1_node, *not1_node, *ff1_node, *mux1_node;
	nnode_t *and2_node, *not2_node, *ff2_node, *mux2_node;
	npin_t *addr1_pin = NULL;
	npin_t *addr2_pin = NULL;
	npin_t *we1_pin = NULL;
	npin_t *we2_pin = NULL;
	npin_t *twe_pin, *taddr_pin, *clk_pin, *tdout_pin;

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
			clk_port = i;
			clk_pin = node->input_pins[idx];
		}
		idx += node->input_port_sizes[i];
	}

	if (addr1_port == -1)
	{
		error_message(1, 0, -1, "No \"addr1\" port on dual port RAM");
	}
	
	/* Check that the memory needs to be split */
	if (node->input_port_sizes[addr1_port] <= split_size) {
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
	node->num_input_pins--;
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
		node->num_input_pins--;
		if ((we1_port != -1) && (we1_pin_idx >= addr2_pin_idx))
			we1_pin_idx--;
		if ((we2_port != -1) && (we2_pin_idx >= addr2_pin_idx))
			we2_pin_idx--;
		if (addr1_pin_idx >= addr2_pin_idx)
			addr1_pin_idx--;
	}

	/* Create the new memory node */
	new_mem_node = allocate_nnode();
	new_mem_node->name = (char *)malloc(strlen(node->name) + 10);
	strcpy(new_mem_node->name, node->name);
	strcat(new_mem_node->name, "__H");

	/* Copy properties from the original memory node */
	new_mem_node->type = node->type;
	new_mem_node->related_ast_node = node->related_ast_node;
	new_mem_node->traverse_visited = node->traverse_visited;
	new_mem_node->node_data = NULL;

	new_mem_node->num_output_pins = node->num_output_pins;
	new_mem_node->num_output_port_sizes = node->num_output_port_sizes;
	new_mem_node->output_pins = (struct npin_t_t **)malloc(sizeof(struct npin_t_t **) * new_mem_node->num_output_pins);
	for (i = 0; i < new_mem_node->num_output_pins; i++)
		new_mem_node->output_pins[i] = NULL;
	new_mem_node->output_port_sizes = (int *)malloc(sizeof(int) * new_mem_node->num_output_port_sizes);
	for (i = 0; i < new_mem_node->num_output_port_sizes; i++)
		new_mem_node->output_port_sizes[i] = node->output_port_sizes[i];
	new_mem_node->num_input_port_sizes = node->num_input_port_sizes;
	new_mem_node->input_port_sizes = (int *)malloc(node->num_input_port_sizes * sizeof(int));
	new_mem_node->input_pins = (struct npin_t_t **)malloc(node->num_input_pins * sizeof(struct npin_t_t **));

	/* KEN - IS THIS AN ERROR? */
	new_mem_node->num_input_pins = node->num_input_pins; // jluu yes -1; is an error, removed

	/* Copy over the pin sizes for the new memory */
	for (j = 0; j < new_mem_node->num_input_port_sizes; j++)
	{
		new_mem_node->input_port_sizes[j] = node->input_port_sizes[j];
	}

	/* Copy over the pins for the new memory */
	for (j = 0; j < node->num_input_pins; j++)
	{
		new_mem_node->input_pins[j] = copy_input_npin(node->input_pins[j]);
	}

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
		for (j = 0; j < node->num_output_pins; j++)
		{
			mux1_node = make_2port_gate(MULTI_PORT_MUX, 2, 2, 1, node, node->traverse_visited);
			connect_nodes(ff1_node, 0, mux1_node, 0);
			not1_node = make_not_gate(node, node->traverse_visited);
			connect_nodes(ff1_node, 0, not1_node, 0);
			connect_nodes(not1_node, 0, mux1_node, 1);
			tdout_pin = node->output_pins[j];
			remap_pin_to_new_node(tdout_pin, mux1_node, 0);
			connect_nodes(node, 0, mux1_node, 3);
			node->output_pins[j]->mapping = tdout_pin->mapping;
			connect_nodes(new_mem_node, 0, mux1_node, 2);
			new_mem_node->output_pins[j]->mapping = tdout_pin->mapping;
			tdout_pin->mapping = NULL;
		}
	}

	if (node->num_output_pins > 1) /* There is an "out2" output */
	{
		ff2_node = make_2port_gate(FF_NODE, 1, 1, 1, node, node->traverse_visited);
		add_a_input_pin_to_node_spot_idx(ff2_node, addr2_pin, 0);
		add_a_input_pin_to_node_spot_idx(ff2_node, copy_input_npin(clk_pin), 1);
	
		/* Copy over the output pins for the new memory */
		for (j = 0; j < node->num_output_pins; j++)
		{
			mux2_node = make_2port_gate(MULTI_PORT_MUX, 2, 2, 1, node, node->traverse_visited);
			connect_nodes(ff2_node, 0, mux2_node, 0);
			not2_node = make_not_gate(node, node->traverse_visited);
			connect_nodes(ff2_node, 0, not2_node, 0);
			connect_nodes(not2_node, 0, mux2_node, 1);
			tdout_pin = node->output_pins[node->output_port_sizes[0] + j];
			remap_pin_to_new_node(tdout_pin, mux2_node, 0);
			connect_nodes(node, 1, mux2_node, 3);
			node->output_pins[node->output_port_sizes[0] + j]->mapping = tdout_pin->mapping;
			connect_nodes(new_mem_node, 1, mux2_node, 2);
			new_mem_node->output_pins[node->output_port_sizes[0] + j]->mapping = tdout_pin->mapping;
			tdout_pin->mapping = NULL;
		}
	}

	/* must recurse on new memory if it's too small */
	if (node->input_port_sizes[addr1_port] <= split_size) {
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
	char *tmp_name;

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
	tmp_name = (char *)malloc(strlen(node->name) + 3);
	strcpy(tmp_name, node->name);
	strcat(tmp_name, "-0");
	free(node->name);
	node->name = tmp_name;

	/* free the additional output pins */
	for (i = 1; i < node->num_output_pins; i++)
		free_npin(node->output_pins[i]);
	node->num_output_pins = 1;
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
	nnet_t *data1_net, *data2_net;

	oassert(node->type == MEMORY);

	/* Find which port is the data port on the input! */
	idx = 0;
	data_port1 = -1;
	data_port2 = -1;
	data_diff1 = 0;
	data_diff2 = 0;
	data1_net = node->output_pins[0]->net;
	if (node->num_output_port_sizes > 1)
		data2_net = node->output_pins[node->output_port_sizes[0]]->net;
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
//			new_node->output_pins[0] = copy_output_npin(node->output_pins[i]);
//			add_a_driver_pin_to_net(data1_net, new_node->output_pins[0]);
//			free_npin(node->output_pins[i]);
			new_node->output_pins[0] = node->output_pins[i];
			node->output_pins[i] = NULL;
			new_node->output_pins[0]->pin_node_idx = 0;
			new_node->output_pins[0]->node = new_node;
		}
		else
		{
			new_node->num_output_pins = 2;
			new_node->output_pins = (npin_t **)malloc(sizeof(void*)*2);
//			new_node->output_pins[0] = copy_output_npin(node->output_pins[i]);
//			new_node->output_pins[1] = copy_output_npin(node->output_pins[i+data_diff1+1]);
//			add_a_driver_pin_to_net(data1_net, new_node->output_pins[0]);
//			free_npin(node->output_pins[i]);
			new_node->output_pins[0] = node->output_pins[i];
			node->output_pins[i] = NULL;
			new_node->output_pins[0]->pin_node_idx = 0;
			new_node->output_pins[0]->node = new_node;

//			add_a_driver_pin_to_net(data2_net, new_node->output_pins[1]);
//			free_npin(node->output_pins[i+data_diff1+1]);
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
//		for (i = 1; i < node->num_output_pins; i++)
//			free_npin(node->output_pins[i]);
		node->num_output_pins = 1;
		node->output_port_sizes[0] = 1;
	}
	else
	{
//		for (i = 1; i < (node->num_output_pins/2); i++)
//			free_npin(node->output_pins[i]);
		node->output_pins[1] = node->output_pins[data_diff1 + 1];
		node->output_pins[data_diff1 + 1] = NULL;
		node->output_pins[1]->pin_node_idx = 1;
//		for (; i < node->num_output_pins; i++)
//			free_npin(node->output_pins[i]);
		node->num_output_pins = 2;
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
	dp_memory_list = insert_in_vptr_list(dp_memory_list, node);

	return;
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
	if (configuration.split_memory_depth != 0)
	{
		/* Jason Luu: HACK detected: split_size should NOT come from configuration.split_memory_depth, 
		   it should come from the maximum model size for the port, IMPORTANT TODO!!!! */
		split_size = configuration.split_memory_depth;

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
	if (configuration.split_memory_width == 1)
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

	return;
}

/*-------------------------------------------------------------------------
 * (function: clean_memories)
 *
 * Clean up the memory by deleting the list structure of memories
 *      during optimization.
 *-----------------------------------------------------------------------*/
void 
clean_memories()
{
	while (sp_memory_list != NULL)
		sp_memory_list = delete_in_vptr_list(sp_memory_list);
	while (dp_memory_list != NULL)
		dp_memory_list = delete_in_vptr_list(dp_memory_list);
	return;
}

