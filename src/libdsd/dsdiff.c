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
  // char id[4]; Read by upper level
  u64_t size;
  u8_t prop[4];
} dsdiff_header;

typedef struct {
  u8_t id[4];
  u64_t size;
} dsdiff_chunk_header;

static u64_t unpack64be(const u8_t *p) {
	return 
		(u64_t)p[0] << 56 | (u64_t)p[1] << 48 | (u64_t)p[2] << 40 | (u64_t)p[3] << 32 |
		(u64_t)p[4] << 24 | (u64_t)p[5] << 16 |	(u64_t)p[6] << 8 | (u64_t)p[7];
}

static u32_t unpack32be(const u8_t *p) {
	return
		(u32_t)p[0] << 24 | (u32_t)p[1] << 16 |	(u32_t)p[2] << 8 | (u32_t)p[3];
}

static u16_t unpack16be(const u8_t *p) {
	return
		(u16_t)p[0] << 8 | (u32_t)p[1];
}

static bool read_header(dsdiff_header *head, dsdfile *file) {
  u8_t buffer[12];
  u8_t *ptr = buffer;

  if (!dsd_read_raw(buffer, sizeof(buffer), file)) return false;

  head->size = unpack64be(ptr); ptr+=8;

  memcpy(head->prop, ptr, 4);

  if (!DSD_MATCH(head->prop, "DSD ")) return false;

  return true;
}

static bool read_chunk_header(dsdiff_chunk_header *head, dsdfile *file) {
  u8_t buffer[12];
  u8_t *ptr = buffer;

  if (!dsd_read_raw(buffer, sizeof(buffer), file)) return false;

  memcpy(head->id, ptr, 4); ptr+=4;

  head->size = unpack64be(ptr);

  return true;
}

static bool parse_prop_chunk(dsdfile *file, dsdiff_chunk_header *head) {
  u64_t stop = file->offset + head->size;
  dsdiff_chunk_header prop_head;
  u8_t propType[4];

  if (!dsd_read_raw(propType, sizeof(propType), file)) return false;

  if (!DSD_MATCH(propType, "SND ")) {
    if (dsd_seek(file, head->size - sizeof(propType), SEEK_CUR))
      return true;
    else
      return false;
  }
  
  while (file->offset < stop) {

    if (!read_chunk_header(&prop_head, file)) return false;

    if (DSD_MATCH(prop_head.id, "FS  ")) {
      u32_t sample_frequency;
      if (!dsd_read_raw((u8_t *)&sample_frequency, sizeof(sample_frequency), file)) return false;
      file->sampling_frequency = unpack32be((u8_t *)&sample_frequency);
    } else if (DSD_MATCH(prop_head.id, "CHNL")) {
      u16_t num_channels;
      if (!dsd_read_raw((u8_t *)&num_channels, sizeof(num_channels), file) ||
					!dsd_seek(file, prop_head.size - sizeof(num_channels), SEEK_CUR)) return false;
      file->channel_num = (u32_t)unpack16be((u8_t *)&num_channels);
    } else
      dsd_seek(file, prop_head.size, SEEK_CUR);
  }
  
  return true;
}

bool dsdiff_init(dsdfile *file) {
  dsdiff_header head;
  dsdiff_chunk_header chunk_head;

  if (!read_header(&head, file)) return false;
  file->file_size = head.size + 12;

  while (read_chunk_header(&chunk_head, file)) {

    if (DSD_MATCH(chunk_head.id, "FVER")) {
      char version[4];
      if (chunk_head.size != 4 ||
					!dsd_read_raw(&version, sizeof(version), file) ||
					version[0] > 1) { // Major version 1 supported
				return false;
      }
    } else if (DSD_MATCH(chunk_head.id, "PROP")) {
      if (!parse_prop_chunk(file, &chunk_head)) return false;
    } else if (DSD_MATCH(chunk_head.id, "DSD ")) {
      if (file->channel_num == 0) return false;
      
      file->sample_offset = 0;
      file->sample_count = 8 * chunk_head.size / file->channel_num;
      file->sample_stop = file->sample_count / 8;
			
      file->dataoffset = file->offset;
      file->datasize = chunk_head.size;
			
      file->buffer.max_bytes_per_ch = 4096;
      file->buffer.lsb_first = false;
      file->buffer.sample_step = file->channel_num;
      file->buffer.ch_step = 1;
      
      return true;
    } else
      dsd_seek(file, chunk_head.size, SEEK_CUR);
  }

  return false;
}

bool dsdiff_set_start(dsdfile *file, u32_t mseconds) {
  u64_t skip_bytes;

  if (!file) return false;

  file->sample_offset = (u64_t)file->sampling_frequency * mseconds / 8000;
  skip_bytes = file->sample_offset * file->channel_num;

  return dsd_seek(file, skip_bytes, SEEK_CUR);
}

bool dsdiff_set_stop(dsdfile *file, u32_t mseconds) {
	u64_t include_samples;

  if (!file) return false;

  include_samples = (u64_t)file->sampling_frequency * mseconds / 8000;

  if ((include_samples * file->channel_num) < file->datasize)
    file->sample_stop = include_samples;

  if (file->sample_stop < file->sample_offset) file->eof = true;

  return true;
}

dsdbuffer *dsdiff_read(dsdfile *file) {
  u32_t num_samples;

  if (file->eof) return NULL;

  if ((file->sample_stop - file->sample_offset) < file->buffer.max_bytes_per_ch)
    num_samples = (u32_t)(file->sample_stop - file->sample_offset);
  else
    num_samples = (u32_t)file->buffer.max_bytes_per_ch;

  if (!dsd_read_raw(file->buffer.data, file->channel_num * num_samples, file))
      return NULL;

  file->buffer.bytes_per_channel = num_samples;
  file->sample_offset += num_samples;
  if (file->sample_offset >= file->sample_stop) file->eof = true;

  return &file->buffer;
}
