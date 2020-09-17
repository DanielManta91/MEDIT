#ifndef COMANDOS_H
#define COMANDOS_H

#include "servidor-defaults.h"

void settings(int lin,int col,char *filename,int maxuser, char *mainpipe, int nrpipes);

void load(char *filename);

void save(char *filename);

void users(serv *ativo, int total);

void text();

#endif
