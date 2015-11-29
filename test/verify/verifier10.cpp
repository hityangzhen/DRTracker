#include <iostream>
#include <pthread.h> 
#include <stdio.h>

//declare the mutex and ondition variable
pthread_mutex_t m1=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m2=PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cv=PTHREAD_COND_INITIALIZER;

int flag1=0;
int flag2=0;
int x=0;
int y=0;


void *task1(void *argv)
{
    x++;
    pthread_mutex_lock(&m1);
    flag1=1;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&m1);
    return NULL;
}

void *task2(void *argv)
{
    y++;
    pthread_mutex_lock(&m2);
    flag2=1;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&m2);
    return NULL;
}

void *task3(void *argv)
{
    pthread_mutex_lock(&m1);
    while(flag1!=1)
        pthread_cond_wait(&cv,&m1);
    pthread_mutex_unlock(&m1);
    y--;
    return NULL;
}

void *task4(void *argv)
{
    pthread_mutex_lock(&m2);
    while(flag2!=1)
        pthread_cond_wait(&cv,&m2);
    pthread_mutex_unlock(&m2);
    x--;
    return NULL;
}
      
int main(int argc, char **argv)  
{ 
    pthread_t thd1,thd2,thd3,thd4;  
    int ret;  
    pthread_create(&thd1, NULL, task1, NULL);
    pthread_create(&thd2, NULL, task2, NULL);
    pthread_create(&thd3, NULL, task3, NULL);
    pthread_create(&thd4, NULL, task4, NULL);

    pthread_join(thd2, NULL); 
    pthread_join(thd1, NULL);  
    pthread_join(thd3, NULL); 
    pthread_join(thd4, NULL); 
    return 0;  
}