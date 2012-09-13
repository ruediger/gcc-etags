#!/bin/bash

rm -f TAGS
make CC=./gccetags.sh CXX=./g++etags.sh

# todo merge all TAGS files in subdirs
