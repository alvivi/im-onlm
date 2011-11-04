#!/usr/bin/env bash

if [ "$1" = "clean" ]
then
	rm -f src/*.o src/*.bin filter
else
	gcc -O2 -Wall -c -I./includes src/stb_reader.c -o src/stb_reader.o
	gcc -O2 -Wall -c -I./includes src/stb_writer.c -o src/stb_writer.o
	gcc -O2 -Wall -I./includes -o filter -framework OpenCL src/filter.c src/stb_reader.o src/stb_writer.o
fi
