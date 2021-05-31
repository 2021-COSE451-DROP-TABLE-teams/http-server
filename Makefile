CC = gcc
CFLAGS = -z execstack -fno-stack-protector -z norelro -g -O0

all: server guestbook.cgi

server: main.o core.o 
	${CC} ${CFLAGS} -o server main.o core.o

main.o: main.c
	${CC} ${CFLAGS} -c main.c

core.o: core.h core.c
	${CC} ${CFLAGS} -c core.c

guestbook.cgi: guestbook.o sha256.o
	${CC} ${CFLAGS} -o guestbook.cgi guestbook.o sha256.o

sha256.o: sha256.h sha256.c
	${CC} ${CFLAGS} -c sha256.c

clean:
	rm -rf ./*.o ./server ./guestbook.cgi
