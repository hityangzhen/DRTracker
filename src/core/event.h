#ifndef __CORE_EVENT_H
#define __CORE_EVENT_H

#include "core/basictypes.h"

#define PROGRAM_START 1
#define PROGRAM_EXIT 2

#define IMAGE_LOAD 3
#define IMAGE_UNLOAD 4

#define THREAD_START 5
#define THREAD_EXIT 6
#define MAIN 7
#define THREAD_MAIN 8

#define BEFORE_MEM_READ 9
#define AFTER_MEM_READ 10
#define BEFORE_MEM_WRITE 11
#define AFTER_MEM_WRITE 12

#define BEFORE_ATOMIC_INST 13
#define AFTER_ATOMIC_INST 14

#define BEFORE_CALL 15
#define AFTER_CALL 16
#define BEFORE_RETURN 17
#define AFTER_RETURN 18

#define BEFORE_PTHREAD_CREATE 19
#define AFTER_PTHREAD_CREATE 20
#define BEFORE_PTHREAD_JOIN 21
#define AFTER_PTHREAD_JOIN 22

#define BEFORE_PTHREAD_MUTEX_TRYLOCK 23
#define AFTER_PTHREAD_MUTEX_TRYLOCK 24
#define BEFORE_PTHREAD_MUTEX_LOCK 25
#define AFTER_PTHREAD_MUTEX_LOCK 26
#define BEFORE_PTHREAD_MUTEX_UNLOCK 27
#define AFTER_PTHREAD_MUTEX_UNLOCK 28
#define BEFORE_PTHREAD_RWLOCK_TRYRDLOCK 29
#define AFTER_PTHREAD_RWLOCK_TRYRDLOCK 30
#define BEFORE_PTHREAD_RWLOCK_RDLOCK 31
#define AFTER_PTHREAD_RWLOCK_RDLOCK 32
#define BEFORE_PTHREAD_RWLOCK_TRYWRLOCK 33
#define AFTER_PTHREAD_RWLOCK_TRYWRLOCK 34
#define BEFORE_PTHREAD_RWLOCK_WRLOCK 35
#define AFTER_PTHREAD_RWLOCK_WRLOCK 36
#define BEFORE_PTHREAD_RWLOCK_UNLOCK 37
#define AFTER_PTHREAD_RWLOCK_UNLOCK 38

#define BEFORE_MALLOC 39
#define AFTER_MALLOC 40
#define BEFORE_CALLOC 41
#define AFTER_CALLOC 42
#define BEFORE_REALLOC 43
#define AFTER_REALLOC 44
#define BEFORE_FREE 45
#define AFTER_FREE 46

#define BEFORE_PTHREAD_COND_SIGNAL 47
#define AFTER_PTHREAD_COND_SIGNAL 48
#define BEFORE_PTHREAD_COND_BROADCAST 49
#define AFTER_PTHREAD_COND_BROADCAST 50
#define BEFORE_PTHREAD_COND_WAIT 51
#define AFTER_PTHREAD_COND_WAIT 52
#define BEFORE_PTHREAD_COND_TIMEDWAIT 53
#define AFTER_PTHREAD_COND_TIMEDWAIT 54
#define BEFORE_PTHREAD_BARRIER_INIT 55
#define AFTER_PTHREAD_BARRIER_INIT 56
#define BEFORE_PTHREAD_BARRIER_WAIT 57
#define AFTER_PTHREAD_BARRIER_WAIT 58

#define BEFORE_SEM_INIT 59
#define AFTER_SEM_INIT 60
#define BEFORE_SEM_POST 61
#define AFTER_SEM_POST 62
#define BEFORE_SEM_WAIT 63
#define AFTER_SEM_WAIT 64


class EventBase {
public:
	virtual std::string name()=0;
	virtual std::string func()=0;
	virtual int argc()=0;
protected:
	EventBase() {}
	virtual ~EventBase() {}
private:
	DISALLOW_COPY_CONSTRUCTORS(EventBase);
};

#define MAX_EVENT_NUM 10
class EventBuffer {
public:
	EventBuffer():bgn_idx_(0),end_idx_(0) {
		vec_.assign(MAX_EVENT_NUM+1,NULL);
	}
	~EventBuffer() {}
	void Push(EventBase *eb) {
		vec_[end_idx_]=eb;
		end_idx_=(end_idx_+1)%(MAX_EVENT_NUM+1);
	}
	EventBase *Pop() {
		EventBase *eb=vec_[bgn_idx_];
		vec_[bgn_idx_]=NULL;
		bgn_idx_=(bgn_idx_+1)%(MAX_EVENT_NUM+1);
		return eb;
	}
	bool Empty() { return bgn_idx_==end_idx_; }
	bool Full() { 
		return bgn_idx_<end_idx_?(end_idx_-bgn_idx_==MAX_EVENT_NUM):
			(end_idx_-bgn_idx_+1==0);
	}
private:
	int bgn_idx_;
	int end_idx_;
	std::vector<EventBase *> vec_;
};

#define EVENT_ARGS_0
#define EVENT_ARGS_1 A0 arg0
#define EVENT_ARGS_2 EVENT_ARGS_1,A1 arg1
#define EVENT_ARGS_3 EVENT_ARGS_2,A2 arg2
#define EVENT_ARGS_4 EVENT_ARGS_3,A3 arg3
#define EVENT_ARGS_5 EVENT_ARGS_4,A4 arg4
#define EVENT_ARGS_6 EVENT_ARGS_5,A5 arg5
#define EVENT_ARGS_7 EVENT_ARGS_6,A6 arg6
#define EVENT_ARGS(i) EVENT_ARGS_##i

#define PARAM_ARGS_0
#define PARAM_ARGS_1 A0
#define PARAM_ARGS_2 PARAM_ARGS_1,A1
#define PARAM_ARGS_3 PARAM_ARGS_2,A2
#define PARAM_ARGS_4 PARAM_ARGS_3,A3
#define PARAM_ARGS_5 PARAM_ARGS_4,A4
#define PARAM_ARGS_6 PARAM_ARGS_5,A5
#define PARAM_ARGS_7 PARAM_ARGS_6,A6
#define PARAM_ARGS(i) PARAM_ARGS_##i

#define TYPENAME_ARGS_0
#define TYPENAME_ARGS_1 ,typename A0
#define TYPENAME_ARGS_2 TYPENAME_ARGS_1,typename A1
#define TYPENAME_ARGS_3 TYPENAME_ARGS_2,typename A2
#define TYPENAME_ARGS_4 TYPENAME_ARGS_3,typename A3
#define TYPENAME_ARGS_5 TYPENAME_ARGS_4,typename A4
#define TYPENAME_ARGS_6 TYPENAME_ARGS_5,typename A5
#define TYPENAME_ARGS_7 TYPENAME_ARGS_6,typename A6
#define TYPENAME_ARGS(i) TYPENAME_ARGS_##i

//argument iterators for variadic template
#define MEMBER_ARGS_0
#define MEMBER_ARGS_1 A0 arg0_
#define MEMBER_ARGS_2 MEMBER_ARGS_1;A1 arg1_
#define MEMBER_ARGS_3 MEMBER_ARGS_2;A2 arg2_
#define MEMBER_ARGS_4 MEMBER_ARGS_3;A3 arg3_
#define MEMBER_ARGS_5 MEMBER_ARGS_4;A4 arg4_
#define MEMBER_ARGS_6 MEMBER_ARGS_5;A3 arg5_
#define MEMBER_ARGS_7 MEMBER_ARGS_6;A3 arg6_
#define MEMBER_ARGS(i) MEMBER_ARGS_##i

#define ARG_ACCESSORS_0
#define ARG_ACCESSORS_1 A0 arg0() {return arg0_;}
#define ARG_ACCESSORS_2 ARG_ACCESSORS_1 A1 arg1() {return arg1_;}
#define ARG_ACCESSORS_3 ARG_ACCESSORS_2 A2 arg2() {return arg2_;}
#define ARG_ACCESSORS_4 ARG_ACCESSORS_3 A3 arg3() {return arg3_;}
#define ARG_ACCESSORS_5 ARG_ACCESSORS_4 A4 arg4() {return arg4_;}
#define ARG_ACCESSORS_6 ARG_ACCESSORS_5 A5 arg5() {return arg5_;}
#define ARG_ACCESSORS_7 ARG_ACCESSORS_6 A6 arg6() {return arg6_;}
#define ARG_ACCESSORS(i) ARG_ACCESSORS_##i

#define SET_EVENT_ARGS_0
#define SET_EVENT_ARGS_1 event->arg0_=arg0
#define SET_EVENT_ARGS_2 SET_EVENT_ARGS_1;event->arg1_=arg1
#define SET_EVENT_ARGS_3 SET_EVENT_ARGS_2;event->arg2_=arg2
#define SET_EVENT_ARGS_4 SET_EVENT_ARGS_3;event->arg3_=arg3
#define SET_EVENT_ARGS_5 SET_EVENT_ARGS_4;event->arg4_=arg4
#define SET_EVENT_ARGS_6 SET_EVENT_ARGS_5;event->arg5_=arg5
#define SET_EVENT_ARGS_7 SET_EVENT_ARGS_6;event->arg6_=arg6
#define SET_EVENT_ARGS(i) SET_EVENT_ARGS_##i

#define CREATE_EVENT(Name,...)	Name##Event::GetInstance(__VA_ARGS__)
#define DELETE_EVENT(event)												\
	if(event->decrease_ref()==0)										\
		delete event


#define GET_INSTANCE(NUM_ARGS)											\
	static E *GetInstance(EVENT_ARGS(NUM_ARGS))							\
	{																	\
		E *event=new E;													\
		SET_EVENT_ARGS(NUM_ARGS);										\
		return event;													\
	}

//define event template
#define EVENT_TEMPLATE(NUM_ARGS)										\
	template <typename E 												\
		TYPENAME_ARGS(NUM_ARGS)>										\
	class Event<E,void(PARAM_ARGS(NUM_ARGS))>:public EventBase			\
	{																	\
	public:																\
		Event():ref_(1) {}												\
		virtual ~Event() {}												\
		int argc() { return NUM_ARGS; }									\
		ARG_ACCESSORS(NUM_ARGS);										\
		MEMBER_ARGS(NUM_ARGS);											\
		GET_INSTANCE(NUM_ARGS);											\
		int ref_;														\
		int ref() {return ref_;}										\
		void increase_ref() {++ref_;}									\
		int decrease_ref() {return --ref_;}								\
	private:															\
		DISALLOW_COPY_CONSTRUCTORS(Event);								\
	}


template <typename E,typename Signature>
class Event;

//Partial specilization templates
EVENT_TEMPLATE(0);
EVENT_TEMPLATE(1);
EVENT_TEMPLATE(2);
EVENT_TEMPLATE(3);
EVENT_TEMPLATE(4);
EVENT_TEMPLATE(5);
EVENT_TEMPLATE(6);
EVENT_TEMPLATE(7);

#define EVENT_CLASS(Name) Name##Event

//EVENT_TEMPLATE specilizes the number of the parameters
//EVENT specilizes the event class
#define EVENT(name_,func_,sig_)											\
	class EVENT_CLASS(name_):public Event<EVENT_CLASS(name_),sig_>		\
	{																	\
	public:																\
		EVENT_CLASS(name_)() {}											\
		~EVENT_CLASS(name_)() {}										\
		std::string name() { return #name_; }							\
		std::string func() { return func_; }							\
	private:															\
		DISALLOW_COPY_CONSTRUCTORS(EVENT_CLASS(name_));					\
	}

EVENT(ImageLoad,"ImageLoad",void (Image *,address_t,address_t,address_t,size_t,
	address_t,size_t));
EVENT(ImageUnload,"ImageUnload",void (Image *,address_t,address_t,address_t,size_t,
	address_t,size_t));
EVENT(ThreadStart,"ThreadStart",void (thread_t,thread_t));
EVENT(ThreadExit,"ThreadExit",void (thread_t,timestamp_t));
EVENT(Main,"Main",void (thread_t,timestamp_t));
EVENT(ThreadMain,"ThreadMain",void (thread_t,timestamp_t));
EVENT(BeforeMemRead,"BeforeMemRead", void (thread_t,timestamp_t,Inst *,
	address_t,size_t));
EVENT(AfterMemRead,"AfterMemRead", void (thread_t,timestamp_t,Inst *,
	address_t,size_t));
EVENT(BeforeMemWrite,"BeforeMemWrite", void (thread_t,timestamp_t,Inst *,
	address_t,size_t));
EVENT(AfterMemWrite,"AfterMemWrite", void (thread_t,timestamp_t,Inst *,
	address_t,size_t));
EVENT(BeforeAtomicInst,"BeforeAtomicInst",void (thread_t,timestamp_t,Inst *,
	std::string,address_t));
EVENT(AfterAtomicInst,"AfterAtomicInst",void (thread_t,timestamp_t,Inst *,
	std::string,address_t));
EVENT(BeforeCall,"BeforeCall",void (thread_t,timestamp_t,Inst *,address_t));
EVENT(AfterCall,"AfterCall",void (thread_t,timestamp_t,Inst *,address_t,
	address_t));
EVENT(BeforeReturn,"BeforeReturn",void (thread_t,timestamp_t,Inst *,address_t));
EVENT(AfterReturn,"AfterReturn",void (thread_t,timestamp_t,Inst *,address_t));
EVENT(BeforePthreadCreate,"BeforePthreadCreate",void (thread_t,timestamp_t,
	Inst *));
EVENT(AfterPthreadCreate,"AfterPthreadCreate",void (thread_t,timestamp_t,
	Inst *,thread_t));
EVENT(BeforePthreadJoin,"BeforePthreadJoin",void (thread_t,timestamp_t,Inst *,
	thread_t));
EVENT(AfterPthreadJoin,"AfterPthreadJoin",void (thread_t,timestamp_t,Inst *,
	thread_t));
EVENT(BeforePthreadMutexTryLock,"BeforePthreadMutexTryLock",void (thread_t,
	timestamp_t,Inst *,address_t));
EVENT(AfterPthreadMutexTryLock,"AfterPthreadMutexTryLock",void (thread_t,
	timestamp_t,Inst *,address_t,int));
EVENT(BeforePthreadMutexLock,"BeforePthreadMutexLock",void (thread_t,
	timestamp_t,Inst *,address_t));
EVENT(AfterPthreadMutexLock,"AfterPthreadMutexLock",void (thread_t,
	timestamp_t,Inst *,address_t));
EVENT(BeforePthreadMutexUnlock,"BeforePthreadMutexUnlock",void (thread_t,
	timestamp_t,Inst *,address_t));
EVENT(AfterPthreadMutexUnlock,"AfterPthreadMutexUnlock",void (thread_t,
	timestamp_t,Inst *,address_t));
EVENT(BeforePthreadRwlockTryRdlock,"BeforePthreadRwlockTryRdlock",void (thread_t,
	timestamp_t,Inst *,address_t));
EVENT(AfterPthreadRwlockTryRdlock,"AfterPthreadRwlockTryRdlock",void (thread_t,
	timestamp_t,Inst *,address_t,int));
EVENT(BeforePthreadRwlockRdlock,"BeforePthreadRwlockRdlock",void (thread_t,
	timestamp_t,Inst *,address_t));
EVENT(AfterPthreadRwlockRdlock,"AfterPthreadRwlockRdlock",void (thread_t,
	timestamp_t,Inst *,address_t));
EVENT(BeforePthreadRwlockTryWrlock,"BeforePthreadRwlockTryWrlock",void (thread_t,
	timestamp_t,Inst *,address_t));
EVENT(AfterPthreadRwlockTryWrlock,"AfterPthreadRwlockTryWrlock",void (thread_t,
	timestamp_t,Inst *,address_t,int));
EVENT(BeforePthreadRwlockWrlock,"BeforePthreadRwlockWrlock",void (thread_t,
	timestamp_t,Inst *,address_t));
EVENT(AfterPthreadRwlockWrlock,"AfterPthreadRwlockWrlock",void (thread_t,
	timestamp_t,Inst *,address_t));
EVENT(BeforePthreadRwlockUnlock,"BeforePthreadRwlockUnlock",void (thread_t,
	timestamp_t,Inst *,address_t));
EVENT(AfterPthreadRwlockUnlock,"AfterPthreadRwlockUnlock",void (thread_t,
	timestamp_t,Inst *,address_t));
EVENT(BeforeMalloc,"BeforeMalloc",void (thread_t,timestamp_t,Inst *,size_t));
EVENT(AfterMalloc,"AfterMalloc",void (thread_t,timestamp_t,Inst *,size_t,address_t));
EVENT(BeforeCalloc,"BeforeCalloc",void (thread_t,timestamp_t,Inst *,size_t,size_t));
EVENT(AfterCalloc,"AfterCalloc",void (thread_t,timestamp_t,Inst *,size_t,size_t,
	address_t));
EVENT(BeforeRealloc,"BeforeRealloc",void (thread_t,timestamp_t,Inst *,address_t,size_t));
EVENT(AfterRealloc,"AfterRealloc",void (thread_t,timestamp_t,Inst *,address_t,size_t,
	address_t));
EVENT(BeforeFree,"BeforeFree",void (thread_t,timestamp_t,Inst *,address_t));
EVENT(AfterFree,"AfterFree",void (thread_t,timestamp_t,Inst *,address_t));
EVENT(BeforePthreadCondSignal,"BeforePthreadCondSignal",void (thread_t,timestamp_t,Inst *,
	address_t));
EVENT(AfterPthreadCondSignal,"AfterPthreadCondSignal",void (thread_t,timestamp_t,Inst *,
	address_t));
EVENT(BeforePthreadCondBroadcast,"BeforePthreadCondBroadcast",void (thread_t,timestamp_t,Inst *,
	address_t));
EVENT(AfterPthreadCondBroadcast,"AfterPthreadCondBroadcast",void (thread_t,timestamp_t,Inst *,
	address_t));
EVENT(BeforePthreadCondWait,"BeforePthreadCondWait",void (thread_t,timestamp_t,
	Inst *,address_t,address_t));
EVENT(AfterPthreadCondWait,"AfterPthreadCondWait",void (thread_t,timestamp_t,
	Inst *,address_t,address_t));
EVENT(BeforePthreadCondTimedwait,"BeforePthreadCondTimedwait",void (thread_t,timestamp_t,
	Inst *,address_t,address_t));
EVENT(AfterPthreadCondTimedwait,"AfterPthreadCondTimedwait",void (thread_t,timestamp_t,
	Inst *,address_t,address_t));
EVENT(BeforePthreadBarrierInit,"BeforePthreadBarrierInit",void (thread_t,timestamp_t,Inst *,
	address_t,unsigned int));
EVENT(AfterPthreadBarrierInit,"AfterPthreadBarrierInit",void (thread_t,timestamp_t,Inst *,
	address_t,unsigned int));
EVENT(BeforePthreadBarrierWait,"BeforePthreadBarrierWait",void (thread_t,timestamp_t,Inst *,
    address_t));
EVENT(AfterPthreadBarrierWait,"AfterPthreadBarrierWait",void (thread_t,timestamp_t,Inst *,
    address_t));
EVENT(BeforeSemInit,"BeforeSemInit",void (thread_t,timestamp_t,Inst *,address_t,unsigned int));
EVENT(AfterSemInit,"AfterSemInit",void (thread_t,timestamp_t,Inst *,address_t,unsigned int));
EVENT(BeforeSemPost,"BeforeSemPost",void (thread_t,timestamp_t,Inst *,address_t));
EVENT(AfterSemPost,"AfterSemPost",void (thread_t,timestamp_t,Inst *,address_t));
EVENT(BeforeSemWait,"BeforeSemWait",void (thread_t,timestamp_t,Inst *,address_t));
EVENT(AfterSemWait,"AfterSemWait",void (thread_t,timestamp_t,Inst *,address_t));

#endif /* __CORE_EVENT_H */