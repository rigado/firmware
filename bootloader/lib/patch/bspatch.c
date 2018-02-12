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

#include <stdio.h>
#include "bspatch.h"
#include "heatshrink_decoder.h"

#define BSPATCH_CTRL_CNT			3

#define BSPATCH_STATE_IDLE			0
#define BSPATCH_STATE_READ_CTRL		1
#define BSPATCH_STATE_READ_NEW		2
#define BSPATCH_STATE_READ_EXTRA	3

static int32_t m_newsize;
static int32_t m_newpos;
static int32_t m_new_buf_size;
static uint8_t * m_new_ptr;
static int32_t m_total_new;

static int32_t m_oldsize;
static int32_t m_oldpos;
static const uint8_t * m_old_ptr;

static uint8_t m_ctrl_buf[8];
static uint8_t m_ctrl_buf_idx;

static uint8_t m_patch_state;

static int64_t offtin(uint8_t *buf)
{
	int64_t y;

    y = buf[7] & 0x7F;
    for(int8_t i = 6; i >= 0; i--)
    {
        y = y * 256;
        y += buf[i];
    }
    
	if(buf[7] & 0x80) 
    {
        y = -y;
    }

	return y;
}

void bspatch_init(const uint8_t* old, int32_t oldsize, uint8_t* new_buf, int32_t newsize, int32_t new_buf_size)
{
	m_newsize = newsize;
	m_oldsize = oldsize;

	m_oldpos = 0;
	m_newpos = 0;
	m_total_new = 0;
	m_ctrl_buf_idx = 0;

	m_new_buf_size = new_buf_size;
	m_new_ptr = new_buf;
	m_old_ptr = old;

	m_patch_state = BSPATCH_STATE_READ_CTRL;
}

uint32_t bspatch_get_total_received(void)
{
    return m_total_new;
}

int32_t bspatch(struct bspatch_stream* stream)
{
	int64_t i;
	int64_t op_bytes;
	int32_t res;

	while(m_total_new < m_newsize) 
	{
		switch(m_patch_state)
		{
			case BSPATCH_STATE_READ_CTRL:
				/* Read control data */
				for(; stream->ctrl_cnt < BSPATCH_CTRL_CNT; stream->ctrl_cnt++) 
				{
					res = stream->read(stream, &m_ctrl_buf[m_ctrl_buf_idx], 8 - m_ctrl_buf_idx);
					//printf("ctrlrd: %d\n", res);
					if(res >= 0 && res != 8 - m_ctrl_buf_idx) {
						m_ctrl_buf_idx += res;
						return BSPATCH_RES_NEED_MORE;
					} 
					else if(res < 0) 
					{
						return BSPATCH_RES_ERROR;
					}
					
					stream->ctrl[stream->ctrl_cnt] = offtin(m_ctrl_buf);
					m_ctrl_buf_idx = 0;
				}

				//printf("ctrl[0]: %lld\n", stream->ctrl[0]);
				/* Sanity-check */
				if(m_total_new + stream->ctrl[0] > m_newsize)
				{
					return BSPATCH_RES_ERROR;
				}

				stream->ctrl_cnt = 0;
				m_patch_state = BSPATCH_STATE_READ_NEW;
				break;
			case BSPATCH_STATE_READ_NEW:
			{
				uint32_t new_buf_bytes_left = m_new_buf_size - m_newpos;
				uint8_t status = 0;
				
				while(stream->ctrl[0] != 0)
				{
					new_buf_bytes_left = m_new_buf_size - m_newpos;
					status = 0;

					//printf("np: %d     op: %d\n", m_newpos, m_oldpos);
					
					op_bytes = (new_buf_bytes_left < stream->ctrl[0]) ? new_buf_bytes_left : stream->ctrl[0];
					

					if(op_bytes > m_new_buf_size) {
						op_bytes = m_new_buf_size;
					}

					//printf("opb: %lld nbbl: %d\n", op_bytes, new_buf_bytes_left);
					res = stream->read(stream, m_new_ptr + m_newpos, op_bytes);
					//printf("res: %d\n", res);
					if(res < 0) 
					{
						return BSPATCH_RES_ERROR;
					}

					if(res != op_bytes)
					{
						status = BSPATCH_RES_NEED_MORE;
						op_bytes = res;
					}


					for(i = 0; i < op_bytes; i++) 
					{
						if((m_oldpos + i >= 0) && (m_oldpos + i < m_oldsize)) 
						{
							m_new_ptr[m_newpos + i] += m_old_ptr[m_oldpos + i];
						}
					}
					//printf("copied\n");

					m_newpos += op_bytes;
					m_total_new += op_bytes;
					m_oldpos += op_bytes;
					stream->ctrl[0] -= op_bytes;

					if(m_newpos == m_new_buf_size)
					{
						//TODO: If patching needs to wait on storage to be complete, return
                        //appropriate status
                        status = BSPATCH_RES_FLASHING;
						stream->store_data(m_new_ptr, m_new_buf_size);
						m_newpos = 0;
					}

					//printf("np: %d     op: %d\n", m_newpos, m_oldpos);

					if(status != 0)
					{
						return status;
					}
				}

				m_patch_state = BSPATCH_STATE_READ_EXTRA;
			}
				break;
			case BSPATCH_STATE_READ_EXTRA:
			{
				//printf("ctrl[1]: %lld\n", stream->ctrl[1]);
				//printf("ctrl[2]: %lld\n", stream->ctrl[2]);

				/* Sanity-check */
				if(m_total_new + stream->ctrl[1] > m_newsize)
					return BSPATCH_RES_ERROR;

				op_bytes = stream->ctrl[1];
				uint32_t new_buf_bytes_left = m_new_buf_size - m_newpos;

				if(op_bytes > new_buf_bytes_left)
				{
					op_bytes = new_buf_bytes_left;
				}

				while(stream->ctrl[1] != 0) {
					/* Read extra string */
					uint8_t status = 0;
					//printf("rn ob: %lld\n", op_bytes);
					res = stream->read(stream, m_new_ptr + m_newpos, op_bytes);
					if(res < 0)
					{
						return BSPATCH_RES_ERROR;
					} 
					else if(res != op_bytes)
					{
						status = BSPATCH_RES_NEED_MORE;
						//adjust op_bytes to match actual amount of data read
						op_bytes = res;
					}

					m_newpos += op_bytes;

					if(m_newpos == m_new_buf_size)
					{
						//write data to flash
						//printf("write in new\n");
                        //TODO: If patching needs to wait on storage to be complete, return
                        //appropriate status
                        status = BSPATCH_RES_FLASHING;
						stream->store_data(m_new_ptr, m_new_buf_size);
						m_newpos = 0;
					}

					m_total_new += op_bytes;
					stream->ctrl[1] -= op_bytes;

					//if we have a non-zero status, return instead of continuing with loop
					if(status != 0)
					{
						return status;
					}
					else if(stream->ctrl[1] < op_bytes) 
					{
						op_bytes = stream->ctrl[1];
					}

					new_buf_bytes_left = m_new_buf_size - m_newpos;
					if(new_buf_bytes_left < op_bytes) 
					{
						op_bytes = new_buf_bytes_left;
					}
				}

				m_oldpos += stream->ctrl[2];

				//printf("np: %d     op: %d\n", m_newpos, m_oldpos);
				m_patch_state = BSPATCH_STATE_READ_CTRL;
			}
				break;
		}
	};

	//write final data if any
	//note, control flow should never reach this point unless the patch is complete
	if(m_newpos != 0) 
	{
		stream->store_data(m_new_ptr, m_newpos);
		m_newpos = 0;
        return BSPATCH_RES_FLASHING;
	}

	return BSPATCH_RES_FINISHED;
}
