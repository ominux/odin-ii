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
#ifndef SIMULATE_BLIF_H
#define SIMULATE_BLIF_H
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <dlfcn.h>
#include <sys/time.h>

#include "queue.h"
#include "sim_block.h"
#include "types.h"
#include "globals.h"
#include "errors.h"
#include "netlist_utils.h"
#include "odin_util.h"
#include "outputs.h"
#include "util.h"
#include "multipliers.h"
#include "hard_blocks.h"
#include "types.h"

#define BUFFER_MAX_SIZE 1024

#define INPUT_VECTOR_FILE_NAME "input_vectors"
#define OUTPUT_VECTOR_FILE_NAME "output_vectors"

#define CLOCK_PORT_NAME_1 "clock"
#define CLOCK_PORT_NAME_2 "clk"
#define RESET_PORT_NAME "reset_n"

#define SINGLE_PORT_MEMORY_NAME "single_port_ram"
#define DUAL_PORT_MEMORY_NAME "dual_port_ram"

#define line_t struct line_t_t
line_t {
	int number_of_pins;
	int max_number_of_pins;
	npin_t **pins;
	char *name;
	int type;
};

void simulate_blif (char *test_vector_file_name, netlist_t *netlist);
void simulate_new_vectors (int num_test_vectors, netlist_t *netlist);

void simulate_netlist(int num_test_vectors, char *test_vector_file_name, netlist_t *netlist);
void simulate_cycle(netlist_t *netlist, int cycle, int num_test_vectors, nnode_t ***ordered_nodes, int *num_ordered_nodes);

nnode_t **get_children_of(nnode_t *node, int *count);

inline int is_node_ready(nnode_t* node, int cycle);
inline int is_node_complete(nnode_t* node, int cycle);
int enqueue_node_if_ready(queue_t* queue, nnode_t* node, int cycle);

void compute_and_store_value(nnode_t *node, int cycle);
void update_pin_value(npin_t *pin, int value, int cycle);
signed char get_pin_value(npin_t *pin, int cycle);
inline int get_values_offset(int cycle); 

int *multiply_arrays(int *a, int a_length, int *b, int b_length);
void compute_memory(npin_t **inputs, npin_t **outputs, int data_width, npin_t **addr, int addr_width, int we, int clock, int cycle, int *data);
void instantiate_memory(nnode_t *node, int **memory, int data_width, int addr_width);

line_t *create_line(char *name);
int verify_lines(line_t **lines, int lines_size);
void free_lines(line_t **lines, int lines_size);

line_t **create_input_test_vector_lines(int *lines_size, netlist_t *netlist);
line_t **create_output_test_vector_lines(int *lines_size, netlist_t *netlist);

void assign_input_vector_to_lines(line_t **lines, char *buffer, int cycle);
void assign_random_vector_to_input_lines(line_t **lines, int lines_size, int cycle);
void store_value_in_line(char *token, line_t *line, int cycle);
void assign_node_to_line(nnode_t *node, line_t **lines, int lines_size, int type);
line_t** read_test_vector_headers(FILE *out, int *lines_size, int max_lines_size);

void write_vector_headers(FILE *file, line_t **lines, int lines_size);
void write_vectors_to_file(line_t **lines, int lines_size, FILE *file, int type, int cycle);
void write_all_vectors_to_file(line_t **lines, int lines_size, FILE* file, int type, int wave, int wave_length);
int verify_output_vectors(netlist_t *netlist, line_t **lines, int lines_size, int cycle);

void free_blocks();

#endif

