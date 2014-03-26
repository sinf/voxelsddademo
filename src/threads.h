#ifndef _THREADS_H
#define _THREADS_H

#ifdef _POSIX_C_SOURCE

/* Use POSIX threads directly */
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
typedef pthread_t Thread;
typedef pthread_cond_t Cond;
typedef pthread_mutex_t Mutex;
#define thread_create(t,func,arg) pthread_create(t,NULL,func,arg)
#define thread_join(t) pthread_join((t),NULL)
#define COND_INITIALIZER PTHREAD_COND_INITIALIZER
#define MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define mutex_lock pthread_mutex_lock
#define mutex_unlock pthread_mutex_unlock
#define cond_signal pthread_cond_signal
#define cond_broadcast pthread_cond_broadcast
#define cond_wait pthread_cond_wait

#else

/* Use SDL threads */
#include <SDL_thread.h>
typedef SDL_Thread *Thread;
typedef SDL_mutex *Mutex;
typedef SDL_cond *Cond;
#define thread_create(t,func,arg) *(t)=SDL_CreateThread((int(*)(void*))(func),(arg))
#define thread_join(t) SDL_WaitThread((t),NULL)
#define NEED_EXPLICIT_MUTEX_INIT
#define MUTEX_INITIALIZER NULL
#define COND_INITIALIZER NULL
#define mutex_init(m) *(m)=SDL_CreateMutex();
#define cond_init(c) *(c)=SDL_CreateCond();
#define mutex_lock(m) SDL_mutexP(*m)
#define mutex_unlock(m) SDL_mutexV(*m)
#define cond_signal(c) SDL_CondSignal(*c)
#define cond_broadcast(c) SDL_CondBroadcast(*c)
#define cond_wait(c,m) SDL_CondWait(*c,*m)

#endif

#endif
