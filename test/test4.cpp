#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;
int a[8]={0,1,2,3,4,5,6,7};

void *thread1(void *tmp)
{
	for(int i=0;i<8;i++) {
		a[i]=i;
		sleep(1);
	}
		
	return NULL;
}
void *thread2(void *tmp)
{

	for(int i=0;i<8;i++) {
		a[i]=i;
		sleep(2);
	}
		
	return NULL;
}
void threadCretator()
{	
	pthread_t id1,id2;

	pthread_create(&id1,NULL,thread1,NULL);
	pthread_create(&id2,NULL,thread2,NULL);
	for(int i=0;i<8;i++) {
		a[i]=i;
		sleep(1);
	}	
	pthread_join(id1,NULL);
	pthread_join(id2,NULL);
}

int main()
{
	threadCretator();
	//tester+=1;
	return 0;
}