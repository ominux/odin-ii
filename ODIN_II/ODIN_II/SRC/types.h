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
#include "string_cache.h"
#include "odin_util.h"
#include "read_xml_arch_file.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define config_t struct config_t_t
#define global_args_t struct global_args_t_t
#define global_args_read_blif_t struct global_args_read_blif_t 
/* new struct for the global arguments of verify_blif function */

#define ast_node_t struct ast_node_t_t
#define info_ast_visit_t struct info_on_ast_visit_t_t

#define sim_state_t struct sim_state_t_t
#define nnode_t struct nnode_t_t
#define npin_t struct npin_t_t
#define nnet_t struct nnet_t_t
#define signal_list_t struct signal_list_t_t
#define char_list_t struct char_list_t_t
#define netlist_t struct netlist_t_t
#define netlist_stats_t struct netlist_stats_t_t

#ifndef TYPES_H
#define TYPES_H

/* for parsing and AST creation errors */
#define PARSE_ERROR -3
/* for netlist creation oerrors */
#define NETLIST_ERROR -4
/* for blif read errors */
#define BLIF_ERROR -5
/* for errors in netlist (clustered after tvpack) errors */
#define NETLIST_FILE_ERROR -6
/* for errors in activation estimateion creation */
#define ACTIVATION_ERROR -7
/* for errors in the netlist simulation */
#define SIMULATION_ERROR -8

/* unique numbers to mark the nodes as we DFS traverse the netlist */
#define PARTIAL_MAP_TRAVERSE_VALUE 10
#define OUTPUT_TRAVERSE_VALUE 12
#define COUNT_NODES 14 /* NOTE that you can't call countnodes one after the other or the mark will be incorrect */
#define COMBO_LOOP 15
#define COMBO_LOOP_ERROR 16
#define GRAPH_CRUNCH 17
#define STATS 18
#define SEQUENTIAL_LEVELIZE 19

/* unique numbers for using void *data entries in some of the datastructures */
#define RESET -1
#define LEVELIZE 12
#define ACTIVATION 13

//#define oassert(x) {if(!(x)){exit(-1);}} // causes an interrupt in GDB
#define oassert(x) {if(!(x)){__asm("int3");}} // causes an interrupt in GDB
// bitvector library (PETER_LIB) defines it, so we don't

/* This is the data structure that holds config file details */
config_t
{
	char **list_of_file_names;
	int num_list_of_file_names;

	char *output_type; // string name of the type of output file

	char *debug_output_path; // path for where to output the debug outputs
	short output_ast_graphs; // switch that outputs ast graphs per node for use with GRaphViz tools
	short output_netlist_graphs; // switch that outputs netlist graphs per node for use with GraphViz tools
	short print_parse_tokens; // switch that controls whether or not each token is printed during parsing
	short output_preproc_source; // switch that outputs the pre-processed source
	int min_hard_multiplier; // threshold from hard to soft logic
	int fixed_hard_multiplier; // flag for fixed or variable hard mult
	int fracture_hard_multiplier; // flag for fractured hard multipliers
	short split_memory_width;
	short split_memory_depth;
	char *arch_file; // Name of the FPGA architecture file
};

typedef enum {
	NO_SIMULATION = 0,
	TEST_EXISTING_VECTORS,
	GENERATE_VECTORS,
}simulation_type;

/* the global arguments of the software */
global_args_t
{
	char *config_file;
	char *verilog_file;
	char *output_file;
	char *arch_file; // Name of the FPGA architecture file
	char *activation_blif_file;
	char *activation_netlist_file;
	char *high_level_block;
	char *sim_vectors_file;
	int num_test_vectors;
	simulation_type sim_type;
};

/* global arguments of the verify_blif function */
global_args_read_blif_t
{
	char *config_file;
	char *blif_file;
	char *arch_file; // Name of the FPGA architecture file
	char *sim_vectors_file;
	int num_test_vectors;
	simulation_type sim_type;
};

#endif // TYPES_H

//-----------------------------------------------------------------------------------------------------

#ifndef AST_TYPES_H
#define AST_TYPES_H

typedef enum 
{
	DEC,
	HEX,
	OCT,
	BIN,
	LONG_LONG,
} bases;

typedef enum
{
	MULTI_PORT_MUX, // port 1 = control, port 2+ = mux options
	FF_NODE,
	BUF_NODE,
	INPUT_NODE,
	OUTPUT_NODE,
	GND_NODE,
	VCC_NODE,
	CLOCK_NODE,
	ADD, // +
	MINUS, // -
	BITWISE_NOT, // ~	
	BITWISE_AND, // & /* 10 */
	BITWISE_OR, // |
	BITWISE_NAND, // ~&
	BITWISE_NOR, // ~| 
	BITWISE_XNOR, // ~^ 
	BITWISE_XOR, // ^
	LOGICAL_NOT, // ! 
	LOGICAL_OR, // ||
	LOGICAL_AND, // &&
	LOGICAL_NAND, // No Symbol
	LOGICAL_NOR, // No Symbol
	LOGICAL_XNOR, // No symbol
	LOGICAL_XOR, // No Symbol
	MULTIPLY, // * /*20*/
	DIVIDE, // /
	MODULO, // %
	LT, // <
	GT, // >
	LOGICAL_EQUAL, // == 
	NOT_EQUAL, // !=
	LTE, // <=
	GTE, // >=
	SR, // >>
	SL, // << /*30*/
	CASE_EQUAL, // ===
	CASE_NOT_EQUAL, // !==
	ADDER_FUNC,
	CARRY_FUNC,
	MUX_2,
	BLIF_FUNCTION,
	NETLIST_FUNCTION,
	MEMORY,
	PAD_NODE,
	HARD_IP, /* 40 */
	GENERIC /*41 added for the unknown node type */
} operation_list;
	
typedef enum 
{
	/* top level things */
	FILE_ITEMS,
	MODULE, 
	/* VARIABLES */
	INPUT, 
	OUTPUT, 
	INOUT,
	WIRE,
	REG,
	PARAMETER,
	PORT,
	/* OTHER MODULE ITEMS */
	MODULE_ITEMS, 
	VAR_DECLARE,//10
	VAR_DECLARE_LIST,
	ASSIGN,
	/* primitives */
	GATE,
	GATE_INSTANCE,
	/* Module instances */
	MODULE_CONNECT_LIST,
	MODULE_CONNECT,
	MODULE_NAMED_INSTANCE,
	MODULE_INSTANCE,
	/* statements */
	BLOCK, 
	NON_BLOCKING_STATEMENT,//20
	BLOCKING_STATEMENT,
	CASE,
	CASE_LIST,
	CASE_ITEM,
	CASE_DEFAULT,
	ALWAYS,
	IF,
	IF_Q,
	/* Delay Control */
	DELAY_CONTROL,
	POSEDGE,// 30
	NEGEDGE, 
	/* expressions */
	BINARY_OPERATION, 
	UNARY_OPERATION,
	/* basic primitives */
	ARRAY_REF, 
	RANGE_REF,
	CONCATENATE,
	/* basic identifiers */
	IDENTIFIERS,
	NUMBERS, 
	/* Hard Blocks */
	HARD_BLOCK, 
	HARD_BLOCK_NAMED_INSTANCE, // 40
	HARD_BLOCK_CONNECT_LIST,
	HARD_BLOCK_CONNECT
} ids;

ast_node_t
{
	int unique_count;
	int far_tag;
	int high_number;
	short type;
	union
	{
		char *identifier;
		struct
		{
			short base;
			int size;
			int binary_size;
			char *binary_string;
			char *number;
			long long value;
		} number;
		struct
		{
			short op;
		} operation;
		struct
		{
			short is_parameter;
			short is_port;
			short is_input;
			short is_output;
			short is_inout;
			short is_wire;
			short is_reg;
		} variable;
		struct
		{
			short is_instantiated;
			ast_node_t **module_instantiations_instance;
			int size_module_instantiations;
			int index;
		} module;
		struct
		{
			int num_bit_strings;
			char **bit_strings;
		} concat;
	} types;

	ast_node_t **children;
	int num_children;

	int line_number;
	int file_number;

	short shared_node;
	void *hb_port;
	void *net_node;

	void *additional_data; // this is a point where you can add additional data for your optimization or technique
};

info_ast_visit_t
{
	ast_node_t *me;
	ast_node_t *from;
	int from_child_position;

	short is_constant;
	long long value;

	short is_constant_folded;
};
#endif // AST_TYPES_H

//-----------------------------------------------------------------------------------------------------
#ifndef NETLIST_UTILS_H
#define NETLIST_UTILS_H

sim_state_t
{
	int cycle;
	int value;
	int prev_value;
};

/* DEFINTIONS for all the different types of nodes there are.  This is also used cross-referenced in utils.c so that I can get a string version 
 * of these names, so if you add new tpyes in here, be sure to add those same types in utils.c */
nnode_t
{
	long unique_id;
	char *name; // unique name of a node
	short type; // the type of node
	ast_node_t *related_ast_node; // the abstract syntax node that made this node

	short traverse_visited; // a way to mark if we've visited yet

	npin_t **input_pins; // the input pins
	int num_input_pins;
	int *input_port_sizes; // info about the input ports
	int num_input_port_sizes;

	npin_t **output_pins; // the output pins
	int num_output_pins;
	int *output_port_sizes; // info if there is ports
	int num_output_port_sizes;

	short unique_node_data_id;
	void *node_data; // this is a point where you can add additional data for your optimization or technique

	int forward_level; // this is your logic level relative to PIs and FFs .. i.e farthest PI
	int backward_level; // this is your reverse logic level relative to POs and FFs .. i.e. farthest PO
	int sequential_level; // the associated sequential network that the node is in
	short sequential_terminator; // if this combinational node is a terminator for the sequential level (connects to flip-flop or Output pin

	netlist_t* internal_netlist; // this is a point of having a subgraph in a node

	int *memory_data;
	//(int cycle, int num_input_pins, npin_t *inputs, int num_output_pins, npin_t *outputs);
	void (*simulate_block_cycle)(int, int, int*, int, int*);

	short *associated_function;

	char** bit_map; /*storing the bit map */
};

npin_t
{
	long unique_id;
	short type; // INPUT or OUTPUT
	char *name;

	nnet_t *net; // related net
	int pin_net_idx;

	nnode_t *node; // related node
	int pin_node_idx; // pin on the node where we're located
	char *mapping; // name of mapped port from hard block

	sim_state_t *sim_state;
};

nnet_t
{
	long unique_id;
	char *name; // name for the net
	short combined;

	npin_t *driver_pin; // the pin that drives the net

	npin_t **fanout_pins; // the pins pointed to by the net
	int num_fanout_pins; // the list size of pins

	short unique_net_data_id;
	void *net_data;
};

signal_list_t 
{
	npin_t **signal_list;	
	int signal_list_size;

	short is_memory;
};

char_list_t 
{
	char **strings;	
	int num_strings;
};

netlist_t
{
	nnode_t *gnd_node;
	nnode_t *vcc_node;
	nnode_t *pad_node;
	nnet_t *zero_net;
	nnet_t *one_net;
	nnet_t *pad_net;
	nnode_t** top_input_nodes;
	int num_top_input_nodes;
	nnode_t** top_output_nodes;
	int num_top_output_nodes;
	nnode_t** ff_nodes;
	int num_ff_nodes;
	nnode_t** internal_nodes;
	int num_internal_nodes;
	nnode_t** clocks;
	int num_clocks;
	

	/* netlist levelized structures */
	nnode_t ***forward_levels;
	int num_forward_levels;
	int* num_at_forward_level;
	nnode_t ***backward_levels; // NOTE backward levels isn't neccessarily perfect.  Because of multiple output pins, the node can be put closer to POs than should be.  To fix, run a rebuild of the list afterwards since the marked "node->backward_level" is correct */
	int num_backward_levels;
	int* num_at_backward_level;

	nnode_t ***sequential_level_nodes;
	int num_sequential_levels;
	int* num_at_sequential_level;
	/* these structures store the last combinational node in a level before a flip-flop or output pin */
	nnode_t ***sequential_level_combinational_termination_node;
	int num_sequential_level_combinational_termination_nodes;
	int* num_at_sequential_level_combinational_termination_node;

	STRING_CACHE *nets_sc;
	STRING_CACHE *out_pins_sc;
	STRING_CACHE *nodes_sc;

	netlist_stats_t *stats;

	t_type_ptr type; 
};

netlist_stats_t
{
	int num_inputs;
	int num_outputs;
	int num_ff_nodes;
	int num_logic_nodes;
	int num_nodes;

	float average_fanin; /* = to the fanin of all nodes: basic outs, combo and ffs */	
	int *fanin_distribution;
	int num_fanin_distribution;

	long num_output_pins;
	float average_output_pins_per_node;
	float average_fanout; /* = to the fanout of all nodes: basic IOs, combo and ffs...no vcc, clocks, gnd */	
	int *fanout_distribution;
	int num_fanout_distribution;

	/* expect these should be close but doubt they'll be equal */
	long num_edges_fo; /* without control signals like clocks and gnd and vcc...= to number of fanouts from output pins */
	long num_edges_fi; /* without control signals like clocks and gnd and vcc...= to number of fanins from output pins */

	int **combinational_shape;
	int *num_combinational_shape_for_sequential_level;
};

#endif // NET_TYPES_H 
