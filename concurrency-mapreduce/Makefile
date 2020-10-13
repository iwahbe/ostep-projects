
.PHONY = test clean
CC = clang
RUST_DEBUG_LOC = -L./target/debug ./target/debug/libconcurrency_mapreduce.dylib
RUST_RELEASE_LOC = -L./target/release ./target/release/libconcurrency_mapreduce.dylib
OUT = mapreduce


build:
	cargo build
	${CC} comp_test.c mapreduce.h ${RUST_DEBUG_LOC} -o ${OUT}

release:
	cargo build --release
	${CC} comp_test.c mapreduce.h ${RUST_DEBUG_LOC} -o ${OUT}

test: build
	cargo test

clean:
	@ cargo clean
	@ rm -r *.dSYM 2>/dev/null || true
	@ rm *.h.gch 2>/dev/null || true
	@ rm ${OUT} 2>/dev/null || true
	@ rm *.ghc 2>/dev/null || true
