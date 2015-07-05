#include <iostream>
#include <pthread.h> 
#include <stdio.h>

//declare the mutex and ondition variable
pthread_mutex_t counter_lock= PTHREAD_MUTEX_INITIALIZER;  
pthread_cond_t counter_nonzero=PTHREAD_COND_INITIALIZER;  

int counter = 0;  
      
void *decrement_counter(void *argv);  
void *increment_counter(void *argv);  
      
int main(int argc, char **argv)  
{ 
    pthread_t thd1, thd2;  
    int ret;  
    //先创建的是等待线程，用pthread_cond_wait  
    pthread_create(&thd1, NULL, decrement_counter, NULL);
    //后创建的是激活线程，用pthread_cond_signal
    pthread_create(&thd2, NULL, increment_counter, NULL); 
    //pthread_join(thd2, NULL); 
    //pthread_join(thd1, NULL); 

    sleep(3);
    return 0;  
}  
      

void *decrement_counter(void *argv)  
{  
    pthread_mutex_lock(&counter_lock);  
    while(counter == 0){  
        printf("thd1 decrement before cond_wait\n");  
        pthread_cond_wait(&counter_nonzero, &counter_lock);  
        printf("thd1 decrement after cond_wait \n");  
    }  
    counter--;  
    printf("thd1 decrement:counter = %d \n", counter);  
    pthread_mutex_unlock(&counter_lock);
    return NULL;
}  
      
void *increment_counter(void *argv)  
{  
    pthread_mutex_lock(&counter_lock);  
    printf("thd2 increment get the lock\n");  
    if(counter == 0) {  
        printf("thd2 increment before cond_signal\n");  
        counter++;  
        pthread_cond_signal(&counter_nonzero);  
        printf("thd2 increment after  cond_signal\n");  
    }  
    printf("thd2 increment:counter = %d \n", counter);  
    pthread_mutex_unlock(&counter_lock); 
    return NULL;
}