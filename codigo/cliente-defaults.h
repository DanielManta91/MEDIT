#ifndef CLIENTE_DEFAULTS_H
#define CLIENTE_DEFAULTS_H

#define MAX_NOME 9



typedef struct cliente{
	char username[MAX_NOME];
	int pid;
	int valido;
	int slot;
}cli;

#endif
