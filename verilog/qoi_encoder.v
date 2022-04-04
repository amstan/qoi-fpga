`define QOI_OP_INDEX  8'h00 /* 00xxxxxx */
`define QOI_OP_DIFF   8'h40 /* 01xxxxxx */
`define QOI_OP_LUMA   8'h80 /* 10xxxxxx */
`define QOI_OP_RUN    8'hc0 /* 11xxxxxx */
`define QOI_OP_RGB    8'hfe /* 11111110 */
`define QOI_OP_RGBA   8'hff /* 11111111 */

`define QOI_MASK_2    8'hc0 /* 11000000 */

module qoi_encoder(
	input wire[7:0] r,
	input wire[7:0] g,
	input wire[7:0] b,
	input wire clk,
	input wire rst,

	output reg[31:0] chunk,
	output reg[2:0] chunk_bytes
);

wire[31:0] px = {r, g, b, 8'(0)};

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

// QOI_OP_RUN
// We need to delay chunk output by one pixel because we cannot emit 2 chunks
// per pixel like software does when the run ends.
wire is_repeating = {prev_r, prev_g, prev_b, 8'(0)} == px;
reg[31:0] next_chunk;
reg[2:0] next_chunk_bytes;
reg[5:0] run;

always @ (posedge clk) begin
	prev_r <= r;
	prev_g <= g;
	prev_b <= b;

	if (is_repeating) begin /* QOI_OP_RUN */
		// For debugging: uncomment. For power-saving: comment
		//next_chunk <= {`QOI_OP_RUN | run, 24'(0)}; // Dummy
		// This chunk is not over, let output know not to expect anything yet
		next_chunk_bytes <= 0;
		run <= run + 1;

	end else if (
		vr > -3 && vr < 2 &&
		vg > -3 && vg < 2 &&
		vb > -3 && vb < 2
	) begin
		next_chunk <= {
			`QOI_OP_DIFF | 8'(vr + 2) << 4 | 8'(vg + 2) << 2 | 8'(vb + 2),
			24'(0)
		};
		next_chunk_bytes <= 1;

	end else if (
		vg_r >  -9 && vg_r <  8 &&
		vg   > -33 && vg   < 32 &&
		vg_b >  -9 && vg_b <  8
	) begin
		next_chunk <= {
			`QOI_OP_LUMA | 8'(vg + 32),
			8'(vg_r + 8) << 4 | 8'(vg_b +  8),
			16'(0)
		};
		next_chunk_bytes <= 2;

	end else begin
		next_chunk <= {`QOI_OP_RGB, r, g, b};
		next_chunk_bytes <= 4;
	end

	chunk <= next_chunk;
	chunk_bytes <= next_chunk_bytes;
	if (((run > 0) && !is_repeating) || (run == 62)) begin
		run <= is_repeating; // count the current one, otherwise start from 0
		chunk <= {`QOI_OP_RUN | 8'(run-1), 24'(0)};
		chunk_bytes <= 1;
	end
end

endmodule
