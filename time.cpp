#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdnoreturn.h>
#include <cstdlib>
#include <sys/resource.h>

#include <forward_list>

using namespace std;

struct thread_rusage{
	long tid;
	struct rusage rusage;
};

forward_list<struct thread_rusage*> thread_rusages;
pthread_mutex_t thread_rusages_lock;

class ThreadObject{
	public:
		ThreadObject();
		~ThreadObject();
		void create();
};

ThreadObject::ThreadObject(){
	printf("Thread has been started.[TID=%ld][PID=%d]\n", syscall(SYS_gettid), getpid());
}

ThreadObject::~ThreadObject(){
	printf("Thread has been terminated.[TID=%ld][PID=%d]\n", syscall(SYS_gettid), getpid());

	// get rusage and tid
	struct thread_rusage* r = (struct thread_rusage*) malloc(sizeof(struct thread_rusage));
	r->tid = syscall(SYS_gettid); 
	getrusage(RUSAGE_THREAD, &(r->rusage));

	// atomically add rusages to list
	pthread_mutex_lock(&thread_rusages_lock);
	thread_rusages.push_front(r);
	pthread_mutex_unlock(&thread_rusages_lock);
}

void ThreadObject::create(){
	// Do nothing, is called, this will ensure this object is created
}

thread_local ThreadObject threadobject;


// real function pointers
//real_pthread
int (*real_pthread_create)(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
[[noreturn]] void (*real_pthread_exit)(void *retval);


void link_pthread_functions(){
	real_pthread_create = (int (*)(pthread_t*, const pthread_attr_t*, void *(*)(void *), void*)) dlsym(RTLD_NEXT, "pthread_create");
	real_pthread_exit = (void (*)(void*)) dlsym(RTLD_NEXT, "pthread_exit");
}

// Setup functions (setup.c)
static void con() __attribute__((constructor));
static void dest() __attribute__((destructor));

void con(){
	link_pthread_functions();

	printf("PROCESS STARTING\n");
	// touch object for main threa
	threadobject.create();
}


void print_thread_rusage(struct thread_rusage* thread_rusage){
	struct rusage* rusage = &thread_rusage->rusage;

	printf("TID:%ld\n", thread_rusage->tid);
	
	// Calculate usr/system time
	//
	/* Convert all times to milliseconds.  Occasionally, one of these values
	 *      comes out as zero.  Dividing by zero causes problems, so we first
	 *           check the time value.  If it is zero, then we take `evasive action'
	 *                instead of calculating a value.  (Calculate off tics)?*/


	struct timeval* utime = &rusage->ru_utime;
	time_t usrtime_ms = (utime->tv_sec * 1000) + (utime->tv_usec / 1000);

	double usrtime_s = ((double) usrtime_ms) / 1000;
	printf("\t User time (seconds): %f\n", usrtime_s);
		
	struct timeval* stime = &rusage->ru_stime;
	time_t stime_ms = (stime->tv_sec * 1000) + (stime->tv_usec / 1000);

	double stime_s = ((double) stime_ms) / 1000;
	printf("\t Sys time (seconds): %f\n", stime_s);
		

	printf("\t ru_maxrss: %ld\n", rusage->ru_maxrss);
	printf("\t ru_minflt: %ld\n", rusage->ru_minflt);
	printf("\t ru_majflt: %ld\n", rusage->ru_majflt);
	printf("\t ru_inblock: %ld\n", rusage->ru_inblock);
	printf("\t ru_oublock: %ld\n", rusage->ru_oublock);
	printf("\t ru_nvcsw: %ld\n", rusage->ru_nvcsw);
	printf("\t ru_nivcsw: %ld\n", rusage->ru_nivcsw);
	
	
	//long   ru_maxrss;        /* maximum resident set size */
	//long   ru_minflt;        /* page reclaims (soft page faults) */
	//long   ru_majflt;        /* page faults (hard page faults) */
	//long   ru_inblock;       /* block input operations */
	//long   ru_oublock;       /* block output operations */
	//long   ru_nvcsw;         /* voluntary context switches */
	//long   ru_nivcsw;        /* involuntary context switches */


}


void dest(){
	// don't need to do anything
	printf("PROCESS ENGING\n");

	// get rusage for main thread
	struct thread_rusage* r = (struct thread_rusage*) malloc(sizeof(struct thread_rusage));
	r->tid = syscall(SYS_gettid); 
	getrusage(RUSAGE_THREAD, &(r->rusage));

	// atomically add rusages to list
	pthread_mutex_lock(&thread_rusages_lock);
	thread_rusages.push_front(r);
	pthread_mutex_unlock(&thread_rusages_lock);

	for(auto thread_rusage: thread_rusages){
		//printf("TID: %ld\n", thread_rusage->tid);
		print_thread_rusage(thread_rusage);
		free(thread_rusage);
	}

}

struct routine{
	 void *(*start_routine)(void *);
	 void *arg;
};

void *hook(void *args){

	threadobject.create();

	struct routine* routine = (struct routine*) args;
	void *result = routine->start_routine(routine->arg);

	free(routine);

	return result;
}

// Intercept pthread calls 
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg){
	printf("pthread_create() has been called.[TID=%ld][PID=%d]\n", syscall(SYS_gettid), getpid());
	threadobject.create();	
	
	struct routine *routine = (struct routine*) malloc(sizeof(struct routine));
	routine->start_routine = start_routine;
	routine->arg = arg;
	
	return real_pthread_create(thread, attr, hook, (void *) routine);
}

void pthread_exit(void *retval){
	printf("pthread_exit() has been called.[TID=%ld][PID=%d]\n", syscall(SYS_gettid), getpid());
	real_pthread_exit(retval);
}
