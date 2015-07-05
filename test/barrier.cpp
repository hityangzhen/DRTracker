#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <stdint.h>

pthread_barrier_t b;

void* task(void* param) {
    uint64_t id=reinterpret_cast<uint64_t>(param);
    printf("before the barrier %ld\n", id);
    pthread_barrier_wait(&b);
    printf("after the barrier %ld\n", id);
    return NULL;
}

int main() {
    const int nThread = 5;
    pthread_t thread[nThread];
    pthread_barrier_init(&b, 0, nThread);
    for (int i = 0; i < nThread; i++)
        pthread_create(&thread[i], 0, task, reinterpret_cast<void *>(i));
        
    for (int i = 0; i < nThread; i++)
        pthread_join(thread[i], 0);
    pthread_barrier_destroy(&b);
    return 0;
}
