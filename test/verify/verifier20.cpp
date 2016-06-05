#include <iostream>
#include <pthread.h> 
#include <unistd.h>
#include <stdio.h>
/* dangling pointer access memory bug */
class A {
public:
	virtual ~A() {}
	virtual void F() {
		printf("A::F()\n");
	}
};

class B:public A{
public:
	virtual void F() {
		printf("B:F()\n");
	}
};

void *thread1(void *tmp)
{
	sleep(1);
	((A *)tmp)->F();
	return NULL;
}

void *thread2(void *tmp)
{
	delete (A *)tmp;
	return NULL;
}

void threadCretator()
{
	A *tmp=new B;
	pthread_t id1,id2;
	pthread_create(&id1,NULL,thread2,tmp);
	pthread_create(&id2,NULL,thread1,tmp);
	pthread_join(id1,NULL);
	pthread_join(id2,NULL);
}

int main()
{
	threadCretator();
	return 0;
}