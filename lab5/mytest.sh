#! /usr/bin/env bash

./start.sh
echo "hello" > ./yfs1/f1
./yfs-version -c
echo "hello" > ./yfs1/f2
./yfs-version -p
