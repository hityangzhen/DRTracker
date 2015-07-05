#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;

int tester = 0;
int LOOP_COUNT = 5;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void *thread1(void *tmp)
{
	pthread_mutex_lock(&m);
	*(int *)tmp = 1;
	pthread_mutex_unlock(&m);
	return NULL;
}
void *thread2(void *tmp)
{
	pthread_mutex_lock(&m);
	cout<<*(int *)tmp<<endl;
	*(int *)tmp += 1;
	pthread_mutex_unlock(&m);
	return NULL;
}
void *thread3(void *tmp)
{
	pthread_mutex_lock(&m);
	*(int *)tmp =3;
	*(int *)tmp =2;
	pthread_mutex_unlock(&m);
	return NULL;
}

void threadCretator()
{
	int *tmp=new int(4);
	cout<<*tmp<<endl;
	*tmp+=6;
	pthread_t id1,id2,id3;

	pthread_create(&id1,NULL,thread1,tmp);
	pthread_create(&id2,NULL,thread2,tmp);
	pthread_create(&id3,NULL,thread3,tmp);

	pthread_join(id1,NULL);
	pthread_join(id2,NULL);
	pthread_join(id3,NULL);

	delete tmp;
}

int main()
{
	threadCretator();
	//tester+=1;
	return 0;
}