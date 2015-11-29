#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <stdint.h>

pthread_barrier_t b;
pthread_mutex_t m=PTHREAD_MUTEX_INITIALIZER;
int tester=0;

void* task1(void* param) {
    tester+=2;
    pthread_barrier_wait(&b);
    return NULL;
}

void* task2(void* param) {

    pthread_barrier_wait(&b);
    tester=1;
    return NULL;
}

void* task3(void *param) {
    pthread_mutex_lock(&m);
    tester+=3;
    pthread_mutex_unlock(&m);
    pthread_barrier_wait(&b);
    return NULL;
}

void* task4(void *param) {
    pthread_mutex_lock(&m);
    tester=5;
    pthread_mutex_unlock(&m);
    return NULL;    
}

int main() {
    pthread_t tid1,tid2,tid3,tid4;
    pthread_barrier_init(&b, 0, 3);

    pthread_create(&tid1,NULL,task1,NULL);
    pthread_create(&tid2,NULL,task2,NULL);
    pthread_create(&tid3,NULL,task3,NULL);
    pthread_create(&tid4,NULL,task4,NULL);

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    pthread_join(tid4, NULL);
    
    pthread_barrier_destroy(&b);
    return 0;
}
