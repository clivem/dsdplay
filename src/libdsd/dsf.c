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
#include <string.h>
#include "types.h"
#include "libdsd.h"
#include "dsdinternals.h"

typedef struct {
  // guchar header[4]; already ready and verified by upper level
  u64_t size_of_chunk;
  u64_t total_size;
  s64_t ptr_to_metadata;
} dsd_chunk;

typedef struct {
  u8_t header[4];
  u64_t size_of_chunk;
  u32_t format_version;
  u32_t format_id;
  u32_t channel_type;
  u32_t channel_num;
  u32_t sampling_frequency;
  u32_t bits_per_sample;
  u64_t sample_count;
  u32_t block_size_per_channel;
  u8_t reserved[4];
} fmt_chunk;

typedef struct {
  u8_t header[4];
  u64_t size_of_chunk;
} data_header;

static u64_t unpack64le(const u8_t *p) {
	return
		(u64_t)p[7] << 56 | (u64_t)p[6] << 48 | (u64_t)p[5] << 40 | (u64_t)p[4] << 32 |
		(u64_t)p[3] << 24 | (u64_t)p[2] << 16 |	(u64_t)p[1] << 8 | (u64_t)p[0];
}

static u32_t unpack32le(const u8_t *p) {
	return
		(u32_t)p[3] << 24 | (u32_t)p[2] << 16 |	(u32_t)p[1] << 8 | (u32_t)p[0];
}

bool read_dsd_chunk(dsd_chunk *dsf_head, dsdfile *file) {
  u8_t buffer[24];
  u8_t *ptr=buffer;

  if (!dsd_read_raw(buffer, sizeof(buffer), file)) return false;

  dsf_head->size_of_chunk = unpack64le(ptr); ptr+=8;

  if (dsf_head->size_of_chunk != 28) return false;

  dsf_head->total_size = unpack64le(ptr); ptr+=8;
  dsf_head->ptr_to_metadata = unpack64le(ptr);

  return true;
}

bool read_fmt_chunk(fmt_chunk *dsf_fmt, dsdfile *file) {
  u8_t buffer[52];
  u8_t *ptr = buffer;

  if (!dsd_read_raw(buffer, sizeof(buffer), file)) return false;

  memcpy(dsf_fmt->header, ptr, 4); ptr+=4;

  if (!DSD_MATCH(dsf_fmt->header, "fmt ")) return false;

  dsf_fmt->size_of_chunk = unpack64le(ptr); ptr+=8;

  if (dsf_fmt->size_of_chunk != 52) return false;

  dsf_fmt->format_version = unpack32le(ptr); ptr+=4;
  dsf_fmt->format_id = unpack32le(ptr); ptr+=4;
  dsf_fmt->channel_type = unpack32le(ptr); ptr+=4;
  dsf_fmt->channel_num = unpack32le(ptr); ptr+=4;
  dsf_fmt->sampling_frequency = unpack32le(ptr); ptr+=4;
  dsf_fmt->bits_per_sample = unpack32le(ptr); ptr+=4;
  dsf_fmt->sample_count = unpack64le(ptr); ptr+=8;
  dsf_fmt->block_size_per_channel = unpack32le(ptr); ptr+=4;

  return true;
}

bool read_data_header(data_header *dsf_datahead, dsdfile *file) {
  u8_t buffer[12];
  u8_t *ptr = buffer;

  if (!dsd_read_raw(buffer, sizeof(buffer), file)) return false;

  memcpy(dsf_datahead->header, ptr, 4); ptr+=4;

  if (!DSD_MATCH(dsf_datahead->header, "data"))	return false;

  dsf_datahead->size_of_chunk = unpack64le(ptr);

  return true;
}

bool dsf_init(dsdfile *file) {  
  dsd_chunk dsf_head;
  fmt_chunk dsf_fmt;
  data_header dsf_datahead;
  s64_t padtest;

  if (!read_dsd_chunk(&dsf_head, file))	return false;
  
  if (!read_fmt_chunk(&dsf_fmt, file)) return false;

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
    return false;

  if (!read_data_header(&dsf_datahead, file))
    return false;

  padtest = (s64_t)(dsf_datahead.size_of_chunk - 12) / file->channel_num
    - file->sample_count / 8;
  if ((padtest < 0) || (padtest > file->dsf.block_size_per_channel)) {
    return false;
  }

  file->dataoffset = file->offset;
  file->datasize = (file->sample_count / 8 * file->channel_num);
  
  file->buffer.max_bytes_per_ch = file->dsf.block_size_per_channel;
  file->buffer.lsb_first = (file->dsf.bits_per_sample == 1);
  file->buffer.sample_step = 1;
  file->buffer.ch_step = file->dsf.block_size_per_channel;

  return true;
}

bool dsf_set_start(dsdfile *file, u32_t mseconds) {
	u64_t skip_blocks, skip_bytes;
  
  if (!file) return false;

  skip_blocks = (u64_t) file->sampling_frequency * mseconds / (8000 * file->dsf.block_size_per_channel);
  skip_bytes = skip_blocks * file->dsf.block_size_per_channel * file->channel_num;

  file->sample_offset = skip_blocks * file->dsf.block_size_per_channel;

  return dsd_seek(file, skip_bytes, SEEK_CUR);
}

bool dsf_set_stop(dsdfile *file, u32_t mseconds) {
	u64_t include_blocks, include_bytes;

  if (!file) return false;

  include_blocks = (u64_t)file->sampling_frequency * mseconds / (8000 * file->dsf.block_size_per_channel) + 1;
  include_bytes = include_blocks * file->dsf.block_size_per_channel * file->channel_num;

  if (include_bytes < file->datasize) {
    file->sample_stop = include_blocks * file->dsf.block_size_per_channel;
  }

  if (file->sample_stop < file->sample_offset) file->eof = true;

  return true;
}

dsdbuffer *dsf_read(dsdfile *file) {
  if (file->eof) return NULL;

  if (!dsd_read_raw(file->buffer.data, file->channel_num * file->dsf.block_size_per_channel, file))
    return NULL;

  if (file->dsf.block_size_per_channel >= (file->sample_stop - file->sample_offset)) {
    file->buffer.bytes_per_channel = (u32_t)(file->sample_stop - file->sample_offset);
    file->eof = true;
  } else {
    file->sample_offset += file->dsf.block_size_per_channel;
    file->buffer.bytes_per_channel = file->dsf.block_size_per_channel;
  }

  return &file->buffer;
}
