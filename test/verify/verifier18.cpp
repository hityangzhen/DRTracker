#include <iostream>
#include <pthread.h> 
#include <unistd.h>
/* Null pointer deference memory bug */
int *tmp;

void *thread1(void *)
{
	tmp=NULL;
	return NULL;
}

void *thread2(void *)
{
	sleep(1);
	std::cout<<*tmp<<std::endl;
	return NULL;
}

void threadCretator()
{
	int res=18;
	tmp=&res;
	pthread_t id1,id2;
	pthread_create(&id1,NULL,thread1,NULL);
	pthread_create(&id2,NULL,thread2,NULL);
	pthread_join(id1,NULL);
	pthread_join(id2,NULL);
}

int main()
{
	threadCretator();
	return 0;
}