#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
using namespace std;

int *tmp=NULL;

pthread_rwlock_t rwlock1=PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t rwlock2=PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t rwlock3=PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t rwlock4=PTHREAD_RWLOCK_INITIALIZER;

void* foo1(void *)
{
	pthread_rwlock_rdlock(&rwlock1);
	pthread_rwlock_rdlock(&rwlock2);
	pthread_rwlock_wrlock(&rwlock3);

	*tmp=4;
	
	pthread_rwlock_unlock(&rwlock3);
	pthread_rwlock_unlock(&rwlock2);
	pthread_rwlock_unlock(&rwlock1);

	return NULL;
}

void* foo2(void *)
{
	pthread_rwlock_rdlock(&rwlock3);
	pthread_rwlock_wrlock(&rwlock4);

	cout<<*tmp<<endl;

	pthread_rwlock_unlock(&rwlock4);
	pthread_rwlock_unlock(&rwlock3);

	return NULL;
}

void* foo3(void *)
{
	tmp=new int(1);
	cout<<*tmp<<endl;
	return NULL;
}

int main()
{
	pthread_t id1,id2,id3;
	void *retval;

	pthread_create(&id3,NULL,foo3,NULL);
	pthread_join(id3,&retval);	

	pthread_create(&id1,NULL,foo1,NULL);
	pthread_create(&id2,NULL,foo2,NULL);

	

	pthread_join(id1,&retval);
	pthread_join(id2,&retval);

	delete tmp;

	return 0;
}