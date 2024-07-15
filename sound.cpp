// SO (sound output) terminal emulation
#include "pch.h"

static HWAVEOUT hWaveOut;

static DWORD gSndBufSize;

PCM pcm;

#define THRESHOLD1 (pcm.len*2)
#define THRESHOLD2 (pcm.len*4)

/* Additional definitions by E}I{

*/

struct WBuffer {
	HGLOBAL hWaveHdr;
	LPWAVEHDR lpWaveHdr;
	HANDLE hData;
	HPSTR lpData;
} wavebuffer[NBUFFERS];

int wb_current, wb_free;

void FreeWaveBuffer(struct WBuffer* w) {

	// I wanted to make check for DONE flag, but it should be on because of "Reset" 

	if (w->lpWaveHdr /*&& w->lpWaveHdr->dwFlags & WHDR_PREPARED*/)
		waveOutUnprepareHeader(hWaveOut, w->lpWaveHdr, sizeof(WAVEHDR));
	if (w->hWaveHdr)
	{
		__log("...freeing WAV header\n");
		GlobalUnlock(w->hWaveHdr);
		GlobalFree(w->hWaveHdr);
	}

	if (w->hData)
	{
		__log("...freeing WAV buffer\n");
		GlobalUnlock(w->hData);
		GlobalFree(w->hData);
	}
	memset(w, 0, sizeof(*w));
}

void AllocWaveBuffer(struct WBuffer* w) {

	//int i;
	FreeWaveBuffer(w);	// Who knows :)

	__log("...allocating waveform buffer: ");

	if (!(w->hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_ZEROINIT, gSndBufSize)))
	{
		__log("\t failed\n");
		FreeSound();
		return;
	}
	__log("\tok\n");

	__log("...locking waveform buffer: ");
	if (!(w->lpData = (HPSTR)GlobalLock(w->hData)))
	{
		__log("\t failed\n");
		FreeSound();
		return;
	}
	__log("\tok\n");

	/*
	 * Allocate and lock memory for the header. This memory must
	 * also be globally allocated with GMEM_MOVEABLE and
	 * GMEM_SHARE flags.
	 */
	__log("...allocating waveform header: ");

	if ((w->hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_ZEROINIT,
		(DWORD)sizeof(WAVEHDR))) == NULL)
	{
		__log("\tfailed\n");
		FreeSound();
		return;
	}
	__log("\tok\n");

	__log("...locking waveform header: ");


	if ((w->lpWaveHdr = (LPWAVEHDR)GlobalLock(w->hWaveHdr)) == NULL)
	{
		__log("\tfailed\n");
		FreeSound();
		return;
	}
	__log("\tok\n");
}

void CALLBACK BufferFinished(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2) {
	if (uMsg == WOM_DONE) {
		wb_free++;
		pcm_submit();
	}
}

void PlayWaveBuffer(struct WBuffer* w)
{
	/* After allocation, set up and prepare headers. */
	if (!w->lpWaveHdr) return;
	if (!w->lpWaveHdr->dwBufferLength ||	// 1-st time usage
		w->lpWaveHdr->dwFlags & WHDR_DONE) { // only use this block if done
		waveOutUnprepareHeader(hWaveOut, w->lpWaveHdr, sizeof(WAVEHDR));
		w->lpWaveHdr->dwFlags = 0;
		w->lpWaveHdr->dwBufferLength = gSndBufSize;
		w->lpWaveHdr->lpData = w->lpData;
		if (waveOutPrepareHeader(hWaveOut, w->lpWaveHdr, sizeof(WAVEHDR)) !=
			MMSYSERR_NOERROR) {
			__log("waveOutPrepareHeader failed\n");
			//FreeSound ();
			return;
		}
		if (waveOutWrite(hWaveOut, w->lpWaveHdr, sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
			wb_free--; // decrement number of free buffers
		}
	}
}

int InitSound(unsigned long freq)
{
	WAVEFORMATEX  format;
	HRESULT         hr;
	unsigned int i;

	__log("Initializing wave sound\n");

	memset(&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = WAV_CHANNELS;
	format.wBitsPerSample = WAV_SAMPLEBITS;
	format.nSamplesPerSec = freq;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;	// =1 :) E}I{
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	/* Open a waveform device for output using window callback. */
	__log("...opening waveform device: ");
	while ((hr = waveOutOpen((LPHWAVEOUT)&hWaveOut, WAVE_MAPPER,
		&format,
		(DWORD_PTR)BufferFinished, 0L, CALLBACK_FUNCTION)) != MMSYSERR_NOERROR)
	{
		if (hr != MMSYSERR_ALLOCATED)
		{
			__log("\tfailed\n");
			return 0;
		}

		if (MessageBox(NULL,
			"The sound hardware is in use by another app.\n\n"
			"Select Retry to try to start sound again or Cancel to run emulator without sound.",
			"Sound not available",
			MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
		{/* Quake 2 znachit? :) A ya dumal iz MSDN-ovskogo helpa, tam to zhe samoe
		 chast' etoy labudy s vydeleniem pamyati-fignya, u menya(i ne tol'ko u menya) malloc rabotal otlichno
		 (t.e. zachem vydelyat' MOVEABLE block, a zatem vse ravno ego permanantno GlobalLock?  )
		 */
			__log("\thw in use\n");
			return 0;
		}
	}
	__log("\tok\n");

	waveOutReset(hWaveOut);

	/*
	 * Allocate and lock memory for the waveform data. The memory
	 * for waveform data must be globally allocated with
	 * GMEM_MOVEABLE and GMEM_SHARE flags.

	*/
	gSndBufSize = WAV_BUFFER_SIZE;
	//for (i = 1; i < gSndBufSize; i <<= 1);
	//gSndBufSize = i;
	for (i = 0; i < NBUFFERS; i++) AllocWaveBuffer(wavebuffer + i);

	wb_free = NBUFFERS;
	pcm.pos = 0;
	wb_current = 0;
	pcm.stereo = WAV_CHANNELS - 1;
	pcm.hz = freq;
	pcm.len = gSndBufSize;//WAV_BUFFER_SIZE;

	return 1;
}

void FreeSound(void)
{
	int i;
	__log("Shutting down sound system\n");

	if (hWaveOut)
	{
		__log("...resetting waveOut\n");
		waveOutReset(hWaveOut);

		for (i = 0; i < NBUFFERS; i++) FreeWaveBuffer(wavebuffer + i);

		__log("...closing waveOut\n");
		waveOutClose(hWaveOut);
	}
}

/*
==============
psm_submit aka SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
int pcm_submit(void)
{
	int tmp;
	//if(pcm_dump) fwrite(pcm.buf, 1, pcm.len, pcm_dump);

	/*
	 * Now the data block can be sent to the output device. The
	 * waveOutWrite function returns immediately and waveform
	 * data is sent to the output device in the background.
	*/


	while (wb_free) {

		tmp = pcm.pos - pcm.len;
		if (tmp < 0) {
			// To prevent the sound from clicking at the first frames you need to fill the buffer with the value 0x7f.
			memset(pcm.buf + pcm.pos, 0x7f, -tmp);
			tmp = 0;
		}
		memcpy(wavebuffer[wb_current].lpData, pcm.buf, pcm.len);
		//pcm.pos = 0;
		if (tmp) memmove(pcm.buf, pcm.buf + pcm.len, tmp);
		/*if(wb_free<NBUFFERS && tmp>THRESHOLD1) {
			tmp-=(pcm.stereo+1)<<2;
		}*/
		if (tmp > THRESHOLD2) tmp = THRESHOLD2;
		//pcm.pos = 0;
		pcm.pos = tmp;
		PlayWaveBuffer(wavebuffer + wb_current);
		if (++wb_current >= NBUFFERS) wb_current = 0;
	}
	return 0;
}
