// SO (sound output) terminal emulation (via SDL2)
#include "pch.h"

#define WAV_CHANNELS            2
#define WAV_BUFFER_SIZE         6144

SDL_AudioSpec spec;
SDL_AudioSpec spec_obtainted;
SDL_AudioDeviceID dev_id;

static void SDLCALL Mixer(void* unused, Uint8* stream, int len);

int8_t* SampleBuf;
int SampleBuf_Ptr;
int SampleBuf_Size;
bool Dma;

static void SDLCALL Mixer(void* unused, Uint8* stream, int len)
{
	if (Dma) {
		SDL_MixAudioFormat(stream, (const Uint8*)SampleBuf, AUDIO_S16SYS, len, SDL_MIX_MAXVOLUME);
		Dma = false;
	}
	else {
		memset(SampleBuf, 0, SampleBuf_Size * WAV_CHANNELS);
		SDL_PauseAudioDevice(dev_id, 1);
	}
}

int InitSound(int freq)
{
	SampleBuf_Size = WAV_BUFFER_SIZE;
	SampleBuf = new int8_t[SampleBuf_Size * WAV_CHANNELS];
	memset(SampleBuf, 0, SampleBuf_Size * WAV_CHANNELS);
	SampleBuf_Ptr = 0;

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		__log ("SDL audio could not initialize! SDL_Error: %s\n", SDL_GetError());
		return 1;
	}

	spec.freq = freq;
	spec.format = AUDIO_S8;
	spec.channels = WAV_CHANNELS;
	spec.samples = 1024;
	spec.callback = Mixer;
	spec.userdata = nullptr;

	dev_id = SDL_OpenAudioDevice(NULL, 0, &spec, &spec_obtainted, 0);
	SDL_PauseAudioDevice(dev_id, 1);
	return 0;
}

void FreeSound(void)
{
	SDL_CloseAudioDevice(dev_id);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	delete[] SampleBuf;
}

static void Playback()
{
	Dma = 1;
	SDL_PauseAudioDevice(dev_id, 0);
	SampleBuf_Ptr = 0;
}

void pop_sample(int l, int r)
{
	SampleBuf[WAV_CHANNELS * SampleBuf_Ptr] = l;
	SampleBuf[WAV_CHANNELS * SampleBuf_Ptr + 1] = r;
	SampleBuf_Ptr++;

	if (SampleBuf_Ptr >= SampleBuf_Size)
	{
		Playback();
	}
}
