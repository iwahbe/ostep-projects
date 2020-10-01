${CC} = clang

DEBUG = -g -fsanitize=address

build:
	${CC} pzip.c -Werror -pthreads -o pzip ${DEBUG}

clean:
	@ [ -f pzip ] && rm pzip || true
	@ [ -f test_file ] && rm test_file || true

test_file:
	./make_test_file.sh
