#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;

int global=0;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void *thread1(void *tmp)
{
	global=1;
	for(int i=0;i<1000000;i++) ;
	*(int *)tmp += 1;
	return NULL;
}
void *thread2(void *tmp)
{
	
	*(int *)tmp += 2;
	for(int i=0;i<1000000;i++) ;
	global=2;
	return NULL;
}

void threadCretator()
{
	int *tmp=new int(4);
	pthread_t id1,id2;
	*tmp=2;
	pthread_create(&id1,NULL,thread1,tmp);
	*tmp=3;
	pthread_create(&id2,NULL,thread2,tmp);
	for(int i=0;i<1000000;i++) ;
	pthread_join(id2,NULL);
	pthread_join(id1,NULL);
	delete tmp;
}

int main()
{
	threadCretator();
	return 0;
}