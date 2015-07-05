#ifndef __TEST_UTILS_H
#define __TEST_UTILS_H
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <queue>
#include "thread_wrappers_pthread.h"
#include <vector>

static bool IsQueueNotEmpty(std::queue<void *> *q) { return !q->empty(); }

//We only use a buff as the pool
class ProducerConsumerQueue {
public:

	ProducerConsumerQueue(int unused) {}
	~ProducerConsumerQueue() {
		assert(q_.empty());
	}
	void Produce(void *);
	void *Consume();
protected:

	bool TryConsume(void **res) {
		if(q_.empty())
			return false;
		*res=q_.front();
		q_.pop();
		return true;
	}
	Mutex mu_;
	std::queue<void *> q_;
};
void *ProducerConsumerQueue::Consume()
{
	mu_.LockWhen(Condition(&IsQueueNotEmpty,&q_));
	void *res;
	bool ok=TryConsume(&res);
	assert(ok);
	mu_.Unlock();
	return res;
}
void ProducerConsumerQueue::Produce(void *ptr)
{
	mu_.Lock();
	q_.push(ptr);
	mu_.Unlock();
}

struct Test;
std::map<int,Test> test_map;
typedef void (*RUN_HANDLER)(void) ;

struct Test {
	RUN_HANDLER f_;
	int id_;
	//other flags

	Test(RUN_HANDLER f,int id):f_(f),id_(id) {}
	Test():f_(0),id_(0) {}
	void Run() {
		f_();
	}
};
struct TestAdder {
	TestAdder(RUN_HANDLER f,int id) {
		test_map[id]=Test(f,id);
	}
};

// An array of threads. Create/start/join all elements at once.
class ThreadArray {
public:
	static const int kSize = 4;
  	typedef void (*F) (void);
  	ThreadArray(F f1, F f2 = NULL, F f3 = NULL, F f4 = NULL, F f5 = NULL) {
    	ar_[0] = new Thread(f1);
    	ar_[1] = f2 ? new Thread(f2) : NULL;
    	ar_[2] = f3 ? new Thread(f3) : NULL;
    	ar_[3] = f4 ? new Thread(f4) : NULL;
  	}
  	~ThreadArray() {
  		for(int i=0;i<kSize;i++)
  			delete ar_[i];
  	}
  	void Start() {
  		for(int i=0;i<kSize;i++) {
  			if(ar_[i]) {
  				ar_[i]->Start();
  				usleep(10);
  			}
  		}
  	}

  	void Join() {
  		for(int i=0;i<kSize;i++) {
  			if(ar_[i])
  				ar_[i]->Join();
  		}
  	}

private:
	Thread *ar_[kSize];
};

struct Closure {
	typedef void (*F0)();
	typedef void (*F1)(void *arg1);
	typedef void (*F2)(void *arg1,void *arg2);
	int n_params;
	void *f;
	void *param1;
	void *param2;

	void Execute() {
		if(n_params==0)
			((F0)(f))();
		else if(n_params==1)
			((F1)(f))(param1);
		else
			((F2)(f))(param1,param2);
		//at this point we will not use this object
		delete this;
	}
};

static Closure *NewCallBack(void (*f)()) {
	Closure *res=new Closure;
	res->n_params=0;
	res->f=(void *)(f);
	res->param1=NULL;
	res->param2=NULL;
	return res;
}

template <typename T>
static Closure *NewCallBack(void (*f)(T *),T *p1) {
	Closure *res=new Closure;
	res->n_params=1;
	res->f=(void *)(f);
	res->param1=(void *)p1;
	res->param2=NULL;
	return res;
}

template <typename T1,typename T2>
static Closure *NewCallBack(void (*f)(T1 *,T2 *),T1 *p1,T2 *p2)
{
	Closure *res=new Closure;
	res->n_params=2;
	res->f=(void *)(f);
	res->param1=(void *)p1;
	res->param2=(void *)p2;
	return res;	
}

/*! A thread pool that uses ProducerConsumerQueue.
  Usage:
  {
    ThreadPool pool(n_workers);
    pool.StartWorkers();
    pool.Add(NewCallback(func_with_no_args));
    pool.Add(NewCallback(func_with_one_arg, arg));
    pool.Add(NewCallback(func_with_two_args, arg1, arg2));
    ... // more calls to pool.Add()

    // the ~ThreadPool() is called: we wait workers to finish
    // and then join all threads in the pool.
  }
*/

class ThreadPool {
public:
	//create n_threads threads,but do not start
	explicit ThreadPool(int n_threads)
	:queue_(n_threads) {
		for(int i=0;i<n_threads;i++){
			Thread *t=new Thread(&ThreadPool::Worker,this);
			workers_.push_back(t);
		}
	}

	void StartWorkers() {
		for(int i=0;i<workers_.size();i++)
			workers_[i]->Start();
	}

	void Add(Closure *closure) {
		queue_.Produce(closure);
	}
	//wait workers to finish,then join all threads
	~ThreadPool() {
		for(int i=0;i<workers_.size();i++)
			Add(NULL);
		for(int i=0;i<workers_.size();i++) {
			workers_[i]->Join();
			delete workers_[i];
		}
	}
private:
	std::vector<Thread *> workers_;
	ProducerConsumerQueue queue_;

	static void *Worker(void *p) {
		ThreadPool *pool=reinterpret_cast<ThreadPool *>(p);
		while(true) {
			Closure *closure=reinterpret_cast<Closure*>(pool->queue_.Consume());
			if(closure==NULL)
				return NULL;
			closure->Execute();
		}
	}
};

// This class does not implement a signal-wait synchronization
// primitive, even if it looks like one. Its purpose is to enforce an
// order of execution of threads in unit tests in a way that is
// invisible to ThreadSanitizer and similar tools. It lacks memory
// barriers, therefore it only works reliably if there is a real
// synchronization primitive before signal() or after wait().
class StealthNotification {
public:
	StealthNotification():flag_(0) {}
	void signal() { flag_=1; }
	void wait() {
		while(!flag_)
			sched_yield();
	}
private:
	volatile int flag_;
};


//helpful to macro
typedef void* (*HANDLER)(void *);

pthread_mutex_t m=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m1=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m2=PTHREAD_MUTEX_INITIALIZER;

pthread_rwlock_t rwlock=PTHREAD_RWLOCK_INITIALIZER;
//condvar
pthread_cond_t cv=PTHREAD_COND_INITIALIZER; 

//semaphore
sem_t p_sem1;
sem_t p_sem2;
sem_t c_sem1;
sem_t c_sem2;

//barrier
pthread_barrier_t b;

#define PTHREAD_ID_1 pthread_t id1
#define PTHREAD_ID_2 PTHREAD_ID_1, id2
#define PTHREAD_ID_3 PTHREAD_ID_2, id3
#define PTHREAD_ID_4 PTHREAD_ID_3, id4
#define PTHREAD_ID(num) PTHREAD_ID_##num

#define PTHREAD_CREATE_1 pthread_create(&id1,NULL,(HANDLER)Worker1,NULL)
#define PTHREAD_CREATE_2 PTHREAD_CREATE_1;									\
	pthread_create(&id2,NULL,(HANDLER)Worker2,NULL)
#define PTHREAD_CREATE_3 PTHREAD_CREATE_2;									\
	pthread_create(&id3,NULL,(HANDLER)Worker3,NULL)
#define PTHREAD_CREATE_4 PTHREAD_CREATE_3;									\
	pthread_create(&id4,NULL,(HANDLER)Worker4,NULL)
#define PTHREAD_CREATE(num) PTHREAD_CREATE_##num

#define PTHREAD_JOIN_1 pthread_join(id1,NULL)
#define PTHREAD_JOIN_2 PTHREAD_JOIN_1;pthread_join (id2,NULL)
#define PTHREAD_JOIN_3 PTHREAD_JOIN_2;pthread_join (id3,NULL)
#define PTHREAD_JOIN_4 PTHREAD_JOIN_3;pthread_join (id4,NULL)
#define PTHREAD_JOIN(num) PTHREAD_JOIN_##num

#define PTHREAD_RUN(num) 													\
	PTHREAD_ID(num);     													\
	PTHREAD_CREATE(num); 													\
	PTHREAD_JOIN(num)


//semaphore
#define PC_SEM_INIT_1(val) sem_init(&p_sem1,0,val);sem_init(&c_sem1,0,0)
#define PC_SEM_INIT_2(val) PC_SEM_INIT_1(val); 								\
	sem_init(&p_sem2,0,val);sem_init(&c_sem2,0,0)
#define PC_SEM_INIT(num,val) PC_SEM_INIT_##num(val)

#define PC_SEM_DESTROY_1 sem_destroy(&p_sem1);sem_destroy(&c_sem1)
#define PC_SEM_DESTROY_2 													\
	PC_SEM_DESTROY_1;sem_destroy(&p_sem2);sem_destroy(&c_sem2)
#define PC_SEM_DESTROY(num) PC_SEM_DESTROY_##num

#define PC_SEM_RUN(num,t_num,val) 											\
	PC_SEM_INIT(num,val); 													\
	PTHREAD_RUN(t_num); 													\
	PC_SEM_DESTROY(num)
//use semaphore simulate producer-consumer(doesn't consider pool)
#define PC_SEM_PRODUCE(num) sem_wait(&p_sem##num);sem_post(&c_sem##num)
#define PC_SEM_CONSUME(num) sem_wait(&c_sem##num);sem_post(&p_sem##num)

//condvar
#define CV_SIGNAL(val) 														\
	pthread_mutex_lock(&m);													\
	cond=val;																\
	pthread_cond_signal(&cv);												\
	pthread_mutex_unlock(&m)

#define CV_SIGNAL_ALL(val) 													\
	pthread_mutex_lock(&m);													\
	cond=val;																\
	pthread_cond_broadcast(&cv);											\
	pthread_mutex_unlock(&m)

#define CV_WAIT(condition)													\
	pthread_mutex_lock(&m);													\
	while(cond!=condition)													\
		pthread_cond_wait(&cv,&m);											\
	pthread_mutex_unlock(&m)


#define CV_SIGNAL_COND(cond,val) 											\
	pthread_mutex_lock(&m);													\
	cond=val;																\
	pthread_cond_signal(&cv);												\
	pthread_mutex_unlock(&m)

#define CV_SIGNAL_ALL_COND(cond,val) 										\
	pthread_mutex_lock(&m);													\
	cond=val;																\
	pthread_cond_broadcast(&cv);											\
	pthread_mutex_unlock(&m)


#define CV_WAIT_COND(cond,condition)										\
	pthread_mutex_lock(&m);													\
	while(cond!=condition)													\
		pthread_cond_wait(&cv,&m);											\
	pthread_mutex_unlock(&m)


#define CV_SIGNAL_COND_MUTEX(cond,mutex,val)								\
	pthread_mutex_lock(&mutex);												\
	cond=val;																\
	pthread_cond_signal(&cv);												\
	pthread_mutex_unlock(&mutex)

#define CV_WAIT_COND_MUTEX(cond,mutex,condition)							\
	pthread_mutex_lock(&mutex);												\
	while(cond!=condition)													\
		pthread_cond_wait(&cv,&m);											\
	pthread_mutex_unlock(&mutex)


#define ATOMIC_INCREMENT(ptr,val) __sync_add_and_fetch((ptr),val)
#define ATOMIC_LOAD(ptr) *((volatile int *)(ptr))

#define REGISTER_TEST(f,id) TestAdder add_test##id (f,id)
#endif