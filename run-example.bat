gcc src\interpreter.c -pedantic -std=c89 -Wall -lm -o bug && bug --run-tests
bug -c example.bug -o example.bbc && bug example.bbc