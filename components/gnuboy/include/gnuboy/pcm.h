
#ifndef __PCM_H__
#define __PCM_H__


#include "gnuboy/defs.h"
#include <stdint.h>

struct pcm
{
	int hz, len;
	int stereo;
	int16_t* buf;
	int pos;
};

extern struct pcm pcm;


#endif
