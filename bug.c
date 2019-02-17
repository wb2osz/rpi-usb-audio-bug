

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <time.h>
#include <sys/time.h>		// for  gettimeofday()


#include "audio.h"

static struct audio_s audio_config;


int main (int argc, char *argv[])
{

        struct timeval tv;
	int card_number;
	int error_count = 0;
	int start_sec;


	if (argc == 2) {
	  card_number = atoi(argv[1]);
	}
	else {
	  fprintf (stderr, "Specify card number (like 0 or 1) on the command line.\n");
	  fprintf (stderr, "Use \"aplay -l\" to get a list of the card numbers.\n");
	  exit (EXIT_FAILURE);
	}

	setlinebuf (stdout);

	memset (&audio_config, 0, sizeof(struct audio_s));

// Select audio output device.

	sprintf (audio_config.adevice_out, "plughw:%d,0", card_number );

	audio_config.num_channels = 1;
	audio_config.samples_per_sec = 44100;
	//audio_config.samples_per_sec = 48000;  // Same result.
	audio_config.bits_per_sample = 16;



	int err = audio_open (&audio_config);
	if (err < 0) {
	  printf ("Pointless to continue without audio device.\n");
	  exit (EXIT_FAILURE);
	}

	int spacing;
	int repeat;

	for (spacing = 8; spacing < 16 ; spacing++) { 

	printf ("\n ------ spacing of %d seconds ------ \n\n", spacing);

	for (repeat = 0; repeat < 10; repeat ++ ) {


// Wait until N second boundary.

	  do {
	    usleep (100000);
            gettimeofday (&tv, NULL);
	  } while ((tv.tv_sec % spacing) != 0);

// Wait until second boundary.

	  do {
	    usleep (500);
            gettimeofday (&tv, NULL);
	  } while (tv.tv_usec <= 900000);
	  do {
	    usleep (500);
            gettimeofday (&tv, NULL);
	  } while (tv.tv_usec >= 900000);

// Send 1/4 sec of 700 Hz.

	  start_sec = (int)tv.tv_sec;
	  printf ("%d\n", (int)tv.tv_usec / 1000);
	  int k;
	  int f = 700;
	  for (k = 0; k < audio_config.samples_per_sec / 4; k++) {

	    int n = (int)(32000 * sin (k * 2 * M_PI * f / audio_config.samples_per_sec)); 
	    audio_put (0, n & 0xff);
	    audio_put (0, (n >> 8) & 0xff);
	  }
	  audio_put (0, 0);
	  audio_put (0, 0);
	  audio_flush (0);
          audio_wait (0);

// We expect 1/4 second elapsed time since the start of the tone.
// (Or more accurately, from the time we told the operating system to start producing audio.)
// As we demonstrate, here, there is often a delay from when we tell the operating system
// to produce sound and when it actually starts coming out.

          gettimeofday (&tv, NULL);

	  int elapsed = (tv.tv_sec - start_sec) * 1000 + (int)tv.tv_usec / 1000;
	  printf ("        %d\n", elapsed);
	  if (elapsed > 300) {
	    printf ("                Delayed extra %d mSec\n", elapsed - 250);
	    error_count++;
	  }
	  if (elapsed < 200) {
	    printf ("                Too short!\n");
	    error_count++;
	  }
	}
	}

	if (error_count) {

	  printf ("*******************  TEST FAILED  ********************\n");
	  exit (EXIT_FAILURE);
	}

	printf ("Success.\n");
	exit (EXIT_SUCCESS);
}

