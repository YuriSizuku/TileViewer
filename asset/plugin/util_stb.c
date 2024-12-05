/**
 * implement for c module plugin to decode images by stb
 *   developed by devseed
 */

#include <stdio.h>
#include <string.h>
#include "plugin.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "stb_image.h"

#if defined(_MSC_VER) || defined(__TINYC__)
#ifndef EXPORT
#define EXPORT __declspec(dllexport)
#endif
#else
#ifndef EXPORT 
#define EXPORT __attribute__((visibility("default")))
#endif
#endif

static char s_msg[4096] = {0};
static stbi_uc *g_img = NULL;

PLUGIN_STATUS decode_open(const char *name, void **context)
{
    s_msg[0] = '\0';
    sprintf(s_msg, "[cmodule_util_png::open] open %s, version v0.1", name);
    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return STATUS_OK;
}

PLUGIN_STATUS decode_close(void *context)
{
    s_msg[0] = '\0';
    sprintf(s_msg, "[cmodule_util_png::close]");
    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return STATUS_OK;
}

static bool decode_offset_default(void *context, 
    const struct tilepos_t *pos, const struct tilefmt_t *fmt, size_t *offset)
{
    // no safety check for pointer here
    int i = pos->i, x = pos->x, y = pos->y;
    size_t w = fmt->w, h = fmt->h;
    size_t nbytes = fmt->nbytes;
    uint8_t bpp = fmt->bpp;

    size_t offset1 = i * nbytes;
    size_t offset2 = (x + y * w) * bpp / 8;
    *offset = offset1 + offset2;

    return true;
}

PLUGIN_STATUS decode_pixel(void *context,
    const uint8_t* data, size_t datasize, 
    const struct tilepos_t *pos, const struct tilefmt_t *fmt, 
    struct pixel_t *pixel, bool remain_index)
{
    // find decode offset 
    uint8_t bpp = fmt->bpp;
    size_t offset = 0;
    if(!decode_offset_default(context, pos, fmt, &offset)) return STATUS_RANGERROR;
    if(offset + bpp/8 >= fmt->nbytes ) return STATUS_RANGERROR;

    // try decode in different bpp
    if(bpp==32) // rgba8888
    {
        memcpy(pixel, g_img + offset, 4);
    }
    else if(bpp==24) // rgb888
    {
        memcpy(pixel, g_img + offset, 3);
        if(!remain_index) pixel->a = 255;
    }
    return STATUS_OK;
}

PLUGIN_STATUS decode_pixels(void *context, 
    const uint8_t* data, size_t datasize, 
    const struct tilefmt_t *fmt, struct pixel_t *pixels[], 
    size_t *npixel, bool remain_index)
{
    *pixels = (struct pixel_t *)g_img;
    *npixel = fmt->nbytes;
    return STATUS_OK;
}


PLUGIN_STATUS decode_pre(void *context, 
    const uint8_t* rawdata, size_t rawsize, struct tilecfg_t *cfg)
{
    s_msg[0] = '\0';

    // check size
    size_t datasize = cfg->size;
    if(!datasize)
    {
        datasize = rawsize - cfg->start;
    }
    if(datasize > rawsize - cfg->start)
    {
        datasize = rawsize - cfg->start;
    }

    // decode png
    int w, h, c;
    g_img = stbi_load_from_memory(rawdata + cfg->start, datasize, &w, &h, &c, 4);
    if(!g_img)
    {
        sprintf(s_msg + strlen(s_msg), "[cmodule_util_png::pre] not a png format\n");
        return STATUS_FAIL;
    }
    sprintf(s_msg + strlen(s_msg), "[cmodule_util_png::pre] png %dx%dx%d\n", w, h, c);

    // change tilefmt
    cfg->bpp = c * 8;
    cfg->w = w; 
    cfg->h = h;
    cfg->nrow = 1;
    cfg->nbytes = w*h*c;

    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    
    return STATUS_OK;
}

PLUGIN_STATUS decode_post(void *context, 
    const uint8_t* rawdata, size_t rawsize, struct tilecfg_t *cfg)
{
    s_msg[0] = '\0';
    sprintf(s_msg + strlen(s_msg), "[cmodule_util_png::post] \n");
    if(g_img) stbi_image_free(g_img);

    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';

    return STATUS_OK;
}

EXPORT struct tile_decoder_t decoder = {
    .version = 340, .size = sizeof(struct tile_decoder_t), 
    .msg = s_msg, .context = NULL, 
    .open = decode_open, .close = decode_close, 
    .decodeone = NULL,  .decodeall = decode_pixels,  // this example use decode_pixels
    .pre = decode_pre, .post=decode_post, 
    .sendui=NULL, .recvui=NULL, 
};