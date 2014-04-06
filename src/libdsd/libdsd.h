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

#include "types.h"

typedef struct {
  u32_t format_version;
  u32_t format_id;
  u32_t channel_type;
  u32_t bits_per_sample;
  u32_t block_size_per_channel;
} dsfinfo;

typedef enum { DSF, DSDIFF } dsdtype;

typedef struct {
  u8_t num_channels;
  u32_t bytes_per_channel;   // number of valid bytes (not size of array)
  u32_t max_bytes_per_ch;
  bool lsb_first;
  unsigned sample_step;
  unsigned ch_step;

  u8_t *data;
} dsdbuffer;

typedef struct {
  FILE *stream;                // init @ dsd_open
  bool canseek;                // init @ dsd_open
  bool eof;                    // init @ dsd_open
  u64_t offset;                // init @ dsd_open
  dsdtype type;                // init @ dsd_open

  u64_t file_size;             // init @ dsf_init or dsdiff_init

  u32_t channel_num;           // init 0 0 dsd_open, update @ dsf_init or dsdiff:parse_prop_chunk, check not 0
  u32_t sampling_frequency;    // init 0 0 dsd_open, update @ dsf_init or dsdiff:parse_prop_chunk, check not 0

  u64_t sample_offset;         // init 0 @ dsd_open, set X @ dsd_set_start
  u64_t sample_count;          // init @ dsf_init or dsdiff_init
  u64_t sample_stop;           // init @ dsf_init or dsdiff_init, set X @ dsd_set_stop

  dsfinfo dsf;                 // init @ dsf_init

  u64_t dataoffset;            // init @ dsf_init or dsdiff_init
  u64_t datasize;              // init @ dsf_init or dsdiff_init
  dsdbuffer buffer;            // 

} dsdfile;

extern const u8_t bit_reverse_table[];
static inline void dsd_buffer_msb_order(dsdbuffer *ibuffer) {
  u32_t s;
  u8_t *ptr;
  if (ibuffer->lsb_first) {		  
    ptr = ibuffer->data;
    for (s = ibuffer->max_bytes_per_ch * ibuffer->num_channels; s > 0; s--, ptr++)
      *ptr = bit_reverse_table[(u8_t)*ptr];
  }
}

dsdfile *dsd_open(const char *name);
bool dsd_close(dsdfile *file);
bool dsd_eof(dsdfile *file);
bool dsd_set_start(dsdfile *file, u32_t mseconds);
bool dsd_set_stop(dsdfile *file, u32_t mseconds);
u32_t dsd_sample_frequency(dsdfile *file);
u32_t dsd_channels(dsdfile *file);
dsdbuffer *dsd_read(dsdfile *file);
dsdbuffer *init_halfrate(dsdbuffer *ibuffer);
void halfrate_filter(dsdbuffer *in, dsdbuffer *out);
void dsd_over_pcm(dsdbuffer *buf, s32_t *pcmout, bool right_just);
void dsd_to_pcm(dsdbuffer *buf, s32_t *pcmout, bool right_just);
