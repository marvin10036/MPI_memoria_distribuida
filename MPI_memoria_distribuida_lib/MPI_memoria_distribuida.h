#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <signal.h>

extern char *segmento_memoria;
extern int rank;
extern int tamanho_global;
extern int num_procs;
extern int bytes_por_segmento;
extern pthread_t thread_leitura, thread_escrita, thread_no_central;
extern pthread_mutex_t mutex_escritor_leitor;

void escreve(char* buffer, int tamanho, int posicao);

void le(char* buffer, int tamanho, int posicao);

void* escutando_escrita(void* arg);
void* escutando_leitura(void* arg);
void* no_central(void* arg);

int aloca(int tamanho);

void finalizar_mem_distribuida();

