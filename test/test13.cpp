#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;

int *val=NULL;

//pthread_rwlock_t m1 = PTHREAD_RWLOCK_INITIALIZER;

void *thread1(void *tmp)
{
	//pthread_rwlock_rdlock(&m1);
	*val=3;
	//pthread_rwlock_unlock(&m1);
	return NULL;
}

void *thread2(void *tmp)
{
	for(int i=0;i<10000;i++) ;
	//pthread_rwlock_rdlock(&m1);
	cout<<"thread2 val:"<<*val<<endl;
	//pthread_rwlock_unlock(&m1);
	return NULL;
}

void *thread3(void *tmp)
{
	for(int i=0;i<10000;i++) ;
	//pthread_rwlock_rdlock(&m1);
	*val=6;
	cout<<"thread3 val:"<<*val<<endl;
	//pthread_rwlock_unlock(&m1);
	return NULL;
}

void threadCretator()
{
	val=new int(4);

	pthread_t id1,id2,id3;

	pthread_create(&id1,NULL,thread1,NULL);
	pthread_create(&id2,NULL,thread2,NULL);
	pthread_create(&id3,NULL,thread3,NULL);

	pthread_join(id3,NULL);	
	pthread_join(id2,NULL);
	pthread_join(id1,NULL);

	delete val;
}

int main()
{
	threadCretator();
	return 0;
}