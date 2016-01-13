#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;
//test for invalid spinning read loop 
int global=0;
int N=10;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void *thread1(void *tmp)
{
	global=1;
	for(int i=0;i<N;i++)
		*(int *)tmp += 1;
	return NULL;
}
void *thread2(void *tmp)
{
	int j=*(int *)tmp;
	for(int i=0;i<N;i++)
		j+=2;
	global=2;
	return NULL;
}

void threadCretator()
{
	int *tmp=new int(4);
	*tmp=0;
	pthread_t id1,id2;
	pthread_create(&id1,NULL,thread1,tmp);
	pthread_create(&id2,NULL,thread2,tmp);
	for(int i=0;i<N;i++) ;
	pthread_join(id2,NULL);
	pthread_join(id1,NULL);
	delete tmp;
}

int main()
{
	threadCretator();
	return 0;
}