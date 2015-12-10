#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;

void *thread1(void *tmp)
{
	for(int i=0;i<4;i++) {
		cout<<*(int *)tmp<<endl;
		*(int *)tmp=2;
	}
	return NULL;
}
void *thread2(void *tmp)
{
	for(int i=0;i<4;i++)
		cout<<*(int *)tmp<<endl;
	return NULL;
}

void *thread3(void *tmp)
{
	*(int *)tmp=3;
	return NULL;
}

void threadCretator()
{
	int *tmp=new int(4);
	pthread_t id1,id2;
	pthread_create(&id1,NULL,thread1,tmp);
	pthread_create(&id2,NULL,thread2,tmp);
	for(int i=0;i<1000000;i++) ;
	pthread_join(id1,NULL);
	pthread_join(id2,NULL);
	delete tmp;
}

int main()
{
	threadCretator();
	return 0;
}