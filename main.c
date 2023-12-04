#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include "MPI_memoria_distribuida_lib/MPI_memoria_distribuida.h"

int main (int argc, char **argv) {
	int output_gen;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &output_gen);
	MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	aloca(24);

	if(rank == 1){
		char buffer_input[15] = "HelloWorldLegal";
		escreve(buffer_input, 15, 5);
	}

	if(rank == 2){
		//tem que sincronizar ainda infelizmente
		sleep(3);
		char buffer_output[15];
		le(buffer_output, 15, 5);
		for(int i = 0; i<15; i++){
			printf("Na posicao %d leu %c\n", i, buffer_output[i]);
		}
	}
	
	/*
	if(rank == 2){
		char buffer_input[4] = "Worl";
		escreve(buffer_input, 4, 3);
	}

	if(rank == 0){
		char buffer_input[5] = "Hello";
		escreve(buffer_input, 5, 10);
	}

	sleep(3);
	for (char i = 0; i < bytes_por_segmento; i++){
		printf("processo de rank %d: posicao[%d] tem %d\n", rank, i+(bytes_por_segmento*rank), segmento_memoria[i]);
	}
	*/

	sleep(4);
	//cuidado, só usar depois de terminar todas as operações
	finalizar_mem_distribuida();
	MPI_Finalize();
	
	return 0;
}

