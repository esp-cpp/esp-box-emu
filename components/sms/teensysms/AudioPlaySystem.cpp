extern "C" {
#include "emuapi.h"  
}

#include "AudioPlaySystem.h"
#include "esp_system.h"

#include "driver/i2s.h"
#include "freertos/queue.h"
#include "string.h"
#define I2S_NUM         ((i2s_port_t)0)

volatile uint16_t *Buffer;
volatile uint16_t BufferSize;


static const short square[]={
32767,32767,32767,32767,
32767,32767,32767,32767,
32767,32767,32767,32767,
32767,32767,32767,32767,
32767,32767,32767,32767,
32767,32767,32767,32767,
32767,32767,32767,32767,
32767,32767,32767,32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,
};

const short noise[] {
-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,
-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,-32767,32767,-32767,
-32767,-32767,32767,-32767,-32767,-32767,32767,-32767,-32767,-32767,32767,-32767,-32767,-32767,32767,-32767,
-32767,-32767,32767,-32767,-32767,-32767,32767,-32767,-32767,-32767,32767,-32767,-32767,32767,32767,-32767,
-32767,-32767,32767,-32767,-32767,32767,32767,-32767,-32767,-32767,32767,-32767,-32767,32767,32767,-32767,
-32767,-32767,32767,-32767,-32767,32767,32767,-32767,-32767,-32767,32767,-32767,32767,32767,32767,-32767,
32767,-32767,32767,-32767,-32767,32767,32767,-32767,-32767,-32767,32767,-32767,32767,32767,32767,-32767,
32767,-32767,32767,-32767,-32767,32767,32767,-32767,-32767,-32767,32767,32767,32767,32767,32767,-32767,
32767,-32767,32767,-32767,-32767,32767,32767,-32767,-32767,-32767,32767,32767,32767,32767,32767,-32767,
32767,-32767,32767,-32767,-32767,32767,32767,-32767,-32767,-32767,-32767,32767,32767,32767,-32767,-32767,
32767,-32767,-32767,-32767,-32767,32767,-32767,-32767,-32767,-32767,32767,32767,32767,32767,32767,-32767,
32767,-32767,32767,-32767,-32767,32767,32767,-32767,-32767,32767,-32767,32767,32767,32767,-32767,-32767,
32767,32767,-32767,-32767,-32767,32767,-32767,-32767,-32767,-32767,32767,32767,32767,32767,32767,-32767,
32767,-32767,32767,-32767,-32767,32767,32767,-32767,32767,32767,-32767,32767,-32767,32767,-32767,-32767,
32767,32767,-32767,-32767,-32767,32767,-32767,-32767,-32767,-32767,32767,32767,32767,32767,32767,-32767,
32767,-32767,32767,-32767,-32767,32767,32767,32767,32767,32767,-32767,32767,-32767,32767,-32767,-32767,
};

#define NOISEBSIZE 0x100

typedef struct
{
  unsigned int spos;
  unsigned int sinc;
  unsigned int vol;
} Channel;

volatile bool playing = false;


static Channel chan[6] = {
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0},
  {0,0,0} };


static void snd_Reset(void)
{
  chan[0].vol = 0;
  chan[1].vol = 0;
  chan[2].vol = 0;
  chan[3].vol = 0;
  chan[4].vol = 0;
  chan[5].vol = 0;
  chan[0].sinc = 0;
  chan[1].sinc = 0;
  chan[2].sinc = 0;
  chan[3].sinc = 0;
  chan[4].sinc = 0;
  chan[5].sinc = 0;
}

#ifdef CUSTOM_SND 
extern "C" {
void SND_Process(void *sndbuffer, int sndn);
}
#endif


static void snd_Mixer16(uint16_t *  stream, int len )
{
  if (playing) 
  {
#ifdef CUSTOM_SND 
    SND_Process((void*)stream, len);
#else
    int i;
    long s;  
    //len = len >> 1; 
     
    short v0=chan[0].vol;
    short v1=chan[1].vol;
    short v2=chan[2].vol;
    short v3=chan[3].vol;
    short v4=chan[4].vol;
    short v5=chan[5].vol;
    for (i=0;i<len;i++)
    {
      s = ( v0*(square[(chan[0].spos>>8)&0x3f]) );
      s+= ( v1*(square[(chan[1].spos>>8)&0x3f]) );
      s+= ( v2*(square[(chan[2].spos>>8)&0x3f]) );
      s+= ( v3*(noise[(chan[3].spos>>8)&(NOISEBSIZE-1)]) );
      s+= ( v4*(noise[(chan[4].spos>>8)&(NOISEBSIZE-1)]) );
      s+= ( v5*(noise[(chan[5].spos>>8)&(NOISEBSIZE-1)]) );         
      *stream++ = int16_t((s>>11));      
      chan[0].spos += chan[0].sinc;
      chan[1].spos += chan[1].sinc;
      chan[2].spos += chan[2].sinc;
      chan[3].spos += chan[3].sinc;  
      chan[4].spos += chan[4].sinc;  
      chan[5].spos += chan[5].sinc;  
    }
#endif         
  }
}

void AudioPlaySystem::begin(void)
{
}

void AudioPlaySystem::start(void)
{ 
  playing = true;  
}

void AudioPlaySystem::setSampleParameters(float clockfreq, float samplerate) {
}

void AudioPlaySystem::reset(void)
{
	snd_Reset();
}

void AudioPlaySystem::stop(void)
{
	playing = false;
}

bool AudioPlaySystem::isPlaying(void) 
{ 
  return playing; 
}


void AudioPlaySystem::sound(int C, int F, int V) {
  if (C < 6) {
    //printf("play %d %d %d\n",C,F,V);  

    chan[C].vol = V;
    chan[C].sinc = F>>1; 
  }
}

void AudioPlaySystem::step(void) 
{
  if (playing) {
    int left=DEFAULT_SAMPLERATE/50;

    while(left) {
      int n=DEFAULT_SAMPLESIZE;
      if (n>left) n=left;
      snd_Mixer16((uint16_t*)Buffer, n);
      //16 bit mono -> 16 bit r+l
      for (int i=n-1; i>=0; i--) {
        Buffer[i*2+1]=Buffer[i]+32767;
        Buffer[i*2]=Buffer[i]+32767;
      }
      // TODO: update this to use hal
      // i2s_write_bytes(I2S_NUM, (const void*)Buffer, n*4, portMAX_DELAY);
      left-=n;
    }
  }
}
