server: main.o core.o 
	gcc -o server main.o core.o

main.o: main.c
	gcc -c main.c

core.o: core.h core.c
	gcc -c core.c

clean:
	rm -rf ./*.o ./server
