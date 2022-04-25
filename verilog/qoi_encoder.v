`define QOI_OP_INDEX  2'b00       /* 0x00 */
`define QOI_OP_DIFF   2'b01       /* 0x40 */
`define QOI_OP_LUMA   2'b10       /* 0x80 */
`define QOI_OP_RUN    2'b11       /* 0xc0 */
`define QOI_OP_RGB    8'b11111110 /* 0xfe */
`define QOI_OP_RGBA   8'b11111111 /* 0xff */

module qoi_encoder(
	input wire[7:0] r,
	input wire[7:0] g,
	input wire[7:0] b,
	input wire[7:0] a,

	input wire clk,
	input wire rst,

	output reg[7:0] chunk[4:0],
	output reg[2:0] chunk_len
);

wire[31:0] px = {r, g, b, a};

// QOI_OP_DIFF + QOI_OP_LUMA
reg[7:0] prev_r;
reg[7:0] prev_g;
reg[7:0] prev_b;

wire signed[7:0] vr = r - prev_r;
wire signed[7:0] vg = g - prev_g;
wire signed[7:0] vb = b - prev_b;

// QOI_OP_LUMA
wire signed[7:0] vg_r = vr - vg;
wire signed[7:0] vg_b = vb - vg;

// QOI_OP_RGBA
reg[7:0] prev_a;

// QOI_OP_RUN
// We need to delay chunk output by one pixel because we cannot emit 2 chunks
// per pixel like software does when the run ends.
wire is_repeating = {prev_r, prev_g, prev_b, prev_a} == px;
reg[7:0] next_chunk[4:0];
reg[2:0] next_chunk_len;
reg[5:0] run;

// QOI_OP_INDEX
// previously seen pixel array, indexed by hash of the color
reg[31:0] index[63:0];
wire[5:0] index_pos = r * 3 + g * 5 + b * 7 + a * 11;

always @ (posedge clk) begin
	if (is_repeating) begin /* QOI_OP_RUN */
		// For debugging: uncomment. For power-saving: comment
		next_chunk[0] <= {`QOI_OP_RUN, run}; // Dummy
		// This chunk is not over, let output know not to expect anything yet
		next_chunk_len <= 0;
		run <= run + 1;

	end else if (index[index_pos] == px) begin
		next_chunk[0] <= {`QOI_OP_INDEX, index_pos};
		next_chunk_len <= 1;

	end else if (prev_a != a) begin
		next_chunk[0] <= `QOI_OP_RGBA;
		next_chunk[1] <= r;
		next_chunk[2] <= g;
		next_chunk[3] <= b;
		next_chunk[4] <= a;
		next_chunk_len <= 5;

	end else if (
		vr > -3 && vr < 2 &&
		vg > -3 && vg < 2 &&
		vb > -3 && vb < 2
	) begin
		next_chunk[0] <= {`QOI_OP_DIFF, 2'(vr + 2), 2'(vg + 2), 2'(vb + 2)};
		next_chunk_len <= 1;

	end else if (
		vg_r >  -9 && vg_r <  8 &&
		vg   > -33 && vg   < 32 &&
		vg_b >  -9 && vg_b <  8
	) begin
		next_chunk[0] <= {`QOI_OP_LUMA, 6'(vg + 32)};
		next_chunk[1] <= {4'(vg_r + 8), 4'(vg_b + 8)};
		next_chunk_len <= 2;

	end else begin
		next_chunk[0] <= `QOI_OP_RGB;
		next_chunk[1] <= r;
		next_chunk[2] <= g;
		next_chunk[3] <= b;
		next_chunk_len <= 4;
	end

	prev_r <= r;
	prev_g <= g;
	prev_b <= b;
	prev_a <= a;

	index[index_pos] <= px;

	chunk <= next_chunk;
	chunk_len <= next_chunk_len;
	if (((run > 0) && !is_repeating) || (run == 62)) begin
		run <= is_repeating; // count the current repeat, otherwise start from 0
		chunk[0] <= {`QOI_OP_RUN, 6'(run - 1)};
		chunk_len <= 1;
	end

	if (rst) begin
		prev_r <= 0;
		prev_g <= 0;
		prev_b <= 0;
		prev_a <= 255;

		chunk <= '{default:0};
		chunk_len <= 0;

		next_chunk <= '{default:0};
		next_chunk_len <= 0;

		run <= 0;

		index <= '{default: 0};
	end
end

endmodule
