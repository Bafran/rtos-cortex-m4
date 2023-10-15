#include <stdio.h>
#include "main.h"
#include "kernel.h"

// Max number of threads/stacks
#define MAX_STACKS 15
#define IDLE_TASK (MAX_STACKS - 1)

// SVC call case numbers
#define RUN_FIRST_THREAD 2
#define YIELD 3

extern void runFirstThread(void);

k_thread threads[MAX_STACKS] = {0};
uint8_t current_thread = 0;
uint8_t active_threads = 0;

static uint32_t* s_stackptr = 0;

void SVC_Handler_Main( unsigned int *svc_args )
	{
		unsigned int svc_number;
		uint32_t min_deadline = UINT32_MAX;
		/*
		* Stack contains:
		* r0, r1, r2, r3, r12, r14, the return address and xPSR
		* First argument (r0) is svc_args[0]
		*/
		svc_number = ( ( char * )svc_args[ 6 ] )[ -2 ] ;
		switch( svc_number )
		{
		case 0:
			printf("Success!\r\n");
			break;
		case 1:
			printf("Something else!\r\n");
			break;
		case RUN_FIRST_THREAD:
			for (uint8_t i = 0; i < active_threads; i++) {
				if (threads[i].current_deadline < min_deadline) {
					min_deadline = threads[i].current_deadline;
					current_thread = i;
				}
			}
			__set_PSP((uint32_t)threads[current_thread].sp);
			runFirstThread();
			break;
		case YIELD:
			// Pend an interrupt to do the context switch
			_ICSR |= 1<<28;
			__asm("isb");
			break;
		default: /* unknown SVC */
			break;
	}
}

static bool init_new_thread_stack(uint32_t thread_function, void* args, uint32_t deadline, uint32_t period) {
	// Subtract one because the idle task is the last thread
	if(active_threads >= MAX_STACKS-1) {
		return false;
	}
	uint32_t* new_stackptr = 0;
	// MSP_INIT_VAL - 0x200 * number of stacks already created
	new_stackptr = *(uint32_t**)0x0 - (0x200 * (active_threads+1));

	*(--new_stackptr) = 1 << 24;
	*(--new_stackptr) = (uint32_t)thread_function;
	for (int i = 0; i < 5; i++) {
	  *(--new_stackptr) = 0x0A;
	}
	*(--new_stackptr) = (uint32_t)args; // R0
	for (int i = 0; i < 8; i++) {
	  *(--new_stackptr) = 0x0A;
	}
	s_stackptr = new_stackptr;

	// Store SP and thread function in correct location
	threads[active_threads].sp = new_stackptr;
	threads[active_threads].thread_function = (void *)thread_function;
	threads[active_threads].deadline = deadline;
	threads[active_threads].current_deadline = deadline;
	threads[active_threads].period = period;
	threads[active_threads].ready = true;
	current_thread = active_threads;
	active_threads++;
	return true;
}

bool osCreateThread(uint32_t thread_function, void* args, uint32_t deadline, uint32_t period) {
	if(!init_new_thread_stack(thread_function, args, deadline, period)) {
		return false;
	}
	return true;
}

void osKernelInitialize(uint32_t idle_task) {
	uint32_t* idle_stackptr = 0;
	idle_stackptr = *(uint32_t**)0x0 - (0x200 * (IDLE_TASK+1));
	*(--idle_stackptr) = 1 << 24;
	*(--idle_stackptr) = (uint32_t) idle_task;
	idle_stackptr -= 14;

	threads[IDLE_TASK].sp = idle_stackptr;
	threads[IDLE_TASK].thread_function = (void *) idle_task;
	threads[IDLE_TASK].deadline = UINT32_MAX;
	threads[IDLE_TASK].current_deadline = UINT32_MAX;
	threads[IDLE_TASK].period = UINT32_MAX;

	// Set the priority of PendSV to almost the weakest
	SHPR3 |= 0xFE << 16; // Shift the constant 0xFE 16 bits to set PendSV priority
	SHPR2 |= 0xFDU << 24; // Set the priority of SVC higher than PendSV
	return;
}

bool osKernelStart(void) {
	// Must have one thread created
	if(active_threads == 0) {
		return false;
	}
	__asm("SVC #2");
	return true;
}

// This function must set PSP, this is how it picks the next thread,
// essentially the return value.
void osSched(void) {
	uint32_t min_deadline = UINT32_MAX;
	threads[current_thread].sp = (uint32_t*)(__get_PSP() - 8*4);

	bool scheduled = false;
	for (uint8_t i = 0; i < active_threads; i++) {
		if (threads[i].current_deadline < min_deadline && threads[i].ready) {
			min_deadline = threads[i].current_deadline;
			current_thread = i;
			scheduled = true;
		}
	}

	if (scheduled) {
		__set_PSP((uint32_t)threads[current_thread].sp);
	} else {
		current_thread = IDLE_TASK;
		__set_PSP((uint32_t)threads[IDLE_TASK].sp);
	}
	return;
}

void osYield(void) {
	threads[current_thread].ready = false;
	__asm("SVC #3");
}
