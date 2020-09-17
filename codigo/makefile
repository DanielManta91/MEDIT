FLAGS=-g -lncurses -lpthread

all : serv cli

serv : comandos.o servidor.o 
	gcc comandos.o servidor.o -o serv $(FLAGS)

cli : cliente.o
	gcc cliente.o -o cli $(FLAGS)

servidor.o : servidor.c comandos.h servidor-defaults.h cliente-defaults.h medit-defaults.h
	gcc servidor.c -c $(FLAGS)

comandos.o : comandos.c
	gcc comandos.c -c $(FLAGS)

cliente.o : cliente.c servidor-defaults.h cliente-defaults.h medit-defaults.h
	gcc cliente.c -c $(FLAGS)

limpa :
	rm servidor.o comandos.o cliente.o
