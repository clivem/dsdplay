/*
 *  dsdplay - DSD to PCM/DoP.
 *
 *  Copyright (C) 2013 Kimmo Taskinen <www.daphile.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include <glib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdbool.h>
#include "libdsd/libdsd.h"

void error(char *msg) {
  fprintf(stderr, "ERROR: %s\n", msg);
  exit(1);
}

int main(int argc, char *argv[]) {
  pid_t pid;
  bool dop = FALSE, halfrate = FALSE;
  int i, commpipe[2];
  char *filename = NULL, *outfile = "-";
  dsdfile *file;
  guint64 size = 0; 
  gint64 start = -1, stop = -1;
  guint32 channels, frequency, freq_limit = 0, mins;
  float secs;

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
      case 'o':
	outfile = argv[i+1];
	break;
      case 'r':
	freq_limit = atol(argv[i+1]);
	break;
      case 's':
	sscanf(argv[i+1],"%u:%f", &mins, &secs);
	start = (gint64)((secs + 60.0 * mins) * 1000.0);
	break;
      case 'e':
	sscanf(argv[i+1],"%u:%f", &mins, &secs);
	stop = (gint64)((secs + 60.0 * mins) * 1000.0);
	break;
      case 'u':
	dop = TRUE;
	i--;
	break;
      default:
	error("Unknown option!");
      }
      i++;
    } else {
      filename = argv[i];
    }
  }

  if ((file = dsd_open(filename)) == NULL) error("could not open file!");

  frequency = dsd_sample_frequency(file);
  channels = dsd_channels(file);

  /* 
  ** Current implementation is for DSD64 and DSD128. 
  ** Other DSD sample frequencies might work but not optimally. 
  */

#define DSD64 (guint32)(64 * 44100)
#define DSD128 (guint32)(128 * 44100)

#if 0

  if (dop && (freq_limit != 0) && (freq_limit < (frequency / 16))) {
    if ((freq_limit < (frequency / 32)) || (frequency < DSD128))
      dop = FALSE;
    else {
      halfrate = TRUE;
      frequency = frequency / 2;
    }
  }

  if (!dop && frequency > DSD64) {
    halfrate = TRUE;
    frequency = frequency / 2;
  }

#else

  if ((freq_limit != 0) && (freq_limit < (frequency / 16))) dop = FALSE;

#endif

  if (pipe(commpipe)) error("Pipe error!");
  if ((pid = fork()) == -1) error("Fork error!");

  if (pid) {
    dsdbuffer *obuffer, *ibuffer;
    guchar *pcmout;
    gsize bsize;

    dup2(commpipe[1],1);
    close(commpipe[0]);

    ibuffer = &file->buffer;
    
    if (halfrate) {
      obuffer = init_halfrate(ibuffer);
    } else
      obuffer = ibuffer;

    if (dop) {
      bsize = obuffer->num_channels * obuffer->max_bytes_per_ch / 2 * sizeof(guchar) * 3;
    } else {
      bsize = obuffer->num_channels * obuffer->max_bytes_per_ch * sizeof(guchar) * 3;
    }
    pcmout = (guchar *)malloc(bsize);
    
    if (start >= 0) dsd_set_start(file, start);
    if (stop >= 0) dsd_set_stop(file, stop);
    
    while ((ibuffer = dsd_read(file))) {

      size += ibuffer->bytes_per_channel;

      dsd_buffer_msb_order(ibuffer);

      if (halfrate) halfrate_filter(ibuffer, obuffer);

      if (dop)
	dsd_over_pcm(obuffer, pcmout);
      else
	dsd_to_pcm(obuffer, pcmout); // DSD64 to 352.8kHz PCM

      if (fwrite(pcmout, 1, bsize, stdout) != bsize) error("write error");
    }
    
    close(commpipe[1]);
    
    if (!dsd_eof(file)) error("file read error - EOF was expected!");
    if (!dsd_close(file)) error("failed to close!");

  } else {
    /* 
    ** Sox fork. Sox will:
    ** - convert raw pcm to flac 
    ** - take care of possible rate conversion for PCM converted 352.8kHz output
    */
    char freq[16], ch[16], fout[16];

    dup2(commpipe[0], 0);
    close(commpipe[1]);

    if (dop) {
      sprintf(freq,"%u", frequency / 16);
      freq_limit = 0;
    } else {
      sprintf(freq,"%u", frequency / 8);
      if (freq_limit > frequency / 8) freq_limit = 0;
    }

    sprintf(ch,"%u",channels);

    if (freq_limit == 0) {
      if (execl("/usr/bin/sox", "sox", "-t", "raw", "-c", ch, "-r", freq,
		"-e", "signed", "-b", "24", "-", "-t", "flac", "-b", "24",
		"-C", "0", outfile, NULL) == -1) {
	fprintf(stderr, "execl Error!");
	exit(1);
      }
    } else {
      sprintf(fout, "%u", freq_limit);
      if (execl("/usr/bin/sox", "sox", "-t", "raw", "-c", ch, "-r", freq,
		"-e", "signed", "-b", "24", "-", "-t", "flac", "-b", "24",
		"-C", "0", "-r", fout, outfile, NULL) == -1) {
	fprintf(stderr, "execl Error!");
	exit(1);
      }
    }
  }
  return 0;
}
