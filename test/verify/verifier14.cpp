#include <iostream>
#include <pthread.h>
#include <unistd.h>
using namespace std;
typedef void * (*HANDLER)(void *);
int flag=0;

void thread1()
{
	// sleep(1);
	flag=1;
}
void thread2()
{
	int i=0;
	while(flag!=1) {
		cout<<flag<<endl;	
	}
}

void threadCretator()
{
	pthread_t id1,id2;
	pthread_create(&id1,NULL,(HANDLER)thread1,NULL);
	pthread_create(&id2,NULL,(HANDLER)thread2,NULL);
	for(int i=0;i<1000000;i++) ;
	pthread_join(id1,NULL);
	pthread_join(id2,NULL);
}

int main()
{
	threadCretator();
	return 0;
}