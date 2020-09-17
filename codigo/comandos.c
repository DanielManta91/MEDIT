#include "comandos.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <ncurses.h>

void settings(int lin, int col, char *filename, int maxuser, char *mainpipe, int nrpipes){
	printf("\nNumero de linhas: %d", lin);
	printf("\nNumero de colunas: %d", col);
	printf("\nBase de dados(usernames): %s", filename);
	printf("\nNumero maximo de utilizadores: %d", maxuser);
	printf("\nNumero de named pipes de interaccao: %d", nrpipes);
	printf("\nNome de named pipe principal: %s\n", mainpipe);
}

void load(char *filename){
	printf("load\n");
}

void save(char *filename){
	printf("save\n");
}

void users(serv *ativo, int total){
	int h, m, s;	
	time_t now;
	time(&now);
	
	struct tm *local;
	local = localtime(&now);


	for(int i=0; i<MAX_LIMITE_USERS; i++){
		if(ativo[i].pid!=0){		

			if(local->tm_hour > ativo[i].hora && local->tm_min >= ativo[i].min && local->tm_sec >= ativo[i].seg)
				h = local->tm_hour - ativo[i].hora;
			else 
				h = 0;
			if((h>0 && local->tm_min < ativo[i].min) || local->tm_min < ativo[i].min)
				m = (60-local->tm_min) + ativo[i].min;
	
			else
			if(local->tm_min > ativo[i].min && local->tm_sec >= ativo[i].seg)
				m = local->tm_min - ativo[i].min;
			else
				m = 0;	
	
			if(m > 0 && local->tm_sec < ativo[i].seg || local->tm_sec < ativo[i].seg)
				s = (60-local->tm_sec) + ativo[i].seg;	
			else
				s = local->tm_sec - ativo[i].seg;

			if(ativo[i].emLinha==1)
				printf("\nNome:%s| Tempo activo: %dh.%dm.%ds | Pipe de interaccao: %s | Linhas editadas: %d%% | Linha em edicao: %d\n", ativo[i].username, h, m, s, ativo[i].comunica, (ativo[i].linhas_editadas*100)/total, ativo[i].posy);
			else
				printf("\nNome:%s| Tempo activo: %dh.%dm.%ds | Pipe de interaccao: %s | Linhas editadas: %d%% | Nao esta a editar\n", ativo[i].username, h, m, s, ativo[i].comunica, (ativo[i].linhas_editadas*100)/total);
		}
	}
}

void text(medit edit){
	WINDOW* tam_interface;
	WINDOW* info;
	WINDOW* interface;
	WINDOW* janela_linha;

	int l, c;
	int posx, posy;
	int k;
	int tecla;
	char line[edit.colunas];
	
	initscr();	//inicializa o controlo do terminal para o modo ncurses
	clear();
	noecho();	//o que é escrito no teclado deixa de aparecer no ecran
	cbreak();	//passa a ignorar o que possa estar em buffer para mudanças de linha etc 

	start_color();


	interface = newwin(edit.linhas+1, edit.colunas+1, 0, 13);
	keypad(interface, TRUE);	//activar a função de leitura de teclas de funcionalidades variadas(ESC, ENTER, etc)

	tam_interface = newwin(1, 7, edit.linhas+2, edit.colunas+2);

	info = newwin(edit.linhas, 13, 0, 0);

	posy = 0;
	posx = 0;

	for(l=0;l<edit.linhas;l++)
		mvwprintw(info, l, 0, "%s", edit.nome[l]);		//apresentaçao do username do utilizador que esteja a alterar a linha
		
	wrefresh(info);
	
	//Exibiçao da interface ( | na vertical, - na horizontal)

	for(l=0; l<edit.linhas; l++)					
		for(c=0; c<edit.colunas; c++)
			mvwaddch(interface, l, c, edit.texto[l][c]);

	for(l=0; l<edit.linhas+1; l++)
		mvwaddch(interface, l, edit.colunas, '|');

	for(c=0; c<edit.colunas+1; c++)
		mvwaddch(interface, edit.linhas, c, '-');

	mvwaddch(info, 0, 10, '0');
	mvwaddch(info, 0, 11, '0');
	
	//Exibiçao dos Numeros correspondentes das linhas
	
	k=0;
	for(l=0; l<edit.linhas; l++){
		if(l<10){
			mvwaddch(info, l, 10, '0');
			mvwaddch(info, l, 11, '0'+l);
		}
		else{
			mvwaddch(info, l, 10, '1');
			mvwaddch(info, l, 11, '0'+k);
			k++;
		}
		mvwaddch(info, l, 12, '|');
		for(c=11; c>0; c--);
	}
	wrefresh(info);

	wmove(interface, posy, posx);	//repor o cursor novamente na posição inicial depois de ele imprimir o conteúdo da janela "info"
	wrefresh(interface);
	
	//Exibiçao do numero de linhas e colunas do editor
	
	mvwprintw(tam_interface, 0, 0, "(%d,%d)", edit.linhas, edit.colunas);
	wrefresh(tam_interface);
	
	wmove(interface, posy, posx);
	wrefresh(interface);
	

	//Menu de teclas disponiveis em modo de navegaçao
	do{
		tecla = wgetch(interface);

	} while(tecla!=27);

	endwin();
	return;
}


