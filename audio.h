
/*------------------------------------------------------------------
 *
 * Module:      audio.h
 *
 * Purpose:   	Interface to audio device commonly called a "sound card"
 *		for historical reasons.
 *		
 *---------------------------------------------------------------*/


#ifndef AUDIO_H
#define AUDIO_H 1


struct audio_s {


	    char adevice_out[80];	/* Name of the audio output device (or file?). */

	    int num_channels;		/* Should be 1 for mono or 2 for stereo. */
	    int samples_per_sec;	/* Audio sampling rate.  Typically 11025, 22050, or 44100. */
	    int bits_per_sample;	/* 8 (unsigned char) or 16 (signed short). */

};



int audio_open (struct audio_s *pa);


int audio_put (int a, int c);

int audio_flush (int a);

void audio_wait (int a);

int audio_close (void);


#endif  /* ifdef AUDIO_H */


/* end audio.h */

