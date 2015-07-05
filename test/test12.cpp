#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;

int glob[3];

pthread_rwlock_t m1 = PTHREAD_RWLOCK_INITIALIZER;

void *thread1(void *tmp)
{
	pthread_rwlock_rdlock(&m1);
	glob[0]++;
	pthread_rwlock_unlock(&m1);
	return NULL;
}

void *thread2(void *tmp)
{
	pthread_rwlock_rdlock(&m1);
	glob[1]++;
	pthread_rwlock_unlock(&m1);
	return NULL;
}

void *thread3(void *tmp)
{
	pthread_rwlock_rdlock(&m1);
	glob[2]++;
	pthread_rwlock_unlock(&m1);
	return NULL;
}

void *thread4(void *tmp)
{
	pthread_rwlock_wrlock(&m1);
	int sum=glob[0]+glob[1]+glob[2];
	cout<<sum<<endl;
	pthread_rwlock_unlock(&m1);
	return NULL;
}

void threadCretator()
{

	pthread_t id1,id2,id3,id4;

	pthread_create(&id1,NULL,thread1,NULL);
	//pthread_create(&id2,NULL,thread2,NULL);
	//pthread_create(&id3,NULL,thread3,NULL);
	pthread_create(&id4,NULL,thread4,NULL);

	for(int i=0;i<1000000;i++) ;

	pthread_join(id1,NULL);	
//	pthread_join(id2,NULL);	
//	pthread_join(id3,NULL);
	pthread_join(id4,NULL);
}

int main()
{
	threadCretator();
	return 0;
}