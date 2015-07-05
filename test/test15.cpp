/**
 * specified for AccuLock
 */
#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;

int tester=0;
int LOOP_COUNT = 5;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m_top = PTHREAD_MUTEX_INITIALIZER;

void *thread1(void *tmp)
{
	pthread_mutex_lock(&m);
	cout<<tester<<endl;
	pthread_mutex_unlock(&m);
	return NULL;
}
void *thread2(void *tmp)
{
	pthread_mutex_lock(&m_top);
	pthread_mutex_lock(&m);
	
	for(int i=0;i<1000000;i++) ;
	cout<<tester<<endl;
	pthread_mutex_unlock(&m);
	pthread_mutex_unlock(&m_top);
	return NULL;
}
void *thread3(void *tmp)
{
	pthread_mutex_lock(&m_top);
	tester = 3;
	for(int i=0;i<1000000;i++) ;
	pthread_mutex_unlock(&m_top);
	return NULL;
}

void threadCretator()
{
	pthread_t id1,id2,id3;
	pthread_create(&id1,NULL,thread1,NULL);
	pthread_create(&id2,NULL,thread2,NULL);
	pthread_create(&id3,NULL,thread3,NULL);
	pthread_join(id3,NULL);
	pthread_join(id2,NULL);
	pthread_join(id1,NULL);	
}

int main()
{
	threadCretator();
	return 0;
}