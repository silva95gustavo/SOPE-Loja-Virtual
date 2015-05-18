#include "balcao.h"

int ownIndex;
int opening_duration;

int main(int argc, char *argv[])
{
	if(argc != 3)
	{
		printf("\n\t%s: Invalid number of arguments.\n\tMust be: %s <shared_memory_name> <opening_time>\n\n", argv[0], argv[0]);
		return 1;
	}

	opening_duration = 0;
	if(parse_int(&opening_duration, argv[2], 10) != 0)
	{
		printf("\n\t%s - ERROR: Invalid opening time. Must be a number\n\n", argv[0]);
		return 1;
	}

	int shm_id;
	shop_t *shop;
	//shop = (shop_t *)create_shared_memory(argv[1], &shm_id, SHARED_MEM_SIZE);
	//if (shop == NULL) return 1;

	//balcao_t balcao = join_shmemory(shop);

	//if(balcao.num == -1) return 1;

	printf("timer : %d\n", opening_duration);

	pthread_t counterThread;
	pthread_create(&counterThread, NULL, timer_countdown, "");
	//pthread_join(counterThread, NULL);

	while(opening_duration > 0) // TODO Atender clientes
	{
		printf("working - %d\n", opening_duration);
	}

	return 0;
	//return terminate_balcao(argv[1], shop);
}

shop_t *create_shared_memory(const char *name, int *shm_id, long size)
{
	if((*shm_id = shm_open(name, O_RDWR, 0600)) < 0)
	{
		if((*shm_id = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600)) < 0)
		{
			printf("Error: couldn't create/open shared memory.\n");
			return NULL;
		}
		if (ftruncate(*shm_id, size) == -1)
		{
			printf("Error: couldn't allocate space in the shared memory.\n");
			return NULL;
		}

		time_t time_open = time(NULL);

		shop_t shop;
		shop.opening_time = time_open;
		pthread_mutex_t mt = PTHREAD_MUTEX_INITIALIZER;	// had to do this to solve compilation error
		shop.loja_mutex = mt;
		shop.num_balcoes = 0;
		//TODO tabela balcões

		shop_t *shmem;
		shmem = (shop_t *) mmap(0,size,PROT_READ|PROT_WRITE,MAP_SHARED,*shm_id,0);
		*shmem = shop;
		return shmem;
	}

	if (ftruncate(*shm_id, size) == -1)
	{
		printf("Error: couldn't allocate space in the shared memory.\n");
		return NULL;
	}

	shop_t *shmem;
	shmem = (shop_t *) mmap(0,size,PROT_READ|PROT_WRITE,MAP_SHARED,*shm_id,0);
	return shmem;
}

balcao_t join_shmemory(shop_t* shop)
{
	balcao_t thisBalcao; thisBalcao.num = -1;

	time_t curr_time = time(NULL);
	int pid = getpid();

	thisBalcao.abertura = curr_time;
	thisBalcao.duracao = (time_t)-1;
	sprintf(thisBalcao.fifo_name, "/tmp/fb_%d", pid);

	if(mkfifo(thisBalcao.fifo_name, BALCAO_FIFO_MODE) != 0)
	{
		printf("Error: unable to create FIFO %s\n", thisBalcao.fifo_name);
		return thisBalcao;
	}

	thisBalcao.clientes_em_atendimento = 0;
	thisBalcao.clientes_atendidos = 0;
	thisBalcao.atendimento_medio = 0;

	if(pthread_mutex_lock(&shop->loja_mutex) != 0)
	{
		printf("Error: unable to lock \"loja\" mutex.\n");
		return thisBalcao;
	}

	thisBalcao.num = shop->num_balcoes + 1;
	ownIndex = shop->num_balcoes;
	shop->balcoes[shop->num_balcoes] = thisBalcao;
	shop->num_balcoes++;

	pthread_mutex_unlock(&shop->loja_mutex);

	return thisBalcao;
}

int terminate_balcao(char* shmem, shop_t *shop)
{
	time_t end = time(NULL);

	shop->balcoes[ownIndex].duracao = end - shop->balcoes[ownIndex].abertura;

	int last = 0;
	int i = 0;
	for(; i < shop->num_balcoes; i++)
	{
		if(shop->balcoes[i].duracao != -1)
		{
			last = 1;
			break;
		}
	}

	if(last == 0)	// this is the last balcao active
	{
		pthread_mutex_lock(&shop->loja_mutex);
		pthread_mutex_unlock(&shop->loja_mutex);	// to ensure no process is using it
		pthread_mutex_destroy(&shop->loja_mutex);

		if(unlink(shop->balcoes[ownIndex].fifo_name) != 0)
		{
			printf("Error: unable to unlink fifo %s\n", shop->balcoes[ownIndex].fifo_name);
			return 1;
		}

		if (shm_unlink(shmem) == -1)
		{
			printf("Error: shared memory wasn't properly cleaned.\n");
			return 1;
		}
	}



	return 0;
}

void *timer_countdown(void *arg)
{
	while(opening_duration > 0)
	{
		sleep(1);
		opening_duration--;
	}

	return NULL;
}
