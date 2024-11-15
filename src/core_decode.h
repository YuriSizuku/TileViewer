#ifndef _CORE_DECODE_H
#define _CORE_DECODE_H
#include <stdint.h>
#include <stdbool.h>
#include "core_type.h"


#ifdef  __cplusplus
extern "C" {
#endif

/**
 *  decode pre alloc data
 *  @return outsize
 */
typedef size_t (*CB_decode_alloc)(const uint8_t* indata, size_t insize);

/**
 *  decode preprocessing
 */
typedef bool (*CB_decode_pre)(const uint8_t* indata, size_t insize, uint8_t *outdata, size_t outsize);

/**
 *  get the pixel offset of current pixel_idx 
 *    (tile_idx, pixel_idx) -> tile_offset
 * @param tilepos_t, current pixel position
 * @param offset, out offset for current posiiton
 */
typedef bool (*CB_pixel_offset)(const struct tilepos_t *pos, const struct tilefmt_t *fmt, size_t *offset);

bool pixel_offset_default(const struct tilepos_t *pos, const struct tilefmt_t *fmt, size_t *offset);

/**
 *  decode 1 pixel from the offset
 * @param pixel_t out a uint32_t value
 * @param remain_index keep the origin index
 */
typedef bool (*CB_decode_pixel)(const uint8_t* data, size_t datasize, 
    const struct tilepos_t *pos, const struct tilefmt_t *fmt, 
    struct pixel_t *pixel, bool remain_index);

bool decode_pixel_default(const uint8_t* data, size_t datasize,
    const struct tilepos_t *pos, const struct tilefmt_t *fmt, 
    struct pixel_t *pixel, bool remain_index);

struct tile_decoder_t
{
    REQUIRED CB_decode_pixel decode_pixel;
    OPTIONAL CB_decode_alloc decode_alloc;
    OPTIONAL CB_decode_pre decode_pre;
};

#ifdef  __cplusplus
}
#endif

#endif