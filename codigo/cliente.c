#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <ctype.h>
#include <ncurses.h>
#include <pthread.h>
#include "cliente-defaults.h"
#include "servidor-defaults.h"
#include "medit-defaults.h"

WINDOW* tam_interface;
WINDOW* info;
WINDOW* interface;
WINDOW* janela_linha;

medit edit;
serv ativo;

int fd_cli;
int fd_ser;
char fifo_cli[20];

int escreve_linha();
int editor();


void sinal(int s){
	int r, l, c;
	FILE *f;
	char pipename[20];
	int w;
	if(s==2){
		f=fopen("ppipe.txt","r");
		
		if(f==NULL){
			printf("Nao foi possivel abrir o ficheiro ppipe.txt\n");
			exit(1);
		}
		
		fscanf(f,"%s",pipename);
		fclose(f);
		fd_ser = open(pipename, O_WRONLY);
		kill(ativo.servpid,SIGUSR1);
		w=write(fd_ser, &ativo,sizeof(serv));
		endwin();
		close(fd_ser);
		close(fd_cli);
		unlink(fifo_cli);
		exit(0);
	}
	if(s==10){	//Receber actualização no medit		//SIGUSR1
		r = read(fd_cli, &edit, sizeof(medit));

		for(l=0;l<edit.linhas;l++)
			mvwprintw(info, l, 0, "%s", edit.nome[l]);		//apresentaçao do username do utilizador que esteja a alterar a linha
		
		wrefresh(info);

		for(l=0; l<edit.linhas; l++)					
			for(c=0; c<edit.colunas; c++)
				mvwaddch(interface, l, c, edit.texto[l][c]);
		wmove(interface, ativo.posy, ativo.posx);
		wrefresh(interface);
	}
	if(s==12){	//Receber aviso que o servidor saiu	//SIGUSR2
		endwin();
		close(fd_ser);
		close(fd_cli);
		unlink(fifo_cli);
		printf("\nservidor terminou\n");
		exit(0);
	}
	
}


int main(int argc, char *argv[]){
	
	cli user;
	
	int i,w,r;
	int op, ok;
	char pipename[20];
	user.valido=0;
	user.slot=0;

	signal(SIGINT, sinal);	//2
	signal(SIGUSR1, sinal);	//10
	signal(SIGUSR2, sinal); //12
	
	//vai buscar o nome do pipe principal

	FILE *f;
	f=fopen("ppipe.txt","r");
	
	if(f==NULL){
		printf("Nao foi possivel abrir o ficheiro ppipe.txt\n");
		return 1;
	}
	
	fscanf(f,"%s",pipename);
	fclose(f);
	
	
	if(access(pipename,F_OK)!=0){
		fprintf(stderr, "[ERRO] Nao ha servidor\n");
		return -1;
	}
	
	sprintf(fifo_cli, FIFO_CLI, getpid());
	user.pid=getpid();

	mkfifo(fifo_cli, 0600);

	fd_cli = open(fifo_cli, O_RDWR);
	
	fd_ser = open(pipename, O_WRONLY);

	user.username[0]='\0';
	
	opterr=0;	//isto é para prevenir uma messagem de erro vinda do getopt no case de não reconhecer um caracter de opção
	
	while((op = getopt(argc, argv, "u:")) != -1)
		switch(op){
			case 'u':
				strcpy(user.username, optarg);				
				break;
			case '?':	//quando getopt não reconhece um caracter de opção ele retorna '?' e guarda o caracter em optopt
				if(optopt == 'u')
					fprintf(stderr, "Opcao -%c precisa de um argumento!\n", optopt);
				else if(isprint(optopt))	//verificar se um caracter pode ser impresso
					fprintf(stderr, "Opcao desconhecida -%c !\n", optopt);
				else
					fprintf(stderr, "Caracter de opcao desconhecido : %x\n", optopt);	//mensagem de erro onde imprime o caracter desconhecido em hexadecima e lowercase
				return 1;
			default :
				return 0;
		}

	
	
	if(user.username[0]=='\0'){
		while(user.valido==0 && user.slot==0){
			printf("\nInsira o username: ");
			scanf(" %s", user.username);
			
			w=write(fd_ser, &user, sizeof(cli));
			//printf("LE\n");
			r=read(fd_cli, &user, sizeof(cli));
			//printf("AQUI\n");
			
			if(user.slot==1 && user.valido==1){
				r = read(fd_cli, &edit, sizeof(medit));
				r = read(fd_cli, &ativo, sizeof(serv));
				close(fd_ser);
				ok = editor();
			}
			else if(user.slot != user.valido){
				printf("\nServidor Cheio, tente mais tarde");
			}
			else {
				printf("\nNome %s não encontrado na base de dados\n",user.username);
			}
		}
	}
	else{
		w=write(fd_ser, &user, sizeof(cli));
		//printf("LE\n");
		r=read(fd_cli, &user, sizeof(cli));
		printf("%s", user.username);
		//printf("AQUI\n");
			
		if(user.slot==1 && user.valido==1){
			r = read(fd_cli, &edit, sizeof(medit));
			r = read(fd_cli, &ativo, sizeof(serv));	
			close(fd_ser);
			ok = editor();
		}
		else if(user.slot != user.valido){
			printf("\nServidor cheio, tente mais tarde");
		}
		else {
			printf("\nNome %s não encontrado na base de dados\n",user.username);
		}	

	}
	close(fd_ser);
	close(fd_cli);
	unlink(fifo_cli);

	return 0;
}




int editor(){

	int l, c;
	int posx, posy;
	int r, w;
	int ok;
	int k;
	char pipename[20];
	FILE *f;
	
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
	
	fd_ser = open(ativo.comunica, O_WRONLY);

	//Menu de teclas disponiveis em modo de navegaçao
	do{
		ativo.tecla = wgetch(interface);

		w = write(fd_ser, &ativo, sizeof(serv));
		r = read(fd_cli, &ativo, sizeof(serv));	

		switch(ativo.tecla){
			case KEY_UP:		//mover uma posiçao para cima
				break;
			case KEY_DOWN:		//mover uma posiçao para baixo
				break;
			case KEY_LEFT:		//mover uma posiçao para a esquerda
				break;
			case KEY_RIGHT:		//mover uma posiçao para a direita
				break;
			case 27:	//ESC

				f=fopen("ppipe.txt","r");
				
				if(f==NULL){
					printf("Nao foi possivel abrir o ficheiro ppipe.txt\n");
					return 1;
				}
				
				fscanf(f,"%s",pipename);
				fclose(f);
				fd_ser = open(pipename, O_WRONLY);
				kill(ativo.servpid,SIGUSR1);
				w=write(fd_ser, &ativo,sizeof(serv));
				endwin();
				close(fd_ser);
				close(fd_cli);
				unlink(fifo_cli);
				exit(0);
			case 10:	//ENTER
				if(ativo.emLinha==1){
					r=read(fd_cli,&edit,sizeof(medit));
					ok = escreve_linha();		//funçao de escrita de linha(modo de ediçao)
					wmove(interface, ativo.posy, 0);
					wrefresh(interface);
				}
				break;
			default:
				break;
		}
		if(ativo.tecla == KEY_UP || ativo.tecla == KEY_DOWN || ativo.tecla == KEY_LEFT || ativo.tecla == KEY_RIGHT){
			wmove(interface, ativo.posy, ativo.posx);
			wrefresh(interface);
		}
	} while(1);

	endwin();

	return 0;
}


int escreve_linha(){		//modo de ediçao
	int i, l, c, w, r, ok;

	janela_linha = newwin(1, edit.colunas, ativo.posy, 13);		//Janela de ediçao

	keypad(janela_linha, TRUE);

	init_pair(1, COLOR_BLACK, COLOR_CYAN);

	wattrset(janela_linha, COLOR_PAIR(1));

	wbkgd(janela_linha, COLOR_PAIR(1));
	wclear(janela_linha);


	for(l=0;l<edit.linhas;l++)
		mvwprintw(info, l, 0, "%s", edit.nome[l]);		//apresentaçao do username do utilizador que esteja a alterar a linha
		
	wrefresh(info);

	
	for(i=0; i<edit.colunas; i++)				
		mvwaddch(janela_linha, 0, i, edit.texto[ativo.posy][i]);

	wmove(janela_linha, 0, ativo.posx);
	wrefresh(janela_linha);

		//Menu de teclas disponiveis em modo de ediçao
	
	do{
		ativo.tecla = wgetch(janela_linha);
		w = write(fd_ser, &ativo, sizeof(serv));
		r = read(fd_cli, &ativo, sizeof(serv));	

		switch(ativo.tecla){
			case KEY_UP:
			case KEY_DOWN:
				break;
			case KEY_LEFT:			//mover uma posiçao para a esquerda			
				break;
			case KEY_RIGHT:			//mover uma posiçao para a direita
				break;
			case KEY_DC:
			case KEY_BACKSPACE:		//remover uma letra
				r = read(fd_cli, &edit, sizeof(medit));
				for(c=0; c<edit.colunas; c++)	// update de ecran pos-alteraçao
					mvwaddch(janela_linha, 0, c, edit.texto[ativo.posy][c]);
				wrefresh(janela_linha);
				break;
			case 10:	//ENTER		aceitar alteraçoes da linha(alterações feitas fora da função)	
				r=read(fd_cli, &edit, sizeof(medit));		
				r=read(fd_cli, &ativo, sizeof(serv));		

				if(ativo.erro == 0){
					for(l=0; l<edit.linhas; l++)								// update de ecran pos-alteraçao
						for(c=0; c<edit.colunas; c++)
							mvwaddch(interface, l, c, edit.texto[l][c]);

					mvwprintw(tam_interface, 0, 0, "(%d,%d)", edit.linhas, edit.colunas);
					wrefresh(tam_interface);
					delwin(janela_linha);
					
					for(l=0;l<edit.linhas;l++)
						mvwprintw(info, l, 0, "%s", edit.nome[l]);		//apresentaçao do username do utilizador que esteja a alterar a linha
		
					wrefresh(info);				
					return 0;
				}

				for(c=0; c<edit.colunas; c++)
					mvwaddch(interface, ativo.posy, c, edit.texto[ativo.posy][c]);
				
				mvwprintw(tam_interface, 0, 0, "(Erro!)");
				wrefresh(tam_interface);
				wmove(janela_linha, 0, ativo.posx);
				wrefresh(janela_linha);
				break;
					
			case 27:	//ESC		cancelar alteraçoes da linha
				mvwprintw(info,ativo.posy,0,"        ");	//apaga nome do user
				wrefresh(info);
				delwin(janela_linha);
				r = read(fd_cli, &edit, sizeof(medit));

				for(c=0; c<edit.colunas; c++)
					mvwaddch(interface, ativo.posy, c, edit.texto[ativo.posy][c]);

				r = read(fd_cli, &ativo, sizeof(serv));
				return 0;
			default:	//Inserçao de uma tecla na linha
				r = read(fd_cli, &edit, sizeof(medit));	

				for(c=0; c<edit.colunas; c++)	// update de ecran pos-alteraçao
					mvwaddch(janela_linha, 0, c, edit.texto[ativo.posy][c]);
				
				wrefresh(janela_linha);
				break;
		}
		r = read(fd_cli, &ativo, sizeof(serv));
		wmove(janela_linha, 0, ativo.posx);
		wrefresh(janela_linha);
	} while(1);

	return 0;
}