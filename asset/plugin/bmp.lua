---@diagnostic disable : lowercase-global
---@diagnostic disable : undefined-global
---@diagnostic disable : duplicate-doc-field

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
g_bfOffbits = 0 -- bitmap data start position

---@type tilecfg_t
g_tilecfg = nil

-- c callbacks implement
---@type fun() : boolean 
function decode_pre() -- callback for pre process
    --- get neccessory data for decoding
    g_tilecfg = get_tilecfg()
    g_datasize = get_rawsize() - g_tilecfg.start
    if(g_tilecfg.size > 0) then 
        g_datasize = math.min(g_datasize, g_tilecfg.size)
    end
    g_data = get_rawdata(g_tilecfg.start, g_datasize);
    if(g_data:byte(1)~=0x42 and g_data:byte(2)~=0x4d) then
        log("[lua::decode_pre] invalid format for bmp!")
        return false
    end

    --- set the information for decoding, notice that lua index from 1
    g_bfOffbits  = string.unpack("<I4", g_data, 0xa + 1)
    g_tilecfg.nrow = 1
    g_tilecfg.w, g_tilecfg.h = string.unpack("<I4I4", g_data, 0x12 + 1)
    g_tilecfg.bpp = string.unpack("<I4", g_data, 0x1c + 1)
    g_tilecfg.nbytes = 0
    set_tilecfg(g_tilecfg) -- set the fmt data back to ui
    
    log(string.format("[lua::decode_pre] datasize=%d w=%d h=%d bpp=%d nbytes=%d", 
        g_data:len(), g_tilecfg.w, g_tilecfg.h, g_tilecfg.bpp, g_tilecfg.nbytes ))
    return true
end

---@type fun( i: integer, x: integer, y: integer) : integer
function decode_pixel(i, x, y) -- callback for decoding tile i, (x, y) position
    local bytes_per_pixel = g_tilecfg.bpp // 8
    local tilew = g_tilecfg.w
    local tileh = g_tilecfg.h
    local nbytes = tilew * tileh * bytes_per_pixel
    y = tileh -1 - y
    local offset = g_bfOffbits + i * nbytes + (y * tilew + x) * bytes_per_pixel
    if(offset + 4 > g_datasize) then return 0 end
    local b, g, r, a = string.unpack("<I1I1I1I1", g_data, offset + 1)
    return r + (g << 8) + (b << 16) + (a << 24)
end

---@type fun() : boolean 
function decode_post() -- callback for post process
    log("[lua::decode_post] decode finished")
    set_tilenav({index=0, offset=-1})
    -- set_tilestyle({reset_scale=true})
    g_data = nil
    return true
end


version = "v0.1"
description = "[lua::init] lua plugin to decode bmp format"
log(description, version)