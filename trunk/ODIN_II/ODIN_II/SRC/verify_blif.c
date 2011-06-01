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

// The contains the main for the verify.exe
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "arch_types.h"
#include "globals.h"
#include "types.h"
#include "util.h"
#include "netlist_utils.h"
#include "arch_types.h"
#include "read_xml_arch_file.h"
#include "simulate_blif.h"
#include "read_blif_new.h"
#include "multipliers.h"
#include "hard_blocks.h"
#include "memories.h"
#include "netlist_visualizer.h"
#include "print_netlist.h"

//#define print_netlist

global_args_read_blif_t global_args_read_blif;
t_arch Arch;
t_type_descriptor* type_descriptors;
config_t configuration; 


/* CONSTANT NET ELEMENTS */
char *one_string = "ONE_VCC_CNS";
char *zero_string = "ZERO_GND_ZERO";
char *pad_string = "ZERO_PAD_ZERO";


void do_simulation_of_netlist();
void get_options(int argc, char **argv);



int main(int argc,char **argv)
{
    int num_types; // not sure what value does this store
    printf("----------------------------------------------------------------------\n");
    printf("Reading the verify_blif for verification\n");

    // reading the command line flags
    get_options(argc,argv);

    /* read the configuration file (unavailable) */
    if (global_args_read_blif.config_file != NULL)
	{
		printf("Reading Configuration file\n");
		printf("Config read function not included yet\n");
	}
    
    /* read the architecture file */
    if (global_args_read_blif.arch_file != NULL)
    {
       printf("Reading FPGA Architecture file\n");
#ifdef VPR5
       t_clocks ClockDetails = { 0 };
       t_power PowerDetails = { 0 };
       XmlReadArch(global_args_read_blif.arch_file, FALSE, &Arch, &type_descriptors, &num_types, &ClockDetails, &PowerDetails);
#endif
#ifdef VPR6
       XmlReadArch(global_args_read_blif.arch_file, FALSE, &Arch, &type_descriptors, &num_types);
#endif
    }

printf("--------------------------------------------------------------------------\n");
printf("Reading the read_blif and Extracting the netlist\n");
    //lut_size = type_descriptors[2].max_subblock_inputs; /* VPR6 does not support
   read_blif_new(global_args_read_blif.blif_file);

printf("Printing the netlist as a graph\n");
    char path[]=".";
    char name[]="net_blif1";
    graphVizOutputNetlist(path,name,1, blif_netlist);
    /* prints the netlist as net_blif.dot */	

printf("Extraction of netlist Completed\n");    
    // Call the function to due simulation of the extracted netlist
    do_simulation_of_netlist();

#ifdef print_netlist    
	print_netlist_for_checking(blif_netlist,"blif_netlist");
#endif

return 0;
}
 





/*
--------------------------------------------------------------------------------------------
*(function: do_simulation_of_netlist)
* *-------------------------------------------------------------------------------------------*/
void do_simulation_of_netlist()
{
	if (global_args_read_blif.sim_type == NO_SIMULATION)
		return;
	printf("---------------------------------------------------------------------------------------------");
	printf("Netlist Simulation Begin\n");
	if (global_args_read_blif.sim_type == GENERATE_VECTORS)
	{
		printf("Testing new (random) vectors.\n");
		simulate_new_vectors(global_args_read_blif.num_test_vectors,blif_netlist);
	}
	else //global_args_read_blif.sim_type == TEST_EXISTING_VECTORS
	{
		printf("Testing existing vectors.\n");
		simulate_blif(global_args_read_blif.sim_vectors_file,blif_netlist);
	}
	printf("\n--------------------------------------------------------------------\n");
}


static const char *optString = "hHa:b:s:S:h:H:g:G:t:T:c:"; // list must end in ":"
/*---------------------------------------------------------------------------------------------
 * (function: get_options)
 *-------------------------------------------------------------------------*/
void get_options(int argc, char **argv)
{
	int opt = 0;

	/* set up the global arguments to there defualts */
	global_args_read_blif.config_file = NULL;
	global_args_read_blif.blif_file = NULL;
	global_args_read_blif.arch_file = NULL;
	global_args_read_blif.sim_vectors_file = NULL;
	global_args_read_blif.sim_type = NO_SIMULATION;
	global_args_read_blif.num_test_vectors = 0;


   /* Does not read from a config file as of now. Can be included later  */


	/* read in the option line */
	opt = getopt(argc, argv, optString);

	while(opt != -1) 
	{
       		switch(opt) 
		{
		/* arch file */
		case 'a': 
			global_args_read_blif.arch_file = optarg;
			break;
		/* config file not included yet */
		case 'c': 
			global_args_read_blif.config_file = optarg;
			break; 
		
		/* blif file */
		case 'b':
			global_args_read_blif.blif_file = optarg;
			break;
		
		case 'h':
		case 'H':
			printf("Usage: verify_blif.exe\n\t Options :\n\t\t-c <config_file_name.xml>\n\t\t-b <input_blif_fil_name.blif>\n\t\t-a <architecture_file_in_VPR6.0_form>\n\nSimulation options:\n\t\t -g <number_of_random_test_vectors\n\t\t -s <Simulate a particular input_vector_file>\n\t\t -t test_vector_file\n");
			exit(-1);
			break;

		/* simulation options */
		case 'g':
		case 'G':
			global_args_read_blif.num_test_vectors = atoi(optarg);
			global_args_read_blif.sim_type = GENERATE_VECTORS;
			break;
		case 't':
		case 'T':
			global_args_read_blif.sim_vectors_file = optarg;
			global_args_read_blif.sim_type = TEST_EXISTING_VECTORS;
			break;
		case 's':
		case 'S':
			global_args_read_blif.sim_vectors_file = optarg;
			break;
		default : 
			printf("Usage: \"verify_blif.exe -h\" for usage\n");
			exit(-1);
			break;
		}

		opt = getopt(argc, argv, optString);

	}

	if ((global_args_read_blif.config_file == NULL) && ((global_args_read_blif.blif_file == NULL)|| (global_args_read_blif.arch_file==NULL)))
			
	{
		printf("Error: must include either a config file, or a blif file and Fpga Architecture file \n");
		exit(-1);
	}
	
}


