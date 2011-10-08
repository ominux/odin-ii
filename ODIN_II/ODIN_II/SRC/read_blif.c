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

#define TOKENS " \t\n"

static FILE * blif;
int linenum;

STRING_CACHE *output_nets_sc;
STRING_CACHE *input_nets_sc;

const char *GND ="gnd";
const char *VCC="vcc";
const char *HBPAD="unconn";

char *blif_one_string = "ONE_VCC_CNS";
char *blif_zero_string = "ZERO_GND_ZERO";
char *blif_pad_string = "ZERO_PAD_ZERO";
char *default_clock_name="top^clock"; 

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
char* search_clock_name(char * blif_file);

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
	printf("Reading lines\n"); fflush(stdout);

	linenum = 0;
	int done = 0;
	char buffer[BUFSIZE];
	while (!done && (my_fgets(buffer, BUFSIZE, blif) != NULL))
	{       
		read_tokens(buffer,&done,blif_file);/* read one token at a time */
	}

	fclose (blif);
	
	printf("Looking for clocks\n"); fflush(stdout);

	/* Now look for high-level signals */
	rb_look_for_clocks();
	/* Not needed as simulator does't propagate clock signals */
}

/*---------------------------------------------------------------------------------------------
 * (function: read_tokens)
 *-------------------------------------------------------------------------------------------*/
static void read_tokens (char *buffer,int * done,char * blif_file)
{
	/* Figures out which, if any token is at the start of this line and *
	 * takes the appropriate action.                                    */
	char *ptr = my_strtok (buffer, TOKENS, blif, buffer); // ptr has the token now compare it with the standards tokens

	if (ptr)
	{
		if((skip_reading_bit_map==TRUE) &&((ptr == NULL) && ((ptr[0] == '0') || (ptr[0] == '1') || (ptr[0] == '-'))))
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
				/* not need for now */
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
	char *extracted_string=(char*)malloc(sizeof(char)*((end-start+2)));

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
	char ** names=NULL;       // Store the names of the tokens
	int input_token_count=0; /*to keep track whether controlling clock is specified or not */
	/*input_token_count=3 it is not and =5 it is */
	char *ptr;
	char buffer[BUFSIZE];
	while ((ptr= my_strtok (NULL, TOKENS, blif, buffer)) != NULL)
	{
		names = (char**)realloc(names, sizeof(char*)* (input_token_count + 1));
		names[input_token_count++]= strdup(ptr);
	}

	/* assigning the new_node */
	if(input_token_count != 5)
	{
		/* supported added for the ABC .latch output without control */
		if(input_token_count == 3)
		{
			char *clock_name=search_clock_name(blif_file);
			input_token_count=5;
			names = (char**)realloc(names, sizeof(char*)* input_token_count);

			if(clock_name) names[3]= strdup(clock_name);
			else           names[3]=NULL;

			names[4] = strdup(names[2]);
			names[2] = "re";
		}
		else
		{	
			printf("This .latch Format not supported \n\t required format :.latch <input> <output> [<type> <control/clock>] <initial val>");
			exit(-1);
		}
	}

	nnode_t *new_node = allocate_nnode();
	new_node->related_ast_node = NULL;
	new_node->type=FF_NODE;

	/* allocate the output pin (there is always one output pin) */
	allocate_more_node_output_pins(new_node, 1);
	add_output_port_information(new_node, 1);

	/* allocate the input pin */
	allocate_more_node_input_pins(new_node,2);/* input[1] is clock */
  
	/* add the port information */
	int i;
	for(i = 0; i < 2; i++)
		add_input_port_information(new_node,1);

	/* add names and type information to the created input pins */
	npin_t *new_pin = allocate_npin();
	new_pin->name=names[0];
	new_pin->type=INPUT;
	add_a_input_pin_to_node_spot_idx(new_node, new_pin,0);

	new_pin = allocate_npin();
	new_pin->name=names[3];
	new_pin->type=INPUT;
	add_a_input_pin_to_node_spot_idx(new_node, new_pin,1);

	/* add a name for the node, keeping the name of the node same as the output */
	new_node->name = make_full_ref_name(names[1],NULL, NULL, NULL,-1);

	/*add this node to verilog_netlist as an ff (flip-flop) node */
	verilog_netlist->ff_nodes = (nnode_t**)realloc(verilog_netlist->ff_nodes, sizeof(nnode_t*)*(verilog_netlist->num_ff_nodes+1));
	verilog_netlist->ff_nodes[verilog_netlist->num_ff_nodes] =new_node;
	verilog_netlist->num_ff_nodes++;

	/*add name information and a net(driver) for the output */
	nnet_t *new_net = allocate_nnet();
	new_net->name = new_node->name;

	new_pin = allocate_npin();
	new_pin->name = new_node->name;
	new_pin->type=OUTPUT;
	add_a_output_pin_to_node_spot_idx(new_node, new_pin, 0);
	add_a_driver_pin_to_net(new_net,new_pin);
  
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
	FILE * temp_read_blif = fopen(blif_file,"r");

	char ** input_names=NULL;
	int input_names_count=0;
	int found=0;
	while(!found)
	{
		char buffer[BUFSIZE];
		my_fgets(buffer,BUFSIZE,temp_read_blif);

		// not sure if this is needed
		if(feof(temp_read_blif)) break;

		char *ptr;
		if((ptr= my_strtok(buffer,TOKENS,temp_read_blif,buffer)))
		{
			if(!strcmp(ptr,".end")) break;

			if(strcmp(ptr,".inputs")==0)
			{
				/* store the inputs in array of string */
				while((ptr=my_strtok (NULL, TOKENS,temp_read_blif, buffer)))
				{
					if(strcmp(ptr,"top^reset_n"))
					{
						input_names=(char**)realloc(input_names,sizeof(char*) * (input_names_count + 1));
						input_names[input_names_count++]= strdup(ptr);
					}
				}
			}
			else if(strcmp(ptr,".names")==0 || strcmp(ptr,".latch")==0)
			{
				while((ptr=my_strtok (NULL, TOKENS,temp_read_blif, buffer)))
				{
					int i=0;
					for(i = 0; i < input_names_count; i++)
					{
						if(!strcmp(ptr,input_names[i]))
						{
						  free(input_names[i]);
						  input_names[i] = input_names[input_names_count-1];
						  input_names_count--;
						}
					}
				}
			}
			else if(input_names_count==1)
			{
				found = 1;
			}
		}
	}
	
	fclose(temp_read_blif);

	if(found) return input_names[0];
	else      return default_clock_name;
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
	char **names_parameters=NULL;
	while ((token = my_strtok (NULL, TOKENS, blif, buffer)) != NULL)
  	{
		names_parameters          = (char**)realloc(names_parameters, sizeof(char*)*(count + 1));
		names_parameters[count++] = strdup(token);
  	}	
   
	char **names_actual=(char**)malloc(sizeof(char*)*count);
	char **names_formal=(char**)malloc(sizeof(char*)*count);

	int i = 0;
	for (i = 0; i < count; i++)
	{
		names_actual[i] = strdup(strtok(names_parameters[i], "="));
		names_formal[i] = strdup(strtok(NULL, "="));
	}

	for(i = 0; i<count; i++)
		free(names_parameters[i]);

	free(names_parameters);

	// store the current position of the file pointer
	fpos_t pos;
	fgetpos(blif,&pos);
	FILE *temp_search=blif;

	while (1)
  	{
		my_fgets(buffer, BUFSIZE, temp_search);
		if(feof(temp_search))
		{
			error_message(NETLIST_ERROR,linenum, -1,"Error : The '%s' subckt no found ",name_subckt);
		}

		char *ptr = my_strtok(buffer,TOKENS, temp_search, buffer);
		if (ptr)
		{
			if(!strcmp(ptr,".model"))
			{
				if(!strcmp(my_strtok(NULL,TOKENS, temp_search, buffer),name_subckt))
					break;
			}
		}
	}

	/* read the subckt model to find out the number of input pin*/
	/* count-input_count=number of output pin */
	/* Thus we can decide number of passed parameter are output parameter */
	int done=0;
	int input_count=0;
	while (!done && (my_fgets (buffer, BUFSIZE,temp_search) != NULL) )
	{
		if(!strcmp(my_strtok (buffer, TOKENS, temp_search, buffer),".inputs"))
		{
			while ((my_strtok (NULL, TOKENS,temp_search, buffer)))
				input_count++;

			done = 1;
		}
	}

	nnode_t *new_node = allocate_nnode();

	/* calculate the number of input and output port size */
	int input_port_count=0;
	if(input_count > 0)
	{
		input_port_count = 1;

		for (i = 0; i < input_count-1; i++)
		{
			if(!strcmp(strtok(names_actual[i],"~"),strtok(names_actual[i+1],"~")))
			{
				input_port_count++;
			}
			else
			{
				add_input_port_information(new_node,input_port_count);
				input_port_count = 1;
			}
		}
		add_input_port_information(new_node,input_port_count);
	}

	int output_port_count=0;
	if(count-input_count > 0)
	{
		output_port_count = 1;

		for(i = input_count; i < count-input_count-1; i++)
		{
			if(!strcmp(strtok(names_actual[i],"~"),strtok(names_actual[i+1],"~")))
			{
				output_port_count++;
			}
			else
			{
				add_output_port_information(new_node,output_port_count);
				output_port_count = 1;
			}
		}
		add_output_port_information(new_node,output_port_count);
	}

	/* creating the node */
	// new_node->related_ast_node = NULL;
	if (!strcmp(name_subckt,"multiply"))
		new_node->type=MULTIPLY;
	else
		new_node->type=MEMORY;

	/* allocate the output pin (there is always one output pin) */
	allocate_more_node_output_pins(new_node,count-input_count);

	/* allocate the input pin (= input_count-1)*/
	if (input_count > 0) // check if there is any input pins
		allocate_more_node_input_pins(new_node, input_count);
  
	/* add names and type information to the created input pins */
  	for(i = 0; i < input_count; i++)
  	{
		new_node->input_pins[i]=allocate_npin();
		new_node->input_pins[i]->name=names_formal[i];
		new_node->input_pins[i]->type=INPUT;

		char *ptr = strtok(names_actual[i],"~");
		new_node->input_pins[i]->mapping=(char*)malloc(sizeof(char)*(strlen(ptr)+1));
		sprintf(new_node->input_pins[i]->mapping,"%s",ptr);
  	}

	/* Check this , I am not sure (simulator needs this information) */
	new_node->related_ast_node = (ast_node_t *)calloc(1, sizeof(ast_node_t));
	new_node->related_ast_node->children = (ast_node_t **)calloc(1,sizeof(ast_node_t *));
	new_node->related_ast_node->children[0] = (ast_node_t *)calloc(1, sizeof(ast_node_t));
	new_node->related_ast_node->children[0]->types.identifier = (char*)calloc(1,sizeof(char)*(strlen(name_subckt)+1));
	sprintf(new_node->related_ast_node->children[0]->types.identifier,"%s",name_subckt);

	/* add a name for the node,same as the subckt name */
	new_node->name = make_full_ref_name(name_subckt,NULL, NULL, NULL,-1);

	/*add this node to verilog_netlist as an internal node */
	verilog_netlist->internal_nodes = (nnode_t**)realloc(verilog_netlist->internal_nodes, sizeof(nnode_t*)*(verilog_netlist->num_internal_nodes+1));
	verilog_netlist->internal_nodes[verilog_netlist->num_internal_nodes] =new_node;
	verilog_netlist->num_internal_nodes++;
 
	/* Add name information and a net(driver) for the output */
  	for(i=0;i<count-input_count;i++)
  	{
		npin_t *new_pin = allocate_npin();
		new_pin->name=names_formal[i+input_count];
		new_pin->type=OUTPUT;

		add_a_output_pin_to_node_spot_idx(new_node, new_pin,i);

		char *ptr=strtok(names_actual[i+input_count],"~");
		new_node->output_pins[i]->mapping = (char*)malloc(sizeof(char)*(strlen(ptr)+1));
		sprintf(new_node->output_pins[i]->mapping,"%s",ptr);

		add_a_output_pin_to_node_spot_idx(new_node, new_pin,i);

		nnet_t *new_net = allocate_nnet();
		new_net->name = names_formal[i+input_count];
		add_a_driver_pin_to_net(new_net,new_pin);
		
		/*list this output in output_nets_sc */
		long sc_spot = sc_add_string(output_nets_sc,names_formal[i+input_count]);
		if (output_nets_sc->data[sc_spot])
		  	warning_message(NETLIST_ERROR,linenum, -1,"Net (%s) with the same name already created\n",names_formal[i+input_count]);

		/* store the data which is an idx here */
		output_nets_sc->data[sc_spot] = new_net;
	}
  	free(names_actual);
  	free(names_formal);

	fsetpos(blif,&pos);
}

/*---------------------------------------------------------------------------------------------
   * function:create_internal_node_and_driver
     to create an internal node and driver from that node 
*-------------------------------------------------------------------------------------------*/

void create_internal_node_and_driver()
{
	/* Storing the names of the input and the final output in array names */
	char *ptr;
	char ** names=NULL; // stores the names of the input and the output, last name stored would be of the output
	int input_count=0;
	char buffer[BUFSIZE];
	while ((ptr= my_strtok (NULL, TOKENS, blif, buffer)) != NULL)
	{
		names = (char**)realloc(names, sizeof(char*)* (input_count + 1));
		names[input_count++]= strdup(ptr);
	}
  
	/* assigning the new_node */
	nnode_t *new_node = allocate_nnode();
	new_node->related_ast_node = NULL;

	/* gnd vcc unconn already created as top module so ignore them */
	if((strcmp(names[input_count-1],"gnd")==0) ||  (strcmp(names[input_count-1],"vcc")==0) ||  (strcmp(names[input_count-1],"unconn")==0))
	{
		skip_reading_bit_map = TRUE;
	}
	else
	{

		/* assign the node type by seeing the name */
		short node_type = assign_node_type_from_node_name(names[input_count-1]);

		if(node_type != GENERIC)
		{
			new_node->type=node_type;
			skip_reading_bit_map=TRUE;
		}
		/* Check for GENERIC type , change the node by reading the bit map */
		else if(node_type == GENERIC)
		{
			new_node->type=read_bit_map_find_unknown_gate(input_count-1,new_node);
			skip_reading_bit_map=TRUE;
		}

		/* allocate the input pin (= input_count-1)*/
		if (input_count-1 > 0) // check if there is any input pins
		{
			allocate_more_node_input_pins(new_node, input_count-1);

			/* add the port information */
			if(new_node->type==MUX_2)
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

			npin_t *new_pin =allocate_npin();
			new_pin->name=(char *)GND;
			new_pin->type=INPUT;
			add_a_input_pin_to_node_spot_idx(new_node, new_pin,0);
		}

		if(new_node->type == VCC_NODE)
		{
			allocate_more_node_input_pins(new_node,1);
			add_input_port_information(new_node, 1);

			npin_t *new_pin = allocate_npin();
			new_pin->name=(char *)VCC;
			new_pin->type=INPUT;
			add_a_input_pin_to_node_spot_idx(new_node, new_pin,0);
		}

		/* allocate the output pin (there is always one output pin) */
		allocate_more_node_output_pins(new_node, 1);
		add_output_port_information(new_node, 1);

		/* add a name for the node, keeping the name of the node same as the output */
		new_node->name = make_full_ref_name(names[input_count-1],NULL, NULL, NULL,-1);

		/*add this node to verilog_netlist as an internal node */
		verilog_netlist->internal_nodes = (nnode_t**)realloc(verilog_netlist->internal_nodes, sizeof(nnode_t*)*(verilog_netlist->num_internal_nodes+1));
		verilog_netlist->internal_nodes[verilog_netlist->num_internal_nodes] =new_node;
		verilog_netlist->num_internal_nodes++;

		/*add name information and a net(driver) for the output */
		nnet_t *new_net = allocate_nnet();
		new_net->name=new_node->name;

		npin_t *new_pin = allocate_npin();
		new_pin->name=new_node->name;
		new_pin->type=OUTPUT;

		add_a_output_pin_to_node_spot_idx(new_node, new_pin, 0);
		add_a_driver_pin_to_net(new_net,new_pin);

		/* List this output in output_nets_sc */
		long sc_spot = sc_add_string(output_nets_sc,new_node->name );
		if (output_nets_sc->data[sc_spot] != NULL)
			warning_message(NETLIST_ERROR,linenum,-1, "Net (%s) with the same name already created\n",ptr);

		/* store the data which is an idx here */
		output_nets_sc->data[sc_spot] = (void*)new_net;

		/* Free the char** names */
		free(names);
	}
}

/*
*---------------------------------------------------------------------------------------------
   * function: read_bit_map_find_unknown_gate
     read the bit map for simulation
*-------------------------------------------------------------------------------------------*/
  
short read_bit_map_find_unknown_gate(int input_count,nnode_t * node)
{
	FILE * temp_fpointer;
	char buffer[BUFSIZE];
	fpos_t pos;// store the current position of the file pointer
	int i,j;
	int line_count_bitmap=0; //stores the number of lines in a particular bit map
	char ** bit_map=NULL;
	char *output_bit_map;// to distinguish whether for the bit_map output is 1 or 0
		
	fgetpos(blif,&pos);
	temp_fpointer=blif;

	if(!input_count)
	{
		my_fgets (buffer, BUFSIZE, temp_fpointer);

		fsetpos(blif,&pos);

		char *ptr = my_strtok(buffer,"\t\n",temp_fpointer, buffer);
		if      (strcmp(ptr," 0")==0) return GND_NODE;
		else if (strcmp(ptr," 1")==1) return VCC_NODE;
		else if (!ptr)                return GND_NODE;
		else                          return VCC_NODE;
	}
	
	while(1)
	{
		my_fgets (buffer, BUFSIZE, temp_fpointer);
		if(!(buffer[0]=='0' || buffer[0]=='1' || buffer[0]=='-'))
			break;

		bit_map=(char**)realloc(bit_map,sizeof(char*) * (line_count_bitmap + 1));
		bit_map[line_count_bitmap++] = strdup(my_strtok(buffer,TOKENS,temp_fpointer, buffer));

		output_bit_map = strdup(my_strtok(NULL,TOKENS,temp_fpointer, buffer));
	}

	fsetpos(blif,&pos);

	/* Single line bit map : */
	if(line_count_bitmap==1)
	{
		// GT
		if(strcmp(bit_map[0],"100") == 0) return GT;
		// LT
		if(strcmp(bit_map[0],"010") == 0) return LT;

		/* LOGICAL_AND and LOGICAL_NAND for ABC*/
		for(i = 0; i < input_count && bit_map[0][i] == '1'; i++);
		if(i == input_count)
		{
			if      (strcmp(output_bit_map,"1") == 0) return LOGICAL_AND;
			else if (strcmp(output_bit_map,"0") == 0) return LOGICAL_NAND;
		}

		/* BITWISE_NOT */
		if(strcmp(bit_map[0],"0") == 0) return BITWISE_NOT;

		/* LOGICAL_NOR and LOGICAL_OR for ABC */
		for(i = 0; i < input_count && bit_map[0][i] == '0'; i++);
		if(i == input_count)
		{
			if      (strcmp(output_bit_map,"1")==0) return LOGICAL_NOR;
			else if (strcmp(output_bit_map,"0")==0) return LOGICAL_OR;
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
		if((strcmp(bit_map[0],"001")==0) && (strcmp(bit_map[1],"010")==0) && (strcmp(bit_map[2],"100")==0) && (strcmp(bit_map[3],"111")==0))
			return ADDER_FUNC;
		/* CARRY_FUNC */
		if((strcmp(bit_map[0],"011")==0) && (strcmp(bit_map[1],"100")==0) && (strcmp(bit_map[2],"110")==0) && (strcmp(bit_map[3],"111")==0))
			return 	CARRY_FUNC;
		/* LOGICAL_XOR */
		if((strcmp(bit_map[0],"001")==0) && (strcmp(bit_map[1],"010")==0) && (strcmp(bit_map[2],"100")==0) && (strcmp(bit_map[3],"111")==0))
			return 	LOGICAL_XOR;
		/* LOGICAL_XNOR */
		if((strcmp(bit_map[0],"000")==0) && (strcmp(bit_map[1],"011")==0) && (strcmp(bit_map[2],"101")==0) && (strcmp(bit_map[3],"110")==0))
			return 	LOGICAL_XNOR;
	}
  

	if(line_count_bitmap == input_count)
	{
		/* LOGICAL_OR */
		for(i = 0; i < line_count_bitmap; i++)
		{
			if(bit_map[i][i]=='1')
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

		if(i == line_count_bitmap) return LOGICAL_OR;

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
		if(i == line_count_bitmap) return LOGICAL_NAND;
	}

	/* MUX_2 */
	if(line_count_bitmap*2 == input_count)
	{
		for(i=0;i<line_count_bitmap;i++)
		{
			if((bit_map[i][i]=='1') && (bit_map[i][i+line_count_bitmap] =='1'))
			{
				for(j=1;j<line_count_bitmap;j++)
					if((bit_map[i][(i+j)%line_count_bitmap]!='-') || (bit_map[i][((i+j)%line_count_bitmap)+line_count_bitmap]!='-'))
						break;

				if(j!=input_count) break;
			}
			else
			{
				break;
			}
		}

		if(i == line_count_bitmap) return MUX_2;
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
		/* create a new top input node and net*/
		nnet_t *new_net = allocate_nnet();
		nnode_t *new_node = allocate_nnode();

		char *temp_string = make_full_ref_name(ptr, NULL, NULL,NULL, -1);
		long sc_spot = sc_add_string(output_nets_sc, temp_string);
		if (output_nets_sc->data[sc_spot] != NULL)
		{
			warning_message(NETLIST_ERROR,linenum,-1, "Net (%s) with the same name already created\n",ptr);
		}

		/* store the data which is an idx here */
		output_nets_sc->data[sc_spot] = (void*)new_net;

		new_node->related_ast_node = NULL;
		new_node->type = INPUT_NODE;

		/* add the name of the input variable */
		new_node->name = temp_string;

		/* allocate the pins needed */
		allocate_more_node_output_pins(new_node, 1);
		add_output_port_information(new_node, 1);

		/* Create the pin connection for the net */
		npin_t *new_pin = allocate_npin();

		/* hookup the pin, net, and node */
		add_a_output_pin_to_node_spot_idx(new_node, new_pin, 0);
		add_a_driver_pin_to_net(new_net, new_pin);

		/*adding the node to the verilog_netlist input nodes
		add_node_to_netlist() function can also be used */

		verilog_netlist->top_input_nodes = (nnode_t**)realloc(verilog_netlist->top_input_nodes, sizeof(nnode_t*)*(verilog_netlist->num_top_input_nodes+1));
		verilog_netlist->top_input_nodes[verilog_netlist->num_top_input_nodes] = new_node;
		verilog_netlist->num_top_input_nodes++;
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
		char *temp_string= make_full_ref_name(ptr, NULL, NULL,NULL, -1);;

		/* Create the pin connection for the net */
		npin_t *new_pin = allocate_npin();
		new_pin->name=temp_string;

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

		/* hookup the pin, net, and node */
		add_a_input_pin_to_node_spot_idx(new_node, new_pin, 0);

		/*adding the node to the verilog_netlist output nodes
		add_node_to_netlist() function can also be used */
		verilog_netlist->top_output_nodes = (nnode_t**)realloc(verilog_netlist->top_output_nodes, sizeof(nnode_t*)*(verilog_netlist->num_top_output_nodes+1));
		verilog_netlist->top_output_nodes[verilog_netlist->num_top_output_nodes] = new_node;
		verilog_netlist->num_top_output_nodes++;
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
	if (output_nets_sc->data[sc_spot] != NULL)
	{
		error_message(NETLIST_ERROR,-1,-1, "Error in Odin\n");
	}

	/* store the data which is an idx here */
	output_nets_sc->data[sc_spot] = (void*)verilog_netlist->zero_net;
	verilog_netlist->zero_net->name =blif_zero_string;

	/* CREATE the driver for the ONE and store twice */
	blif_one_string = make_full_ref_name(instance_name_prefix, NULL, NULL, one_string, -1);
	verilog_netlist->vcc_node->name = (char *)VCC;

	sc_spot = sc_add_string(output_nets_sc, (char *)VCC);
	if (output_nets_sc->data[sc_spot] != NULL)
	{
		error_message(NETLIST_ERROR,-1,-1, "Error in Odin\n");
	}
	/* store the data which is an idx here */
	output_nets_sc->data[sc_spot] = (void*)verilog_netlist->one_net;
	verilog_netlist->one_net->name =blif_one_string;

	/* CREATE the driver for the PAD */
	blif_pad_string = make_full_ref_name(instance_name_prefix, NULL, NULL, pad_string, -1);
	verilog_netlist->pad_node->name = (char *)HBPAD;

	sc_spot = sc_add_string(output_nets_sc, (char *)HBPAD);
	if (output_nets_sc->data[sc_spot] != NULL)
	{
		error_message(NETLIST_ERROR, -1,-1, "Error in Odin\n");
	}
	/* store the data which is an idx here */
	output_nets_sc->data[sc_spot] = (void*)verilog_netlist->pad_net;
	verilog_netlist->pad_net->name = blif_pad_string;
}

/*---------------------------------------------------------------------------------------------
 * (function: dum_parse)
 *-------------------------------------------------------------------------------------------*/
static void dum_parse (char *buffer)
{
	/* Continue parsing to the end of this (possibly continued) line. */
	while (my_strtok (NULL, TOKENS, blif, buffer) != NULL);
}



/*---------------------------------------------------------------------------------------------
 * function: hook_up_nets() 
 * find the output nets and add the corrosponding nets   *-------------------------------------------------------------------------------------------*/

void hook_up_nets()
{
	/* hook all the input pins in all the internal nodes to the net */
	int i;
	for(i = 0; i < verilog_netlist->num_internal_nodes; i++)
	{
		nnode_t *node = verilog_netlist->internal_nodes[i];

		int j;
		for(j = 0; j < node->num_input_pins; j++)
		{
			npin_t *input_pin = node->input_pins[j];

			long sc_spot = sc_lookup_string(output_nets_sc,input_pin->name);
			if(sc_spot == -1) error_message(NETLIST_ERROR,linenum, -1, "Error :Could not hook the pin %s not available ", input_pin->name);

			nnet_t *output_net = output_nets_sc->data[sc_spot];
			/* add the pin to this net as fanout pin */
			add_a_fanout_pin_to_net(output_net,input_pin);
		}
	}

	/* hook all the ff nodes' input pin to the nets */
	for(i=0;i<verilog_netlist->num_ff_nodes;i++)
	{
		nnode_t *node = verilog_netlist->ff_nodes[i];

		int j;
		for(j = 0; j < node->num_input_pins; j++)
		{
			npin_t *input_pin=node->input_pins[j];

			long sc_spot=sc_lookup_string(output_nets_sc,input_pin->name);
			if(sc_spot == -1) error_message(NETLIST_ERROR,linenum, -1, "Error :Could not hook the pin %s not available ", input_pin->name);

			nnet_t *output_net = output_nets_sc->data[sc_spot];
			/* add the pin to this net as fanout pin */
			add_a_fanout_pin_to_net(output_net,input_pin);
		}
	}

	/* hook the top output nodes input pins */
	for(i = 0; i < verilog_netlist->num_top_output_nodes; i++)
	{
		nnode_t *node = verilog_netlist->top_output_nodes[i];

		int j;
		for(j = 0; j<node->num_input_pins; j++)
		{
			npin_t *input_pin=node->input_pins[j];

			long sc_spot = sc_lookup_string(output_nets_sc,input_pin->name);
			if(sc_spot == -1) error_message(NETLIST_ERROR,linenum, -1, "Error:Could not hook the pin %s not available ", input_pin->name);

			nnet_t *output_net = output_nets_sc->data[sc_spot];
			/* add the pin to this net as fanout pin */
			add_a_fanout_pin_to_net(output_net,input_pin);
		}
	}
}
