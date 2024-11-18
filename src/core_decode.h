#ifndef _CORE_DECODE_H
#define _CORE_DECODE_H
#include <stdint.h>
#include <stdbool.h>
#include "core_type.h"


#ifdef  __cplusplus
extern "C" {
#endif

typedef enum _DECODE_STATUS
{
    DECODE_OK = 0, 
    DECODE_FAIL, 
    DECODE_OPENERROR, // open decoder file failed 
    DECODE_SCRIPTERROR, // parsing decoder script failed
    DECODE_CALLBACKERROR,  // finding decoder callback faild
    DECODE_FORMATERROR, // target format error
    DECODE_RANGERROR, // pixel or index out of range
    DECODE_UNKNOW 
}DECODE_STATUS;

#define DECODE_SUCCESS(x) (x==DECODE_OK)

inline const char *decode_status_str(DECODE_STATUS x)
{
    if(x==DECODE_OK) return "DECODE_OK";
    if(x==DECODE_FAIL) return "DECODE_FAIL";
    if(x==DECODE_OPENERROR) return "DECODE_OPENERROR, open decoder file failed ";
    if(x==DECODE_SCRIPTERROR) return "DECODE_SCRIPTERROR, parsing decoder script failed";
    if(x==DECODE_CALLBACKERROR) return "DECODE_CALLBACKERROR, finding decoder callback faild";
    if(x==DECODE_FORMATERROR) return "DECODE_FORMATERROR, target format error";
    if(x==DECODE_RANGERROR) return "DECODE_RANGERROR, pixel or index out of range";
    return "DECODE_UNKNOW";
}

/**
 * open decoder
 * @return decoder context
 */
typedef DECODE_STATUS (*CB_decode_open)(const char *name, void **context);
DECODE_STATUS decode_open_default(const char *name, void **context);


/**
 * close decoder
 */
typedef DECODE_STATUS (*CB_decode_close)(void *context);
DECODE_STATUS decode_close_default(void *context);

/**
 *  get the pixel offset of current pixel_idx 
 *    (tile_idx, pixel_idx) -> tile_offset
 * @param tilepos_t, current pixel position
 * @param offset, out offset for current posiiton
 */
bool decode_offset_default(void *context, 
    const struct tilepos_t *pos, const struct tilefmt_t *fmt, size_t *offset);

/**
 *  decode 1 pixel from the offset
 * @param data, corrent decoding data
 * @param pixel_t out a uint32_t value
 * @param remain_index keep the origin index
 */
typedef DECODE_STATUS (*CB_decode_pixel)(void *context, 
    const uint8_t* data, size_t datasize, 
    const struct tilepos_t *pos, const struct tilefmt_t *fmt, 
    struct pixel_t *pixel, bool remain_index);

DECODE_STATUS decode_pixel_default(void *context, 
    const uint8_t* data, size_t datasize,
    const struct tilepos_t *pos, const struct tilefmt_t *fmt, 
    struct pixel_t *pixel, bool remain_index);

/**
 * decode pre, post processing
 * @param rawdata rawdata of the file
 */
typedef DECODE_STATUS (*CB_decode_parse)(void *context, 
    const uint8_t* rawdata, size_t rawsize, struct tilecfg_t *cfg);

struct tile_decoder_t
{
    REQUIRED CB_decode_open open;
    REQUIRED CB_decode_close close;
    REQUIRED CB_decode_pixel decode;
    OPTIONAL CB_decode_parse pre;
    OPTIONAL CB_decode_parse post;
    void* context;
    const char *msg; // for store decoder failed msg
};

extern struct tile_decoder_t g_decoder_default;

#ifdef  __cplusplus
}
#endif

#endif