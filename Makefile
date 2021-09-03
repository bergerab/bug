all: src/interpreter.c
	gcc -Wall -std=c89 -pedantic -o bug src/interpreter.c 
