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

#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include "globals.h"
#include "read_blif.h"
#include "util.h"
#include "string_cache.h"
#include "netlist_utils.h"
#include "errors.h"
#include "netlist_utils.h"
#include "types.h"
#include "hashtable.h"
#include "netlist_check.h"

#define TOKENS " \t\n"

static FILE * blif;
int linenum;

STRING_CACHE *output_nets_sc;
STRING_CACHE *input_nets_sc;

char *GND ="gnd";
char *VCC="vcc";
char *HBPAD="unconn";

char *blif_one_string = "ONE_VCC_CNS";
char *blif_zero_string = "ZERO_GND_ZERO";
char *blif_pad_string = "ZERO_PAD_ZERO";
char *default_clock_name="top^clock"; 


typedef struct {
	int count;
	char **names;
	hashtable_t *index;
} hard_block_pins;

typedef struct {
	int count;
	int *sizes;
	char **names;
	hashtable_t *index;
} hard_block_ports;

typedef struct {
	hard_block_pins *inputs;
	hard_block_pins *outputs;

	hard_block_ports *input_ports;
	hard_block_ports *output_ports;
} hard_block_model;

//netlist_t * verilog_netlist;
int linenum;/* keeps track of the present line, used for printing the error line : line number */
short static skip_reading_bit_map=FALSE; 

void rb_create_top_driver_nets( char *instance_name_prefix);
void rb_look_for_clocks();// not sure if this is needed
void add_top_input_nodes();
void rb_create_top_output_nodes();
static void read_tokens (char *buffer,int * done,char * blif_file);
static void dum_parse ( char *buffer); 
void create_internal_node_and_driver();
short assign_node_type_from_node_name(char * output_name);// function will decide the node->type of the given node
short read_bit_map_find_unknown_gate(int input_count,nnode_t * node);
void create_latch_node_and_driver(char * blif_file);
void create_hard_block_nodes();
void hook_up_nets();
void hook_up_node(nnode_t *node);
char* search_clock_name(char * blif_file);
hard_block_model *read_hard_block_model(char *name_subckt, FILE *file);
void free_hard_block_model(hard_block_model *model);
char *get_hard_block_port_name(char *name);
long get_hard_block_pin_number(char *original_name);
static int compare_pin_names(const void *p1, const void *p2);
hard_block_ports *get_hard_block_ports(char **pins, int count);
hashtable_t *index_names(char **names, int count);
hashtable_t *associate_names(char **names1, char **names2, int count);
void verify_hard_block_ports_against_model(hard_block_ports *ports, hard_block_model *model);
void free_hard_block_pins(hard_block_pins *p);
void free_hard_block_ports(hard_block_ports *p);

void read_blif(char * blif_file)
{
	/* Initialise the string caches that hold the aliasing of module nets and input pins */
	output_nets_sc = sc_new_string_cache();
	input_nets_sc = sc_new_string_cache();

	verilog_netlist = allocate_netlist();
	/*Opening the blif file */
	blif = my_fopen (blif_file, "r", 0);

	printf("Reading top level module\n"); fflush(stdout);

	/* create the top level module */
	rb_create_top_driver_nets("top");

	/* Extracting the netlist by reading the blif file */
	printf("Reading blif netlist..."); fflush(stdout);

	int num_lines = 0;
	linenum = 0;
	int done = 0;

	char buffer[BUFSIZE];
	while (my_fgets(buffer, BUFSIZE, blif))
	{
		if (strstr(buffer, ".end")) break;
		num_lines++;
	}

	rewind(blif);

	int line_count = 0;
	int position = -1;
	double time = wall_time();
	printf("\n");
	while (!done && (my_fgets(buffer, BUFSIZE, blif) != NULL))
	{
		read_tokens(buffer, &done, blif_file);/* read one token at a time */
		linenum = line_count++;
		position = print_progress_bar(line_count/(double)num_lines, position, 50, wall_time() - time);
	}
	print_progress_bar(1.0, position, 50, wall_time() - time);

	printf("\n");
	check_netlist(verilog_netlist);

	fclose (blif);
	
	printf("Looking for clocks\n"); fflush(stdout);

	/* Now look for high-level signals */
	rb_look_for_clocks();
	printf("-------------------------------------\n"); fflush(stdout);
}

/*---------------------------------------------------------------------------------------------
 * (function: read_tokens)
 *-------------------------------------------------------------------------------------------*/
void read_tokens (char *buffer, int *done, char *blif_file)
{
	/* Figures out which, if any token is at the start of this line and *
	 * takes the appropriate action.                                    */
	char *ptr = my_strtok (buffer, TOKENS, blif, buffer);

	if (ptr)
	{
		if(skip_reading_bit_map && !ptr && ((ptr[0] == '0') || (ptr[0] == '1') || (ptr[0] == '-')))
		{
			dum_parse(buffer);
		}
		else
		{
			skip_reading_bit_map= FALSE;
			if (strcmp (ptr, ".inputs") == 0)
			{
				add_top_input_nodes();// create the top input nodes
			}
			else if (strcmp (ptr, ".outputs") == 0)
			{
				rb_create_top_output_nodes();// create the top output nodes
			}
			else if (strcmp (ptr, ".names") == 0)
			{
				create_internal_node_and_driver();
			}
			else if (strcmp(ptr,".latch") == 0)
			{
				create_latch_node_and_driver(blif_file);
			}
			else if (strcmp(ptr,".subckt") == 0)
			{
				create_hard_block_nodes();
			}
			else if (strcmp(ptr,".end")==0)
			{
				/* marks the end of the main module of the blif */
				/* call functions to hook up the nets */
				hook_up_nets();
				*done=1;
			}
			else if (strcmp(ptr,".model")==0)
			{
				/* not needed for now */
				dum_parse(buffer);
			}
		}
	}
}


/*---------------------------------------------------------------------------------------------
   * function:assign_node_type_from_node_name(char *)
     This function tries to assign the node->type by looking at the name
     Else return GENERIC
*-------------------------------------------------------------------------------------------*/
short assign_node_type_from_node_name(char * output_name)
{
	//variable to extract the type
	int start, end;
	int length_string = strlen(output_name);
	for(start = length_string-1; (start >= 0) && (output_name[start] != '^'); start--);
	for(end   = length_string-1; (end   >= 0) && (output_name[end]   != '~'); end--  );

	if((start >= end) || (end == 0)) return GENERIC;

	// Stores the extracted string
	char *extracted_string = (char*)malloc(sizeof(char)*((end-start+2)));

	int j=0;
	int i;
	for(i = start + 1; i < end; i++)
	{
		extracted_string[j] = output_name[i];
		j++;
	}

	extracted_string[j]='\0';

	if(strcmp(extracted_string,"GT")==0)
		return GT;
	else if(strcmp(extracted_string,"LT")==0)
		return LT;
	else if(strcmp(extracted_string,"ADDER_FUNC")==0)
		return ADDER_FUNC;
	else if(strcmp(extracted_string,"CARRY_FUNC")==0)
		return CARRY_FUNC;
	else if(strcmp(extracted_string,"BITWISE_NOT")==0)
		return BITWISE_NOT;
	else if(strcmp(extracted_string,"LOGICAL_AND")==0)
		return LOGICAL_AND;
	else if(strcmp(extracted_string,"LOGICAL_OR")==0)
		return LOGICAL_OR;
	else if(strcmp(extracted_string,"LOGICAL_XOR")==0)
		return LOGICAL_XOR;
	else if(strcmp(extracted_string,"LOGICAL_XNOR")==0)
		return LOGICAL_XNOR;
	else if(strcmp(extracted_string,"LOGICAL_NAND")==0)
		return LOGICAL_NAND;
	else if(strcmp(extracted_string,"LOGICAL_NOR")==0)
		return LOGICAL_NOR;
	else if(strcmp(extracted_string,"LOGICAL_EQUAL")==0)
		return LOGICAL_EQUAL;
	else if(strcmp(extracted_string,"NOT_EQUAL")==0)
		return NOT_EQUAL;
	else if(strcmp(extracted_string,"LOGICAL_NOT")==0)
		return LOGICAL_NOT;
	else if(strcmp(extracted_string,"MUX_2")==0)
		return MUX_2;
	else if(strcmp(extracted_string,"FF_NODE")==0)
		return FF_NODE;
	else if(strcmp(extracted_string,"MULTIPLY")==0)
		return MULTIPLY;
	else if(strcmp(extracted_string,"HARD_IP")==0)
		return HARD_IP;
	else if(strcmp(extracted_string,"INPUT_NODE")==0)
		return INPUT_NODE;
	else if(strcmp(extracted_string,"OUTPUT_NODE")==0)
		return OUTPUT_NODE;
	else if(strcmp(extracted_string,"PAD_NODE")==0)
		return PAD_NODE;
	else if(strcmp(extracted_string,"CLOCK_NODE")==0)
		return CLOCK_NODE;
	else if(strcmp(extracted_string,"GND_NODE")==0)
		return GND_NODE;
	else if(strcmp(extracted_string,"VCC_NODE")==0)
		return VCC_NODE;
	else if(strcmp(extracted_string,"BITWISE_AND")==0)
		return BITWISE_AND;
	else if(strcmp(extracted_string,"BITWISE_NAND")==0)
		return BITWISE_NAND;
	else if(strcmp(extracted_string,"BITWISE_NOR")==0)
		return BITWISE_NOR;
	else if(strcmp(extracted_string,"BITWISE_XNOR")==0)
		return BITWISE_XNOR;
	else if(strcmp(extracted_string,"BITWISE_XOR")==0)
		return BITWISE_XOR;
	else if(strcmp(extracted_string,"BITWISE_OR")==0)
		return BITWISE_OR;
	else if(strcmp(extracted_string,"BUF_NODE")==0)
		return BUF_NODE;
	else if(strcmp(extracted_string,"MULTI_PORT_MUX")==0)
		return MULTI_PORT_MUX;
	else if(strcmp(extracted_string,"SL")==0)
		return SL;
	else if(strcmp(extracted_string,"SR")==0)
		return SR;
	else if(strcmp(extracted_string,"CASE_EQUAL")==0)
		return CASE_EQUAL;
	else if(strcmp(extracted_string,"CASE_NOT_EQUAL")==0)
		return CASE_NOT_EQUAL;
	else if(strcmp(extracted_string,"DIVIDE")==0)
		return DIVIDE;
	else if(strcmp(extracted_string,"MODULO")==0)
		return MODULO;
	else if(strcmp(extracted_string,"GTE")==0)
		return GTE;
	else if(strcmp(extracted_string,"LTE")==0)
		return LTE;
	else if(strcmp(extracted_string,"ADD")==0)
		return ADD;
	else if(strcmp(extracted_string,"MINUS")==0)
		return MINUS;
	else
		return GENERIC; /* If name type does not exits */
}

/*---------------------------------------------------------------------------------------------
   * function:create_latch_node_and_driver
     to create an ff node and driver from that node 
     format .latch <input> <output> [<type> <control/clock>] <initial val>
*-------------------------------------------------------------------------------------------*/
void create_latch_node_and_driver(char * blif_file)
{
	/* Storing the names of the input and the final output in array names */
	char ** names = NULL;       // Store the names of the tokens
	int input_token_count = 0; /*to keep track whether controlling clock is specified or not */
	/*input_token_count=3 it is not and =5 it is */
	char *ptr;
	char buffer[BUFSIZE];
	while ((ptr = my_strtok (NULL, TOKENS, blif, buffer)) != NULL)
	{
		names = (char**)realloc(names, sizeof(char*)* (input_token_count + 1));
		names[input_token_count++] = strdup(ptr);
	}

	/* assigning the new_node */
	if(input_token_count != 5)
	{
		/* supported added for the ABC .latch output without control */
		if(input_token_count == 3)
		{
			char *clock_name = search_clock_name(blif_file);
			input_token_count = 5;
			names = (char**)realloc(names, sizeof(char*) * input_token_count);

			if(clock_name) names[3] = strdup(clock_name);
			else           names[3] = NULL;

			names[4] = strdup(names[2]);
			names[2] = "re";
		}
		else
		{	
			error_message(NETLIST_ERROR,linenum,-1, "This .latch Format not supported \n\t required format :.latch <input> <output> [<type> <control/clock>] <initial val>");
		}
	}

	nnode_t *new_node = allocate_nnode();
	new_node->related_ast_node = NULL;
	new_node->type = FF_NODE;

	/* allocate the output pin (there is always one output pin) */
	allocate_more_node_output_pins(new_node, 1);
	add_output_port_information(new_node, 1);

	/* allocate the input pin */
	allocate_more_node_input_pins(new_node,2);/* input[1] is clock */
  
	/* add the port information */
	int i;
	for(i = 0; i < 2; i++)
	{
		add_input_port_information(new_node,1);
	}

	/* add names and type information to the created input pins */
	npin_t *new_pin = allocate_npin();
	new_pin->name = names[0];
	new_pin->type = INPUT;
	add_a_input_pin_to_node_spot_idx(new_node, new_pin,0);

	new_pin = allocate_npin();
	new_pin->name = names[3];
	new_pin->type = INPUT;
	add_a_input_pin_to_node_spot_idx(new_node, new_pin,1);

	/* add a name for the node, keeping the name of the node same as the output */
	new_node->name = make_full_ref_name(names[1],NULL, NULL, NULL,-1);

	/*add this node to verilog_netlist as an ff (flip-flop) node */
	verilog_netlist->ff_nodes = realloc(verilog_netlist->ff_nodes, sizeof(nnode_t*)*(verilog_netlist->num_ff_nodes+1));
	verilog_netlist->ff_nodes[verilog_netlist->num_ff_nodes++] = new_node;

	/*add name information and a net(driver) for the output */
	nnet_t *new_net = allocate_nnet();
	new_net->name = new_node->name;

	new_pin = allocate_npin();
	new_pin->name = new_node->name;
	new_pin->type = OUTPUT;
	add_a_output_pin_to_node_spot_idx(new_node, new_pin, 0);
	add_a_driver_pin_to_net(new_net, new_pin);
  
	/*list this output in output_nets_sc */
	long sc_spot = sc_add_string(output_nets_sc,new_node->name);
	if (output_nets_sc->data[sc_spot])
		warning_message(NETLIST_ERROR,linenum,-1, "Net (%s) with the same name already created\n",ptr);

	/* store the data which is an idx here */
	output_nets_sc->data[sc_spot] = (void*)new_net;

	/* Free the char** names */
	free(names);
}

/*---------------------------------------------------------------------------------------------
   * function: search_clock_name
     to search the clock if the control in the latch 
     is not mentioned
*-------------------------------------------------------------------------------------------*/
char* search_clock_name(char * blif_file)
{
	FILE * file = fopen(blif_file,"r");

	char ** input_names = NULL;
	int input_names_count = 0;
	int found = 0;
	while(!found)
	{
		char buffer[BUFSIZE];
		my_fgets(buffer,BUFSIZE,file);

		// not sure if this is needed
		if(feof(file))
			break;

		char *ptr;
		if((ptr = my_strtok(buffer, TOKENS, file, buffer)))
		{
			if(!strcmp(ptr,".end"))
				break;

			if(!strcmp(ptr,".inputs"))
			{
				/* store the inputs in array of string */
				while((ptr = my_strtok (NULL, TOKENS, file, buffer)))
				{
					input_names = (char**)realloc(input_names,sizeof(char*) * (input_names_count + 1));
					input_names[input_names_count++] = strdup(ptr);
				}
			}
			else if(!strcmp(ptr,".names") || !strcmp(ptr,".latch"))
			{
				while((ptr = my_strtok (NULL, TOKENS,file, buffer)))
				{
					int i;
					for(i = 0; i < input_names_count; i++)
					{
						if(!strcmp(ptr,input_names[i]))
						{
							free(input_names[i]);
							input_names[i] = input_names[--input_names_count];
						}
					}
				}
			}
			else if(input_names_count == 1)
			{
				found = 1;
			}
		}
	}
	
	fclose(file);

	if (found) return input_names[0];
	else       return default_clock_name;
}
  


/*---------------------------------------------------------------------------------------------
   * function:create_hard_block_nodes
     to create the hard block nodes
*-------------------------------------------------------------------------------------------*/
void create_hard_block_nodes()
{
	char buffer[BUFSIZE];
	char *name_subckt = my_strtok (NULL, TOKENS, blif, buffer);

	/* storing the names on the formal-actual parameter */
	char *token;
	int count = 0;
	char **names_parameters = NULL;
	while ((token = my_strtok (NULL, TOKENS, blif, buffer)) != NULL)
  	{
		names_parameters          = (char**)realloc(names_parameters, sizeof(char*)*(count + 1));
		names_parameters[count++] = strdup(token);
  	}	
   
	char **mappings = (char**)malloc(sizeof(char*) * count);
	char **names = (char**)malloc(sizeof(char*) * count);

	int i = 0;
	for (i = 0; i < count; i++)
	{
		mappings[i] = strdup(strtok(names_parameters[i], "="));
		names[i]    = strdup(strtok(NULL, "="));
	}

	for(i = 0; i < count; i++)
		free(names_parameters[i]);

	free(names_parameters);

	// Associate mappings with their mapped to thing.
	hashtable_t *mapping_index = associate_names(mappings, names, count);

	// Sort the mappings.
	qsort(mappings,  count,  sizeof(char *), compare_pin_names);

	hard_block_ports *ports = get_hard_block_ports(mappings, count);
 	hard_block_model *model = read_hard_block_model(name_subckt, blif);

 	verify_hard_block_ports_against_model(ports, model);

	nnode_t *new_node = allocate_nnode();

	/* calculate the number of input and output port size */

	/* Add input and output ports to the new node. */
	{
		hard_block_ports *p;

		p = model->input_ports;
		for (i = 0; i < p->count; i++)
			add_input_port_information(new_node, p->sizes[i]);

		p = model->output_ports;
		for (i = 0; i < p->count; i++)
			add_output_port_information(new_node, p->sizes[i]);
	}

	/* creating the node */
	char *subcircuit_name_prefix = strndup(name_subckt,5);
	if (!strcmp(name_subckt, "multiply") || !strcmp(subcircuit_name_prefix, "mult_"))
		new_node->type = MULTIPLY;
	else
		new_node->type = MEMORY;
	free(subcircuit_name_prefix);

	allocate_more_node_output_pins(new_node, model->outputs->count);

	if (model->inputs->count > 0) // check if there is any input pins
		allocate_more_node_input_pins(new_node, model->inputs->count);

	/* add names and type information to the created input pins */
  	for(i = 0; i < model->inputs->count; i++)
  	{
  		char *mapping = model->inputs->names[i];
  		char *name = mapping_index->get(mapping_index, mapping, strlen(mapping) * sizeof(char));

  		if (!name) error_message(NETLIST_ERROR, linenum, -1, "Invalid hard block mapping: %s", mapping);

		npin_t *new_pin = allocate_npin();
		new_pin->name = strdup(name);
		new_pin->type = INPUT;
		new_pin->mapping = get_hard_block_port_name(mapping);

		add_a_input_pin_to_node_spot_idx(new_node, new_pin, i);
  	}

	/* Check this , I am not sure (simulator needs this information) */
	new_node->related_ast_node = (ast_node_t *)calloc(1, sizeof(ast_node_t));
	new_node->related_ast_node->children = (ast_node_t **)calloc(1,sizeof(ast_node_t *));
	new_node->related_ast_node->children[0] = (ast_node_t *)calloc(1, sizeof(ast_node_t));
	new_node->related_ast_node->children[0]->types.identifier = strdup(name_subckt);

	// Name the node subcircuit_name~hard_block_number
	static long hard_block_number = 0;
	sprintf(buffer, "%s~%ld", name_subckt, hard_block_number++);
	new_node->name = make_full_ref_name(buffer, NULL, NULL, NULL,-1);

	/* Add name information and a net(driver) for the output */
  	for(i = 0; i < model->outputs->count; i++)
  	{
  		char *mapping = model->outputs->names[i];
  		char *name = mapping_index->get(mapping_index, mapping, strlen(mapping) * sizeof(char));

  		if (!name) error_message(NETLIST_ERROR, linenum, -1,"Invalid hard block mapping: %s", model->outputs->names[i]);

		npin_t *new_pin = allocate_npin();
		new_pin->name = strdup(name);
		new_pin->type = OUTPUT;
		new_pin->mapping = strdup(mapping);

		add_a_output_pin_to_node_spot_idx(new_node, new_pin, i);

		nnet_t *new_net = allocate_nnet();
		new_net->name = strdup(name);

		add_a_driver_pin_to_net(new_net,new_pin);

		/*list this output in output_nets_sc */
		long sc_spot = sc_add_string(output_nets_sc, strdup(name));
		if (output_nets_sc->data[sc_spot])
		  	error_message(NETLIST_ERROR,linenum, -1,"Hard block: Net (%s) with the same name already created\n",name);

		/* store the data which is an idx here */
		output_nets_sc->data[sc_spot] = new_net;
	}

  	/*add this node to verilog_netlist as an internal node */
  	verilog_netlist->internal_nodes = realloc(verilog_netlist->internal_nodes, sizeof(nnode_t*) * (verilog_netlist->num_internal_nodes + 1));
  	verilog_netlist->internal_nodes[verilog_netlist->num_internal_nodes++] = new_node;

  	free_hard_block_model(model);
  	free_hard_block_ports(ports);
  	mapping_index->destroy(mapping_index);
  	free(mappings);
  	free(names);


}

/*---------------------------------------------------------------------------------------------
   * function:create_internal_node_and_driver
     to create an internal node and driver from that node 
*-------------------------------------------------------------------------------------------*/

void create_internal_node_and_driver()
{
	/* Storing the names of the input and the final output in array names */
	char *ptr;
	char **names = NULL; // stores the names of the input and the output, last name stored would be of the output
	int input_count = 0;
	char buffer[BUFSIZE];
	while ((ptr = my_strtok (NULL, TOKENS, blif, buffer)))
	{
		names = (char**)realloc(names, sizeof(char*) * (input_count + 1));
		names[input_count++]= strdup(ptr);
	}
  
	/* assigning the new_node */
	nnode_t *new_node = allocate_nnode();
	new_node->related_ast_node = NULL;

	/* gnd vcc unconn already created as top module so ignore them */
	if (
			   !strcmp(names[input_count-1],"gnd")
			|| !strcmp(names[input_count-1],"vcc")
			|| !strcmp(names[input_count-1],"unconn")
	)
	{
		skip_reading_bit_map = TRUE;
	}
	else
	{
		/* assign the node type by seeing the name */
		short node_type = assign_node_type_from_node_name(names[input_count-1]);

		if(node_type != GENERIC)
		{
			new_node->type = node_type;
			skip_reading_bit_map = TRUE;
		}
		/* Check for GENERIC type , change the node by reading the bit map */
		else if(node_type == GENERIC)
		{
			new_node->type = read_bit_map_find_unknown_gate(input_count-1,new_node);
			skip_reading_bit_map = TRUE;
		}

		/* allocate the input pin (= input_count-1)*/
		if (input_count-1 > 0) // check if there is any input pins
		{
			allocate_more_node_input_pins(new_node, input_count-1);

			/* add the port information */
			if(new_node->type == MUX_2)
			{
				add_input_port_information(new_node, (input_count-1)/2);
				add_input_port_information(new_node, (input_count-1)/2);
			}
			else
			{
				int i;
				for(i = 0; i < input_count-1; i++)
					add_input_port_information(new_node, 1);
			}
		}

		/* add names and type information to the created input pins */
		int i;
		for(i = 0; i <= input_count-2; i++)
		{
			npin_t *new_pin = allocate_npin();
			new_pin->name = names[i];
			new_pin->type = INPUT;
			add_a_input_pin_to_node_spot_idx(new_node, new_pin, i);
		}
	
		/* add information for the intermediate VCC and GND node (appears in ABC )*/
		if(new_node->type == GND_NODE)
		{
			allocate_more_node_input_pins(new_node,1);
			add_input_port_information(new_node, 1);

			npin_t *new_pin = allocate_npin();
			new_pin->name = GND;
			new_pin->type = INPUT;
			add_a_input_pin_to_node_spot_idx(new_node, new_pin,0);
		}

		if(new_node->type == VCC_NODE)
		{
			allocate_more_node_input_pins(new_node,1);
			add_input_port_information(new_node, 1);

			npin_t *new_pin = allocate_npin();
			new_pin->name = VCC;
			new_pin->type = INPUT;
			add_a_input_pin_to_node_spot_idx(new_node, new_pin,0);
		}

		/* allocate the output pin (there is always one output pin) */
		allocate_more_node_output_pins(new_node, 1);
		add_output_port_information(new_node, 1);

		/* add a name for the node, keeping the name of the node same as the output */
		new_node->name = make_full_ref_name(names[input_count-1],NULL, NULL, NULL,-1);

		/*add this node to verilog_netlist as an internal node */
		verilog_netlist->internal_nodes = (nnode_t**)realloc(verilog_netlist->internal_nodes, sizeof(nnode_t*)*(verilog_netlist->num_internal_nodes+1));
		verilog_netlist->internal_nodes[verilog_netlist->num_internal_nodes++] = new_node;

		/*add name information and a net(driver) for the output */

		npin_t *new_pin = allocate_npin();
		new_pin->name = new_node->name;
		new_pin->type = OUTPUT;

		add_a_output_pin_to_node_spot_idx(new_node, new_pin, 0);

		nnet_t *new_net = allocate_nnet();
		new_net->name = new_node->name;

		add_a_driver_pin_to_net(new_net,new_pin);

		/* List this output in output_nets_sc */
		long sc_spot = sc_add_string(output_nets_sc, new_node->name);
		if (output_nets_sc->data[sc_spot])
			error_message(NETLIST_ERROR,linenum,-1, "Internal node and driver: Net (%s) with the same name already created\n",ptr);

		/* store the data which is an idx here */
		output_nets_sc->data[sc_spot] = new_net;

		/* Free the char** names */
		free(names);
	}
}

/*
*---------------------------------------------------------------------------------------------
   * function: read_bit_map_find_unknown_gate
     read the bit map for simulation
*-------------------------------------------------------------------------------------------*/
  
short read_bit_map_find_unknown_gate(int input_count, nnode_t *node)
{

	char buffer[BUFSIZE];
	fpos_t pos;// store the current position of the file pointer
	int i,j;
	int line_count_bitmap=0; //stores the number of lines in a particular bit map
	char ** bit_map=NULL;
	char *output_bit_map = 0;// to distinguish whether for the bit_map output is 1 or 0
		
	fgetpos(blif,&pos);


	if(!input_count)
	{
		my_fgets (buffer, BUFSIZE, blif);

		fsetpos(blif,&pos);

		char *ptr = my_strtok(buffer,"\t\n", blif, buffer);
		if      (!strcmp(ptr," 0")) return GND_NODE;
		else if (!strcmp(ptr," 1")) return VCC_NODE;
		else if (!ptr)              return GND_NODE;
		else                        return VCC_NODE;
	}
	


	while(1)
	{
		my_fgets (buffer, BUFSIZE, blif);
		if(!(buffer[0]=='0' || buffer[0]=='1' || buffer[0]=='-'))
			break;

		bit_map=(char**)realloc(bit_map,sizeof(char*) * (line_count_bitmap + 1));
		bit_map[line_count_bitmap++] = strdup(my_strtok(buffer,TOKENS, blif, buffer));

		output_bit_map = strdup(my_strtok(NULL,TOKENS, blif, buffer));
	}

	fsetpos(blif,&pos);

	/* Single line bit map : */
	if(line_count_bitmap == 1)
	{
		// GT
		if(!strcmp(bit_map[0],"100"))
			return GT;

		// LT
		if(!strcmp(bit_map[0],"010"))
			return LT;

		/* LOGICAL_AND and LOGICAL_NAND for ABC*/
		for(i = 0; i < input_count && bit_map[0][i] == '1'; i++);

		if(i == input_count)
		{
			if (!strcmp(output_bit_map,"1"))
				return LOGICAL_AND;
			else if (!strcmp(output_bit_map,"0"))
				return LOGICAL_NAND;
		}

		/* BITWISE_NOT */
		if(!strcmp(bit_map[0],"0"))
			return BITWISE_NOT;

		/* LOGICAL_NOR and LOGICAL_OR for ABC */
		for(i = 0; i < input_count && bit_map[0][i] == '0'; i++);
		if(i == input_count)
		{
			if (!strcmp(output_bit_map,"1"))
				return LOGICAL_NOR;
			else if (!strcmp(output_bit_map,"0"))
				return LOGICAL_OR;
		}
	}
	/* Assumption that bit map is in order when read from blif */
	else if(line_count_bitmap == 2)
	{
		/* LOGICAL_XOR */
		if((strcmp(bit_map[0],"01")==0) && (strcmp(bit_map[1],"10")==0)) return LOGICAL_XOR;
		/* LOGICAL_XNOR */
		if((strcmp(bit_map[0],"00")==0) && (strcmp(bit_map[1],"11")==0)) return LOGICAL_XNOR;
	}
	else if (line_count_bitmap == 4)
	{
		/* ADDER_FUNC */
		if (
				   (!strcmp(bit_map[0],"001"))
				&& (!strcmp(bit_map[1],"010"))
				&& (!strcmp(bit_map[2],"100"))
				&& (!strcmp(bit_map[3],"111"))
		)
			return ADDER_FUNC;
		/* CARRY_FUNC */
		if(
				   (!strcmp(bit_map[0],"011"))
				&& (!strcmp(bit_map[1],"101"))
				&& (!strcmp(bit_map[2],"110"))
				&& (!strcmp(bit_map[3],"111"))
		)
			return 	CARRY_FUNC;
		/* LOGICAL_XOR */
		if(
				   (!strcmp(bit_map[0],"001"))
				&& (!strcmp(bit_map[1],"010"))
				&& (!strcmp(bit_map[2],"100"))
				&& (!strcmp(bit_map[3],"111"))
		)
			return 	LOGICAL_XOR;
		/* LOGICAL_XNOR */
		if(
				   (!strcmp(bit_map[0],"000"))
				&& (!strcmp(bit_map[1],"011"))
				&& (!strcmp(bit_map[2],"101"))
				&& (!strcmp(bit_map[3],"110"))
		)
			return 	LOGICAL_XNOR;
	}
  

	if(line_count_bitmap == input_count)
	{
		/* LOGICAL_OR */
		for(i = 0; i < line_count_bitmap; i++)
		{
			if(bit_map[i][i] == '1')
			{
				for(j = 1; j < input_count; j++)
					if(bit_map[i][(i+j)% input_count]!='-')
						break;

				if(j != input_count)
					break;
			}
			else
			{
				break;
			}
		}

		if(i == line_count_bitmap)
			return LOGICAL_OR;

		/* LOGICAL_NAND */
		for(i = 0; i < line_count_bitmap; i++)
		{
			if(bit_map[i][i]=='0')
			{
				for(j=1;j<input_count;j++)
					if(bit_map[i][(i+j)% input_count]!='-')
						break;

				if(j != input_count) break;
			}
			else
			{
				break;
			}
		}

		if(i == line_count_bitmap)
			return LOGICAL_NAND;
	}

	/* MUX_2 */
	if(line_count_bitmap*2 == input_count)
	{
		for(i = 0; i < line_count_bitmap; i++)
		{
			if (
					   (bit_map[i][i]=='1')
					&& (bit_map[i][i+line_count_bitmap] =='1')
			)
			{
				for (j = 1; j < line_count_bitmap; j++)
				{
					if (
							   (bit_map[i][ (i+j) % line_count_bitmap] != '-')
							|| (bit_map[i][((i+j) % line_count_bitmap) + line_count_bitmap] != '-')
					)
					{
						break;
					}
				}

				if(j != input_count)
					break;
			}
			else
			{
				break;
			}
		}

		if(i == line_count_bitmap)
			return MUX_2;
	}

	 /* assigning the bit_map to the node if it is GENERIC */
	
	node->bit_map = bit_map;
	node->bit_map_line_count = line_count_bitmap;
	return GENERIC;
}

/*
*---------------------------------------------------------------------------------------------
   * function: add_top_input_nodes
     to add the top level inputs to the netlist 
*-------------------------------------------------------------------------------------------*/
void add_top_input_nodes()
{
	char *ptr;
	char buffer[BUFSIZE];
	while ((ptr = my_strtok (NULL, TOKENS, blif, buffer)))
	{
		char *temp_string = make_full_ref_name(ptr, NULL, NULL,NULL, -1);

		/* create a new top input node and net*/

		nnode_t *new_node = allocate_nnode();

		new_node->related_ast_node = NULL;
		new_node->type = INPUT_NODE;

		/* add the name of the input variable */
		new_node->name = temp_string;

		/* allocate the pins needed */
		allocate_more_node_output_pins(new_node, 1);
		add_output_port_information(new_node, 1);

		/* Create the pin connection for the net */
		npin_t *new_pin = allocate_npin();
		new_pin->name = strdup(temp_string);
		new_pin->type = OUTPUT;

		/* hookup the pin, net, and node */
		add_a_output_pin_to_node_spot_idx(new_node, new_pin, 0);

		nnet_t *new_net = allocate_nnet();
		new_net->name = strdup(temp_string);

		add_a_driver_pin_to_net(new_net, new_pin);

		verilog_netlist->top_input_nodes = (nnode_t**)realloc(verilog_netlist->top_input_nodes, sizeof(nnode_t*)*(verilog_netlist->num_top_input_nodes+1));
		verilog_netlist->top_input_nodes[verilog_netlist->num_top_input_nodes++] = new_node;

		long sc_spot = sc_add_string(output_nets_sc, temp_string);
		if (output_nets_sc->data[sc_spot])
			warning_message(NETLIST_ERROR,linenum,-1, "Net (%s) with the same name already created\n",temp_string);

		output_nets_sc->data[sc_spot] = new_net;
	}
}

/*---------------------------------------------------------------------------------------------
   * function: create_top_output_nodes
     to add the top level outputs to the netlist 
*-------------------------------------------------------------------------------------------*/	
void rb_create_top_output_nodes()
{
	char *ptr;
	char buffer[BUFSIZE];

	while ((ptr = my_strtok (NULL, TOKENS, blif, buffer)))
	{
		char *temp_string = make_full_ref_name(ptr, NULL, NULL,NULL, -1);;

		/*add_a_fanout_pin_to_net((nnet_t*)output_nets_sc->data[sc_spot], new_pin);*/

		/* create a new top output node and */
		nnode_t *new_node = allocate_nnode();
		new_node->related_ast_node = NULL;
		new_node->type = OUTPUT_NODE;

		/* add the name of the output variable */
		new_node->name = temp_string;

		/* allocate the input pin needed */
		allocate_more_node_input_pins(new_node, 1);
		add_input_port_information(new_node, 1);

		/* Create the pin connection for the net */
		npin_t *new_pin = allocate_npin();
		new_pin->name   = temp_string;
		/* hookup the pin, net, and node */
		add_a_input_pin_to_node_spot_idx(new_node, new_pin, 0);

		/*adding the node to the verilog_netlist output nodes
		add_node_to_netlist() function can also be used */
		verilog_netlist->top_output_nodes = (nnode_t**)realloc(verilog_netlist->top_output_nodes, sizeof(nnode_t*)*(verilog_netlist->num_top_output_nodes+1));
		verilog_netlist->top_output_nodes[verilog_netlist->num_top_output_nodes++] = new_node;
	}
} 
  
 
/*---------------------------------------------------------------------------------------------
   * (function: look_for_clocks)
 *-------------------------------------------------------------------------------------------*/

void rb_look_for_clocks()
{
	int i; 
	for (i = 0; i < verilog_netlist->num_ff_nodes; i++)
	{
		if (verilog_netlist->ff_nodes[i]->input_pins[1]->net->driver_pin->node->type != CLOCK_NODE)
		{
			verilog_netlist->ff_nodes[i]->input_pins[1]->net->driver_pin->node->type = CLOCK_NODE;
		}
	}

}

/*
----------------------------------------------------------------------------
function: Creates the drivers for the top module
   Top module is :
                * Special as all inputs are actually drivers.
                * Also make the 0 and 1 constant nodes at this point.
---------------------------------------------------------------------------
*/

void rb_create_top_driver_nets(char *instance_name_prefix)
{
	npin_t *new_pin;
	long sc_spot;// store the location of the string stored in string_cache
	/* create the constant nets */

	/* ZERO net */ 
	/* description given for the zero net is same for other two */
	verilog_netlist->zero_net = allocate_nnet(); // allocate memory to net pointer
	verilog_netlist->gnd_node = allocate_nnode(); // allocate memory to node pointer
	verilog_netlist->gnd_node->type = GND_NODE;  // mark the type
	allocate_more_node_output_pins(verilog_netlist->gnd_node, 1);// alloacate 1 output pin pointer to this node
	add_output_port_information(verilog_netlist->gnd_node, 1);// add port info. this port has 1 pin ,till now number of port for this is one
	new_pin = allocate_npin();
	add_a_output_pin_to_node_spot_idx(verilog_netlist->gnd_node, new_pin, 0);// add this pin to output pin pointer array of this node
	add_a_driver_pin_to_net(verilog_netlist->zero_net,new_pin);// add this pin to net as driver pin

	/*ONE net*/
	verilog_netlist->one_net = allocate_nnet();
	verilog_netlist->vcc_node = allocate_nnode();
	verilog_netlist->vcc_node->type = VCC_NODE;
	allocate_more_node_output_pins(verilog_netlist->vcc_node, 1);
	add_output_port_information(verilog_netlist->vcc_node, 1);
	new_pin = allocate_npin();
	add_a_output_pin_to_node_spot_idx(verilog_netlist->vcc_node, new_pin, 0);
	add_a_driver_pin_to_net(verilog_netlist->one_net, new_pin);

	/* Pad net */
	verilog_netlist->pad_net = allocate_nnet();
	verilog_netlist->pad_node = allocate_nnode();
	verilog_netlist->pad_node->type = PAD_NODE;
	allocate_more_node_output_pins(verilog_netlist->pad_node, 1);
	add_output_port_information(verilog_netlist->pad_node, 1);
	new_pin = allocate_npin();
	add_a_output_pin_to_node_spot_idx(verilog_netlist->pad_node, new_pin, 0);
	add_a_driver_pin_to_net(verilog_netlist->pad_net, new_pin);

	/* CREATE the driver for the ZERO */
	blif_zero_string = make_full_ref_name(instance_name_prefix, NULL, NULL, zero_string, -1);
	verilog_netlist->gnd_node->name = (char *)GND;

	sc_spot = sc_add_string(output_nets_sc, (char *)GND);
	if (output_nets_sc->data[sc_spot])
	{
		error_message(NETLIST_ERROR,-1,-1, "Error in Odin: failed to index ground node.\n");
	}

	/* store the data which is an idx here */
	output_nets_sc->data[sc_spot] = (void*)verilog_netlist->zero_net;
	verilog_netlist->zero_net->name =blif_zero_string;

	/* CREATE the driver for the ONE and store twice */
	blif_one_string = make_full_ref_name(instance_name_prefix, NULL, NULL, one_string, -1);
	verilog_netlist->vcc_node->name = (char *)VCC;

	sc_spot = sc_add_string(output_nets_sc, (char *)VCC);
	if (output_nets_sc->data[sc_spot])
	{
		error_message(NETLIST_ERROR,-1,-1, "Error in Odin: failed to index vcc node.\n");
	}
	/* store the data which is an idx here */
	output_nets_sc->data[sc_spot] = (void*)verilog_netlist->one_net;
	verilog_netlist->one_net->name =blif_one_string;

	/* CREATE the driver for the PAD */
	blif_pad_string = make_full_ref_name(instance_name_prefix, NULL, NULL, pad_string, -1);
	verilog_netlist->pad_node->name = (char *)HBPAD;

	sc_spot = sc_add_string(output_nets_sc, (char *)HBPAD);
	if (output_nets_sc->data[sc_spot])
	{
		error_message(NETLIST_ERROR, -1,-1, "Error in Odin: failed to index pad node.\n");
	}
	/* store the data which is an idx here */
	output_nets_sc->data[sc_spot] = verilog_netlist->pad_net;
	verilog_netlist->pad_net->name = blif_pad_string;
}

/*---------------------------------------------------------------------------------------------
 * (function: dum_parse)
 *-------------------------------------------------------------------------------------------*/
static void dum_parse (char *buffer)
{
	/* Continue parsing to the end of this (possibly continued) line. */
	while (my_strtok (NULL, TOKENS, blif, buffer));
}



/*---------------------------------------------------------------------------------------------
 * function: hook_up_nets() 
 * find the output nets and add the corresponding nets
 *-------------------------------------------------------------------------------------------*/

void hook_up_nets()
{
	/* hook all the input pins in all the internal nodes to the net */
	int i;
	for(i = 0; i < verilog_netlist->num_internal_nodes; i++)
	{
		nnode_t *node = verilog_netlist->internal_nodes[i];
		hook_up_node(node);
	}

	/* hook all the ff nodes' input pin to the nets */
	for(i = 0; i < verilog_netlist->num_ff_nodes; i++)
	{
		nnode_t *node = verilog_netlist->ff_nodes[i];
		hook_up_node(node);
	}

	/* hook the top output nodes input pins */
	for(i = 0; i < verilog_netlist->num_top_output_nodes; i++)
	{
		nnode_t *node = verilog_netlist->top_output_nodes[i];
		hook_up_node(node);
	}
}

/*
 * Connect the given node's input pins to their corresponding nets by
 * looking each one up in the output_nets_sc.
 */
void hook_up_node(nnode_t *node)
{
	int j;
	for(j = 0; j < node->num_input_pins; j++)
	{
		npin_t *input_pin = node->input_pins[j];

		long sc_spot = sc_lookup_string(output_nets_sc,input_pin->name);
		if(sc_spot == -1)
			error_message(NETLIST_ERROR,linenum, -1, "Error:Could not hook up the pin %s: not available.", input_pin->name);

		nnet_t *output_net = output_nets_sc->data[sc_spot];

		add_a_fanout_pin_to_net(output_net, input_pin);
	}
}

/*
 * Scans ahead in the given file to find the
 * model for the hard block by the given name.
 * Returns the file to its original position when finished.
 */
hard_block_model *read_hard_block_model(char *name_subckt, FILE *file)
{
	char buffer[BUFSIZE];
	// store the current position of the file pointer
	fpos_t pos;
	fgetpos(blif,&pos);


	while (1)
  	{
		my_fgets(buffer, BUFSIZE, file);
		if(feof(file))
			error_message(NETLIST_ERROR,linenum, -1,"Error : The '%s' subckt was not found.",name_subckt);

		char *token = my_strtok(buffer,TOKENS, file, buffer);

		// Look for .model followed buy the subcircuit name.
		if (
			   token
			&& !strcmp(token,".model")
			&& !strcmp(my_strtok(NULL,TOKENS, file, buffer), name_subckt)
		)
		{
			break;
		}
	}

	hard_block_model *model = malloc(sizeof(hard_block_model));
	model->inputs = malloc(sizeof(hard_block_pins));
	model->inputs->count = 0;
	model->inputs->names = NULL;

	model->outputs = malloc(sizeof(hard_block_pins));
	model->outputs->count = 0;
	model->outputs->names = NULL;

	// Read the inputs and outputs.
	while (my_fgets(buffer, BUFSIZE, file))
	{
		if(feof(file))
			error_message(NETLIST_ERROR,linenum, -1,"Hit the end of the file while reading .model %s", name_subckt);

		char *first_word = my_strtok(buffer, TOKENS, file, buffer);
		if(!strcmp(first_word, ".inputs"))
		{
			char *name;
			while ((name = my_strtok(NULL, TOKENS, file, buffer)))
			{
				model->inputs->names = realloc(model->inputs->names, sizeof(char *) * (model->inputs->count + 1));
				model->inputs->names[model->inputs->count++] = strdup(name);
			}
		}
		else if(!strcmp(first_word, ".outputs"))
		{
			char *name;
			while ((name = my_strtok(NULL, TOKENS, file, buffer)))
			{
				model->outputs->names = realloc(model->outputs->names, sizeof(char *) * (model->outputs->count + 1));
				model->outputs->names[model->outputs->count++] = strdup(name);
			}
		}
		else if(!strcmp(first_word, ".end"))
		{
			break;
		}
	}

	// Sort the names.
	qsort(model->inputs->names,  model->inputs->count,  sizeof(char *), compare_pin_names);
	qsort(model->outputs->names, model->outputs->count, sizeof(char *), compare_pin_names);

	// Index the names.
	model->inputs->index  = index_names(model->inputs->names, model->inputs->count);
	model->outputs->index = index_names(model->outputs->names, model->outputs->count);

	model->input_ports  = get_hard_block_ports(model->inputs->names,  model->inputs->count);
	model->output_ports = get_hard_block_ports(model->outputs->names, model->outputs->count);

 	fsetpos(blif,&pos);

	return model;
}

/*
 * Callback function for qsort which compares pin names
 * of the form port_name[pin_number] primarily
 * on the port_name, and on the pin_number if the port_names
 * are identical.
 */
static int compare_pin_names(const void *p1, const void *p2)
{
	char *name1 = *(char * const *)p1;
	char *name2 = *(char * const *)p2;
	char *port_name1 = get_hard_block_port_name(name1);
	char *port_name2 = get_hard_block_port_name(name2);
	int portname_difference = strcmp(port_name1, port_name2);
	free(port_name1);
	free(port_name2);

	if (!portname_difference)
	{	int n1 = get_hard_block_pin_number(name1);
		int n2 = get_hard_block_pin_number(name2);
		return n1 - n2;
	}
	else
	{
		return portname_difference;
	}
}

/*
 * Creates a hashtable index for an array of strings of
 * the form names[i]=>i.
 */
hashtable_t *index_names(char **names, int count)
{
	hashtable_t *index = create_hashtable((count*2) + 1);
	int i;
	for (i = 0; i < count; i++)
	{	int *offset = malloc(sizeof(int));
		*offset = i;
		index->add(index, names[i], sizeof(char) * strlen(names[i]), offset);
	}
	return index;
}

/*
 * Create an associative index of names1[i]=>names2[i]
 */
hashtable_t *associate_names(char **names1, char **names2, int count)
{
	hashtable_t *index = create_hashtable((count*2) + 1);
	int i;
	for (i = 0; i < count; i++)
		index->add(index, names1[i], sizeof(char) * strlen(names1[i]), names2[i]);

	return index;
}


/*
 * Organises the given strings representing pin names on a hard block
 * model into ports, and indexes the ports by name. Returns the organised
 * ports as a hard_block_ports struct.
 */
hard_block_ports *get_hard_block_ports(char **pins, int count)
{
	// Count the input port sizes.
	hard_block_ports *ports = malloc(sizeof(hard_block_ports));
	ports->count = 0;
	ports->sizes = 0;
	ports->names = 0;
	char *prev_portname = 0;
	int i;
	for (i = 0; i < count; i++)
	{
		char *portname = get_hard_block_port_name(pins[i]);
		// Compare the part of the name before the "["
		if (!i || strcmp(prev_portname, portname))
		{
			ports->sizes = realloc(ports->sizes, sizeof(int) * (ports->count + 1));
			ports->names = realloc(ports->names, sizeof(char *) * (ports->count + 1));

			ports->sizes[ports->count] = 0;
			ports->names[ports->count] = portname;
			ports->count++;
		}

		ports->sizes[ports->count-1]++;
		prev_portname = portname;
	}

	ports->index  = index_names(ports->names, ports->count);

	return ports;
}

/*
 * Check for inconsistencies between the hard block model and the ports found
 * in the hard block instance. Print appropreate error messages if
 * differences are found.
 */
void verify_hard_block_ports_against_model(hard_block_ports *ports, hard_block_model *model)
{

	hard_block_ports *port_sets[] = {model->input_ports, model->input_ports};
	int i;
	for (i = 0; i < 2; i++)
	{
		hard_block_ports *p = port_sets[i];
		int j;
		for (j = 0; j < p->count; j++)
		{
			// Look up each port from the model in "ports" and make sure they match in size.
			char *name = p->names[j];
			int   size = p->sizes[j];
			int *idx   = ports->index->get(ports->index, name, strlen(name) * sizeof(char));
			if (!idx)
				error_message(NETLIST_ERROR, linenum, -1,"Port %s is not specified by this subckt.", name);

			int instance_size = ports->sizes[*idx];
			if (size != instance_size)
				error_message(NETLIST_ERROR, linenum, -1,"The width of %s (%d) differs from the model width of %d.", name, instance_size, size);
		}
	}

	hard_block_ports *in = model->input_ports;
	hard_block_ports *out = model->output_ports;
	int j;
	for (j = 0; j < ports->count; j++)
	{
		// Look up each port from the subckt to make sure it appears in the model.
		char *name   = ports->names[j];
		int *in_idx  = in->index->get(in->index, name, strlen(name) * sizeof(char));
		int *out_idx = out->index->get(out->index, name, strlen(name) * sizeof(char));
		if (!in_idx && !out_idx)
			error_message(NETLIST_ERROR, linenum, -1,"Port %s is this subckt does not appear in the model.", name);

	}
}

/*
 * Gets the text in the given string which occurs
 * before the first instance of "[". The string is
 * presumably of the form "port[pin_number]"
 *
 * The retuned string is strduped and must be freed.
 * The original string is unaffected.
 */
char *get_hard_block_port_name(char *name)
{
	name = strdup(name);
	if (strchr(name,'['))
		return strtok(name,"[");
	else
		return name;
}

/*
 * Parses a port name of the form port[pin_number]
 * and returns the pin number as a long. Returns -1
 * if there is no [pin_number] in the name. Throws an
 * error if pin_number is not parsable as a long.
 *
 * The original string is unaffected.
 */
long get_hard_block_pin_number(char *original_name)
{
	if (!strchr(original_name,'['))
		return -1;

	char *name = strdup(original_name);
	strtok(name,"[");
	char *endptr;
	char *pin_number_string = strtok(NULL,"]");
	long pin_number = strtol(pin_number_string, &endptr, 10);

	if (pin_number_string == endptr)
		error_message(NETLIST_ERROR,linenum, -1,"The given port name \"%s\" does not contain a valid pin number.", original_name);

	free(name);

	return pin_number;
}

/*
 * Frees a hard_block_model.
 */
void free_hard_block_model(hard_block_model *model)
{
	free_hard_block_pins(model->inputs);
	free_hard_block_pins(model->outputs);

	free_hard_block_ports(model->input_ports);
	free_hard_block_ports(model->output_ports);

	free(model);
}

void free_hard_block_pins(hard_block_pins *p)
{
	while (p->count--) free(p->names[p->count]);

	free(p->names);

	p->index->destroy_free_items(p->index);
	free(p);
}


void free_hard_block_ports(hard_block_ports *p)
{
	while(p->count--) free(p->names[p->count]);

	free(p->names);
	free(p->sizes);

	p->index->destroy_free_items(p->index);
	free(p);
}

