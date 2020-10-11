CC = clang

DEBUG = -g -fsanitize=address
OPT = -O3

.PHONY = test speed

pzip:
	${CC} pzip.c -Werror -pthread -o pzip ${OPT}

debug:
	${CC} pzip.c -Werror -pthread -o pzip ${DEBUG}

clean:
	@ [ -f pzip ] && rm pzip || true
	@ [ -f test_file ] && rm test_file || true
	@ [ -d tests-out ] && rm -r tests-out || true

bigTest.txt:
	./make_test_file.sh

speed: bigTest.txt
	./speed.sh

test: pzip
	./test-pzip.sh
