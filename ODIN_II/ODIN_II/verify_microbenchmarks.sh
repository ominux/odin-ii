#!/bin/bash 

TEST_DIR="REGRESSION_TESTS/BENCHMARKS/MICROBENCHMARKS"
ARCH="$TEST_DIR/k4_N10_memSize32768_memData32.xml"

for benchmark in $TEST_DIR/*.v
do 
	basename=${benchmark%.v}
	input_vectors="$basename"_input
	output_vectors="$basename"_output	

	############################
	# Simulate using verilog. 
	############################
	# With the arch file
	rm output_vectors
	./odin_II.exe -a $ARCH -V "$benchmark" -t "$input_vectors" -T "$output_vectors" || exit 1
	[ -e "output_vectors" ] || exit 1

	# Without the arch file
	rm output_vectors
	./odin_II.exe -V "$benchmark" -t "$input_vectors" -T "$output_vectors" || exit 1
	[ -e "output_vectors" ] || exit 1

	############################
	# Simulate using the blif file. 
	############################
	# With the arch file. 	
	rm "temp.blif"
	./odin_II.exe -a $ARCH -V "$benchmark" -o "temp.blif" || exit 1
	[ -e "temp.blif" ] || exit 1

	rm output_vectors
	./odin_II.exe -a $ARCH -b "temp.blif" -t "$input_vectors" -T "$output_vectors" || exit 1
	[ -e "output_vectors" ] || exit 1

	# Without the arch file. 	
	rm "temp.blif"
	./odin_II.exe -V "$benchmark" -o "temp.blif" || exit 1
	[ -e "temp.blif" ] || exit 1

	rm output_vectors
	./odin_II.exe -b "temp.blif" -t "$input_vectors" -T "$output_vectors" || exit 1
	[ -e "output_vectors" ] || exit 1


done

echo "--------------------------------------" 
echo "$0: All tests completed successfully." 