#!/usr/bin/env bash
set -euo pipefail

echo "Taking tests/6.in and"
cat ./tests/6.in ./tests/6.in > bigTest.txt
echo "Doubling it"
cat bigTest.txt bigTest.txt > bigtest2.txt
echo "Doubling it"
cat bigTest2.txt bigTest2.txt > bigtest.txt
echo "Doubling it"
cat bigTest.txt bigTest.txt > bigtest2.txt
echo "Doubling it"
cat bigTest2.txt bigTest2.txt > bigtest.txt
echo "Doubling it"
cat bigTest.txt bigTest.txt > bigtest2.txt
echo "Doubling it"
cat bigTest2.txt bigTest2.txt > bigtest.txt
echo "Doubling it"

rm bigTest2.txt 
echo "Thats 2^7 times bigger"
