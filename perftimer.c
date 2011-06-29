#include <windows.h>
/*
The perfect timer :) code (C) by E}I{  ej@tut.by
this part of gpuE}I{soft can be redistributed and modified
in either source or binary form as long as this notice isn't changed or removed.

Code requires x98 MMX CPU and Win32 VC5.0 compatible compiler.
*/


// Timer () will return time passed from previous call in mksec as ulong

static __int64 oldtimer,newtimer;
static __int64 kf;
static unsigned long dv;

// Prepare data structures for timers, call at least once
void TimerInit(void) {
	QueryPerformanceFrequency((LARGE_INTEGER*)&kf);
	_asm {
		mov edx,1000000
		xor eax,eax
		//movq mm0,[kf]
		mov ebx,DWORD PTR [kf]
		mov ecx,DWORD PTR [kf+4]
lp:
		test ecx,ecx
		jz skip
		shr ecx,1
		shr edx,1
		rcr eax,1
		jmp lp
skip:
		div ebx

		mov [dv],eax
	}
	__asm EMMS;
}

// reset Timer
void Timer(void) {
	QueryPerformanceCounter((LARGE_INTEGER*)&oldtimer);
	__asm EMMS;
}

// Get timer, no reset
unsigned long GetTimer(void) {
	unsigned long t;
	QueryPerformanceCounter((LARGE_INTEGER*)&newtimer);
	__asm {
		movq mm0,[newtimer]
		movq mm1,[oldtimer]
		movq mm2,mm0
		psubd mm0,mm1
		mov ebx,[dv]
		movd eax,mm0
		mul ebx
		mov [t],edx
	}
	__asm EMMS;
	return t;
}

// Get timer and reset
unsigned long GetTimerR(void) {
	unsigned long t;
	QueryPerformanceCounter((LARGE_INTEGER*)&newtimer);
	__asm {
		movq mm0,[newtimer]
		movq mm1,[oldtimer]
		movq mm2,mm0
		psubd mm0,mm1
		mov ebx,[dv]
		movd eax,mm0
		mul ebx
		mov [t],edx
		movq [oldtimer],mm2
	}
	__asm EMMS;
	return t;
}
