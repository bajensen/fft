main: main.c
	gcc -o main main.c -lm -lpulse -lpulse-simple -lfftw3 -lSDL -lGL -lGLU -lGLEW -llirc_client -lpthread
	
all: main
