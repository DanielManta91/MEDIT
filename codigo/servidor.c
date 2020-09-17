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
#include "comandos.h"
#include "servidor-defaults.h"
#include "medit-defaults.h"
#include "cliente-defaults.h"

serv ativo[MAX_LIMITE_USERS];	//estrutura dos clientes activos no servidor.
medit edit;	
tr_info dados;	//dados relevantes para gerir named pipes nas threads.

pthread_mutex_t trinco;


int fd_ser;	//descritor do pipe do servidor. É global para poder ser fechado pela função sinal.
int nrusers;	//nr máximo de utilizadores activos.
int npipes;	//nr de named pipes utilizados para comunicação com clientes.

int anonR[2];	//matriz para os descritores das extremidades r/w do pipe anónimo para leitura.
int anonW[2];	//matriz para os descritores das extremidades r/w do pipe anónimo para escritura.

char pipename[20];
char filename[20];

int total_editadas;
int flag;
int linha;

int valida(char* nome, char* filename);
void* login(void* data);
void* executa(void* data);
void* statistics(void* data);

void sinal(int s){
	int i,r;
	serv ativosaida;
	if(s==2){
		for(i=0;i<MAX_LIMITE_USERS;i++)
			if(ativo[i].pid!=0)
				kill(ativo[i].pid,SIGUSR2);

		remove("ppipe.txt");
		close(fd_ser);
		unlink(pipename);
		for(i=0; i<MAX_LIMITE_USERS; i++)
			unlink(dados.pipes[i]);
		exit(0);
	}
	if(s==10){
		r=read(fd_ser, &ativosaida,sizeof(serv));
		for(i=0;i< MAX_LIMITE_USERS;i++){
			if(ativo[i].pid==ativosaida.pid){
				ativo[i].username[0]='\0'; 
				ativo[i].pid=0;
				ativo[i].pipe[0]='\0';
				ativo[i].posx=0;
				ativo[i].posy=0;
				ativo[i].tecla=0;
				ativo[i].comunica[0]='\0';
				ativo[i].emLinha=0;
				ativo[i].linha[0]='\0';
				ativo[i].buffer[0]='\0';
				ativo[i].hora=0;
				ativo[i].min=0;
				ativo[i].seg=0;
				ativo[i].linhas_editadas=0;
				dados.ocupados[ativo[i].indice]=dados.ocupados[ativo[i].indice]-1;
			}
		
		}
		printf("\nUtilizador %s abandonou o medit\n",ativosaida.username);
		printf("comando: ");
		fflush(stdout); 
	}
	
}

int main(int argc, char** argv){
	cli cliente;
	char comando[40];
	const char s[2] = " ";	//O delimitador no strtok
	char* token;
	char* ambiente;
	int entrou;
	int i, l, c, k, r, w, op;
	
	int fd_temp;

	pthread_t tarefa;
	pthread_t mainpipe;
	pthread_t* namedpipes;
	void* estado;		

	signal(SIGINT,sinal);
	signal(SIGUSR1,sinal);

	linha = -1;		//linha a ser libertada com o comando "Free"
		
	keypad(stdscr,TRUE);
		
	//Opções de especificaçao no arranque do servidor	
	filename[0]='\0';
	pipename[0]='\0';
	npipes=0;

	opterr=0;	//isto é para prevenir uma mensagem de erro vinda do getopt no case de não reconhecer um caracter de opção	

	while((op = getopt(argc, argv, "fpn:")) != -1){//FALTA A OPÇÃO "N" PARA O NR DE PIPES A CRIAR
		switch(op){
			case 'f':
				strcpy(filename, optarg);
				break;
			case 'p':
				strcpy(pipename, optarg);
				break;
			case 'n':
				npipes = atoi(optarg);
				break;
			case '?':	//quando getopt não reconhece um caracter de opção ele retorna '?' e guarda o caracter em optopt
				if(optopt == 'f' || optopt == 'p' || optopt == 'n')
					fprintf(stderr, "Opcao -%c precisa de um argumento!\n", optopt);
				else if(isprint(optopt))	//verificar se um caracter pode ser impresso
					fprintf(stderr, "Opcao desconhecida -%c !\n", optopt);
				else
					fprintf(stderr, "Caracter de opcao desconhecido : %x\n", optopt);	//mensagem de erro onde imprime o caracter desconhecido em hexadecima e lowercase
				return 1;
			default :
				return 0;
		}
	}

	if(filename[0]=='\0')
		strcpy(filename, DATABASE);

	if(pipename[0]=='\0')
		strcpy(pipename, PIPEPR);	
	
	if(npipes==0)
		npipes = NR_PIPES;


	if(access(pipename, F_OK)==0){
		fprintf(stderr, "[ERRO] Ja existe um servidor activo\n");
		return 1;
	}
	

	//Criação de um ficheiro de acesso ao nome do pipe principal
	FILE *f;
	f=fopen("ppipe.txt","w+");

	if(f==NULL){
		printf("Nao foi possivel abrir o ficheiro (ppipe.txt)\n");
		return 1;
	}

	fprintf(f, "%s\n",pipename);
	fclose(f);
	

	//VARIÁVEIS DE AMBIENTE: INICIO
	ambiente = getenv("MEDIT_TIMEOUT");
	
	if(ambiente != NULL)	//verificar se variável "ambiente" está vazia
		printf("\n%s", ambiente);


	ambiente = getenv("MEDIT_MAXLINES");
	
	if(ambiente != NULL){
		edit.linhas=atoi(ambiente);
		if(edit.linhas < 0 || edit.linhas > LIN)
			edit.linhas=LIN;
		
	}
	else
		edit.linhas=LIN;

	
	ambiente = getenv("MEDIT_MAXCOLUMNS");	
	
	if(ambiente != NULL){
		edit.colunas=atoi(ambiente);
		if(edit.colunas < 0 || edit.colunas > COL)
			edit.colunas=COL;
		
	}
	else
		edit.colunas=COL;
	
	
	ambiente = getenv("MEDIT_MAXUSERS");

	if(ambiente != NULL){
		nrusers = atoi(ambiente);
		
		if(nrusers < 0 || nrusers > MAX_LIMITE_USERS)
			nrusers=MAX_USERS;
	}
	else
		nrusers=MAX_USERS;
	
	//VARIÁVEIS DE AMBIENTE: FIM
	
	
	//Inicializaçao da matriz do medit
	for(l=0; l<edit.linhas; l++)
		for(c=0; c<edit.colunas; c++)
			edit.texto[l][c] = ' ';
	
	for(l=0;l<edit.linhas;l++)
		strcpy(edit.nome[l],"        ");
	
	
	//CRIAR PROCESSO FILHO E PIPES PARA EXECUÇÃO DO ASPELL COM COMUNICAÇÃO
	
	//cria os pipe anónimos	
	pipe(anonR);
	pipe(anonW);

	//buffer
	char lixo[200];	
	
	if(fork() == 0){
		close(0);	//nos descritores, liberta a posição 0 onde se encontra o stdin
		dup(anonW[0]);	//duplica extremidade de leitura do pipe para a posição 0
		close(anonW[0]);	//fecha a extremidade de leitura do pipe porque já foi duplicada
		close(anonW[1]);	//fecha a extremidade de escrita que não vai usar
		close(1);	//liberta a posição 1 onde se encontra o stdout
		dup(anonR[1]);	//duplica extremidade de escrita do pipe para a posição 1
		close(anonR[1]);	//fecha a extremidade de escrita já duplicada
		close(anonR[0]);	//fecha a extremidade de leitura que não vai usar
		execlp("aspell", "aspell", "-a", NULL);		// "-a" entra em modo de funcionamento pipe, recebe/escreve
	}
					
	r = read(anonR[0], lixo, sizeof(lixo));	//lê a primeira coisa que o aspell envia quando é iniciado
	//printf("%s",buftemp);
	
	//LANÇAMENTO DE THREADS: INICIO
	pthread_mutex_init(&trinco, NULL);

	namedpipes = (pthread_t*) malloc(npipes * sizeof(pthread_t));

	for(i=0; i<npipes; i++)
		dados.ocupados[i]=0;

	dados.sair=0;

	for(i=0; i<npipes; i++){
		sprintf(dados.pipes[i], "T%d", i);
		pthread_create(&namedpipes[i], NULL, executa, (char*)i);
	}
	

	pthread_create(&mainpipe, NULL, login, NULL);
	//LANÇAMENTO DE THREADS: FIM

	do{
		//Ler teclado
		printf("comando: "); 
		fflush(stdout);	
		scanf(" %[^\n]", comando);
		flag=1;
		

		token = strtok(comando,s);

		if(strcmp(token, "settings") == 0)
			settings(edit.linhas, edit.colunas, filename, nrusers, pipename, npipes);

		if(strcmp(token, "load") == 0){
			token = strtok(NULL, s);	//avança uma vez
			load(token);
		}
		if(strcmp(token, "save") == 0){
			token = strtok(NULL, s);
			save(token);
		}
		if(strcmp(token, "free") == 0){
			token = strtok(NULL, s);
			linha = atoi(token);
		}
		if(strcmp(token, "statistics") == 0){
			flag=0;
			entrou=1;
			pthread_create(&tarefa, NULL, statistics, NULL);
		}
		if(strcmp(token, "users") == 0)
			users(ativo, total_editadas);
		if(strcmp(token, "text") == 0)
			text(edit);
		if(flag==1 && entrou==1){
			pthread_join(tarefa, &estado);
			entrou=0;
		}
	
		if(strcmp(token, "shutdown") == 0)
			break;		
			
	}while(1);

	for(i=0; i<MAX_LIMITE_USERS; i++)
		if(ativo[i].pid!=0)
			kill(ativo[i].pid,SIGUSR2);	
	
	dados.sair=1;
	
	w=write(fd_ser, &cliente, sizeof(cli));
	pthread_join(mainpipe, &estado);
	
	for(i=0; i<npipes; i++){
		fd_ser = open(dados.pipes[i], O_WRONLY);
		w=write(fd_ser, &ativo[i], sizeof(serv));
		pthread_join(namedpipes[i], &estado);
	}
	
	pthread_mutex_destroy(&trinco);
	
	remove("ppipe.txt");

	return 0;
}


//////////////////////////
//THREAD DE AUTENTICAÇÃO//
//////////////////////////
void* login(void* data){
	int fd_cli, r, w;
	cli cliente;
	char fifo_cli[20];
	int i, l, c, k;
	int pos_cliente, pos_livre;
	int livre;
	int ok;

	int clientela[nrusers];		//array de users ativos	
	
	time_t now;

	struct tm *timeinfo;



	for(i=0; i<nrusers; i++)
		clientela[i]=-1;
	
	for(i=0; i<MAX_LIMITE_USERS; i++){
		ativo[i].username[0]='\0'; 
		ativo[i].pid=0;
		ativo[i].pipe[0]='\0';
		ativo[i].posx=0;
		ativo[i].posy=0;
		ativo[i].tecla=0;
		ativo[i].indice=0;
		ativo[i].comunica[0]='\0';
		ativo[i].emLinha=0;
		ativo[i].linha[0]='\0';
		ativo[i].buffer[0]='\0';
		ativo[i].hora=0;
		ativo[i].min=0;
		ativo[i].seg=0;
		ativo[i].linhas_editadas=0;
		ativo[i].servpid=getpid();
	}

	mkfifo(pipename, 0600);
	
	fd_ser = open(pipename, O_RDWR);
	
	do{
		r = read(fd_ser, &cliente, sizeof(cli));
		//printf("recebi um cliente %d\n",cliente.pid);
		for(k=0; ativo[k].pid != 0 && k<MAX_LIMITE_USERS; k++);	//Procurar uma estrutura de utilizador activo vazia
		

		for(l=0; l<npipes; l++){	//Pesquisar o namedpipe menos usado para enviar ao cliente
			if(dados.ocupados[l] == 0){
				ativo[k].indice = l;
				break;
			}
			if(dados.ocupados[l] != 0){
				livre = dados.ocupados[l];
				ativo[k].indice = l;
			}
			if(dados.ocupados[l] < livre){
				livre = dados.ocupados[l];
				ativo[k].indice = l;
			}
		}

		if(ativo[k].pid != cliente.pid){
			sprintf(fifo_cli, FIFO_CLI, cliente.pid);
			fd_cli = open(fifo_cli, O_WRONLY);
			pos_cliente = -1;
			pos_livre = -1;
			if(valida(cliente.username, filename)==0){
				for(i=0; i<nrusers; i++){		
					if(clientela[i] == cliente.pid)		//verificar se o cliente já se encontra activo						
						pos_cliente=i;
				
					if(clientela[i] == -1 && pos_livre==-1)
						pos_livre=i;	
				}
				if(pos_cliente==-1 && pos_livre!=-1){
					clientela[pos_livre] = cliente.pid;
					ativo[k].pid=cliente.pid;
					strcpy(ativo[k].username, cliente.username);
					strcpy(ativo[k].pipe, fifo_cli);
					strcpy(ativo[k].comunica, dados.pipes[ativo[k].indice]);
					time(&now);
					timeinfo = localtime(&now);
					ativo[k].hora = timeinfo->tm_hour;
					ativo[k].min = timeinfo->tm_min;
					ativo[k].seg = timeinfo->tm_sec;
					dados.ocupados[ativo[k].indice]=dados.ocupados[ativo[k].indice]+1;
				}

				cliente.slot=1;
				cliente.valido=1;
				w = write(fd_cli, &cliente, sizeof(cli));
				close(fd_cli); 
					
				fd_cli = open(fifo_cli, O_WRONLY);
				w = write(fd_cli, &edit, sizeof(medit));
				close(fd_cli); 
					
				fd_cli = open(fifo_cli, O_WRONLY);
				w = write(fd_cli, &ativo[k], sizeof(serv));
				close(fd_cli);
				//printf("Enviei %d bytes\n",w);
			} 
			if(valida(cliente.username,filename)==0 && pos_livre == -1){
				cliente.slot=0;
				cliente.valido=1;
				w=write(fd_cli, &cliente, sizeof(cli));
				close(fd_cli);
			}
			if(valida(cliente.username,filename)!=0){
				cliente.slot=0;
				cliente.valido=0;
				w=write(fd_cli, &cliente, sizeof(cli));
				close(fd_cli);
			}
		}
	}while(dados.sair==0);	

	close(fd_ser);
	unlink(pipename);
	pthread_exit(0);
}


/////////////////////////////////
//THREAD PARA EXECUÇÃO DO MEDIT//
/////////////////////////////////
void* executa(void* data){
	int i, l, c, k, r, w, ok;
	int conta;
	int fd_exe, fd_cli, nr;
	serv ativotemp;
	
	char buffer[edit.colunas];
	char temp;
	char spellR;
	char enter='\n';
	int comercial=0;
	int fd_tds;
	
	nr = (int)data;

	mkfifo(dados.pipes[nr], 0600);
	fd_exe = open(dados.pipes[nr], O_RDWR);

	do{
		r = read(fd_exe, &ativotemp, sizeof(serv));
		fd_cli = open(ativotemp.pipe, O_WRONLY);
		
		//printf("\ntecla:%d\n",ativotemp.tecla);
		for(k=0; k < MAX_LIMITE_USERS; k++)		//perceber qual é o cliente que comunicou
			if(ativo[k].pid == ativotemp.pid)
				break;
		ativo[k].posx = ativotemp.posx;
		ativo[k].posy = ativotemp.posy;
		ativo[k].tecla = ativotemp.tecla;
		ativo[k].erro = ativotemp.erro;
		ativo[k].emLinha = ativotemp.emLinha;
		ativo[k].indice = ativotemp.indice;
		for(i=0; i < edit.colunas; i++){
			ativo[k].linha[i]=ativotemp.linha[i];
			ativo[k].buffer[i]=ativotemp.buffer[i];
		}
				
		if(linha>-1 && ativo[k].posy == linha){
			ativo[k].tecla=27;
			linha=-1;
		}

		if(ativo[k].emLinha==1)
			w = write(fd_cli, &ativo[k], sizeof(serv));

		switch(ativo[k].tecla){
			case KEY_UP:		//mover uma posiçao para cima
				if(ativo[k].emLinha==0)				
					ativo[k].posy = (ativo[k].posy>0)?ativo[k].posy-1:ativo[k].posy; //if(pos>0) posy = posy-1 else posy = posy;
				break;
			case KEY_DOWN:		//mover uma posiçao para baixo
				if(ativo[k].emLinha==0)	
					ativo[k].posy = (ativo[k].posy<(edit.linhas-1))?ativo[k].posy+1:ativo[k].posy;
				break;
			case KEY_LEFT:		//mover uma posiçao para a esquerda
				if(ativo[k].emLinha==0 || ativo[k].emLinha==1)	
					ativo[k].posx = (ativo[k].posx>0)?ativo[k].posx-1:ativo[k].posx;
				break;
			case KEY_RIGHT:		//mover uma posiçao para a direita
				if(ativo[k].emLinha==0 || ativo[k].emLinha==1)
					ativo[k].posx = (ativo[k].posx<(edit.colunas-1))?ativo[k].posx+1:ativo[k].posx;
				break;
			case KEY_DC:
			case KEY_BACKSPACE:		//remover uma letra
				if(ativo[k].emLinha==0){
					break;
				}
				if(ativo[k].emLinha==1){
					pthread_mutex_lock(&trinco);

					ativo[k].linha[ativo[k].posx+1]=' ';
					ativo[k].posx = (ativo[k].posx>0)?ativo[k].posx-1:ativo[k].posx;
					for(i=ativo[k].posx;i<edit.colunas;i++){
						ativo[k].linha[i]=ativo[k].linha[i+1];
						ativo[k].linha[edit.colunas]=' ';
					}
					for(i=0; i<edit.colunas; i++)								//alteraçao da linha mesmo que nao haja alteraçao
						edit.texto[ativo[k].posy][i] = ativo[k].linha[i];

					w=write(fd_cli, &edit,sizeof(medit));

					for(i=0; ativo[i].pid != 0 && i < MAX_LIMITE_USERS; i++){
						if(ativo[i].pid != ativo[k].pid){
							kill(ativo[i].pid, SIGUSR1);
							fd_tds = open(ativo[i].pipe, O_WRONLY);
							w=write(fd_tds, &edit,sizeof(medit));
							close(fd_tds);
						}
					}
					w = write(fd_cli, &ativo[k], sizeof(serv));
					pthread_mutex_unlock(&trinco);
					break;
				}
			case 10:	//ENTER
				if(ativo[k].emLinha==0){	
					pthread_mutex_lock(&trinco);
					ativo[k].emLinha=1;
					ativo[k].linhas_editadas++;
					total_editadas++;
					for(i=0;i<MAX_LIMITE_USERS;i++)
						if(i!=k && ativo[i].posy==ativo[k].posy && ativo[i].emLinha==1){
							ativo[k].emLinha=0;				
					}
	
					if(ativo[k].emLinha==1){
						for(i=0; i<edit.colunas; i++){		//linha de buffer caso o utilizador decida descartar as alteraçoes
							ativo[k].buffer[i] = edit.texto[ativo[k].posy][i];
							ativo[k].linha[i] = edit.texto[ativo[k].posy][i];
						}
						ativo[k].posx=0;

						strcpy(edit.nome[ativo[k].posy],ativo[k].username);

						
					}

					w = write(fd_cli, &ativo[k], sizeof(serv));
					if(ativo[k].emLinha==1){					
						w=write(fd_cli,&edit,sizeof(medit));
						//printf("w:%d\nnome:%s",w,edit.nome[ativo[k].posy]);
					
						for(i=0; ativo[i].pid != 0 && i < MAX_LIMITE_USERS; i++){
							if(ativo[i].pid != ativo[k].pid){
								kill(ativo[i].pid, SIGUSR1);
								fd_tds = open(ativo[i].pipe, O_WRONLY);
								w=write(fd_tds, &edit,sizeof(medit));
								close(fd_tds);
							}
						}

					}
					pthread_mutex_unlock(&trinco);
					break;
				}

				if(ativo[k].emLinha==1){

					pthread_mutex_lock(&trinco);
					//CONTAGEM DE NR DE PALAVRAS NA LINHA: INICIO
					conta=0;
					i=0;			
					while(i<edit.colunas){		
						for(; ativo[k].linha[i]==' ' && i<edit.colunas; i++);
						for(; ativo[k].linha[i]!=' ' && i<edit.colunas; i++);
						conta++;	
					}
					conta--;	//ajuste do K para representar correctamente o nr de palavras
					//CONTAGEM DE NR DE PALAVRAS NA LINHA: FIM

					comercial=0;

					if(conta > 0){
					//CHAMADA AO ASPELL: INICIO
						for(i=0; i<edit.colunas; i++){
							w = write(anonW[1], &ativo[k].linha[i], sizeof(char));
							//printf("%c", linha[i]);
						}
						w = write(anonW[1], &enter, sizeof(enter));
						//printf("\n%d\n",w);
						comercial=0;
						ativo[k].erro=0;
						
						while(conta > 0){
							r = read(anonR[0], &temp, sizeof(char));
							if(temp == '&')
								comercial=1;

							do{
								r = read(anonR[0], &temp, sizeof(char));
								
								if(temp == '&')
									comercial = 1;

							}while(temp != '\n');
											
							conta--;										
						}
						
						//CHAMADA AO ASPELL: FIM
					}	
					if(comercial == 0){
						for(i=0; i<edit.colunas; i++)								//alteraçao da linha mesmo que nao haja alteraçao
							edit.texto[ativo[k].posy][i] = ativo[k].linha[i];
						
						strcpy(edit.nome[ativo[k].posy],"        ");
		

						w = write(fd_cli, &edit, sizeof(medit));

						for(i=0; ativo[i].pid != 0 && i < MAX_LIMITE_USERS; i++){
							if(ativo[i].pid != ativo[k].pid){
								kill(ativo[i].pid, SIGUSR1);
								fd_tds = open(ativo[i].pipe, O_WRONLY);
								w=write(fd_tds, &edit,sizeof(medit));
								close(fd_tds);
							}
						}
						ativo[k].posx=0;
						ativo[k].emLinha=0;
						ativo[k].erro=0;

					w = write(fd_cli, &ativo[k], sizeof(serv));
					pthread_mutex_unlock(&trinco);	
						break;
					}

					if(comercial == 1){
						ativo[k].erro = 1;
						ativo[k].emLinha = 1;
					}		
					
					for(i=0; i<edit.colunas; i++)								//alteraçao da linha mesmo que nao haja alteraçao
						edit.texto[ativo[k].posy][i] = ativo[k].linha[i];					

					w = write(fd_cli, &edit, sizeof(medit));
					
					for(i=0; ativo[i].pid != 0 && i < MAX_LIMITE_USERS; i++){
						if(ativo[i].pid != ativo[k].pid){
							kill(ativo[i].pid, SIGUSR1);
							fd_tds = open(ativo[i].pipe, O_WRONLY);
							w=write(fd_tds, &edit,sizeof(medit));
							close(fd_tds);
						}
					}
					w = write(fd_cli, &ativo[k], sizeof(serv));

					if(ativo[k].erro==1)
						w = write(fd_cli, &ativo[k], sizeof(serv));

					pthread_mutex_unlock(&trinco);
					break;
				}
			case 27:	//ESC		cancelar alteraçoes da linha
				if(ativo[k].emLinha==0){
					w = write(fd_cli, &ativo[k], sizeof(serv));
					break;
				}
				if(ativo[k].emLinha==1){
					pthread_mutex_lock(&trinco);

					for(i=0; i<edit.colunas; i++)								//alteraçao da linha mesmo que nao haja alteraçao
							edit.texto[ativo[k].posy][i] = ativo[k].buffer[i];

					strcpy(edit.nome[ativo[k].posy],"        ");

					w = write(fd_cli, &edit, sizeof(medit));
					
					for(i=0; ativo[i].pid != 0 && i < MAX_LIMITE_USERS; i++){
						if(ativo[i].pid != ativo[k].pid){
							kill(ativo[i].pid, SIGUSR1);
							fd_tds = open(ativo[i].pipe, O_WRONLY);
							w=write(fd_tds, &edit,sizeof(medit));
							close(fd_tds);
						}
					}
					ativo[k].emLinha=0;
					w = write(fd_cli, &ativo[k], sizeof(serv));
					pthread_mutex_unlock(&trinco);
					break;
				}											
			default:
				if(ativo[k].emLinha==0){
					w = write(fd_cli, &ativo[k], sizeof(serv));
					break;
				}
				if(ativo[k].emLinha==1){
					pthread_mutex_lock(&trinco);
					if(ativo[k].linha[ativo[k].posx] == ' '){			//alteraçao do espaço para uma letra caso seja um espaço vazio
						ativo[k].linha[ativo[k].posx] = ativo[k].tecla;
						ativo[k].posx = (ativo[k].posx<(edit.colunas-1))?ativo[k].posx+1:ativo[k].posx;

						if(ativo[k].posx==edit.colunas-1){
							ativo[k].linha[ativo[k].posx] = ativo[k].tecla;
						}
					}
					if(ativo[k].linha[ativo[k].posx] != ' '){			//alteraçao do espaço para uma letra e desvio das restantes para a frente
						for(i=ativo[k].posx; ativo[k].linha[i] != ' ' && i < (edit.colunas-1); i++);
						if(i < (edit.colunas-1)){
							for(; i>ativo[k].posx; i--){
								ativo[k].linha[i] = ativo[k].linha[i-1];
							}
							ativo[k].linha[ativo[k].posx] = ativo[k].tecla;
							ativo[k].posx = (ativo[k].posx<(edit.colunas-1))?ativo[k].posx+1:ativo[k].posx;
						}
					}
					
					for(i=0;i<edit.colunas;i++)
						edit.texto[ativo[k].posy][i]=ativo[k].linha[i];

					w=write(fd_cli, &edit, sizeof(medit));
					
					for(i=0; ativo[i].pid != 0 && i < MAX_LIMITE_USERS; i++){
						if(ativo[i].pid != ativo[k].pid){
							kill(ativo[i].pid, SIGUSR1);
							fd_tds = open(ativo[i].pipe, O_WRONLY);
							w=write(fd_tds, &edit,sizeof(medit));
							close(fd_tds);
						}
					}
					w = write(fd_cli, &ativo[k], sizeof(serv));
					pthread_mutex_unlock(&trinco);
					break;
				}
		}
		if(ativo[k].tecla == KEY_UP || ativo[k].tecla == KEY_DOWN || ativo[k].tecla == KEY_LEFT || ativo[k].tecla == KEY_RIGHT){
			w = write(fd_cli, &ativo[k], sizeof(serv));	
		}
		
		close(fd_cli);

	}while(dados.sair==0);

	close(fd_exe);
	unlink(dados.pipes[nr]);
	pthread_exit(0);
}



//////////////////////////////////
//THREAD PARA COMANDO STATISTICS//
//////////////////////////////////
void* statistics(void* data){
	do{
		settings(edit.linhas, edit.colunas, filename, nrusers, pipename, npipes);
		users(ativo, total_editadas);
		printf("comando: ");
		fflush(stdout);
		sleep(1);
	}while(flag==0);

	pthread_exit(0);
}



/////////////////////////////////
//VALIDAÇÃO DO NOME DE USERNAME//
/////////////////////////////////
int valida(char* nome, char* filename){
	FILE* f;
	char buffer[TAM];

	if(filename[0]=='\0')
		strcpy(filename, DATABASE);

	f=fopen(filename, "rt");
	if(!f){
		fprintf(stderr, "Erro ao abrir o ficheiro %s\n", filename);
		return 1;
	}
	
	while(fscanf(f, " %s\n", buffer)==1){
		if(strcmp(nome, buffer)==0){
			fclose(f);
			return 0;
		}
	}
	fclose(f);
	return 1;
}