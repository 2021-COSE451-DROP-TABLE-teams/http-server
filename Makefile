all: server guestbook.cgi

server: main.o core.o 
	gcc -o server main.o core.o

guestbook.cgi: guestbook.c
	gcc -o guestbook.cgi guestbook.c

main.o: main.c
	gcc -c main.c

core.o: core.h core.c
	gcc -c core.c

clean:
	rm -rf ./*.o ./server ./guestbook.cgi
