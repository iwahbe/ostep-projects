CC = clang

.PHONY = test clean wtest 3test debug-flags

DEBUG = -fsanitize=address -g

ARGS = -Wextra

all : wish 3wish test

debug-flags :
ARGS +=${DEBUG}

3debug : | debug-flags 3wish
wdebug : | debug-flags wish

wish : wish.c
	${CC} wish.c -o wish ${ARGS}

3wish : 3wish.c readline-8.0
	${CC} 3wish.c -c ${CFLAGS}
	${CC} -o 3wish 3wish.o readline-8.0/libreadline.a -lncurses ${ARGS}

test : wtest 3test

wtest : wdebug
	./test-wish.sh

3test : CFLAGS = "-DTEST_ERROR=1"
3test : 3debug
	@ [ -f wish ] && mv wish wish-backup || true
	@ mv 3wish wish
	./test-wish.sh || mv wish 3wish
	@ mv wish 3wish
	@ [ -f wish-backup ] && mv wish-backup wish || true


readline-8.0 :
	curl ftp://ftp.cwru.edu/pub/bash/readline-8.0.tar.gz --output readline-8.0.tar.gz
	tar -xf readline-8.0.tar.gz
	rm readline-8.0.tar.gz
	cd readline-8.0
	cd readline-8.0 && ./configure && make

clean :
	@ rm *.o 2&> /dev/null || true
	@ [ -e tests-out ] && rm -r tests-out || true
	@ [ -f 3wish ] && rm 3wish || true
	@ [ -f wish ] && rm wish || true
	@ [ -f wish-backup ] && rm wish-backup || true
