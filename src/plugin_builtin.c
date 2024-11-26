/**
 * implement for builtin plugin
 *   developed by devseed
 */

#include <stdio.h>
#include <string.h>
#include "plugin.h"

static char s_msg[4096] = {0};

PLUGIN_STATUS decode_open_default(const char *name, void **context)
{
    s_msg[0] = '\0';
    sprintf(s_msg, "[plugin_builtin::open]");
    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return STATUS_OK;
}

PLUGIN_STATUS decode_close_default(void *context)
{
    s_msg[0] = '\0';
    sprintf(s_msg, "[plugin_builtin::close]");
    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return STATUS_OK;
}

bool decode_offset_default(void *context, 
    const struct tilepos_t *pos, const struct tilefmt_t *fmt, size_t *offset)
{
    // no safety check for pointer here
    int i = pos->i, x = pos->x, y = pos->y;
    size_t w = fmt->w, h = fmt->h;
    size_t nbytes = calc_tile_nbytes(fmt);
    uint8_t bpp = fmt->bpp;

    size_t offset1 = i * nbytes;
    size_t offset2 = (x + y * w) * bpp / 8;
    *offset = offset1 + offset2;

    return true;
}

PLUGIN_STATUS decode_pixel_default(void *context,
    const uint8_t* data, size_t datasize, 
    const struct tilepos_t *pos, const struct tilefmt_t *fmt, 
    struct pixel_t *pixel, bool remain_index)
{
    // find decode offset 
    uint8_t bpp = fmt->bpp;
    size_t offset = 0;
    if(!decode_offset_default(context, pos, fmt, &offset)) return STATUS_RANGERROR;
    if(offset + bpp/8 >= datasize) return STATUS_RANGERROR;

    // try decode in different bpp
    if(bpp==32) // rgba8888
    {
        memcpy(pixel, data + offset, 4);
    }
    else if(bpp==24) // rgb888
    {
        memcpy(pixel, data + offset, 3);
        if(!remain_index) pixel->a = 255;
    }
    else if(bpp==16) // rgb565, index16
    {
        if(remain_index)
        {
            pixel->d = data[offset] | data[offset+1] << 8;
        }
        else
        {   
            pixel->r = (data[offset] & 0b11111000) > 0x3; 
            pixel->g = (data[offset] & 0b00000111) | ((data[offset+1] & 0b11100000) >> 5) << 3;
            pixel->b = data[offset+1] & 0b00011111;
            pixel->a = 255;
        }
    }
    else if(bpp==8) // index8
    {
        if(remain_index) 
        {
            pixel->d = data[offset];
        }
        else
        {
            pixel->r = pixel->g = pixel->b = data[offset];
            pixel->a = 255;
        }
    }
    else if(bpp==3) // 3 bytes for 8 pixels
    {
        size_t nbytes = calc_tile_nbytes(fmt);
        int pixel_idx = pos->x + pos->y * fmt->w;
        offset =  pos->i * nbytes + pixel_idx / 8 * 3; // offset is incresed by 3
        uint8_t bitshift = 21  - (pixel_idx % 8) * bpp; 
        uint32_t mask = ((1<<bpp) - 1) << bitshift;
        uint32_t d3 = *(uint32_t*)(data + offset) & 0x00ffffff;
        d3 = ((d3 & 0xFF) << 16) | ((d3 & 0xFF00)) | ((d3 & 0xFF0000) >> 16);
        uint8_t d = (d3 & mask) >> bitshift ;
        if(remain_index)
        {
            pixel->d = d;
        }
        else
        {
            pixel->r = pixel->g = pixel->b = d * 255/ ((1<<bpp) - 1);
            pixel->a = 255;
        }
    }
    else if (bpp < 8) // index4, index2, index1
    {
        int pixel_idx = pos->x + pos->y * fmt->w;
        uint8_t bitshift = (pixel_idx % (8 / bpp)) * bpp; 
        uint8_t mask = ((1<<bpp) - 1) << bitshift;
        uint8_t d = (data[offset] & mask) >> bitshift;
        if(remain_index)
        {
            pixel->d = d;
        }
        else
        {
            pixel->r = pixel->g = pixel->b = d * 255/ ((1<<bpp) - 1);
            pixel->a = 255;
        }
    }
    return STATUS_OK;
}

struct tile_decoder_t g_decoder_default = {
    .version = 340, .size = sizeof(struct tile_decoder_t), 
    .msg = s_msg, .context = NULL,
    .open = decode_open_default, .close = decode_close_default, 
    .decodeone = decode_pixel_default, .decodeall = NULL, 
    .pre = NULL, .post=NULL, 
    .setui=NULL, .getui=NULL, 
};
