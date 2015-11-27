#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;

int tester = 0;
int LOOP_COUNT = 5;
pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m3 = PTHREAD_MUTEX_INITIALIZER;

void *thread1(void *tmp)
{
	pthread_mutex_lock(&m1);
	pthread_mutex_lock(&m2);
	*(int *)tmp = 1;
	for(int i=0;i<1000000;i++) ;
	pthread_mutex_unlock(&m2);
	pthread_mutex_unlock(&m1);
	return NULL;
}
void *thread2(void *tmp)
{
	pthread_mutex_lock(&m2);
	pthread_mutex_lock(&m3);
	for(int i=0;i<1000000;i++) ;
	*(int *)tmp = 2;
	pthread_mutex_unlock(&m3);
	pthread_mutex_unlock(&m2);
	return NULL;
}
void *thread3(void *tmp)
{
	pthread_mutex_lock(&m1);
	pthread_mutex_lock(&m3);
	*(int *)tmp = 3;
	for(int i=0;i<1000000;i++) ;
	pthread_mutex_unlock(&m3);
	pthread_mutex_unlock(&m1);
	return NULL;
}

void threadCretator()
{
	//pthread_mutex_lock(&m);
	int *tmp=new int(4);
	//pthread_mutex_unlock(&m);
	pthread_t id1,id2,id3;

	pthread_create(&id1,NULL,thread1,tmp);
	pthread_create(&id2,NULL,thread2,tmp);
	pthread_create(&id3,NULL,thread3,tmp);
	pthread_join(id3,NULL);
	pthread_join(id2,NULL);
	pthread_join(id1,NULL);	

	delete tmp;
}

int main()
{
	threadCretator();
	return 0;
}