// SO (sound output) terminal emulation (via SDL2)
#include "pch.h"

#define WAV_CHANNELS            2
#define WAV_SAMPLEBITS          8
#define WAV_BUFFER_SIZE         512				// Length of one chunk for audio playback 
#define WAV_BUFFER_CHUNKS		32				// Reserve for circular buffer (total number of chunks)

SDL_AudioSpec spec;
SDL_AudioSpec spec_obtainted;
SDL_AudioDeviceID dev_id;

static void SDLCALL Mixer(void* unused, Uint8* stream, int len);

int8_t* SampleBuf;
int SampleBuf_WrPtr;		// in stereo-samples
int SampleBuf_RdPtr;		// in stereo-samples
int SampleBuf_Size;			// in stereo-samples

static void SDLCALL Mixer(void* unused, Uint8* stream, int len)
{
	int dist = SampleBuf_WrPtr - SampleBuf_RdPtr;
	if (dist < 0) dist = -dist;
	if (dist * WAV_CHANNELS < len) {
		return;
	}

	for (int n = 0; n < len / WAV_CHANNELS; n++) {
		stream[2 * n] = SampleBuf[WAV_CHANNELS * SampleBuf_RdPtr];
		stream[2 * n + 1] = SampleBuf[WAV_CHANNELS * SampleBuf_RdPtr + 1];
		SampleBuf_RdPtr++;
		if (SampleBuf_RdPtr >= SampleBuf_Size) {
			SampleBuf_RdPtr = 0;
		}
	}
}

int InitSound(int freq)
{
	SampleBuf_Size = WAV_BUFFER_SIZE * WAV_BUFFER_CHUNKS;
	SampleBuf = new int8_t[SampleBuf_Size * WAV_CHANNELS];
	memset(SampleBuf, 0, SampleBuf_Size * WAV_CHANNELS);
	SampleBuf_WrPtr = 0;
	SampleBuf_RdPtr = 0;

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		__log ("SDL audio could not initialize! SDL_Error: %s\n", SDL_GetError());
		return 0;
	}

	spec.freq = freq;
	spec.format = AUDIO_S8;
	spec.channels = WAV_CHANNELS;
	spec.samples = WAV_BUFFER_SIZE;
	spec.callback = Mixer;
	spec.userdata = nullptr;

	dev_id = SDL_OpenAudioDevice(NULL, 0, &spec, &spec_obtainted, 0);
	SDL_PauseAudioDevice(dev_id, 0);
	return 1;
}

void FreeSound(void)
{
	SDL_CloseAudioDevice(dev_id);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	if (SampleBuf != nullptr) {
		delete[] SampleBuf;
		SampleBuf = nullptr;
	}
}

void pop_sample(int l, int r)
{
	if (SampleBuf != nullptr) {
		SampleBuf[WAV_CHANNELS * SampleBuf_WrPtr] = l;
		SampleBuf[WAV_CHANNELS * SampleBuf_WrPtr + 1] = r;
		SampleBuf_WrPtr++;

		if (SampleBuf_WrPtr >= SampleBuf_Size)
		{
			SampleBuf_WrPtr = 0;
		}
	}
}
