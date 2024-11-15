/**
 * defines some structue or values for tile
 *   developed by devseed
 */

#ifndef _CORE_VALUES_H
#define _CORE_VALUES_H
#include <stdint.h>

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

inline int calc_tile_nbytes(const struct tilefmt_t *fmt)
{
    if(!fmt) return 0;
    uint32_t nbytes = fmt->nbytes;
    if(!nbytes) nbytes = (fmt->w * fmt->h * fmt->bpp + 7) / 8;
    return nbytes;
}

#ifdef  __cplusplus
}
#endif

#endif