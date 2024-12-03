---@diagnostic disable : lowercase-global, missing-fields, undefined-global, duplicate-doc-field, undefined-field

version = "v0.1"
description = "[lua_baranoki_fnt::init] lua plugin to decode BaranoKiniBaranoSaku 4bpp non monospace font"

-- struct declear
---@class xtx_t -- 10 bytes
---@field wchar string
---@field dispw integer
---@field tilew integer
---@field tileh integer
---@field x integer
---@field y integer
---@field offset integer

-- global declear 
g_data = nil --- @type string
g_tilecfg = nil ---@type tilecfg_t
g_fontmap = {}

function decode_pre()
    --- get neccessory data for decoding
    g_tilecfg = get_tilecfg()
    g_data = get_rawdata()

    --- set the information for decoding
    local bpp, w, count = string.unpack("<I1I1I2", g_data, 1)
    g_tilecfg.w, g_tilecfg.h, g_tilecfg.bpp = w, w, bpp
    g_tilecfg.nbytes = 1
    g_tilecfg.size = count
    set_tilecfg(g_tilecfg)

    log(string.format("[lua_baranoki_fnt::pre] datasize=%d w=%d h=%d bpp=%d nbytes=%d",
        g_data:len(), g_tilecfg.w, g_tilecfg.h, g_tilecfg.bpp, g_tilecfg.nbytes))

    -- set other intormations
    for i=0,count do
        local wchar, dispw, tilew, tileh, x, y, offset = string.unpack("<I2I1I1I1I1I1I3", g_data, 0x24 + i*10 + 1)
        g_fontmap[i] = {wchar=wchar, dispw=dispw, tilew=tilew, tileh=tileh, x=x, y=y, offset=offset}
    end

    return true
end

function decode_pixel(i, x, y)
    if(g_fontmap[i]==nil) then return 0 end
    local bpp = 4 -- supposed 4bpp
    local w, h = g_fontmap[i].tilew, g_fontmap[i].tileh
    local x1, y1 = x - g_fontmap[i].x, y - g_fontmap[i].y

    if(x1 < 0 or y1 < 0 or x1 >= w or y1 >= h) then return 0 end
    local pixeli = y1*w + x1
    local offset = g_fontmap[i].offset + pixeli * bpp // 8
    if(offset >= g_data:len()) then return 0 end

    d = string.byte(g_data, offset + 1)
    if(pixeli%2 == 0) then d = (d>>4) & 0xf
    else d = d & 0xf end
    d = 255 * d // (2<<bpp-1)

    return d + (d << 8) + (d << 16) + (255 << 24)
end

function decode_post()
    log("[lua_baranoki_fnt::post] decode finished")
    set_tilenav({index=0, offset=-1})
    set_tilestyle({scale=2, style=0})
    g_data = nil
    return true
end

log("   ", description, version)