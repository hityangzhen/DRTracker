#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>

#define MAX_BUFFER 10    /*define the number of buffer*/
#define RUN_TIME 20

//We only use a buff as the pool
class ProducerConsumer {
public:

	ProducerConsumer() {
		//initialize semaphore
		sem_init(&p_sem_,0,MAX_BUFFER);
		sem_init(&c_sem_,0,0);
		sem_init(&buff_mutex_,0,1);
		p_index_=c_index_=0;
		pc=this;
	}
	~ProducerConsumer() {
		sem_destroy(&p_sem_);  
		sem_destroy(&c_sem_);
		sem_destroy(&buff_mutex_);
	}

	//execute automatically
	void Setup() {
		signal(SIGALRM,sig_handler);
		memset(buffer,0,sizeof(buffer));

		pthread_create(&p_id_,NULL,Produce,NULL);
		pthread_create(&c_id_,NULL,Consume,NULL);

		alarm(RUN_TIME);
		pthread_join(p_id_,NULL);
		pthread_join(c_id_,NULL);
	}
protected:

	static ProducerConsumer *pc;
	static void sig_handler(int ) {
		pthread_cancel(pc->p_id_);
		pthread_cancel(pc->c_id_);
	}

	static void *Produce(void *);

	static void *Consume(void *);

	static void showbuf();

	typedef void* (*HANDLER)(void *);
	sem_t p_sem_;
	sem_t c_sem_;
	sem_t buff_mutex_;

	int p_index_;
	int c_index_;

	pthread_t p_id_;
	pthread_t c_id_;

	HANDLER p_handler_;
	HANDLER c_handler_;

	int buffer[MAX_BUFFER];
};

ProducerConsumer *ProducerConsumer::pc=NULL;

void *ProducerConsumer::Consume(void *)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);

	while(1) {
		sem_wait(&pc->c_sem_);
		srand(time(NULL));
		sleep(rand()%2);
		//block
		while((pc->c_index_+1)%MAX_BUFFER==pc->p_index_) ;
		sem_wait(&pc->buff_mutex_);
		printf("Consume:%2d\n",pc->buffer[pc->c_index_]);
		pc->c_index_=(pc->c_index_+1)%MAX_BUFFER;
		showbuf();
		sem_post(&pc->buff_mutex_);
		//nodify the producer
		sem_post(&pc->p_sem_);
		srand(time(NULL));
		sleep(rand()%2+1);	
	}
	return NULL;
}

void *ProducerConsumer::Produce(void *)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);

	while(1) {
		sem_wait(&pc->p_sem_);
		srand(time(NULL));
		sleep(rand()%2);
		//block
		while((pc->p_index_+1)%MAX_BUFFER==pc->c_index_) ;

		sem_wait(&pc->buff_mutex_);
		int tmp=rand()%MAX_BUFFER+1;
		pc->buffer[pc->p_index_]=tmp;
		printf("Produce:%2d\n",tmp);
		pc->p_index_=(pc->p_index_+1)%MAX_BUFFER;
		showbuf();
		sem_post(&pc->buff_mutex_);
		//nodify the consumer
		sem_post(&pc->c_sem_);
		srand(time(NULL));
		sleep(rand()%2+1);
	}

	return NULL;
}

void ProducerConsumer::showbuf() {
	printf("buffer:");
	for(int i=0;i<MAX_BUFFER;i++) {
		printf("%2d ",pc->buffer[i]);
	}
	printf("\n");
}

int main(){
	ProducerConsumer pc;
	pc.Setup();
	return 0;
}
