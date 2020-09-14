
CC = gcc

.PHONY = test clean wtest 3test

ARGS = -Wextras

wish : wish.c
	${CC} wish.c -o wish ${ARGS}

3wish : 3wish.c readline-8.0
	${CC} 3wish.c -c
	${CC} -o 3wish 3wish.o readline-8.0/libreadline.a -lncurses ${ARGS}

test : wtest 3test

wtest : wish
	./test-wish.sh

3test : 3wish
	@ [ -f wish ] && mv wish wish-backup
	@mv 3wish wish
	./test-wish.sh
	@mv wish 3wish
	@ [ -f wish-backup ] && mv wish-backup wish


readline-8.0 :
	curl ftp://ftp.cwru.edu/pub/bash/readline-8.0.tar.gz --output readline-8.0.tar.gz
	tar -xf readline-8.0.tar.gz
	rm readline-8.0.tar.gz
	cd readline-8.0
	cd readline-8.0 && ./configure && make

clean :
	@ [ -z $(ls | grep -e .\*\\.o) ] || rm *.o
	@ [ -f 3wish ] && rm 3wish || :
	@ [ -f wish ] && rm wish || :
