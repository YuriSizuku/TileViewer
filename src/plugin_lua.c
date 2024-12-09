/**
 * implement for lua plugin
 *   developed by devseed
 */

#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <cJSON.h>
#include "plugin.h"

static char s_msg[4096] = {'\0'};
extern struct tilecfg_t g_tilecfg;
extern struct tilenav_t g_tilenav;
extern struct tilestyle_t g_tilestyle;

struct tile_decoder_t g_decoder_lua;

struct memblock_t
{
    void *p;
    size_t n;
};

static struct decode_context_t
{
    lua_State *L;
    union
    {
        struct
        {
            const uint8_t *rawdata;
            size_t rawsize;
        };
        struct memblock_t rawblock;
    };
} s_decode_context = {NULL};


static int capi_log(lua_State* L) 
{
    int nargs = lua_gettop(L);
    for (int i=1; i <= nargs; i++) 
    {
        const char *text = luaL_tolstring(L, i, NULL); // get the string on stack
        strncat(s_msg, text, sizeof(s_msg) - strlen(s_msg) - 1);
        strcat(s_msg, " ");
        fputs(text, stdout);
        fputc(' ', stdout);
        lua_pop(L, 1); // remove the string in stack
    }
    strcat(s_msg, "\n");
    fputc('\n', stdout);
    fflush(stdout);
    return 0;
}

// function memnew(size)
static int capi_memnew(lua_State *L)
{
    if(lua_gettop(L) < 1 && !lua_isinteger(L, 1)) return 0;
    size_t size = lua_tointeger(L, 1);
    struct memblock_t *block = malloc(size + sizeof(struct memblock_t));
    block->n = size;
    block->p = (uint8_t*)block + sizeof(struct memblock_t);
    lua_pushlightuserdata(L, (void*) block);
    return 1;
}

// function memdel(p)
static int capi_memdel(lua_State *L)
{
    if(lua_gettop(L) < 1 && !lua_islightuserdata(L, 1)) return 0;
    struct memblock_t *block = lua_touserdata(L, 1);
    free(block);
    lua_pushboolean(L, true);
    return 1;
}

// function memsize(p)
static int capi_memsize(lua_State *L)
{
    if(lua_gettop(L) < 1 && !lua_islightuserdata(L, 1)) return 0;
    struct memblock_t *block = (struct memblock_t *)lua_touserdata(L, 1);
    lua_pushinteger(L, block->n);
    return 1;
}

// function memreadi(p, size, offset)
static int capi_memreadi(lua_State *L)
{
    if(lua_gettop(L) < 2 && !lua_islightuserdata(L, 1)) return 0;
    int nargs = lua_gettop(L);
    struct memblock_t *block = (struct memblock_t *)lua_touserdata(L, 1);
    
    size_t offset = 0;
    if(nargs > 2) offset = lua_tointeger(L, 3);
    if(offset >= block->n)  goto capi_memreadi_fail;
    size_t size = 1;
    if(nargs > 1) size = lua_tointeger(L, 2);
    if(offset + size > block->n)  goto capi_memreadi_fail;

    lua_Integer I = 0;
    memcpy(&I, (uint8_t*)block->p + offset, size);
    lua_pushinteger(L, I);
    return 1;

capi_memreadi_fail:
    lua_pushnil(L);
    return 1;
}

// memreads(p, size, offset) 
static int capi_memreads(lua_State *L)
{
    if(lua_gettop(L) < 1 && !lua_islightuserdata(L, 1)) return 0;
    int nargs = lua_gettop(L);
    struct memblock_t *block = (struct memblock_t *)lua_touserdata(L, 1);
    
    size_t offset = 0;
    if(nargs > 2) offset = lua_tointeger(L, 3);
    if(offset >= block->n)  goto capi_memreads_fail;
    size_t size = block->n - offset;
    if(nargs > 1) size = lua_tointeger(L, 2);
    if(offset + size > block->n)  goto capi_memreads_fail;

    lua_pushlstring(L, (uint8_t*)block->p + offset, size);
    return 1;

capi_memreads_fail:
    lua_pushnil(L);
    return 1;
}

// memwrite(p, data, size, offset1, offset2)
static int capi_memwrite(lua_State *L)
{
    if(lua_gettop(L) < 2 && !lua_islightuserdata(L, 1)) return 0;
    int nargs = lua_gettop(L);
    struct memblock_t *block = (struct memblock_t *)lua_touserdata(L, 1);
    
    size_t offset1 = 0;
    if(nargs > 3) offset1 = lua_tointeger(L, 4);
    if(offset1 >= block->n)  goto capi_memwrite_fail;
    size_t size = block->n - offset1;
    if(nargs > 2) size = lua_tointeger(L, 3);
    if(offset1 + size > block->n)  goto capi_memwrite_fail;

    if (lua_isinteger(L, 2)) // the integer can also be string
    {
        lua_Integer I = lua_tointeger(L, 2);
        memcpy((uint8_t*)block->p + offset1, &I, size);
    }
    else if(lua_isstring(L, 2))
    {
        size_t size2 = 0;
        size_t offset2 = 0;
        if(nargs > 4) offset2 = lua_tointeger(L, 5);
        const char *data = lua_tolstring(L, 2, &size2);
        if(offset2 >= size2) goto capi_memwrite_fail;
        size2 -= offset2;
        if(size > size2) size = size2;
        memcpy((uint8_t*)block->p + offset1, data + offset2, size);
    }
    else if(lua_isuserdata(L, 2))
    {
        struct memblock_t *block2 = (struct memblock_t *)lua_touserdata(L, 2);
        size_t offset2 = 0;
        if(nargs > 4) offset2 = lua_tointeger(L, 5);
        if(offset2 >= block2->n) goto capi_memwrite_fail;
        size_t size2 = block2->n - offset2;
        if(size > size2) size = size2;
        memcpy((uint8_t*)block->p + offset1, (uint8_t*)block2->p + offset2, size);
    }

    else goto capi_memwrite_fail;
    lua_pushinteger(L, size);
    return 1;

capi_memwrite_fail:
    lua_pushinteger(L, 0);
    return 1;
}

static int capi_get_tilecfg(lua_State* L)
{
    lua_newtable(L);
    lua_pushinteger(L, g_tilecfg.start);
    lua_setfield(L, -2, "start");
    lua_pushinteger(L, g_tilecfg.size);
    lua_setfield(L, -2, "size");
    lua_pushinteger(L, g_tilecfg.w);
    lua_setfield(L, -2, "w");
    lua_pushinteger(L, g_tilecfg.h);
    lua_setfield(L, -2, "h");
    lua_pushinteger(L, g_tilecfg.bpp);
    lua_setfield(L, -2, "bpp");
    lua_pushinteger(L, g_tilecfg.nbytes);
    lua_setfield(L, -2, "nbytes");
    lua_pushinteger(L, g_tilecfg.nrow);
    lua_setfield(L, -2, "nrow");
    return 1;
}

static int capi_set_tilecfg(lua_State* L)
{
    if(lua_gettop(L) < 1 && !lua_istable(L, 1)) return 0;
    
    lua_getfield(L, 1, "start");
    if(lua_isinteger(L, -1))
    {
        g_tilecfg.start = lua_tointeger(L, -1);
        
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "size");
    if(lua_isinteger(L, -1))
    {
        g_tilecfg.size = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    
    lua_getfield(L, 1, "w");
    if(lua_isinteger(L, -1))
    {
        g_tilecfg.w = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    
    lua_getfield(L, 1, "h");
    if(lua_isinteger(L, -1))
    {
        g_tilecfg.h = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    
    lua_getfield(L, 1, "bpp");
    if(lua_isinteger(L, -1))
    {
        g_tilecfg.bpp = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    
    lua_getfield(L, 1, "nbytes");
    if(lua_isinteger(L, -1))
    {
        g_tilecfg.nbytes = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    
    lua_getfield(L, 1, "nrow");
    if(lua_isinteger(L, -1))
    {
        g_tilecfg.nrow = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    return 0;
}

static int capi_get_tilenav(lua_State* L)
{
    lua_newtable(L);
    lua_pushinteger(L, g_tilenav.index);
    lua_setfield(L, -2, "index");
    lua_pushinteger(L, g_tilenav.offset);
    lua_setfield(L, -2, "offset");
    lua_pushinteger(L, g_tilenav.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, g_tilenav.y);
    lua_setfield(L, -2, "y");
    lua_pushboolean(L, g_tilenav.scrollto);
    lua_setfield(L, -2, "scrollto");
    return 1;
}

static int capi_set_tilenav(lua_State* L)
{
    if(lua_gettop(L) < 1 && !lua_istable(L, 1)) return 0;
    
    lua_getfield(L, 1, "index");
    if(lua_isinteger(L, -1))
    {
        g_tilenav.index = lua_tointeger(L, -1);
        
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "offset");
    if(lua_isinteger(L, -1))
    {
        g_tilenav.offset = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    
    lua_getfield(L, 1, "x");
    if(lua_isinteger(L, -1))
    {
        g_tilenav.x = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    
    lua_getfield(L, 1, "y");
    if(lua_isinteger(L, -1))
    {
        g_tilenav.y = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    
    lua_getfield(L, 1, "scrollto");
    if(lua_isboolean(L, -1))
    {
        g_tilenav.scrollto = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    return 0;
}

static int capi_get_tilestyle(lua_State* L)
{
    lua_newtable(L);
    lua_pushinteger(L, g_tilestyle.style);
    lua_setfield(L, -2, "style");
    lua_pushnumber(L, g_tilestyle.scale);
    lua_setfield(L, -2, "offset");
    lua_pushboolean(L, g_tilestyle.reset_scale);
    lua_setfield(L, -2, "reset_scale");
    return 1;
}

static int capi_set_tilestyle(lua_State* L)
{
    if(lua_gettop(L) < 1 && !lua_istable(L, 1)) return 0;
    
    lua_getfield(L, 1, "style");
    if(lua_isinteger(L, -1))
    {
        g_tilestyle.style = lua_tointeger(L, -1);
        
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "scale");
    if(lua_isnumber(L, -1))
    {
        g_tilestyle.scale = lua_tonumber(L, -1);
    }
    lua_pop(L, 1);
    
    lua_getfield(L, 1, "reset_scale");
    if(lua_isboolean(L, -1))
    {
        g_tilestyle.reset_scale = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    return 0;
}

// function get_rawsize()
static int capi_get_rawsize(lua_State *L)
{
    lua_pushinteger(L, s_decode_context.rawsize);
    return 1;
}

// function get_rawdata(offset, size)
static int capi_get_rawdata(lua_State *L)
{
    int nargs = lua_gettop(L);
    size_t offset =0, size = 0;
    if(nargs > 1)
    {
        size = lua_tointeger(L, 2);
    }
    if(nargs > 0)
    {
        offset = lua_tointeger(L, 1);
    }
    
    if(offset > s_decode_context.rawsize)
    {
        lua_pushnil(L);
    }
    else
    {
        if(offset + size > s_decode_context.rawsize) size = s_decode_context.rawsize - offset;
        if(!size) size = s_decode_context.rawsize;
        lua_pushlstring(L, (const char *)s_decode_context.rawdata + offset, size);
    }
    return 1;
}

static int capi_get_rawdatap(lua_State *L)
{
    lua_pushlightuserdata(L, (void*)&s_decode_context.rawblock);
    return 1;
}

static void register_capis(lua_State *L)
{
    lua_register(L, "memnew", capi_memnew);
    lua_register(L, "memdel", capi_memdel);
    lua_register(L, "memsize", capi_memsize);
    lua_register(L, "memreadi", capi_memreadi);
    lua_register(L, "memreads", capi_memreads);
    lua_register(L, "memwrite", capi_memwrite);
    lua_register(L, "get_tilecfg", capi_get_tilecfg);
    lua_register(L, "set_tilecfg", capi_set_tilecfg);
    lua_register(L, "get_tilenav", capi_get_tilenav);
    lua_register(L, "set_tilenav", capi_set_tilenav);
    lua_register(L, "get_tilestyle", capi_get_tilestyle);
    lua_register(L, "set_tilestyle", capi_set_tilestyle);
    lua_register(L, "get_rawsize", capi_get_rawsize);
    lua_register(L, "get_rawdata", capi_get_rawdata);
    lua_register(L, "get_rawdatap", capi_get_rawdatap);
}

PLUGIN_STATUS STDCALL decode_open_lua(const char *luastr, void **context)
{
    s_msg[0] = '\0';
    lua_State* L = luaL_newstate();
    if(!L) return STATUS_FAIL;
    luaL_openlibs(L);

    // load the script
    sprintf(s_msg, "[plugin_lua::open]\n");
    lua_register(L, "log", capi_log);
    if(luaL_dostring(L, luastr) != LUA_OK)
    {
        const char *text = lua_tostring(L, -1);
        sprintf(s_msg, " %s", text);
        luaL_error(L, "Error: %s\n", text);
        lua_close(L);
        return STATUS_SCRIPTERROR;
    }

    lua_getglobal(L, "decode_pixels");
    if(!lua_isfunction(L, -1))
    {
        g_decoder_lua.decodeall = NULL;
    }
    lua_pop(L, 1);

    register_capis(L);
    s_decode_context.L = L;
    *context = &s_decode_context;

    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return STATUS_OK;
}

PLUGIN_STATUS STDCALL decode_close_lua(void *context)
{
    s_msg[0] = '\0';
    sprintf(s_msg, "[plugin_lua::close]");
    struct decode_context_t* _context = (struct decode_context_t*)context;
    lua_close(_context->L);
    _context->rawdata = NULL;
    _context->rawsize = 0;

    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return STATUS_OK;
}

PLUGIN_STATUS STDCALL decode_sendui_lua(void *context, const char **buf, size_t *bufsize)
{
    s_msg[0] = '\0';
    
    lua_State *L = ((struct decode_context_t*) context)->L;
    lua_getglobal(L, "decode_sendui");
    if(!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
        return STATUS_CALLBACKERROR;
    }
    lua_call(L, 0, 1);
    *buf = lua_tolstring(L, -1, bufsize);
    lua_pop(L, 1);

    sprintf(s_msg, "[plugin_lua::sendui] send %zu bytes\n", *bufsize);
    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';

    return STATUS_OK;
}

PLUGIN_STATUS STDCALL decode_recvui_lua(void *context, const char *buf, size_t bufsize)
{
    s_msg[0] = '\0';
    sprintf(s_msg, "[plugin_lua::recvui] recv %zu bytes\n", bufsize);

    lua_State *L = ((struct decode_context_t*) context)->L;
    lua_getglobal(L, "decode_recvui");
    if(!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
        return STATUS_CALLBACKERROR;
    }

    cJSON *root = cJSON_Parse(buf);
    if(!root) goto decode_recvui_lua_fail;
    const cJSON* props = cJSON_GetObjectItem(root, "plugincfg");
    const cJSON* prop = NULL;
    if(!props) goto decode_recvui_lua_fail;
    
    int  i=1;
    lua_newtable(L); // root
    lua_newtable(L); // plugincfg
    cJSON_ArrayForEach(prop, props)
    {
        const cJSON *name = cJSON_GetObjectItem(prop, "name");
        const cJSON *value = cJSON_GetObjectItem(prop, "value");
        if(!name) continue;
        if(!value) continue;
        lua_newtable(L);
        lua_pushstring(L, name->valuestring);
        lua_setfield(L, -2, "name");
        if(value->type==cJSON_Number) lua_pushnumber(L, value->valuedouble);
        else lua_pushstring(L, value->valuestring);
        lua_setfield(L, -2, "value");
        lua_seti(L, -2, i);
        i++;
    }
    lua_setfield(L, -2, "plugincfg");
    lua_call(L, 1, 1);
    bool res = lua_toboolean(L, -1);
    lua_pop(L, 1);

    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    
    cJSON_Delete(root);
    return res ? STATUS_OK : STATUS_FAIL;

decode_recvui_lua_fail:
    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    
    cJSON_Delete(root);
    return STATUS_FAIL;
}

// function decode_pixel(i, x, y)
PLUGIN_STATUS STDCALL decode_pixel_lua(void *context, 
    const uint8_t* data, size_t datasize,
    const struct tilepos_t *pos, const struct tilefmt_t *fmt, 
    struct pixel_t *pixel, bool remain_index)
{
    s_msg[0] = '\0';
    lua_State *L = ((struct decode_context_t*) context)->L;
    lua_getglobal(L, "decode_pixel");
    if(!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
        return STATUS_CALLBACKERROR;
    }
    lua_pushinteger(L, pos->i);
    lua_pushinteger(L, pos->x);
    lua_pushinteger(L, pos->y);
    lua_call(L, 3, 1);
    pixel->d = lua_tointeger(L, -1); // no check pixel valid here
    lua_pop(L, 1); // should pop after lua_tointeger
    
    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return STATUS_OK;
}

PLUGIN_STATUS decode_pixels_lua(void *context, 
    const uint8_t* data, size_t datasize, 
    const struct tilefmt_t *fmt, struct pixel_t *pixels[], 
    size_t *npixel, bool remain_index)
{
    s_msg[0] = '\0';
    lua_State *L = ((struct decode_context_t*) context)->L;
    lua_getglobal(L, "decode_pixels");
    if(!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
        return STATUS_CALLBACKERROR;
    }

    lua_call(L, 0, 3);
    *npixel = lua_tointeger(L, 2);
    size_t offset = lua_tointeger(L, 3);
    if(lua_isuserdata(L, 1))
    {
        struct memblock_t* block = lua_touserdata(L, 1);
        *pixels = (struct pixel_t *)((uint8_t*)block->p + offset); 
    }
    else if (lua_isstring(L, 1))
    {
        const char *data = lua_tostring(L, 1);
        *pixels = (struct pixel_t *) data;
    }
    else
    {
        goto decode_pixels_fail;
    }
    lua_pop(L, 3);
    
    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return STATUS_OK;

decode_pixels_fail:
    *npixel = 0;
    *pixels = NULL; 
    return STATUS_FAIL;
}


PLUGIN_STATUS STDCALL decode_pre_lua(void *context, 
    const uint8_t* rawdata, size_t rawsize, struct tilecfg_t *cfg)
{
    s_msg[0] = '\0';
    struct decode_context_t* _context = (struct decode_context_t*) context;
    _context->rawdata = rawdata;
    _context->rawsize = rawsize;

    if(cfg->start > rawsize) return STATUS_RANGERROR; 
    
    lua_State *L = _context->L;
    lua_getglobal(L, "decode_pre");
    if(!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
        return STATUS_CALLBACKERROR;
    }
    lua_call(L, 0, 1);
    bool res = lua_toboolean(L, -1);
    lua_pop(L, 1);

    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return res ? STATUS_OK : STATUS_FAIL;
}

PLUGIN_STATUS STDCALL decode_post_lua(void *context, 
    const uint8_t* rawdata, size_t rawsize, struct tilecfg_t *cfg)
{
    s_msg[0] = '\0';
    lua_State *L = ((struct decode_context_t*) context)->L;
    lua_getglobal(L, "decode_post");
    if(!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
        return STATUS_CALLBACKERROR;
    }
    lua_call(L, 0, 1);
    bool res = lua_toboolean(L, -1);
    lua_pop(L, 1);

    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return res ? STATUS_OK : STATUS_FAIL;
}

struct tile_decoder_t g_decoder_lua = {
    .version = 340, .size = sizeof(struct tile_decoder_t), 
    .msg = s_msg, .context = NULL, 
    .open = decode_open_lua, .close = decode_close_lua, 
    .decodeone = decode_pixel_lua, .decodeall = decode_pixels_lua, 
    .pre=decode_pre_lua, .post=decode_post_lua, 
    .sendui=decode_sendui_lua, .recvui=decode_recvui_lua, 
};