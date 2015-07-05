#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <stdint.h>

pthread_barrier_t b;
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

int main() {
    pthread_t tid1,tid2;
    pthread_barrier_init(&b, 0, 2);

    pthread_create(&tid1,NULL,task1,NULL);
    pthread_create(&tid2,NULL,task2,NULL);

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    pthread_barrier_destroy(&b);
    return 0;
}
