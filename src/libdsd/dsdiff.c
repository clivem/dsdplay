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
  // char id[4]; Read by upped level
  guint64 size;
  char prop[4];
} dsdiff_header;

static
bool read_header(dsdiff_header *head, dsdfile *file) {
  guchar buffer[12];
  guchar *ptr=buffer;
  if (!dsd_read_raw(buffer, sizeof(buffer), file)) return FALSE;
  head->size = GUINT64_FROM_BE(*((guint64*)ptr)); ptr+=8;
  memcpy(head->prop, ptr, 4);
  if (!DSD_MATCH(head->prop, "DSD ")) return FALSE;
  return TRUE;
}

typedef struct {
  char id[4];
  guint64 size;
} dsdiff_chunk_header;

static
bool read_chunk_header(dsdiff_chunk_header *head, dsdfile *file) {
  guchar buffer[12];
  guchar *ptr=buffer;
  if (!dsd_read_raw(buffer, sizeof(buffer), file)) return FALSE;
  memcpy(head->id, ptr, 4); ptr+=4;
  head->size = GUINT64_FROM_BE(*((guint64*)ptr));
  return TRUE;
}

static
bool parse_prop_chunk(dsdfile *file, dsdiff_chunk_header *head) {
  guint64 stop = file->offset + head->size;
  dsdiff_chunk_header prop_head;
  char propType[4];

  if (!dsd_read_raw(propType, sizeof(propType), file)) return FALSE;

  if (!DSD_MATCH(propType, "SND ")) {
    if (dsd_seek(file, head->size - sizeof(propType), SEEK_CUR))
      return TRUE;
    else
      return FALSE;
  }
  
  while (file->offset < stop) {
    if (!read_chunk_header(&prop_head, file)) return FALSE;
    if (DSD_MATCH(prop_head.id, "FS  ")) {
      guint32 sample_frequency;
      if (!dsd_read_raw(&sample_frequency, sizeof(sample_frequency), file)) return FALSE;
      file->sampling_frequency = GUINT32_FROM_BE(sample_frequency);
    } else if (DSD_MATCH(prop_head.id, "CHNL")) {
      guint16 num_channels;
      if (!dsd_read_raw(&num_channels, sizeof(num_channels), file) ||
	  !dsd_seek(file, prop_head.size - sizeof(num_channels), SEEK_CUR)) return FALSE;
      file->channel_num = (guint32)GUINT16_FROM_BE(num_channels);
    } else
      dsd_seek(file, prop_head.size, SEEK_CUR);
  }
  
  return TRUE;
}

bool dsdiff_init(dsdfile *file) {
  dsdiff_header head;
  dsdiff_chunk_header chunk_head;

  if (!read_header(&head, file)) return FALSE;
  file->file_size = head.size + 12;

  while (read_chunk_header(&chunk_head, file)) {
    if (DSD_MATCH(chunk_head.id, "FVER")) {
      char version[4];
      if (chunk_head.size != 4 ||
	  !dsd_read_raw(&version, sizeof(version), file) ||
	  version[0] > 1) { // Major version 1 supported
	return FALSE;
      }
    } else if (DSD_MATCH(chunk_head.id, "PROP")) {
      if (!parse_prop_chunk(file, &chunk_head)) return FALSE;
    } else if (DSD_MATCH(chunk_head.id, "DSD ")) {
      if (file->channel_num == 0) return FALSE;
      
      file->sample_offset = 0;
      file->sample_count = 8 * chunk_head.size / file->channel_num;
      file->sample_stop = file->sample_count / 8;

      file->dataoffset = file->offset;
      file->datasize = chunk_head.size;

      file->buffer.max_bytes_per_ch = 4096;
      file->buffer.lsb_first = FALSE;
      file->buffer.sample_step = file->channel_num;
      file->buffer.ch_step = 1;
      
      return TRUE;
    } else
      dsd_seek(file, chunk_head.size, SEEK_CUR);
  }

  return FALSE;
}

bool dsdiff_set_start(dsdfile *file, guint32 mseconds) {
  
  if (!file) return FALSE;

  file->sample_offset = (guint64)file->sampling_frequency * mseconds / 8000;
  guint64 skip_bytes = file->sample_offset * file->channel_num;

  return dsd_seek(file, skip_bytes, SEEK_CUR);
}

bool dsdiff_set_stop(dsdfile *file, guint32 mseconds) {

  if (!file) return FALSE;

  guint64 include_samples = (guint64)file->sampling_frequency * mseconds / 8000;

  if ((include_samples * file->channel_num) < file->datasize)
    file->sample_stop = include_samples;

  if (file->sample_stop < file->sample_offset) file->eof = TRUE;

  return TRUE;
}

dsdbuffer *dsdiff_read(dsdfile *file) {
  guint num_samples;

  if (file->eof) return NULL;

  if ((file->sample_stop - file->sample_offset) < file->buffer.max_bytes_per_ch)
    num_samples = file->sample_stop - file->sample_offset;
  else
    num_samples = file->buffer.max_bytes_per_ch;

  if (!dsd_read_raw(file->buffer.data, file->channel_num * num_samples, file))
      return NULL;

  file->buffer.bytes_per_channel = num_samples;
  file->sample_offset += num_samples;
  if (file->sample_offset >= file->sample_stop) file->eof = TRUE;

  return &file->buffer;
}
