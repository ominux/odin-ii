Odin II - Version 0.2 - Quick Start

In the DOCUMENTATION directory there is more thorough details about
Odin II.  This is a quick start guide.

INSTALL
------------

1. LIBVPR

This library needs to be built for your platform.  The Makefile for
Odin will link the LIBVPR library located in:
../libvpr_5 or/and ../libvpr_6

Note: 
- for WIN32 vs. NIX compilation you'll have to edit the ezxml.c files
to "#define WIN32" or not.

2. Odin II

Odin is compiled with a standard Makefile.  For the first time
I would suggest a:
make clean && make

Note: 
- that Odin relies on Bison and Flex.  These details are not 
included in this install and are left to the user.
- the default make targets VRP 5.0.  For VPR 6.0 you can either
change the Makefile variable "BUILD=" to VPR6 or you can type:
make clean && make BUILD=VPR6

USAGE
-------------

To run Odin II quickly once installed, go into the QUICK_TEST
directory and type:
./quick_test.bash

This will compile a simple verilog file and generate outputs
from Odin.  The most important file being test.blif, which
is the BLIF formatted version of test.v.

To use Odin II, invoke it from the command line as ./odin_II.exe.
./odin_II.exe -h will give you a list of the possible commands 
available. You can specify a configuration file with the -c
option, as it ./odin_II.exe -c config.xml. A config file looks
like the following:
<config>
	<verilog_files>
		<!-- Way of specifying multiple files in a project -->
		<verilog_file>verilog_file.v</verilog_file>
	</verilog_files>
	<output>
		<!-- These are the output flags for the project -->
		<output_type>blif</output_type>
		<output_path_and_name>./output_file.blif</output_path_and_name>
		<target>
			<!-- This is the target device the output is being built for -->
			<arch_file>fpga_architecture_file.xml</arch_file>
		</target>
	</output>
	<optimizations>
		<!-- This is where the optimization flags go -->
	</optimizations>
	<debug_outputs>
		<!-- Various debug options -->
		<debug_output_path>.</debug_output_path>
		<output_ast_graphs>1</output_ast_graphs>
		<output_netlist_graphs>1</output_netlist_graphs>
	</debug_outputs>
</config>

You may specify multiple verilog files for synthesis. The 
fpga_architecture_file.xml format is specified from VPR. output_ast_graphs set 
to 1 will give you abstract syntax tree graphs which can be viewed using 
GraphViz. The output_netliet_graphs does the same, except it visualizes the
netlist synthesized by Odin II.

odin_II.exe can be invoked with a -V to synthesize a single verilog file with
the default architecture.

The -g x flag will simulate the generated netlist with x clock cycles, using 
random test input vectors. These vectors and the resulting output vectors are
written to input_vectors.out and output_vectors.out respectively. You can also
verify previous results with the -t test_vectors_file which supplies both
input and output vectors. Odin II will use the input vectors in order to test
if the netlist generates identical output vecotrs, or not. This can be used to
verify results of FPGA architecture exploration. 

A test vector file is as follows:
intput_1 input_2 output_1 output_2 output_3
0 0xA 1 0xD x

Binary values are represented with 0 and 1, while hex values are prepended by
``0x''. ``x'' represents an ``unknown'' value. Any input values which are not
specified in the test vector file will be set to ``unknown'' and may result in
faulty or unknown output vectors. Each line in the file represents an 
individual clock cycle's input and output values.

CONTACT
-------------

jamieson dot peter at gmail dot com
ken at unb dot ca
- We will service all requests as timely as possible, but
please explain the problem with enough detail to help. 
