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
#include "types.h"
#include "libdsd.h"
#include "../dsd2pcm/dsd2pcm.h"

static u8_t halfrate_nibble[512];
static u8_t halfrate_error[512];
static u8_t *qerror;

dsdbuffer *init_halfrate(dsdbuffer *ibuffer) {

  unsigned ierr, bitpair, prev_error, index, itmp;
  int shr;
  dsdbuffer *obuffer;
  u32_t ch;

  for (ierr = 0; ierr <= 1; ierr++) {

    for (index = 0; index < 256; index++) {      
      
      itmp = 256 * ierr + index;
      halfrate_nibble[itmp] = 0;
      prev_error = ierr;

      for (shr = 6; shr >= 0; shr -= 2) {
        halfrate_nibble[itmp] <<= 1;
        bitpair = (index >> shr) & 0x03;
        if ((prev_error && bitpair == 0x03) || (!prev_error && bitpair != 0x00)) 
					halfrate_nibble[itmp] |= 1;
        if (bitpair == 0x01 || bitpair == 0x02) prev_error = 1 - prev_error;
      }
      halfrate_error[itmp] = prev_error;
    }
  }

  obuffer = (dsdbuffer *)malloc(sizeof(dsdbuffer));
  obuffer->num_channels = ibuffer->num_channels;
  obuffer->bytes_per_channel = 0;
  obuffer->max_bytes_per_ch = ibuffer->max_bytes_per_ch / 2;
  obuffer->lsb_first = 0;
  obuffer->sample_step = ibuffer->sample_step;
  obuffer->ch_step = ibuffer->ch_step;
  obuffer->data = (u8_t *)malloc(sizeof(u8_t) * obuffer->max_bytes_per_ch * obuffer->num_channels);
  qerror = (u8_t *)malloc(sizeof(u8_t) * obuffer->num_channels);
  for (ch = 0; ch < obuffer->num_channels; ch++) qerror[ch] = 0;

  return obuffer;
}

void halfrate_filter(dsdbuffer *in, dsdbuffer *out) {
  u32_t s, ch, index;
  u8_t *dsdin = in->data;
  u8_t *dsdout = out->data;
  u8_t newbyte;

  out->bytes_per_channel = in->bytes_per_channel / 2;

  for (ch = 0; ch < in->num_channels; ch++) {
    u8_t *dsdin2 = dsdin;
    u8_t *dsdout2 = dsdout;
    u8_t qe = qerror[ch];
    for (s = 0; s < in->bytes_per_channel; s += 2) {
      index = (u32_t)256 * qe + *dsdin2;
      newbyte = halfrate_nibble[index] << 4;
      qe = halfrate_error[index];
      dsdin2 += in->sample_step;

      index = (u32_t)256 * qe + *dsdin2;
      newbyte |= halfrate_nibble[index];
      qe = halfrate_error[index];
      dsdin2 += in->sample_step;
      
      *dsdout2 = newbyte;
      dsdout2 += out->sample_step;
    }
    qerror[ch] = qe;
    dsdin += in->ch_step;
    dsdout += out->ch_step;
  }
}

void dsd_over_pcm(dsdbuffer *buf, s32_t *pcmout, bool right_just) {
  u32_t s, ch;
  static u8_t dop_marker = 0x05;
  u8_t *dsdin = buf->data;

  for (s = 0; s < buf->bytes_per_channel; s += 2) {
    u8_t *dsdin2 = dsdin;
    for (ch = 0; ch < buf->num_channels; ch++) {
			*pcmout++ = right_just ? 
				dop_marker << 16 | *dsdin2 << 8 | *(dsdin2 + buf->sample_step) :
				dop_marker << 24 | *dsdin2 << 16 | *(dsdin2 + buf->sample_step) << 8;
      dsdin2 += buf->ch_step;
    }
    dsdin += 2 * buf->sample_step;
    dop_marker = (0xfa + 0x05) - dop_marker; // Switch between 0x05 and 0xfa
  }  
}

inline s32_t myround(float x) {
  return (s32_t)(x + (x>=0 ? 0.5f : -0.5f));
}

inline s32_t clip(s32_t min, s32_t value, s32_t max) {
  if (value<min) return min;
  if (value>max) return max;
  return value;
}

void dsd_to_pcm(dsdbuffer *buf, s32_t *pcmout, bool right_just) {
  u32_t s, ch;
  static dsd2pcm_ctx **dsd2pcm = NULL;
  static float *dest = NULL;

  if (!dsd2pcm) {
    dsd2pcm = (dsd2pcm_ctx **)malloc(buf->num_channels * sizeof(dsd2pcm_ctx *));
    for (ch = 0; ch < buf->num_channels; ch++)
      dsd2pcm[ch] = dsd2pcm_init();
    dest = (float *)malloc(buf->num_channels * buf->max_bytes_per_ch * sizeof(float));
  }

  for (ch = 0; ch < buf->num_channels; ch++) {
    dsd2pcm_translate(dsd2pcm[ch], buf->bytes_per_channel, 
											buf->data + ch * buf->ch_step, 
											buf->sample_step, 0, // 0 = lsb_first, bitreverse already done.
											dest + ch, buf->num_channels);
  }

  for (s = 0; s < buf->num_channels * buf->bytes_per_channel; s++) {
    float r = dest[s] * (1<<23);
    s32_t x = clip(-(1<<23), myround(r), ((1<<23)-1));
		*pcmout++ = right_just ? x : x << 8;
  }
}
