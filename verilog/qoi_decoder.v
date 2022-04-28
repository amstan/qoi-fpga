`define QOI_OP_INDEX  2'b00       /* 0x00 */
`define QOI_OP_DIFF   2'b01       /* 0x40 */
`define QOI_OP_LUMA   2'b10       /* 0x80 */
`define QOI_OP_RUN    2'b11       /* 0xc0 */
`define QOI_OP_RGB    8'b11111110 /* 0xfe */
`define QOI_OP_RGBA   8'b11111111 /* 0xff */

module qoi_decoder(
	input reg[7:0] chunk[4:0],
	output reg[2:0] chunk_len_consumed,

	input wire clk,
	input wire rst,

	output wire[7:0] r,
	output wire[7:0] g,
	output wire[7:0] b,
	output wire[7:0] a
);

wire[2:0] shortop = chunk[0][7:6];

reg[7:0] next_r;
reg[7:0] next_g;
reg[7:0] next_b;
reg[7:0] next_a;
reg[2:0] next_chunk_len_consumed;

// QOI_OP_INDEX
// previously seen pixel array, indexed by hash of the color
reg[31:0] index[63:0];
wire[5:0] index_pos = next_r * 3 + next_g * 5 + next_b * 7 + next_a * 11;

// QOI_OP_DIFF
wire signed[1:0] vr = chunk[0][5:4] - 2;
wire signed[1:0] vg = chunk[0][3:2] - 2;
wire signed[1:0] vb = chunk[0][1:0] - 2;

// QOI_OP_LUMA
// Reference implementation software calls "luma" variables "vg" too, but we
// can't since we want different sized wires between the OPs.
wire signed[5:0] luma = chunk[0][5:0] - 32;
wire signed[3:0] luma_r = chunk[1][7:4] - 8;
wire signed[3:0] luma_b = chunk[1][3:0] - 8;

// QOI_OP_RUN counters
reg[5:0] run;
reg[5:0] next_run;
// The spec has a few illegal run lengths, so let's re-purpose one of them
`define NOT_IN_A_RUN 'b111111

always @ * begin
	next_r = r;
	next_g = g;
	next_b = b;
	next_a = a;

	next_run = run;

	if (chunk[0] == `QOI_OP_RGB) begin
		{next_r, next_g, next_b} = {chunk[1], chunk[2], chunk[3]};
		next_chunk_len_consumed = 4;

	end else if (chunk[0] == `QOI_OP_RGBA) begin
		{next_r, next_g, next_b, next_a} = {chunk[1], chunk[2], chunk[3], chunk[4]};
		next_chunk_len_consumed = 5;

	end else if (shortop == `QOI_OP_INDEX) begin
		{next_r, next_g, next_b, next_a} = index[chunk[0][5:0]];
		next_chunk_len_consumed = 1;

	end else if (shortop == `QOI_OP_DIFF) begin
		next_r = signed'(r) + vr;
		next_g = signed'(g) + vg;
		next_b = signed'(b) + vb;
		next_chunk_len_consumed = 1;

	end else if (shortop == `QOI_OP_LUMA) begin
		next_r = signed'(r) + luma + luma_r;
		next_g = signed'(g) + luma;
		next_b = signed'(b) + luma + luma_b;
		next_chunk_len_consumed = 2;

	end else if (shortop == `QOI_OP_RUN) begin
		if (run == `NOT_IN_A_RUN) begin // Is this run new?
			next_run = chunk[0][5:0];
		end else begin
			next_run--;
		end

		if (next_run > 0) begin
			// We still have pixels to go, don't consume chunk yet
			next_chunk_len_consumed = 0;
		end else begin
			// Finish run
			next_run = `NOT_IN_A_RUN;
			next_chunk_len_consumed = 1;
		end

	end else begin
		$error("Uncaught QOI_OP decoding. chunk[0]=%h", chunk[0]);
		{next_r, next_g, next_b, next_a} = 'h_deadbeef;
		next_chunk_len_consumed = 0;
	end
end

always @ (posedge clk) begin
	chunk_len_consumed <= next_chunk_len_consumed;
	{r, g, b, a} <= {next_r, next_g, next_b, next_a};

	index[index_pos] <= {next_r, next_g, next_b, next_a};
	run <= next_run;

	if (rst) begin
		chunk_len_consumed <= 0;

		r <= 0;
		g <= 0;
		b <= 0;
		a <= 255;

		index <= '{default: 0};
		run <= `NOT_IN_A_RUN;
	end
end

endmodule
