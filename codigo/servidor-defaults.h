#ifndef SERVIDOR_DEFAULTS_H
#define SERVIDOR_DEFAULTS_H

#define TAM 9	//tamanho máximo de username
#define MAX_USERS 3
#define MAX_LIMITE_USERS 15
#define DATABASE "meditdb.txt"
#define PIPEPR "pipr"
#define FIFO_CLI "sss%d"
#define NR_PIPES 1

typedef struct server_users{
	char username[TAM]; 
	char pipe[20];
	char comunica[10];
	char linha[45];		//buffer para fazer alterações
	char buffer[45];	//buffer para repor no caso de ESC
	int indice;		//indice no vector de ocupação dos pipes
	int pid;
	int servpid;
	int posx;
	int posy;
	int hora, min, seg;	//guardar tempo de chegada do utilizador
	int linhas_editadas;
	int tecla;
	int erro;	//erro ortográfico
	int emLinha;	//para saber se linha está trancada ou não
	
}serv;


typedef struct thread_info{
	char pipes[MAX_LIMITE_USERS][3];
	int ocupados[MAX_LIMITE_USERS];
	int sair;
}tr_info;


#endif
