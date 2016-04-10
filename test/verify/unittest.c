#include "unittest.h"
#include "thread_wrappers_pthread.h"
#include "lf_queue.h"
#include <dirent.h>
#include <sys/mman.h>
#include <time.h>
// #include "wrappers.h"


// test0:None
static int glob_0=0;
static void work_0() {
	return ;
}
static void run_0() {
	PTHREAD_RUN(1,work_0);
}

// test1:TP.Simple race(write vs write).
static int glob_1=0;
static void work1_1() {
	glob_1=1;
}
static void work1_2() {
	glob_1=2;
}
static void run_1() {
	PTHREAD_RUN(2,work1_1,work1_2);
}

// test2:TN.Synchronization via condvar
static int glob_2=0;
static int cond_2=0;
// Two write accesses to GLOB are synchronized because
// the pair of CV.Signal() and CV.Wait() establish happens-before relation.
//
// Waiter:                      Waker:
// 1. COND = 0
// 2. Start(Waker)
// 3. MU.Lock()                 a. write(GLOB)
//                              b. MU.Lock()
//                              c. COND = 1
//                         /--- d. CV.Signal()
//  4. while(COND)        /     e. MU.Unlock()
//       CV.Wait(MU) <---/
//  5. MU.Unlock()
//  6. write(GLOB)
static void waker_2() {
	sleep(2);
	glob_2=1;
	CV_SIGNAL_COND(cond_2,1); 
}
static void waiter_2() {
	CV_WAIT_COND(cond_2,1);
	glob_2=2;
}
static void run_2() {
	PTHREAD_RUN(2,waker_2,waiter_2);
}

// test3:TP.Synchronization via LockWhen, signaller gets there first. 
static int glob_3=0;
static int cond_3=0;
static Lock lock_3;
// Two write accesses to GLOB are synchronized via conditional critical section.
// Note that LockWhen() happens first (we use sleep(1) to make sure)!
//
// Waiter:                           Waker:
// 1. COND = 0
// 2. Start(Waker)
//                                   a. write(GLOB)
//                                   b. MU.Lock()
//                                   c. COND = 1
//                              /--- d. MU.Unlock()
// 3. MU.LockWhen(COND==1) <---/
// 4. MU.Unlock()
// 5. write(GLOB)
static void waker_3() {
	// sleep(1); //if not sleep, then cond_3 will be written to 0 by waiter
	glob_3=1;
	lock(&lock_3);
	cond_3=1;
	unlock(&lock_3);
}
static void waiter_3() {
	// cond_3=0;
	lock_when_unlock(&lock_3,arg_is_one,&cond_3);
	
	glob_3=2;
}
static void run_3() {
	LOCK_INIT(1,lock_3); 
	PTHREAD_RUN(2,waker_3,waiter_3);
}

// test04: TN. Synchronization via PCQ
static int glob_4=0;
// Two write accesses to GLOB are separated by PCQ Put/Get.
//
// Putter:                        Getter:
// 1. write(GLOB)
// 2. Q.Put() ---------\          .
//                      \-------> a. Q.Get()
//                                b. write(GLOB)
//
//use semephore to simulate consumer-producer
static void putter_4() {
	glob_4=1;
	PC_SEM_PRODUCE(1);
}
static void getter_4() {
	PC_SEM_CONSUME(1);
	glob_4=2;
}
static void run_4() {
	PC_SEM_RUN(1,2,1,putter_4,getter_4);
}

// test6:TN.Synchronization via condvar,but waker gets there first
static int glob_6=0;
static int cond_6=0;
//
// Waiter:                                            Waker:
// 1. COND = 0
// 2. Start(Waker)
// 3. MU.Lock()                                       a. write(GLOB)
//                                                    b. MU.Lock()
//                                                    c. COND = 1
//                                           /------- d. CV.Signal()
//  4. while(COND)                          /         e. MU.Unlock()
//       CV.Wait(MU) <<< not called        /
//  6. ANNOTATE_CONDVAR_WAIT(CV, MU) <----/
//  5. MU.Unlock()
//  6. write(GLOB)
static void waker_6() {
	glob_6=1;
	CV_SIGNAL_COND(cond_6,1);
}
static void waiter_6() {
	sleep(1);
	CV_WAIT_COND(cond_6,1);
	glob_6=2;
}
static void run_6() {
	PTHREAD_RUN(2,waker_6,waiter_6);
}

// test07: TN. Synchronization via LockWhen(), Signaller is observed first.
static int glob_7=0;
static bool cond_7=false;
static Lock lock_7;
// Two write accesses to GLOB are synchronized via conditional critical section.
// LockWhen() is observed after COND has been set (due to sleep).
// Unlock() calls ANNOTATE_CONDVAR_SIGNAL().
//
// Waiter:                           Signaller:
// 1. COND = 0
// 2. Start(Signaller)
//                                   a. write(GLOB)
//                                   b. MU.Lock()
//                                   c. COND = 1
//                              /--- d. MU.Unlock calls ANNOTATE_CONDVAR_SIGNAL
// 3. MU.LockWhen(COND==1) <---/
// 4. MU.Unlock()
// 5. write(GLOB)
static void signaller_7() {
	glob_7=1;
	lock(&lock_7);
	cond_7=true;
	unlock(&lock_7);
}
static void waiter_7() {
	cond_7=false;
	Thread thread;
	thread_init(&thread,signaller_7,NULL);
	thread_start(&thread);
	sleep(1); // Make sure the signaller gets there first.

	lock_when_unlock(&lock_7,arg_is_true,&cond_7);

	glob_7=2;
	thread_join(&thread);
}
static void run_7() {
	LOCK_INIT(1,lock_7);
	PTHREAD_RUN(1,waiter_7);
}
// test8:TN.Synchronization via thread start/join
static int glob_8=0;
// Three accesses to GLOB are separated by thread start/join.
//
// Parent:                        Worker:
// 1. write(GLOB)
// 2. Start(Worker) ------------>
//                                a. write(GLOB)
// 3. Join(Worker) <------------
// 4. write(GLOB)
static void worker_8() {
	glob_8=2;
}
static void parent_8() {
	glob_8=1;
	PTHREAD_RUN(1,worker_8);
	glob_8=3;
}
static void run_8() {
	parent_8();
}

// test9:TP.Simple race (read vs write).
static int glob_9=0;
// A simple data race between writer and reader.
// Write happens after read (enforced by sleep).
// Usually, easily detectable by a race detector.
static void writer_9() {
	sleep(1);
	glob_9=1;
}
static void reader_9() {
	assert(glob_9!=-777);
}
static void run_9() {
	PTHREAD_RUN(2,writer_9,reader_9);
}

// test10:TP.Simple race (write vs read).
static int glob_10=0;
// A simple data race between writer and reader.
// Write happens before Read (enforced by sleep),
// otherwise this test is the same as test09.
//
// Writer:                    Reader:
// 1. write(GLOB)             a. sleep(long enough so that GLOB
//                                is most likely initialized by Writer)
//                            b. read(GLOB)
//
//
// Eraser algorithm does not detect the race here,
// see Section 2.2 of http://citeseer.ist.psu.edu/savage97eraser.html.
//
static void writer_10() {
	glob_10=1;
}
static void reader_10() {
	sleep(1);
	assert(glob_10!=-777);
}
static void run_10() {
	PTHREAD_RUN(2,writer_10,reader_10);
}

// test11: FP. Synchronization via CondVar, 2 workers.
static int glob_11=0;
static int cond_11=0;
static Lock lock_11;
// This test is properly synchronized, but currently (Dec 2007)
// helgrind reports a false positive.
//
// Parent:                              Worker1, Worker2:
// 1. Start(workers)                    a. read(GLOB)
// 2. MU.Lock()                         b. MU.Lock()
// 3. while(COND != 2)        /-------- c. CV.Signal()
//      CV.Wait(&MU) <-------/          d. MU.Unlock()
// 4. MU.Unlock()
// 5. write(GLOB)
//
static void worker_11() {
	usleep(100000);
	assert(glob_11!=777);
	CV_SIGNAL_COND(cond_11,cond_11+1);
}
static void parent_11() {
	cond_11=0;
	int i;
	Thread thread[2];
	for(i=0;i<2;i++)
		thread_init(&thread[i],worker_11,NULL);
	for(i=0;i<2;i++)
		thread_start(&thread[i]);
	CV_WAIT_COND(cond_11,2);
	glob_11=2;
	for(i=0;i<2;i++)
		thread_join(&thread[i]);
}
static void run_11() {
	LOCK_INIT(1,lock_11);
	PTHREAD_RUN(1,parent_11);
}

// test12:TN.Synchronization via Mutex, then via PCQ
static int glob_12=0;
static Lock lock_12;
// This test is properly synchronized, but currently
// helgrind reports a false positive.
//
// First, we write to GLOB under MU, then we synchronize via PCQ,
// which is essentially a semaphore.
//
// Putter:                       Getter:
// 1. MU.Lock()                  a. MU.Lock()
// 2. write(GLOB) <---- MU ----> b. write(GLOB)
// 3. MU.Unlock()                c. MU.Unlock()
// 4. Q.Put()   ---------------> d. Q.Get()
//                               e. write(GLOB)

//use semephore to simulate consumer-producer
static void putter_12() {
	lock(&lock_12);
	glob_12++;
	unlock(&lock_12);
	PC_SEM_PRODUCE(1);
}
static void getter_12() {
	lock(&lock_12);
	glob_12++;
	unlock(&lock_12);
	PC_SEM_CONSUME(1);glob_12++;
}
static void run_12() {
	LOCK_INIT(1,lock_12);
	PC_SEM_RUN(1,2,1,putter_12,getter_12);
}

// test13:TN.Synchronization via Mutex, then via LockWhen
static int glob_13=0;
static int cond_13=0;
static Lock lock_13;
// This test is essentially the same as test12, but uses LockWhen
// instead of PCQ.
//
// Waker:                                     Waiter:
// 1. MU.Lock()                               a. MU.Lock()
// 2. write(GLOB) <---------- MU ---------->  b. write(GLOB)
// 3. MU.Unlock()                             c. MU.Unlock()
// 4. MU.Lock()                               .
// 5. COND = 1                                .
// 6. ANNOTATE_CONDVAR_SIGNAL -------\        .
// 7. MU.Unlock()                     \       .
//                                     \----> d. MU.LockWhen(COND == 1)
//                                            e. MU.Unlock()
//                                            f. write(GLOB)
static void waker_13() {
	lock(&lock_13);
	glob_13++;
	unlock(&lock_13);
	lock(&lock_13);
	cond_13=1;
	unlock(&lock_13);
}
static void waiter_13() {
	lock(&lock_13);
	glob_13++;
	unlock(&lock_13);
	lock_when_unlock(&lock_13,arg_is_one,&cond_13);
	glob_13++;
}
static void run_13() {
	LOCK_INIT(1,lock_13);
	PTHREAD_RUN(2,waiter_13,waker_13);
}

// test14:TN.Synchronization via PCQ, reads, 2 workers.
static int glob_14=0;
// This test is properly synchronized, but currently (Dec 2007)
// helgrind reports a false positive.
//
// This test is similar to test11, but uses PCQ (semaphore).
//
// Putter2:                  Putter1:                     Getter:
// 1. read(GLOB)             a. read(GLOB)
// 2. Q2.Put() ----\         b. Q1.Put() -----\           .
//                  \                          \--------> A. Q1.Get()
//                   \----------------------------------> B. Q2.Get()
//                                                        C. write(GLOB)

//use semephore to simulate consumer-producer
static void putter_14_1() {
	assert(glob_14!=-777);
	PC_SEM_PRODUCE(1);
}
static void putter_14_2() {
	assert(glob_14!=-777);
	PC_SEM_PRODUCE(2);
}
static void getter_14() {
	PC_SEM_CONSUME(1);
	PC_SEM_CONSUME(2);
	glob_14++;
}
static void run_14() {
	PC_SEM_RUN(2,3,1,putter_14_1,putter_14_2,getter_14);
}

// test16:TN.Barrier,2 threads
static int glob_16=0;
static Barrier barrier_16;
static void worker_16_1() {
	assert(glob_16!=-777);
	barrier_wait(&barrier_16);
}
static void worker_16_2() {
	assert(glob_16!=-777);
	barrier_wait(&barrier_16);
	glob_16++;
}
static void run_16() {
	BARRIER_RUN(2,barrier_16,2,worker_16_1,worker_16_2);
}

// test23:TN.TryLock, ReaderLock, ReaderTryLock
static int glob_23=0;
static RWLock rwlock_23;
static void worker_trylock_23() {
	int i;
	for(i=0;i<5;i++) {
		while(true) {
			if(rwlock_trywrlock(&rwlock_23)) {
				glob_23++;
				rwlock_unlock(&rwlock_23);
				break;
			}
			sleep(1);
		}
	}
}
static void worker_tryreadlock_23() {
	int i;
	for(i=0;i<5;i++) {
		while(true) {
			if(rwlock_tryrdlock(&rwlock_23)) {
				assert(glob_23!=-777);
				rwlock_unlock(&rwlock_23);
				break;
			}
			sleep(1);
		}
	}	
}
static void worker_lock_23() {
	int i;
	for(i=0;i<5;i++) {
		rwlock_wrlock(&rwlock_23);
		glob_23++;
		rwlock_unlock(&rwlock_23);
		sleep(1);
	}	
}
static void worker_readlock_23() {
	int i;
	for(i=0;i<5;i++) {
		rwlock_rdlock(&rwlock_23);
		assert(glob_23!=-777);
		rwlock_unlock(&rwlock_23);
		sleep(1);
	}		
}
static void run_23() {
	RWLOCK_INIT(1,rwlock_23);
	PTHREAD_RUN(4,worker_trylock_23,worker_tryreadlock_23,
		worker_lock_23,worker_readlock_23);
}

// test28:TN.Synchronization via Mutex, then PCQ. 3 threads
static int glob_28=0;
static Lock lock_28;
// Putter1:                       Getter:                         Putter2:
// 1. MU.Lock()                                                   A. MU.Lock()
// 2. write(GLOB)                                                 B. write(GLOB)
// 3. MU.Unlock()                                                 C. MU.Unlock()
// 4. Q.Put() ---------\                                 /------- D. Q.Put()
// 5. MU.Lock()         \-------> a. Q.Get()            /         E. MU.Lock()
// 6. read(GLOB)                  b. Q.Get() <---------/          F. read(GLOB)
// 7. MU.Unlock()                   (sleep)                       G. MU.Unlock()
//                                c. read(GLOB)

//use semaphore to simulate PCQ
static void putter_28() {
	lock(&lock_28);
	glob_28=1;
	unlock(&lock_28);
	PC_SEM_PRODUCE(1);
	lock(&lock_28);
	assert(glob_28!=-777);
	unlock(&lock_28);
}
static void getter_28() {
	PC_SEM_CONSUME(1);
	PC_SEM_CONSUME(1);
	sleep(1);
	assert(glob_28!=-777);
}
static void run_28() {
	LOCK_INIT(1,lock_28);
	PC_SEM_RUN(1,3,2,putter_28,putter_28,getter_28);
}

// test29:TN.Synchronization via Mutex, then PCQ. 4 threads.
static int glob_29=0;
static Lock lock_29;
static void putter_29(int num) {
	lock(&lock_29);
	glob_29++;
	unlock(&lock_29);
	if(num==0) {
		PC_SEM_PRODUCE(1);
		PC_SEM_PRODUCE(1);
	}
	else {
		PC_SEM_PRODUCE(2);
		PC_SEM_PRODUCE(2);
	}
	lock(&lock_29);
	assert(glob_29!=-777);
	unlock(&lock_29);
}
static void putter_29_1() {
	putter_29(0);
}
static void putter_29_2() {
	putter_29(1);
}
static void getter_29() {
	PC_SEM_CONSUME(1);
	PC_SEM_CONSUME(2);
	sleep(1);
	assert(glob_29==2);
}
static void run_29() {
	LOCK_INIT(1,lock_29);
	PC_SEM_RUN(2,4,2,putter_29_1,putter_29_2,getter_29,
		getter_29);
}

// test32:TN.Synchronization via thread create/join. W/R.
static int glob_32=0;
static Lock lock_32;
// This test is well synchronized but helgrind 3.3.0 reports a race.
//
// Parent:                   Writer:               Reader:
// 1. Start(Reader) -----------------------\       .
//                                          \      .
// 2. Start(Writer) ---\                     \     .
//                      \---> a. MU.Lock()    \--> A. sleep(long enough)
//                            b. write(GLOB)
//                      /---- c. MU.Unlock()
// 3. Join(Writer) <---/
//                                                 B. MU.Lock()
//                                                 C. read(GLOB)
//                                   /------------ D. MU.Unlock()
// 4. Join(Reader) <----------------/
// 5. write(GLOB)
//
//
// The call to sleep() in Reader is not part of synchronization,
// it is required to trigger the false positive in helgrind 3.3.0.
//
static void writer_32() {
	lock(&lock_32);
	glob_32=1;
	unlock(&lock_32);
}
static void reader_32() {
	usleep(480000);
	lock(&lock_32);
	assert(glob_32!=-777);
	unlock(&lock_32);
}
static void parent_32() {
	LOCK_INIT(1,lock_32);
	PTHREAD_RUN(2,writer_32,reader_32);
	glob_32=2;
}
static void run_32() {
	parent_32();
}

// test36:TN.Synchronization via Mutex, then PCQ. 3 threads.
static int glob_36=0;
static Lock lock_36_1;
static Lock lock_36_2;
// variation of test28 (W/W instead of W/R)
// Putter1:                       Getter:                         Putter2:
// 1. MU.Lock();                                                  A. MU.Lock()
// 2. write(GLOB)                                                 B. write(GLOB)
// 3. MU.Unlock()                                                 C. MU.Unlock()
// 4. Q.Put() ---------\                                 /------- D. Q.Put()
// 5. MU1.Lock()        \-------> a. Q.Get()            /         E. MU1.Lock()
// 6. MU.Lock()                   b. Q.Get() <---------/          F. MU.Lock()
// 7. write(GLOB)                                                 G. write(GLOB)
// 8. MU.Unlock()                                                 H. MU.Unlock()
// 9. MU1.Unlock()                  (sleep)                       I. MU1.Unlock()
//                                c. MU1.Lock()
//                                d. write(GLOB)
//                                e. MU1.Unlock()
//
static void putter_36() {
	lock(&lock_36_1);
	glob_36=1;
	unlock(&lock_36_1);
	PC_SEM_PRODUCE(1);
	lock(&lock_36_1);
	lock(&lock_36_2);
	glob_36=1;
	unlock(&lock_36_2);
	unlock(&lock_36_1);
}
static void getter_36() {
	PC_SEM_CONSUME(1);
	PC_SEM_CONSUME(1);
	usleep(100000);
	lock(&lock_36_1);
	glob_36++;
	unlock(&lock_36_1);
}
static void run_36() {
	LOCK_INIT(2,lock_36_1,lock_36_2);
	PC_SEM_RUN(1,3,2,putter_36,putter_36,getter_36);
}

// test37:TN.Simple synchronization (write vs read).
static int glob_37=0;
static Lock lock_37;
// Similar to test10, but properly locked.
// Writer:             Reader:
// 1. MU.Lock()
// 2. write
// 3. MU.Unlock()
//                    a. MU.Lock()
//                    b. read
//                    c. MU.Unlock();
//
static void writer_37() {
	lock(&lock_37);
	glob_37=3;
	unlock(&lock_37);
}
static void reader_37() {
	usleep(100000);
	lock(&lock_37);
	assert(glob_37!=-777);
	unlock(&lock_37);
}
static void run_37() {
	LOCK_INIT(1,lock_37);
	PTHREAD_RUN(2,writer_37,reader_37);
}

// test38:TN.Synchronization via Mutexes and PCQ. 4 threads. W/W
static int glob_38=0;
static Lock lock_38_1;
static Lock lock_38_2;
// Putter1:            Putter2:           Getter1:       Getter2:
//    MU1.Lock()          MU1.Lock()
//    write(GLOB)         write(GLOB)
//    MU1.Unlock()        MU1.Unlock()
//    Q1.Put()            Q2.Put()
//    Q1.Put()            Q2.Put()
//    MU1.Lock()          MU1.Lock()
//    MU2.Lock()          MU2.Lock()
//    write(GLOB)         write(GLOB)
//    MU2.Unlock()        MU2.Unlock()
//    MU1.Unlock()        MU1.Unlock()     sleep          sleep
//                                         Q1.Get()       Q1.Get()
//                                         Q2.Get()       Q2.Get()
//                                         MU2.Lock()     MU2.Lock()
//                                         write(GLOB)    write(GLOB)
//                                         MU2.Unlock()   MU2.Unlock()
//
static void putter_38(int num) {
	lock(&lock_38_1);
	glob_38++;
	unlock(&lock_38_1);
	if(num==0) {
		PC_SEM_PRODUCE(1);
		PC_SEM_PRODUCE(1);
	}
	else {
		PC_SEM_PRODUCE(2);
		PC_SEM_PRODUCE(2);
	}
	lock(&lock_38_1);
	lock(&lock_38_2);
	assert(glob_38!=-777);
	unlock(&lock_38_2);
	unlock(&lock_38_1);
}
static void putter_38_1() {
	putter_38(0);
}
static void putter_38_2() {
	putter_38(1);
}
static void getter_38() {
	usleep(100000);
	PC_SEM_CONSUME(1);
	PC_SEM_CONSUME(2);
	lock(&lock_38_2);
	glob_38++;
	unlock(&lock_38_2);
}
static void run_38() {
	LOCK_INIT(2,lock_38_1,lock_38_2);
	PC_SEM_RUN(2,4,2,putter_38_1,putter_38_2,getter_38,getter_38);
}

// test40: TN.Synchronization via Mutexes and PCQ. 4 threads. W/W
static int glob_40=0;
static Lock lock_40_1;
static Lock lock_40_2;
// Similar to test38 but with different order of events (due to sleep).
// Putter1:            Putter2:           Getter1:       Getter2:
//    MU1.Lock()          MU1.Lock()
//    write(GLOB)         write(GLOB)
//    MU1.Unlock()        MU1.Unlock()
//    Q1.Put()            Q2.Put()
//    Q1.Put()            Q2.Put()
//                                        Q1.Get()       Q1.Get()
//                                        Q2.Get()       Q2.Get()
//                                        MU2.Lock()     MU2.Lock()
//                                        write(GLOB)    write(GLOB)
//                                        MU2.Unlock()   MU2.Unlock()
//
//    MU1.Lock()          MU1.Lock()
//    MU2.Lock()          MU2.Lock()
//    write(GLOB)         write(GLOB)
//    MU2.Unlock()        MU2.Unlock()
//    MU1.Unlock()        MU1.Unlock()
//
static void putter_40(int num) {
	lock(&lock_40_1);
	glob_40++;
	unlock(&lock_40_1);
	if(num==0) {
		PC_SEM_PRODUCE(1);
		PC_SEM_PRODUCE(1);
	}
	else {
		PC_SEM_PRODUCE(2);
		PC_SEM_PRODUCE(2);
	}
	usleep(100000);
	lock(&lock_40_1);
	lock(&lock_40_2);
	assert(glob_40!=-777);
	unlock(&lock_40_2);
	unlock(&lock_40_1);
}
static void putter_40_1() {
	putter_40(0);
}
static void putter_40_2() {
	putter_40(1);
}
static void getter_40() {
	PC_SEM_CONSUME(1);
	PC_SEM_CONSUME(2);
	lock(&lock_40_1);
		glob_40++;
	unlock(&lock_40_1);
}
static void run_40() {
	LOCK_INIT(2,lock_40_1,lock_40_2);
	PC_SEM_RUN(2,4,2,putter_40_1,putter_40_2,getter_40,getter_40);
}

// test42:Using the same cond var several times.
static int glob_42=0;
static int cond_42=0;
static void worker_42_1() {
	glob_42=1;
	CV_SIGNAL_COND(cond_42,1);
	CV_WAIT_COND(cond_42,0);
	glob_42=3;
}
static void worker_42_2() {
	CV_WAIT_COND(cond_42,1);
	glob_42=2;
	CV_SIGNAL_COND(cond_42,0);
}
static void run_42() {
	PTHREAD_RUN(2,worker_42_1,worker_42_2);
}

// test43:TN.
static int glob_43=0;
// Putter:            Getter:
// 1. write
// 2. Q.Put() --\     .
// 3. read       \--> a. Q.Get()
//                    b. read
//
static void putter_43() {
	glob_43=1;
	PC_SEM_PRODUCE(1);
	assert(glob_43==1);
}
static void getter_43() {
	PC_SEM_CONSUME(1);
	usleep(100000);
	assert(glob_43==1);
}
static void run_43() {
	PC_SEM_RUN(1,2,1,putter_43,getter_43);
}

// test44:TN.
static int glob_44=0;
static Lock lock_44;
// Putter:            Getter:
// 1. read
// 2. Q.Put() --\     .
// 3. MU.Lock()  \--> a. Q.Get()
// 4. write
// 5. MU.Unlock()
//                    b. MU.Lock()
//                    c. write
//                    d. MU.Unlock();
//
static void putter_44() {
	assert(glob_44==0);
	PC_SEM_PRODUCE(1);
	lock(&lock_44);
	glob_44=1;
	unlock(&lock_44);
}
static void getter_44() {
	PC_SEM_CONSUME(1);
	usleep(100000);
	lock(&lock_44);
	glob_44=1;
	unlock(&lock_44);
}
static void run_44() {
	LOCK_INIT(1,lock_44);
	PC_SEM_RUN(1,2,1,putter_44,getter_44);
}

// test45:TN.
static int glob_45=0;
static Lock lock_45;
// Putter:            Getter:
// 1. read
// 2. Q.Put() --\     .
// 3. MU.Lock()  \--> a. Q.Get()
// 4. write
// 5. MU.Unlock()
//                    b. MU.Lock()
//                    c. read
//                    d. MU.Unlock();
//
static void putter_45() {
	assert(glob_45==0);
	PC_SEM_PRODUCE(1);
	lock(&lock_45);
	glob_45=1;
	unlock(&lock_45);
}
static void getter_45() {
	PC_SEM_CONSUME(1);
	usleep(100000);
	lock(&lock_45);
	assert(glob_45<=1);
	unlock(&lock_45);
}
static void run_45() {
	LOCK_INIT(1,lock_45);
	PC_SEM_RUN(1,2,1,putter_45,getter_45);
}

// test46:TP
static int glob_46=0;
static Lock lock_46;
// First:                             Second:
// 1. write
// 2. MU.Lock()
// 3. write
// 4. MU.Unlock()                      (sleep)
//                                    a. MU.Lock()
//                                    b. write
//                                    c. MU.Unlock();
//
static void worker_46_1() {
	glob_46++;
	lock(&lock_46);
	glob_46++;
	unlock(&lock_46);
}
static void worker_46_2() {
	usleep(480000);
	lock(&lock_46);
	glob_46++;
	unlock(&lock_46);
}
static void run_46() {
	LOCK_INIT(1,lock_46);
	PTHREAD_RUN(2,worker_46_1,worker_46_2);
}

// test47:TP.Not detected by pure happens-before detectors.
static int glob_47=0;
static Lock lock_47;
// A true race that can not be detected by a pure happens-before
// race detector.
//
// First:                             Second:
// 1. write
// 2. MU.Lock()
// 3. MU.Unlock()                      (sleep)
//                                    a. MU.Lock()
//                                    b. MU.Unlock();
//                                    c. write
//
static void worker_47_1() {
	glob_47=1;
	lock(&lock_47);
	unlock(&lock_47);
}
static void worker_47_2() {
	usleep(480000);
	lock(&lock_47);
	unlock(&lock_47);
	glob_47++;
}
static void run_47() {
	LOCK_INIT(1,lock_47);
	PTHREAD_RUN(2,worker_47_1,worker_47_2);
}

// test48: TP.Simple race (single write vs multiple reads)
static int glob_48=0;
// same as test10 but with single writer and  multiple readers
// A simple data race between single writer and  multiple readers.
// Write happens before Reads (enforced by sleep(1)),

//
// Writer:                    Readers:
// 1. write(GLOB)             a. sleep(long enough so that GLOB
//                                is most likely initialized by Writer)
//                            b. read(GLOB)
//
//
// Eraser algorithm does not detect the race here,
// see Section 2.2 of http://citeseer.ist.psu.edu/savage97eraser.html.
//
static void writer_48() {
	glob_48=2;
}
static void reader_48() {
	usleep(10000);
	assert(glob_48!=-777);
}
static void run_48() {
	PTHREAD_RUN(3,writer_48,reader_48,reader_48);
}
// test49:TP.Simple race (single write vs multiple reads).
static int glob_49=0;
// same as test10 but with multiple read operations done by a single reader
// A simple data race between writer and readers.
// Write happens before Read (enforced by sleep(1)),
//
// Writer:                    Reader:
// 1. write(GLOB)             a. sleep(long enough so that GLOB
//                                is most likely initialized by Writer)
//                            b. read(GLOB)
//                            c. read(GLOB)
//                            d. read(GLOB)
//                            e. read(GLOB)
//
//
// Eraser algorithm does not detect the race here,
// see Section 2.2 of http://citeseer.ist.psu.edu/savage97eraser.html.
//
static void writer_49() {
	glob_49=1;
}
static void reader_49() {
	usleep(10000);
	assert(glob_49!=-777);
	assert(glob_49!=-777);
	assert(glob_49!=-777);
	assert(glob_49!=-777);
}
static void run_49() {
	PTHREAD_RUN(2,writer_49,reader_49);
}

// test50:TP.Synchronization via CondVar
static int glob_50=0;
static int cond_50=0;
static Lock lock_50;
// Two last write accesses to GLOB are not synchronized
//
// Waiter:                      Waker:
// 1. COND = 0
// 2. Start(Waker)
// 3. MU.Lock()                 a. write(GLOB)
//                              b. MU.Lock()
//                              c. COND = 1
//                         /--- d. CV.Signal()
//  4. while(COND != 1)   /     e. MU.Unlock()
//       CV.Wait(MU) <---/
//  5. MU.Unlock()
//  6. write(GLOB)              f. MU.Lock()
//                              g. write(GLOB)
//                              h. MU.Unlock()
//
static void waker_50() {
	usleep(10000);
	glob_50=1;
	CV_SIGNAL_COND(cond_50,1);
	usleep(10000);
	lock(&lock_50);
	glob_50=3;
	unlock(&lock_50);
}
static void waiter_50() {
	CV_WAIT_COND(cond_50,1);
	glob_50=2;
}
static void run_50() {
	LOCK_INIT(1,lock_50);
	PTHREAD_RUN(2,waker_50,waiter_50);
}

// test53:TN.
static int glob_53=0;
static bool flag_53=false;
static Lock lock_53_1,lock_53_2;
// Correctly synchronized test, but the common lockset is empty.
// The variable FLAG works as an implicit semaphore.
// MSMHelgrind still does not complain since it does not maintain the lockset
// at the exclusive state. But MSMProp1 does complain.
// See also test54.
//
//
// Initializer:                  Users
// 1. MU1.Lock()
// 2. write(GLOB)
// 3. FLAG = true
// 4. MU1.Unlock()
//                               a. MU1.Lock()
//                               b. f = FLAG;
//                               c. MU1.Unlock()
//                               d. if (!f) goto a.
//                               e. MU2.Lock()
//                               f. write(GLOB)
//                               g. MU2.Unlock()
//
static void initializer_53() {
	lock(&lock_53_1);
	glob_53=1;
	flag_53=true;
	unlock(&lock_53_1);
	usleep(10000);
}
static void user_53() {
	bool f=false;
	while(!f) {
		lock(&lock_53_1);
		f=flag_53;
		unlock(&lock_53_1);
		usleep(10000);
	}
	// at this point Initializer will not access GLOB again
	lock(&lock_53_2);
	assert(glob_53>=1);
	glob_53++;
	unlock(&lock_53_2);
}
static void run_53() {
	LOCK_INIT(2,lock_53_1,lock_53_2);
	PTHREAD_RUN(3,user_53,user_53,initializer_53);
}

// test57:TN.
static int glob_57=0;
static void writer_57() {
	int i;
	for(i=0;i<10;i++)
		ATOMIC_INCREMENT(&glob_57,1);
	usleep(1000);
}
static void reader_57() {
	while(ATOMIC_LOAD(&glob_57)<20)
		usleep(1000);
}
static void run_57() {
	PTHREAD_RUN(4,reader_57,reader_57,writer_57,writer_57);
	assert(glob_57==20);
}

// test58:TP.User defined synchronization.
static int glob_58_1=1;
static int glob_58_2=2;
static int flag_58_1=0;
static int flag_58_2=0;
// Correctly synchronized test, but the common lockset is empty.
// The variables FLAG1 and FLAG2 used for synchronization and as
// temporary variables for swapping two global values.
// Such kind of synchronization is rarely used (Excluded from all tests??).
static void worker_58_1() {
	flag_58_1=glob_58_2;
	while(!flag_58_2) ;
	glob_58_2=flag_58_2;
}
static void worker_58_2() {
	flag_58_2=glob_58_1;
	while(!flag_58_1) ;
	glob_58_1=flag_58_1;
}
static void run_58() {
	PTHREAD_RUN(2,worker_58_1,worker_58_2);
}

// test60:TN.Correct synchronization using signal-wait
static int glob_60_1=1;
static int glob_60_2=2;
static int cond_60_1=0;
static int cond_60_2=0;
static int flag_60_1=0;
static int flag_60_2=0;
// same as test 58 but synchronized with signal-wait.
static void worker_60_1() {
	flag_60_1=glob_60_2;
	CV_SIGNAL_COND(cond_60_1,1);
	CV_WAIT_COND(cond_60_2,1);
	glob_60_2=flag_60_2;
}
static void worker_60_2() {
	flag_60_2=glob_60_1;
	CV_SIGNAL_COND(cond_60_2,1);
	CV_WAIT_COND(cond_60_1,1);
	glob_60_1=flag_60_1;
}
static void run_60() {
	PTHREAD_RUN(2,worker_60_1,worker_60_2);
}

// test61:Tn.Synchronization via Mutex as in happens-before
static int glob_61=0;
static Lock lock_61;
static int *p1_61=NULL;
static int *p2_61=NULL;
// In this test Mutex lock/unlock operations introduce happens-before relation.
// We annotate the code so that MU is treated as in pure happens-before detector.
static void worker_61_1() {
	lock(&lock_61);
	if(p1_61==NULL) {
		p1_61=&glob_61;
		*p1_61=1;
	}
	unlock(&lock_61);
}
static void worker_61_2() {
	bool done=false;
	while(!done) {
		lock(&lock_61);
		if(p1_61) {
			done=true;
			p2_61=p1_61;
			p1_61=NULL;
		}
		unlock(&lock_61);
	}
	*p2_61=2;
}
static void run_61() {
	LOCK_INIT(1,lock_61);
	PTHREAD_RUN(2,worker_61_1,worker_61_2);
}

// test64:TP.T2 happens-before T3, but T1 is independent. Reads in T1/T2.
static int glob_64=0;
// True race between T1 and T3:
//
// T1:                   T2:                   T3:
// 1. read(GLOB)         (sleep)
//                       a. read(GLOB)
//                       b. Q.Put() ----->    A. Q.Get()
//                                            B. write(GLOB)
//
static void worker_64_1() {
	assert(glob_64!=-777);
}
static void worker_64_2() {
	usleep(10000);
	assert(glob_64!=-777);
	PC_SEM_PRODUCE(1);
}
static void worker_64_3() {
	PC_SEM_CONSUME(1);
	glob_64=1;
}
static void run_64() {
	PC_SEM_RUN(1,3,1,worker_64_1,worker_64_2,worker_64_3);
}

// test65:TP.T2 happens-before T3, but T1 is independent. Writes in T1/T2.
static int glob_65=0;
static Lock lock_65;
// Similar to test64.
// True race between T1 and T3:
//
// T1:                   T2:                   T3:
// 1. MU.Lock()
// 2. write(GLOB)
// 3. MU.Unlock()         (sleep)
//                       a. MU.Lock()
//                       b. write(GLOB)
//                       c. MU.Unlock()
//                       d. Q.Put() ----->    A. Q.Get()
//                                            B. write(GLOB)
//
static void worker_65_1() {
	lock(&lock_65);
	glob_65++;
	unlock(&lock_65);
}
static void worker_65_2() {
	usleep(10000);
	lock(&lock_65);
	glob_65++;
	unlock(&lock_65);
	PC_SEM_PRODUCE(1);
}
static void worker_65_3() {
	PC_SEM_CONSUME(1);
	glob_65=1;
}
static void run_65() {
	LOCK_INIT(1,lock_65);
	PC_SEM_RUN(1,3,1,worker_65_1,worker_65_2,worker_65_3);	
}

// test68: TP. Writes are protected by MU, reads are not.
static int glob_68=0;
static int cond_68=0;
static const int n_writers_68=3;
static Lock lock_68_1;
static Lock lock_68_2;
// In this test, all writes to GLOB are protected by a mutex
// but some reads go unprotected.
// This is certainly a race, but in some cases such code could occur in
// a correct program. For example, the unprotected reads may be used
// for showing statistics and are not required to be precise.
static void writer_68() {
	int i;
	for(i=0;i<100;i++) {
		lock(&lock_68_1);
		glob_68++;
		unlock(&lock_68_1);
	}
	//we are done
	lock(&lock_68_2);
	cond_68++;
	unlock(&lock_68_2);
}
static void reader_68() {
	bool cont=true;
	while(cont) {
		assert(glob_68>=0);
		//are we done?
		lock(&lock_68_2);
		if(cond_68==n_writers_68)
			cont=false;
		unlock(&lock_68_2);
		usleep(100);	
	}
}
static void run_68() {
	LOCK_INIT(2,lock_68_1,lock_68_2);
	PTHREAD_RUN(4,reader_68,writer_68,writer_68,writer_68);
}

// test75: TN. Test for sem_post, sem_wait, sem_trywait.
static int glob_75=0;
static void poster_75() {
	glob_75=1;
	SEM_POST_1();
	SEM_POST_2();
}
static void waiter_75() {
	SEM_WAIT_1();
	assert(glob_75==1);
}
static void trywait_75() {
	usleep(500000);
	SEM_TRYWAIT_2();
	assert(glob_75==1);
}
static void run_75() {
	SEM_INIT(2,0,0);
	Thread thread[2];
	thread_init(&thread[0],poster_75,NULL);
	thread_init(&thread[1],waiter_75,NULL);
	int i;
	for(i=0;i<2;i++)
		thread_start(&thread[i]);
	for(i=0;i<2;i++)
		thread_join(&thread[i]);
	
	glob_75=2;

	thread_init(&thread[0],poster_75,NULL);
	thread_init(&thread[1],trywait_75,NULL);
	for(i=0;i<2;i++)
		thread_start(&thread[i]);
	for(i=0;i<2;i++)
		thread_join(&thread[i]);
	SEM_DESTROY(2);
}

// test83:TN.Object published w/o synchronization (simple version)
static volatile int *ptr_83=NULL;
static Lock lock_83;
// A simplified version of test83 (example of a wrong code).
// This test, though incorrect, will almost never fail.	
static void writer_83() {
	usleep(1000);
	ptr_83=(int *)malloc(sizeof(*ptr_83));
	*ptr_83=777;
}
static void reader_83() {
	while(!ptr_83) {
		lock(&lock_83);
		unlock(&lock_83);
	}
	if(*ptr_83==777)
		printf("%d\n",*ptr_83);
	free((void *)ptr_83);
}
static void run_83() {
	LOCK_INIT(1,lock_83);
	PTHREAD_RUN(2,writer_83,reader_83);
}

// test84: TN. True race (regression test for a bug related to atomics)
static int x_84=0;
//dummy_84[] ensures that x_84 and y_84 are not in the same cache line.
static char dummy_84[512]={0};
static int y_84=0;
// Helgrind should not create HB arcs for the bus lock even when
// --pure-happens-before=yes is used.
// Bug found in by Bart Van Assche, the test is taken from
// valgrind file drd/tests/atomic_var.c.
static void worker_84_1() {
	y_84=1;
	ATOMIC_INCREMENT(&x_84,1);
}
static void worker_84_2() {
	while(ATOMIC_INCREMENT(&x_84,0)==0) ;
	assert(y_84>=0);
}
static void run_84() {
	assert(dummy_84[0]==0);
	PTHREAD_RUN(2,worker_84_1,worker_84_2);
}

// test89:
typedef struct STRUCT_89 {
	int a,b,c;
} STRUCT_89;
static int glob_89=0;
static int *stack_89=NULL;
static STRUCT_89 GLOB_STRUCT_89;
static STRUCT_89 *STACK_STRUCT_89=NULL;
static STRUCT_89 *HEAP_STRUCT_89=NULL;
// Simlpe races with different objects (stack, heap globals; scalars, structs).
// Also, if run with --trace-level=2 this test will show a sequence of
// CTOR and DTOR calls.
static void worker_89() {
	glob_89=1;
	*stack_89=1;
	GLOB_STRUCT_89.b=1;
	STACK_STRUCT_89->b=1;
	HEAP_STRUCT_89->b=1;
}
static void run_89() {
	int stack_var=0;
	stack_89=&stack_var;
	STRUCT_89 stack_struct;
	STACK_STRUCT_89=&stack_struct;
	HEAP_STRUCT_89=(STRUCT_89*)malloc(sizeof(*HEAP_STRUCT_89));
	PTHREAD_RUN(2,worker_89,worker_89);
	free(HEAP_STRUCT_89);
}

// test90: TN. Test for a safely-published pointer (read-only).
static int *glob_90=0;
static Lock lock_90;
static StealthNotify sn_90;
static void publisher_90() {
	lock(&lock_90);
	glob_90=(int *)malloc(128*sizeof(int));
	glob_90[42]=777;
	unlock(&lock_90);
	stealth_notify_signal(&sn_90);
	usleep(200000);
}
static void reader_90() {
	stealth_notify_wait(&sn_90);
	while(true) {
		lock(&lock_90);
		int *p=glob_90;
		unlock(&lock_90);
		if(p) {
			assert(p[42]==777);
			break;
		}
	}
	free(glob_90);
}
static void run_90() {
	LOCK_INIT(1,lock_90);
	STEALTH_NOTIFY_INIT(1,sn_90);
	PTHREAD_RUN(2,publisher_90,reader_90);
}

// test91: TN. Test for a safely-published pointer (read-write).
static int *glob_91=NULL;
static Lock lock_91_1;
static Lock lock_91_2;
// Similar to test90.
// The Publisher creates an object and safely publishes it under a mutex MU1.
// Accessors get the object under MU1 and access it (read/write) under MU2.
//
// Without annotations Helgrind will issue a false positive in Accessor().
//
static void publisher_91() {
	lock(&lock_91_1);
	glob_91=(int *)malloc(128 *sizeof(int));
	glob_91[42]=777;
	unlock(&lock_91_1);
}
static void accessor_91() {
	usleep(100000);
	while(true) {
		lock(&lock_91_1);
		int *p=glob_91;
		unlock(&lock_91_1);
		if(p) {
			lock(&lock_91_2);
			p[42]++;
			assert(p[42]>777);
			unlock(&lock_91_2);
			break;
		}
	}
	free(glob_91);
}
static void run_91() {
	LOCK_INIT(2,lock_91_1,lock_91_2);
	PTHREAD_RUN(2,publisher_91,accessor_91);
}

// test92: TN. Test for a safely-published pointer (read-write)
typedef struct ObjType {
  	int arr[10];
}ObjType;
static ObjType *glob_92=NULL;
static Lock lock_92_1;
static Lock lock_92_2;
static void publisher_92() {
	lock(&lock_92_1);
	glob_92=(ObjType *)malloc(sizeof(*glob_92));
	int i;
	for(i=0;i<10;i++)
		glob_92->arr[i]=777;
	unlock(&lock_92_1);
}
static void accessor_92(int index) {
	while(true) {
		lock(&lock_92_1);
		ObjType *p=glob_92;
		unlock(&lock_92_1);
		if(p) {
			lock(&lock_92_2);
			p->arr[index]++;
			assert(p->arr[index]==778);
			unlock(&lock_92_2);
			break;
		}
	}
}
static void accessor_92_1() {accessor_92(0);}
static void accessor_92_2() {accessor_92(5);}
static void accessor_92_3() {accessor_92(9);}
static void run_92() {
	LOCK_INIT(2,lock_92_1,lock_92_2);
	PTHREAD_RUN(4,accessor_92_1,accessor_92_2,
		accessor_92_3,publisher_92);
	free(glob_92);
}

// test94: TP. Check do_cv_signal/fake segment logic
static int glob_94=0;
static int cond_94_1=0, cond_94_2=0;
static Lock lock_94_1, lock_94_2;
static StealthNotify sn_94_1, sn_94_2, sn_94_3;
static CondVar cv_94_1, cv_94_2;
static void worker_94_1() {
	stealth_notify_wait(&sn_94_2); //make sure the waiter blocks
	glob_94=1;

	lock(&lock_94_1);
	cond_94_1=1;
	cond_signal(&cv_94_1);
	unlock(&lock_94_1);
	
	stealth_notify_signal(&sn_94_1);	
}
static void worker_94_2() {
	//make sure cv_94_2.signal() happends after cv_94_1.
	stealth_notify_wait(&sn_94_1);
	stealth_notify_wait(&sn_94_3);

	lock(&lock_94_2);
	cond_94_2=1;
	cond_signal(&cv_94_2);
	unlock(&lock_94_2);
}
static void worker_94_3() {
	lock(&lock_94_1);
	stealth_notify_signal(&sn_94_2);
	while(cond_94_1!=1)
		cond_wait(&cv_94_1,&lock_94_1);
	unlock(&lock_94_1);
}
static void worker_94_4() {
	lock(&lock_94_2);
	stealth_notify_signal(&sn_94_3);
	while(cond_94_2!=1)
		cond_wait(&cv_94_2,&lock_94_2);
	unlock(&lock_94_2);
	glob_94=2;
}
static void run_94() {
	LOCK_INIT(2,lock_94_1,lock_94_2);
	STEALTH_NOTIFY_INIT(3,sn_94_1,sn_94_2,sn_94_3);
	CV_INIT(2,cv_94_1,cv_94_2);
	PTHREAD_RUN(4,worker_94_1,worker_94_2,worker_94_3,
		worker_94_4);
}

// test95: TP. 
static int glob_95=0;
static int cond_95_1=0, cond_95_2=0;
static CondVar cv_95_1, cv_95_2;
static Lock lock_95_1, lock_95_2;
static void worker_95_1() {
	usleep(50000);
	glob_95=1;
	lock(&lock_95_1);
	cond_95_1=1;
	cond_signal(&cv_95_1);
	unlock(&lock_95_1);
}
static void worker_95_2() {
	usleep(10000);
	lock(&lock_95_2);
	cond_95_2=1;
	cond_signal(&cv_95_2);
	unlock(&lock_95_2);
}
static void worker_95_3() {
	lock(&lock_95_1);
	while(cond_95_1!=1)
		cond_wait(&cv_95_1,&lock_95_1);
	unlock(&lock_95_1);
}
static void worker_95_4() {
	lock(&lock_95_2);
	while(cond_95_2!=1)
		cond_wait(&cv_95_2,&lock_95_2);
	unlock(&lock_95_2);
	glob_95=2;
}
static void run_95() {
	LOCK_INIT(2,lock_95_1,lock_95_2);
	CV_INIT(2,cv_95_1,cv_95_2);
	PTHREAD_RUN(4,worker_95_1,worker_95_2,worker_95_3,
		worker_95_4);
}

// test96: TN. tricky LockSet behaviour
static int glob_96=0;
static Lock lock_96_1, lock_96_2, lock_96_3;
static void worker_96_1() {
	lock(&lock_96_1);
	lock(&lock_96_2);
	glob_96++;
	unlock(&lock_96_2);
	unlock(&lock_96_1);
}
static void worker_96_2() {
	lock(&lock_96_2);
	lock(&lock_96_3);
	glob_96++;
	unlock(&lock_96_3);
	unlock(&lock_96_2);
}
static void worker_96_3() {
	lock(&lock_96_1);
	lock(&lock_96_3);
	glob_96++;
	unlock(&lock_96_3);
	unlock(&lock_96_1);
}
static void run_96() {
	LOCK_INIT(3,lock_96_1,lock_96_2,lock_96_3);
	PTHREAD_RUN(3,worker_96_1,worker_96_2,worker_96_3);
}

// test100:Test for initialization bit.
static int glob_100_1=0,glob_100_2=0,glob_100_3=0,glob_100_4=0;
static void worker_100_1() {
	glob_100_1=1;assert(glob_100_1!=-1);
	glob_100_2=1;
	glob_100_3=1;assert(glob_100_3!=-1);
	glob_100_4=1;
}
static void worker_100_2() {
	usleep(100000);
	assert(glob_100_1!=-1);
	assert(glob_100_2!=-1);
	glob_100_3=1;
	glob_100_4=1;
}
static void worker_100_3() {
	usleep(100000);
	glob_100_1=1;
	glob_100_2=1;
	assert(glob_100_3!=-1);
	assert(glob_100_4!=-1);
}
static void run_100() {
	PTHREAD_RUN(3,worker_100_1,worker_100_2,worker_100_3);
}

// test101: TN. Two signals and two waits
static int glob_101=0;
static int cond_101_1=0,cond_101_2=0;
static void signaller_101() {
	//usleep(100000);
	CV_SIGNAL_COND(cond_101_1,1);
	glob_101=1;
	// usleep(500000);
	CV_SIGNAL_COND(cond_101_2,1);
}
static void waiter_101() {
	CV_WAIT_COND(cond_101_1,1);
	CV_WAIT_COND(cond_101_2,1);
	glob_101=2;
}
static void run_101() {
	PTHREAD_RUN(2,signaller_101,waiter_101);
}

// test102: TP.
#define ARRAY_SIZE_102 64
static int array_102[ARRAY_SIZE_102+1];
static int *glob_102=NULL;
static Lock lock_102;
static StealthNotify sn_102_1,sn_102_2,sn_102_3;
static Thread thread_102;
// We use sizeof(array) == 4 * HG_CACHELINE_SIZE to be sure that GLOB points
// to a memory inside a CacheLineZ which is inside array's memory range
static void reader_102() {
	stealth_notify_wait(&sn_102_1);
	assert(777==glob_102[1]);
    stealth_notify_signal(&sn_102_2);
    stealth_notify_wait(&sn_102_3);
    assert(777==glob_102[0]);
}
static void run_102() {
	glob_102=&array_102[ARRAY_SIZE_102/2];
	LOCK_INIT(1,lock_102);
	STEALTH_NOTIFY_INIT(3,sn_102_1,sn_102_2,sn_102_3);
	PTHREAD_INIT(1,thread_102,reader_102);
	thread_start(&thread_102);
	glob_102[1]=777;
    stealth_notify_signal(&sn_102_1);
    stealth_notify_wait(&sn_102_2); 
    glob_102[0]=777;
    stealth_notify_signal(&sn_102_3);
    thread_join(&thread_102);
}

// test104: TP. Simple race (write vs write). Heap mem.
static int *glob_104=NULL;
static Thread thread_104;
static void worker_104() { glob_104[42]=1; }
static void parent_104() {
	PTHREAD_INIT(1,thread_104,worker_104);
	thread_start(&thread_104);
	usleep(100000);
	glob_104[42]=2;
	thread_join(&thread_104);
}
static void run_104() {
	glob_104=(int *)malloc(128 *sizeof(int));
	glob_104[42]=0;
	parent_104();
	free(glob_104);
}

// test109: TN. Checking happens before between parent and child threads.
#define N_109 4
static int glob_109[N_109];
static void worker_109(void *a) {
	usleep(100000);
	int *arg=(int *)a;
	(*arg)++;
}
static void run_109() {
	Thread *t[N_109];
	int i;
	for(i=0;i<N_109;i++) {
		t[i]=(Thread *)malloc(sizeof(Thread));	
		thread_init(t[i],worker_109,&glob_109[i]);	
	}
	for(i=0;i<N_109;i++)
		thread_start(t[i]);
	for(i=0;i<N_109;i++)
		thread_join(t[i]);
	for(i=0;i<N_109;i++)
		free(t[i]);
}

// test111: TN. Unit test for a bug related to stack handling.
static char *glob_111=NULL;
static bool cond_111=false;
static Lock lock_111;
#define N_111 3000
static void write_to_p_111(char *p,int val) {
	int i;
	for(i=0;i<N_111;i++)
		p[i]=val;
}
static void f1_111() {
	char some_stack[N_111];
	write_to_p_111(some_stack,1);
	lock_when_unlock(&lock_111,arg_is_true,&cond_111);
}
static void f2_111() {
	char some_stack[N_111];
	char some_more_stack[N_111];
	write_to_p_111(some_stack,2);
	write_to_p_111(some_more_stack,2);
}
static void f0_111() { f2_111(); }
static void worker_111_1() {
	f0_111();
	f1_111();
	f2_111();
}
static void worker_111_2() {
	worker_111_1();
}
static void worker_111_3() {
	usleep(100000);
	lock(&lock_111);
	cond_111=true;
	unlock(&lock_111);
}
static void run_111() {
	LOCK_INIT(1,lock_111);
	PTHREAD_RUN(3,worker_111_1,worker_111_2,worker_111_3);
}

// test113: TN. A lot of lock/unlock calls.Many locks
#define kNumIter_113 1000
#define kNumLocks_113 5
static Lock lock_113[kNumLocks_113];
static void worker_113() {
	int i,j;
	for(i=0;i<kNumIter_113;i++) {
		for(j=0;j<kNumLocks_113;j++)
			if(i & (1<<j)) lock(&lock_113[j]);
		for(j=kNumLocks_113-1;j>-1;j--)
			if(i & (1<<j)) unlock(&lock_113[j]);
	}
}
static void run_113() {
	LOCK_INIT(5,lock_113[0],lock_113[1],lock_113[2],lock_113[3],lock_113[4]);
	PTHREAD_RUN(1,worker_113);
}

// test114: TN. Recursive static initialization
static int bar_114() {
	static int b_114=1;
	return b_114;
}
static int foo_114() {
	static int f_114;
	f_114=bar_114();
	return f_114;	
}
static void worker_114() {
	static int x_114;
	x_114=foo_114();
	assert(x_114!=-1);
}
static void run_114() {
	PTHREAD_RUN(2,worker_114,worker_114);
}

// test117: TN. Many calls to function-scope static inititialization
#define N_117 5
static int foo_117() {
	usleep(20000);
	return 1;
}
static void worker_117() {
	static int f_117;
	f_117=foo_117();
	assert(f_117!=-1);
}
static void run_117() {
	Thread *t[N_117];
	int i;
	for(i=0;i<N_117;i++) {
		t[i]=(Thread *)malloc(sizeof(Thread));	
		thread_init(t[i],worker_117,NULL);	
	}
	for(i=0;i<N_117;i++)
		thread_start(t[i]);
	for(i=0;i<N_117;i++)
		thread_join(t[i]);
	for(i=0;i<N_117;i++)
		free(t[i]);
}

// test119: TP. Testing that malloc does not introduce any HB arc.
static int glob_119=0;
static void worker_119_1() {
	glob_119=1;
	free(malloc(123));
}
static void worker_119_2() {
	usleep(100000);
	free(malloc(345));
	glob_119=2;
}
static void run_119() {
	PTHREAD_RUN(2,worker_119_1,worker_119_2);
}

// test120: TP. Thread1: write then read. Thread2: read.
static int glob_120=0;
static void worker_120_1() {
	glob_120=1;
	assert(glob_120!=-1);
}
static void worker_120_2() {
	usleep(100000);
	assert(glob_120!=-1);
}
static void run_120() {
	PTHREAD_RUN(2,worker_120_1,worker_120_2);
}

// test121: TN. DoubleCheckedLocking
typedef struct Foo_121 {
	uintptr_t padding1[16];
	volatile uintptr_t a;
	uintptr_t padding2[16];
} Foo_121;
static Foo_121 *foo_121;
static Lock lock_121;
static void initme_121() {
	if(!foo_121) {
		lock(&lock_121);
		if(!foo_121)
			foo_121=malloc(sizeof(*foo_121));
		foo_121->a=42;
		unlock(&lock_121);
	}
}
static void useme_121() {
	initme_121();
	assert(foo_121);
	if(foo_121->a!=42)
		printf("foo_121->a = %d (should be 42)\n",(int)foo_121->a);
}
static void worker_121_1() { useme_121(); }
static void worker_121_2() { useme_121(); }
static void worker_121_3() { useme_121(); }
static void run_121() {
	LOCK_INIT(1,lock_121);
	PTHREAD_RUN(3,worker_121_1,worker_121_2,worker_121_3);
	free(foo_121);
}

// test122: TN. DoubleCheckedLocking2
typedef struct Foo_122 {
	uintptr_t padding1[16];
	uintptr_t a;
	uintptr_t padding2[16];
}Foo_122;
static Foo_122 *foo_122;
static Lock lock_122;
static void initme_122() {
	if(foo_122) return ;
	Foo_122 *x=malloc(sizeof(*foo_122));
	x->a=42;
	lock(&lock_122);
	if(!foo_122) {
		foo_122=x;
		x=NULL;
	}
	unlock(&lock_122);
	if(x)
		free(x);
}
static void worker_122() {
	initme_122();
	assert(foo_122);
	assert(foo_122->a==42);
}
static void run_122() {
	LOCK_INIT(1,lock_122);
	PTHREAD_RUN(3,worker_122,worker_122,worker_122);
	free(foo_122);	
}

// test137: TP. Races on stack variables.
static int glob_137=0;
static lf_queue queue_137;
static void worker_137() {
	int stack=0;
	int *tmp=&stack;
	lf_queue_pop(queue_137,&tmp);
	(*tmp)++;
	int *racy=&stack;
	lf_queue_push(queue_137,&racy);
	(*racy)++;
	sleep(4);
	// We may miss the races if we sleep less due to die_memory events...
}
static void run_137() {
	int stack=0;
	int *tmp=&stack;
	lf_queue_init(&queue_137, 0, sizeof(int *), 5);
	lf_queue_push(queue_137,&tmp);
	PTHREAD_RUN(4,worker_137,worker_137,worker_137,worker_137);
	lf_queue_pop(queue_137,&tmp);
	lf_queue_free(&queue_137);
}

// test148: TP. 3 threads, h-b hides race between T1 and T3.
static int glob_148=0;
static int cond_148=0;
static Lock lock_148;
static CondVar cv_148;
static void signaller_148() {
	usleep(200000);
	glob_148=1;
	lock(&lock_148);
	cond_148=1;
	cond_signal(&cv_148);
	unlock(&lock_148);
}
static void waiter_148() {
	lock(&lock_148);
	while(cond_148==0)
		cond_wait(&cv_148,&lock_148);
	glob_148=2;
	unlock(&lock_148);
}
static void racer_148() {
	usleep(300000);
	lock(&lock_148);
	glob_148=3;
	unlock(&lock_148);
}
static void run_148() {
	LOCK_INIT(1,lock_148);
	CV_INIT(1,cv_148);
	PTHREAD_RUN(3,signaller_148,waiter_148,racer_148);
}

// test149: TN. allocate and memset lots of of mem in several chunks.
static void run_149() {
	int kChunkSize=1<<16;
	printf("test149:malloc 8x%dM\n",kChunkSize/(1<<10));
	void *mem[8];
	int i;
	for(i=0;i<8;i++) {
		mem[i]=malloc(kChunkSize);
		memset(mem[i],0,kChunkSize);
		printf("+");
	}
	for(i=0;i<8;i++) {
		free(mem[i]);
		printf("-");
	}
	printf(" Done\n");
}

// test150: TP. race which is detected after one of the thread has joined.
static int glob_150=0;
static StealthNotify sn_150;
static Thread thd_150_1, thd_150_2;
static void worker_150_1() {
	glob_150++; 
}
static void worker_150_2() {
	stealth_notify_wait(&sn_150);
	glob_150++;
}
static void run_150() {
	STEALTH_NOTIFY_INIT(1,sn_150);
	thread_init(&thd_150_1,worker_150_1,NULL);
	thread_init(&thd_150_2,worker_150_2,NULL);
	thread_start(&thd_150_1);
	thread_start(&thd_150_2);
	thread_join(&thd_150_1);
	stealth_notify_signal(&sn_150);
	thread_join(&thd_150_2);
}

// test154: TP. long test with lots of races
#define kNumIter_154 100
#define kArraySize_154 100
static int arr_154[kArraySize_154];
static void racyaccess_154(int *a) {
	(*a)++;
}
static void racyloop_154() {
	int j;
	for(j=0;j<kArraySize_154;j++)	
		racyaccess_154(&arr_154[j]);
}
static void worker_154() {
	int i;
	for(i=0;i<kNumIter_154;i++) {
		usleep(10);
		racyloop_154();
	}
}
static void run_154() {
	PTHREAD_RUN(2,worker_154,worker_154);
}

// test162: TP. 
static int *glob_162=NULL;
#define kIdx 77
static StealthNotify sn_162;
static void read_162() {
	assert(glob_162[kIdx]==777);
	stealth_notify_signal(&sn_162);
}
static void free_162() {
	stealth_notify_wait(&sn_162);
	free(glob_162);
}
static void run_162() {
	STEALTH_NOTIFY_INIT(1,sn_162);
	glob_162=(int *)malloc(100 * sizeof(int));
	glob_162[kIdx]=777;
	PTHREAD_RUN(2,read_162,free_162);
}

// test163: TP.
static int *glob_163=NULL;
#define kIdx 77
static StealthNotify sn_163;
static void free_163() {
	free(glob_163);
	stealth_notify_signal(&sn_163);
}
static void read_163() {
	stealth_notify_wait(&sn_163);
	assert(glob_163);
}
static void run_163() {
	STEALTH_NOTIFY_INIT(1,sn_163);
	glob_163=(int *)malloc(100 * sizeof(int));
	glob_163[kIdx]=777;
	PTHREAD_RUN(2,read_163,free_163);
}

// test506: TN. massive HB's using Barriers
static int glob_506=0;
#define N_506 2
#define ITERATOR_506 4
static Barrier barrier_506[ITERATOR_506];
static Lock lock_506;
static Thread thread_506[N_506];
static void worker_506() {
	int i;
	for(i=0;i<ITERATOR_506;i++) {
		lock(&lock_506);
		++glob_506;
		unlock(&lock_506);
		barrier_wait(&barrier_506[i]);
	}
}
static void run_506() {
	LOCK_INIT(1,lock_506);
	int i;
	for(i=0;i<ITERATOR_506;i++)
		barrier_init(&barrier_506[i],N_506);
	for(i=0;i<N_506;i++)
		thread_init(&thread_506[i],worker_506,NULL);
	for(i=0;i<N_506;i++)
		thread_start(&thread_506[i]);
	for(i=0;i<N_506;i++)
		thread_join(&thread_506[i]);
	assert(glob_506==N_506*ITERATOR_506);
	for(i=0;i<ITERATOR_506;i++)
		barrier_destroy(&barrier_506[i]);
}

// test580: TP. Race on fwrite argument
static char s_580[]="abrabdagasd\n";
static void worker_580_1() {
	fwrite(s_580,1,sizeof(s_580)-1,stdout);
}
static void worker_580_2() {
	s_580[3]='z';
}
static void run_580() {
	PTHREAD_RUN(2,worker_580_1,worker_580_2);
}

// test581: TP. Race on puts argument
static char s_581[]="aasdasdfasdf";
static void worker_581_1() {
	puts(s_581);
}
static void worker_581_2() {
	s_581[3]='z';
}
static void run_581() {
	PTHREAD_RUN(2,worker_581_1,worker_581_2);
}

// test582: TP. Race on printf argument
static volatile char s_582_1[]="asdfasdfasdf";
static volatile char s_582_2[]="asdfasdfasdf";
static void worker_582_1() {
	fprintf(stdout, "printing a string: %s\n", s_582_1);
	fprintf(stderr, "printing a string: %s\n", s_582_2);
}
static void worker_582_2() {
	s_582_1[3]='z';
	s_582_2[3]='z';
}
static void run_582() {
	PTHREAD_RUN(2,worker_582_1,worker_582_1);
}

//===========================belong to posix_tests=========================
// test600: TP. Simple test with RWLock
static int var_600_1=0,var_600_2=0;
static RWLock rwlock_600;
static void write_while_holding_rdlock_600(int *p) {
	usleep(100000);
	rwlock_rdlock(&rwlock_600);
	(*p)++;
	rwlock_unlock(&rwlock_600);
}
static void correct_write_600(int *p) {
	rwlock_wrlock(&rwlock_600);
	(*p)++;
	rwlock_unlock(&rwlock_600);
}
static void worker_600_1() { write_while_holding_rdlock_600(&var_600_1); }
static void worker_600_2() { correct_write_600(&var_600_1); }
static void worker_600_3() { write_while_holding_rdlock_600(&var_600_2); }
static void worker_600_4() { correct_write_600(&var_600_2); }
static void run_600() {
	RWLOCK_INIT(1,rwlock_600);
	PTHREAD_RUN(4,worker_600_1,worker_600_2,worker_600_3,worker_600_4);
}

// test601:Backwards lock
#define N_601 3
static RWLock rwlock_601;
static int adder_num_601;
static int glob_601[N_601];
static Thread thread_601[4];
// This test uses "Backwards mutex" locking protocol.
// We take a *reader* lock when writing to a per-thread data
// (GLOB[thread_num])  and we take a *writer* lock when we
// are reading from the entire array at once.
//
// Such locking protocol is not understood by ThreadSanitizer's
// hybrid state machine. So, you either have to use a pure-happens-before
// detector ("tsan --pure-happens-before") or apply pure happens-before mode
// to this particular lock by using ANNOTATE_PURE_HAPPENS_BEFORE_MUTEX(&mu).
//
static void adder_601() {
	int my_num=ATOMIC_INCREMENT(&adder_num_601,1)-1;
	assert(my_num>=0);
	assert(my_num<3);
	rwlock_rdlock(&rwlock_601);
	glob_601[my_num]++;
	rwlock_unlock(&rwlock_601);
}
static void aggregator_601() {
	int sum=0,i;
	rwlock_wrlock(&rwlock_601);
	for(i=0;i<N_601;i++)
		sum+=glob_601[i];
	rwlock_unlock(&rwlock_601);
}
static void run_601() {
	RWLOCK_INIT(1,rwlock_601);
	adder_num_601=0;
	int i;
	{
		thread_init(&thread_601[0],adder_601,NULL);
		thread_init(&thread_601[1],adder_601,NULL);
		thread_init(&thread_601[2],adder_601,NULL);
		thread_init(&thread_601[3],aggregator_601,NULL);
		for(i=0;i<4;i++)
			thread_start(&thread_601[i]);
		for(i=0;i<4;i++)
			thread_join(&thread_601[i]);
	}
	adder_num_601=0;
	{
		thread_init(&thread_601[0],aggregator_601,NULL);
		thread_init(&thread_601[1],adder_601,NULL);
		thread_init(&thread_601[2],adder_601,NULL);
		thread_init(&thread_601[3],adder_601,NULL);
		for(i=0;i<4;i++)
			thread_start(&thread_601[i]);
		for(i=0;i<4;i++)
			thread_join(&thread_601[i]);
	}
}

// test602: TN.
#define kMmapSize_602 65536
static void subworker_602() {
	int i;
	for(i=0;i<100;i++) {
		int *ptr=(int *)mmap(NULL,kMmapSize_602,PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
		//any write will result in a private complication
		*ptr=42;
		munmap(ptr,kMmapSize_602);
	}
}
static void worker_602() {
	Thread thread[4];
	int i;
	for(i=0;i<4;i++)
		thread_init(&thread[i],subworker_602,NULL);
	for(i=0;i<4;i++)
		thread_start(&thread[i]);
	for(i=0;i<4;i++)
		thread_join(&thread[i]);
}
static void run_602() {
	Thread thread[4];
	int i;
	for(i=0;i<4;i++)
		thread_init(&thread[i],worker_602,NULL);
	for(i=0;i<4;i++)
		thread_start(&thread[i]);
	for(i=0;i<4;i++)
		thread_join(&thread[i]);
}

// test603: TN. unlink/fopen, rmdir/opendir.
static int glob_603_1=0,glob_603_2=0;
static char *dir_name_603=NULL,*file_name_603=NULL;
static Thread thread_603[2];
static void waker_603_1() {
	usleep(100000);
	glob_603_1=1;
	// unlink deletes a file 'filename'
  	// which exits spin-loop in Waiter1().
  	printf("  Deleting file...\n");
  	assert(unlink(file_name_603)==0);
}
static void waiter_603_1() {
	FILE *tmp;
	while((tmp=fopen(file_name_603,"r"))!=NULL) {
		fclose(tmp);
		usleep(10000);
	}
	printf("  ...file has been deleted\n");
	glob_603_1=2;//write
}
static void waker_603_2() {
	usleep(100000);
	glob_603_2=1;
	// rmdir deletes a directory 'dir_name'
  	// which exit spin-loop in Waker().
  	printf("  Deleting directory...\n");
  	assert(rmdir(dir_name_603) == 0);
}
static void waiter_603_2() {
	DIR *tmp;
	while((tmp=opendir(dir_name_603))!=NULL) {
		closedir(tmp);
		usleep(10000);
	}
	printf("  ...directory has been deleted\n");
  	glob_603_2=2;
}
static void run_603() {
	dir_name_603=strdup("/tmp/data-race-XXXXXX");
	assert(mkdtemp(dir_name_603)!=NULL);//dir_name is unqiue
	char tmp[150];
	strcpy(tmp,dir_name_603);
	strcat(tmp,"/XXXXXX");
	file_name_603=strdup(tmp);
	const int fd=mkstemp(file_name_603);//create and open temp file
	assert(fd>=0);
	close(fd);
	int i;
	thread_init(&thread_603[0],waker_603_1,NULL);
	thread_init(&thread_603[1],waiter_603_1,NULL);
	for(i=0;i<2;i++)
		thread_start(&thread_603[i]);
	for(i=0;i<2;i++)
		thread_join(&thread_603[i]);

	thread_init(&thread_603[0],waker_603_2,NULL);
	thread_init(&thread_603[1],waiter_603_2,NULL);
	for(i=0;i<2;i++)
		thread_start(&thread_603[i]);
	for(i=0;i<2;i++)
		thread_join(&thread_603[i]);

	free(file_name_603);
	file_name_603=NULL;
	free(dir_name_603);
	dir_name_603=NULL;
}

// test604: TP. Unit test for RWLock::TryLock and RWLock::ReaderTryLock.
static int glob_604_1=0;
static char padding_604_1[64];
static int glob_604_2=0;
static char padding_604_2[64];
static int glob_604_3=0;
static char padding_604_3[64];
static int glob_604_4=0;
static char padding_604_4[64];
static RWLock rwlock_604;
static StealthNotify sn_604_1,sn_604_2,sn_604_3,sn_604_4,sn_604_5;
// Worker1 locks the globals for writing for a long time.
// Worker2 tries to write to globals twice: without a writer lock and with it.
// Worker3 tries to read from globals twice: without a reader lock and with it.
static void worker_604_1() {
	rwlock_wrlock(&rwlock_604);
	glob_604_1=1;
	glob_604_2=1;
	glob_604_3=1;
	glob_604_4=1;
	stealth_notify_signal(&sn_604_1);
	stealth_notify_wait(&sn_604_2);
	stealth_notify_wait(&sn_604_3);
	rwlock_unlock(&rwlock_604);
	stealth_notify_signal(&sn_604_4);
}
static void worker_604_2() {
	stealth_notify_wait(&sn_604_1);
	if(rwlock_trywrlock(&rwlock_604)) //this point mu is held in Worker1
		assert(0);
	else //write without a writer lock
		glob_604_1=2;
	stealth_notify_signal(&sn_604_2); 
	stealth_notify_wait(&sn_604_5);
	//mu is released by Worker3
	if(rwlock_trywrlock(&rwlock_604)) {
		glob_604_2=2;
		rwlock_unlock(&rwlock_604);
	}
	else
		assert(0);
}
static void worker_604_3() {
	stealth_notify_wait(&sn_604_1);
	if(rwlock_tryrdlock(&rwlock_604)) //this point mu is held in Worker1
		assert(0);
	else //read without a reader lock
		printf("\treading glob3: %d\n", glob_604_3); 
	stealth_notify_signal(&sn_604_3);
	stealth_notify_wait(&sn_604_4);
	//mu is released by Worker1
	if(rwlock_tryrdlock(&rwlock_604)) {
		printf("\treading glob4: %d\n", glob_604_4);
		rwlock_unlock(&rwlock_604);
	}
	else
		assert(0);
	stealth_notify_signal(&sn_604_5);
}
static void run_604() {
	RWLOCK_INIT(1,rwlock_604);
	STEALTH_NOTIFY_INIT(5,sn_604_1,sn_604_2,sn_604_3,sn_604_4,sn_604_5);
	PTHREAD_RUN(3,worker_604_1,worker_604_2,worker_604_3);
}

// test605: TP. Test that reader lock/unlock do not create a hb-arc.
static RWLock rwlock_605;
static int glob_605;
static StealthNotify sn_605;
static void worker_605_1() {
	glob_605=1;
	rwlock_rdlock(&rwlock_605);
	rwlock_unlock(&rwlock_605);
	stealth_notify_signal(&sn_605);
}
static void worker_605_2() {
	stealth_notify_wait(&sn_605);
	rwlock_rdlock(&rwlock_605);
	rwlock_unlock(&rwlock_605);
	glob_605=1;
}
static void run_605() {
	RWLOCK_INIT(1,rwlock_605);
	STEALTH_NOTIFY_INIT(1,sn_605);
	PTHREAD_RUN(2,worker_605_1,worker_605_2);
}

// test606: TN. Test the support for libpthread TSD(Thread-specific Date) 
// destructors.
pthread_key_t key_606;
#define kInitialValue_606 0xfeedface
static int tsd_array_606[2];
static Thread thread_606[2];
static void destructor_606(void *ptr) {
	int *writer=(int *)ptr;
	*writer=kInitialValue_606;
}
static void dowork_606(int index) {
	int *value=&(tsd_array_606[index]);
	*value=2;
	pthread_setspecific(key_606,value);
	int *read=(int *)pthread_getspecific(key_606);
	assert(read==value);
}
static void worker_606_1() { dowork_606(0); }
static void worker_606_2() { dowork_606(1); }
static void run_606() {
	pthread_key_create(&key_606,destructor_606);
	int i;
	thread_init(&thread_606[0],worker_606_1,NULL);
	thread_init(&thread_606[1],worker_606_2,NULL);
	for(i=0;i<2;i++)
		thread_start(&thread_606[i]);
	for(i=0;i<2;i++)
		thread_join(&thread_606[i]);
	for(i=0;i<2;i++)
		assert(tsd_array_606[i]==kInitialValue_606);
}

// test607: TP.
static void worker_607() {
	struct tm timestruct;
	timestruct.tm_sec=56;
	timestruct.tm_min=34;
	timestruct.tm_hour=12;
	timestruct.tm_mday=31;
	timestruct.tm_mon=3-1;
	timestruct.tm_year=2015-1900;
	timestruct.tm_wday=4;
	timestruct.tm_yday=0;
	time_t ret=mktime(&timestruct);
	assert(ret!=-1);
}
static void run_607() {
	PTHREAD_RUN(2,worker_607,worker_607);
}

//===========================belong to demo_tests=========================
// test302:Complex race which happens at least twice
static int glob_302=0;
static Lock lock_302_1, lock_302_2;
// In this test we have many different accesses to glob and only one access
// is not synchronized properly.
static void worker_302() {
	int i;
	for(i=0;i<100;i++) {
		switch(i % 4) {
	      case 0:
	        // This read is protected correctly.
	        lock(&lock_302_1); 
	        assert(glob_302>=0);
	        unlock(&lock_302_1);
	        break;
	      case 1:
	        // Here we used the wrong lock! The reason of the race is here.
	        lock(&lock_302_2); 
	        assert(glob_302>=0);
	        unlock(&lock_302_2);
	        break;
	      case 2:
	        // This read is protected correctly.
	        lock(&lock_302_1);
	        assert(glob_302>=0);
	        unlock(&lock_302_1);
	        break;
	      case 3:
	        // This write is protected correctly.
	        lock(&lock_302_2);
	        glob_302++;
	        unlock(&lock_302_2);
	        break;
	    }
	    // sleep a bit so that the threads interleave
	    // and the race happens at least twice.
	    usleep(100);
	}
}
static void run_302() {
	LOCK_INIT(2,lock_302_1,lock_302_2);
	PTHREAD_RUN(2,worker_302,worker_302);
}

// test305:A bit more tricky: two locks used inconsistenly.
static int glob_305=0;
static Lock lock_305_1, lock_305_2;
static void worker_305_1() {
	lock(&lock_305_1);
	lock(&lock_305_2);
	glob_305=1;
	unlock(&lock_305_2);
	unlock(&lock_305_1);
}
static void worker_305_2() {
	lock(&lock_305_1);
	glob_305=2;
	unlock(&lock_305_1);
}
static void worker_305_3() {
	lock(&lock_305_1);
	lock(&lock_305_2);
	glob_305=3;
	unlock(&lock_305_2);
	unlock(&lock_305_1);
}
static void worker_305_4() {
	lock(&lock_305_2);
	glob_305=4;
	unlock(&lock_305_2);
}
static void run_305() {
	LOCK_INIT(2,lock_305_1,lock_305_2);
	Thread thread[4];
	int i;
	thread_init(&thread[0],worker_305_1,NULL);
	thread_init(&thread[1],worker_305_2,NULL);
	thread_init(&thread[2],worker_305_3,NULL);
	thread_init(&thread[3],worker_305_4,NULL);
	for(i=0;i<4;i++) {
		thread_start(&thread[i]);
		usleep(10000);		
	}
	for(i=0;i<4;i++)
		thread_join(&thread[i]);
}

// test306:Two locks are used to protect a var.
static int glob_306=0;
static Lock lock_306_1, lock_306_2;
// Thread1 and Thread2 access the var under two locks.
// Thread3 uses no locks.
static void worker_306_1() {
	lock(&lock_306_1);
	lock(&lock_306_2);
	glob_306=1;
	unlock(&lock_306_2);
	unlock(&lock_306_1);
}
static void worker_306_2() {
	lock(&lock_306_1);
	lock(&lock_306_2);
	glob_306=3;
	unlock(&lock_306_2);
	unlock(&lock_306_1);
}
static void worker_306_3() {
	glob_306=4;
}
static void run_306() {
	LOCK_INIT(2,lock_306_1,lock_306_2);
	Thread thread[3];
	int i;
	thread_init(&thread[0],worker_306_1,NULL);
	thread_init(&thread[1],worker_306_2,NULL);
	thread_init(&thread[2],worker_306_3,NULL);
	for(i=0;i<3;i++) {
		thread_start(&thread[i]);
		usleep(10000);		
	}
	for(i=0;i<3;i++)
		thread_join(&thread[i]);
}

// test310:One more simple race
static int *glob_310=NULL;
static Lock lock_310_1,lock_310_2,lock_310_3;
static void writer_310_1() {
	lock(&lock_310_3);
	lock(&lock_310_1);
	*glob_310=1;
	unlock(&lock_310_1);
	unlock(&lock_310_3);
}
static void writer_310_2() {
	lock(&lock_310_2);
	lock(&lock_310_1);	
	int some_unrelated_stuff = 0;
  	if (some_unrelated_stuff == 0)
    	some_unrelated_stuff++;
  	*glob_310=2;
  	unlock(&lock_310_1);
	unlock(&lock_310_2);
}
static void reader_310() {
	lock(&lock_310_2);
	assert(*glob_310<=2);
	unlock(&lock_310_2);
}
static void run_310() {
	glob_310=calloc(1,sizeof(*glob_310));
	LOCK_INIT(3,lock_310_1,lock_310_2,lock_310_3);
	Thread thread[3];
	thread_init(&thread[0],writer_310_1,NULL);
	thread_init(&thread[1],writer_310_2,NULL);
	thread_init(&thread[2],reader_310,NULL);

	thread_start(&thread[0]);
	thread_start(&thread[1]);

	usleep(100000);//let the writers go first
	thread_start(&thread[2]);
	int i;
	for(i=0;i<3;i++)
		thread_join(&thread[i]);	
	free(glob_310);
}

// test311:A test with a very deep stack
static int glob_311=0;
static void racywrite_311() { glob_311++; }
static void fun_311_1() { racywrite_311(); }
static void fun_311_2() { fun_311_1(); }
static void fun_311_3() { fun_311_2(); }
static void fun_311_4() { fun_311_3(); }
static void fun_311_5() { fun_311_4(); }
static void fun_311_6() { fun_311_5(); }
static void fun_311_7() { fun_311_6(); }
static void fun_311_8() { fun_311_7(); }
static void fun_311_9() { fun_311_8(); }
static void fun_311_10() { fun_311_9(); }
static void fun_311_11() { fun_311_10(); }
static void fun_311_12() { fun_311_11(); }
static void fun_311_13() { fun_311_12(); }
static void fun_311_14() { fun_311_13(); }
static void fun_311_15() { fun_311_14(); }
static void fun_311_16() { fun_311_15(); }
static void fun_311_17() { fun_311_16(); }
static void fun_311_18() { fun_311_17(); }
static void fun_311_19() { fun_311_18(); }
static void worker_311() { fun_311_19(); }
static void run_311() {
	PTHREAD_RUN(3,worker_311,worker_311,worker_311);
}

// test313:
static int glob_313;
static Lock lock_313;
static int flag_313;
static void worker_313_1() {
	sleep(1);
	lock(&lock_313);
	bool f=flag_313;
	unlock(&lock_313);
	if(!f)
		glob_313++;
}
static void worker_313_2() {
	glob_313++;
	lock(&lock_313);
	flag_313=true;
	unlock(&lock_313);
}
static void run_313() {
	LOCK_INIT(1,lock_313);
	glob_313=0;
	PTHREAD_RUN(2,worker_313_1,worker_313_2);
}

int main()
{
	TEST_RUN(0);
	TEST_RUN(1);
	TEST_RUN(2);
	TEST_RUN(3);
	TEST_RUN(4);
	TEST_RUN(6);
	TEST_RUN(7);
	TEST_RUN(8);
	TEST_RUN(9);
	TEST_RUN(10);
	TEST_RUN(11);
	TEST_RUN(12);
	TEST_RUN(13);
	TEST_RUN(14);
	TEST_RUN(16);
	TEST_RUN(23);
	TEST_RUN(28);
	TEST_RUN(29);
	TEST_RUN(32);
	TEST_RUN(36);
	TEST_RUN(37);
	TEST_RUN(38);
	TEST_RUN(40);
	TEST_RUN(42);
	TEST_RUN(43);
	TEST_RUN(44);
	TEST_RUN(45);
	TEST_RUN(46);
	TEST_RUN(47);
	TEST_RUN(48);
	TEST_RUN(49);TEST_RUN(50);
	TEST_RUN(53);
	TEST_RUN(57);
	TEST_RUN(58);
	TEST_RUN(60);
	TEST_RUN(61);
	TEST_RUN(64);
	TEST_RUN(65);
	TEST_RUN(68);
	TEST_RUN(75);
	TEST_RUN(83);
	TEST_RUN(84);
	TEST_RUN(89);
	TEST_RUN(90);
	TEST_RUN(91);
	TEST_RUN(92);
	TEST_RUN(94);
	TEST_RUN(95);
	TEST_RUN(96);
	TEST_RUN(100);
	TEST_RUN(101);
	TEST_RUN(102);
	TEST_RUN(104);
	TEST_RUN(109);
	TEST_RUN(111);
	TEST_RUN(113);
	TEST_RUN(114);
	TEST_RUN(117);
	TEST_RUN(119);
	TEST_RUN(120);
	TEST_RUN(121);
	TEST_RUN(122);
	TEST_RUN(137);
	TEST_RUN(148);
	TEST_RUN(149);
	TEST_RUN(150);
	TEST_RUN(154);
	TEST_RUN(162);
	TEST_RUN(163);
	TEST_RUN(506);
	TEST_RUN(580);
	TEST_RUN(581);
	TEST_RUN(582);
	TEST_RUN(600);
	TEST_RUN(601);
	TEST_RUN(602);
	TEST_RUN(603);
	TEST_RUN(604);
	TEST_RUN(605);
	TEST_RUN(606);
	TEST_RUN(607);
	TEST_RUN(302);
	TEST_RUN(305);
	TEST_RUN(306);
	TEST_RUN(310);
	TEST_RUN(311);
	TEST_RUN(313);
	return 0;
}
