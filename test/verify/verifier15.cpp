#include <iostream>
#include <pthread.h> 
#include <stdio.h>
#include <unistd.h>
//declare the mutex and ondition variable
pthread_mutex_t counter_lock= PTHREAD_MUTEX_INITIALIZER;  
pthread_cond_t counter_nonzero=PTHREAD_COND_INITIALIZER;  
pthread_mutex_t counter_lock2= PTHREAD_MUTEX_INITIALIZER; 

int counter = 0;  
int tester=0;
      
void *decrement_counter(void *argv);  
void *increment_counter(void *argv);  
      
int main(int argc, char **argv)  
{ 
    pthread_t thd1, thd2;  
    int ret;  
    //后创建的是激活线程，用pthread_cond_signal
    pthread_create(&thd2, NULL, increment_counter, NULL); 
    sleep(1);  
    pthread_create(&thd1, NULL, decrement_counter, NULL);
    pthread_join(thd2, NULL); 
    pthread_join(thd1, NULL); 

    return 0;  
}  
      

void *decrement_counter(void *argv)  
{  
    sleep(2);
    pthread_mutex_lock(&counter_lock);  
    while(counter == 0){  
        tester=1; 
        pthread_cond_wait(&counter_nonzero, &counter_lock);   
    }  
    counter--;  
    printf("thd1 decrement:counter = %d \n", counter);  
    pthread_mutex_unlock(&counter_lock);
    printf("%d\n",tester);
    return NULL;
}  
      
void *increment_counter(void *argv)  
{  
    pthread_mutex_lock(&counter_lock2);
    tester=2;
    pthread_mutex_unlock(&counter_lock2);

    pthread_mutex_lock(&counter_lock2);
    tester=3;
    pthread_mutex_lock(&counter_lock); 
    if(counter == 0) {  
        counter++;  
        pthread_cond_signal(&counter_nonzero);   
        counter++;  
    }  
    pthread_mutex_unlock(&counter_lock); sleep(1);
    pthread_mutex_unlock(&counter_lock2);
    return NULL;
}