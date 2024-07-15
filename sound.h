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

#define WAV_CHANNELS            2
#define WAV_SAMPLEBITS          8
#define WAV_BUFFER_SIZE         6144


#define NBUFFERS 3

int InitSound(unsigned long freq);
void FreeSound(void);
int pcm_submit(void);
