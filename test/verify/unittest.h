#ifndef __TEST_VERIFY_UNITTEST_H
#define __TEST_VERIFY_UNITTEST_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

typedef void *(*HANDLER)(void *);
typedef void (*RUN_HANDLER)(void);

pthread_t tid1, tid2, tid3;

pthread_mutex_t m1=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m2=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m3=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m4=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m5=PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t atom=PTHREAD_MUTEX_INITIALIZER;

pthread_rwlock_t rwlk1=PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t rwlk2=PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t rwlk3=PTHREAD_RWLOCK_INITIALIZER;
//condvar
pthread_cond_t cv1=PTHREAD_COND_INITIALIZER;
pthread_cond_t cv2=PTHREAD_COND_INITIALIZER;
pthread_cond_t cv3=PTHREAD_COND_INITIALIZER;
pthread_cond_t cv4=PTHREAD_COND_INITIALIZER;
pthread_cond_t cv5=PTHREAD_COND_INITIALIZER;

//semaphore
sem_t sem1;
sem_t sem2;

//producer-consumer semaphore
sem_t p_sem1;
sem_t p_sem2;
sem_t c_sem1;
sem_t c_sem2;

//barrier
pthread_barrier_t b1;

// This struct does not implement a signal-wait synchronization
// primitive, even if it looks like one. Its purpose is to enforce an
// order of execution of threads in unit tests in a way that is
// invisible to ThreadSanitizer and similar tools. It lacks memory
// barriers, therefore it only works reliably if there is a real
// synchronization primitive before signal() or after wait().
typedef struct StealthNotify {
	volatile int flag_;
} StealthNotify;
void stealth_notify_signal(StealthNotify *sn) {
	sn->flag_=1;
}
void stealth_notify_wait(StealthNotify *sn) {
	while(!sn->flag_)
		sched_yield();
}

#define STEALTH_NOTIFY_INIT_1(sn1) sn1.flag_=0
#define STEALTH_NOTIFY_INIT_2(sn1,sn2) STEALTH_NOTIFY_INIT_1(sn1);			\
	sn2.flag_=0;
#define STEALTH_NOTIFY_INIT_3(sn1,sn2,sn3) STEALTH_NOTIFY_INIT_2(sn1,sn2);	\
	sn3.flag_=0;
#define STEALTH_NOTIFY_INIT_4(sn1,sn2,sn3,sn4) 								\
	STEALTH_NOTIFY_INIT_3(sn1,sn2,sn3);										\
	sn4.flag_=0;
#define STEALTH_NOTIFY_INIT_5(sn1,sn2,sn3,sn4,sn5) 							\
	STEALTH_NOTIFY_INIT_4(sn1,sn2,sn3,sn4);									\
	sn5.flag_=0;
#define STEALTH_NOTIFY_INIT(num,...) STEALTH_NOTIFY_INIT_##num(__VA_ARGS__)			 

//thread
#define PTHREAD_INIT_1(thd1,e1)												\
	thd1.entry_=e1, thd1.arg_=NULL
#define PTHREAD_INIT_2(thd1,e1,thd2,e2) PTHREAD_INIT_1(thd1,e1);			\
	thd2.entry_=e2, thd2.arg_=NULL
#define PTHREAD_INIT(num,...) PTHREAD_INIT_##num(__VA_ARGS__)

#define PTHREAD_ID_1 pthread_t id1
#define PTHREAD_ID_2 PTHREAD_ID_1, id2
#define PTHREAD_ID_3 PTHREAD_ID_2, id3
#define PTHREAD_ID_4 PTHREAD_ID_3, id4
#define PTHREAD_ID(num) PTHREAD_ID_##num

#define PTHREAD_CREATE_1(f1) pthread_create(&id1,NULL,(HANDLER)f1,NULL)
#define PTHREAD_CREATE_2(f1,f2) PTHREAD_CREATE_1(f1);						\
	pthread_create(&id2,NULL,(HANDLER)f2,NULL)
#define PTHREAD_CREATE_3(f1,f2,f3) PTHREAD_CREATE_2(f1,f2);					\
	pthread_create(&id3,NULL,(HANDLER)f3,NULL)
#define PTHREAD_CREATE_4(f1,f2,f3,f4) PTHREAD_CREATE_3(f1,f2,f3);			\
	pthread_create(&id4,NULL,(HANDLER)f4,NULL)
#define PTHREAD_CREATE(num,...) PTHREAD_CREATE_##num(__VA_ARGS__)

#define PTHREAD_JOIN_1 pthread_join(id1,NULL)
#define PTHREAD_JOIN_2 PTHREAD_JOIN_1;pthread_join (id2,NULL)
#define PTHREAD_JOIN_3 PTHREAD_JOIN_2;pthread_join (id3,NULL)
#define PTHREAD_JOIN_4 PTHREAD_JOIN_3;pthread_join (id4,NULL)
#define PTHREAD_JOIN(num) PTHREAD_JOIN_##num

#define PTHREAD_RUN(num,...) 												\
	PTHREAD_ID(num);     													\
	PTHREAD_CREATE(num,__VA_ARGS__); 										\
	PTHREAD_JOIN(num)

//mutex-lock
#define LOCK_INIT_1(lk1)													\
	lk1.m_=&m1,lk1.cv_=&cv1,lk1.signal_at_unlock_=false
#define LOCK_INIT_2(lk1,lk2) LOCK_INIT_1(lk1);								\
	lk2.m_=&m2,lk2.cv_=&cv2,lk2.signal_at_unlock_=false
#define LOCK_INIT_3(lk1,lk2,lk3) LOCK_INIT_2(lk1,lk2);						\
	lk3.m_=&m3,lk3.cv_=&cv3,lk3.signal_at_unlock_=false
#define LOCK_INIT_4(lk1,lk2,lk3,lk4) LOCK_INIT_3(lk1,lk2,lk3);				\
	lk4.m_=&m4,lk4.cv_=&cv4,lk4.signal_at_unlock_=false
#define LOCK_INIT_5(lk1,lk2,lk3,lk4,lk5) LOCK_INIT_4(lk1,lk2,lk3,lk4);		\
	lk5.m_=&m5,lk5.cv_=&cv5,lk5.signal_at_unlock_=false
#define LOCK_INIT(num,...) LOCK_INIT_##num(__VA_ARGS__)

//rwlock
#define RWLOCK_INIT_1(rw1) rw1.rw_=&rwlk1
#define RWLOCK_INIT_2(rw1,rw2) RWLOCK_INIT_1(rw1);							\
	rw2.rw_=&rwlk2
#define RWLOCK_INIT(num,...) RWLOCK_INIT_##num(__VA_ARGS__)

//condvar
#define CV_INIT_1(condvar1) condvar1.cv_=&cv1
#define CV_INIT_2(condvar1,condvar2) CV_INIT_1(condvar1);					\
	condvar2.cv_=&cv2
#define CV_INIT(num,...) CV_INIT_##num(__VA_ARGS__)

#define CV_SIGNAL_COND(cond,val) 											\
	pthread_mutex_lock(&m1);												\
	cond=val;																\
	pthread_cond_signal(&cv1);												\
	pthread_mutex_unlock(&m1)

#define CV_SIGNAL_ALL_COND(cond,val) 										\
	pthread_mutex_lock(&m1);												\
	cond=val;																\
	pthread_cond_broadcast(&cv1);											\
	pthread_mutex_unlock(&m1)


#define CV_WAIT_COND(cond,condition)										\
	pthread_mutex_lock(&m1);												\
	while(cond!=condition)													\
		pthread_cond_wait(&cv1,&m1);										\
	pthread_mutex_unlock(&m1)


#define CV_SIGNAL_COND_MUTEX(cond,mutex,val)								\
	pthread_mutex_lock(&mutex);												\
	cond=val;																\
	pthread_cond_signal(&cv1);												\
	pthread_mutex_unlock(&mutex)

#define CV_WAIT_COND_MUTEX(cond,mutex,condition)							\
	pthread_mutex_lock(&mutex);												\
	while(cond!=condition)													\
		pthread_cond_wait(&cv1,&m);											\
	pthread_mutex_unlock(&mutex)

//semaphore
#define SEM_INIT_1(val1) sem_init(&sem1,0,val1)
#define SEM_INIT_2(val1,val2) SEM_INIT_1(val1);								\
	sem_init(&sem2,0,val2)
#define SEM_INIT(num,...) SEM_INIT_##num(__VA_ARGS__)

#define SEM_POST_1() sem_post(&sem1)
#define SEM_POST_2() sem_post(&sem2)

#define SEM_WAIT_1() sem_wait(&sem1)
#define SEM_WAIT_2() sem_wait(&sem2)
#define SEM_TRYWAIT_1() sem_trywait(&sem1)
#define SEM_TRYWAIT_2() sem_trywait(&sem2)

#define SEM_DESTROY_1() sem_destroy(&sem1)
#define SEM_DESTROY_2() SEM_DESTROY_1(); sem_destroy(&sem2)
#define SEM_DESTROY(num) SEM_DESTROY_##num() 

//producer-consumer semaphore
#define PC_SEM_INIT_1(val) sem_init(&p_sem1,0,val);sem_init(&c_sem1,0,0)
#define PC_SEM_INIT_2(val) PC_SEM_INIT_1(val); 								\
	sem_init(&p_sem2,0,val);sem_init(&c_sem2,0,0)
#define PC_SEM_INIT(num,val) PC_SEM_INIT_##num(val)

#define PC_SEM_DESTROY_1 sem_destroy(&p_sem1);sem_destroy(&c_sem1)
#define PC_SEM_DESTROY_2 													\
	PC_SEM_DESTROY_1;sem_destroy(&p_sem2);sem_destroy(&c_sem2)
#define PC_SEM_DESTROY(num) PC_SEM_DESTROY_##num

#define PC_SEM_RUN(s_num,t_num,val,...) 									\
	PC_SEM_INIT(s_num,val); 												\
	PTHREAD_RUN(t_num,__VA_ARGS__);											\
	PC_SEM_DESTROY(s_num)
//use semaphore simulate producer-consumer(doesn't consider pool)
#define PC_SEM_PRODUCE(num) sem_wait(&p_sem##num);sem_post(&c_sem##num)
#define PC_SEM_CONSUME(num) sem_wait(&c_sem##num);sem_post(&p_sem##num)

//barrier
#define BARRIER_INIT(b,val) pthread_barrier_init(&b.b_,0,val)
#define BARRIER_DESTROY(b) pthread_barrier_destroy(&b.b_)
#define BARRIER_RUN(t_num,b,val,...)										\
	BARRIER_INIT(b,val);													\
	PTHREAD_RUN(t_num,__VA_ARGS__);											\
	BARRIER_DESTROY(b)

#ifdef ATOMIC
#define ATOMIC_INCREMENT(ptr,val) __sync_add_and_fetch((ptr),val)
#else
static int32_t fetch_and_add(volatile int32_t* ptr, int32_t val) {
    pthread_mutex_lock(&atom);
    int tmp=*ptr;
    *ptr += val;
    pthread_mutex_unlock(&atom);
    return tmp;
}
#define ATOMIC_INCREMENT(ptr,val) fetch_and_add(ptr,val)
#endif


#define ATOMIC_LOAD(ptr) *((volatile int *)(ptr))

//test thread id
pthread_t test_id;

static int test_num=0;
typedef struct Test {
	HANDLER f_;
	int id_;
} Test;
static Test test[100];

#define REGISTER_TEST(f,id)													\
	test[test_num].f_=f; 													\
	test[test_num++].id_=id 	

#define TEST_CREATE(run) pthread_create(&test_id,NULL,(HANDLER)run,NULL)
#define TEST_FINISH() pthread_join(test_id,NULL)
#define TEST_RUN(i)															\
	printf("[TEST%d]========================================\n",(i));		\
	TEST_CREATE(run_##i);													\
	TEST_FINISH();															\
	printf("[TEST%d]========================================\n\n",(i))

#endif //__TEST_VERIFY_UNITTEST_H