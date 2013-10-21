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
typedef struct {
  guint32 format_version;
  guint32 format_id;
  guint32 channel_type;
  guint32 bits_per_sample;
  guint32 block_size_per_channel;
} dsfinfo;

typedef enum { DSF, DSDIFF } dsdtype;

typedef struct {
  guint8 num_channels;
  guint32 bytes_per_channel;   // number of valid bytes (not size of array)
  guint32 max_bytes_per_ch;
  bool lsb_first;
  guint sample_step;
  guint ch_step;

  guchar *data;
} dsdbuffer;

typedef struct {
  FILE *stream;                // init @ dsd_open
  bool canseek;                // init @ dsd_open
  bool eof;                    // init @ dsd_open
  gsize offset;                // init @ dsd_open
  dsdtype type;                // init @ dsd_open

  gsize file_size;             // init @ dsf_init or dsdiff_init

  guint32 channel_num;         // init 0 0 dsd_open, update @ dsf_init or dsdiff:parse_prop_chunk, check not 0
  guint32 sampling_frequency;  // init 0 0 dsd_open, update @ dsf_init or dsdiff:parse_prop_chunk, check not 0

  guint64 sample_offset;       // init 0 @ dsd_open, set X @ dsd_set_start
  guint64 sample_count;        // init @ dsf_init or dsdiff_init
  guint64 sample_stop;         // init @ dsf_init or dsdiff_init, set X @ dsd_set_stop

  dsfinfo dsf;                 // init @ dsf_init

  gsize dataoffset;            // init @ dsf_init or dsdiff_init
  gsize datasize;              // init @ dsf_init or dsdiff_init
  dsdbuffer buffer;            // 

} dsdfile;

extern const guchar bit_reverse_table[];
static inline void dsd_buffer_msb_order(dsdbuffer *ibuffer) {
  guint32 s;
  guchar *ptr;
  if (ibuffer->lsb_first) {		  
    ptr = ibuffer->data;
    for (s = ibuffer->max_bytes_per_ch * ibuffer->num_channels; s > 0; s--, ptr++)
      *ptr = bit_reverse_table[(guint8)*ptr];
  }
}

dsdfile *dsd_open(const char *name);
bool dsd_close(dsdfile *file);
bool dsd_eof(dsdfile *file);
bool dsd_set_start(dsdfile *file, guint32 mseconds);
bool dsd_set_stop(dsdfile *file, guint32 mseconds);
guint32 dsd_sample_frequency(dsdfile *file);
guint32 dsd_channels(dsdfile *file);
dsdbuffer *dsd_read(dsdfile *file);
dsdbuffer *init_halfrate(dsdbuffer *ibuffer);
void halfrate_filter(dsdbuffer *in, dsdbuffer *out);
void dsd_over_pcm(dsdbuffer *buf, guchar *pcmout);
void dsd_to_pcm(dsdbuffer *buf, guchar *pcmout);
