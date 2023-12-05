#include "MPI_memoria_distribuida.h"

char *segmento_memoria;
int rank;
int tamanho_global;
int num_procs;
int bytes_por_segmento;
pthread_t thread_leitura, thread_escrita, thread_no_central;
pthread_mutex_t mutex_escritor_leitor = PTHREAD_MUTEX_INITIALIZER;

void escreve(char* buffer, int tamanho, int posicao){
	if ((posicao + tamanho) > (tamanho_global)){
		raise(SIGSEGV);
	}

	int destination_rank, restante_do_segmento;
	int ponteiro_buffer = 0;
	int iterador_posicao = posicao;

	//Requisicao de acesso a mem distribuida. Arquitetura estrela
	int requisicao = 0;
	MPI_Send(&requisicao, 1, MPI_INT, 0, 3, MPI_COMM_WORLD);
	MPI_Recv(&requisicao, 1, MPI_INT, 0, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	int confirmador_de_escritas;
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
		MPI_Recv(&requisicao, 1, MPI_INT, destination_rank, 6, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	//Esperar confirmação de todas as requisições
	MPI_Send(&requisicao, 1, MPI_INT, 0, 5, MPI_COMM_WORLD);
}

void le(char* buffer, int tamanho, int posicao){
	if ((tamanho + posicao) > (tamanho_global)){
		raise(SIGSEGV);
	}

	int destination_rank, restante_do_segmento;
	int ponteiro_buffer = 0;
	int iterador_posicao = posicao;
	int vetor_de_argumentos[2];

	int requisicao = 0;
	MPI_Send(&requisicao, 1, MPI_INT, 0, 3, MPI_COMM_WORLD);
	MPI_Recv(&requisicao, 1, MPI_INT, 0, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

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
	//leitura já espera confirmação pelo receive
	MPI_Send(&requisicao, 1, MPI_INT, 0, 5, MPI_COMM_WORLD);
}

void* escutando_escrita(void* arg){
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	int confirmacao_operacao = 1;
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

		//necessário apenas para a intercalação das thread escrita e leitura
		pthread_mutex_lock(&mutex_escritor_leitor);
		for(int i = start_position; i < end_position; i++){
			segmento_memoria[i] = (char) valores_recebidos[pivot];
			pivot++;
		}
		pthread_mutex_unlock(&mutex_escritor_leitor);
		MPI_Send(&confirmacao_operacao, 1, MPI_INT, message_source, 6, MPI_COMM_WORLD);
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

		pthread_mutex_lock(&mutex_escritor_leitor);
		for (int i = start_position; i < end_position; i++){
			vetor_resposta[pivot] = segmento_memoria[i];
			pivot++;
		}
		pthread_mutex_unlock(&mutex_escritor_leitor);

		MPI_Send(vetor_resposta, valores_recebidos[0], MPI_INT, message_source, 2, MPI_COMM_WORLD);
	}
}

void* no_central(void* arg){
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	int var = 1;
	int requisicao, message_source;
	MPI_Status status;
	while(1){
		MPI_Recv(&requisicao, 1, MPI_INT, MPI_ANY_SOURCE, 3, MPI_COMM_WORLD, &status);
		message_source = status.MPI_SOURCE;
		printf("primeira requisicao recebida de %d\n", message_source);
		MPI_Send(&var, 1, MPI_INT, message_source, 4, MPI_COMM_WORLD);
		MPI_Recv(&requisicao, 1, MPI_INT, message_source, 5, MPI_COMM_WORLD, &status);
		message_source = status.MPI_SOURCE;
		printf("finalizacao de requisicao recebida de %d\n", message_source);
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
		segmento_memoria[i%bytes_por_segmento] = 0;
	}

	pthread_create(&thread_leitura, NULL, escutando_leitura, NULL);
	pthread_create(&thread_escrita, NULL, escutando_escrita, NULL);
	//thread do coordenador de mutex
	if(rank == 0){
		pthread_create(&thread_no_central, NULL, no_central, NULL);
	}
	return 0;
}

void finalizar_mem_distribuida(){
	free(segmento_memoria);
	pthread_cancel(thread_leitura);
	pthread_cancel(thread_escrita);
	//pthread_join(thread_leitura, NULL);
	//pthread_join(thread_escrita, NULL);
}

