#include <iostream>
#include <pthread.h>
#include <stdlib.h>
using namespace std;

int tester = 0;


void *thread1(void *)
{
	
	for(int i=0;i<1000000;i++) ;
	cout<<tester<<endl;
	return NULL;
}
void *thread2(void *)
{
	
	for(int i=0;i<1000000;i++) ;
	tester += 3;
	return NULL;
}

void threadCretator()
{
	
	pthread_t id1,id2;
	pthread_create(&id1,NULL,thread1,NULL);
	tester += 2;
	pthread_create(&id2,NULL,thread2,NULL);
	for(int i=0;i<1000000;i++) ;
	
	pthread_join(id1,NULL);
	pthread_join(id2,NULL);
	
}

int main()
{
	threadCretator();
	//tester+=1;
	return 0;
}