/**
 * specified for AccuLock
 */
#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;

int tester = 0;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void *thread1(void *)
{
	cout<<tester<<endl;
	pthread_mutex_lock(&m);
	cout<<tester<<endl;
	pthread_mutex_unlock(&m);
	return NULL;
}
void *thread2(void *)
{
	pthread_mutex_lock(&m);
	for(int i=0;i<1000000;i++) ;
	tester = 2;
	pthread_mutex_unlock(&m);
	return NULL;
}


void threadCretator()
{
	pthread_t id1,id2,id3;
	pthread_create(&id1,NULL,thread1,NULL);
	pthread_create(&id2,NULL,thread2,NULL);
	pthread_join(id2,NULL);
	pthread_join(id1,NULL);	

}

int main()
{
	threadCretator();
	return 0;
}