/**
 * implement for lua plugin
 *   developed by devseed
 */

#include <string.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "core_type.h"
#include "core_decode.h"
#include "core_plugin.h"

static char s_msg[4096] = {'\0'};
extern struct tilecfg_t g_tilecfg;
extern struct tilenav_t g_tilenav;
extern struct tilestyle_t g_tilestyle;

struct tile_decoder_t g_decoder_lua = {
    .open = decode_open_lua, .close = decode_close_lua, .decode = decode_pixel_lua, 
    .pre=decode_pre_lua, .post=decode_post_lua, .msg = s_msg, .context = NULL
};

static struct decode_context_t
{
    lua_State *L;
    const uint8_t *data;
    size_t datasize;
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
    lua_pushinteger(L, s_decode_context.datasize);
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
    
    if(offset > s_decode_context.datasize)
    {
        lua_pushnil(L);
    }
    else
    {
        if(offset + size > s_decode_context.datasize) size = s_decode_context.datasize - offset;
        if(!size) size = s_decode_context.datasize;
        lua_pushlstring(L, (const char *)s_decode_context.data + offset, size);
    }
    return 1;
}

static void register_capis(lua_State *L)
{
    lua_register(L, "get_tilecfg", capi_get_tilecfg);
    lua_register(L, "set_tilecfg", capi_set_tilecfg);
    lua_register(L, "get_tilenav", capi_get_tilenav);
    lua_register(L, "set_tilenav", capi_set_tilenav);
    lua_register(L, "get_tilestyle", capi_get_tilestyle);
    lua_register(L, "set_tilestyle", capi_set_tilestyle);
    lua_register(L, "get_rawsize", capi_get_rawsize);
    lua_register(L, "get_rawdata", capi_get_rawdata);
}

DECODE_STATUS decode_open_lua(const char *luastr, void **context)
{
    s_msg[0] = '\0';
    lua_State* L = luaL_newstate();
    if(!L) return DECODE_FAIL;
    luaL_openlibs(L);

    // load the script
    lua_register(L, "log", capi_log);
    if(luaL_dostring(L, luastr) != LUA_OK)
    {
        const char *text = lua_tostring(L, -1);
        sprintf(s_msg, "[decode_open_lua]  %s", text);
        luaL_error(L, "Error: %s\n", text);
        lua_close(L);
        return DECODE_SCRIPTERROR;
    }

    register_capis(L);
    s_decode_context.L = L;
    *context = &s_decode_context;

    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return DECODE_OK;
}

DECODE_STATUS decode_close_lua(void *context)
{
    s_msg[0] = '\0';
    struct decode_context_t* _context = (struct decode_context_t*)context;
    lua_close(_context->L);
    _context->data = NULL;
    _context->datasize = 0;

    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return DECODE_OK;
}

// function decode_pixel(i, x, y)
DECODE_STATUS decode_pixel_lua(void *context, 
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
        return DECODE_CALLBACKERROR;
    }
    lua_pushinteger(L, pos->i);
    lua_pushinteger(L, pos->x);
    lua_pushinteger(L, pos->y);
    lua_call(L, 3, 1);
    pixel->d = lua_tointeger(L, -1); // no check pixel valid here
    lua_pop(L, 1); // should pop after lua_tointeger
    
    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return DECODE_OK;
}

DECODE_STATUS decode_pre_lua(void *context, 
    const uint8_t* data, size_t datasize, struct tilecfg_t *cfg)
{
    s_msg[0] = '\0';
    struct decode_context_t* _context = (struct decode_context_t*) context;
    _context->data = data;
    _context->datasize = datasize;

    if(cfg->start > datasize) return DECODE_RANGERROR; 
    
    lua_State *L = _context->L;
    lua_getglobal(L, "decode_pre");
    if(!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        return DECODE_CALLBACKERROR;
    }
    lua_call(L, 0, 1);
    bool res = lua_toboolean(L, -1);
    lua_pop(L, 1);

    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return res ? DECODE_OK : DECODE_FAIL;
}

DECODE_STATUS decode_post_lua(void *context, 
    const uint8_t* data, size_t datasize, struct tilecfg_t *cfg)
{
    s_msg[0] = '\0';
    lua_State *L = ((struct decode_context_t*) context)->L;
    lua_getglobal(L, "decode_post");
    if(!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        return DECODE_CALLBACKERROR;
    }
    lua_call(L, 0, 1);
    bool res = lua_toboolean(L, -1);
    lua_pop(L, 1);

    if(s_msg[strlen(s_msg) - 1] =='\n') s_msg[strlen(s_msg) - 1] = '\0';
    return res ? DECODE_OK : DECODE_FAIL;
}