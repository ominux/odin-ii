#ifndef SIMULATE_BLIF_H
#define SIMULATE_BLIF_H

#define line_t struct line_t_t

#define INPUT_VECTOR_FILE_NAME "input_vectors"
#define OUTPUT_VECTOR_FILE_NAME "output_vectors"

void simulate_blif (char *test_vector_file_name, netlist_t *netlist);
void simulate_new_vectors (int num_test_vectors, netlist_t *netlist);

line_t
{
	int number_of_pins;
	int max_number_of_pins;
	npin_t **pins;
	char *name;
	int type;
};


#endif

