#include <string.h>

#include "heatshrink_decoder.h"

#include "patcher.h"

static heatshrink_decoder m_decoder;
static struct bspatch_stream m_stream;
size_t total_sunk_cnt = 0;

static int32_t heatshrink_read(const struct bspatch_stream* stream, void* buffer, uint32_t length);

int32_t patcher_init(patch_init_t * init_data) 
{
    if(init_data == NULL)
        return PATCHER_FAIL;
    
    if(init_data->new_buf_ptr == NULL || init_data->old_ptr == NULL)
        return PATCHER_FAIL;
    
    heatshrink_decoder_reset(&m_decoder);
    bspatch_init(init_data->old_ptr, init_data->old_size, init_data->new_buf_ptr, init_data->new_size, init_data->new_buf_size);
    
    memset(&m_stream, 0, sizeof(m_stream));
    m_stream.read = heatshrink_read;
    m_stream.store_data = init_data->store_func;
    total_sunk_cnt = 0;
    
    return PATCHER_SUCCESS;
}

int32_t patcher_add_data(uint8_t * data, uint32_t len)
{
    size_t sunk_cnt;
    HSD_sink_res status = heatshrink_decoder_sink(&m_decoder, data, len, &sunk_cnt);
    total_sunk_cnt += sunk_cnt;
    if(status == HSDR_SINK_FULL)
    {
        return PATCHER_INPUT_FULL;
    } 
    else if(status < HSDR_SINK_OK) 
    {
        return PATCHER_FAIL;
    }
    
    return PATCHER_SUCCESS;
}

int32_t patcher_patch(void)
{
    int32_t status = BSPATCH_RES_NEED_MORE;
    while(status != BSPATCH_RES_FINISHED)
    {
        status = bspatch(&m_stream);
        
        if(status == BSPATCH_RES_NEED_MORE)
        {
            return PATCHER_NEED_MORE;
        } 
        else if(status == BSPATCH_RES_ERROR) 
        {
            return PATCHER_FAIL;
        }
        else if(status == BSPATCH_RES_FLASHING)
        {
            return PATCHER_FLASHING;
        }
    }
    
    return PATCHER_COMPLETE;
}

uint32_t patcher_get_bytes_received(void)
{
    return bspatch_get_total_received();
}

static int32_t heatshrink_read(const struct bspatch_stream* stream, void* buffer, uint32_t length) 
{
	size_t bytes_out = 0;
	HSD_poll_res status = heatshrink_decoder_poll(&m_decoder, buffer, length, &bytes_out);
	if(status < 0) {
		return -1;
	}

	return bytes_out;
}
