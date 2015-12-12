#include <iostream>
#include <pthread.h>
#include <unistd.h>
using namespace std;

volatile int flag=0;

void *thread1(void *tmp)
{
	sleep(2);
	flag=1;
	return NULL;
}
void *thread2(void *tmp)
{
	int i=0;
	while(flag!=1) {
		cout<<flag<<endl;	
	}
	return NULL;
}

void threadCretator()
{
	pthread_t id1,id2;
	pthread_create(&id2,NULL,thread2,NULL);
	pthread_create(&id1,NULL,thread1,NULL);
	for(int i=0;i<1000000;i++) ;
	pthread_join(id1,NULL);
	pthread_join(id2,NULL);
}

int main()
{
	threadCretator();
	return 0;
}