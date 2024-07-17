#include "pch.h"

long oldtimer;
long newtimer;

double microtime() {

	struct timeval time;
	gettimeofday(&time, NULL);
	long microsec = ((unsigned long long)time.tv_sec * 1000000) + time.tv_usec;
	return microsec;
}

// Prepare data structures for timers, call at least once
void TimerInit(void) {

}

// reset Timer
void Timer(void) {

	oldtimer = microtime();
}

// Get timer, no reset
unsigned long GetTimer(void) {

	newtimer = microtime();
	return newtimer - oldtimer;
}

// Get timer and reset
unsigned long GetTimerR(void) {
	unsigned long t = GetTimer();
	oldtimer = newtimer;
	return t;
}

void TimerTest() {
	Timer();
	unsigned long stamp1 = GetTimer();
#ifdef _WIN32
	::Sleep(1000);
#endif
#ifdef _LINUX
	usleep(1000);
#endif
	unsigned long stamp2 = GetTimer();
	printf("TimerTest, ticks between Sleep(1000), should be around 1'000'000: %d\n", (int)(stamp2 - stamp1));
}