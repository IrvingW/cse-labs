#!/usr/bin/env bash
make clean
rm *.log
make
./rsm_tester.pl 0 1 2 3 4
