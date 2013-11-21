/*
	Thread planning functions, used in sched_switch
*/


#ifndef _SCHED_THREADROUTINE_H_
#define _SCHED_THREADROUTINE_H_

#include <hal/context.h>
#include <hal/ipl.h>
#include <stdlib.h>
#include <assert.h>
#include <kernel/critical.h>
#include <kernel/thread.h>
#include <kernel/runnable/runnable.h>
#include <kernel/thread/state.h>
#include <profiler/tracing/trace.h>

#define SCHED_THREAD_REPLANNED 1
#define SCHED_THREAD_REMAINED 0

struct thread_pair{
	struct thread *prev, *next;
};

struct thread_pair *thread_pair_init(struct thread *prev, struct thread *next) {
	struct thread_pair *tp;

	tp = (struct thread_pair *)malloc(sizeof(struct thread_pair));
	tp->prev = prev;
	tp->next = next;

	return tp;
}

static int sched_prepare_thread(struct runnable *p, struct runnable *n,
		struct runq *rq) {

	struct thread *next, *prev;
	struct thread_pair *arg;
	clock_t new_clock;

	/* Upcast runnables to threads */
	prev = (struct thread *) p;
	next = (struct thread *) n;

	arg = thread_pair_init(prev, next);
	next->run_arg = (void *)arg;

	assert(next);
	assert(next->state & (__THREAD_STATE_RUNNING | __THREAD_STATE_READY));

	if (prev == next) {
		ipl_disable();
		return SCHED_THREAD_REMAINED;
	} else {
		if (prev->state & __THREAD_STATE_RUNNING) {
			prev->state |= __THREAD_STATE_READY;
			/* TODO maybe without waiting */
			prev->state &= ~(__THREAD_STATE_RUNNING | __THREAD_STATE_WAITING);
		}
		next->state |= __THREAD_STATE_RUNNING;
		/* TODO maybe without waiting */
		next->state &= ~(__THREAD_STATE_READY | __THREAD_STATE_WAITING);
	}

	if (prev->runnable.policy == SCHED_FIFO && next->runnable.policy != SCHED_FIFO) {
		sched_ticker_init();
	}

	if (prev->runnable.policy != SCHED_FIFO && next->runnable.policy == SCHED_FIFO) {
		sched_ticker_fini(rq);
	}

	/* Running time recalculation */
	new_clock = clock();
	sched_timing_stop(prev, new_clock);
	sched_timing_start(next, new_clock);

	return SCHED_THREAD_REPLANNED;
}

static void sched_execute_thread(void *arg) {
	struct thread_pair *tp;

	tp = (struct thread_pair *)arg;

	trace_point("context switch");

	ipl_disable();

	thread_set_current(tp->next);
	context_switch(&(tp->prev->context), &(tp->next->context));
}

#endif /* _SCHED_THREADROUTINE_H_ */
