/**
 * implement for tile decoding
 *   developed by devseed
 */

#include <stdio.h>
#include <string.h>
#include "core_type.h"
#include "core_decode.h"

bool pixel_offset_default(const struct tilepos_t *pos, const struct tilefmt_t *fmt, size_t *offset)
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

bool decode_pixel_default(const uint8_t* data, size_t datasize, 
    const struct tilepos_t *pos, const struct tilefmt_t *fmt, 
    struct pixel_t *pixel, bool remain_index)
{
    // find decode offset 
    uint8_t bpp = fmt->bpp;
    size_t offset = 0;
    if(!pixel_offset_default(pos, fmt, &offset)) return false;
    if(offset + bpp/8 >= datasize) return false;

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
    else if(bpp==16) // rgb565, index16, not tested yet
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
    return true;
}