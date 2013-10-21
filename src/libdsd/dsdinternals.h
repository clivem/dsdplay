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
#define DSD_MATCH(x,y) (memcmp((x),(y),sizeof(x))==0)

extern const guchar bit_reverse_table[256];

static inline guchar bit_reverse(guchar value) {
  return bit_reverse_table[value];
}

bool dsdiff_init(dsdfile *file);

bool dsf_init(dsdfile *file);
bool dsf_set_start(dsdfile *file, guint32 mseconds);
bool dsf_set_stop(dsdfile *file, guint32 mseconds);
dsdbuffer *dsf_read(dsdfile *file);

bool dsdiff_init(dsdfile *file);
bool dsdiff_set_start(dsdfile *file, guint32 mseconds);
bool dsdiff_set_stop(dsdfile *file, guint32 mseconds);
dsdbuffer *dsdiff_read(dsdfile *file);

bool dsd_read_raw(void *buffer, size_t bytes, dsdfile *file);
bool dsd_seek(dsdfile *file, goffset offset, int whence);
