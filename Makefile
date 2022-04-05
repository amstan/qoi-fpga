CFLAGS += -I/usr/include/stb/ -Wall -Wno-unused-function

.PHONY: all
all: qoiconv qoibench

TEST?=images/testcard

.PHONY: test_encode
test_encode: qoiconv
	./qoiconv ${TEST}.png /tmp/ours.qoi
	md5sum /tmp/ours.qoi ${TEST}.qoi
	qoiconv ${TEST}.qoi /tmp/theirs.png
	qoiconv /tmp/ours.qoi /tmp/ours.png
	md5sum /tmp/ours.png /tmp/theirs.png

.PHONY: test_decode
test_decode: qoiconv
	./qoiconv ${TEST}.qoi /tmp/ours.png
	qoiconv ${TEST}.qoi /tmp/theirs.png
	md5sum /tmp/ours.png /tmp/theirs.png

.PHONY: fuzz
fuzz: qoifuzz.c
	clang -fsanitize=address,fuzzer -g -O0 qoifuzz.c
	./a.out; rm -Rf a.out

.c.o:
	gcc $(CFLAGS) $< -c -o $@

qoibench: qoibench.o
	g++ -lpng ${LDFLAGS} qoibench.o -o qoibench

qoiconv: qoiconv.o
	g++ ${LDFLAGS} qoiconv.o -o qoiconv

qoiconv.o qoibench.o fuzz: qoi.h

.PHONY: clean
clean:
	rm -Rf *.o
	rm -Rf qoiconv qoibench a.out
	rm -Rf build/

## Verilog via Verilator

VFLAGS ?= -Wall

 # These are inane, something as simple as (var == 25) ? : will warn with this on
VFLAGS += -Wno-WIDTH

build/V%__ALL.o: verilog/%.v
	# Generate the verilator makefile + cpp then compile it
	verilator $(VFLAGS) -cc $< --Mdir build/ --build

build/verilated.o: /usr/share/verilator/include/verilated.cpp
	g++ -I /usr/share/verilator/include -I build $^ -c -o $@

build/qoi_verilator_shim.o: qoi_verilator_shim.c build/Vqoi_encoder__ALL.o build/Vqoi_decoder__ALL.o
	g++ -I /usr/share/verilator/include -I build ${CFLAGS} $< -c -o $@

ifdef VERILATED
CFLAGS += -DQOI_FPGA_IMPLEMENTATION
qoiconv qoibench fuzz: build/Vqoi_encoder__ALL.o build/Vqoi_decoder__ALL.o build/verilated.o build/qoi_verilator_shim.o
LDFLAGS += build/Vqoi_encoder__ALL.o build/Vqoi_decoder__ALL.o build/verilated.o build/qoi_verilator_shim.o
endif
