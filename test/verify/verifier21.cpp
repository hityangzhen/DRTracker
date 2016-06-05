#include <iostream>
#include <pthread.h> 
#include <unistd.h>
#include <string.h>

/* buffer overflow memory bug */
int cnt;
void *thread1(void *tmp)
{
	char buf[4];
	memcpy(&buf[cnt],tmp,4);
	return NULL;
}

void *thread2(void *tmp)
{
	cnt=6;
	return NULL;
}

void threadCretator()
{
	char *tmp="123";
	pthread_t id1,id2;
	pthread_create(&id1,NULL,thread1,tmp);
	pthread_create(&id2,NULL,thread2,tmp);
	pthread_join(id1,NULL);
	pthread_join(id2,NULL);
}

int main()
{
	threadCretator();
	return 0;
}