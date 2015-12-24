#include <iostream>
#include <vector>
#include <sys/mman.h>
#include <dirent.h>
#include <time.h>
#include "test_utils.h"

using namespace std;

// // test0:None
// namespace test0 {
// 	int glob=0;
// 	void Run() {}

// 	REGISTER_TEST(Run,0);
// }//namespace test0

// // test1:TP.Simple race(write vs write).
// namespace test1 {
// 	int glob=0;
// 	void Worker1() {
// 		glob=1;
// 	}
// 	void Worker2() {
// 		glob=2;
// 	}
// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,1);
// }//namespace test1

// // test2:TN.Synchronization via condvar
// namespace test2 {
// 	int glob=0;
// 	int cond=0;
// 	// Two write accesses to GLOB are synchronized because
// 	// the pair of CV.Signal() and CV.Wait() establish happens-before relation.
// 	//
// 	// Waiter:                      Waker:
// 	// 1. COND = 0
// 	// 2. Start(Waker)
// 	// 3. MU.Lock()                 a. write(GLOB)
// 	//                              b. MU.Lock()
// 	//                              c. COND = 1
// 	//                         /--- d. CV.Signal()
// 	//  4. while(COND)        /     e. MU.Unlock()
// 	//       CV.Wait(MU) <---/
// 	//  5. MU.Unlock()
// 	//  6. write(GLOB)
// 	void Waker() {
// 		usleep(200000);
// 		glob=1;
// 		CV_SIGNAL(1);
// 	}

// 	void Waiter() {
// 		CV_WAIT(1);
// 		glob=2;
// 	}

// 	void Run() {	
// 		HANDLER Worker1=(HANDLER)Waker;
// 		HANDLER Worker2=(HANDLER)Waiter;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,2);
// }//namespace test2

// // test3:TP.Synchronization via LockWhen, signaller gets there first. 
// namespace test3 {
// 	// Two write accesses to GLOB are synchronized via conditional critical section.
// 	// Note that LockWhen() happens first (we use sleep(1) to make sure)!
// 	//
// 	// Waiter:                           Waker:
// 	// 1. COND = 0
// 	// 2. Start(Waker)
// 	//                                   a. write(GLOB)
// 	//                                   b. MU.Lock()
// 	//                                   c. COND = 1
// 	//                              /--- d. MU.Unlock()
// 	// 3. MU.LockWhen(COND==1) <---/
// 	// 4. MU.Unlock()
// 	// 5. write(GLOB)
// 	int glob=0;
// 	int cond=0;
// 	Mutex mu;
// 	void Waker() {
// 		usleep(100000);//make sure the waiter blocks
// 		glob=1;

// 		mu.Lock();
// 		cond=1;
// 		mu.Unlock();
// 	}
// 	void Waiter() {
// 		cond=0;
// 		mu.LockWhen(Condition(&ArgIsOne,&cond));
// 		mu.Unlock();
// 		glob=2;
// 	}
// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Waker;
// 		HANDLER Worker2=(HANDLER)Waiter;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,3);
// }//namespace test3

// // test04: TN. Synchronization via PCQ
// namespace test4 {
// 	int     glob = 0;
// 	// Two write accesses to GLOB are separated by PCQ Put/Get.
// 	//
// 	// Putter:                        Getter:
// 	// 1. write(GLOB)
// 	// 2. Q.Put() ---------\          .
// 	//                      \-------> a. Q.Get()
// 	//                                b. write(GLOB)

// 	//use semephore to simulate consumer-producer

// 	void Putter() {
// 		glob=1;
// 		PC_SEM_PRODUCE(1);
// 	}

// 	void Getter() {
// 		PC_SEM_CONSUME(1);
// 		glob=2;
// 	}	

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Putter;
// 		HANDLER Worker2=(HANDLER)Getter;
// 		PC_SEM_RUN(1,2,1);
// 	}
// 	REGISTER_TEST(Run,4);
// }//namespace test4

// // test6:TN.Synchronization via condvar,but waker gets there first
// namespace test6 {
// 	int glob=0;
// 	int cond=0;
// 	// Same as test05 but we annotated the Wait() loop.
// 	//
// 	// Waiter:                                            Waker:
// 	// 1. COND = 0
// 	// 2. Start(Waker)
// 	// 3. MU.Lock()                                       a. write(GLOB)
// 	//                                                    b. MU.Lock()
// 	//                                                    c. COND = 1
// 	//                                           /------- d. CV.Signal()
// 	//  4. while(COND)                          /         e. MU.Unlock()
// 	//       CV.Wait(MU) <<< not called        /
// 	//  6. ANNOTATE_CONDVAR_WAIT(CV, MU) <----/
// 	//  5. MU.Unlock()
// 	//  6. write(GLOB)
// 	void Waker() {
// 		glob=1;
// 		CV_SIGNAL(1);
// 	}

// 	void Waiter() {
// 		usleep(500000);
// 		CV_WAIT(1);
// 		glob=2;
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Waker;
// 		HANDLER Worker2=(HANDLER)Waiter;	
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,6);
// }//namespace test6

// // test8:TN.Synchronization via thread start/join
// namespace test8 {
// 	int glob=0;
// 	// Three accesses to GLOB are separated by thread start/join.
// 	//
// 	// Parent:                        Worker:
// 	// 1. write(GLOB)
// 	// 2. Start(Worker) ------------>
// 	//                                a. write(GLOB)
// 	// 3. Join(Worker) <------------
// 	// 4. write(GLOB)
// 	void Worker1(void ) {
// 		glob=2;
// 	}

// 	void Parent(void ) {glob=1;
// 		PTHREAD_RUN(1);glob=2;
// 	}

// 	void Run() {
// 		Parent();
// 	}
// 	REGISTER_TEST(Run,8);
// }//namespace test8

// // test9:TP.Simple race (read vs write).
// namespace test9 {
// 	int glob=0;
// 	// A simple data race between writer and reader.
// 	// Write happens after read (enforced by sleep).
// 	// Usually, easily detectable by a race detector.

// 	void Writer(void ) {
// 		usleep(100000);
// 		glob=3;
// 	}

// 	void Reader(void ) {
// 		assert(glob!=-777);
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Writer;
// 		HANDLER Worker2=(HANDLER)Reader;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,9);
// }//namespace test9

// // test10:TP.Simple race (write vs read).
// namespace test10 {
// 	int glob=0;
// 	// A simple data race between writer and reader.
// 	// Write happens before Read (enforced by sleep),
// 	// otherwise this test is the same as test09.
// 	//
// 	// Writer:                    Reader:
// 	// 1. write(GLOB)             a. sleep(long enough so that GLOB
// 	//                                is most likely initialized by Writer)
// 	//                            b. read(GLOB)
// 	//
// 	//
// 	// Eraser algorithm does not detect the race here,
// 	// see Section 2.2 of http://citeseer.ist.psu.edu/savage97eraser.html.
// 	//
// 	void Writer(void ) {
// 		glob=3;
// 	}

// 	void Reader(void ) {
// 		usleep(100000);
// 		assert(glob!=-777);
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Writer;
// 		HANDLER Worker2=(HANDLER)Reader;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,10);
// }//namespace test10

// // test12:TN.Synchronization via Mutex, then via PCQ
// namespace test12 {
// 	int glob=0;
// 	// This test is properly synchronized, but currently
// 	// helgrind reports a false positive.
// 	//
// 	// First, we write to GLOB under MU, then we synchronize via PCQ,
// 	// which is essentially a semaphore.
// 	//
// 	// Putter:                       Getter:
// 	// 1. MU.Lock()                  a. MU.Lock()
// 	// 2. write(GLOB) <---- MU ----> b. write(GLOB)
// 	// 3. MU.Unlock()                c. MU.Unlock()
// 	// 4. Q.Put()   ---------------> d. Q.Get()
// 	//                               e. write(GLOB)

// 	//use semephore to simulate consumer-producer

// 	void Putter() {
// 		pthread_mutex_lock(&m);
// 		glob++;
// 		pthread_mutex_unlock(&m);
// 		PC_SEM_PRODUCE(1);
// 	}

// 	void Getter() {
// 		pthread_mutex_lock(&m);
// 		glob++;
// 		pthread_mutex_unlock(&m);
// 		PC_SEM_CONSUME(1);
// 		glob++;
// 	}	

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Putter;
// 		HANDLER Worker2=(HANDLER)Getter;
// 		PC_SEM_RUN(1,2,1);
// 	}
// 	REGISTER_TEST(Run,12);
// }//namespace test12

// // test13:TN.Synchronization via Mutex, then via LockWhen
// namespace test13 {
// 	int glob=0;
// 	int cond=0;
// 	// This test is essentially the same as test12, but uses LockWhen
// 	// instead of PCQ.
// 	//
// 	// Waker:                                     Waiter:
// 	// 1. MU.Lock()                               a. MU.Lock()
// 	// 2. write(GLOB) <---------- MU ---------->  b. write(GLOB)
// 	// 3. MU.Unlock()                             c. MU.Unlock()
// 	// 4. MU.Lock()                               .
// 	// 5. COND = 1                                .
// 	// 6. ANNOTATE_CONDVAR_SIGNAL -------\        .
// 	// 7. MU.Unlock()                     \       .
// 	//                                     \----> d. MU.LockWhen(COND == 1)
// 	//                                            e. MU.Unlock()
// 	//                                            f. write(GLOB)

// 	Mutex mu;

// 	//use condvar to simulate LockWhen
// 	void Waker() {
// 		mu.Lock();
// 		glob++;
// 		mu.Unlock();

// 		mu.Lock();
// 		cond=1;
// 		mu.Unlock();
// 	}

// 	void Waiter() {
// 		mu.Lock();
// 		glob++;
// 		mu.Unlock();
// 		mu.LockWhen(Condition(&ArgIsOne,&cond));
// 		mu.Unlock();
// 		glob++;
// 	}

// 	void Run() {	
// 		HANDLER Worker1=(HANDLER)Waker;
// 		HANDLER Worker2=(HANDLER)Waiter;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,13);
// }//namespace test13

// // test14:TN.Synchronization via PCQ, reads, 2 workers.
// namespace test14 {
// 	int glob=0;
// 	// This test is properly synchronized, but currently (Dec 2007)
// 	// helgrind reports a false positive.
// 	//
// 	// This test is similar to test11, but uses PCQ (semaphore).
// 	//
// 	// Putter2:                  Putter1:                     Getter:
// 	// 1. read(GLOB)             a. read(GLOB)
// 	// 2. Q2.Put() ----\         b. Q1.Put() -----\           .
// 	//                  \                          \--------> A. Q1.Get()
// 	//                   \----------------------------------> B. Q2.Get()
// 	//                                                        C. write(GLOB)

// 	//use semephore to simulate consumer-producer

// 	void Putter(void ) {
// 		if(glob!=-777) { }
// 		PC_SEM_PRODUCE(1);
// 	}

// 	void Putter1(void ) {
// 		if(glob!=-777) { }
// 		PC_SEM_PRODUCE(2);
// 	}

// 	void Getter(void ) {
// 		PC_SEM_CONSUME(1);
// 		PC_SEM_CONSUME(2);
// 		glob++;	
// 	}	

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Putter;
// 		HANDLER Worker2=(HANDLER)Putter1;
// 		HANDLER Worker3=(HANDLER)Getter;
// 		PC_SEM_RUN(2,3,1);
// 	}
// 	REGISTER_TEST(Run,14);
// }//namespace test14

// // test16:TN.Barrier,2 threads
// namespace test16 {
// 	int glob=0;

// 	void Worker1(void ) {
// 		if(glob!=-777) { }
// 		pthread_barrier_wait(&b);
// 	}
// 	void Worker2(void ) {
// 		if(glob!=-777) { }
// 		pthread_barrier_wait(&b);
// 		glob=2;
// 	}

// 	void Run() {
// 		pthread_barrier_init(&b,0,2);
// 		PTHREAD_RUN(2);
// 		pthread_barrier_destroy(&b);
// 	}
// 	REGISTER_TEST(Run,16);
// }//namespace test16

// // test:TN.TryLock, ReaderLock, ReaderTryLock
// namespace test23 {
// 	int glob=0;

// 	void Worker_TryLock(void ) {
// 		for(int i=0;i<5;i++) {
// 			while(true) {
// 				if(pthread_rwlock_trywrlock(&rwlock)==0) {
// 					glob++;
// 					pthread_rwlock_unlock(&rwlock);
// 					break;
// 				}
// 				usleep(1000);
// 			}
// 		}
// 	}

// 	void Worker_ReaderTryLock(void ) {
// 		for(int i=0;i<5;i++) {
// 			while(true) {
// 				if(pthread_rwlock_tryrdlock(&rwlock)==0) {
// 					if(glob!=-777) { }
// 					pthread_rwlock_unlock(&rwlock);
// 					break;
// 				}
// 				usleep(1000);
// 			}
// 		}	
// 	}

// 	void Worker_Lock(void ) {
// 		for(int i=0;i<5;i++) {
// 			pthread_rwlock_wrlock(&rwlock);
// 			glob++;
// 			pthread_rwlock_unlock(&rwlock);
// 			usleep(1000);
// 		}	
// 	}

// 	void Worker_ReaderLock(void ) {
// 		for(int i=0;i<5;i++) {
// 			pthread_rwlock_rdlock(&rwlock);
// 			if(glob!=-777) { }
// 			pthread_rwlock_unlock(&rwlock);
// 			usleep(1000);
// 		}		
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Worker_TryLock;
// 		HANDLER Worker2=(HANDLER)Worker_ReaderTryLock;
// 		HANDLER Worker3=(HANDLER)Worker_Lock;
// 		HANDLER Worker4=(HANDLER)Worker_ReaderLock;

// 		PTHREAD_RUN(4);
// 	}
// 	REGISTER_TEST(Run,23);
// }//namespace test23

// // test28:TN.Synchronization via Mutex, then PCQ. 3 threads
// namespace test28 {
// 	// Putter1:                       Getter:                         Putter2:
// 	// 1. MU.Lock()                                                   A. MU.Lock()
// 	// 2. write(GLOB)                                                 B. write(GLOB)
// 	// 3. MU.Unlock()                                                 C. MU.Unlock()
// 	// 4. Q.Put() ---------\                                 /------- D. Q.Put()
// 	// 5. MU.Lock()         \-------> a. Q.Get()            /         E. MU.Lock()
// 	// 6. read(GLOB)                  b. Q.Get() <---------/          F. read(GLOB)
// 	// 7. MU.Unlock()                   (sleep)                       G. MU.Unlock()
// 	//                                c. read(GLOB)

// 	//use semaphore to simulate PCQ

// 	int glob=0;

// 	void Putter(void ) {
// 		pthread_mutex_lock(&m);
// 		glob=1;
// 		pthread_mutex_unlock(&m);
// 		PC_SEM_PRODUCE(1);
// 		pthread_mutex_lock(&m);
// 		if(glob!=-777) { }
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Getter(void ) {
// 		PC_SEM_CONSUME(1);
// 		PC_SEM_CONSUME(1);
// 		usleep(100000);
// 		if(glob!=-777) { }
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Putter;
// 		HANDLER Worker2=(HANDLER)Putter;
// 		HANDLER Worker3=(HANDLER)Getter;

// 		PC_SEM_RUN(1,3,2);
// 	}
// 	REGISTER_TEST(Run,28);
// }//namespace test28

// // test29:TN.Synchronization via Mutex, then PCQ. 4 threads.
// namespace test29 {
// 	// Similar to test29, but has two Getters and two PCQs.
// 	int glob=0;
// 	void Putter(int num) {
// 		pthread_mutex_lock(&m);
// 		glob++;
// 		pthread_mutex_unlock(&m);
// 		if(num==0) {
// 			PC_SEM_PRODUCE(1);
// 			PC_SEM_PRODUCE(1);
// 		}
// 		else {
// 			PC_SEM_PRODUCE(2);
// 			PC_SEM_PRODUCE(2);
// 		}
// 		pthread_mutex_lock(&m);
// 		if(glob!=-777) { }
// 		pthread_mutex_unlock(&m);
// 	}
// 	void Putter1(void ) {
// 		Putter(0);
// 	}
// 	void Putter2(void ) {
// 		Putter(1);
// 	}
// 	void Getter(void ) {
// 		PC_SEM_CONSUME(1);
// 		PC_SEM_CONSUME(2);
// 		usleep(100000);
// 		if(glob==2) { }
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Putter1;
// 		HANDLER Worker2=(HANDLER)Putter2;
// 		HANDLER Worker3=(HANDLER)Getter;
// 		HANDLER Worker4=(HANDLER)Getter;
// 		PC_SEM_RUN(2,4,2);
// 	}
// 	REGISTER_TEST(Run,29);
// }//namespace test29

// // test32:TN.Synchronization via thread create/join. W/R.
// namespace test32 {
// 	// This test is well synchronized but helgrind 3.3.0 reports a race.
// 	//
// 	// Parent:                   Writer:               Reader:
// 	// 1. Start(Reader) -----------------------\       .
// 	//                                          \      .
// 	// 2. Start(Writer) ---\                     \     .
// 	//                      \---> a. MU.Lock()    \--> A. sleep(long enough)
// 	//                            b. write(GLOB)
// 	//                      /---- c. MU.Unlock()
// 	// 3. Join(Writer) <---/
// 	//                                                 B. MU.Lock()
// 	//                                                 C. read(GLOB)
// 	//                                   /------------ D. MU.Unlock()
// 	// 4. Join(Reader) <----------------/
// 	// 5. write(GLOB)
// 	//
// 	//
// 	// The call to sleep() in Reader is not part of synchronization,
// 	// it is required to trigger the false positive in helgrind 3.3.0.
// 	//
// 	int glob=0;

// 	void Writer(void ) {
// 		pthread_mutex_lock(&m);
// 		glob=1;
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Reader(void ) {
// 		usleep(480000);
// 		pthread_mutex_lock(&m);
// 		if(glob!=-777) { }
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Parent(void ) {
// 		HANDLER Worker1=(HANDLER)Reader;
// 		HANDLER Worker2=(HANDLER)Writer;

// 		PTHREAD_RUN(2);

// 		glob=2;
// 	}

// 	void Run() {
// 		Parent();
// 	}
// 	REGISTER_TEST(Run,32);
// }//namespace test32

// // test36:TN.Synchronization via Mutex, then PCQ. 3 threads.
// namespace test36 {
// 	// variation of test28 (W/W instead of W/R)
// 	// Putter1:                       Getter:                         Putter2:
// 	// 1. MU.Lock();                                                  A. MU.Lock()
// 	// 2. write(GLOB)                                                 B. write(GLOB)
// 	// 3. MU.Unlock()                                                 C. MU.Unlock()
// 	// 4. Q.Put() ---------\                                 /------- D. Q.Put()
// 	// 5. MU1.Lock()        \-------> a. Q.Get()            /         E. MU1.Lock()
// 	// 6. MU.Lock()                   b. Q.Get() <---------/          F. MU.Lock()
// 	// 7. write(GLOB)                                                 G. write(GLOB)
// 	// 8. MU.Unlock()                                                 H. MU.Unlock()
// 	// 9. MU1.Unlock()                  (sleep)                       I. MU1.Unlock()
// 	//                                c. MU1.Lock()
// 	//                                d. write(GLOB)
// 	//                                e. MU1.Unlock()
// 	int glob=0;

// 	void Putter(void ) {
// 		pthread_mutex_lock(&m);
// 		glob++;
// 		pthread_mutex_unlock(&m);
// 		PC_SEM_PRODUCE(1);
// 		pthread_mutex_lock(&m);
// 		pthread_mutex_lock(&m1);
// 		glob++;
// 		pthread_mutex_unlock(&m1);
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Getter(void ) {
// 		PC_SEM_CONSUME(1);
// 		PC_SEM_CONSUME(1);
// 		usleep(100000);
// 		pthread_mutex_lock(&m1);
// 		glob++;
// 		pthread_mutex_unlock(&m1);
// 	}

// 	void Run() {
// 		glob=0;
// 		HANDLER Worker1=(HANDLER)Putter;
// 		HANDLER Worker2=(HANDLER)Putter;
// 		HANDLER Worker3=(HANDLER)Getter;
// 		PC_SEM_RUN(1,3,2);
// 	}
// 	REGISTER_TEST(Run,36);
// }//namespace test36

// // test37:TN.Simple synchronization (write vs read).
// namespace test37 {
// 	// Similar to test10, but properly locked.
// 	// Writer:             Reader:
// 	// 1. MU.Lock()
// 	// 2. write
// 	// 3. MU.Unlock()
// 	//                    a. MU.Lock()
// 	//                    b. read
// 	//                    c. MU.Unlock();
// 	int glob=0;

// 	void Writer(void ) {
// 		pthread_mutex_lock(&m);
// 		glob=3;
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Reader(void ) {
// 		usleep(100000);
// 		pthread_mutex_lock(&m);
// 		if(glob!=-777) { }
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Writer;
// 		HANDLER Worker2=(HANDLER)Reader;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,37);
// }//namespace test37

// // test38:TN.Synchronization via Mutexes and PCQ. 4 threads. W/W
// namespace test38 {
// 	// Putter1:            Putter2:           Getter1:       Getter2:
// 	//    MU1.Lock()          MU1.Lock()
// 	//    write(GLOB)         write(GLOB)
// 	//    MU1.Unlock()        MU1.Unlock()
// 	//    Q1.Put()            Q2.Put()
// 	//    Q1.Put()            Q2.Put()
// 	//    MU1.Lock()          MU1.Lock()
// 	//    MU2.Lock()          MU2.Lock()
// 	//    write(GLOB)         write(GLOB)
// 	//    MU2.Unlock()        MU2.Unlock()
// 	//    MU1.Unlock()        MU1.Unlock()     sleep          sleep
// 	//                                         Q1.Get()       Q1.Get()
// 	//                                         Q2.Get()       Q2.Get()
// 	//                                         MU2.Lock()     MU2.Lock()
// 	//                                         write(GLOB)    write(GLOB)
// 	//                                         MU2.Unlock()   MU2.Unlock()
// 	//
// 	int glob=0;
// 	void Putter(int num) {
// 		pthread_mutex_lock(&m);
// 		glob++;
// 		pthread_mutex_unlock(&m);
// 		if(num==0) {
// 			PC_SEM_PRODUCE(1);
// 			PC_SEM_PRODUCE(1);
// 		}
// 		else {
// 			PC_SEM_PRODUCE(2);
// 			PC_SEM_PRODUCE(2);
// 		}
// 		pthread_mutex_lock(&m);
// 		pthread_mutex_lock(&m1);
// 		if(glob!=-777) { }
// 		pthread_mutex_unlock(&m1);
// 		pthread_mutex_unlock(&m);
// 	}
// 	void Putter1(void ) {
// 		Putter(0);
// 	}
// 	void Putter2(void ) {
// 		Putter(1);
// 	}
// 	void Getter(void ) {
// 		usleep(100000);
// 		PC_SEM_CONSUME(1);
// 		PC_SEM_CONSUME(2);
// 		pthread_mutex_lock(&m1);
// 		glob++;
// 		pthread_mutex_unlock(&m1);
// 	}
// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Putter1;
// 		HANDLER Worker2=(HANDLER)Putter2;
// 		HANDLER Worker3=(HANDLER)Getter;
// 		HANDLER Worker4=(HANDLER)Getter;
// 		PC_SEM_RUN(2,4,2);
// 	}
// 	REGISTER_TEST(Run,38);
// }//namespace test38

// // test40: TN.Synchronization via Mutexes and PCQ. 4 threads. W/W
// namespace test40 {
// 	// Similar to test38 but with different order of events (due to sleep).
// 	// Putter1:            Putter2:           Getter1:       Getter2:
// 	//    MU1.Lock()          MU1.Lock()
// 	//    write(GLOB)         write(GLOB)
// 	//    MU1.Unlock()        MU1.Unlock()
// 	//    Q1.Put()            Q2.Put()
// 	//    Q1.Put()            Q2.Put()
// 	//                                        Q1.Get()       Q1.Get()
// 	//                                        Q2.Get()       Q2.Get()
// 	//                                        MU2.Lock()     MU2.Lock()
// 	//                                        write(GLOB)    write(GLOB)
// 	//                                        MU2.Unlock()   MU2.Unlock()
// 	//
// 	//    MU1.Lock()          MU1.Lock()
// 	//    MU2.Lock()          MU2.Lock()
// 	//    write(GLOB)         write(GLOB)
// 	//    MU2.Unlock()        MU2.Unlock()
// 	//    MU1.Unlock()        MU1.Unlock()
// 	int glob=0;
// 	void Putter(int num) {
// 		pthread_mutex_lock(&m);
// 		glob++;
// 		pthread_mutex_unlock(&m);
// 		if(num==0) {
// 			PC_SEM_PRODUCE(1);
// 			PC_SEM_PRODUCE(1);
// 		}
// 		else {
// 			PC_SEM_PRODUCE(2);
// 			PC_SEM_PRODUCE(2);
// 		}
// 		usleep(100000);
// 		pthread_mutex_lock(&m);
// 		pthread_mutex_lock(&m1);
// 		if(glob!=-777) { }
// 		pthread_mutex_unlock(&m1);
// 		pthread_mutex_unlock(&m);
// 	}
// 	void Putter1(void ) {
// 		Putter(0);
// 	}
// 	void Putter2(void ) {
// 		Putter(1);
// 	}
// 	void Getter(void ) {
// 		PC_SEM_CONSUME(1);
// 		PC_SEM_CONSUME(2);
// 		pthread_mutex_lock(&m1);
// 		glob++;
// 		pthread_mutex_unlock(&m1);
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Putter1;
// 		HANDLER Worker2=(HANDLER)Putter2;
// 		HANDLER Worker3=(HANDLER)Getter;
// 		HANDLER Worker4=(HANDLER)Getter;
// 		PC_SEM_RUN(2,4,2);
// 	}
// 	REGISTER_TEST(Run,40);
// }//namespace test40

// // test42:Using the same cond var several times.
// namespace test42 {
// 	int glob=0;
// 	int cond=0;

// 	void Worker1(void ) {
// 		glob=1;
// 		CV_SIGNAL(1);
// 		CV_WAIT(0);
// 		glob=3;

// 	}

// 	void Worker2(void ) {
// 		CV_WAIT(1);
// 		glob=2;
// 		CV_SIGNAL(0);
// 	}

// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,42);
// }//namespace test42

// // test43:TN.
// namespace test43 {
// 	//
// 	// Putter:            Getter:
// 	// 1. write
// 	// 2. Q.Put() --\     .
// 	// 3. read       \--> a. Q.Get()
// 	//                    b. read
// 	int glob=0;

// 	void Putter(void ) {
// 		glob=1;
// 		PC_SEM_PRODUCE(1);
// 		if(glob==1) { }
// 	}

// 	void Getter(void ) {
// 		PC_SEM_CONSUME(1);
// 		usleep(100000);
// 		if(glob==1) { }
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Putter;
// 		HANDLER Worker2=(HANDLER)Getter;
// 		PC_SEM_RUN(1,2,1);
// 	}

// 	REGISTER_TEST(Run,43);
// }//namespace test43

// // test44:TN.
// namespace test44 {
// 	//
// 	// Putter:            Getter:
// 	// 1. read
// 	// 2. Q.Put() --\     .
// 	// 3. MU.Lock()  \--> a. Q.Get()
// 	// 4. write
// 	// 5. MU.Unlock()
// 	//                    b. MU.Lock()
// 	//                    c. write
// 	//                    d. MU.Unlock();
// 	int glob=0;

// 	void Putter(void ) {
// 		if(glob==0) { }
// 		PC_SEM_PRODUCE(1);
// 		pthread_mutex_lock(&m);
// 		glob=1;
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Getter(void ) {
// 		PC_SEM_CONSUME(1);
// 		usleep(100000);
// 		pthread_mutex_lock(&m);
// 		glob=1;
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Putter;
// 		HANDLER Worker2=(HANDLER)Getter;
// 		PC_SEM_RUN(1,2,1);
// 	}
// 	REGISTER_TEST(Run,44);
// }//namespace test44

// // test45:TN.
// namespace test45 {
// 	//
// 	// Putter:            Getter:
// 	// 1. read
// 	// 2. Q.Put() --\     .
// 	// 3. MU.Lock()  \--> a. Q.Get()
// 	// 4. write
// 	// 5. MU.Unlock()
// 	//                    b. MU.Lock()
// 	//                    c. read
// 	//                    d. MU.Unlock();
// 	int glob=0;

// 	void Putter(void ) {
// 		if(glob==0) { }
// 		PC_SEM_PRODUCE(1);
// 		pthread_mutex_lock(&m);
// 		glob=1;
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Getter(void ) {
// 		PC_SEM_CONSUME(1);
// 		usleep(100000);
// 		pthread_mutex_lock(&m);
// 		if(glob<=1) { }
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Putter;
// 		HANDLER Worker2=(HANDLER)Getter;
// 		PC_SEM_RUN(1,2,1);
// 	}
// 	REGISTER_TEST(Run,45);
// }//namespace test45

// // test46:TP
// namespace test46 {
// 	//
// 	// First:                             Second:
// 	// 1. write
// 	// 2. MU.Lock()
// 	// 3. write
// 	// 4. MU.Unlock()                      (sleep)
// 	//                                    a. MU.Lock()
// 	//                                    b. write
// 	//                                    c. MU.Unlock();
// 	int glob=0;
// 	void Worker1(void ) {
// 		glob++;
// 		pthread_mutex_lock(&m);
// 		glob++;
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Worker2(void ) {
// 		usleep(480000);
// 		pthread_mutex_lock(&m);
// 		glob++;
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}

// 	REGISTER_TEST(Run,46);
// }//namespace test46

// // test47:TP.Not detected by pure happens-before detectors.
// namespace test47 {
// 	// A true race that can not be detected by a pure happens-before
// 	// race detector.
// 	//
// 	// First:                             Second:
// 	// 1. write
// 	// 2. MU.Lock()
// 	// 3. MU.Unlock()                      (sleep)
// 	//                                    a. MU.Lock()
// 	//                                    b. MU.Unlock();
// 	//                                    c. write
// 	int glob=0;
// 	void Worker1(void ) {
// 		glob=1;
// 		pthread_mutex_lock(&m);
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Worker2(void ) {
// 		usleep(480000);
// 		pthread_mutex_lock(&m);
// 		pthread_mutex_unlock(&m);
// 		glob++;
// 	}

// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}

// 	REGISTER_TEST(Run,47);
// }//namespace test47

// // test48: TP.Simple race (single write vs multiple reads)
// namespace test48 {
// 	// same as test10 but with single writer and  multiple readers
// 	// A simple data race between single writer and  multiple readers.
// 	// Write happens before Reads (enforced by sleep(1)),

// 	//
// 	// Writer:                    Readers:
// 	// 1. write(GLOB)             a. sleep(long enough so that GLOB
// 	//                                is most likely initialized by Writer)
// 	//                            b. read(GLOB)
// 	//
// 	//
// 	// Eraser algorithm does not detect the race here,
// 	// see Section 2.2 of http://citeseer.ist.psu.edu/savage97eraser.html.
// 	//
// 	int glob=0;
// 	void Writer(void ) {
// 		glob=3;
// 	}

// 	void Reader(void ) {
// 		usleep(100000);
// 		assert(glob!=-777);
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Writer;
// 		HANDLER Worker2=(HANDLER)Reader;
// 		HANDLER Worker3=(HANDLER)Reader;
// 		PTHREAD_RUN(3);
// 	}

// 	REGISTER_TEST(Run,48);
// }//namespace test48

// // test49:TP.Simple race (single write vs multiple reads).
// namespace test49 {
// 	// same as test10 but with multiple read operations done by a single reader
// 	// A simple data race between writer and readers.
// 	// Write happens before Read (enforced by sleep(1)),
// 	//
// 	// Writer:                    Reader:
// 	// 1. write(GLOB)             a. sleep(long enough so that GLOB
// 	//                                is most likely initialized by Writer)
// 	//                            b. read(GLOB)
// 	//                            c. read(GLOB)
// 	//                            d. read(GLOB)
// 	//                            e. read(GLOB)
// 	//
// 	//
// 	// Eraser algorithm does not detect the race here,
// 	// see Section 2.2 of http://citeseer.ist.psu.edu/savage97eraser.html.
// 	//
// 	int glob=0;
// 	void Writer(void ) {
// 		glob=3;
// 	}

// 	void Reader(void ) {
// 		usleep(100000);
// 		assert(glob!=-777);
// 		assert(glob!=-777);
// 		assert(glob!=-777);
// 		assert(glob!=-777);
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Writer;
// 		HANDLER Worker2=(HANDLER)Reader;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,49);
// }//namespace test49

// // test50:TP.Synchronization via CondVar
// namespace test50 {
// 	// Two last write accesses to GLOB are not synchronized
// 	//
// 	// Waiter:                      Waker:
// 	// 1. COND = 0
// 	// 2. Start(Waker)
// 	// 3. MU.Lock()                 a. write(GLOB)
// 	//                              b. MU.Lock()
// 	//                              c. COND = 1
// 	//                         /--- d. CV.Signal()
// 	//  4. while(COND != 1)   /     e. MU.Unlock()
// 	//       CV.Wait(MU) <---/
// 	//  5. MU.Unlock()
// 	//  6. write(GLOB)              f. MU.Lock()
// 	//                              g. write(GLOB)
// 	//                              h. MU.Unlock()
// 	int glob=0;
// 	int cond=0;
// 	void Waker(void ) {
// 		usleep(100000);
// 		glob=1;
// 		CV_SIGNAL(1);
// 		usleep(100000);
// 		pthread_mutex_lock(&m);
// 		glob=3;
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Waiter(void ) {
// 		CV_WAIT(1);
// 		glob=2;
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Waiter;
// 		HANDLER Worker2=(HANDLER)Waker;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,50);
// }//namespace test50

// // test53:TN.
// namespace test53 {
// 	// Correctly synchronized test, but the common lockset is empty.
// 	// The variable FLAG works as an implicit semaphore.
// 	// MSMHelgrind still does not complain since it does not maintain the lockset
// 	// at the exclusive state. But MSMProp1 does complain.
// 	// See also test54.
// 	//
// 	//
// 	// Initializer:                  Users
// 	// 1. MU1.Lock()
// 	// 2. write(GLOB)
// 	// 3. FLAG = true
// 	// 4. MU1.Unlock()
// 	//                               a. MU1.Lock()
// 	//                               b. f = FLAG;
// 	//                               c. MU1.Unlock()
// 	//                               d. if (!f) goto a.
// 	//                               e. MU2.Lock()
// 	//                               f. write(GLOB)
// 	//                               g. MU2.Unlock()
// 	//
// 	int glob=0;
// 	bool flag=false;
// 	void Initializer(void ) {
// 		pthread_mutex_lock(&m);
// 		glob=1000;
// 		flag=true;
// 		pthread_mutex_unlock(&m);
// 		usleep(100000);
// 	}

// 	void User(void ) {
// 		bool f=false;
// 		while(!f) {
// 			pthread_mutex_lock(&m);
// 			f=flag;
// 			pthread_mutex_unlock(&m);
// 			usleep(100000);
// 		}
// 		// at this point Initializer will not access GLOB again
// 		pthread_mutex_lock(&m1);
// 		assert(glob>=1000);
// 		glob++;
// 		pthread_mutex_unlock(&m1);
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Initializer;
// 		HANDLER Worker2=(HANDLER)User;
// 		HANDLER Worker3=(HANDLER)User;
// 		PTHREAD_RUN(3);
// 	}
// 	REGISTER_TEST(Run,53);
// }//namespace test53

// // // test55:TN.Synchronization with TryLock. Not easy for race detectors
// // namespace test55 {
// // 	// "Correct" synchronization with TryLock and Lock.
// // 	//
// // 	// This scheme is actually very risky.
// // 	// It is covered in detail in this video:
// // 	// http://youtube.com/watch?v=mrvAqvtWYb4 (slide 36, near 50-th minute).
// // 	int glob=0;
// // 	void Worker_Lock(void ) {
// // 		glob=1;
// // 		pthread_mutex_lock(&m);
// // 	}

// // 	void Worker_TryLock(void ) {
// // 		while(true) {
// // 			if(pthread_mutex_trylock(&m)!=0) {
// // 				pthread_mutex_unlock(&m);
// // 				break;
// // 			}else
// // 				pthread_mutex_unlock(&m);
// // 			usleep(100);
// // 		}
// // 		glob=2;

// // 	}

// // 	void Run() {
// // 		HANDLER Worker1=(HANDLER)Worker_Lock;
// // 		HANDLER Worker2=(HANDLER)Worker_TryLock;
// // 		PTHREAD_RUN(2);
// // 	}
// // 	REGISTER_TEST(Run,55);
// // }//namespace test55

// // test57:TN.
// namespace test57 {
// 	int glob=0;
// 	void Writer(void ) {
// 		for(int i=0;i<10;i++)
// 			ATOMIC_INCREMENT(&glob,1);
// 		usleep(1000);

// 	}
// 	void Reader(void ) {
// 		while(ATOMIC_LOAD(&glob)<20)
// 			usleep(1000);
// 	}
// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Writer;
// 		HANDLER Worker2=(HANDLER)Writer;
// 		HANDLER Worker3=(HANDLER)Reader;
// 		HANDLER Worker4=(HANDLER)Reader;
// 		PTHREAD_RUN(2);
// 		if(glob==20) { }
// 	}
// 	REGISTER_TEST(Run,57);
// }//namespace test57

// // test58:TP.User defined synchronization.
// namespace test58 {
// 	// Correctly synchronized test, but the common lockset is empty.
// 	// The variables FLAG1 and FLAG2 used for synchronization and as
// 	// temporary variables for swapping two global values.
// 	// Such kind of synchronization is rarely used (Excluded from all tests??).
// 	int glob1=1,glob2=2;
// 	int flag1=0,flag2=0;

// 	void Worker1(void ) {
// 		flag1=glob2;
// 		while(!flag2) ;
// 		glob2=flag2;
// 	}

// 	void Worker2(void ) {
// 		flag2=glob1;
// 		while(!flag1) ;
// 		glob1=flag1;
// 	}
// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,58);
// }//namespace test58

// // test60:TN.Correct synchronization using signal-wait
// namespace test60 {
// 	// same as test 58 but synchronized with signal-wait.
// 	int cond1=0,cond2=0;
// 	int glob1=1,glob2=2;
// 	int flag1=0,flag2=0;
// 	void Worker1(void ) {
// 		flag1=glob2;
// 		CV_SIGNAL_COND(cond1,1);
// 		CV_WAIT_COND(cond2,1);
// 		glob2=flag2;
// 	}
// 	void Worker2(void ) {
// 		flag2=glob1;
// 		CV_SIGNAL_COND(cond2,1);
// 		CV_WAIT_COND(cond1,1);
// 		glob1=flag1;
// 	}
// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,60);
// }//namespace test60

// // test61:Tn.Synchronization via Mutex as in happens-before
// namespace test61 {
// 	int glob=0;
// 	int *p1=NULL,*p2=NULL;
// 	// In this test Mutex lock/unlock operations introduce happens-before relation.
// 	// We annotate the code so that MU is treated as in pure happens-before detector.
// 	void Worker1(void ) {
// 		pthread_mutex_lock(&m);
// 		if(p1==NULL) {
// 			p1=&glob;
// 			*p1=1;
// 		}
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Worker2(void ) {
// 		bool done=false;
// 		while(!done) {
// 			pthread_mutex_lock(&m);
// 			if(p1) {
// 				done=true;
// 				p2=p1;
// 				p1=NULL;
// 			}
// 			pthread_mutex_unlock(&m);
// 		}
// 		*p2=2;
// 	}

// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,61);
// }//namespace test61

// // test64:TP.T2 happens-before T3, but T1 is independent. Reads in T1/T2.
// namespace test64 {
// 	// True race between T1 and T3:
// 	//
// 	// T1:                   T2:                   T3:
// 	// 1. read(GLOB)         (sleep)
// 	//                       a. read(GLOB)
// 	//                       b. Q.Put() ----->    A. Q.Get()
// 	//                                            B. write(GLOB)
// 	//
// 	//
// 	int glob=0;
// 	void Worker1(void ) {
// 		if(glob==0) { return; }
// 	}
// 	void Worker2(void ) {
// 		usleep(100000);
// 		if(glob==0) {  }
// 		PC_SEM_PRODUCE(1);
// 	}
// 	void Worker3(void ) {
// 		PC_SEM_CONSUME(1);
// 		glob=1;
// 	}
// 	void Run() {
// 		PC_SEM_RUN(1,3,1);
// 	}
// 	REGISTER_TEST(Run,64);
// }//namespace test64

// // test65:TP.T2 happens-before T3, but T1 is independent. Writes in T1/T2.
// namespace test65 {
// 	// Similar to test64.
// 	// True race between T1 and T3:
// 	//
// 	// T1:                   T2:                   T3:
// 	// 1. MU.Lock()
// 	// 2. write(GLOB)
// 	// 3. MU.Unlock()         (sleep)
// 	//                       a. MU.Lock()
// 	//                       b. write(GLOB)
// 	//                       c. MU.Unlock()
// 	//                       d. Q.Put() ----->    A. Q.Get()
// 	//                                            B. write(GLOB)
// 	//
// 	//
// 	int glob=0;
// 	void Worker1(void ) {
// 		pthread_mutex_lock(&m);
// 		glob++;
// 		pthread_mutex_unlock(&m);
// 	}
// 	void Worker2(void ) {
// 		usleep(100000);
// 		pthread_mutex_lock(&m);
// 		glob++;
// 		pthread_mutex_unlock(&m);
// 		PC_SEM_PRODUCE(1);
// 	}
// 	void Worker3(void ) {
// 		PC_SEM_CONSUME(1);
// 		glob=1;
// 	}
// 	void Run() {
// 		PC_SEM_RUN(1,3,1);
// 	}
// 	REGISTER_TEST(Run,65);
// }//namespace test65

// // // test67:TP.Race between Signaller1 and Waiter2
// // namespace test67{
// // 	// Here we create a happens-before arc between Signaller1 and Waiter2
// // 	// even though there should be no such arc.
// // 	// However, it's probably improssible (or just very hard) to avoid it.
// // 	int glob=0;
// // 	int c1=0,c2=0;
// // 	void Signaller1(void ) {
// // 		glob=1;
// // 		CV_SIGNAL_COND(c1,1);
// // 	}
// // 	void Signaller2(void ) {
// // 		usleep(100000);
// // 		CV_SIGNAL_COND(c2,1);
// // 	}
// // 	void Waiter1(void ) {
// // 		CV_WAIT_COND(c1,1);
// // 	}
// // 	void Waiter2(void ) {
// // 		CV_WAIT_COND(c2,1);
// // 		glob=2;
// // 	}
// // 	void Run() {
// // 		HANDLER Worker1=(HANDLER)Signaller1;
// // 		HANDLER Worker2=(HANDLER)Signaller2;
// // 		HANDLER Worker3=(HANDLER)Waiter1;
// // 		HANDLER Worker4=(HANDLER)Waiter2;
// // 		PTHREAD_RUN(4);
// // 	}
// // 	REGISTER_TEST(Run,67);
// // }//namespace test67



// // test68: TP. Writes are protected by MU, reads are not.
// namespace test68 {
// 	// In this test, all writes to GLOB are protected by a mutex
// 	// but some reads go unprotected.
// 	// This is certainly a race, but in some cases such code could occur in
// 	// a correct program. For example, the unprotected reads may be used
// 	// for showing statistics and are not required to be precise.
// 	int glob=0;
// 	int cond=0;
// 	const int n_writers=3;
// 	void Writer() {
// 		for(int i=0;i<100;i++) {
// 			pthread_mutex_lock(&m);
// 			glob++;
// 			pthread_mutex_unlock(&m);
// 		}
// 		//we are done
// 		pthread_mutex_lock(&m1);
// 		cond++;
// 		pthread_mutex_unlock(&m1);
// 	}
// 	void Reader() {
// 		bool cont=true;
// 		while(cont) {
// 			assert(glob>=0);

// 			//are we done?
// 			pthread_mutex_lock(&m1);
// 			if(cond==n_writers)
// 				cont=false;
// 			pthread_mutex_unlock(&m1);
// 			usleep(100);	
// 		}
// 	}
// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Reader;
// 		HANDLER Worker2=(HANDLER)Writer;
// 		HANDLER Worker3=(HANDLER)Writer;
// 		HANDLER Worker4=(HANDLER)Writer;
// 		PTHREAD_RUN(4);
// 	}
// 	REGISTER_TEST(Run,68);
// }//namespace test68

// // test79: TN. Swap
// namespace test79 {
// 	typedef std::map<int,int> map_t;
// 	map_t MAP;
// 	// Here we use swap to pass MAP between threads.
// 	// The synchronization is correct, but w/o ANNOTATE_PURE_HAPPENS_BEFORE_MUTEX
// 	// Helgrind will complain.

// 	void Worker1() {
// 		map_t tmp;
// 		pthread_mutex_lock(&m);
// 		//we swap the new empty map 'map' with 'MAP'
// 		MAP.swap(tmp);
// 		pthread_mutex_unlock(&m);
// 		//tmp (which is the old version of MAP) is destroyed here.
// 	}
// 	void Worker2() {
// 		pthread_mutex_lock(&m);
// 		MAP[1]++;
// 		pthread_mutex_unlock(&m);
// 	}
// 	void Run() {
// 		HANDLER Worker3=(HANDLER)Worker1;
// 		HANDLER Worker4=(HANDLER)Worker2;
// 		PTHREAD_RUN(4);
// 	}
// 	REGISTER_TEST(Run,79);
// }//namespace test79

// // test82: TN. Object published w/o synchronization.
// namespace test82 {
// 	// Writer creates a new object and makes the pointer visible to the Reader.
// 	// Reader waits until the object pointer is non-null and reads the object.
// 	//
// 	// On Core 2 Duo this test will sometimes (quite rarely) fail in
// 	// the CHECK below, at least if compiled with -O2.
// 	//
// 	// The sequence of events::
// 	// Thread1:                  Thread2:
// 	//   a. arr_[...] = ...
// 	//   b. foo[i]    = ...
// 	//                           A. ... = foo[i]; // non NULL
// 	//                           B. ... = arr_[...];
// 	//
// 	//  Since there is no proper synchronization, during the even (B)
// 	//  Thread2 may not see the result of the event (a).
// 	//  On x86 and x86_64 this happens due to compiler reordering instructions.
// 	//  On other arcitectures it may also happen due to cache inconsistency.
// 	class Foo {
// 	public:
// 		Foo() {
// 			idx_=rand()%1024;
// 			arr_[idx_]=7777;
// 		}

// 		static void check(volatile Foo *foo) {
// 			assert(foo->arr_[foo->idx_]==7777);
// 		}
// 	private:
// 		int idx_;
// 		int arr_[1024];
// 	};

// 	const int N=1000;
// 	static volatile Foo *foo[N];

// 	void Writer() {
// 		for(int i=0;i<N;i++) {
// 			foo[i]=new Foo;
// 			usleep(100);
// 		}
// 	}
// 	void Reader() {
// 		for(int i=0;i<N;i++) {
// 			while(!foo[i]) {
// 				pthread_mutex_lock(&m);
// 				pthread_mutex_unlock(&m);
// 			}
// 			// At this point Reader() sees the new value of foo[i]
//     		// but in very rare cases will not see the new value 
//     		//of foo[i]->arr_.Thus this CHECK will sometimes fail.
//     		Foo::check(foo[i]);
// 		}
// 	}
// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Writer;
// 		HANDLER Worker2=(HANDLER)Reader;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,82);
// }//namespace test82

// // test83:TN.Object published w/o synchronization (simple version)
// namespace test83 {
// 	// A simplified version of test83 (example of a wrong code).
// 	// This test, though incorrect, will almost never fail.	
// 	volatile static int *ptr=NULL;

// 	void Writer() {
// 		usleep(100);
// 		ptr=new int(777);
// 	}
// 	void Reader() {
// 		while(!ptr) {
// 			pthread_mutex_lock(&m);
// 			pthread_mutex_unlock(&m);
// 		}
// 		if(*ptr==777) { return ; }
// 	}
// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Writer;
// 		HANDLER Worker2=(HANDLER)Reader;
// 		PTHREAD_RUN(2);	
// 	}
// 	REGISTER_TEST(Run,83);
// }//namespace test83

// // test84: TN. True race (regression test for a bug related to atomics)
// namespace test84 {
// 	// Helgrind should not create HB arcs for the bus lock even when
// 	// --pure-happens-before=yes is used.
// 	// Bug found in by Bart Van Assche, the test is taken from
// 	// valgrind file drd/tests/atomic_var.c.
// 	static int s_x=0;
// 	/* s_dummy[] ensures that s_x and s_y are not in the same cache line. */
// 	static char s_dummy[512] = {0};
// 	static int s_y=0;

// 	void Worker1() {
// 		s_y=1;
// 		ATOMIC_INCREMENT(&s_x,1);
// 	}
// 	void Worker2() {
// 		while(ATOMIC_INCREMENT(&s_x,0)==0) ;
// 		if(s_y>=0) { return ; }
// 	}
// 	void Run() {
// 		if(s_dummy[0]==0) { } //avoid compiler warning about 's_dummy unused'
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,84);
// }//namespace test84

// // test85: TP. Benigh race
// namespace test85 {
// 	// Test for race inside DTOR: racey write to vptr. Benign.
// 	// This test shows a racey access to vptr (the pointer to vtbl).
// 	// We have class A and class B derived from A.
// 	// Both classes have a virtual function f() and a virtual DTOR.
// 	// We create an object 'A *a = new B'
// 	// and pass this object from Thread1 to Thread2.
// 	// Thread2 calls a->f(). This call reads a->vtpr.
// 	// Thread1 deletes the object. B::~B waits untill the object can be destroyed
// 	// (flag_stopped == true) but at the very beginning of B::~B
// 	// a->vptr is written to.
// 	// So, we have a race on a->vptr.
// 	// On this particular test this race is benign, but HarmfulRaceInDtor shows
// 	// how such race could harm.
// 	//
// 	//
// 	//
// 	// Threa1:                                            Thread2:
// 	// 1. A a* = new B;
// 	// 2. Q.Put(a); ------------\                         .
// 	//                           \-------------------->   a. a = Q.Get();
// 	//                                                    b. a->f();
// 	//                                       /---------   c. flag_stopped = true;
// 	// 3. delete a;                         /
// 	//    waits untill flag_stopped <------/
// 	//    inside the dtor
// 	//
// 	ProducerConsumerQueue pcq(1);
// 	bool flag_stopped=false;
// 	Mutex mu;
// 	struct A {
// 		A() {
// 			printf("A:A()\n");
// 		}
// 		virtual ~A() {
// 			printf("A:~A()\n");	
// 		}
// 		virtual void f() {}
// 	};

// 	struct B:A {
// 		B() {
// 			printf("B:B()\n");
// 		}
// 		virtual ~B() {
// 			// The race is here.    <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// 			printf("B:B~()\n");
// 			//wait until flag_stopped is true.
// 			mu.LockWhen(Condition(&ArgIsTrue,&flag_stopped));
// 			mu.Unlock();
// 			printf("B:B~() done\n");
// 		}
// 		virtual void f() {}
// 	};

// 	void Waiter() {
// 		A *a = new B;
// 	  	printf("Waiter: B created\n");
// 	  	pcq.Produce(a);
// 	  	usleep(200000); // so that Waker calls a->f() first.
// 	  	printf("Waiter: deleting B\n");
// 	  	delete a;
// 	  	printf("Waiter: B deleted\n");
// 	  	usleep(200000);
// 	  	printf("Waiter: done\n");
// 	}
// 	void Waker() {
// 		A *a=reinterpret_cast<A*>(pcq.Consume());
// 		printf("Worker: got A\n");
//   		a->f();
//   		mu.Lock();
//   		flag_stopped = true;
//   		mu.Unlock();
//   		usleep(200000);
//   		printf("Worker: done\n");
// 	}

// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Waiter;
// 		HANDLER Worker2=(HANDLER)Waker;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,85);
// }//namespace test85

// // test86: TP. Harmful race
// namespace test86 {
// 	// A variation of BenignRaceInDtor where the race is harmful.
// 	// Race on vptr. Will run A::F() or B::F() depending on the timing.
// 	class A {
// 	public:
// 		A():done_(false) {}
// 		virtual ~A() {
// 			while(true) {
// 				MutexLock lock(&mu_);
// 				if(done_)
// 					break;
// 			}
// 		}
// 		virtual void F() {
// 			printf("A::F()\n");
// 		}
// 		void Done() {
// 			MutexLock lock(&mu_);
// 			done_=true;
// 		}
// 	private:
// 		Mutex mu_;
// 		bool done_;
// 	};

// 	class B:public A{
// 	public:
// 		virtual void F() {
// 			printf("B:F()\n");
// 		}
// 	};

// 	A *a;
// 	void Worker1() {
// 		usleep(10000);
// 		a->F();
// 		a->Done();
// 	}
// 	void Worker2() {
// 		delete a;
// 	}
// 	void Run() {
// 		a=new B;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,86);
// }//namespace test86

// // test89:
// namespace test89 {
// 	// Simlpe races with different objects (stack, heap globals; scalars, structs).
// 	// Also, if run with --trace-level=2 this test will show a sequence of
// 	// CTOR and DTOR calls.
// 	struct STRUCT {
//   		int a, b, c;
// 	};

// 	struct A {
//   		int a;
//   		A() { a = 1; }
//   		virtual ~A() { a = 4; }
// 	};
// 	struct B : A {
//   		B()  { if(a == 1) { } }
//   		virtual ~B() { if(a == 3) { } }
// 	};
// 	struct C : B {
//   		C()  { a = 2; }
//   		virtual ~C() { a = 3; }
// 	};
// 	int glob=0;
// 	int *stack=NULL;
// 	STRUCT GLOB_STRUCT;
// 	STRUCT *STACK_STRUCT=NULL;
// 	STRUCT *HEAP_STRUCT=NULL;

// 	void Worker() {
// 		glob=1;
// 		*stack=1;
// 		GLOB_STRUCT.b=1;
// 		STACK_STRUCT->b=1;
// 		HEAP_STRUCT->b=1;
// 	}

// 	void Run() {
// 		int stack_var=0;
// 		stack=&stack_var;

// 		STRUCT stack_struct;
// 		STACK_STRUCT=&stack_struct;

// 		HEAP_STRUCT=new STRUCT;

// 		HANDLER Worker1=(HANDLER)Worker;
// 		HANDLER Worker2=(HANDLER)Worker;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,89);
// }//namespace test89

// // test90: TN. Test for a safely-published pointer (read-only).
// namespace test90 {
// 	// The Publisher creates an object and safely publishes it under a mutex.
// 	// Readers access the object read-only.
// 	// See also test91.
// 	//
// 	// Without annotations Helgrind will issue a false positive in Reader().
// 	//
// 	int *glob=NULL;
// 	Mutex mu;
// 	StealthNotification n1;

// 	void Publisher() {
// 		mu.Lock();
// 		glob=(int *)malloc(128*sizeof(int));
// 		glob[42]=777;
// 		mu.Unlock();
// 		n1.signal();
// 		usleep(200000);
// 	}
// 	void Reader() {
// 		n1.wait();
// 		while(true) {
// 			mu.Lock();
// 			int *p=glob;
// 			mu.Unlock();
// 			if(p) {
// 				if(p[42]==777) { }
// 				break;
// 			}
// 		}
// 	}
// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Publisher;
// 		HANDLER Worker2=(HANDLER)Reader;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,90);
// }//namespace test90

// // test91: TN. Test for a safely-published pointer (read-write).
// namespace test91 {
// 	// Similar to test90.
// 	// The Publisher creates an object and safely publishes it under a mutex MU1.
// 	// Accessors get the object under MU1 and access it (read/write) under MU2.
// 	//
// 	// Without annotations Helgrind will issue a false positive in Accessor().
// 	//
// 	int *glob=NULL;
// 	Mutex mu1,mu2;
// 	void Publisher() {
// 		mu1.Lock();
// 		glob=(int*)malloc(128 * sizeof(int));
// 		glob[42]=777;
// 		mu1.Unlock();
// 	}
// 	void Accessor() {
// 		usleep(100000);
// 		while(true) {
// 			mu1.Lock();
// 			int *p=glob;
// 			mu1.Unlock();
// 			if(p) {
// 				mu2.Lock();
// 				p[42]++;
// 				assert(p[42]>777);
// 				mu2.Unlock();
// 				break;
// 			}
// 		}
// 	}
// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Publisher;
// 		HANDLER Worker2=(HANDLER)Accessor;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,91);
// }//namespace test91

// // test92: TN. Test for a safely-published pointer (read-write)
// namespace test92 {
// 	struct ObjType {
//   		int arr[10];
// 	};

// 	ObjType *GLOB=0;
// 	void Publisher() {
// 		pthread_mutex_lock(&m);
// 		GLOB=new ObjType;
// 		for(int i=0;i<10;i++)
// 			GLOB->arr[i]=777;
// 		pthread_mutex_unlock(&m);
// 	}
// 	void Accessor(int index) {
// 		while(true) {
// 			pthread_mutex_lock(&m);
// 			ObjType *p=GLOB;
// 			pthread_mutex_unlock(&m);
// 			if(p) {
// 				pthread_mutex_lock(&m1);
// 				p->arr[index]++;
// 				assert(p->arr[index]==778);
// 				pthread_mutex_unlock(&m1);
// 				break;
// 			}
// 		}
// 	}
// 	void Accessor0() { Accessor(0); }
// 	void Accessor5() { Accessor(5); }
// 	void Accessor9() { Accessor(9); }
// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Publisher;
// 		HANDLER Worker2=(HANDLER)Accessor0;
// 		HANDLER Worker3=(HANDLER)Accessor5;
// 		HANDLER Worker4=(HANDLER)Accessor9;

// 		PTHREAD_RUN(4);
// 	}
// 	REGISTER_TEST(Run,92);
// }//namespace test92

// // test94: TP. Check do_cv_signal/fake segment logic
// namespace test94 {
// 	int glob;
// 	int cond1=0,cond2=0;
// 	CondVar cv1,cv2;
// 	Mutex mu1,mu2;
// 	StealthNotification n1,n2,n3;
// 	void Worker1() {
// 		n2.wait(); //make sure the waiter blocks.
// 		glob=1;

// 		mu1.Lock();
// 		cond1=1;
// 		cv1.Signal();
// 		mu1.Unlock();
// 		n1.signal();
// 	}

// 	void Worker2() {
// 		n1.wait(); //make sure cv2.signal() "happens after" cv1.signal()
// 		n3.wait(); //make sure the waiter blocks.

// 		mu2.Lock();
// 		cond2=1;
// 		cv2.Signal();
// 		mu2.Unlock();
// 	}

// 	void Worker3() {
// 		mu1.Lock();
// 		n2.signal();
// 		while(cond1!=1)
// 			cv1.Wait(&mu1);
// 		mu1.Unlock();
// 	}

// 	void Worker4() {
// 		mu2.Lock();
// 		n3.signal();
// 		while(cond2!=1)
// 			cv2.Wait(&mu2);
// 		mu2.Unlock();
// 		glob=2;
// 	} void Run() {PTHREAD_RUN(4); }REGISTER_TEST(Run,94);
// }//namespace test94

// // test95: TP. 
// namespace test95 {
// 	int glob=0;
// 	int cond1=0,cond2=0;
// 	CondVar cv1,cv2;
// 	Mutex mu1,mu2;
// 	void Worker1() {
// 		usleep(400000);//make sure CV2.Signal() "happens before" CV1.Signal()
// 		usleep(100000);//make sure the waiter blocks
// 		glob=1;
// 		mu1.Lock();
// 		cond1=1;
// 		cv1.Signal();//CV1.Signal()
// 		mu1.Unlock();
// 	}
// 	void Worker2() {
// 		usleep(100000);
// 		mu2.Lock();
// 		cond2=1;
// 		cv2.Signal();//CV2.Signal()
// 		mu2.Unlock();	
// 	}
// 	void Worker3() {
// 		mu1.Lock();
// 		while(cond1!=1)
// 			cv1.Wait(&mu1);
// 		mu1.Unlock();
// 	}
// 	void Worker4() {
// 		mu2.Lock();
// 		while(cond2!=1)
// 			cv2.Wait(&mu2);
// 		mu2.Unlock();
// 		glob=2;
// 	}
// 	void Run() {
// 		PTHREAD_RUN(4);
// 	}
// 	REGISTER_TEST(Run,95);
// }//namespace test95

// // test96: TN. tricky LockSet behaviour
// namespace test96 {
// 	int glob=0;
// 	void Worker1() {
// 		pthread_mutex_lock(&m);
// 		pthread_mutex_lock(&m1);
// 		glob++;
// 		pthread_mutex_unlock(&m1);
// 		pthread_mutex_unlock(&m);
// 	}
// 	void Worker2() {
// 		pthread_mutex_lock(&m1);
// 		pthread_mutex_lock(&m2);
// 		glob++;
// 		pthread_mutex_unlock(&m2);
// 		pthread_mutex_unlock(&m1);
// 	}
// 	void Worker3() {
// 		pthread_mutex_lock(&m);
// 		pthread_mutex_lock(&m2);
// 		glob++;
// 		pthread_mutex_unlock(&m2);
// 		pthread_mutex_unlock(&m);
// 	}
// 	void Run() {
// 		PTHREAD_RUN(3);
// 	}
// 	REGISTER_TEST(Run,96);
// }//namespace test96

// // test100:Test for initialization bit.
// namespace test100 {
// 	int glob1=0;
// 	int glob2=0;
// 	int glob3=0;
// 	int glob4=0;
// 	void Worker1() {
// 		glob1=1;assert(glob1!=-1);
// 		glob2=1;
// 		glob3=1;assert(glob3!=-1);
// 		glob4=1;
// 	}
// 	void Worker2() {
// 		usleep(100000);
// 		assert(glob1!=-1);
// 		assert(glob2!=-1);
// 		glob3=1;
// 		glob4=1;
// 	}
// 	void Worker3() {
// 		usleep(100000);
// 		glob1=1;
// 		glob2=1;
// 		assert(glob3!=-1);
// 		assert(glob4!=-1);
// 	}
// 	void Run() {
// 		PTHREAD_RUN(3);
// 	}
// 	REGISTER_TEST(Run,100);
// }//namespace test100

// // test101: TN. Two signals and two waits
// namespace test101 {
// 	int glob=0;
// 	int cond1=0,cond2=0;
// 	void Signaller() {
// 		usleep(100000);
// 		CV_SIGNAL_COND(cond1,1);
// 		glob=1;
// 		usleep(500000);
// 		CV_SIGNAL_COND(cond2,1);
// 	}
// 	void Waiter() {
// 		CV_WAIT_COND(cond1,1);
// 		CV_WAIT_COND(cond2,1);
// 		glob=2;
// 	}
// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Signaller;
// 		HANDLER Worker2=(HANDLER)Waiter;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,101);
// }//namespace test101

// // test102: TP. 
// namespace test102 {
// 	const int HG_CACHELINE_SIZE=64;
// 	Mutex mu;
// 	const int ARRAY_SIZE=HG_CACHELINE_SIZE * 4 / sizeof(int);
// 	int array[ARRAY_SIZE+1];

//   	// We use sizeof(array) == 4 * HG_CACHELINE_SIZE to be sure that GLOB points
//     // to a memory inside a CacheLineZ which is inside array's memory range
//     int *glob=&array[ARRAY_SIZE/2];

//     StealthNotification n1,n2,n3;

//     void Reader() {
//     	n1.wait();
//     	assert(777==glob[0]);
//     	n2.signal();
//     	n3.wait();
//     	assert(777==glob[1]);
//     }

//     void Run() {
//     	Thread t(Reader);
//     	t.Start();
//     	glob[0]=777;
//     	n1.signal();
//     	n2.wait();
//     	glob[1]=777;
//     	n3.signal();
//     	t.Join();
//     }
//     REGISTER_TEST(Run,102);
// }//namespace test102

// // test104: TP. Simple race (write vs write). Heap mem.
// namespace test104 {
// 	int *glob=NULL;
// 	void Worker() { glob[42]=1; }
// 	void Parent() {
// 		PTHREAD_ID(1);
// 		HANDLER Worker1=(HANDLER)Worker;
// 		PTHREAD_CREATE(1);
// 		usleep(100000);
// 		glob[42]=2;
// 		PTHREAD_JOIN(1);
// 	}
// 	void Run() {
// 		glob=(int *)malloc(128 *sizeof(int));
// 		glob[42]=0;
// 		Parent();
// 		free(glob);
// 	}
// 	REGISTER_TEST(Run,104);
// }//namespace test104

// // test108: TN. Initialization of static object.
// namespace test108 {
// 	// Here we have a function-level static object.
// 	// Starting from gcc 4 this is therad safe,
// 	// but is is not thread safe with many other compilers.
// 	//
// 	// Helgrind/ThreadSanitizer supports this kind of initialization by
// 	// intercepting __cxa_guard_acquire/__cxa_guard_release
// 	// and ignoring all accesses between them.
// 	// pthread_once is supported in the same manner.
// 	class Foo {
// 	public:
// 		Foo() {a_=42;}
// 		void Check() const {assert(a_==42); }
// 	private:
// 		int a_;
// 	};
// 	const Foo *GetFoo() {
// 		static const Foo *foo=new Foo;
// 		return foo;
// 	}
// 	void Worker1() {
// 		GetFoo();
// 	}
// 	void Worker2() {
// 		usleep(200000);
// 		const Foo *foo=GetFoo();
// 		foo->Check();
// 	}
// 	void Run() {
// 		HANDLER Worker3=(HANDLER)Worker2;
// 		PTHREAD_RUN(3);
// 	}
// 	REGISTER_TEST(Run,108);
// }//namespace test108

// // test109: TN. Checking happens before between parent and child threads.
// namespace test109 {
// 	// Check that the detector correctly connects
// 	//   pthread_create with the new thread
// 	// and
// 	//   thread exit with pthread_join
// 	const int N=4;
// 	static int glob[N];
// 	void Worker(void *a) {
// 		usleep(100000);
// 		int *arg=(int *)a;
// 		(*arg)++;
// 	}

// 	void Run() {
// 		Thread *t[N];
// 		for(int i=0;i<N;i++) {
// 			t[i]=new Thread(Worker,&glob[i]);
// 		}
// 		for(int i=0;i<N;i++) {
// 			glob[i]=1;
// 			t[i]->Start();
// 		}
// 		for(int i=0;i<N;i++) {
// 			t[i]->Join();
// 			glob[i]++;
// 		}
// 		for(int i=0;i<N;i++)
// 			delete t[i];
// 	}
// 	REGISTER_TEST(Run,109);
// }//namespace test109

// // test111: TN. Unit test for a bug related to stack handling.
// namespace test111 {
// 	char *glob=NULL;
// 	bool cond=false;
// 	Mutex mu;
// 	const int N=3000;
// 	void write_to_p(char *p,int val) {
// 		for(int i=0;i<N;i++)
// 			p[i]=val;
// 	}

// 	void f1() {
// 		char some_stack[N];
// 		write_to_p(some_stack,1);
// 		mu.LockWhen(Condition(&ArgIsTrue,&cond));
// 		mu.Unlock();
// 	}

// 	void f2() {
// 		char some_stack[N];
// 		char some_more_stack[N];
// 		write_to_p(some_stack,2);
// 		write_to_p(some_more_stack,2);
// 	}
// 	void f0() { f2(); }

// 	void Worker1() {
// 		f0();
// 		f1();
// 		f2();
// 	}
// 	void Worker2() {
// 		Worker1();
// 	}
// 	void Worker3() {
// 		usleep(100000);
// 		mu.Lock();
// 		cond=true;
// 		mu.Unlock();
// 	}
// 	void Run() {
// 		PTHREAD_RUN(3);
// 	}
// 	REGISTER_TEST(Run,111);
// }//namespace test111

// // test113: TN. A lot of lock/unlock calls.Many locks
// namespace test113 {
// 	const int kNumIter=1000;
// 	const int kNumLocks=7;
// 	Mutex mu[kNumLocks];
// 	void Worker1() {
// 		for(int i=0;i<kNumIter;i++) {
// 			for(int j=0;j<kNumLocks;j++)
// 				if(i & (1<<j)) mu[j].Lock();
// 			for(int j=kNumLocks-1;j>-1;j--)
// 				if(i & (1<<j)) mu[j].Unlock();
// 		}
// 	}
// 	void Run() {
// 		PTHREAD_RUN(1);
// 	}
// 	REGISTER_TEST(Run,113);
// }//namespace test113

// // test114: TN. Recursive static initialization
// namespace test114 {
// 	int Bar() {
// 		static int bar=1;
// 		return bar;
// 	}
// 	int Foo() {
// 		static int foo=Bar();
// 		return foo;
// 	}
// 	void Worker() {
// 		static int x=Foo();
// 		assert(x!=-1);
// 	}
// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Worker;
// 		HANDLER Worker2=(HANDLER)Worker;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,114);
// }//namespace test114

// // test117: TN. Many calls to function-scope static inititialization
// namespace test117 {
// 	const int N=5;
// 	int Foo() {
// 		usleep(20000);
// 		return 1;
// 	}
// 	void Worker() {
// 		static int foo=Foo();
// 		assert(foo!=-1);
// 	}
// 	void Run() {
// 		Thread *t[N];
// 		for(int i=0;i<N;i++)
// 			t[i]=new Thread(Worker);
// 		for(int i=0;i<N;i++)
// 			t[i]->Start();
// 		for(int i=0;i<N;i++)
// 			t[i]->Join();
// 		for(int i=0;i<N;i++)
// 			delete t[i];
// 	}
// 	REGISTER_TEST(Run,117);
// }//namespace test117

// // test119: TP. Testing that malloc does not introduce any HB arc.
// namespace test119 {
// 	int glob=0;
// 	void Worker1() {
// 		glob=1;
// 		free(malloc(123));
// 	}
// 	void Worker2() {
// 		usleep(100000);
// 		free(malloc(345));
// 		glob=2;
// 	}
// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,119);
// }//namespace test119

// // test120: TP. Thread1: write then read. Thread2: read.
// namespace test120 {
// 	int glob=0;
// 	void Worker1() {
// 		glob=1;
// 		assert(glob!=-1);
// 	}
// 	void Worker2() {
// 		usleep(100000);
// 		assert(glob!=-1);
// 	}
// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,2);
// }//namespace test120

// test121: TN. DoubleCheckedLocking
namespace test121 {
	struct Foo {
		uintptr_t padding1[16];
		volatile uintptr_t a;
		uintptr_t padding2[16];
	};

	Foo *foo;

	void InitMe() {
		if(!foo) {
			pthread_mutex_lock(&m);
			if(!foo)
				foo=new Foo;
			foo->a=42;
			pthread_mutex_unlock(&m);
		}
	}
	void UseMe() {
		InitMe();
		assert(foo);
		if(foo->a!=42)
			printf("foo->a = %d (should be 42)\n",(int)foo->a);
	}
	void Worker1() { UseMe(); }
	void Worker2() { UseMe(); }
	void Worker3() { UseMe(); }
	void Run() {
		PTHREAD_RUN(3);
		delete foo;
	}
	REGISTER_TEST(Run,121);
}//namespace test121

// // test122: TN. DoubleCheckedLocking2
// namespace test122 {
// 	struct Foo {
// 		uintptr_t padding1[16];
// 		uintptr_t a;
// 		uintptr_t padding2[16];
// 	};
// 	Foo *foo;
// 	void InitMe() {
// 		if(foo) return ;
// 		Foo *x=new Foo;
// 		x->a=42;
// 		pthread_mutex_lock(&m);
// 		if(!foo) {
// 			foo=x;
// 			x=NULL;
// 		}
// 		pthread_mutex_unlock(&m);
// 		if(x)
// 			delete x;
// 	}
// 	void Worker() {
// 		InitMe();
// 		assert(foo);
// 		assert(foo->a==42);
// 	}
// 	void Run() {
// 		HANDLER Worker1=(HANDLER)Worker;
// 		HANDLER Worker2=(HANDLER)Worker;
// 		HANDLER Worker3=(HANDLER)Worker;
// 		PTHREAD_RUN(3);
// 		delete foo;
// 	}
// 	REGISTER_TEST(Run,122);
// }//namespace test122

// // // test127:unlocking a mutex locked by another thread.
// // namespace test127 {
// // 	Mutex *mu;
// // 	void Worker1() {
// // 		mu->Lock();
// // 		usleep(1); //avoid tail call elimination-for debug
// // 	}
// // 	void Worker2() {
// // 		usleep(100000);
// // 		mu->Unlock();
// // 		usleep(1); //avoid tail call elimination-for debug
// // 	}
// // 	void Run() {
// // 		mu=new Mutex;
// // 		PTHREAD_RUN(2);
// // 		delete mu;
// // 	}
// // 	REGISTER_TEST(Run,127);
// // }//namespace test127

// // test137: TP. Races on stack variables.
// namespace test137 {
// 	int glob=0;
// 	ProducerConsumerQueue q(4);
// 	void Worker() {
// 		int stack;
// 		int *tmp=(int *)q.Consume();
// 		(*tmp)++;
// 		int *racy=&stack;
// 		q.Produce(racy);
// 		(*racy)++;
// 		usleep(150000);
// 		// We may miss the races if we sleep less due to die_memory events...
// 	}
// 	void Run() {
// 		int tmp=0;
// 		q.Produce(&tmp);
// 		ThreadArray t(Worker,Worker,Worker,Worker);
// 		t.Start();
// 		t.Join();
// 		q.Consume();
// 	}
// 	REGISTER_TEST(Run,137);
// }//namespace test137

// // test148: TP. 3 threads, h-b hides race between T1 and T3.
// namespace test148 {
// 	int glob=0;
// 	int cond=0;
// 	Mutex mu;
// 	CondVar cv;
// 	void Signaller() {
// 		usleep(200000);
// 		glob=1;
// 		mu.Lock();
// 		cond=1;
// 		cv.Signal();
// 		mu.Unlock();
// 	}
// 	void Waiter() {
// 		mu.Lock();
// 		while(cond==0)
// 			cv.Wait(&mu);
// 		glob=2;
// 		mu.Unlock();
// 	}
// 	void Racer() {
// 		usleep(300000);
// 		mu.Lock();
// 		glob=3;
// 		mu.Unlock();
// 	}
// 	void Run() {
// 		ThreadArray t(Signaller,Waiter,Racer);
// 		t.Start();
// 		t.Join();
// 	}
// 	REGISTER_TEST(Run,148);
// }//namespace test148

// // test149: TN. allocate and memset lots of of mem in several chunks.
// namespace test149 {
// 	void Run() {
// 		int kChunkSize=1<<16;
// 		printf("test149:malloc 8x%dM\n",kChunkSize/(1<<10));
// 		void *mem[8];
// 		for(int i=0;i<8;i++) {
// 			mem[i]=malloc(kChunkSize);
// 			memset(mem[i],0,kChunkSize);
// 			printf("+");
// 		}
// 		for(int i=0;i<8;i++) {
// 			free(mem[i]);
// 			printf("-");
// 		}
// 		printf(" Done\n");
// 	}
// 	REGISTER_TEST(Run,149);
// }//namespace test149

// // test150: TP. race which is detected after one of the thread has joined.
// namespace test150 {
// 	int glob=0;
// 	StealthNotification n;
// 	void Worker1() { glob++; }
// 	void Worker2() {
// 		n.wait();
// 		glob++;
// 	}
// 	void Run() {
// 		Thread t1(Worker1),t2(Worker2);
// 		t1.Start();
// 		t2.Start();
// 		t1.Join();
// 		n.signal();
// 		t2.Join();
// 	}
// 	REGISTER_TEST(Run,150);
// }//namespace test150

// // test154: TP. long test with lots of races
// namespace test154 {
// 	const int kNumIter=100;
// 	const int kArraySize=100;
// 	int *arr;
// 	void RacyAccess(int *a) {
// 		(*a)++;
// 	}
// 	void RacyLoop() {
// 		for(int j=0;j<kArraySize;j++)	
// 			RacyAccess(&arr[j]);
// 	}
// 	void Worker1() {
// 		for(int i=0;i<kNumIter;i++) {
// 			usleep(1);
// 			RacyLoop();
// 		}
// 	}
// 	void Run() {
// 		arr=new int[kArraySize];
// 		HANDLER Worker2=(HANDLER)Worker1;
// 		PTHREAD_RUN(2);
// 		delete []arr;
// 	}
// 	REGISTER_TEST(Run,154);
// }//namespace test154

// // test162: TP. 
// namespace test162 {
// 	int *glob=NULL;
// 	const int kIdx=77;
// 	StealthNotification n;
// 	void Read() {
// 		if(glob[kIdx]==777) { }
// 		n.signal();
// 	}
// 	void Free() {
// 		n.wait();
// 		free(glob);
// 	}
// 	void Run() {
// 		glob=(int *)malloc(100 * sizeof(int));
// 		glob[kIdx]=777;
// 		HANDLER Worker1=(HANDLER)Read;
// 		HANDLER Worker2=(HANDLER)Free;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,162);
// }//namespace test162

// // test163: TP.
// namespace test163 {
// 	int *glob=NULL;
// 	const int kIdx=77;
// 	StealthNotification n;
// 	void Free() {
// 		free(glob);
// 		n.signal();
// 	}
// 	void Read() {
// 		n.wait();
// 		if(glob[kIdx]!=0x123456) { }
// 	}
// 	void Run() {
// 		glob=(int *)malloc(100 * sizeof(int));
// 		glob[kIdx]=777;
// 		HANDLER Worker1=(HANDLER)Free;
// 		HANDLER Worker2=(HANDLER)Read;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,163);
// }//namespace test163

// // test400: TN. Demo of a simple false positive.
// namespace test400 {
// 	vector<int> *vec;

// 	void InitAllBeforeStartingThreads() {
// 	  	vec = new vector<int>;
// 	  	vec->push_back(1);
// 	  	vec->push_back(2);
// 	}

// 	void Worker1() {
// 		pthread_mutex_lock(&m);
// 		vec->pop_back();
// 		pthread_mutex_unlock(&m);
// 	}

// 	void Worker2() {
// 		pthread_mutex_lock(&m);
// 		vec->pop_back();
// 		pthread_mutex_unlock(&m);
// 	}

// 	size_t NumberOfElementsLeft() {
// 	  	pthread_mutex_lock(&m);
// 	  	size_t s=vec->size();
// 		pthread_mutex_unlock(&m);
// 		return s;
// 	}
// 	void WaitForAllThreadsToFinish_InefficientAndUnfriendly() {
//   		while(NumberOfElementsLeft()) {
//     		; // sleep or print or do nothing.
//   		}
// 	  	// It is now safe to access vec w/o lock.
// 	  	// But a hybrid detector (like ThreadSanitizer) can't see it.
// 	  	// Solutions:
// 	  	//   1. Use pure happens-before detector (e.g. "tsan --pure-happens-before")
// 	  	//   2. Call ANNOTATE_PURE_HAPPENS_BEFORE_MUTEX(&mu)
// 	  	//      in InitAllBeforeStartingThreads()
// 	  	//   3. (preferred) Use WaitForAllThreadsToFinish_Good() (see below).
//   		if(vec->empty()) { }
//   		delete vec;
// 	}

// 	void Run() {
// 		InitAllBeforeStartingThreads();
// 		Thread t1(Worker1),t2(Worker2);
// 		t1.Start();
// 		t2.Start();
// 		WaitForAllThreadsToFinish_InefficientAndUnfriendly();
// 		t1.Join();
// 		t2.Join();
// 	}
// 	REGISTER_TEST(Run,400);
// }//namespace test400

// // test506: TN. massive HB's using Barriers
// namespace test506 {
// 	int glob=0;
// 	const int N=8,ITERATOR=100;
// 	Barrier *barrier[ITERATOR];
// 	Mutex mu;

// 	void Worker() {
// 		for(int i=0;i<ITERATOR;i++) {
// 			mu.Lock();
// 			glob++;
// 			mu.Unlock();
// 			barrier[i]->Block();
// 		}
// 	}

// 	void Run() {
// 		for(int i=0;i<ITERATOR;i++)
// 			barrier[i]=new Barrier(N);
// 		{
// 			ThreadPool pool(N);
// 			pool.StartWorkers();
// 			for(int i=0;i<N;i++)
// 				pool.Add(NewCallBack(Worker));
// 		}
// 		if(glob==N*ITERATOR) { }
// 		for(int i=0;i<ITERATOR;i++)
// 			delete barrier[i];
// 	}
// 	REGISTER_TEST(Run,506);
// }//namespace test506

// // test580: TP. Race on fwrite argument
// namespace test580 {
// 	char s[]="abrabdagasd\n";
// 	void Worker1() {
// 		fwrite(s,1,sizeof(s)-1,stdout);
// 	}
// 	void Worker2() {
// 		s[3]='z';
// 	}
// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,580);
// }//namespace test580

// // test581: TP. Race on puts argument
// namespace test581 {
// 	char s[]="aasdasdfasdf";
// 	void Worker1() {
// 		puts(s);
// 	}
// 	void Worker2() {
// 		s[3]='z';
// 	}
// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,581);
// }//namespace test581

// // test582: TP. Race on printf argument
// namespace test582 {
// 	volatile char s[]="asdfasdfasdf";
// 	volatile char s2[]="asdfasdfasdf";

// 	void Worker1() {
// 		fprintf(stdout, "printing a string: %s\n", s);
// 		fprintf(stderr, "printing a string: %s\n", s2);
// 	}
// 	void Worker2() {
// 		s[3]='z';
// 		s2[3]='z';
// 	}
// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,582);
// }//namespace test582

// //===========================belong to posix_tests=========================
// // test600: TP. Simple test with RWLock
// namespace test600 {
// 	int var1=0,var2=0;
// 	RWLock mu;

// 	void WriteWhileHoldingReaderLock(int *p) {
// 		usleep(100000);
// 		ReaderLockScoped lock(&mu); //reader lock for writing
// 		(*p)++;
// 	}

// 	void CorrecWrite(int *p) {
// 		WriterLockScoped lock(&mu);
// 		(*p)++;
// 	}

// 	void Worker1() { WriteWhileHoldingReaderLock(&var1); }
// 	void Worker2() { CorrecWrite(&var1); }
// 	void Worker3() { WriteWhileHoldingReaderLock(&var2); }
// 	void Worker4() { CorrecWrite(&var2); }

// 	void Run() {
// 		PTHREAD_RUN(4);
// 	}
// 	REGISTER_TEST(Run,600);
// }//namespace test600

// // test601:Backwards lock
// namespace test601 {
// 	// This test uses "Backwards mutex" locking protocol.
// 	// We take a *reader* lock when writing to a per-thread data
// 	// (GLOB[thread_num])  and we take a *writer* lock when we
// 	// are reading from the entire array at once.
// 	//
// 	// Such locking protocol is not understood by ThreadSanitizer's
// 	// hybrid state machine. So, you either have to use a pure-happens-before
// 	// detector ("tsan --pure-happens-before") or apply pure happens-before mode
// 	// to this particular lock by using ANNOTATE_PURE_HAPPENS_BEFORE_MUTEX(&mu).
// 	const int N=3;
// 	RWLock mu;
// 	int adder_num;
// 	int glob[N];
// 	void Adder() {
// 		int my_num=ATOMIC_INCREMENT(&adder_num,1)-1;
// 		assert(my_num>=0);
// 		assert(my_num<3);
// 		ReaderLockScoped lock(&mu);
// 		glob[my_num]++;
// 	}
// 	void Aggregator() {
// 		int sum=0;
// 		{
// 			WriterLockScoped lock(&mu);
// 			for(int i=0;i<N;i++)
// 				sum+=glob[i];
// 		}
// 	}
// 	void Run() {
// 		adder_num=0;
// 		{
// 			ThreadArray t(Adder,Adder,Adder,Aggregator);
// 			t.Start();
// 			t.Join();
// 		}
// 		adder_num=0;
// 		{
// 			ThreadArray t(Aggregator,Adder,Adder,Adder);
// 			t.Start();
// 			t.Join();	
// 		}
// 	}

// 	REGISTER_TEST(Run,601);
// }//namespace test601

// // test602: TN.
// namespace test602 {
// 	const int kMmapSize=65536;
// 	void SubWorker() {
// 		for(int i=0;i<100;i++) {
// 			int *ptr=(int *)mmap(NULL,kMmapSize,PROT_READ | PROT_WRITE,
// 				MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
// 			//any write will result in a private complication
// 			*ptr=42;
// 			munmap(ptr,kMmapSize);
// 		}
// 	}
// 	void Worker() {
// 		ThreadArray t(SubWorker,SubWorker,SubWorker,SubWorker);
// 		t.Start();
// 		t.Join();
// 	}
// 	void Worker1() {
// 		ThreadArray t(Worker,Worker,Worker,Worker);
// 		t.Start();
// 		t.Join();
// 	}
// 	void Run() {
// 		PTHREAD_RUN(1);
// 	}
// 	REGISTER_TEST(Run,602);
// }//namespace test602

// // test603: TN. unlink/fopen, rmdir/opendir.
// namespace test603 {
// 	int glob1=0,glob2=0;
// 	char *dir_name=NULL,*file_name=NULL;

// 	void Waker1() {
// 		usleep(100000);
// 		glob1=1;
// 		// unlink deletes a file 'filename'
//   		// which exits spin-loop in Waiter1().
//   		printf("  Deleting file...\n");
//   		if(unlink(file_name)==0) { }
// 	}
// 	void Waiter1() {
// 		FILE *tmp;
// 		while((tmp=fopen(file_name,"r"))!=NULL) {
// 			fclose(tmp);
// 			usleep(10000);
// 		}
// 		printf("  ...file has been deleted\n");
// 		glob1=2;//write
// 	}
// 	void Waker2() {
// 		usleep(100000);
// 		glob2=1;
// 		// rmdir deletes a directory 'dir_name'
//   		// which exit spin-loop in Waker().
//   		printf("  Deleting directory...\n");
//   		if(rmdir(dir_name) == 0) { }
// 	}
// 	void Waiter2() {
// 		DIR *tmp;
// 		while((tmp=opendir(dir_name))!=NULL) {
// 			closedir(tmp);
// 			usleep(10000);
// 		}
// 		printf("  ...directory has been deleted\n");
//   		glob2=2;
// 	}
// 	void Run() {
// 		dir_name=strdup("/tmp/data-race-XXXXXX");
// 		if(mkdtemp(dir_name)!=NULL) { }//dir_name is unqiue

// 		file_name=strdup((std::string()+dir_name+"/XXXXXX").c_str());
// 		const int fd=mkstemp(file_name);//create and open temp file
// 		assert(fd>=0);
// 		close(fd);
// 		ThreadArray t1(Waker1,Waiter1);
// 		t1.Start();
// 		t1.Join();

// 		ThreadArray t2(Waker2,Waiter2);
// 		t2.Start();
// 		t2.Join();

// 		free(file_name);
// 		file_name=NULL;
// 		free(dir_name);
// 		dir_name=NULL;
// 	}
// 	REGISTER_TEST(Run,603);
// }//namespace test603

// // test604: TP. Unit test for RWLock::TryLock and RWLock::ReaderTryLock.
// namespace test604 {
// 	// Worker1 locks the globals for writing for a long time.
// 	// Worker2 tries to write to globals twice: without a writer lock and with it.
// 	// Worker3 tries to read from globals twice: without a reader lock and with it.
// 	int glob1=0;
// 	char padding1[64];
// 	int glob2=0;
// 	char padding2[64];
// 	int glob3=0;
// 	char padding3[64];
// 	int glob4=0;
// 	char padding4[64];
// 	RWLock mu;
// 	StealthNotification n1,n2,n3,n4,n5;

// 	void Worker1() {
// 		mu.Lock();
// 		glob1=1;
// 		glob2=1;
// 		glob3=1;
// 		glob4=1;
// 		n1.signal();
// 		n2.wait();
// 		n3.wait();
// 		mu.Unlock();
// 		n4.signal();
// 	}
// 	void Worker2() {
// 		n1.wait();
// 		if(mu.TryLock()) //this point mu is held in Worker1
// 			assert(0);
// 		else //write without a writer lock
// 			glob1=2; 
// 		n2.signal();
// 		n5.wait();
// 		//mu is released by Worker3
// 		if(mu.TryLock()) {
// 			glob2=2;
// 			mu.Unlock();
// 		}
// 		else
// 			assert(0);
// 	}
// 	void Worker3() {
// 		n1.wait();
// 		if(mu.ReaderTryLock()) //this point mu is held in Worker1
// 			assert(0);
// 		else //read without a reader lock
// 			printf("\treading glob3: %d\n", glob3); 
// 		n3.signal();
// 		n4.wait();
// 		//mu is released by Worker1
// 		if(mu.ReaderTryLock()) {
// 			printf("\treading glob4: %d\n", glob4);
// 			mu.ReaderUnlock();
// 		}
// 		else
// 			assert(0);
// 		n5.signal();
// 	}
// 	void Run() {
// 		PTHREAD_RUN(3);
// 	}
// 	REGISTER_TEST(Run,604);
// }//namespace test604

// // test605: TP. Test that reader lock/unlock do not create a hb-arc.
// namespace test605 {
// 	RWLock mu;
// 	int glob;
// 	StealthNotification n;

// 	void Worker1() {
// 		glob=1;
// 		mu.ReaderLock();
// 		mu.ReaderUnlock();
// 		n.signal();
// 	}
// 	void Worker2() {
// 		n.wait();
// 		mu.ReaderLock();
// 		mu.ReaderUnlock();
// 		glob=1;
// 	}
// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,605);
// }//namespace test605

// // test606: TN. Test the support for libpthread TSD(Thread-specific Date) destructors.
// namespace test606 {
// 	pthread_key_t key;
// 	const int kInitialValue = 0xfeedface;
// 	int tsd_array[2];

// 	void Destructor(void *ptr) {
// 		int *writer=(int *)ptr;
// 		*writer=kInitialValue;
// 	}
// 	void DoWork(int index) {
// 		int *value=&(tsd_array[index]);
// 		*value=2;
// 		pthread_setspecific(key,value);
// 		int *read=(int *)pthread_getspecific(key);
// 		if(read==value) { }
// 	}
// 	void Worker1() { DoWork(0); }
// 	void Worker2() { DoWork(1); }

// 	void Run() {
// 		pthread_key_create(&key,Destructor);
// 		ThreadArray t(Worker1,Worker2);
// 		t.Start();
// 		t.Join();
// 		for(int i=0;i<2;i++)
// 			if(tsd_array[i]==kInitialValue) { }
// 	}
// 	REGISTER_TEST(Run,606);
// }//namespace test606

// // test607: TP.
// namespace test607 {
// 	void Worker1() {
// 		struct tm timestruct;
// 		timestruct.tm_sec=56;
// 		timestruct.tm_min=34;
// 		timestruct.tm_hour=12;
// 		timestruct.tm_mday=31;
// 		timestruct.tm_mon=3-1;
// 		timestruct.tm_year=2015-1900;
// 		timestruct.tm_wday=4;
// 		timestruct.tm_yday=0;

// 		time_t ret=mktime(&timestruct);
// 		if(ret!=-1) { }
// 	}

// 	void Run() {
// 		HANDLER Worker2=(HANDLER)Worker1;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,607);
// }//namespace test607

// //===========================belong to demo_tests=========================
// // test302:Complex race which happens at least twice
// namespace test302 {
// 	// In this test we have many different accesses to glob and only one access
// 	// is not synchronized properly.
// 	int glob=0;
// 	Mutex mu1;
// 	Mutex mu2;
// 	void Worker1() {
// 		for(int i=0;i<100;i++) {
// 			switch(i % 4) {
//       			case 0:
//         			// This read is protected correctly.
//         			mu1.Lock(); if(glob >= 0) { } mu1.Unlock();
//         			break;
//       			case 1:
//         			// Here we used the wrong lock! The reason of the race is here.
//         			mu2.Lock(); if(glob >= 0) { } mu2.Unlock();
//         			break;
//       			case 2:
//         			// This read is protected correctly.
//         			mu1.Lock(); if(glob >= 0) { } mu1.Unlock();
//         			break;
//       			case 3:
//         			// This write is protected correctly.
//         			mu1.Lock(); glob++; mu1.Unlock();
//         			break;
//     		}
//     		// sleep a bit so that the threads interleave
//     		// and the race happens at least twice.
//     		usleep(100);
// 		}
// 	}
// 	void Run() {
// 		HANDLER Worker2=(HANDLER)Worker1;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,302);
// }//namespace test302

// // test305:A bit more tricky: two locks used inconsistenly.
// namespace test305 {
// 	int glob=0;
// 	Mutex mu1,mu2;
// 	void Worker1() { mu1.Lock(); mu2.Lock(); glob=1; mu2.Unlock(); mu1.Unlock(); }
// 	void Worker2() { mu1.Lock();             glob=2;               mu1.Unlock(); }
// 	void Worker3() { mu1.Lock(); mu2.Lock(); glob=3; mu2.Unlock(); mu1.Unlock(); }
// 	void Worker4() {             mu2.Lock(); glob=4; mu2.Unlock();               }

// 	void Run() {
// 		Thread t1(Worker1),t2(Worker2),t3(Worker3),t4(Worker4);
// 		t1.Start();usleep(1000);
// 		t2.Start();usleep(1000);
// 		t3.Start();usleep(1000);
// 		t4.Start();usleep(1000);
// 		t1.Join();t2.Join();t3.Join();t4.Join();
// 	}
// 	REGISTER_TEST(Run,305);
// }//namespace test305

// // test306:Two locks are used to protect a var.
// namespace test306 {
// 	int glob=0;
// 	// Thread1 and Thread2 access the var under two locks.
// 	// Thread3 uses no locks.
// 	Mutex mu1,mu2;
// 	void Worker1() { mu1.Lock(); mu2.Lock(); glob=1; mu2.Unlock(); mu1.Unlock(); }
// 	void Worker2() { mu1.Lock(); mu2.Lock(); glob=3; mu2.Unlock(); mu1.Unlock(); }
// 	void Worker3() {             			 glob=4;               				 }
// 	void Run() {
// 		Thread t1(Worker1),t2(Worker2),t3(Worker3);
// 		t1.Start();usleep(1000);
// 		t2.Start();usleep(1000);
// 		t3.Start();usleep(1000);
// 		t1.Join();t2.Join();t3.Join();
// 	}
// 	REGISTER_TEST(Run,306);
// }//namespace test306

// // test309:Simple race on an STL object
// namespace test309 {
// 	string glob;
// 	void Worker1() {
// 		glob="Thread1";
// 	}
// 	void Worker2() {
// 		usleep(100000);
// 		glob="Booooooooooooooo";
// 	}
// 	void Run() {
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,309);
// }//namespace test309

// // test310:One more simple race
// namespace test310 {
// 	int *glob=NULL;
// 	Mutex mu1,mu2,mu3;
// 	void Writer1() {
// 		MutexLock lock3(&mu3);
// 		MutexLock lock1(&mu1);
// 		*glob=1;
// 	}
// 	void Writer2() {
// 		MutexLock lock2(&mu2);
// 		MutexLock lock1(&mu1);
// 		int some_unrelated_stuff = 0;
//   		if (some_unrelated_stuff == 0)
//     		some_unrelated_stuff++;
//   		*glob=2;
// 	}
// 	void Reader() {
// 		MutexLock lock2(&mu2);
// 		assert(*glob<=2);
// 	}
// 	void Run() {
// 		glob=new int(0);
// 		Thread t1(Writer1),t2(Writer2),t3(Reader);
// 		t1.Start();
// 		t2.Start();
// 		usleep(100000);//let the writers go first
// 		t3.Start();
// 		t1.Join();t2.Join();t3.Join();
// 		delete glob;
// 	}
// 	REGISTER_TEST(Run,310);
// }//namespace test310

// // test311:A test with a very deep stack
// namespace test311 {
// 	int glob=0;
// 	void RacyWrite() { glob++; }
// 	void Fun1() { RacyWrite(); }
// 	void Fun2() { Fun1(); }
// 	void Fun3() { Fun2(); }
// 	void Fun4() { Fun3(); }
// 	void Fun5() { Fun4(); }
// 	void Fun6() { Fun5(); }
// 	void Fun7() { Fun6(); }
// 	void Fun8() { Fun7(); }
// 	void Fun9() { Fun8(); }
// 	void Fun10() { Fun9(); }
// 	void Fun11() { Fun10(); }
// 	void Fun12() { Fun11(); }
// 	void Fun13() { Fun12(); }
// 	void Fun14() { Fun13(); }
// 	void Fun15() { Fun14(); }
// 	void Fun16() { Fun15(); }
// 	void Fun17() { Fun16(); }
// 	void Fun18() { Fun17(); }
// 	void Fun19() { Fun18(); }
// 	void Worker1() { Fun19(); }
// 	void Run() {
// 		HANDLER Worker2=(HANDLER)Worker1;
// 		HANDLER Worker3=(HANDLER)Worker1;
// 		PTHREAD_RUN(3);
// 	}
// 	REGISTER_TEST(Run,311);
// }//namespace test311

// // test313:
// namespace test313 {
// 	int glob;
// 	Mutex mu;
// 	int flag;
// 	void Worker1() {
// 		// sleep(1);
// 		mu.Lock();
// 		bool f=flag;
// 		mu.Unlock();
// 		if(!f)
// 			glob++;
// 	}
// 	void Worker2() {
// 		glob++;
// 		mu.Lock();
// 		flag=true;
// 		mu.Unlock();
// 	}
// 	void Run() {
// 		glob=0;
// 		PTHREAD_RUN(2);
// 	}
// 	REGISTER_TEST(Run,313);
// }//namespace test313

int main() {
	//execute the test senquencially
	for(std::map<int,Test>::iterator it=test_map.begin();
		it!=test_map.end();it++) {
		printf("[TEST%d]========================================\n",it->first);
		Thread t(it->second.f_);
		t.Start();
		t.Join();
		printf("[TEST%d]========================================\n\n",it->first);
	}
	return 0;
}

