all:
	clang++ main.cpp -std=c++14 -g -O0 -I/usr/local/include -L/usr/local/lib -lSDL2 -lSDL2_ttf
	#./a.out invaders.h
