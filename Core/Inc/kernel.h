#pragma once
// kernel.h
// Includes all OS declarations

#define SHPR2 *(uint32_t*)0xE000ED1C //for setting SVC priority, bits 31-24
#define SHPR3 *(uint32_t*)0xE000ED20 // PendSV is bits 23-16
#define _ICSR *(uint32_t*)0xE000ED04 //This lets us trigger PendSV

#include <stdint.h>
#include <stdbool.h>

typedef struct k_thread {
	uint32_t* sp; //stack pointer
	void (*thread_function)(void*); //function pointer
	uint32_t deadline;
	uint32_t current_deadline;
	uint32_t period;
	bool ready;
} k_thread;

void init_thread_stack(uint32_t *stackptr);

void osKernelInitialize(uint32_t idle_task);
bool osKernelStart(void);
bool osCreateThread(uint32_t thread_function, void* args, uint32_t deadline, uint32_t period);
void osYield(void);
