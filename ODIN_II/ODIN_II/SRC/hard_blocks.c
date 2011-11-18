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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "errors.h"
#include "types.h"
#include "globals.h"
#include "hard_blocks.h"
#include "memories.h"

STRING_CACHE *hard_block_names = NULL;

#ifdef VPR6
void register_hard_blocks()
{
	t_model *hard_blocks = NULL;
	t_model_ports *hb_ports = NULL;

	hard_blocks = Arch.models;
	hard_block_names = sc_new_string_cache();
	while (hard_blocks != NULL)
	{
		sc_add_string(hard_block_names, hard_blocks->name);
		/* Set the size of data inputs/outputs on single/dual port ram 
		 * blocks to 1 to ensure that all memories are split to 
		 * single bit width
		 */
		if (strcmp(hard_blocks->name, "single_port_ram") == 0)
		{
			hard_blocks->outputs->size = 1;

			/* Need to determine the split size based on min, max, or fixed */
			if (configuration.split_memory_depth == 1)
			{
				hb_ports = hard_blocks->inputs;
				while ((hb_ports != NULL) && (strcmp(hb_ports->name, "addr") != 0))
					hb_ports = hb_ports->next;
				if (hb_ports == NULL)
					error_message(1, 0, -1, "No \"addr\" port on single port RAM");

				if (configuration.split_memory_depth == -1) /* MIN */
					split_size = hb_ports->min_size;
				if (configuration.split_memory_depth == -2) /* MAX */
					split_size = hb_ports->size;
				else /* FIXED */
					split_size = configuration.split_memory_depth;
			}

			hb_ports = hard_blocks->inputs;
			while ((hb_ports != NULL) && (strcmp(hb_ports->name, "data") != 0))
				hb_ports = hb_ports->next;
			if (hb_ports != NULL)
				hb_ports->size = 1;
		}

		if (strcmp(hard_blocks->name, "dual_port_ram") == 0)
		{
			hb_ports = hard_blocks->outputs;
			hb_ports->size = 1;
			if (hb_ports->next != NULL)
				hb_ports->next->size = 1;
			hb_ports = hard_blocks->inputs;
			while (hb_ports != NULL)
			{
				if ((strcmp(hb_ports->name, "data1") == 0) ||
				  (strcmp(hb_ports->name, "data2") == 0))
					hb_ports->size = 1;
				hb_ports = hb_ports->next;
			}
		}
		hard_blocks = hard_blocks->next;
	}
	return;
}

void deregister_hard_blocks()
{
	sc_free_string_cache(hard_block_names);
	return;
}

t_model* find_hard_block(char *name)
{
	t_model *hard_blocks;

	hard_blocks = Arch.models;
	while (hard_blocks != NULL)
		if (strcmp(hard_blocks->name, name) == 0)
			return hard_blocks;
		else
			hard_blocks = hard_blocks->next;

	return NULL;
}

void define_hard_block(nnode_t *node, short type, FILE *out)
{
	int i, j;
	int index, port;
	int count;
	char buffer[MAX_BUF];

	/* Assert that every hard block has at least an input and output */
	oassert(node->input_port_sizes[0] > 0);
	oassert(node->output_port_sizes[0] > 0);

	count = fprintf(out, "\n.subckt ");
	count--;
	count += fprintf(out, "%s", node->related_ast_node->children[0]->types.identifier);

	/* print the input port mappings */
	port = index = 0;
	for (i = 0;  i < node->num_input_pins; i++)
	{
		if (node->input_port_sizes[port] == 1)
			j = sprintf(buffer, " %s=%s", node->input_pins[i]->mapping, node->input_pins[i]->net->driver_pin->node->name);
		else
		{
			if (node->input_pins[i]->net->driver_pin->name != NULL)
				j = sprintf(buffer, " %s[%d]=%s", node->input_pins[i]->mapping, index, node->input_pins[i]->net->driver_pin->name);
			else
				j = sprintf(buffer, " %s[%d]=%s", node->input_pins[i]->mapping, index, node->input_pins[i]->net->driver_pin->node->name);
		}

		if (count + j > 79)
		{
			fprintf(out, "\\\n");
			count = 0;
		}
		count += fprintf(out, "%s", buffer);

		index++;
		if (node->input_port_sizes[port] == index)
		{
			index = 0;
			port++;
		}
	}

	/* print the output port mappings */
	port = index = 0;
	for (i = 0; i < node->num_output_pins; i++)
	{
		if (node->output_port_sizes[port] != 1)
			j = sprintf(buffer, " %s[%d]=%s", node->output_pins[i]->mapping, index, node->output_pins[i]->name);
		else
			j = sprintf(buffer, " %s=%s", node->output_pins[i]->mapping, node->output_pins[i]->name);

		if (count + j > 79)
		{
			fprintf(out, "\\\n");
			count = 0;
		}
		count += fprintf(out, "%s", buffer);

		index++;
		if (node->output_port_sizes[port] == index)
		{
			index = 0;
			port++;
		}
	}

	count += fprintf(out, "\n\n");
	return;
}

void output_hard_blocks(FILE *out)
{
	t_model_ports *hb_ports;
	t_model *hard_blocks;
	char buffer[MAX_BUF];
	int count;
	int i;

	oassert(out != NULL);
	hard_blocks = Arch.models;
	while (hard_blocks != NULL)
	{
		if (hard_blocks->used == 1) /* Hard Block is utilized */
		{
			fprintf(out, "\n.model %s\n", hard_blocks->name);
			count = fprintf(out, ".inputs");
			hb_ports = hard_blocks->inputs;
			while (hb_ports != NULL)
			{
				for (i = 0; i < hb_ports->size; i++)
				{
					if (hb_ports->size == 1)
						count = count + sprintf(buffer, " %s", hb_ports->name);
					else
						count = count + sprintf(buffer, " %s[%d]", hb_ports->name, i);

					if (count >= 78)
						count = fprintf(out, " \\\n%s", buffer) - 3;
					else
						fprintf(out, "%s", buffer);
				}
				hb_ports = hb_ports->next;
			}

			count = fprintf(out, "\n.outputs") - 1;
			hb_ports = hard_blocks->outputs;
			while (hb_ports != NULL)
			{
				for (i = 0; i < hb_ports->size; i++)
				{
					if (hb_ports->size == 1)
						count = count + sprintf(buffer, " %s", hb_ports->name);
					else
						count = count + sprintf(buffer, " %s[%d]", hb_ports->name, i);

					if (count >= 78)
						count = fprintf(out, " \\\n%s", buffer) - 3;
					else
						fprintf(out, "%s", buffer);
				}
				hb_ports = hb_ports->next;
			}

			fprintf(out, "\n.blackbox\n.end\n\n");
		}
		hard_blocks = hard_blocks->next;
	}

	return;
}

void
instantiate_hard_block(nnode_t *node, short mark, netlist_t *netlist)
{
	int i, port, index;

	port = index = 0;
	/* Give names to the output pins */
	for (i = 0; i < node->num_output_pins;  i++)
	{
		if (node->output_pins[i]->name == NULL)
			node->output_pins[i]->name = make_full_ref_name(node->name, NULL, NULL, node->output_pins[i]->mapping, -1);

		index++;
		if (node->output_port_sizes[port] == index)
		{
			index = 0;
			port++;
		}
	}

	node->traverse_visited = mark;
	return;
}

int
hard_block_port_size(t_model *hb, char *pname)
{
	t_model_ports *tmp;

	if (hb == NULL)
		return 0;

	/* Indicates that the port size is different for this hard block
	 *  depending on the instance of the hard block. May want to extend
	 *  this list of blocks in the future.
	 */
	if ((strcmp(hb->name, "single_port_ram") == 0) ||
		(strcmp(hb->name, "dual_port_ram") == 0))
	{
		return -1;
	}

	tmp = hb->inputs;
	while (tmp != NULL)
		if ((tmp->name != NULL) && (strcmp(tmp->name, pname) == 0))
			return tmp->size;
		else
			tmp = tmp->next;

	tmp = hb->outputs;
	while (tmp != NULL)
		if ((tmp->name != NULL) && (strcmp(tmp->name, pname) == 0))
			return tmp->size;
		else
			tmp = tmp->next;

	return 0;
}

enum PORTS
hard_block_port_direction(t_model *hb, char *pname)
{
	t_model_ports *tmp;

	if (hb == NULL)
		return ERR_PORT;

	tmp = hb->inputs;
	while (tmp != NULL)
		if ((tmp->name != NULL) && (strcmp(tmp->name, pname) == 0))
			return tmp->dir;
		else
			tmp = tmp->next;

	tmp = hb->outputs;
	while (tmp != NULL)
		if ((tmp->name != NULL) && (strcmp(tmp->name, pname) == 0))
			return tmp->dir;
		else
			tmp = tmp->next;

	return ERR_PORT;
}
#endif
