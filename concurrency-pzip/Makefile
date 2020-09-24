${CC} = clang

build:
	${CC} pzip.c -Werror -pthreads -o pzip

clean:
	@ [ -f pzip ] && rm pzip || true
	@ [ -f test_file ] && rm test_file || true

test_file:
	./make_test_file.sh
