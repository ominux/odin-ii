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
#include <unistd.h>
#include <string.h>
#include "globals.h"
#include "types.h"
#include "util.h"
#include "netlist_utils.h"
#include "arch_types.h"
#include "parse_making_ast.h"
#include "netlist_create_from_ast.h"
#include "outputs.h"
#include "netlist_optimizations.h"
#include "read_xml_config_file.h"
#include "read_xml_arch_file.h"
#include "partial_map.h"
#include "multipliers.h"
#include "netlist_check.h"
#include "read_blif.h"
#include "read_netlist.h"
#include "activity_estimation.h"
#include "high_level_data.h"
#include "hard_blocks.h"
#include "simulate_blif.h"

global_args_t global_args;
int current_parse_file;
t_arch Arch;
t_type_descriptor* type_descriptors;
int block_tag;
void get_options(int argc, char **argv);
void do_high_level_synthesis();
void do_simulation_of_netlist();
void do_activation_estimation( int num_types, t_type_descriptor * type_descriptors);

int main(int argc, char **argv)
{
	int num_types;

	printf("--------------------------------------------------------------------\n");
	printf("Welcome to ODIN II version 0.1 - the better High level synthesis tools++ targetting FPGAs (mainly VPR)\n");
	printf("Email: jamieson.peter@gmail.com and ken@unb.ca for support issues\n\n");

	/* get the command line options */
	get_options(argc, argv);

	/* read the confirguration file .. get options presets the config values just in case theyr'e not read in with config file */
	if (global_args.config_file != NULL)
	{
		printf("Reading Configuration file\n");
		read_config_file(global_args.config_file);
	}

	/* read the FPGA architecture file */
	if (global_args.arch_file != NULL)
	{
		printf("Reading FPGA Architecture file\n");
#ifdef VPR5
		t_clocks ClockDetails = { 0 };
		t_power PowerDetails = { 0 };
		XmlReadArch(global_args.arch_file, FALSE, &Arch, &type_descriptors, &num_types, &ClockDetails, &PowerDetails);
#endif
#ifdef VPR6
		XmlReadArch(global_args.arch_file, FALSE, &Arch, &type_descriptors, &num_types);
#endif
	}

	/* High level synthesis tool */
	do_high_level_synthesis();

	/* Simulate blif netlist */
	do_simulation_of_netlist();

	/* activation estimation tool */
#ifdef VPR5
	do_activation_estimation(num_types, type_descriptors);
#endif

#ifdef VPR6
	report_mult_distribution();
	deregister_hard_blocks();
#endif

	return 0;
} 

static const char *optString = "hHc:V:h:o:O:a:B:N:f:s:S:g:G:t:T:"; // list must end in ":"
/*---------------------------------------------------------------------------------------------
 * (function: get_options)
 *-------------------------------------------------------------------------*/
void get_options(int argc, char **argv)
{
	int opt = 0;

	/* set up the global arguments to there defualts */
	global_args.config_file = NULL;
	global_args.verilog_file = NULL;
	global_args.output_file = "./default_out.blif";
	global_args.arch_file = NULL;
	global_args.activation_blif_file = NULL;
	global_args.activation_netlist_file = NULL;
	global_args.high_level_block = NULL;
	global_args.sim_vectors_file = NULL;
	global_args.sim_type = NO_SIMULATION;
	global_args.num_test_vectors = 0;

	/* set up the global configuration ahead of time */
	configuration.list_of_file_names = NULL;
	configuration.num_list_of_file_names = 0;
	configuration.output_type = "blif";
	configuration.output_ast_graphs = 0;
	configuration.output_netlist_graphs = 0;
	configuration.debug_output_path = ".";
	configuration.arch_file = NULL;

	/* read in the option line */
	opt = getopt(argc, argv, optString);
	while(opt != -1) 
	{
       		switch(opt) 
		{
		/* arch file */
		case 'a': 
			global_args.arch_file = optarg;
			configuration.arch_file = optarg;
			break;
		/* config file */
		case 'c': 
			global_args.config_file = optarg;
			break;
		case 'V': 
			global_args.verilog_file = optarg;
			break;
		case 'o':
		case 'O':
			global_args.output_file = optarg;
			break;
		case 'B':
			global_args.activation_blif_file = optarg;
			break;
		case 'N':
			global_args.activation_netlist_file = optarg;
			break;
		case 'f':
#ifdef VPR5
			global_args.high_level_block = optarg;
#endif
#ifdef VPR6
			warning_message(0, -1, 0, "VPR 6.0 doesn't have this feature yet.  You'll need to deal with the output_blif.c differences wrapped by \"if (global_args.high_level_block != NULL)\"\n");
#endif
			break;
		case 'h':
		case 'H':
			printf("Usage: odin_II.exe\n\tOne of:\n\t\t-c <config_file_name.xml>\n\t\t-V <verilog_file_name.v>\n\tAlso options of:\n\t\t-o <output_path and file name>\n\t\t-a <architecture_file_in_VPR6.0_form>\n\t\t-A <blif_file_for_activation_estimation>\n\t\t \n");
			exit(-1);
			break;
		case 'g':
		case 'G':
			global_args.num_test_vectors = atoi(optarg);
			global_args.sim_type = GENERATE_VECTORS;
			break;
		case 't':
		case 'T':
			global_args.sim_vectors_file = optarg;
			global_args.sim_type = TEST_EXISTING_VECTORS;
			break;
		case 's':
		case 'S':
			global_args.sim_vectors_file = optarg;
			break;
		default : 
			printf("Usage: \"odin_II.exe -h\" for usage\n");
			exit(-1);
			break;
		}

		opt = getopt(argc, argv, optString);
	}

	if ((global_args.config_file == NULL) && (global_args.verilog_file == NULL) && 
			((global_args.activation_blif_file == NULL) || (global_args.activation_netlist_file == NULL)))
	{
		printf("Error: must include either a activation blif and netlist file, a config file, or a verilog file\n");
		exit(-1);
	}
	else if ((global_args.config_file != NULL) && ((global_args.verilog_file != NULL) || (global_args.activation_blif_file != NULL)))
	{
		printf("Warning: Using command line options for verilog input file!!!\n");
	}
}

/*---------------------------------------------------------------------------
 * (function: do_high_level_synthesis)
 *-------------------------------------------------------------------------*/
void do_high_level_synthesis()
{
	printf("--------------------------------------------------------------------\n");
	printf("High-level synthesis Begin\n");

	/* Perform any initialization routines here */
#ifdef VPR6
	find_hard_multipliers();
	register_hard_blocks();
#endif

	/* parse to abstract syntax tree */
	printf("Parser starting - we'll create an abstract syntax tree.  Note this tree can be viewed using GraphViz (see dosumentation)\n");
	parse_to_ast();
	/* Note that the entry point for ast optimzations is done per module with the function void next_parsed_verilog_file(ast_node_t *file_items_list) */

	/* after the ast is made potentiatlly do tagging for downstream links to verilog */
	if (global_args.high_level_block != NULL)
	{
		add_tag_data();
	}

	/* Now that we have a parse tree (abstract syntax tree [ast]) of the Verilog we want to make into a netlist. */
	printf("Converting AST into a Netlist - Note this netlist can be viewed using GraphViz (see dosumentation)\n");
	create_netlist();

	check_netlist(verilog_netlist); // can't levelize yet since the large muxes can look like combinational loops when they're not

	/* point for all netlist optimizations. */
	printf("Performing Optimizations of the Netlist\n");
	netlist_optimizations_top(verilog_netlist);

	/* point where we convert netlist to FPGA or other hardware target compatible format */
	printf("Performing Partial Map to target device\n");
	partial_map_top(verilog_netlist);

	/* check for problems in the partial mapped netlist */
	printf("Check for liveness and combinational loops\n");
#ifdef VPR5
	levelize_and_check_for_combinational_loop_and_liveness(TRUE, verilog_netlist);
#endif

	/* point for outputs.  This includes soft and hard mapping all structures to the target format.  Some of these could be considred optimizations */
	printf("Outputting the netlist to the specified output format\n");
	output_top(verilog_netlist);

	printf("Successful High-level synthesis by Odin\n");
	printf("--------------------------------------------------------------------\n");
}

/*---------------------------------------------------------------------------------------------
 * (function: do_simulation_of_netlist)
 *-------------------------------------------------------------------------------------------*/
void do_simulation_of_netlist()
{
	if (global_args.sim_type == NO_SIMULATION)
		return;
	printf("Netlist Simulation Begin\n");
	if (global_args.sim_type == GENERATE_VECTORS)
	{
		printf("Testing new (random) vectors.\n");
		simulate_new_vectors(global_args.num_test_vectors, verilog_netlist);
	}
	else //global_args.sim_type == TEST_EXISTING_VECTORS
	{
		printf("Testing existing vectors.\n");
		simulate_blif(global_args.sim_vectors_file, verilog_netlist);
	}
	printf("\n--------------------------------------------------------------------\n");
}

/*---------------------------------------------------------------------------------------------
 * (function: do_activation_estimation)
 *-------------------------------------------------------------------------------------------*/
#ifdef VPR5
void do_activation_estimation(
	int num_types,
	t_type_descriptor * type_descriptors)
{
	netlist_t *blif_netlist;
	netlist_t *net_netlist;
	int lut_size;

	if ((global_args.activation_blif_file == NULL) || (global_args.activation_netlist_file == NULL) || (global_args.arch_file == NULL))
	{
		return;
	}
	lut_size = type_descriptors[2].max_subblock_inputs;

	printf("--------------------------------------------------------------------\n");
	printf("Activation Estimation Begin\n");

	/* read in the blif file */
	printf("Reading blif format in for probability densitity estimation\n");
	blif_netlist = read_blif (global_args.activation_blif_file, lut_size);

	/* read in the blif file */
	/* IO type is known from read_arch library #define EMPTY_TYPE_INDEX 0 #define IO_TYPE_INDEX 1 */
	printf("Reading netlist format in for probability densitity estimation\n");
	net_netlist = read_netlist (global_args.activation_netlist_file, num_types, type_descriptors, &type_descriptors[1]);

	/* do activation estimation */
	activity_estimation(NULL, global_args.output_file, lut_size, blif_netlist, net_netlist);

	free_netlist(blif_netlist);
	free_netlist(net_netlist);

	printf("Successful Activation Estimation \n");
	printf("--------------------------------------------------------------------\n");
}
#endif
