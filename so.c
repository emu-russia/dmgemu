/* GameBoy SO (sound output) terminal emulation */
#include "gbemu.h"

// perftimer-good timer implementation for win32/x86MMX
#include "perftimer.h"


/* Warning! like in core, sound clock is incremented
at rate of 1048576Hz, not 1048576*4 Hz*/

typedef struct
{
    int hz, len;
    int stereo;
    uint8_t *buf;
    int pos;
} PCM;

PCM pcm;
static FILE *pcm_dump;

void so_reset();

#define WAV_CHANNELS            2
#define WAV_SAMPLEBITS          8
#define WAV_BUFFER_SIZE         6144


#define THRESHOLD1 (pcm.len*2)
#define THRESHOLD2 (pcm.len*4)

#define NBUFFERS 3



HWAVEOUT hWaveOut;


DWORD gSndBufSize;

/* Additional definitions by E}I{

*/

struct WBuffer {
	HGLOBAL hWaveHdr;
	LPWAVEHDR lpWaveHdr;
	HANDLE hData;
	HPSTR lpData;
} wavebuffer[NBUFFERS];

int wb_current,wb_free;



int pcm_submit(void);
void CALLBACK BufferFinished(  HWAVEOUT hwo,        UINT uMsg,         
						  DWORD dwInstance,    DWORD dwParam1,      DWORD dwParam2);

/*
==================
FreeWaveBuffer
==================
*/
void FreeWaveBuffer (struct WBuffer *w) {
	
// I wanted to make check for DONE flag, but it should be on because of "Reset" 

	if (w->lpWaveHdr /*&& w->lpWaveHdr->dwFlags & WHDR_PREPARED*/)
            waveOutUnprepareHeader (hWaveOut, w->lpWaveHdr, sizeof(WAVEHDR));
	if (w->hWaveHdr)
        {
            __log( "...freeing WAV header\n" );
            GlobalUnlock(w->hWaveHdr);
            GlobalFree(w->hWaveHdr);
        }

        if (w->hData)
        {
            __log( "...freeing WAV buffer\n" );
            GlobalUnlock(w->hData);
            GlobalFree(w->hData);
        }
	memset(w,0,sizeof(*w));
}

/*
==================
AllocWaveBuffer
==================
*/

void FreeSound (void);

void AllocWaveBuffer (struct WBuffer *w) {

	//int i;
	FreeWaveBuffer(w);	// Who knows :)

	__log ("...allocating waveform buffer: ");
    
    if (!(w->hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_ZEROINIT, gSndBufSize))) 
    { 
        __log( "\t failed\n" );
        FreeSound ();
        return; 
    }
    __log( "\tok\n" );

    __log ("...locking waveform buffer: ");
    if (!(w->lpData = GlobalLock(w->hData)))
    { 
        __log( "\t failed\n" );
        FreeSound ();
        return; 
    } 
    //memset (w->lpData, 0, gSndBufSize); 
    __log( "\tok\n" );

    /* 
     * Allocate and lock memory for the header. This memory must 
     * also be globally allocated with GMEM_MOVEABLE and 
     * GMEM_SHARE flags. 
     */ 
    __log ("...allocating waveform header: ");
    
    if ((w->hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_ZEROINIT, 
        (DWORD) sizeof(WAVEHDR))) == NULL)
    { 
        __log( "\tfailed\n" );
        FreeSound ();
        return; 
    } 
    __log( "\tok\n" );

    __log ("...locking waveform header: ");
     
	
    if ((w->lpWaveHdr = (LPWAVEHDR) GlobalLock(w->hWaveHdr)) == NULL)
    { 
        __log( "\tfailed\n" );
        FreeSound ();
        return; 
    }
    //memset (w->lpWaveHdr, 0, sizeof(WAVEHDR));
    __log( "\tok\n" );


}

/*
==================
FreeSound
==================
*/

void FreeSound (void)
{
	int i;
    __log( "Shutting down sound system\n" );

    if ( hWaveOut )
    {
        __log( "...resetting waveOut\n" );
        waveOutReset (hWaveOut);

        for(i=0;i<NBUFFERS;i++) FreeWaveBuffer(wavebuffer+i);

        __log( "...closing waveOut\n" );
        waveOutClose (hWaveOut);

    

    }
}


void CALLBACK BufferFinished(  HWAVEOUT hwo,        UINT uMsg,         
						  DWORD dwInstance,    DWORD dwParam1,      DWORD dwParam2) {
	if(uMsg == WOM_DONE) {
		wb_free++;
		pcm_submit();
	}
}

void PlayWaveBuffer (struct WBuffer *w)
{
	/* After allocation, set up and prepare headers. */ 
    if(!w->lpWaveHdr) return;
	if(!w->lpWaveHdr->dwBufferLength ||	// 1-st time usage
		w->lpWaveHdr->dwFlags & WHDR_DONE) { // only use this block if done
		waveOutUnprepareHeader(hWaveOut, w->lpWaveHdr, sizeof(WAVEHDR) );
		w->lpWaveHdr->dwFlags = 0;
		w->lpWaveHdr->dwBufferLength = gSndBufSize;
		w->lpWaveHdr->lpData = w->lpData;
		if (waveOutPrepareHeader(hWaveOut, w->lpWaveHdr, sizeof(WAVEHDR)) !=
                MMSYSERR_NOERROR) {
        __log ("waveOutPrepareHeader failed\n");
        //FreeSound ();
        return;
		}
		if(waveOutWrite(hWaveOut, w->lpWaveHdr, sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
			wb_free--; // decrement number of free buffers
		}
	}
}
/*
==================
SNDDM_InitWav

Crappy windows multimedia base
==================
*/
int SNDDMA_InitWav (unsigned long freq)
{
    WAVEFORMATEX  format; 
    HRESULT         hr;
    unsigned int i;

    __log( "Initializing wave sound\n" );

    memset (&format, 0, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = WAV_CHANNELS;
    format.wBitsPerSample = WAV_SAMPLEBITS;
    format.nSamplesPerSec = freq;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;	// =1 :) E}I{
    format.cbSize = 0;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign; 
    
    /* Open a waveform device for output using window callback. */ 
    __log ("...opening waveform device: ");
    while ((hr = waveOutOpen((LPHWAVEOUT)&hWaveOut, WAVE_MAPPER, 
                    &format, 
                    (DWORD)BufferFinished, 0L, CALLBACK_FUNCTION)) != MMSYSERR_NOERROR)
    {
        if (hr != MMSYSERR_ALLOCATED)
        {
            __log ("\tfailed\n");
            return 0;
        }

        if (MessageBox (NULL,
                        "The sound hardware is in use by another app.\n\n"
                        "Select Retry to try to start sound again or Cancel to run emulator without sound.",
                        "Sound not available",
                        MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION) != IDRETRY)
        {/* Quake 2 znachit? :) A ya dumal iz MSDN-ovskogo helpa, tam to zhe samoe
		 chast' etoy labudy s vydeleniem pamyati-fignya, u menya(i ne tol'ko u menya) malloc rabotal otlichno
		 (t.e. zachem vydelyat' MOVEABLE block, a zatem vse ravno ego permanantno GlobalLock?  )
		 */
            __log ("\thw in use\n" );
            return 0;
        }
    } 
    __log( "\tok\n" );

    waveOutReset(hWaveOut);

    /* 
     * Allocate and lock memory for the waveform data. The memory 
     * for waveform data must be globally allocated with 
     * GMEM_MOVEABLE and GMEM_SHARE flags. 

    */ 
	gSndBufSize = WAV_BUFFER_SIZE;
    //for (i = 1; i < gSndBufSize; i <<= 1);
    //gSndBufSize = i;
	for(i=0;i<NBUFFERS;i++) AllocWaveBuffer(wavebuffer+i);
    
	wb_free = NBUFFERS;
	pcm.pos = 0;
	wb_current = 0;
    pcm.stereo = WAV_CHANNELS - 1;
    pcm.hz = freq;
    pcm.len = gSndBufSize;//WAV_BUFFER_SIZE;

    return 1;
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

	
	while( wb_free) {

		tmp=pcm.pos-pcm.len;
		if(tmp<0) {
			memset(pcm.buf+pcm.pos,pcm.buf[pcm.pos-2],-tmp);
			tmp = 0;
		}
		memcpy(wavebuffer[wb_current].lpData,pcm.buf,pcm.len);
		//pcm.pos = 0;
		if(tmp) memmove(pcm.buf,pcm.buf+pcm.len,tmp);
		/*if(wb_free<NBUFFERS && tmp>THRESHOLD1) {
			tmp-=(pcm.stereo+1)<<2;
		}*/
		if(tmp>THRESHOLD2) tmp=THRESHOLD2;
		//pcm.pos = 0;
		pcm.pos = tmp;
		PlayWaveBuffer(wavebuffer+wb_current);
		if(++wb_current>=NBUFFERS) wb_current=0;
	}
	return 0;
}

// **********************************************************************

typedef struct
{
    int on;
    unsigned pos;
    unsigned cnt,encnt,swcnt;
    //int len, enlen, swlen;
    //int swfreq;
    unsigned freq,swfreq;
    unsigned envol;//, endir;
} sndchan;

typedef struct
{
    unsigned outfreq,ratelo,ratehi,z0;
    sndchan ch[4];
    uint8_t wave[16];
} so;

#define SO_FREQ (1<<20)

#define RI_NR10 0x10
#define RI_NR11 0x11
#define RI_NR12 0x12
#define RI_NR13 0x13
#define RI_NR14 0x14
#define RI_NR21 0x16
#define RI_NR22 0x17
#define RI_NR23 0x18
#define RI_NR24 0x19
#define RI_NR30 0x1A
#define RI_NR31 0x1B
#define RI_NR32 0x1C
#define RI_NR33 0x1D
#define RI_NR34 0x1E
#define RI_NR41 0x20
#define RI_NR42 0x21
#define RI_NR43 0x22
#define RI_NR44 0x23
#define RI_NR50 0x24
#define RI_NR51 0x25
#define RI_NR52 0x26

const static uint8_t dmgwave[16] =
{
    0xac, 0xdd, 0xda, 0x48,
    0x36, 0x02, 0xcf, 0x16,
    0x2c, 0x04, 0xe5, 0x2c,
    0xac, 0xdd, 0xda, 0x48
};

const static uint8_t sqwave[4][8] =
{
    {  0, 0,-1, 0, 0, 0, 0, 0 },
    {  0,-1,-1, 0, 0, 0, 0, 0 },
    { -1,-1,-1,-1, 0, 0, 0, 0 },
    { -1, 0, 0,-1,-1,-1,-1,-1 }
};

const static int divtab[8] =
{
    1,
    2,
    4,
    6,
    8,
    10,
    12,
    14
};

so snd;

static uint8_t noise7[16];
static uint8_t noise15[4096];

static void makenoise(uint8_t *to,int nbits);

PLAT void so_init(unsigned long freq)
{
    SNDDMA_InitWav(freq);
	makenoise(noise15,15);
	makenoise(noise7,7);
	
	pcm.buf = (uint8_t *)malloc(WAV_BUFFER_SIZE*16*(pcm.stereo+1));
	pcm_submit();

//    pcm_dump = fopen("pcm.bin", "wb");
	so_clk_inner[1]=0;
    so_clk_inner[0] = gb_clk;
    so_reset();
}

PLAT void so_shutdown()
{
    if(pcm_dump) fclose(pcm_dump);
	pcm.pos = 0;
    FreeSound();
	free(pcm.buf);
	pcm.buf=NULL;
    memset(&pcm, 0, sizeof(pcm));
    so_reset();
}

// **********************************************************************

/*
Questionable aspects of SO terminal emulation:

NR51 contains updated "sound enable" flags
Frequency registers can update if sweep is operating
_nothing_ else that changed in the process of sound generation
Envelope/sweep divider/length register counters are reset through flipping bit 7(initial) of NR X7
Frequency 

according to docs, only way to generate reset signal is to make sound stop by reaching
zero volume envelope, counter-based stop mode will not really stop counters,
but will only set channel output to zero. If you reset length counter,
it will probably count again and sound will be hearable.
real full stop mode is activated (only) by setting(or reaching) zero envelope with "down"
direction counter
int his implementation full stop is caused by 3 situations: length counter reaches 

everything not noted here is updated immediately
256/128/64 Hz counters are derived from corresponding bits of main counter
(as probably assumed in reference schematics)
*/



//#define RATE (snd.rate)
#define OUT_FREQ (snd.outfreq)
#define RATELO (snd.ratelo)
#define RATEHI (snd.ratehi)
#define WAVE (snd.wave) /* ram.hi+0x30 */
#define S1 (snd.ch[0])
#define S2 (snd.ch[1])
#define S3 (snd.ch[2])
#define S4 (snd.ch[3])

unsigned long so_clk_inner[2];
unsigned long so_clk_nextchange;

static void makenoise(uint8_t *to,int nbits) {
	unsigned i,j,counter,acc,tmp;
	counter = (1<<nbits)-1;
	i=1<<(nbits-- -3);
	for(;i>0;i--) {
		acc = 0;
		for(j=8;j>0;j--) {
			acc=(acc<<1)|(counter&1);
			tmp=counter>>1;
			counter=tmp|(((tmp^counter)&1) << nbits);
		}
		*to++ = acc;
	}
}

/*static void s1_freq_d(int d)
{
    if (RATE > (d<<4)) S1.freq = 0;
    else S1.freq = (RATE << 17)/d;
}*/

static void s1_freq_d(unsigned freq) {
	unsigned d = 2048 - (2047&freq);
	if(OUT_FREQ) S1.freq = (SO_FREQ<<11)/(OUT_FREQ*d>>3);     // 14 bits frac
}

static void s1_freq() {
	s1_freq_d(*(unsigned short*)&R_NR13);
}

static void s2_freq() {
	unsigned d = 2048 - (2047&*(unsigned short*)&R_NR23);
    if(OUT_FREQ) S2.freq = (SO_FREQ<<11)/(OUT_FREQ*d>>3);
}

static void s3_freq() {
	unsigned d = 2048 - (2047&*(unsigned short*)&R_NR33);
    if(OUT_FREQ) S3.freq = (SO_FREQ<<11)/(OUT_FREQ*d>>6);  // +3,because there is no duty circuit
}

static void s4_freq() {
    if(OUT_FREQ) S4.freq = (SO_FREQ<<11)/(OUT_FREQ*divtab[R_NR43&7]) << 5 >> (R_NR43 >> 4); //17 bits frac
}

void so_dirty()
{
    s1_freq();
    
    s2_freq();
    s3_freq();
	S3.cnt = (256-R_NR31);
	S1.cnt = (64-(R_NR11&63));
	S2.cnt = (64-(R_NR21&63));
    S4.cnt = (64-(R_NR41&63));
	S1.encnt = 0;
	S1.swcnt = 0;
	S2.encnt = 0;
	S4.encnt = 0;

	S1.envol = R_NR12 >> 4;
	S2.envol = R_NR22 >> 4;
    S4.envol = R_NR42 >> 4;
    s4_freq();
}

void so_off()
{
	memset(&snd.ch, 0, sizeof snd.ch);
    /*memset(&S1, 0, sizeof S1);
    memset(&S2, 0, sizeof S2);
    memset(&S3, 0, sizeof S3);
    memset(&S4, 0, sizeof S4);*/
    R_NR10 = 0x80;
    R_NR11 = 0xBF;
    R_NR12 = 0xF3;
    R_NR14 = 0xBF;
    R_NR21 = 0x3F;
    R_NR22 = 0x00;
    R_NR24 = 0xBF;
    R_NR30 = 0x7F;
    R_NR31 = 0xFF;
    R_NR32 = 0x9F;
    R_NR33 = 0xBF;
    R_NR41 = 0xFF;
    R_NR42 = 0x00;
    R_NR43 = 0x00;
    R_NR44 = 0xBF;
    R_NR50 = 0x77;
    R_NR51 = 0xF3;
    R_NR52 = 0xF1;
    so_dirty();
}

void so_reset()
{
    memset(&snd, 0, sizeof snd);
    if (pcm.hz) {
		// stupid 48 bit division :)
		RATEHI = SO_FREQ / (OUT_FREQ=pcm.hz);  // higher part of rate
		RATELO = ((SO_FREQ-RATEHI*OUT_FREQ) << 16)/OUT_FREQ; // lower 16 bits
	}
	// TODO: for SGB use another sound frequency(also for other counters including envelopes etc.)
    
	pcm.pos = 0;
	so_clk_inner[0] = 0;
	so_clk_inner[1] = gb_clk;
	so_clk_nextchange = (gb_clk&~0xFFF)+0x1000; // 256 Hz divider
    memcpy(WAVE, dmgwave, 16);
    memcpy(&hram[0x100+0x30], WAVE, 16);
	so_off();
	R_NR52 |= 0x80;
}

// **********************************************************************

uint8_t so_read(uint8_t r)
{
    so_mix();
    return hram[0x100 + r];
}


void so_1off(void) {
	S1.on = 0;R_NR52&=~1;
}
void so_2off(void) {
	S2.on = 0;R_NR52&=~2;
}
void so_3off(void) {
	S3.on = 0;R_NR52&=~4;
}
void so_4off(void) {
	S4.on = 0;R_NR52&=~8;
}


void s1_init()
{
	if((R_NR12&0xF8) ==0) return; // envelope decoder blocks RS trigger
    //if (R_NR52&1 == 0)
	S1.pos = 0;
	R_NR52|=1;
	S1.on = 1;
	if(!S1.cnt) S1.cnt = 64;
	S1.encnt = 0;	// This value counts up
	S1.envol = R_NR12 >> 4;
	S1.swcnt = 0;
	S1.swfreq = 2047&*(unsigned short*)&R_NR13;
	if(R_NR10&7) {
		S1.swfreq+=S1.swfreq>>(R_NR10&7);
		if(S1.swfreq>2047) so_1off();
	}
}

void s2_init()
{
	if((R_NR22&0xF8) == 0) return; // envelope decoder blocks RS trigger
    //if (R_NR52&2 == 0)
	S2.pos = 0;
	R_NR52|=2;
	S2.on = 1;
	if(!S2.cnt) S2.cnt = 64;
	//S2.cnt = (64- (R_NR21&63));
	S2.encnt = 0;	// This value counts up
	S2.envol = R_NR22 >> 4;
}

void s3_init()
{
    int i;
    //if (R_NR52&4 == 0)
	S3.pos = 0;
	R_NR52|=4;
	if(!S3.cnt) S3.cnt = 256;
    //S3.cnt = (64- (R_NR31&63));
	S3.on=1;
    if (R_NR30 & 0x80) for (i = 0; i < 16; i++)
        hram[0x130+i] = 0x13 ^ hram[0x131+i];  //?
}

void s4_init()
{
	S4.on = 0;
	if((R_NR42&0xF8) == 0) return; // envelope decoder blocks RS trigger
	//if(R_NR52&8 == 0)
	S4.pos = 0;
	R_NR52|=8;
	//S4.cnt = 0;
    S4.on = 1;
	if(!S4.cnt) S4.cnt = 64;
	//S4.cnt = (64- (R_NR41&63));
	S4.encnt = 0;	// This value counts up
	S4.envol = R_NR42 >> 4;
}

void so_write(uint8_t r, uint8_t b)
{
    if (!(R_NR52 & 128) && r != RI_NR52) return;
    if ((r & 0xF0) == 0x30)
    {
        if (!(R_NR52&8 && R_NR30&0x80))
			WAVE[r-0x30] = hram[0x100+r] = b;
        return;
    }
    so_mix();
    switch (r)
    {
    case RI_NR10:
        R_NR10 = b;
        S1.swfreq = 2047&*(unsigned short*)&R_NR13;
		S1.swcnt = 0; // TODO? Is it true?
		break;
    case RI_NR11:
        R_NR11 = b;
		S1.cnt = 64-(b&63);
        break;
    case RI_NR12:
        R_NR12 = b;
        S1.envol = R_NR12 >> 4;
		if((b&0xF8) == 0) so_1off();  // Forced OFF mode(as stated in patent docs) if 0 and down
        //S1.endir = (R_NR12>>3) & 1;
        //S1.endir |= S1.endir - 1;
        break;
    case RI_NR13:
        R_NR13 = b;
        s1_freq();
        break;
    case RI_NR14:
        R_NR14 = b;
        s1_freq();
        if (b & 128) s1_init();
        break;
    case RI_NR21:
        R_NR21 = b;
		S2.cnt = 64-(b&63);
        break;
    case RI_NR22:
        R_NR22 = b;
		if((b&0xF8) == 0) so_2off();  // Forced OFF mode(as stated in patent docs) if 0 and down
        S2.envol = R_NR22 >> 4;
        //S2.endir = (R_NR22>>3) & 1;
        //S2.endir |= S2.endir - 1;
        break;
    case RI_NR23:
        R_NR23 = b;
        s2_freq();
        break;
    case RI_NR24:
        R_NR24 = b;
        s2_freq();
        if (b & 128) s2_init();
        break;
    case RI_NR30:
        R_NR30 = b;
        if (!(b & 128)) so_3off();
        break;
    case RI_NR31:
        R_NR31 = b;
		S3.cnt = 256-b;
        break;
    case RI_NR32:
        R_NR32 = b;
		
        break;
    case RI_NR33:
        R_NR33 = b;
        s3_freq();
        break;
    case RI_NR34:
        R_NR34 = b;
        s3_freq();
        if (b & 128) s3_init();
        break;
    case RI_NR41:
        R_NR41 = b;
		S4.cnt = 64-(b&63);
        break;
    case RI_NR42:
        R_NR42 = b;
        S4.envol = R_NR42 >> 4;
		if((b&0xF8) == 0) so_4off();  // Forced OFF mode(as stated in patent docs) if 0 and down
        break;
    case RI_NR43:
        R_NR43 = b;
        s4_freq();
        break;
    case RI_NR44:
        R_NR44 = b;
		s4_freq();
        if (b & 128) s4_init();
        break;
    case RI_NR50:
        R_NR50 = b;
        break;
    case RI_NR51:
        R_NR51 = b;
        break;
    case RI_NR52:
        R_NR52 = b;
        if (!(R_NR52 & 128))
            so_off();
        break;
    default:
        return;
    }
}

// **********************************************************************



/*
Inner loop does mixing with constant volume/pitch parameters,saving lots of CPU time
n is >0
code is for 32-bit machines only! code is assumed to be "PSX friendly"
*/

/*
TODO:
  this code can do redundant mixind in many cases, they must be optimized later
  for example: don't mix if both l&r outputs are disabled for a channel
  mono mixing can use some additional optimization.
*/
void so_mix_basic(unsigned long so_clk_new) {
	
    uint8_t *s1_waveptr;
    uint8_t *s2_waveptr;
	int l,r,lr[4][2];
	unsigned s;
	unsigned long clk[2];
	clk[0] = so_clk_inner[0];
	clk[1] = so_clk_inner[1];
	if(clk[1]>=so_clk_new) return;
	s1_waveptr = (uint8_t*)(sqwave+((unsigned)R_NR11>>6));
	s2_waveptr = (uint8_t*)(sqwave+((unsigned)R_NR21>>6));
	//on[0]=R_NR52&S1.on;
	//on[1]=((unsigned)R_NR52>>1)&S2.on;
	//on[2]=((unsigned)R_NR52>>2)&((unsigned)R_NR30>>7)&S3.on;
	//on[3]=((unsigned)R_NR52>>3)&S4.on;
	lr[0][0]=(((signed)R_NR51<<31)>>31) & S1.envol;
	lr[0][1]=(((signed)R_NR51<<27)>>31) & S1.envol;
	lr[1][0]=(((signed)R_NR51<<30)>>31) & S2.envol;
	lr[1][1]=(((signed)R_NR51<<26)>>31) & S2.envol;
	lr[2][0]=(((signed)R_NR51<<29)>>31);
	lr[2][1]=(((signed)R_NR51<<25)>>31);
	lr[3][0]=(((signed)R_NR51<<28)>>31) & S4.envol;
	lr[3][1]=(((signed)R_NR51<<24)>>31) & S4.envol;
	do {
		l=r=0;
		// ----------  Channel 1   ------------
		if (S1.on) {
			s = s1_waveptr[(S1.pos>>14)&7];
			S1.pos += S1.freq;
			r+=lr[0][0]&s;
			l+=lr[0][1]&s;
		}
		// ----------  Channel 2   ------------
		if (S2.on) {
			s = s2_waveptr[(S2.pos>>14)&7];
			S2.pos += S2.freq;
			r+=lr[1][0]&s;
			l+=lr[1][1]&s;
		}
		
		// ----------  Channel 3   ------------
		if (S3.on) {
			s = WAVE[(S3.pos>>18) & 0xF];
			if (S3.pos & (1<<21)) s &= 15;
			else s >>= 4;
			S3.pos += S3.freq;
			s>>=((R_NR32>>5)&3)-1;  // if 0, shift by 31 or more (mute)
			r+=lr[2][0]&s;
			l+=lr[2][1]&s;
		}
		// ----------  Channel 4   ------------
		if (S4.on) {
			if (R_NR43 & 8) s = noise7[
                (S4.pos>>20)&0xF] >> ((S4.pos>>17)&7);
            else s = noise15[
                (S4.pos>>20)&0xFFF] >> ((S4.pos>>17)&7);
            s = ((signed)s<<31)>>31;//(-(signed)(s&1));
            S4.pos += S4.freq;
			r+=lr[3][0]&s;
			l+=lr[3][1]&s;
		}
		
		
		l = l*(R_NR50 & 0x07)>>2     ;		
        r = r*((R_NR50 & 0x70)>>4)>>2;

		if(pcm.stereo)  {
			pcm.buf[pcm.pos]  =l+128;
			pcm.buf[pcm.pos+1]=r+128;
			pcm.pos+=2;
		} else {
			pcm.buf[pcm.pos]=128+((l+r)>>1);
			if(pcm.pos<pcm.len*4) pcm.pos++;
		}
		clk[0]+=RATELO;
		clk[1]+=RATEHI+(clk[0]>>16);
		clk[0]&=0xFFFF;			// 48 bit counter
	} while (clk[1]<so_clk_new);
	so_clk_inner[0] =  clk[0];
	so_clk_inner[1] =  clk[1];
}


void so_mix(void) {
	unsigned i,tmp,tmp2,swperiod,enperiod;
    uint8_t *pt;
	if(!pcm.buf) return;
	benchmark_sound-=GetTimer();
	while (so_clk_nextchange<(unsigned long)gb_clk) {
		so_mix_basic(so_clk_nextchange);
		// Change envelopes,counters/etc----------------
		// Sound length check (256 Hz)
		if(R_NR52&128) {
			if(!(so_clk_nextchange&0x1000) && (swperiod=R_NR10&0x70)) {
				S1.swcnt = (S1.swcnt+1) & 7; // counts up(with possible overflow)
				if(S1.swcnt == (swperiod>>4)) {  // Check sweep (128 Hz)
					S1.swcnt = 0;
					tmp = S1.swfreq;
					s1_freq_d(tmp);
					tmp2 = tmp>>(R_NR10&7); // sweep shift
					if (R_NR10 & 8) // count direction
						tmp -= tmp2;  // subtract freq (will never be <0)
					else {
						tmp += tmp2;  // add freq
						if(tmp>=0x800) so_1off(); // stop at sweep overflow(stated in GB faq)
					}
					S1.swfreq = (tmp &= 0x7FF);
					//*(unsigned short*)&R_NR13 = ((*(unsigned short*)&R_NR13)&~0x7FF)|tmp; // update freq (stated in GB faq)
					s1_freq_d(tmp);
					//s1_freq_d(2048 - tmp);
				}
			}
		// TODO: <=? or just =? (to emulate that bug)
		if((R_NR14&0x40) && S1.cnt)   // Counter 1
			if(--S1.cnt<=0) so_1off();
		if ((R_NR24&0x40) && S2.cnt)	// Counter 2
			if(--S2.cnt<=0) so_2off();
		if ((R_NR34&0x40) && S3.cnt)	// Counter 3
			if(--S3.cnt<=0) so_3off();
		if ((R_NR44&0x40) && S4.cnt)	// Counter 4
			if(--S4.cnt<=0) so_4off();

		if(!(so_clk_nextchange&0x3000)) {  // Check envelopes (64 Hz)
			// TODO: I think that envelope is wrapped over 15 when moving up, maybe I wrong
			for(i=0;i<4;i++) if(i!=2) {
				tmp = *(pt=(hram+0x112+i*5));//R_NR12;
				enperiod=tmp&7;
				if(snd.ch[i].on && enperiod) {
					snd.ch[i].encnt=(snd.ch[i].encnt+1)&7;
					if(snd.ch[i].encnt == enperiod) {
						snd.ch[i].encnt = 0;
						if(tmp&8) {
							tmp+=16;
							if(!(tmp & 0xF0)) {
								tmp-=16;
								//tmp&=0xF8;
							}// Volume up, decrement counter
							
						} else {
							tmp-=16;
							if((tmp & 0xF0) == 0xF0) {
								tmp+=16;
								//snd.ch[i].on=0;
								//tmp&=0xF8;
							} // Volume down, decrement counter
							//else snd.ch[i].on=0;
						}
						snd.ch[i].envol = (tmp>>4)&0xF;
						*pt = tmp;
					}
				}
			}
		}
		//----------------------------------------------
		so_clk_nextchange+=0x1000; // Warning, 14 lower buts must ALWAYS be 0
		//if(wb_free > 0) pcm_submit();
		}
	}
	so_mix_basic(gb_clk);
	benchmark_sound+=GetTimer();
	//if(wb_free > 0) pcm_submit();
}
