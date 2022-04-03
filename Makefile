CFLAGS += -I/usr/include/stb/

.PHONY: all
all: qoiconv qoibench

.PHONY: test
test: qoiconv
	./qoiconv images/testcard.png /tmp/testcard.qoi
	md5sum /tmp/testcard.qoi images/testcard.qoi

.PHONY: fuzz
fuzz: qoifuzz.c
	clang -fsanitize=address,fuzzer -g -O0 qoifuzz.c
	./a.out; rm -Rf a.out

qoibench: qoibench.c
	gcc -std=gnu99 -lpng ${CFLAGS} -o qoibench qoibench.c

qoiconv: qoiconv.c
	gcc -std=c99 ${CFLAGS} -o qoiconv qoiconv.c

.PHONY: clean
clean:
	rm -Rf qoiconv qoibench a.out
