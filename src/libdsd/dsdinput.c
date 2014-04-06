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
#include <string.h>
#include "types.h"
#include "libdsd.h"
#include "dsdinternals.h"

const u8_t bit_reverse_table[256] = {
#define R2(n)     n,     n + 2*64,     n + 1*64,     n + 3*64
#define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )
    R6(0), R6(2), R6(1), R6(3)
};

dsdfile *dsd_open(const char *name) {
  u8_t header_id[4];
  dsdfile *file = malloc(sizeof(dsdfile));

  if (name == NULL) {
    file->stream = stdin;
    file->canseek = false;
  } else {
    if ((file->stream = fopen(name, "rb")) == NULL) {
      free(file);
      return NULL;
    }
    file->canseek = true;
  }
  file->eof = false;
  file->offset = 0;
  file->sample_offset = 0;
  file->channel_num = 0;
  file->sampling_frequency = 0;

  dsd_read_raw(header_id, sizeof(header_id), file);

  if (DSD_MATCH(header_id, "DSD ")) {
    file->type = DSF;
    if (!dsf_init(file)) {
      free(file);
      return NULL;
    }
  } else if (DSD_MATCH(header_id, "FRM8")) {
    file->type = DSDIFF;
    if (!dsdiff_init(file)) {
      free(file);
      return NULL;
    }
  } else {
    free(file);
    return NULL;
  }
  
  // Finalize buffer
  file->buffer.num_channels = file->channel_num;
  file->buffer.bytes_per_channel = 0;
  file->buffer.data = (u8_t *)malloc(sizeof(u8_t) * file->buffer.max_bytes_per_ch * file->channel_num);

  return file;
}

bool dsd_eof(dsdfile *file) {
  if (!file->eof) file->eof = (bool)feof(file->stream);
  return file->eof;
}

bool dsd_close(dsdfile *file) {
  bool success = (fclose(file->stream) == 0);
  
  free(file->buffer.data);
  free(file);
  
  return success;
}

bool dsd_read_raw(void *buffer, size_t bytes, dsdfile *file) {
  size_t bytes_read;
  bytes_read = fread(buffer, (size_t)1, bytes, file->stream);
  file->offset += bytes_read;
  
  return (bytes_read == bytes);
}

bool dsd_seek(dsdfile *file, s64_t offset, int whence) {
  u8_t buffer[8192];
  u64_t read_bytes, tmp;
  
  if (file->canseek) {
    if (fseek(file->stream, (long)offset, whence) == 0) {
      file->offset += offset;
      return true;
    } else {
      return false;
    }
  }

  if (whence == SEEK_CUR)
    read_bytes = offset;
  else if (whence == SEEK_SET) {
    if (offset < 0 || (u64_t)offset < file->offset)
      return false;
    read_bytes = offset - file->offset;
  } else
    return false;

  while (read_bytes > 0) {
    tmp = (read_bytes > sizeof(buffer) ? sizeof(buffer) : read_bytes);
    if (dsd_read_raw(buffer, (size_t)tmp, file))
      read_bytes -= tmp;
    else
      return false;
  }

  return true;
}

u32_t dsd_sample_frequency(dsdfile *file) {
  if (file) return file->sampling_frequency;
  return 0;
}

u32_t dsd_channels(dsdfile *file) {
  if (file) return file->channel_num;
  return 0;
}

dsdbuffer *dsd_read(dsdfile *file) {
  if (!file) return NULL;

  if (file->type == DSF) return dsf_read(file);
  if (file->type == DSDIFF) return dsdiff_read(file);

  return NULL;
}

bool dsd_set_start(dsdfile *file, u32_t mseconds) {
  if (!file) return false;

  if (file->type == DSF) return dsf_set_start(file, mseconds);
  if (file->type == DSDIFF) return dsdiff_set_start(file, mseconds);

  return false;
}

bool dsd_set_stop(dsdfile *file, u32_t mseconds) {
  if (!file) return false;

  if (file->type == DSF) return dsf_set_stop(file, mseconds);
  if (file->type == DSDIFF) return dsdiff_set_stop(file, mseconds);

  return false;
}
