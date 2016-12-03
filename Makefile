all: euler3

euler3: euler3.o
	clang -O3 -pedantic -Wall -lglfw3 -framework OpenGL euler3.o -o euler3

euler3.o: euler3.c
	clang -std=c11 -pedantic -O3 -Wall -c euler3.c

clean:
	rm -f euler3.o euler3

.PHONY: clean
