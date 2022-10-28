// mqoi_differ - part of a MQOI encoder
// Diff current frame from previous ("old") frame and output a resulting diff,
// pixel by pixel, that can be passed to an normal qoi_encoder.

module mqoi_encoder(
	input wire[7:0] old_frame_r,
	input wire[7:0] old_frame_g,
	input wire[7:0] old_frame_b,
	input wire[7:0] old_frame_a,

	input wire[7:0] curr_frame_r,
	input wire[7:0] curr_frame_g,
	input wire[7:0] curr_frame_b,
	input wire[7:0] curr_frame_a,

	output wire[7:0] diff_r,
	output wire[7:0] diff_g,
	output wire[7:0] diff_b,
	output wire[7:0] diff_a,
);

always @ * begin
	wire same_r = old_frame_r == curr_frame_r;
	wire same_g = old_frame_g == curr_frame_g;
	wire same_b = old_frame_b == curr_frame_b;
	assert curr_frame_a == 255; // TODO: fix this

	diff_r = curr_frame_r;
	diff_g = curr_frame_g;
	diff_b = curr_frame_b;
	diff_a = 255;

	if same_r && same_g && same_b then
		diff_r = 0;
		diff_g = 0;
		diff_b = 0;
		diff_a = 0;
	end

	// TODO: Make use of a bigger variety of diff_a, ex to
	// slowly increase brightness, might compress better.
end

endmodule
