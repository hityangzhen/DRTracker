#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
using namespace std;


struct  {
	pthread_rwlock_t rwlock;
	int product;
} shareData={PTHREAD_RWLOCK_INITIALIZER,0};

void * produce(void *);
void * consume1(void *);
void * consume2(void *);


int main()
{
	pthread_t id1,id2,id3;
	pthread_create(&id1,NULL,produce,NULL);
	pthread_create(&id2,NULL,consume1,NULL);
	pthread_create(&id3,NULL,consume2,NULL);

	void *retval;

	pthread_join(id1,&retval);
	pthread_join(id2,&retval);
	pthread_join(id3,&retval);

	return 0;
}


void * produce(void *ptr)
{
	for(int i=0;i<1;i++) {
		pthread_rwlock_wrlock(&shareData.rwlock);
		shareData.product += i;
		pthread_rwlock_unlock(&shareData.rwlock);

		sleep(1);
	}

	return NULL;
}

/**
 * we can see that the read lock can also load from memory
 * and store to memory
 */
void * consume1(void *ptr)
{
	for(int i=0;i<1;) {
		pthread_rwlock_rdlock(&shareData.rwlock);
		cout<<"consume1:"<<shareData.product<<endl;
		pthread_rwlock_unlock(&shareData.rwlock);

		++i;
		sleep(1);
	}
	return NULL;
}

void * consume2(void *ptr)
{
	for(int i=0;i<1;) {
		pthread_rwlock_rdlock(&shareData.rwlock);
		cout<<"consume2:"<<shareData.product<<endl;
		shareData.product += 2;
		pthread_rwlock_unlock(&shareData.rwlock);
		++i;
		sleep(1);
	}
	return NULL;
}


