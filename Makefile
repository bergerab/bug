all: src/interpreter.c
	gcc src/interpreter.c -pedantic -std=c89 -Wall -lm -o bug
