#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;

int tester = 0;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m_top = PTHREAD_MUTEX_INITIALIZER;
void *thread1(void *tmp)
{
	pthread_mutex_lock(&m);
	*(int *)tmp = 1;
	pthread_mutex_unlock(&m);
	return NULL;
}
void *thread2(void *tmp)
{
	pthread_mutex_lock(&m_top);
	for(int i=0;i<1000000;i++) ;
	cout<<*(int *)tmp<<endl;
		
	pthread_mutex_unlock(&m_top);
	return NULL;
}
void *thread3(void *tmp)
{
	pthread_mutex_lock(&m);
	*(int *)tmp = 3;
	for(int i=0;i<1000000;i++) ;
	pthread_mutex_unlock(&m);
	*(int *)tmp = 3;
	return NULL;
}

void threadCretator()
{
	pthread_mutex_lock(&m);
	int *tmp=new int(4);
	pthread_mutex_unlock(&m);
	pthread_t id1,id2,id3;

	pthread_create(&id1,NULL,thread1,tmp);
	pthread_join(id1,NULL);	
	pthread_create(&id2,NULL,thread2,tmp);
	pthread_create(&id3,NULL,thread3,tmp);
	pthread_join(id3,NULL);
	pthread_join(id2,NULL);
	
	delete tmp;
}

int main()
{
	threadCretator();
	return 0;
}