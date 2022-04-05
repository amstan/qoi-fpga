`define QOI_OP_INDEX  2'b00       /* 0x00 */
`define QOI_OP_DIFF   2'b01       /* 0x40 */
`define QOI_OP_LUMA   2'b10       /* 0x80 */
`define QOI_OP_RUN    2'b11       /* 0xc0 */
`define QOI_OP_RGB    8'b11111110 /* 0xfe */
`define QOI_OP_RGBA   8'b11111111 /* 0xff */

`define QOI_MASK_2    8'b11000000 /* 0xc0 */

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

endmodule
