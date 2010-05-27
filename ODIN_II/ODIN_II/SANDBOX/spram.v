// DEFINES
`define WIDTH 8         // Bit width 
`define DEPTH 4         // Bit depth

module  spram(clock,
                reset_n,
                value_out,
                value_in
                );

// SIGNAL DECLARATIONS
input   clock;
input   reset_n;
input value_in;
output value_out;
wire value_out;

reg [`DEPTH-1:0] address_counter;
reg [`WIDTH-1:0] temp;
reg my_out;

single_port_ram inst1(
  .we(clock),
  .data(value_in),
  .out(my_out),
  .addr(address_counter));
// defparam inst1.depth = "1024";
// defparam inst1.width = "8";
// defparam inst1.hard_block = "dual_port_16x2048";
// defparam inst1.latency = "2";

always @(posedge clock)
begin
	if (reset_n == 1'b1) begin
		address_counter <= 4'b0000;
		value_out <= my_out;
	end
	else
	begin
		address_counter <= address_counter + 1;
		value_out <= my_out;
	end
end

endmodule

