#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;

bool flag=true;
pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;

void *thread1(void *tmp)
{
	*(int *)tmp = 1;
	pthread_mutex_lock(&m1);
	flag=true;
	pthread_mutex_unlock(&m1);
	return NULL;
}

void *thread2(void *tmp)
{
	pthread_mutex_lock(&m1);
	bool f=flag;
	pthread_mutex_unlock(&m1);
	if(f)
		*(int *)tmp = 2; 
	return NULL;
}

void threadCretator()
{
	int *tmp=new int(4);
	
	pthread_t id1,id2;

	pthread_create(&id1,NULL,thread1,tmp);
	pthread_create(&id2,NULL,thread2,tmp);
	pthread_join(id2,NULL);
	pthread_join(id1,NULL);	
	cout<<*tmp<<endl;
	delete tmp;
}

int main()
{
	threadCretator();
	return 0;
}