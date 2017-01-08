all:
	gcc -O2 -Wall -c bitpack.c -o bitpack.o
	gcc -O2 -Wall -c main.c -o main.o
	gcc -O2 -Wall -c network.c -o network.o
	gcc -o Chattutti bitpack.o main.o network.o -s -lSDL2 -lSDL2_net -lSDL2_ttf
