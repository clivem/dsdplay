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

#define DSD_MATCH(x,y) (memcmp((x),(y),sizeof(x))==0)

extern const u8_t bit_reverse_table[256];

static inline u8_t bit_reverse(u8_t value) {
  return bit_reverse_table[value];
}

bool dsdiff_init(dsdfile *file);

bool dsf_init(dsdfile *file);
bool dsf_set_start(dsdfile *file, u32_t mseconds);
bool dsf_set_stop(dsdfile *file, u32_t mseconds);
dsdbuffer *dsf_read(dsdfile *file);

bool dsdiff_init(dsdfile *file);
bool dsdiff_set_start(dsdfile *file, u32_t mseconds);
bool dsdiff_set_stop(dsdfile *file, u32_t mseconds);
dsdbuffer *dsdiff_read(dsdfile *file);

bool dsd_read_raw(void *buffer, size_t bytes, dsdfile *file);
bool dsd_seek(dsdfile *file, s64_t offset, int whence);
