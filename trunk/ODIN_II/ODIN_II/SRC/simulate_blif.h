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
#include "hashtable.h"
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

// Minimum number of nodes to bother computing in parallel.
#define SIM_PARALLEL_THRESHOLD 150

#define INPUT_VECTOR_FILE_NAME "input_vectors"
#define OUTPUT_VECTOR_FILE_NAME "output_vectors"

#define SINGLE_PORT_MEMORY_NAME "single_port_ram"
#define DUAL_PORT_MEMORY_NAME "dual_port_ram"

typedef struct {
	char **pins;
	int   count;
} additional_pins;

typedef struct {
	int number_of_pins;
	int max_number_of_pins;
	npin_t **pins;
	char *name;
	int type;
} line_t;

typedef struct {
	line_t **lines;
	int    count;
} lines_t;

typedef struct {
	nnode_t ***stages; // Stages.
	int       *counts; // Number of nodes in each stage.
	int 	   count;  // Number of stages.
	// Statistics.
	int    num_nodes;          // The total number of nodes.
	int    num_connections;    // The sum of all the children found under every node.
	int    num_parallel_nodes; // The number of nodes in stages larger than SIM_PARALLEL_THRESHOLD
} stages;

typedef struct {
	signed char  **values;
	int           *counts;
	int            count;
} test_vector;

void simulate_netlist(netlist_t *netlist);
void simulate_cycle(int cycle, stages *s);
stages *simulate_first_cycle(netlist_t *netlist, int cycle, additional_pins *p, lines_t *output_lines);

stages *stage_ordered_nodes(nnode_t **ordered_nodes, int num_ordered_nodes);
void free_stages(stages *s);

int get_num_covered_nodes(stages *s);
nnode_t **get_children_of(nnode_t *node, int *count);
int is_node_ready(nnode_t* node, int cycle);
int is_node_complete(nnode_t* node, int cycle);
int enqueue_node_if_ready(queue_t* queue, nnode_t* node, int cycle);

void compute_and_store_value(nnode_t *node, int cycle);
void compute_memory_node(nnode_t *node, int cycle);
void compute_hard_ip_node(nnode_t *node, int cycle);
void compute_multiply_node(nnode_t *node, int cycle);
void compute_generic_node(nnode_t *node, int cycle);


void update_pin_value(npin_t *pin, signed char value, int cycle);
signed char get_pin_value(npin_t *pin, int cycle);
inline void set_pin(npin_t *pin, signed char value, int cycle);
inline int get_values_offset(int cycle); 

signed char get_line_pin_value(line_t *line, int pin_num, int cycle);
int line_has_unknown_pin(line_t *line, int cycle);

int *multiply_arrays(int *a, int a_length, int *b, int b_length);
void compute_memory(npin_t **inputs, npin_t **outputs, int data_width, npin_t **addr, int addr_width, int we, int clock, int cycle, signed char *data);
void instantiate_memory(nnode_t *node, signed char **memory, int data_width, int addr_width);

int count_test_vectors(FILE *in);
int is_vector(char *buffer);
int get_next_vector(FILE *file, char *buffer);
test_vector *parse_test_vector(char *buffer);
test_vector *generate_random_test_vector(lines_t *l, int cycle);
int compare_test_vectors(test_vector *v1, test_vector *v2);

int verify_test_vector_headers(FILE *in, lines_t *l);
void free_test_vector(test_vector* v);

line_t *create_line(char *name);
int verify_lines(lines_t *l);
void free_lines(lines_t *l);

int find_portname_in_lines(char* port_name, lines_t *l);
lines_t *create_input_test_vector_lines(netlist_t *netlist);
lines_t *create_output_test_vector_lines(netlist_t *netlist);

void store_test_vector_in_lines(test_vector *v, lines_t *l, int cycle);
void assign_node_to_line(nnode_t *node, lines_t *l, int type, int single_pin);

char *generate_vector_header(lines_t *l);
void write_vector_headers(FILE *file, lines_t *l);

void write_vector_to_file(lines_t *l, FILE *file, int cycle);
void write_wave_to_file(lines_t *l, FILE* file, int cycle_offset, int wave_length);

void write_vector_to_modelsim_file(lines_t *l, FILE *modelsim_out, int cycle);
void write_wave_to_modelsim_file(netlist_t *netlist, lines_t *l, FILE* modelsim_out, int cycle_offset, int wave_length);

int verify_output_vectors(char* output_vector_file, int num_test_vectors);

void add_additional_pins_to_lines(nnode_t *node, additional_pins *p, lines_t *l);
additional_pins *parse_additional_pins();
void free_additional_pins(additional_pins *p);

void string_trim(char* string, char *chars);
void string_reverse(char *token, int length);
char *vector_value_to_hex(signed char *value, int length);

int  print_progress_bar(double completion, int position, int length, double time);
void print_netlist_stats(stages *stages, int num_vectors);
void print_simulation_stats(stages *stages, int num_vectors, double total_time, double simulation_time);
void print_time(double time);

double wall_time();

char *get_circuit_filename();

#endif

