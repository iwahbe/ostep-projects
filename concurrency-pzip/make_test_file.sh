#!/usr/bin/env bash
set -euo pipefail


testfilesize=500000000
donetime=$(expr $(date +"%s") + 20)
while [ "$donetime" -gt $(date +"%s") ]; do
    head /dev/urandom | LC_CTYPE=C tr -dc A-Za-z0-9;
done | head -c $testfilesize > test_file
