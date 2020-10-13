
.PHONY = test clean
CC = clang
RUST_DEBUG_LOC = -L./target/debug -lconcurrency_mapreduce
RUST_RELEASE_LOC = -L./target/release -lconcurrency_mapreduce
OUT = mapreduce

DRIVER = comp_test.c


build:
	cargo build
	${CC} ${DRIVER} ${RUST_DEBUG_LOC} -o ${OUT}

release:
	cargo build --release
	${CC} ${DRIVER} ${RUST_RELEASE_LOC} -o ${OUT}

test: build
	cargo test

clean:
	@ cargo clean
	@ rm -r *.dSYM 2>/dev/null || true
	@ rm *.h.gch 2>/dev/null || true
	@ rm ${OUT} 2>/dev/null || true
	@ rm *.ghc 2>/dev/null || true
