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

	output reg[31:0] out,
	output reg[2:0] out_bytes
);

reg[7:0] prev_r;
reg[7:0] prev_g;
reg[7:0] prev_b;

wire signed[7:0] vr = r - prev_r;
wire signed[7:0] vg = g - prev_g;
wire signed[7:0] vb = b - prev_b;

wire signed[7:0] vg_r = vr - vg;
wire signed[7:0] vg_b = vb - vg;

always @ (posedge clk) begin
	prev_r <= r;
	prev_g <= g;
	prev_b <= b;

	if (
		vr > -3 && vr < 2 &&
		vg > -3 && vg < 2 &&
		vb > -3 && vb < 2
	) begin
		out <= (`QOI_OP_DIFF | 8'(vr + 2) << 4 | 8'(vg + 2) << 2 | 8'(vb + 2)) << 24;
		out_bytes <= 1;
	end else begin
		out <= {`QOI_OP_RGB, r, g, b};
		out_bytes <= 4;
	end
end

endmodule
