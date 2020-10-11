CC = clang

DEBUG = -g -fsanitize=address
OPT = -O3

.PHONY = test

pzip:
	${CC} pzip.c -Werror -pthread -o pzip ${OPT}

debug:
	${CC} pzip.c -Werror -pthread -o pzip ${DEBUG}

clean:
	@ [ -f pzip ] && rm pzip || true
	@ [ -f test_file ] && rm test_file || true
	@ [ -d tests-out ] && rm -r tests-out || true

test_file:
	./make_test_file.sh

test: pzip
	./test-pzip.sh
