#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <assert.h>

#include <alsa/asoundlib.h>

#include "audio.h"


/* Audio configuration. */

static struct audio_s          *save_audio_config_p;


/* Current state for the audio device. */

static struct adev_s {

	snd_pcm_t *audio_out_handle;

	int bytes_per_frame;		/* number of bytes for a sample from all channels. */
					/* e.g. 4 for stereo 16 bit. */

	int outbuf_size_in_bytes;
	unsigned char *outbuf_ptr;
	int outbuf_len;

} adev;


#define ONE_BUF_TIME 10


static int set_alsa_params (snd_pcm_t *handle, struct audio_s *pa, char *name, char *dir);


#define roundup1k(n) (((n) + 0x3ff) & ~0x3ff)

static int calcbufsize(int rate, int chans, int bits)
{
	int size1 = (rate * chans * bits  / 8 * ONE_BUF_TIME) / 1000;
	int size2 = roundup1k(size1);
#if DEBUG
	printf ("audio_open: calcbufsize (rate=%d, chans=%d, bits=%d) calc size=%d, round up to %d\n",
		rate, chans, bits, size1, size2);
#endif
	return (size2);
}


/*------------------------------------------------------------------
 *
 * Name:        audio_open
 *              
 * Returns:     0 for success, -1 for failure.
 *	
 *----------------------------------------------------------------*/

int audio_open (struct audio_s *pa)
{
	int err;
	char audio_out_name[30];


	save_audio_config_p = pa;

	memset (&adev, 0, sizeof(adev));


/*
 * Open audio output device.
 */


	    adev.outbuf_size_in_bytes = 0;
	    adev.outbuf_ptr = NULL;
	    adev.outbuf_len = 0;


	    strcpy (audio_out_name, pa->adevice_out);


              printf ("Audio out device: %s\n", audio_out_name);

/*
 * Now attempt actual open.
 */

	    err = snd_pcm_open (&(adev.audio_out_handle), audio_out_name, SND_PCM_STREAM_PLAYBACK, 0);

	    if (err < 0) {
	      printf ("Could not open audio device %s for output\n%s\n", 
			audio_out_name, snd_strerror(err));
	      return (-1);
	    }

	    adev.outbuf_size_in_bytes = set_alsa_params (adev.audio_out_handle, pa, audio_out_name, "output");

	    if (adev.outbuf_size_in_bytes <= 0) {
	      return (-1);
	    }


/*
 * Allocate buffer for data transfer.
 */
	    adev.outbuf_ptr = malloc(adev.outbuf_size_in_bytes);
	    assert (adev.outbuf_ptr  != NULL);
	    adev.outbuf_len = 0;

	
	return (0);

} /* end audio_open */





/*
 * Set parameters for sound card.
 *
 */
/* 
 * Terminology:
 *   Sample	- for one channel.		e.g. 2 bytes for 16 bit.
 *   Frame	- one sample for all channels.  e.g. 4 bytes for 16 bit stereo
 *   Period	- size of one transfer.
 */

static int set_alsa_params (snd_pcm_t *handle, struct audio_s *pa, char *devname, char *inout)
{
	
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_uframes_t fpp; 			/* Frames per period. */

	unsigned int val;

	int dir;
	int err;

	int buf_size_in_bytes;			/* result, number of bytes per transfer. */


	err = snd_pcm_hw_params_malloc (&hw_params);
	if (err < 0) {
	  printf ("Could not alloc hw param structure.\n%s\n", 
			snd_strerror(err));
	  printf ("for %s %s.\n", devname, inout);
	  return (-1);
	}

	err = snd_pcm_hw_params_any (handle, hw_params);
	if (err < 0) {
	  printf ("Could not init hw param structure.\n%s\n", 
			snd_strerror(err));
	  printf ("for %s %s.\n", devname, inout);
	  return (-1);
	}

	/* Interleaved data: L, R, L, R, ... */

	err = snd_pcm_hw_params_set_access (handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);

	if (err < 0) {
	  printf ("Could not set interleaved mode.\n%s\n", 
			snd_strerror(err));
	  printf ("for %s %s.\n", devname, inout);
	  return (-1);
	}

	/* Signed 16 bit little endian or unsigned 8 bit. */


	err = snd_pcm_hw_params_set_format (handle, hw_params,
 		pa->bits_per_sample == 8 ? SND_PCM_FORMAT_U8 : SND_PCM_FORMAT_S16_LE);
	if (err < 0) {
	  printf ("Could not set bits per sample.\n%s\n", 
			snd_strerror(err));
	  printf ("for %s %s.\n", devname, inout);
	  return (-1);
	}

	/* Number of audio channels. */


	err = snd_pcm_hw_params_set_channels (handle, hw_params, pa->num_channels);
	if (err < 0) {
	  printf ("Could not set number of audio channels.\n%s\n", 
			snd_strerror(err));
	  printf ("for %s %s.\n", devname, inout);
	  return (-1);
	}

	/* Audio sample rate. */


	val = pa->samples_per_sec;

	dir = 0;


	err = snd_pcm_hw_params_set_rate_near (handle, hw_params, &val, &dir);
	if (err < 0) {
	  printf ("Could not set audio sample rate.\n%s\n", 
			snd_strerror(err));
	  printf ("for %s %s.\n", devname, inout);
	  return (-1);
	}

	if (val != pa->samples_per_sec) {

	  printf ("Asked for %d samples/sec but got %d.\n",

				pa->samples_per_sec, val);
	  printf ("for %s %s.\n", devname, inout);

	  pa->samples_per_sec = val;

	}
	
	/* Original: */
	/* Guessed around 20 reads/sec might be good. */
	/* Period too long = too much latency. */
	/* Period too short = more overhead of many small transfers. */

	/* fpp = pa->samples_per_sec / 20; */

	/* The suggested period size was 2205 frames.  */
	/* I thought the later "...set_period_size_near" might adjust it to be */
	/* some more optimal nearby value based hardware buffer sizes but */
	/* that didn't happen.   We ended up with a buffer size of 4410 bytes. */

	/* In version 1.2, let's take a different approach. */
	/* Reduce the latency and round up to a multiple of 1 Kbyte. */
	
	/* For the typical case of 44100 sample rate, 1 channel, 16 bits, we calculate */
	/* a buffer size of 882 and round it up to 1k.  This results in 512 frames per period. */
	/* A period comes out to be about 80 periods per second or about 12.5 mSec each. */

	buf_size_in_bytes = calcbufsize(pa->samples_per_sec, pa->num_channels, pa->bits_per_sample);

//#if __arm__
//	/* Ugly hack for RPi. */
//	/* Reducing buffer size is fine for input but not so good for output. */
//	/* I don't recall why this was done. */
//	/* Doesn't make a difference now, so it probabaly doesn't matter.
//	
//	if (*inout == 'o') {
//	  buf_size_in_bytes = buf_size_in_bytes * 4;
//	}
//#endif

	fpp = buf_size_in_bytes / (pa->num_channels * pa->bits_per_sample / 8);

#if DEBUG

	printf ("suggest period size of %d frames\n", (int)fpp);
#endif
	dir = 0;
	err = snd_pcm_hw_params_set_period_size_near (handle, hw_params, &fpp, &dir);

	if (err < 0) {
	  printf ("Could not set period size\n%s\n", snd_strerror(err));
	  printf ("for %s %s.\n", devname, inout);
	  return (-1);
	}

	err = snd_pcm_hw_params (handle, hw_params);
	if (err < 0) {
	  printf ("Could not set hw params\n%s\n", snd_strerror(err));
	  printf ("for %s %s.\n", devname, inout);
	  return (-1);
	}

	/* Driver might not like our suggested period size */
	/* and might have another idea. */

	err = snd_pcm_hw_params_get_period_size (hw_params, &fpp, NULL);
	if (err < 0) {
	  printf ("Could not get audio period size.\n%s\n", snd_strerror(err));
	  printf ("for %s %s.\n", devname, inout);
	  return (-1);
	}

	snd_pcm_hw_params_free (hw_params);
	
	/* A "frame" is one sample for all channels. */

	/* The read and write use units of frames, not bytes. */

	adev.bytes_per_frame = snd_pcm_frames_to_bytes (handle, 1);

	assert (adev.bytes_per_frame == pa->num_channels * pa->bits_per_sample / 8);

	buf_size_in_bytes = fpp * adev.bytes_per_frame;

#if DEBUG
	printf ("audio buffer size = %d (bytes per frame) x %d (frames per period) = %d \n", adev.bytes_per_frame, (int)fpp, buf_size_in_bytes);
#endif

	/* Version 1.3 - after a report of this situation for Mac OSX version. */
	if (buf_size_in_bytes < 256 || buf_size_in_bytes > 32768) {
	  printf ("Audio buffer has unexpected extreme size of %d bytes.\n", buf_size_in_bytes);
	  printf ("Detected at %s, line %d.\n", __FILE__, __LINE__);
	  printf ("This might be caused by unusual audio device configuration values.\n"); 
	  buf_size_in_bytes = 2048;
	  printf ("Using %d to attempt recovery.\n", buf_size_in_bytes);
	}

	return (buf_size_in_bytes);


} /* end alsa_set_params */



/*------------------------------------------------------------------
 *
 * Name:        audio_put
 *
 * Purpose:     Send one byte to the audio device.
 *
 * Inputs:	a	- Number of audio device.
 *
 *		c	- One byte in range of 0 - 255.
 *
 * Returns:     Normally non-negative.
 *              -1 for any type of error.
 *
 * Description:	The caller must deal with the details of mono/stereo
 *		and number of bytes per sample.
 *
 * See Also:	audio_flush
 *		audio_wait
 *
 *----------------------------------------------------------------*/

int audio_put (int a, int c)
{
	/* Should never be full at this point. */
	assert (adev.outbuf_len < adev.outbuf_size_in_bytes);

	adev.outbuf_ptr[adev.outbuf_len++] = c;

	if (adev.outbuf_len == adev.outbuf_size_in_bytes) {
	  return (audio_flush(a));
	}

	return (0);

} /* end audio_put */


/*------------------------------------------------------------------
 *
 * Name:        audio_flush
 *
 * Purpose:     Push out any partially filled output buffer.
 *
 * Returns:     Normally non-negative.
 *              -1 for any type of error.
 *
 * See Also:	audio_flush
 *		audio_wait
 *
 *----------------------------------------------------------------*/

int audio_flush (int a)
{
	int k;
	unsigned char *psound;
	int retries = 10;
	snd_pcm_status_t *status;

	assert (adev.audio_out_handle != NULL);


/*
 * Trying to set the automatic start threshold didn't have the desired
 * effect.  After the first transmitted packet, they are saved up
 * for a few minutes and then all come out together.
 *
 * "Prepare" it if not already in the running state.
 * We stop it at the end of each transmitted packet.
 */


	snd_pcm_status_alloca(&status);

	k = snd_pcm_status (adev.audio_out_handle, status);
	if (k != 0) {
	  printf ("Audio output get status error.\n%s\n", snd_strerror(k));
	}

	if ((k = snd_pcm_status_get_state(status)) != SND_PCM_STATE_RUNNING) {

// FIXME remove debug.

	  //printf ("Audio output state = %d.  Try to start.\n", k);
	  //double b4 = dtime_now();

	  k = snd_pcm_prepare (adev.audio_out_handle);

	  //printf ("snd_pcm_prepare took %.3f seconds.\n", dtime_now() - b4);

	  if (k != 0) {
	    printf ("Audio output start error.\n%s\n", snd_strerror(k));
	  }
	}


	psound = adev.outbuf_ptr;

	while (retries-- > 0) {

	  k = snd_pcm_writei (adev.audio_out_handle, psound, adev.outbuf_len / adev.bytes_per_frame);	
#if DEBUGx
	  printf ("audio_flush(): snd_pcm_writei %d frames returns %d\n",
				adev.outbuf_len / adev.bytes_per_frame, k);
	  fflush (stdout);	
#endif
	  if (k == -EPIPE) {
	    printf ("Audio output data underrun.\n");

	    /* No problemo.  Recover and go around again. */

	    snd_pcm_recover (adev.audio_out_handle, k, 1);
	  }
          else if (k == -ESTRPIPE) {
            printf ("Driver suspended, recovering\n");
            snd_pcm_recover(adev.audio_out_handle, k, 1);
          }
          else if (k == -EBADFD) {
            k = snd_pcm_prepare (adev.audio_out_handle);
            if(k < 0) {
              printf ("Error preparing after bad state: %s\n", snd_strerror(k));
            }
          }
 	  else if (k < 0) {
	    printf ("Audio write error: %s\n", snd_strerror(k));

	    /* Some other error condition. */
	    /* Try again. What do we have to lose? */

            k = snd_pcm_prepare (adev.audio_out_handle);
            if(k < 0) {
              printf ("Error preparing after error: %s\n", snd_strerror(k));
            }
	  }
 	  else if (k != adev.outbuf_len / adev.bytes_per_frame) {
	    printf ("Audio write took %d frames rather than %d.\n",
 			k, adev.outbuf_len / adev.bytes_per_frame);
	
	    /* Go around again with the rest of it. */

	    psound += k * adev.bytes_per_frame;
	    adev.outbuf_len -= k * adev.bytes_per_frame;
	  }
	  else {
	    /* Success! */
	    adev.outbuf_len = 0;
	    return (0);
	  }
	}

	printf ("Audio write error retry count exceeded.\n");

	adev.outbuf_len = 0;
	return (-1);

} /* end audio_flush */


/*------------------------------------------------------------------
 *
 * Name:        audio_wait
 *
 * Purpose:	Finish up audio output before turning transmitter off.
 *
 * Inputs:	a		- Index for audio device (not channel!)
 *
 * Returns:     None.
 *
 * Description:	Flush out any partially filled audio output buffer.
 *		Wait until all the queued up audio out has been played.
 *		Take any other necessary actions to stop audio output.
 *
 *----------------------------------------------------------------*/

void audio_wait (int a)
{	

	audio_flush (a);


	/* For playback, this should wait for all pending frames */
	/* to be played and then stop. */

	snd_pcm_drain (adev.audio_out_handle);

	/*
	 * When this was first implemented, I observed:
 	 *
 	 * 	"Experimentation reveals that snd_pcm_drain doesn't
	 * 	 actually wait.  It returns immediately. 
	 * 	 However it does serve a useful purpose of stopping
	 * 	 the playback after all the queued up data is used."
 	 *
	 *
	 * Now that I take a closer look at the transmit timing, for
 	 * version 1.2, it seems that snd_pcm_drain DOES wait until all
	 * all pending frames have been played.  
 	 */

#if DEBUG
	printf ("audio_wait(): after sync, status=%d\n", err);	
#endif

} /* end audio_wait */


/*------------------------------------------------------------------
 *
 * Name:        audio_close
 *
 * Purpose:     Close the audio device(s).
 *
 * Returns:     Normally non-negative.
 *              -1 for any type of error.
 *
 *
 *----------------------------------------------------------------*/

int audio_close (void)
{
	int err = 0;


	  if (adev.audio_out_handle != NULL) {

	    audio_wait (0);

	    snd_pcm_close (adev.audio_out_handle);
	
	    free (adev.outbuf_ptr);

	    adev.outbuf_size_in_bytes = 0;
	    adev.outbuf_ptr = NULL;
	    adev.outbuf_len = 0;
	  }

	return (err);

} /* end audio_close */


/* end audio.c */

