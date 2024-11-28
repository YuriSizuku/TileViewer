/**
 * implement for builtin plugin
 *   developed by devseed
 */

#include <stdio.h>
#include <string.h>
#include <cJSON.h>
#include "plugin.h"

static char s_msg[4096] = {0};
static const char* s_ui = 
"{\"name\" : \"plugin_default\",\
\"plugincfg\" : [{\
    \"name\" : \"endian\",\
    \"type\" : \"enum\",\
    \"help\" : \"the bit sequence\",\
    \"options\" : [\"little\", \"big\"],\
    \"value\": 0}\
    ]}"; 

static bool s_big_endian = false;

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

PLUGIN_STATUS decode_sendui_default(void *context, const char **buf, size_t *bufsize)
{
    s_msg[0] = '\0';
    
    *buf = s_ui;
    *bufsize = strlen(s_ui);
    sprintf(s_msg, "[plugin_builtin::sendui] send %zu bytes", *bufsize);

    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return STATUS_OK;
}

PLUGIN_STATUS decode_recvui_default(void *context, const char *buf, size_t bufsize)
{
    s_msg[0] = '\0';
    sprintf(s_msg, "[plugin_builtin::recvui] recv %zu bytes", bufsize);

    cJSON *root = cJSON_Parse(buf);
    if(!root) goto decode_recvui_default_fail;
    const cJSON* props = cJSON_GetObjectItem(root, "plugincfg");
    const cJSON* prop = NULL;
    if(!props) goto decode_recvui_default_fail;
    cJSON_ArrayForEach(prop, props)
    {
        const cJSON *name = cJSON_GetObjectItem(prop, "name");
        const cJSON *value = cJSON_GetObjectItem(prop, "value");
        if(!name) continue;
        if(!value) continue;
        if(!strcmp(name->valuestring, "endian"))
        {
            s_big_endian = value->valueint > 0;
            sprintf(s_msg, "%s, bigendian=%d", s_msg, s_big_endian);
        } 
    }

    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    cJSON_Delete(root);
    return STATUS_OK;

decode_recvui_default_fail:
    cJSON_Delete(root);
    return STATUS_FAIL;
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
    if(bpp > 8)
    {
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
    }
    else
    {
        uint8_t d = 0; // index value
        if(bpp==8) // index8
        {
            d = data[offset];
        }
        else if(bpp==3) // 3 bytes for 8 pixels
        {
            size_t nbytes = calc_tile_nbytes(fmt);
            int pixel_idx = pos->x + pos->y * fmt->w;
            offset =  pos->i * nbytes + pixel_idx / 8 * 3; // offset is incresed by 3
            uint8_t bitshift = (pixel_idx % 8) * bpp; 
            if(s_big_endian)
            {
                bitshift = 21 - bitshift; // bit big endian, 00011122 23334445 55666777
            }
            uint32_t mask = ((1<<bpp) - 1) << bitshift; 
            uint32_t d3 = *(uint32_t*)(data + offset) & 0x00ffffff;
            if(s_big_endian)
            {
                d3 = ((d3 & 0xFF) << 16) | ((d3 & 0xFF00)) | ((d3 & 0xFF0000) >> 16); // reverse byte sequence
            }
            d = (d3 & mask) >> bitshift;
        }
        else if (bpp < 8) // index4, index2, index1
        {
            int pixel_idx = pos->x + pos->y * fmt->w;
            uint8_t bitshift = (pixel_idx % (8 / bpp)) * bpp; 
            if(s_big_endian)
            {
                bitshift = 8 - bpp  - bitshift;
            }
            uint8_t mask = ((1<<bpp) - 1) << bitshift;
            d = (data[offset] & mask) >> bitshift;
        }
        
        // chose either use index value or linear palette
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
    .sendui=decode_sendui_default, .recvui=decode_recvui_default, 
};
