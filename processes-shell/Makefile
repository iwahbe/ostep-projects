
CC = gcc

.PHONY = test

wish : wish.c
	${CC} wish.c -o wish

test : wish
	./test-wish.sh

readline-8.0 :
	curl ftp://ftp.cwru.edu/pub/bash/readline-8.0.tar.gz --output readline-8.0.tar.gz
	tar -xf readline-8.0.tar.gz
	rm readline-8.0.tar.gz
