/**
 * defines some structue or values for plugin
 *   developed by devseed
 */

#ifndef _PLUGIN_H
#define _PLUGIN_H
#include <stdint.h>
#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

#define IN
#define OUT
#define OPTIONAL
#define REQUIRED

struct pixel_t
{
    union
    {
        struct
        {
           uint8_t r, g, b, a;
        };    
        uint32_t d;
        uint8_t v[4];
    };
};

struct tilepos_t
{
    int i; // idx th tile
    int x, y; // x, y coordinate in tile  
};

struct tilefmt_t
{
    uint32_t w, h;  // tile width, tile height
    uint8_t bpp;    // tile bpp
    uint32_t nbytes;  // bytes number in a tile
};

struct tilecfg_t
{
    uint32_t start; // tile start offset
    uint32_t size;   // whole tile size
    uint16_t nrow;   // how many tiles in a row
    union
    {
        struct 
        {
            uint32_t w, h;  // tile width, tile height
            uint8_t bpp;    // tile bpp
            uint32_t nbytes;  // bytes number in a tile
        };
       struct tilefmt_t fmt;
    };
};

enum UI_TILE_STYLE
{
    TILE_STYLE_DEFAULT = 0,
    TILE_STYLE_BOARDER = 1,
    TILE_STYLE_AUTOROW = 2
};

// tile navagation
struct tilenav_t
{
    int index; // current select position
    int offset; // tile offset in file (include start)
    int x, y; // tile start position
    bool scrollto; // scroll to the target position
};

// tile render style
struct tilestyle_t
{
    float scale; // tile scale
    long style;
    bool reset_scale; 
};

inline int calc_tile_nbytes(const struct tilefmt_t *fmt)
{
    if(!fmt) return 0;
    uint32_t nbytes = fmt->nbytes;
    if(!nbytes) nbytes = (fmt->w * fmt->h * fmt->bpp + 7) / 8;
    return nbytes;
}

typedef enum _PLUGIN_STATUS
{
    STATUS_OK = 0, 
    STATUS_FAIL, 
    STATUS_OPENERROR, // open decoder file failed 
    STATUS_SCRIPTERROR, // parsing decoder script failed
    STATUS_CALLBACKERROR,  // finding decoder callback faild
    STATUS_FORMATERROR, // target format error
    STATUS_RANGERROR, // pixel or index out of range
    STATUS_UNKNOW 
}PLUGIN_STATUS;

#define PLUGIN_SUCCESS(x) (x==STATUS_OK)

inline const char *decode_status_str(PLUGIN_STATUS x)
{
    if(x==STATUS_OK) return "STATUS_OK";
    if(x==STATUS_FAIL) return "STATUS_FAIL";
    if(x==STATUS_OPENERROR) return "STATUS_OPENERROR, open decoder file failed ";
    if(x==STATUS_SCRIPTERROR) return "STATUS_SCRIPTERROR, parsing decoder script failed";
    if(x==STATUS_CALLBACKERROR) return "STATUS_CALLBACKERROR, finding decoder callback faild";
    if(x==STATUS_FORMATERROR) return "STATUS_FORMATERROR, target format error";
    if(x==STATUS_RANGERROR) return "STATUS_RANGERROR, pixel or index out of range";
    return "STATUS_UNKNOW";
}

/**
 * open decoder
 * @return decoder context
 */
typedef PLUGIN_STATUS (*CB_decode_open)(const char *name, void **context);

/**
 * close decoder
 */
typedef PLUGIN_STATUS (*CB_decode_close)(void *context);

/**
 *  decode 1 pixel 
 * @param data, corrent decoding data
 * @param pixel out a uint32_t value
 * @param remain_index keep the origin index
 */
typedef PLUGIN_STATUS (*CB_decode_pixel)(void *context, 
    const uint8_t* data, size_t datasize, 
    const struct tilepos_t *pos, const struct tilefmt_t *fmt, 
    struct pixel_t *pixel, bool remain_index);

/**
 *  decode all pixels
 * @param data, corrent decoding data
 * @param pixels out all pixel buffer, this should be alloced by the plugin
 * @param npixel how many pixels for all tiles
 * @param remain_index keep the origin index
 */
typedef PLUGIN_STATUS (*CB_decode_pixels)(void *context, 
    const uint8_t* data, size_t datasize, 
    const struct tilefmt_t *fmt, struct pixel_t *pixels[], 
    size_t *npixel, bool remain_index);

/**
 * decode pre, post processing
 * @param rawdata rawdata of the file
 */
typedef PLUGIN_STATUS (*CB_decode_parse)(void *context, 
    const uint8_t* rawdata, size_t rawsize, struct tilecfg_t *cfg);
/**
 * make config widget to main ui, or get config from main ui
 */
typedef PLUGIN_STATUS (*CB_decode_config)(void *context, 
    char *jsonstr, size_t jsonsize);

/**
 * interface for C plugin
 */
struct tile_decoder_t
{
    uint32_t version; // required tileviewer version, for example v0.3.4 = 340
    uint32_t size; // this structure size
    void* context; // opaque pointer for decoder context, user defined struct
    const char *msg; // for passing log informations to log window
    REQUIRED CB_decode_open open; // open the decoder when loading decoder
    REQUIRED CB_decode_close close; // close the decoder when changing decoder
    REQUIRED CB_decode_pixel decodeone; // decode one pixel (fill the (i, x, y) pixel)
    REQUIRED CB_decode_pixels decodeall; // decode all pixels (if not find decodeall, it will use decodeone)
    OPTIONAL CB_decode_parse pre; // before decoding whole tiles (usually make some tmp values here)
    OPTIONAL CB_decode_parse post; // after decoding whole tiles(usually clean some tmp values here)
    OPTIONAL CB_decode_config setui; // for setting ui widget
    OPTIONAL CB_decode_config getui; // for getting ui widget
};

/**
 * if not export decoder struct, use get_decoder function instead
 */
typedef struct tile_decoder_t* (*API_get_decoder)();

#ifdef  __cplusplus
}
#endif

#endif