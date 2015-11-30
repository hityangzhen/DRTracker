#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <stdint.h>
#include <semaphore.h>

sem_t sem;
int tester=0;

void* task1(void* param) {
    tester+=2;
    sem_wait(&sem);
    return NULL;
}

void* task2(void* param) {
	tester=1;
    sem_post(&sem);
    sem_post(&sem);
    return NULL;
}

void* task3(void *param) {
	sem_wait(&sem);
	tester=1;
	return NULL;
} 

int main() {
    pthread_t tid1,tid2,tid3;
    sem_init(&sem,0,0);

    pthread_create(&tid1,NULL,task1,NULL);
    pthread_create(&tid2,NULL,task2,NULL);
    pthread_create(&tid3,NULL,task3,NULL);

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
	pthread_join(tid3, NULL);
	    
    sem_destroy(&sem);
    return 0;
}
