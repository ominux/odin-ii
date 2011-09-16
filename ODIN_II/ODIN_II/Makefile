CC = gcc

BUILD = VPR6

PARSER1 = flex 
PARSER2 = bison

WARN_FLAGS = -Wall 

SRC_DIR = SRC

EXEO = odin_II.exe
EXEV= verify_blif.exe

DEBUG_FLAGS = -g

OPT_FLAGS = 

ifeq "$(BUILD)" "VPR5"
	INC_FLAGS =  -I../libvpr_5/include
	LIB = -L../libvpr_5 -lvpr_5 -lm -ldl
	C_DIRECTIVE = -DVPR5 -DUSING_BISON
else
	INC_FLAGS =  -I../libvpr_6/include 
	LIB = -L../libvpr_6 -lvpr_6 -lm -ldl
	C_DIRECTIVE = -DVPR6 -DUSING_BISON
endif

FLAGS = $(DEBUG_FLAGS) $(WARN_FLAGS) $(OPT_FLAGS) $(INC_FLAGS)
C_FLAGS = $(C_DIRECTIVE)

OBJS = \
OBJ/netlist_visualizer.o \
OBJ/multipliers.o \
OBJ/partial_map.o \
OBJ/errors.o \
OBJ/node_creation_library.o \
OBJ/netlist_utils.o \
OBJ/netlist_stats.o \
OBJ/odin_util.o \
OBJ/string_cache.o \
OBJ/hard_blocks.o \
OBJ/memories.o \
OBJ/queue.o \
OBJ/simulate_blif.o\
OBJ/print_netlist.o 

OBJO = \
OBJ/read_xml_config_file.o \
OBJ/outputs.o \
OBJ/parse_making_ast.o \
OBJ/ast_util.o \
OBJ/high_level_data.o \
OBJ/ast_optimizations.o \
OBJ/netlist_create_from_ast.o \
OBJ/netlist_optimizations.o \
OBJ/output_blif.o \
OBJ/netlist_check.o \
OBJ/activity_estimation.o \
OBJ/read_netlist.o \
OBJ/output_graphcrunch_format.o \
OBJ/verilog_preprocessor.o \
OBJ/verilog_bison.o \
OBJ/verilog_flex.o \
OBJ/odin_ii.o

OBJV = OBJ/read_blif.o \
OBJ/verify_blif.o

SRC = \
SRC/netlist_visualizer.c \
SRC/multipliers.c \
SRC/partial_map.c \
SRC/errors.c \
SRC/node_creation_library.c \
SRC/netlist_utils.c \
SRC/netlist_stats.c \
SRC/odin_util.c \
SRC/string_cache.c \
SRC/hard_blocks.c \
SRC/memories.c \
SRC/queue.c \
SRC/simulate_blif.c \
SRC/print_netlist.c

SRCO = \
SRC/read_xml_config_file.c \
SRC/outputs.c \
SRC/parse_making_ast.c \
SRC/ast_util.c \
SRC/high_level_data.c \
SRC/ast_optimizations.c \
SRC/netlist_create_from_ast.c \
SRC/netlist_optimizations.c \
SRC/output_blif.c \
SRC/netlist_check.c \
SRC/activity_estimation.c \
SRC/read_netlist.c \
SRC/output_graphcrunch_format.c \
SRC/verilog_preprocessor.c \
SRC/odin_ii.c

SRCV = SRC/read_blif.c \
SRC/verify_blif.c 

H = \
SRC/netlist_stats.h \
SRC/output_graphcrunch_format.h \
SRC/read_netlist.h \
SRC/activity_estimation.h \
SRC/read_blif.h \
SRC/netlist_check.h \
SRC/multipliers.h \
SRC/output_blif.h \
SRC/partial_map.h \
SRC/outputs.h \
SRC/netlist_optimizations.h \
SRC/errors.h \
SRC/node_creation_library.h \
SRC/netlist_visualizer.h \
SRC/netlist_create_from_ast.h \
SRC/netlist_utils.h \
SRC/odin_util.h \
SRC/ast_optimizations.h \
SRC/high_level_data.h \
SRC/ast_util.h \
SRC/read_xml_config_file.h \
SRC/parse_making_ast.h \
SRC/string_cache.h \
SRC/types.h \
SRC/hard_blocks.h \
SRC/memories.h \
SRC/verilog_preprocessor.h \
SRC/queue.h \
SRC/simulate_blif.h \
SRC/globals.h \
SRC/print_netlist.h

all: $(EXEO) $(EXEV)

$(EXEO): CTAGS OBJ/verilog_bison.o OBJ/verilog_flex.o $(OBJS) $(OBJO)
	$(CC) $(C_FLAGS) $(FLAGS) $(OBJS) $(OBJO) -o $(EXEO) $(LIB)

$(EXEV): CTAGS $(OBJS) $(OBJV) 
	$(CC) $(C_FLAGS) $(FLAGS) $(OBJS) $(OBJV) -o $(EXEV) $(LIB) 

CTAGS :
	cd SRC && ctags *.[ch] && cd -

OBJ/%.o:$(SRC_DIR)/%.c $(H)
	$(CC) $(C_FLAGS) $(FLAGS) -c $< -o $@ 


SRC/verilog_flex.c: SRC/verilog_flex.l
	$(PARSER1) -oSRC/verilog_flex.c SRC/verilog_flex.l

SRC/verilog_bison.c: SRC/verilog_bison.y
	$(PARSER2) --verbose -d -t -o SRC/verilog_bison.c SRC/verilog_bison.y

clean :
	rm -f SRC/verilog_bison.c
	rm -f SRC/verilog_bison.h
	rm -f SRC/verilog_bison.output
	rm -f SRC/verilog_flex.c
	rm -f *.exe
	rm -f OBJ/*.o
	rm -f SRC/*~
	

regression_clean :
	rm -f REGRESSION_TESTS/BASIC_TESTS/*.*

