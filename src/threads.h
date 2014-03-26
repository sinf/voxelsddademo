#ifndef _THREADS_H
#define _THREADS_H

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

typedef pthread_t Thread;
typedef pthread_cond_t Cond;
typedef pthread_mutex_t Mutex;
/* typedef sem_t Semaphore; */

#define thread_create pthread_create
#define thread_join pthread_join
#define COND_INITIALIZER PTHREAD_COND_INITIALIZER
#define MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define mutex_lock pthread_mutex_lock
#define mutex_unlock pthread_mutex_unlock
#define cond_signal pthread_cond_signal
#define cond_broadcast pthread_cond_broadcast
#define cond_wait pthread_cond_wait
#define sleep_ms(x) usleep((x)*1000)

#endif
