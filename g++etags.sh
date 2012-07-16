#!/bin/bash

${CXX:-g++} -S -fplugin=./gccetags.so "$@" >> TAGS
