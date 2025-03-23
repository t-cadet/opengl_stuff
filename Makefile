run: main
	./main
	
main: main.cpp
	clang++ -ggdb -std=c++20 -lglfw -lGL -lGLEW main.cpp -o main
