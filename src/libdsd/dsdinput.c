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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "libdsd.h"
#include "dsdinternals.h"

const guchar bit_reverse_table[256] = 
{
#define R2(n)     n,     n + 2*64,     n + 1*64,     n + 3*64
#define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )
    R6(0), R6(2), R6(1), R6(3)
};

dsdfile *dsd_open(const char *name) {
  guchar header_id[4];
  dsdfile *file = malloc(sizeof(dsdfile));

  if (name == NULL) {
    file->stream = stdin;
    file->canseek = FALSE;
  } else {
    if ((file->stream = fopen(name, "r")) == NULL) {
      free(file);
      return NULL;
    }
    file->canseek = TRUE;
  }
  file->eof = FALSE;
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
  file->buffer.data = (guchar *)malloc(sizeof(guchar) * file->buffer.max_bytes_per_ch * file->channel_num);

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
  file->offset += (goffset)bytes_read;
  
  return (bytes_read == bytes);
}

bool dsd_seek(dsdfile *file, goffset offset, int whence) {
  
  if (file->canseek) {
    if (fseek(file->stream, offset, whence) == 0) {
      file->offset += offset;
      return TRUE;
    } else {
      return FALSE;
    }
  }

  char buffer[8192];
  gsize read_bytes;
  size_t tmp;

  if (whence == SEEK_CUR)
    read_bytes = offset;
  else if (whence == SEEK_SET) {
    if (offset < 0 || (guint64)offset < file->offset)
      return FALSE;
    read_bytes = offset - file->offset;
  } else
    return FALSE;

  while (read_bytes > 0) {
    tmp = (read_bytes > sizeof(buffer) ? sizeof(buffer) : read_bytes);
    if (dsd_read_raw(buffer, tmp, file))
      read_bytes -= tmp;
    else
      return FALSE;
  }

  return TRUE;
}

guint32 dsd_sample_frequency(dsdfile *file) {
  if (file) return file->sampling_frequency;
  return 0;
}

guint32 dsd_channels(dsdfile *file) {
  if (file) return file->channel_num;
  return 0;
}

dsdbuffer *dsd_read(dsdfile *file) {
  if (!file) return NULL;

  if (file->type == DSF) return dsf_read(file);
  if (file->type == DSDIFF) return dsdiff_read(file);

  return NULL;
}

bool dsd_set_start(dsdfile *file, guint32 mseconds) {
  if (!file) return FALSE;

  if (file->type == DSF) return dsf_set_start(file, mseconds);
  if (file->type == DSDIFF) return dsdiff_set_start(file, mseconds);

  return FALSE;
}

bool dsd_set_stop(dsdfile *file, guint32 mseconds) {
  if (!file) return FALSE;

  if (file->type == DSF) return dsf_set_stop(file, mseconds);
  if (file->type == DSDIFF) return dsdiff_set_stop(file, mseconds);

  return FALSE;
}
