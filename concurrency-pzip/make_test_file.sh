#!/usr/bin/env bash
set -euo pipefail

f1=bigTest.txt
f2=bigTest2.txt 

echo "Taking tests/6.in and"
cat ./tests/6.in ./tests/6.in > $f1
echo "Doubling it"
cat $f1 $f1 > $f2
echo "Doubling it"
cat $f2 $f2 > $f1
echo "Doubling it"
cat $f1 $f1 > $f2
echo "Doubling it"
cat $f2 $f2 > $f1
echo "Doubling it"
cat $f1 $f1 > $f2
echo "Doubling it"
cat $f2 $f2 > $f1
echo "Doubling it"


rm $f2 
echo "Thats 2^7 times bigger"
