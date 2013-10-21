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
#include <string.h>
#include <stdbool.h>
#include "libdsd.h"
#include "dsdinternals.h"

typedef struct {
  // guchar header[4]; already ready and verified by upper level
  guint64 size_of_chunk;
  guint64 total_size;
  goffset ptr_to_metadata;
} dsd_chunk;

bool read_dsd_chunk(dsd_chunk *dsf_head, dsdfile *file) {
  guchar buffer[24];
  guchar *ptr=buffer;
  if (!dsd_read_raw(buffer, sizeof(buffer), file)) return FALSE;
  dsf_head->size_of_chunk = GUINT64_FROM_LE(*((guint64*)ptr)); ptr+=8;
  if (dsf_head->size_of_chunk != 28) return FALSE;
  dsf_head->total_size = GUINT64_FROM_LE(*((guint64*)ptr)); ptr+=8;
  dsf_head->ptr_to_metadata = GUINT64_FROM_LE(*((guint64*)ptr));
  return TRUE;
}

typedef struct {
  guchar header[4];
  guint64 size_of_chunk;
  guint32 format_version;
  guint32 format_id;
  guint32 channel_type;
  guint32 channel_num;
  guint32 sampling_frequency;
  guint32 bits_per_sample;
  guint64 sample_count;
  guint32 block_size_per_channel;
  guchar reserved[4];
} fmt_chunk;

bool read_fmt_chunk(fmt_chunk *dsf_fmt, dsdfile *file) {
  guchar buffer[52];
  guchar *ptr=buffer;
  if (!dsd_read_raw(buffer, sizeof(buffer), file)) return FALSE;
  memcpy(dsf_fmt->header, ptr, 4); ptr+=4;
  if (!DSD_MATCH(dsf_fmt->header, "fmt ")) return FALSE;
  dsf_fmt->size_of_chunk = GUINT64_FROM_LE(*((guint64*)ptr)); ptr+=8;
  if (dsf_fmt->size_of_chunk != 52) return FALSE;
  dsf_fmt->format_version = GUINT32_FROM_LE(*((guint32*)ptr)); ptr+=4;
  dsf_fmt->format_id = GUINT32_FROM_LE(*((guint32*)ptr)); ptr+=4;
  dsf_fmt->channel_type = GUINT32_FROM_LE(*((guint32*)ptr)); ptr+=4;
  dsf_fmt->channel_num = GUINT32_FROM_LE(*((guint32*)ptr)); ptr+=4;
  dsf_fmt->sampling_frequency = GUINT32_FROM_LE(*((guint32*)ptr)); ptr+=4;
  dsf_fmt->bits_per_sample = GUINT32_FROM_LE(*((guint32*)ptr)); ptr+=4;
  dsf_fmt->sample_count = GUINT64_FROM_LE(*((guint64*)ptr)); ptr+=8;
  dsf_fmt->block_size_per_channel = GUINT32_FROM_LE(*((guint32*)ptr)); ptr+=4;
  //      !DSD_MATCH(dsf_fmt.reserved,"\0\0\0\0")) {
  return TRUE;
}

typedef struct {
  guchar header[4];
  guint64 size_of_chunk;
} data_header;

bool read_data_header(data_header *dsf_datahead, dsdfile *file) {
  guchar buffer[12];
  guchar *ptr=buffer;
  if (!dsd_read_raw(buffer, sizeof(buffer), file)) return FALSE;
  memcpy(dsf_datahead->header, ptr, 4); ptr+=4;
  if (!DSD_MATCH(dsf_datahead->header, "data")) return FALSE;
  dsf_datahead->size_of_chunk = GUINT64_FROM_LE(*((guint64*)ptr));
  return TRUE;
}

bool dsf_init(dsdfile *file) {  
  dsd_chunk dsf_head;
  fmt_chunk dsf_fmt;
  data_header dsf_datahead;
  gint64 padtest;

  if (!read_dsd_chunk(&dsf_head, file)) return FALSE;
  
  if (!read_fmt_chunk(&dsf_fmt, file)) return FALSE;

  file->file_size = dsf_head.total_size;

  file->channel_num = dsf_fmt.channel_num;
  file->sampling_frequency = dsf_fmt.sampling_frequency;

  file->sample_offset = 0;
  file->sample_count = dsf_fmt.sample_count;
  file->sample_stop = file->sample_count / 8;

  file->dsf.format_version = dsf_fmt.format_version;
  file->dsf.format_id = dsf_fmt.format_id;
  file->dsf.channel_type = dsf_fmt.channel_type;
  file->dsf.bits_per_sample = dsf_fmt.bits_per_sample;
  file->dsf.block_size_per_channel = dsf_fmt.block_size_per_channel;

  if ((file->dsf.format_version != 1) ||
      (file->dsf.format_id != 0) ||
      (file->dsf.block_size_per_channel != 4096))
    return FALSE;

  if (!read_data_header(&dsf_datahead, file))
    return FALSE;

  padtest = (gint64)(dsf_datahead.size_of_chunk - 12) / file->channel_num
    - file->sample_count / 8;
  if ((padtest < 0) || (padtest > file->dsf.block_size_per_channel)) {
    return FALSE;
  }

  file->dataoffset = file->offset;
  file->datasize = (file->sample_count / 8 * file->channel_num);
  
  file->buffer.max_bytes_per_ch = file->dsf.block_size_per_channel;
  file->buffer.lsb_first = (file->dsf.bits_per_sample == 1);
  file->buffer.sample_step = 1;
  file->buffer.ch_step = file->dsf.block_size_per_channel;

  return TRUE;
}

bool dsf_set_start(dsdfile *file, guint32 mseconds) {
  
  if (!file) return FALSE;

  guint64 skip_blocks = (guint64) file->sampling_frequency * mseconds / (8000 * file->dsf.block_size_per_channel);
  guint64 skip_bytes = skip_blocks * file->dsf.block_size_per_channel * file->channel_num;

  file->sample_offset = skip_blocks * file->dsf.block_size_per_channel;

  return dsd_seek(file, skip_bytes, SEEK_CUR);
}

bool dsf_set_stop(dsdfile *file, guint32 mseconds) {

  if (!file) return FALSE;

  guint64 include_blocks = (guint64)file->sampling_frequency * mseconds / (8000 * file->dsf.block_size_per_channel) + 1;
  guint64 include_bytes = include_blocks * file->dsf.block_size_per_channel * file->channel_num;

  if (include_bytes < file->datasize) {
    file->sample_stop = include_blocks * file->dsf.block_size_per_channel;
  }

  if (file->sample_stop < file->sample_offset) file->eof = TRUE;

  return TRUE;
}

dsdbuffer *dsf_read(dsdfile *file) {
  if (file->eof) return NULL;

  if (!dsd_read_raw(file->buffer.data, file->channel_num * file->dsf.block_size_per_channel, file))
    return NULL;

  if (file->dsf.block_size_per_channel >= (file->sample_stop - file->sample_offset)) {
    file->buffer.bytes_per_channel = (file->sample_stop - file->sample_offset);
    file->eof = TRUE;
  } else {
    file->sample_offset += file->dsf.block_size_per_channel;
    file->buffer.bytes_per_channel = file->dsf.block_size_per_channel;
  }

  return &file->buffer;
}
