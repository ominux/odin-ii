#ifndef ODIN_UTIL_H
#define ODIN_UTIL_H
#include "types.h"

#define MAX_BUF 256

char *make_signal_name(char *signal_name, int bit);
char *make_full_ref_name(char *previous, char *module_name, char *module_instance_name, char *signal_name, int bit);

char *twos_complement(char *str);
char *convert_long_long_to_bit_string(long long orig_long, int num_bits);
long long convert_dec_string_of_size_to_long_long(char *orig_string, int size);
char *convert_hex_string_of_size_to_bit_string(char *orig_string, int size);
char *convert_oct_string_of_size_to_bit_string(char *orig_string, int size);
char *convert_binary_string_of_size_to_bit_string(char *orig_string, int size);

long long int my_power(long long int x, long long int y);
long long int pow2(int to_the_power);

char *make_string_based_on_id(nnode_t *node);
char *make_simple_name(char *input, char *flatten_string, char flatten_char);

void *my_malloc_struct(int bytes_to_alloc);

void string_reverse(char *token, int length);

int is_binary_string(char *string);
int is_octal_string(char *string);
int is_decimal_string(char *string);
int is_hex_string(char *string);

#endif

