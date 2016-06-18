#include <iostream>
#include <pthread.h> 
#include <unistd.h>
/* Null pointer deference memory bug */
int *tmp;

void *func1()
{
	tmp=NULL;
	return NULL;
}

void *func2()
{
	sleep(1);
	std::cout<<*tmp<<std::endl;
	return NULL;
}

void *thread1(void *) { return func1(); }

void *thread2(void *) { return func2(); }

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