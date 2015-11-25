#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void *thread1(void *tmp)
{
	for(int i=0;i<1000000;i++) ;
	*(int *)tmp += 1;
	return NULL;
}
void *thread2(void *tmp)
{
	
	*(int *)tmp += 2;
	for(int i=0;i<1000000;i++) ;
	return NULL;
}

void threadCretator()
{
	int *tmp=new int(4);

	pthread_t id1,id2;
	pthread_create(&id1,NULL,thread1,tmp);
	pthread_create(&id2,NULL,thread2,tmp);
	for(int i=0;i<1000000;i++) ;
	pthread_join(id1,NULL);
	pthread_join(id2,NULL);
	delete tmp;
}

int main()
{
	threadCretator();
	return 0;
}