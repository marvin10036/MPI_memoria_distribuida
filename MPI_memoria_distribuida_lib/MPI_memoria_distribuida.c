#include "MPI_memoria_distribuida.h"

char *segmento_memoria;
int rank;
int tamanho_global;
int num_procs;
int bytes_por_segmento;
pthread_t thread_leitura, thread_escrita;

void escreve(char* buffer, int tamanho, int posicao){
	if ((posicao + tamanho) > (tamanho_global)){
		raise(SIGSEGV);
	}

	int destination_rank, restante_do_segmento;
	int ponteiro_buffer = 0;
	int iterador_posicao = posicao;

	while(ponteiro_buffer <= tamanho - 1){
		destination_rank = (iterador_posicao/bytes_por_segmento);

		restante_do_segmento = bytes_por_segmento - (iterador_posicao % bytes_por_segmento);
		//Se esse for o último segmento a ser alterado
		if(tamanho - ponteiro_buffer < restante_do_segmento){
			restante_do_segmento = tamanho - ponteiro_buffer;
		}

		int vetor_de_argumentos[restante_do_segmento+2];
		vetor_de_argumentos[0] = restante_do_segmento;
		vetor_de_argumentos[1] = iterador_posicao;

		for(int i = 0; i < restante_do_segmento; i++){
			vetor_de_argumentos[i+2] = (int) buffer[ponteiro_buffer];
			ponteiro_buffer++;
			iterador_posicao++;
		}
		MPI_Send(vetor_de_argumentos, restante_do_segmento+2, MPI_INT, destination_rank, 1, MPI_COMM_WORLD);
	}
}

void le(char* buffer, int tamanho, int posicao){
	if ((tamanho + posicao) > (tamanho_global)){
		raise(SIGSEGV);
	}

	int destination_rank, restante_do_segmento;
	int ponteiro_buffer = 0;
	int iterador_posicao = posicao;
	int vetor_de_argumentos[2];

	while(ponteiro_buffer <= tamanho - 1){
		destination_rank = (iterador_posicao/bytes_por_segmento);

		restante_do_segmento = bytes_por_segmento - (iterador_posicao % bytes_por_segmento);
		//Se esse for o último segmento a ser lido
		if(tamanho - ponteiro_buffer < restante_do_segmento){
			restante_do_segmento = tamanho - ponteiro_buffer;
		}

		vetor_de_argumentos[0] = restante_do_segmento;
		vetor_de_argumentos[1] = iterador_posicao;

		int valores_lidos[restante_do_segmento];

		MPI_Send(vetor_de_argumentos, 2, MPI_INT, destination_rank, 0, MPI_COMM_WORLD);
		MPI_Recv(valores_lidos, tamanho, MPI_INT, destination_rank, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		for (int i = 0; i < restante_do_segmento; i++){
			buffer[ponteiro_buffer] = (char) valores_lidos[i];
			ponteiro_buffer++;
			iterador_posicao++;
		}
	}
}

void* escutando_escrita(void* arg){
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	while(1){
		MPI_Status status;
		int tamanho_da_mensagem;

		MPI_Probe(MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
		MPI_Get_count(&status, MPI_INT, &tamanho_da_mensagem);
		int valores_recebidos[tamanho_da_mensagem];

		//garantir que o mesmo processo que ele recebeu no probe eh o mesmo do receive
		int message_source = status.MPI_SOURCE;

		MPI_Recv(&valores_recebidos, tamanho_da_mensagem, MPI_INT, message_source, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		int start_position, end_position, pivot;
		pivot = 2;
		start_position = valores_recebidos[1] % bytes_por_segmento;
		end_position = start_position + valores_recebidos[0];

		for(int i = start_position; i < end_position; i++){
			segmento_memoria[i] = (char) valores_recebidos[pivot];
			pivot++;
		}
	}
}

void* escutando_leitura(void* arg){
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	while(1){
		MPI_Status status;
		int tamanho_da_mensagem = 2;

		int valores_recebidos[tamanho_da_mensagem];

		MPI_Recv(&valores_recebidos, tamanho_da_mensagem, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);

		int message_source = status.MPI_SOURCE;
		int vetor_resposta[valores_recebidos[0]];

		int start_position, end_position, pivot;
		start_position = valores_recebidos[1] % bytes_por_segmento;
		end_position = start_position + valores_recebidos[0];
		pivot = 0;

		for (int i = start_position; i < end_position; i++){
			vetor_resposta[pivot] = segmento_memoria[i];
			pivot++;
		}

		MPI_Send(vetor_resposta, valores_recebidos[0], MPI_INT, message_source, 2, MPI_COMM_WORLD);
	}
}

int aloca(int tamanho) {
	tamanho_global = tamanho;

	bytes_por_segmento = tamanho / num_procs;
	//Em caso da divisao dos segmentos nao ser exata, cada um deles ganha um byte extra
	if(tamanho % num_procs != 0){
		bytes_por_segmento++;
	}

	segmento_memoria = (char *)malloc(bytes_por_segmento * sizeof(char));
	if (segmento_memoria == NULL) {
		printf("Falha na alocação de memória \n");
		exit(1);
  } else{
		printf("%d bytes de memória alocados no processo de rank %d\n", bytes_por_segmento, rank);
	}

	for (char i = (bytes_por_segmento * rank); i < (bytes_por_segmento * (rank+1)); i++){
		segmento_memoria[i%bytes_por_segmento] = i;
	}

	pthread_create(&thread_leitura, NULL, escutando_leitura, NULL);
	pthread_create(&thread_escrita, NULL, escutando_escrita, NULL);

	return 0;
}

void finalizar_mem_distribuida(){
	free(segmento_memoria);
	pthread_cancel(thread_leitura);
	pthread_cancel(thread_escrita);
	//pthread_join(thread_leitura, NULL);
	//pthread_join(thread_escrita, NULL);
}

