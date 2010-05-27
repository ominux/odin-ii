/*
Data types describing the logic models that the architecture can implement.

Date: February 19, 2009
Authors: Jason Luu and Kenneth Kent
*/

#ifndef LOGIC_TYPES_H
#define LOGIC_TYPES_H

/* 
Logic model data types
A logic model is described by its I/O ports and function name
*/

enum PORTS {IN_PORT, OUT_PORT, INOUT_PORT};
typedef struct s_model_ports
{
	enum PORTS dir;
	char *name;
	int size;
	struct s_model_ports *next;
} t_model_ports;

typedef struct s_model
{
	char *name;
	t_model_ports *inputs;
	t_model_ports *outputs;
	void *instances;
	struct s_model *next;
} t_model;

#endif

