#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
using namespace std;

int tmp=5;

pthread_rwlock_t rwlock1=PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t rwlock2=PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t rwlock3=PTHREAD_RWLOCK_INITIALIZER;

void* foo1(void *)
{
	pthread_rwlock_rdlock(&rwlock1);
	pthread_rwlock_wrlock(&rwlock2);
	pthread_rwlock_rdlock(&rwlock3);

	cout<<tmp<<endl;
	
	pthread_rwlock_unlock(&rwlock3);
	pthread_rwlock_unlock(&rwlock2);
	pthread_rwlock_unlock(&rwlock1);

	return NULL;
}

void* foo2(void *)
{
	pthread_rwlock_rdlock(&rwlock2);
	pthread_rwlock_rdlock(&rwlock3);

	tmp=8;	
	
	pthread_rwlock_unlock(&rwlock3);
	pthread_rwlock_unlock(&rwlock2);

	return NULL;
}

int main()
{
	pthread_t id1,id2;
	pthread_create(&id1,NULL,foo1,NULL);
	pthread_create(&id2,NULL,foo2,NULL);

	void *retval;

	pthread_join(id1,&retval);
	pthread_join(id2,&retval);

	return 0;
}