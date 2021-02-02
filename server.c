#include <getopt.h>
#include <limits.h>
#include <sys/wait.h>

#include <pthread.h>
#include <syslog.h>
#include <fcntl.h>

#include "common.h"
#include "queue.h"

void atender_cliente(int connfd);

#define SIZE_F 4 /* array size bandas*/
#define SIZE_C 5 /* array size dispensadores*/
#define NUM_INGREDIENTES 6

static int matrix[SIZE_F][SIZE_C]; //cada columna va corresponder a un ingrediente
static int statusOrder[SIZE_F];	   //0 libre, 1 sirviendo orden, 2 mantenimiento
static int numOrders[SIZE_F];
static Queue queue_order;
pthread_mutex_t queueMUtex, matrixMutex, numOrderMutex, statusOrderMutex;



void gotoxy(int x, int y)
{
	printf("\033[%d;%df", y, x);
}

void fill_queue(int matrix[SIZE_F][SIZE_C], int status[SIZE_F])
{
	int i;

	for (i = 0; i < SIZE_F; i++)
	{
		matrix[i][0] = NUM_INGREDIENTES; // Pan
		matrix[i][1] = NUM_INGREDIENTES; // Hamburguesa
		matrix[i][2] = NUM_INGREDIENTES; // Lechuga
		matrix[i][3] = NUM_INGREDIENTES; // Tomate
		matrix[i][4] = NUM_INGREDIENTES; // Queso
	}

	for (i = 0; i < SIZE_F; i++)
	{
		statusOrder[i] = 0;
		numOrders[i] = 0;
	}
}

void print_queue(int matrix[SIZE_F][SIZE_C])
{
	int i, j;
	int first;

	printf("\n\t Matrix[4_BANDAS][6_INGREDIENTES]\n");
	for (i = 0; i < SIZE_F; i++)
	{
		printf("\t\t[");
		first = 1;
		for (j = 0; j < SIZE_C; j++)
		{
			if (!first)
			{
				printf(",");
			}
			printf("%x", matrix[i][j]);

			first = 0;
		}
		printf("] \t %d hamburguesa(s) despachada(s)\n", numOrders[i]);
	}
}

void print_help(char *command)
{
	printf("Servidor para la ejecución remota de ordenes.\n");
	printf("uso:\n %s [-d] <puerto>\n", command);
	printf(" %s -h\n", command);
	printf("Opciones:\n");
	printf(" -h\t\t\tAyuda, muestra este mensaje\n");
}

/**
 * Recibe SIGINT, termina ejecución
 */
void salir(int signal)
{
	printf("BYE\n");
	exit(0);
}

void *thread(void *vargp);
void print_ingrediente(int index, int cantidad);
void depachar_orden(char buf[MAXLINE]);
void atender_ordenes_queue();

bool jflag = false; //Opción -j, Número de worker threads
sbuf_t sbuf;

int main(int argc, char **argv)
{
	create_queue(&queue_order); // Cola para las ordenes
	int opt, index;

	//Sockets
	int listenfd;
	unsigned int clientlen;
	int nWorkers = 10;  // Numero de clientes concurrentes a atender
	//Direcciones y puertos
	struct sockaddr_in clientaddr;
	char *port;

	while ((opt = getopt(argc, argv, "hj")) != -1)
	{
		switch (opt)
		{
		case 'h':
			print_help(argv[0]);
			return 0;
		case 'j':
			jflag = true;
			break;
		default:
			fprintf(stderr, "uso: %s [-d] <puerto>\n", argv[0]);
			fprintf(stderr, "	 %s -h\n", argv[0]);
			return 1;
		}
	}

	/* Recorre argumentos que no son opción */
	for (index = optind; index < argc; index++)
		port = argv[index];

	if (argv == NULL)
	{
		fprintf(stderr, "uso: %s [-d] <puerto>\n", argv[0]);
		fprintf(stderr, "	 %s -h\n", argv[0]);
		return 1;
	}

	//Valida el puerto
	int port_n = atoi(port);
	if (port_n <= 0 || port_n > USHRT_MAX)
	{
		fprintf(stderr, "Puerto: %s invalido. Ingrese un número entre 1 y %d.\n", port, USHRT_MAX);
		return 1;
	}

	//Registra funcion para señal SIGINT (Ctrl-C)
	signal(SIGINT, salir);

	//Abre un socket de escucha en port
	listenfd = open_listenfd(port);
	if (listenfd < 0)
		connection_error(listenfd);
	printf("server escuchando en puerto %s...\n", port);

	if (jflag)
		nWorkers = atoi(argv[index - 2]);

	int connfd_ptr;
	pthread_t tid;
	sbuf_init(&sbuf, nWorkers + 1);
	fill_queue(matrix, statusOrder);

	pthread_mutex_init(&queueMUtex, NULL);
	pthread_mutex_init(&matrixMutex, NULL);
	pthread_mutex_init(&statusOrderMutex, NULL);

	for (int i = 0; i < nWorkers; i++)
		pthread_create(&tid, NULL, thread, NULL);

	while (1)
	{
		clientlen = sizeof(clientaddr);
		connfd_ptr = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
		sbuf_insert(&sbuf, connfd_ptr);
	}
}

/*
 * thread - En un nuevo hilo atiende la orden del cliente. 
 */
void *thread(void *vargp)
{
	pthread_detach(pthread_self());
	while (1)
	{
		int connfd = sbuf_remove(&sbuf);
		atender_ordenes_queue();
		atender_cliente(connfd);
		close(connfd);
	}
}

void atender_ordenes_queue() {
	pthread_mutex_lock( &queueMUtex );
	if (!isEmpty_queue(queue_order)) { // Si hay ordenes en la cola de espera
		for(int i=0; i<size_queue(queue_order); i++) {
			char *nbuf = dequeue(&queue_order);
			char *pacola = (char *)malloc(MAXLINE);

			enqueue(&queue_order, strcpy(pacola, nbuf));
			depachar_orden(nbuf);
		}
		printf("Cola de ordenes actualizada: \t");
		show_queue(queue_order);
	}
	pthread_mutex_unlock( &queueMUtex );
}

void atender_cliente(int connfd)
{
	int n;
	char buf[MAXLINE] = {0};

	//Comunicación con la orden del cliente
	while (1)
	{

		n = read(connfd, buf, MAXLINE);
		if (n <= 0)
			return;

		buf[n - 1] = '\0'; //Remueve el salto de linea

		char *pacola = (char *)malloc(sizeof(buf));
		pthread_mutex_lock( &queueMUtex );
		enqueue(&queue_order, strcpy(pacola, buf));
		pthread_mutex_unlock( &queueMUtex );

		depachar_orden(buf);

		write(connfd, "Gracias por ordenar\n", 20);
		return;
	}
}


void depachar_orden(char buf[MAXLINE])
{
	printf("\nOrden recibida: %s \n", buf);
	int iBanda, jBanda;
	iBanda = 0;
	jBanda = 0;
	char *token = strtok(buf, ",");

	while (iBanda < SIZE_F)
	{
		if (statusOrder[iBanda] == 1 || statusOrder[iBanda] == 2)
		{
			iBanda++;
			continue;
		}
		// Banda libre pasa a comprobar si tiene los ingredientes necesarios
		int ingredientesCompletos = 1; // 1 si estan completos, 0 si estan incompletos
		int ingredientes[SIZE_C];
		int atLeastOneIgrediente = 0; // 0 si toda los ingredientes son cero, 1 si al menos uno es mayor que cero
		int item;
		if (token != NULL)
		{
			while (token != NULL)
			{
				item = atoi(token);
				if (item <= matrix[iBanda][jBanda])
				{
					if(item > 0 ) atLeastOneIgrediente = 1;

					ingredientes[jBanda] = item;
					token = strtok(NULL, ",");
					jBanda++;
					continue;
				}
				else
				{
					ingredientesCompletos = 0;
					break;
				}
			}
		}
		else{
			printf("Orden en formato invalido, formato aceptado num,num,num,num,num \n");
			break;
		}
		if(atLeastOneIgrediente == 0) {
			printf("Orden vacía no valida.\n");
			break;
		}
		if (ingredientesCompletos == 0)
		{
			printf("Banda %d no tiene los ingredientes necesarios \n", iBanda);
			iBanda++;
		}

		if (statusOrder[iBanda] == 0 && ingredientesCompletos)
		{
			printf("Banda %d antendio su orden \n", iBanda);
			pthread_mutex_unlock(&statusOrderMutex);
			statusOrder[iBanda] = 1; // Banda pasa a estado de ejecución
			pthread_mutex_unlock( &statusOrderMutex );

			jBanda = 0;
			
			pthread_mutex_lock( &matrixMutex );
			while (jBanda < SIZE_C)
			{
				int value = matrix[iBanda][jBanda] - ingredientes[jBanda];
				matrix[iBanda][jBanda] = (value< 0 || value > NUM_INGREDIENTES)? 0: value;
				print_ingrediente(jBanda, ingredientes[jBanda]);

				// if (matrix[iBanda][jBanda] == 0)
				// {
				// 	printf("El ingrediente %d en la banda %d quedó vacío\n", jBanda, iBanda);
				// }
				jBanda++;
			}
			
			numOrders[iBanda] = numOrders[iBanda] + 1; // Sumo el número de hamburguesas
			statusOrder[iBanda] = 0; // Banda pasa a estado libre
			pthread_mutex_unlock( &matrixMutex );			   
			iBanda++;
			
			pthread_mutex_lock( &queueMUtex );
			lastqueue(&queue_order);
			pthread_mutex_unlock( &queueMUtex );

			break;
		}
		else if (iBanda == SIZE_F)
		{
			printf("Orden en cola de espera, puesto que ninguna banda puede despacharla \n");
		}
	}
	memset(buf, 0, MAXLINE); //Encera el buffer
	print_queue(matrix);

}

void print_ingrediente(int index, int cantidad)
{
	if (cantidad > 0)
	{
		switch (index)
		{
		case 0:
			printf("%d pan(es) añadido(s)\n", cantidad);
			break;
		case 1:
			printf("%d carne(s) añadida(s)\n", cantidad);
			break;
		case 2:
			printf("%d lechuga(s) añadida(s)\n", cantidad);
			break;
		case 3:
			printf("%d tomate(s) añadido(s)\n", cantidad);
			break;
		case 4:
			printf("%d queso(s) añadido(s)\n", cantidad);
			break;

		default:
			break;
		}
	}
}