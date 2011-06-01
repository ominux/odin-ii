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
#include "read_blif_new.h"
#include "util.h"
#include "string_cache.h"
#include "netlist_utils.h"
#include "errors.h"
#include "netlist_utils.h"
#include "types.h"

//#define debug_mode

#define TOKENS " \t\n"


static FILE * blif;
int linenum;

STRING_CACHE *output_nets_sc;
STRING_CACHE *input_nets_sc;
STRING_CACHE *top_output_nodes_sc;

const char *GND ="gnd";
const char *VCC="vcc";
const char *HBPAD="hbpad";

char *blif_one_string = "ONE_VCC_CNS";
char *blif_zero_string = "ZERO_GND_ZERO";
char *blif_pad_string = "ZERO_PAD_ZERO";

netlist_t * blif_netlist; 
int linenum;/* keeps track of the present line, used for printing the error line : line number */
short static skip_reading_bit_map=FALSE; 

void create_top_driver_nets( char *instance_name_prefix);
void look_for_clocks();// not sure if this is needed
void add_top_input_nodes();
void create_top_output_nodes();
static void read_tokens (char *buffer,int * done);
static void dum_parse ( char *buffer); 
void create_internal_node_and_driver();
short assign_node_type_from_node_name(char * output_name);// function will decide the node->type of the given node
short read_bit_map_find_unknown_gate();
void create_latch_node_and_driver();
void create_hard_block_nodes();
void hook_up_nets();


void read_blif_new(char * blif_file)
{

      int done;
      char buffer[BUFSIZE]; /* for storing the tokens */

      /* initialize the string caches that hold the aliasing of module nets and input pins */
      output_nets_sc = sc_new_string_cache();
      top_output_nodes_sc = sc_new_string_cache();
      input_nets_sc = sc_new_string_cache();

      blif_netlist = allocate_netlist();
      /*Opening the blif file */
      blif = my_fopen (blif_file, "r", 0);
     

#ifdef debug_mode
  	printf("reading top level module");
#endif

      /* create the top level module */	    
      create_top_driver_nets("top");

      /* Extracting the netlist by reading the blif file */

	linenum = 0;
	done = 0;
	while (!done && (my_fgets (buffer, BUFSIZE, blif) != NULL) )
	{       
		read_tokens(buffer,&done);/* read one token at a time */
	}

	fclose (blif);
 	
#ifdef debug_mode  
	printf("\nreading look_for_clock\n");
#endif
      /* now look for high-level signals */
	look_for_clocks(); 

}



/*---------------------------------------------------------------------------------------------
 * (function: read_tokens)
 *-------------------------------------------------------------------------------------------*/

static void read_tokens (char *buffer,int * done)
{
	
	/* Figures out which, if any token is at the start of this line and *
	 * takes the appropriate action.                                    */
	char *ptr; // pointer to the tokens

        ptr = my_strtok (buffer, TOKENS, blif, buffer); // ptr has the token now compare it with the standards tokens

#ifdef debug_mode
	printf("\nreading the read_tokens : %s",ptr);
#endif
	
	if (ptr == NULL)
		return;
		

	if((skip_reading_bit_map==TRUE) &&((ptr == NULL) && ((ptr[0] == '0') || (ptr[0] == '1') || (ptr[0] == '-'))))
 	{
	  dum_parse(buffer);
	  return ;
   	}
	
	skip_reading_bit_map= FALSE;	
	if (strcmp (ptr, ".inputs") == 0) 
	{
	/* Create an input node */
	   add_top_input_nodes();// create the top input nodes
	   return;
	}	

	if (strcmp (ptr, ".outputs") == 0)
	{
	/* Create output nodes */
	   create_top_output_nodes();// create the top output nodes
	   return;
	}

	if (strcmp (ptr, ".names") == 0)
	{
	  /* create the internal node for a .name (gate)*/	
	  create_internal_node_and_driver();
          return;
	}

	if(strcmp(ptr,".latch") == 0)
	{
	/* create the ff node */	
	create_latch_node_and_driver();
	return;
	}

	if(strcmp(ptr,".subckt") == 0)
	{
#ifdef debug_mode
	printf("\n reading the subckt module \n");
#endif	
	/* create the ff node */	
	create_hard_block_nodes();
	return;
	}

	if(strcmp(ptr,".end")==0)
	{
	/* marks the end of the main module of the blif */
	/* call functions to hook up the nets */
	hook_up_nets();
	*done=1;
	return;
	}

	if(strcmp(ptr,".model")==0)
	{
	/* not need for now */
	dum_parse(buffer);
	return;
	}
	


}



/*---------------------------------------------------------------------------------------------
   * function:assign_node_type_from_node_name(char *)
     This function tries to assign the node->type by looking at the name
     If it fails 
*-------------------------------------------------------------------------------------------*/
short assign_node_type_from_node_name(char * output_name)
{

#ifdef debug_mode
	printf("\n reading assign_node_type_from_node_name");
	printf("\n output_name : %s",output_name);
#endif

  int length_string;
  int start,end,i; //variable to extract the type
  int j=0;
  char *extracted_string;//stores the extracted string
  length_string=strlen(output_name);
  for(start=length_string-1;output_name[start]!='^';start--);
  for(end=length_string-1;output_name[end]!='~';end--);
  extracted_string=(char*)malloc(sizeof(char)*((end-start+2)));
  for(i=start+1;i<end;i++)
  {
    extracted_string[j]=output_name[i];
    j++;
  }
  extracted_string[j]='\0';
  /* Check if the type existe, return the type else return -1 */
  /* if construst use , as we have strings here */
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
  else return -1;


}

/*---------------------------------------------------------------------------------------------
   * function:create_latch_node_and_driver
     to create an ff node and driver from that node 
     format .latch <input> <output> [<type> <control/clock>] <initial val>
*-------------------------------------------------------------------------------------------*/
void create_latch_node_and_driver()
{

#ifdef debug_mode
	printf("\n Reading the create_latch_node and driver ");
#endif
  
  long sc_spot;
  nnode_t *new_node = allocate_nnode();
  npin_t *new_pin;
  nnet_t *new_net;
  char buffer[BUFSIZE];
  char *ptr;
  int input_token_count=0;/*to keep track whether controlling clock is specified or not */
			  /*input_token_count=3 it is not and =5 it is */
  int i;
  char ** names=NULL;// Store the names of the tokens
  /* Storing the names of the input and the final output in array names */
 	while ((ptr= my_strtok (NULL, TOKENS, blif, buffer)) != NULL)
  	{
#ifdef debug_mode
    	  printf("\n\t ptr=%s",ptr);
#endif
          input_token_count++;
    	  names= (char**)realloc(names, sizeof(char*)* input_token_count);  
    	  names[input_token_count-1]=(char*)malloc(sizeof(char)*(strlen(ptr)+1));
  	  sprintf(names[input_token_count-1],"%s",ptr);
  	}
  /* assigning the new_node */

#ifdef debug_mode
printf("\n\tinput_token_count=%d",input_token_count);
#endif

   	if(input_token_count!=5)
  	{
   	  printf("This .latch Format not supported \n\t required format :.latch <input> <output> [<type> <control/clock>] <initial val>");
	  exit(-1);
	}
  new_node->related_ast_node = NULL;
  new_node->type=FF_NODE;
  
  /* allocate the output pin (there is always one output pin) */
  allocate_more_node_output_pins(new_node, 1);
  add_output_port_information(new_node, 1);

  /* allocate the input pin */
  allocate_more_node_input_pins(new_node,2);/* input[1] is clock */ 
  
  /* add the port information */
  for(i=0;i<2;i++)
  {
    add_input_port_information(new_node,1); 
  }

  /* add names and type information to the created input pins */
  new_pin=allocate_npin();
  new_pin->name=names[0];
  new_pin->type=INPUT;
  add_a_input_pin_to_node_spot_idx(new_node, new_pin,0);

  new_pin=allocate_npin();
  new_pin->name=names[3];
  new_pin->type=INPUT;
  add_a_input_pin_to_node_spot_idx(new_node, new_pin,1);


  /* add a name for the node, keeping the name of the node same as the output */
  new_node->name = make_full_ref_name(names[1],NULL, NULL, NULL,-1);  
  	

  /*add this node to blif_netlist as an ff (flip-flop) node */
  blif_netlist->ff_nodes = (nnode_t**)realloc(blif_netlist->ff_nodes, sizeof(nnode_t*)*(blif_netlist->num_ff_nodes+1));
  blif_netlist->ff_nodes[blif_netlist->num_ff_nodes] =new_node;
  blif_netlist->num_ff_nodes++;

  /*add name information and a net(driver) for the output */
  new_pin=allocate_npin();
  new_net=allocate_nnet();
  new_net->name=new_node->name;
  new_pin->name=new_node->name;
  new_pin->type=OUTPUT;
  add_a_output_pin_to_node_spot_idx(new_node, new_pin, 0);
  add_a_driver_pin_to_net(new_net,new_pin);
  
  /*list this output in output_nets_sc */
   sc_spot = sc_add_string(output_nets_sc,new_node->name);
   	if (output_nets_sc->data[sc_spot] != NULL)
   	{
     	  error_message(NETLIST_ERROR,linenum,-1, "Net (%s) with the same name already created\n",ptr);	
   	}

  /* store the data which is an idx here */
  output_nets_sc->data[sc_spot] = (void*)new_net;

  /* Free the char** names */
  free(names);

#ifdef debug_mode
	printf("\n exiting the create_latch_node and driver ");
#endif

}

/*---------------------------------------------------------------------------------------------
   * function:create_hard_block_nodes
     to create the hard block nodes
*-------------------------------------------------------------------------------------------*/
void create_hard_block_nodes()
{
  FILE *temp_search; // for searching the subckt model
  char buffer[BUFSIZE];
  char *name_subckt;
  char *temp1,*temp2;
  char **names_formal=NULL; // store the formal-actual parameter name
  char **names_actual=NULL;
  char **names_parameters=NULL;
  int count=0;
  char *ptr;
  int done=0;
  int input_count=0;
  long sc_spot;
  nnode_t *new_node = allocate_nnode();
  npin_t *new_pin;
  nnet_t *new_net;
  int input_port_count=0;
  int output_port_count=0;
  int i=0;
  fpos_t pos;// store the current position of the file pointer

	
  ptr= my_strtok (NULL, TOKENS, blif, buffer);
  name_subckt=(char*)malloc(sizeof(char)*(strlen(ptr)+1));
  sprintf(name_subckt,"%s",ptr);

#ifdef debug_mode
  printf("\nname_subckt: %s",name_subckt);
#endif
 
  /* storing the names on the fornal-actual parameter */
    	while ((ptr= my_strtok (NULL, TOKENS, blif, buffer)) != NULL)
  	{

#ifdef debug_mode
    	  printf("\n ptr: %s",ptr);
#endif

	  count++;
    	  names_parameters= (char**)realloc(names_parameters, sizeof(char*)*count);  
	  names_parameters[count-1]=(char*)malloc(sizeof(char)*(strlen(ptr)+1));
	  sprintf(names_parameters[count-1],"%s",ptr);
  	}	
   
   names_actual=(char**)malloc(sizeof(char*)*count);
   names_formal=(char**)malloc(sizeof(char*)*count);

   while(i<count)
	{
	  ptr=strtok(names_parameters[i],"=");
#ifdef debug_mode	  
	printf("\n ptr: %s",ptr);
#endif

	  names_actual[i]=(char*)malloc(sizeof(char)*(strlen(ptr)+1));
	  sprintf(names_actual[i],"%s",ptr);
	  ptr=strtok(NULL,"=");
	  names_formal[i]=(char*)malloc(sizeof(char)*(strlen(ptr)+1));
	  sprintf(names_formal[i],"%s",ptr);
	  i++;
	}

	for(i=0;i<count;i++)
	{
	free(names_parameters[i]);

#ifdef debug_mode	
	printf("\n %s \t%s",names_actual[i],names_formal[i]);
#endif

	}
       	free(names_parameters);   

  fgetpos(blif,&pos);
  temp_search=blif;

	while (1)
  	{
	  my_fgets(buffer, BUFSIZE,temp_search);
	  if(feof(temp_search)!=0)
	  {
		printf("Error : The '%s' subckt no found ",name_subckt);
		exit(-1);
	  }
	ptr=my_strtok(buffer,TOKENS, temp_search, buffer);
#ifdef debug_mode	
	printf("\n ptr : %s",ptr);
#endif
	if (ptr==NULL)
	  continue;
	if(strcmp(ptr,".model")!=0)
	continue;

	ptr=my_strtok(NULL,TOKENS, temp_search, buffer);
	if(strcmp(ptr,name_subckt)==0)
		break;
	
	}

  /* read the subckt model to find out the number of input pin*/
  /* count-input_count=number of output pin */
  /* Thus we can decide number of passed parameter are output parameter */

  	while (!done && (my_fgets (buffer, BUFSIZE,temp_search) != NULL) )
	{
	  ptr = my_strtok (buffer, TOKENS, temp_search, buffer);  

#ifdef debug_mode	
	  printf("\n ptr: %s",ptr);  
#endif

	  if(strcmp(ptr,".inputs")==0)
	  {
	    	while ((ptr= my_strtok (NULL, TOKENS,temp_search, buffer)) != NULL)
    	 	 input_count++;
		done=1;
	  }
	}

#ifdef debug_mode
	printf("\n input_count %d",input_count);
	printf("\n count %d",count); 
#endif

  /* calculate the number of input and output port size */
  if(input_count>0)
  {
  input_port_count=1;

	  for(i=0;i<input_count-1;i++)
	  {  
	     ptr=strtok(names_actual[i+1],"~");
	     temp2=ptr;
 	     ptr=strtok(names_actual[i],"~");		
	     temp1=ptr;

#ifdef debug_mode
		printf("\n %s \t %s",temp1,temp2);
#endif
	
		if(strcmp(temp1,temp2)!=0)
		{
		add_input_port_information(new_node,input_port_count);
		input_port_count=1;
		}
		else
		input_port_count++;
	  }
	add_input_port_information(new_node,input_port_count);	
  }

  if(count-input_count>0)
  {
  output_port_count=1;

	  for(i=input_count;i<count-input_count-1;i++)
	  {  
	     ptr=strtok(names_actual[i+1],"~");
	     temp2=ptr;
 	     ptr=strtok(names_actual[i],"~");		
	     temp1=ptr;

#ifdef debug_mode
		printf("\n %s \t %s",temp1,temp2);
#endif

		if(strcmp(temp1,temp2)!=0)
		{
		add_output_port_information(new_node,output_port_count);
		output_port_count=1;
		}
		else
		output_port_count++;
	  }	
	add_output_port_information(new_node,output_port_count);
  }

  /* creating the node */
  // new_node->related_ast_node = NULL;
  if(strcmp(name_subckt,"multiply")==0)
   new_node->type=MULTIPLY;
  else
   new_node->type=MEMORY;
   
  /* allocate the output pin (there is always one output pin) */
    allocate_more_node_output_pins(new_node,count-input_count);

  /* allocate the input pin (= input_count-1)*/
 	 if (input_count > 0) // check if there is any input pins	
  	{  
    	  allocate_more_node_input_pins(new_node, input_count);  
  	}
  
  /* add names and type information to the created input pins */
  	for(i=0;i<input_count;i++)
  	{
    	  new_node->input_pins[i]=allocate_npin();
    	  new_node->input_pins[i]->name=names_formal[i];
    	  new_node->input_pins[i]->type=INPUT;
	  ptr=strtok(names_actual[i],"~");
	  new_node->input_pins[i]->mapping=(char*)malloc(sizeof(char)*(strlen(ptr)+1));		
	  sprintf(new_node->input_pins[i]->mapping,"%s",ptr);
  	}
	 
  /* Check this , I am not sure (simulator needs this information) */
  new_node->related_ast_node=(ast_node_t *)calloc(1, sizeof(ast_node_t));
  new_node->related_ast_node->children=(ast_node_t **)calloc(1,sizeof(ast_node_t *)); 
  new_node->related_ast_node->children[0]=(ast_node_t *)calloc(1, sizeof(ast_node_t));
  new_node->related_ast_node->children[0]->types.identifier=(char*)calloc(1,sizeof(char)*(strlen(name_subckt)+1));
  sprintf(new_node->related_ast_node->children[0]->types.identifier,"%s",name_subckt);

  /* add a name for the node,same as the subckt name */
  new_node->name = make_full_ref_name(name_subckt,NULL, NULL, NULL,-1);  

  /*add this node to blif_netlist as an internal node */
  blif_netlist->internal_nodes = (nnode_t**)realloc(blif_netlist->internal_nodes, sizeof(nnode_t*)*(blif_netlist->num_internal_nodes+1));
  blif_netlist->internal_nodes[blif_netlist->num_internal_nodes] =new_node;
  blif_netlist->num_internal_nodes++;
 
  /*add name information and a net(driver) for the output */
  	for(i=0;i<count-input_count;i++)
  	{
	
     	new_pin=allocate_npin();
  	new_net=allocate_nnet();
	new_net->name=names_formal[i+input_count];
 	new_pin->name=names_formal[i+input_count];
  	new_pin->type=OUTPUT;
	ptr=strtok(names_actual[i+input_count],"~");

#ifdef debug_mode
	printf("\n ptr  %s\n",ptr);
#endif

	add_a_output_pin_to_node_spot_idx(new_node, new_pin,i);
	new_node->output_pins[i]->mapping=(char*)malloc(sizeof(char)*(strlen(ptr)+1));		
	sprintf(new_node->output_pins[i]->mapping,"%s",ptr);
  	add_a_output_pin_to_node_spot_idx(new_node, new_pin,i);
  	add_a_driver_pin_to_net(new_net,new_pin);
		
  /*list this output in output_nets_sc */
	sc_spot = sc_add_string(output_nets_sc,names_formal[i+input_count]);
   		if (output_nets_sc->data[sc_spot] != NULL)
   		{
     	  	error_message(NETLIST_ERROR,linenum, -1,"Net (%s) with the same name already created\n",ptr);	
   		}
  	  /* store the data which is an idx here */
  	output_nets_sc->data[sc_spot] = (void*)new_net;
	}

  /* Free the char**   */
  free(names_formal); 
  free(names_actual);
  free(names_parameters);
  fsetpos(blif,&pos);

#ifdef debug_mode
  printf("\n exiting the create_hard_block module");
#endif
	 
}

/*---------------------------------------------------------------------------------------------
   * function:create_internal_node_and_driver
     to create an internal node and driver from that node 
*-------------------------------------------------------------------------------------------*/

void create_internal_node_and_driver()
{

#ifdef debug_mode
	printf("\n reading create_internal_node_driver");
#endif

  long sc_spot;
  nnode_t *new_node = allocate_nnode();
  npin_t *new_pin;
  nnet_t *new_net;
  short node_type;
  char buffer[BUFSIZE];
  char *ptr;
  char ** names=NULL;// stores the names of the input and the output, last name stored would be of the output
  int input_count=0;
  int i;

  /* Storing the names of the input and the final output in array names */
 	while ((ptr= my_strtok (NULL, TOKENS, blif, buffer)) != NULL)
  	{
    	  input_count++;
    	  names= (char**)realloc(names, sizeof(char*)* input_count);  
    	  names[input_count-1]=(char*)malloc(sizeof(char)*(strlen(ptr)+1));
	  sprintf(names[input_count-1],"%s",ptr);
  	}
  
  /* assigning the new_node */
  new_node->related_ast_node = NULL;
  /* gnd vcc hbpad already created as top module so ignore them */
  if( (strcmp(names[input_count-1],"gnd")==0) ||  (strcmp(names[input_count-1],"vcc")==0) ||  (strcmp(names[input_count-1],"hbpad")==0))
 	{
	skip_reading_bit_map=TRUE;
 	return;
	}

#ifdef debug_mode
  printf("\nnames %s",names[input_count-1]); 
#endif

/* can be a top output node, checking if it a top output node*/
	sc_spot=sc_lookup_string(top_output_nodes_sc,names[input_count-1]);

#ifdef debug_mode	
	  printf("sc_spot : %ld \n",sc_spot);
#endif

		if(sc_spot!=(-1))
		{
		top_output_nodes_sc->data[sc_spot] = (void*)names[0];
		return;
		}

  node_type=assign_node_type_from_node_name(names[input_count-1]);
  
#ifdef debug_mode
  	printf("\n exiting assign_node_type_from_node_name\n");
  	printf("\n node_type= %d\n",node_type);  
#endif  	

	if(node_type != -1)
  	{
    	  new_node->type =node_type;
    	  skip_reading_bit_map=TRUE;
  	}
  	else
  	{ 
    	  new_node->type= read_bit_map_find_unknown_gate();
  	}

  
  /* allocate the input pin (= input_count-1)*/
 	 if (input_count-1 > 0) // check if there is any input pins	
  	{  
    	  allocate_more_node_input_pins(new_node, input_count-1);

 /* add the port information */
    	  for(i=0;i<input_count-1;i++)
	  { 
	  	add_input_port_information(new_node, 1);
	  }  
  	}
  /* add names and type information to the created input pins */
  	for(i=0;i<=input_count-2;i++)
  	{
    	  new_pin=allocate_npin();
	  new_pin->name=names[i];
	  new_pin->type=INPUT;
	  add_a_input_pin_to_node_spot_idx(new_node, new_pin, i);
  	}

 
  /* allocate the output pin (there is always one output pin) */
  allocate_more_node_output_pins(new_node, 1);
  add_output_port_information(new_node, 1);

 /* add a name for the node, keeping the name of the node same as the output */
  new_node->name = make_full_ref_name(names[input_count-1],NULL, NULL, NULL,-1);   
  
  /*add this node to blif_netlist as an internal node */
  blif_netlist->internal_nodes = (nnode_t**)realloc(blif_netlist->internal_nodes, sizeof(nnode_t*)*(blif_netlist->num_internal_nodes+1));
  blif_netlist->internal_nodes[blif_netlist->num_internal_nodes] =new_node;
  blif_netlist->num_internal_nodes++;
 

  /*add name information and a net(driver) for the output */
  new_pin=allocate_npin();
  new_net=allocate_nnet();
  new_net->name=new_node->name;
  new_pin->name=new_node->name;
  new_pin->type=OUTPUT;
  add_a_output_pin_to_node_spot_idx(new_node, new_pin, 0);
  add_a_driver_pin_to_net(new_net,new_pin);
  
  /*list this output in output_nets_sc */
   sc_spot = sc_add_string(output_nets_sc,new_node->name );
   	if (output_nets_sc->data[sc_spot] != NULL)
   	{
     	  error_message(NETLIST_ERROR,linenum,-1, "Net (%s) with the same name already created\n",ptr);	
   	}
  	
  /* store the data which is an idx here */
  output_nets_sc->data[sc_spot] = (void*)new_net;

  /* Free the char** names */
  free(names);

#ifdef debug_mode
	printf("\n exiting create_internal_node_driver");
#endif

}

/*
*---------------------------------------------------------------------------------------------
   * function: read_bit_map_find_unknown_gate
     read the bit map for simulation
*-------------------------------------------------------------------------------------------*/

  
short read_bit_map_find_unknown_gate()
{

	printf("read_bit_map_find_unknown_gate not written till now");
	exit(-1);
	
}

/*
*---------------------------------------------------------------------------------------------
   * function: add_top_input_nodes
     to add the top level inputs to the netlist 
*-------------------------------------------------------------------------------------------*/
void add_top_input_nodes()
{

#ifdef debug_mode
  printf("\n Reading the add_top_input_nodes");
#endif

  long sc_spot;// store the location of the string stored in string_cache
  char *ptr; 
  char buffer[BUFSIZE];
  npin_t *new_pin;
  nnet_t *new_net = NULL;
  nnode_t *new_node;
  char *temp_string;

  while (1)
	{
	/* keep reading the inputs till there is no more to read*/
		ptr = my_strtok (NULL, TOKENS, blif, buffer);
	
		if (ptr == NULL)
			return;

	/* create a new top input node and net*/
	new_net= allocate_nnet();
	new_node = allocate_nnode();

	temp_string= make_full_ref_name(ptr, NULL, NULL,NULL, -1);
        sc_spot = sc_add_string(output_nets_sc, temp_string);
	if (output_nets_sc->data[sc_spot] != NULL)
	     {
	      error_message(NETLIST_ERROR,linenum,-1, "Net (%s) with the same name already created\n",ptr);	
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
	new_pin = allocate_npin();

	/* hookup the pin, net, and node */
	add_a_output_pin_to_node_spot_idx(new_node, new_pin, 0);
	add_a_driver_pin_to_net(new_net, new_pin);

	/*adding the node to the blif_netlist input nodes
	  add_node_to_netlist() function can also be used */
	
	blif_netlist->top_input_nodes = (nnode_t**)realloc(blif_netlist->top_input_nodes, sizeof(nnode_t*)*(blif_netlist->num_top_input_nodes+1));
	blif_netlist->top_input_nodes[blif_netlist->num_top_input_nodes] = new_node;
	blif_netlist->num_top_input_nodes++;
	
	}

#ifdef debug_mode
	printf("\n Exiting the add_top_input_nodes");
#endif

}

/*---------------------------------------------------------------------------------------------
   * function: create_top_output_nodes
     to add the top level outputs to the netlist 
*-------------------------------------------------------------------------------------------*/	
 void create_top_output_nodes()
{

#ifdef debug_mode
  printf("\n reading the create_top_output_nodes\n");
#endif

  long sc_spot;// store the location of the string stored in string_cache
  char *ptr; 
  char buffer[BUFSIZE];
  npin_t *new_pin;
  nnode_t *new_node;
  char *temp_string;

  while (1)
	{
	/* keep reading the outputs till there is no more to read*/
		ptr = my_strtok (NULL, TOKENS, blif, buffer);

		if (ptr == NULL)
			return;

	/* create a new top output node and */
	new_node = allocate_nnode();

	temp_string= make_full_ref_name(ptr, NULL, NULL,NULL, -1);;
	
	/* Create the pin connection for the net */
	new_pin = allocate_npin();
	new_pin->name=temp_string;	
	
	/*add_a_fanout_pin_to_net((nnet_t*)output_nets_sc->data[sc_spot], new_pin);*/

	new_node->related_ast_node = NULL;
	new_node->type = OUTPUT_NODE;

	/* add the name of the output variable */ 
	new_node->name = temp_string;

	/* allocate the input pin needed */
	allocate_more_node_input_pins(new_node, 1);
	add_input_port_information(new_node, 1);

	

	/* hookup the pin, net, and node */
	add_a_input_pin_to_node_spot_idx(new_node, new_pin, 0);
		

	/*adding the node to the blif_netlist output nodes
	  add_node_to_netlist() function can also be used */
	
	blif_netlist->top_output_nodes = (nnode_t**)realloc(blif_netlist->top_output_nodes, sizeof(nnode_t*)*(blif_netlist->num_top_output_nodes+1));
	blif_netlist->top_output_nodes[blif_netlist->num_top_output_nodes] = new_node;
	blif_netlist->num_top_output_nodes++;
  	
 	/*list this output node in top_output_nodes_sc */
 	 sc_spot =sc_add_string(top_output_nodes_sc,new_node->name);
   		if (top_output_nodes_sc->data[sc_spot] != NULL)
   		{
     	 	 error_message(NETLIST_ERROR,linenum,-1, "Net (%s) with the same name already created\n",ptr);	
   		}
	}

#ifdef debug_mode
	 printf("\n exiting the create_top_output_nodes");
#endif

} 
  
 
/*---------------------------------------------------------------------------------------------
   * (function: look_for_clocks)
 *-------------------------------------------------------------------------------------------*/

void look_for_clocks()
{
	int i; 

	for (i = 0; i < blif_netlist->num_ff_nodes; i++)
	{
		if (blif_netlist->ff_nodes[i]->input_pins[1]->net->driver_pin->node->type != CLOCK_NODE)
		{
			blif_netlist->ff_nodes[i]->input_pins[1]->net->driver_pin->node->type = CLOCK_NODE;
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

void create_top_driver_nets(char *instance_name_prefix)
{

#ifdef debug_mode
	printf("\nin top level driver net function");
#endif

	npin_t *new_pin;
	long sc_spot;// store the location of the string stored in string_cache
/* create the constant nets */

	/* ZERO net */ 
	/* description given for the zero net is same for other two */
	blif_netlist->zero_net = allocate_nnet(); // allocate memory to net pointer
	blif_netlist->gnd_node = allocate_nnode(); // allocate memory to node pointer
	blif_netlist->gnd_node->type = GND_NODE;  // mark the type
	allocate_more_node_output_pins(blif_netlist->gnd_node, 1);// alloacate 1 output pin pointer to this node 
	add_output_port_information(blif_netlist->gnd_node, 1);// add port info. this port has 1 pin ,till now number of port for this is one
	new_pin = allocate_npin();
	add_a_output_pin_to_node_spot_idx(blif_netlist->gnd_node, new_pin, 0);// add this pin to output pin pointer array of this node
	add_a_driver_pin_to_net(blif_netlist->zero_net,new_pin);// add this pin to net as driver pin 

	/*ONE net*/

	blif_netlist->one_net = allocate_nnet();
	blif_netlist->vcc_node = allocate_nnode();
	blif_netlist->vcc_node->type = VCC_NODE;
	allocate_more_node_output_pins(blif_netlist->vcc_node, 1);
	add_output_port_information(blif_netlist->vcc_node, 1);
	new_pin = allocate_npin();
	add_a_output_pin_to_node_spot_idx(blif_netlist->vcc_node, new_pin, 0);
	add_a_driver_pin_to_net(blif_netlist->one_net, new_pin);

	/* Pad net */

	blif_netlist->pad_net = allocate_nnet();
	blif_netlist->pad_node = allocate_nnode();
	blif_netlist->pad_node->type = PAD_NODE;
	allocate_more_node_output_pins(blif_netlist->pad_node, 1);
	add_output_port_information(blif_netlist->pad_node, 1);
	new_pin = allocate_npin();
	add_a_output_pin_to_node_spot_idx(blif_netlist->pad_node, new_pin, 0);
	add_a_driver_pin_to_net(blif_netlist->pad_net, new_pin);

	
	/* CREATE the driver for the ZERO */
	blif_zero_string = make_full_ref_name(instance_name_prefix, NULL, NULL, zero_string, -1);
	blif_netlist->gnd_node->name =GND;

	sc_spot = sc_add_string(output_nets_sc,GND);
	if (output_nets_sc->data[sc_spot] != NULL)
	{
		error_message(NETLIST_ERROR,-1,-1, "Error in Odin\n");
	}

	/* store the data which is an idx here */
	output_nets_sc->data[sc_spot] = (void*)blif_netlist->zero_net;
	blif_netlist->zero_net->name =blif_zero_string;

	/* CREATE the driver for the ONE and store twice */
	blif_one_string = make_full_ref_name(instance_name_prefix, NULL, NULL, one_string, -1);
	blif_netlist->vcc_node->name = VCC;

	sc_spot = sc_add_string(output_nets_sc,VCC);
	if (output_nets_sc->data[sc_spot] != NULL)
	{
		error_message(NETLIST_ERROR,-1,-1, "Error in Odin\n");
	}
	/* store the data which is an idx here */
	output_nets_sc->data[sc_spot] = (void*)blif_netlist->one_net;
	blif_netlist->one_net->name =blif_one_string;

	/* CREATE the driver for the PAD */
	blif_pad_string = make_full_ref_name(instance_name_prefix, NULL, NULL, pad_string, -1);
	blif_netlist->pad_node->name = HBPAD;

	sc_spot = sc_add_string(output_nets_sc,HBPAD);
	if (output_nets_sc->data[sc_spot] != NULL)
	{
		error_message(NETLIST_ERROR, -1,-1, "Error in Odin\n");
	}
	/* store the data which is an idx here */
	output_nets_sc->data[sc_spot] = (void*)blif_netlist->pad_net;
	blif_netlist->pad_net->name = blif_pad_string;

#ifdef debug_mode
	printf("\nexiting the top level driver net module");
#endif

}

/*---------------------------------------------------------------------------------------------
 * (function: dum_parse)
 *-------------------------------------------------------------------------------------------*/
static void
dum_parse (
    char *buffer)
{
	/* Continue parsing to the end of this (possibly continued) line. */
	while (my_strtok (NULL, TOKENS, blif, buffer) != NULL);
}



/*---------------------------------------------------------------------------------------------
 * function: hook_up_nets() 
 * find the output nets and add the corrosponding nets   *-------------------------------------------------------------------------------------------*/

void hook_up_nets()
{
  int i,j;
  char * name_pin;
  int input_pin_count=0;
  npin_t * input_pin;
  nnet_t * output_net;
  nnode_t * node;
  long sc_spot;

  /* hook all the input pins in all the internal nodes to the net */
  for(i=0;i<blif_netlist->num_internal_nodes;i++)
  {	
	node=blif_netlist->internal_nodes[i];
	input_pin_count=node->num_input_pins;
	for(j=0;j<input_pin_count;j++)
	{
		input_pin=node->input_pins[j];
		name_pin=input_pin->name;
		sc_spot=sc_lookup_string(output_nets_sc,name_pin);
		if(sc_spot==(-1))
		{
			printf("Error :Could not hook the pin %s not available ",name_pin);
			exit(-1);
		}
	 	output_net=(nnet_t*)output_nets_sc->data[sc_spot];
		/* add the pin to this net as fanout pin */

		add_a_fanout_pin_to_net(output_net,input_pin);
	}
  }	

  /* hook all the ff nodes' input pin to the nets */
    for(i=0;i<blif_netlist->num_ff_nodes;i++)
  {	
	node=blif_netlist->ff_nodes[i];
	input_pin_count=node->num_input_pins;
	for(j=0;j<input_pin_count;j++)
	{
		input_pin=node->input_pins[j];
		name_pin=input_pin->name;
		sc_spot=sc_lookup_string(output_nets_sc,name_pin);
		if(sc_spot==(-1))
		{
			printf("Error :Could not hook the pin %s not available ",name_pin);
			exit(-1);
		}
	 	output_net=(nnet_t*)output_nets_sc->data[sc_spot];
		/* add the pin to this net as fanout pin */

		add_a_fanout_pin_to_net(output_net,input_pin);
		
		
	}
  }

  /* hook the top output nodes input pins */
  for(i=0;i<blif_netlist->num_top_output_nodes;i++)
  {
	node=blif_netlist->top_output_nodes[i];
	input_pin_count=node->num_input_pins;

	for(j=0;j<input_pin_count;j++)
	{
		input_pin=node->input_pins[j];
		sc_spot=sc_lookup_string(top_output_nodes_sc,input_pin->name);
		name_pin=(char*)top_output_nodes_sc->data[sc_spot];
		sc_spot=sc_lookup_string(output_nets_sc,name_pin);
		if(sc_spot==(-1))
		{
			printf("Error:Could not hook the pin %s not available ",name_pin);
			exit(-1);
		}
	 	output_net=(nnet_t*)output_nets_sc->data[sc_spot];
		/* add the pin to this net as fanout pin */
		output_net->name=node->name;
		add_a_fanout_pin_to_net(output_net,input_pin);
	}

  }
}









