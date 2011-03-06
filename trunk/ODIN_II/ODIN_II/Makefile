CC = gcc

BUILD = VPR6
#BUILD = VPR5

PARSER1 = flex 
PARSER2 = bison

WARN_FLAGS = -Wall 

SRC_DIR = SRC

EXE = odin_II.exe

DEBUG_FLAGS = -g
OPT_FLAGS = -O2

ifeq "$(BUILD)" "VPR5"
INC_FLAGS =  -I../libvpr_5/include
LIB = -L../libvpr_5 -lvpr_5 -lm
C_DIRECTIVE = -DVPR5 -DUSING_BISON
else
INC_FLAGS =  -I../libvpr_6/include 
LIB = -L../libvpr_6 -lvpr_6 -lm
C_DIRECTIVE = -DVPR6 -DUSING_BISON
endif

FLAGS = $(DEBUG_FLAGS) $(WARN_FLAGS)  $(INC_FLAGS)
C_FLAGS = $(C_DIRECTIVE)

OBJS = \
OBJ/netlist_stats.o \
OBJ/output_graphcrunch_format.o \
OBJ/read_netlist.o \
OBJ/activity_estimation.o \
OBJ/read_blif.o \
OBJ/netlist_check.o \
OBJ/multipliers.o \
OBJ/output_blif.o \
OBJ/partial_map.o \
OBJ/outputs.o \
OBJ/netlist_optimizations.o \
OBJ/errors.o \
OBJ/node_creation_library.o \
OBJ/netlist_visualizer.o \
OBJ/netlist_create_from_ast.o \
OBJ/netlist_utils.o \
OBJ/odin_util.o \
OBJ/ast_optimizations.o \
OBJ/high_level_data.o \
OBJ/ast_util.o \
OBJ/read_xml_config_file.o \
OBJ/parse_making_ast.o \
OBJ/string_cache.o \
OBJ/verilog_bison.o \
OBJ/verilog_flex.o \
OBJ/hard_blocks.o \
OBJ/memories.o \
OBJ/verilog_preprocessor.o \
OBJ/queue.o \
OBJ/simulate_blif.o \
OBJ/odin_ii.o

SRC = \
SRC/netlist_stats.c \
SRC/output_graphcrunch_format.c \
SRC/read_netlist.c \
SRC/activity_estimation.c \
SRC/read_blif.c \
SRC/netlist_check.c \
SRC/multipliers.c \
SRC/output_blif.c \
SRC/partial_map.c \
SRC/outputs.c \
SRC/netlist_optimizations.c \
SRC/errors.c \
SRC/node_creation_library.c \
SRC/netlist_visualizer.c \
SRC/netlist_create_from_ast.c \
SRC/netlist_utils.c \
SRC/odin_util.c \
SRC/ast_optimizations.c \
SRC/high_level_data.c \
SRC/ast_util.c \
SRC/read_xml_config_file.c \
SRC/parse_making_ast.c \
SRC/string_cache.c \
SRC/hard_blocks.c \
SRC/memories.c \
SRC/verilog_preprocessor.c \
SRC/queue.c \
SRC/simulate_blif.c \
SRC/odin_ii.c

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
SRC/globals.h

$(EXE): CTAGS OBJ/verilog_bison.o OBJ/verilog_flex.o $(OBJS)
	$(CC) $(C_FLAGS) $(FLAGS) $(OBJS) -o $(EXE) $(LIB)

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
