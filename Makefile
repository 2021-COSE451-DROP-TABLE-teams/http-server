CC = gcc
CFLAGS = -z execstack -fno-stack-protector -z norelro -g -O0

all: server guestbook.cgi

server: main.o core.o 
	${CC} ${CFLAGS} -o server main.o core.o

guestbook.cgi: guestbook.c
	${CC} ${CFLAGS} -o guestbook.cgi guestbook.c

main.o: main.c
	${CC} ${CFLAGS} -c main.c

core.o: core.h core.c
	${CC} ${CFLAGS} -c core.c

clean:
	rm -rf ./*.o ./server ./guestbook.cgi
