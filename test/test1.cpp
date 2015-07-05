#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;

int tester = 0;
int LOOP_COUNT = 5;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void *thread1(void *tmp)
{
	tester += 3;
	for(int i=0;i<1000000;i++) ;
	*(int *)tmp += 1;
	cout<<tester<<endl;
	return NULL;
}
void *thread2(void *tmp)
{
	
	*(int *)tmp += 2;
	for(int i=0;i<1000000;i++) ;
	tester += 3;
	return NULL;
}

void threadCretator()
{
	int *tmp=new int(4);

	pthread_t id1,id2;
	pthread_create(&id1,NULL,thread1,tmp);
	tester += 2;
	pthread_create(&id2,NULL,thread2,tmp);
	for(int i=0;i<1000000;i++) ;
	cout<<*tmp<<endl;
	pthread_join(id1,NULL);
	pthread_join(id2,NULL);
	delete tmp;
}

int main()
{
	threadCretator();
	//tester+=1;
	return 0;
}