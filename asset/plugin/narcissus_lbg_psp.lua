---@diagnostic disable : lowercase-global
---@diagnostic disable : undefined-global
---@diagnostic disable : duplicate-doc-field

version = "v0.1"
description = "[lua::init] lua plugin to decode narcissus psp swizzle texture"

-- c types declear
---@class tilecfg_t
---@field start integer
---@field size integer
---@field nrow integer
---@field w integer
---@field h integer
---@field bpp integer
---@field nbytes integer

---@class tilenav_t
---@field start integer
---@field offset integer
---@field scrollto boolean

---@class tilenstyle_t
---@field style integer
---@field scale integer
---@field reset_scale boolean

-- capis declear 
log = log -- use log(...) to redirect to log window

---@type fun() : tilecfg_t
function get_tilecfg() return {} end -- capi

---@type fun(tilecfg_t)
function set_tilecfg(cfg) end -- capi

---@type fun() : tilenav_t
function get_tilenav() return {} end -- capi

---@type fun(tilenav_t)
function set_tilenav(nav) end -- capi

---@type fun() : tilenstyle_t
function get_tilestyle() return {} end -- capi

---@type fun(tilenstyle_t)
function set_tilestyle(style) end -- capi

---@type fun(): integer
function get_rawsize() return 0 end -- capi

-- get tiles bytes, usually used in decode_pre, and then use this to decode pixel
---@type fun(offset:integer, size: integer): string
function get_rawdata(offset, size) return "" end --capi

-- global declear 
--- @type string
g_data = nil
g_datasize = 0
---@type tilecfg_t
g_tilecfg = nil
g_lbg_map = {} -- (x, y) -> i

function lbg_deswizzle(i, w, h)
    blockw1 = 4 -- inner
    blockh1 = 8
    blockw2 = 16 -- outer
    blockh2 = h//blockh1 -- 34
    blocksize1 = blockh1 * blockw1
    blocksize2 = blocksize1 * blockh2 * blockw2

    local blockidx2 = i // blocksize2 -- outer tile idx
    local blockoffset2 = i % blocksize2 
    local blockidx1 = blockoffset2 // blocksize1 -- inner tile idx
    local blockoffset1 = blockoffset2 % blocksize1

    local blockx1 = blockoffset1 % blockw1
    local blocky1 = blockoffset1 // blockw1
    local blockx2 = blockidx1 % blockw2
    local blocky2 = blockidx1 // blockw2
    local xbase = blockidx2 * blockw2 * blockw1
    local ybase = 0
    local x = xbase + blockx2 * blockw1 + blockx1
    local y = ybase + blocky2 * blockh1 + blocky1
    
    return x, y
end

-- c callbacks implement
---@type fun() : boolean 
function decode_pre() -- callback for pre process
    --- get neccessory data for decoding
    g_tilecfg = get_tilecfg()
    g_datasize = get_rawsize() - g_tilecfg.start
    if(g_tilecfg.size > 0) then g_datasize = math.min(g_datasize, g_tilecfg.size) end
    g_data = get_rawdata(0x20, 0);


    --- prepare decoding data
    w, h = 512, 272
    for i=0, w*h do
        local x, y = lbg_deswizzle(i, w, h)
        if g_lbg_map[x] == nil then
            g_lbg_map[x] = {}
        end
        g_lbg_map[x][y] = i
    end

    --- set the information for decoding, notice that lua index from 1
    g_tilecfg.w, g_tilecfg.h = w, h
    g_tilecfg.nrow = 1
    g_tilecfg.bpp = 32
    g_tilecfg.nbytes = g_tilecfg.w * g_tilecfg.h * 4
    set_tilecfg(g_tilecfg) -- set the fmt data back to ui
    
    log(string.format("[lua::decode_pre] datasize=%d w=%d h=%d bpp=%d nbytes=%d", 
        g_data:len(), g_tilecfg.w, g_tilecfg.h, g_tilecfg.bpp, g_tilecfg.nbytes ))
    return true
end

---@type fun( i: integer, x: integer, y: integer) : integer
function decode_pixel(i, x, y) -- callback for decoding tile i, (x, y) position
    if g_lbg_map[x] == nil then
        return 0
    end
    if g_lbg_map[x][y] == nil then
        return 0
    end
    offset = 4 * g_lbg_map[x][y]
    if offset +4 >= g_datasize then return 0 end
    r, g, b, a = string.unpack("<I1I1I1I1", g_data, offset + 1)
    return r + (g << 8) + (b << 16) + (a << 24)
end

---@type fun() : boolean 
function decode_post() -- callback for post process
    log("[lua::decode_post] decode finished")
    set_tilenav({index=0, offset=-1})
    set_tilestyle({style=1, scale=1.5})
    g_data = nil
    return true
end

log(description, version)