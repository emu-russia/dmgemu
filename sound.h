// SO (sound output) terminal emulation

#pragma once

typedef struct
{
	int hz, len;
	int stereo;
	uint8_t* buf;
	int size;			// in bytes
	int pos;
	FILE* dump;
} PCM;

extern PCM pcm;

int InitSound(unsigned long freq);
void FreeSound(void);
int pcm_submit(void);
