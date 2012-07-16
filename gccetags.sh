#!/bin/bash

${CC:-gcc} -S -fplugin=./gccetags.so "$@" >> TAGS
