#!/usr/bin/env bash
set -euo pipefail

echo -n "1 thread"
time NTHREADS=1 ./pzip bigTest.txt > /dev/null
echo ""

echo -n "2 thread"
time NTHREADS=2 ./pzip bigTest.txt > /dev/null
echo ""

echo -n "3 thread"
time NTHREADS=3 ./pzip bigTest.txt > /dev/null
echo ""

echo -n "5 thread"
time NTHREADS=5 ./pzip bigTest.txt > /dev/null
echo ""

echo -n "10 thread"
time NTHREADS=10 ./pzip bigTest.txt > /dev/null
echo ""

