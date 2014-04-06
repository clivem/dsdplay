/*
 *  dsdplay - DSD to PCM/DoP.
 *
 *  Original code:
 *   Copyright (C) 2013 Kimmo Taskinen <www.daphile.com>
 *  
 *  Updates:
 *   Copyright (C) 2014 Adrian Smith (triode1@btinternet.com)
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

#include <stdio.h>
#include <stdlib.h>
#include <FLAC/stream_encoder.h>
#include <soxr.h>
#include "libdsd/types.h"
#include "libdsd/libdsd.h"

#ifdef __GNUC__
#define UNUSED(x) x __attribute__ ((unused))
#else
#define UNUSED(x) x
#endif

void error(char *msg) {
  fprintf(stderr, "ERROR: %s\n", msg);
  exit(1);
}

static FLAC__StreamEncoderWriteStatus write_cb(const FLAC__StreamEncoder UNUSED(*encoder), const FLAC__byte buffer[], size_t bytes, 
																							 unsigned UNUSED(samples), unsigned UNUSED(current_frame), void *client_data) {

	return (fwrite(buffer, 1, bytes, (FILE *)client_data) == bytes) ? 
		FLAC__STREAM_ENCODER_WRITE_STATUS_OK : FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
}

static void right_shift(s32_t *ptr, unsigned frames, unsigned channels) {
	unsigned count = frames * channels;
	while (count--) {
		*ptr = *ptr >> 8;
		ptr++;
	}
}

int main(int argc, char *argv[]) {
  bool dop = false, halfrate = false;
  int i;
  char *filename = NULL, *outfile = NULL;
  dsdfile *file;
  s64_t start = -1, stop = -1;
  u32_t channels, frequency, freq_limit = 0, sample_rate, max_frames;
	dsdbuffer *obuffer, *ibuffer;
	s32_t *pcmout1 = NULL, *pcmout2 = NULL;
	size_t bsize;
	FILE *ofile = NULL;
	FLAC__StreamEncoder *encoder;
	soxr_t resampler;

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
				{ 
					unsigned mins;
					float secs;
					sscanf(argv[i+1],"%u:%f", &mins, &secs);
					start = (s64_t)((secs + 60.0 * mins) * 1000.0);
				}
				break;
      case 'e':
				{
					unsigned mins;
					float secs;
					sscanf(argv[i+1],"%u:%f", &mins, &secs);
					stop = (s64_t)((secs + 60.0 * mins) * 1000.0);
				}
				break;
      case 'u':
				dop = true;
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

	if (outfile) {
		if ((ofile = fopen(outfile, "wb")) == NULL) error("could not output file!");
	} else {
		ofile = stdout;
	}

  /* 
  ** Current implementation is for DSD64 and DSD128. 
  ** Other DSD sample frequencies might work but not optimally. 
  */
	
#define DSD64 (u32_t)(64 * 44100)
#define DSD128 (u32_t)(128 * 44100)
	
#if 0
	
  if (dop && (freq_limit != 0) && (freq_limit < (frequency / 16))) {
    if ((freq_limit < (frequency / 32)) || (frequency < DSD128))
      dop = false;
    else {
      halfrate = true;
      frequency = frequency / 2;
    }
  }
	
  if (!dop && frequency > DSD64) {
    halfrate = true;
    frequency = frequency / 2;
  }
	
#else
	
  if ((freq_limit != 0) && (freq_limit < (frequency / 16))) dop = false;
	
#endif
	
	if (dop) {
		sample_rate = frequency / 16;
		freq_limit = 0;
	} else {
		sample_rate = frequency / 8;
		if (freq_limit > frequency / 8) freq_limit = 0;
	}

	ibuffer = &file->buffer;
  
	if (halfrate) {
		obuffer = init_halfrate(ibuffer);
	} else
		obuffer = ibuffer;

	max_frames = dop ? obuffer->max_bytes_per_ch / 2 : obuffer->max_bytes_per_ch;
	bsize = max_frames * channels * sizeof(s32_t);
	
	pcmout1 = (s32_t *)malloc(bsize);
	if (!pcmout1) error("unable to allocate pcm buffer 1");
  
	if (start >= 0) dsd_set_start(file, (u32_t)start);
	if (stop >= 0) dsd_set_stop(file, (u32_t)stop);

	// create FLAC encoder for output stream
	encoder = FLAC__stream_encoder_new();

	FLAC__stream_encoder_set_compression_level(encoder, 0);
	FLAC__stream_encoder_set_bits_per_sample(encoder, 24);
	FLAC__stream_encoder_set_channels(encoder, channels);
	FLAC__stream_encoder_set_sample_rate(encoder, freq_limit ? freq_limit : sample_rate);

	if (FLAC__stream_encoder_init_stream(encoder, &write_cb, NULL, NULL, NULL, (void *)ofile) != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
		error("unable to create flac encoder");
	}

	// create resampler for output stream if need to limit output frequency
	if (freq_limit) {
		soxr_io_spec_t io_spec = soxr_io_spec(SOXR_INT32_I, SOXR_INT32_I);
		soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_HQ, 0);
		soxr_error_t err;
		io_spec.scale = 0.891251; // -1dB attenuation

		resampler = soxr_create(sample_rate, freq_limit, channels, &err, &io_spec, &q_spec, NULL);
		if (err) error("error creating resampler");

		pcmout2 = (s32_t *)malloc(bsize);
		if (!pcmout2) error("unable to allocate pcm buffer 2");
	}

	// read and decode file
	while ((ibuffer = dsd_read(file))) {
		u32_t frames;
		bool flac_ok = true;

		dsd_buffer_msb_order(ibuffer);
		
		if (halfrate) halfrate_filter(ibuffer, obuffer);
		
		// process dsd into pcmout1 - right justifying samples if we will send direct to flac
		if (dop) {
			dsd_over_pcm(obuffer, pcmout1, !freq_limit);
			frames = ibuffer->bytes_per_channel / 2;
		} else {
			dsd_to_pcm(obuffer, pcmout1, !freq_limit); // DSD64 to 352.8kHz PCM
			frames = ibuffer->bytes_per_channel;
		}
	
		if (freq_limit) {
			// resample and encode
			size_t idone, odone;
			soxr_process(resampler, pcmout1, frames, &idone, pcmout2, max_frames, &odone);
 			right_shift(pcmout2, odone, channels); // shift samples for flac
			flac_ok = FLAC__stream_encoder_process_interleaved(encoder, (FLAC__int32 *)pcmout2, odone);
			if (idone != frames) error("not resampled all frames");
		} else {
			// just encode
			flac_ok = FLAC__stream_encoder_process_interleaved(encoder, (FLAC__int32 *)pcmout1, frames);
		}

		if (!flac_ok) break;
	}

	// drain resampler
	if (freq_limit) {
		size_t odone;
		bool flac_ok = true;
		do {
			soxr_process(resampler, NULL, 0, NULL, pcmout2, max_frames, &odone);
 			right_shift(pcmout2, odone, channels); // shift samples for flac
			flac_ok = FLAC__stream_encoder_process_interleaved(encoder, (FLAC__int32 *)pcmout2, odone);
		} while (odone && flac_ok);
	}

	// drain encoder
	FLAC__stream_encoder_finish(encoder);

  return 0;
}
