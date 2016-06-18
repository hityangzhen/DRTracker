#include <iostream>
#include <pthread.h> 
#include <unistd.h>
/* unintialized read memory bug */
struct obj {
	int *a;
};

obj *ptr;

void *fun1()
{
	
	std::cout<<*(ptr->a)<<std::endl;	
	return NULL;
}

void *fun2(void *tmp)
{
	
	sleep(1);
	ptr->a=(int *)tmp;
	return NULL;
}

void *thread1(void *)
{
	return fun1();
}

void *thread2(void *tmp)
{
	return fun2(tmp);
}

void threadCretator()
{
	ptr=new obj;
	int *tmp=new int(19);
	pthread_t id1,id2;
	pthread_create(&id1,NULL,thread2,tmp);
	pthread_create(&id2,NULL,thread1,NULL);
	pthread_join(id1,NULL);
	pthread_join(id2,NULL);
	delete ptr->a;
	delete ptr;
}

int main()
{
	threadCretator();
	return 0;
}