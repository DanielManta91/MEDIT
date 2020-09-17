#ifndef MEDIT_DEFAULTS_H
#define MEDIT_DEFAULTS_H

#define LIN 15
#define COL 45

typedef struct medit_data{
	char texto[LIN][COL];
	int linhas, colunas;
	char nome[LIN][9];
}medit;


#endif
