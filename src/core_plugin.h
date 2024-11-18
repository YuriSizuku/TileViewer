#ifndef _CORE_PLUGIN_H
#define _CORE_PLUGIN_H
#include "core_plugin.h"

#ifdef  __cplusplus
extern "C" {
#endif

DECODE_STATUS decode_open_lua(const char *name, void **context);
DECODE_STATUS decode_close_lua(void *context);
DECODE_STATUS decode_pixel_lua(void *context, 
    const uint8_t* data, size_t datasize,
    const struct tilepos_t *pos, const struct tilefmt_t *fmt, 
    struct pixel_t *pixel, bool remain_index);

DECODE_STATUS decode_pre_lua(void *context, 
    const uint8_t* rawdata, size_t rawsize, struct tilecfg_t *cfg);
DECODE_STATUS decode_post_lua(void *context, 
    const uint8_t* rawdata, size_t rawsize, struct tilecfg_t *cfg);

extern struct tile_decoder_t g_decoder_lua;

#ifdef  __cplusplus
}
#endif

#endif