CC = gcc
AR = ar

WARN_FLAGS = -Wall 

DEBUG_FLAGS = -g
OPT_FLAGS = -O2
INC_FLAGS = -Iinclude
LIB_FLAGS = rcs

EXE = read_arch

FLAGS = $(DEBUG_FLAGS) $(WARN_FLAGS) $(INC_FLAGS)

OBJS = xml_arch.o read_xml_util.o ezxml.o ReadLine.o util.o 
SRC = xml_arch.c read_xml_util.c ezxml.c ReadLine.c util.c 
H = read_xml_arch_file.h read_xml_util.h ezxml.h ReadLine.h util.h arch_types.h physical_types.h logic_types.h

libvpr_5.a: $(OBJS)
	$(AR) $(LIB_FLAGS) $@ $(OBJS)

# Standalone executable to test architecture reader
$(EXE): main.c main.o libvpr_5.a
	$(CC) main.c $(INC_FLAGS) -c
	$(CC) main.o -o $(EXE) $(INC_FLAGS) -L../libvpr_5 -lm -lvpr_5


.c.o:%.c $(H)
	$(CC) $(FLAGS) -c $< -o $@ 

clean :
	rm -f libvpr_5.a
	rm -f $(OBJS)
	rm -f read_arch

