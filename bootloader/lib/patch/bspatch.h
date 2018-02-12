/*-
 * Copyright 2003-2005 Colin Percival
 * Copyright 2012 Matthew Endsley
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BSPATCH_H
#define BSPATCH_H

#include <stdint.h>

#define BSPATCH_RES_FLASHING        2
#define BSPATCH_RES_NEED_MORE		1
#define BSPATCH_RES_FINISHED		0
#define BSPATCH_RES_ERROR			(-1)

typedef uint32_t (*store_data_fptr)(uint8_t * data, uint32_t len);

struct bspatch_stream
{
	void* opaque;
	int32_t (*read)(const struct bspatch_stream* stream, void* buffer, uint32_t length);
    store_data_fptr store_data;
    int64_t ctrl[3];
	int32_t ctrl_cnt;
};

void bspatch_init(const uint8_t* old, int32_t oldsize, uint8_t* new_buf, int32_t newsize, int32_t new_buf_size);
uint32_t bspatch_get_total_received(void);
int32_t bspatch(struct bspatch_stream* stream);

#endif

