---@diagnostic disable : lowercase-global, missing-fields, undefined-global, duplicate-doc-field

version = "v0.1.1"
description = "[lua_util_bmp::init] lua plugin to decode bmp format2"

-- global declear 
--- @type string
g_data = nil
g_datasize = 0
g_bfOffbits = 0 -- bitmap data start position

---@type tilecfg_t
g_tilecfg = nil

-- c callbacks implement
function decode_pre() -- callback for pre process
    --- get neccessory data for decoding
    g_tilecfg = get_tilecfg()
    g_datasize = get_rawsize() - g_tilecfg.start
    if(g_tilecfg.size > 0) then 
        g_datasize = math.min(g_datasize, g_tilecfg.size)
    end
    g_data = get_rawdata(g_tilecfg.start, g_datasize);
    if(g_data:byte(1)~=0x42 and g_data:byte(2)~=0x4d) then
        log("[lua_util_bmp::pre] invalid format for bmp!")
        return false
    end

    --- set the information for decoding, notice that lua index from 1
    g_bfOffbits  = string.unpack("<I4", g_data, 0xa + 1)
    g_tilecfg.nrow = 1
    g_tilecfg.w, g_tilecfg.h = string.unpack("<I4I4", g_data, 0x12 + 1)
    g_tilecfg.bpp = string.unpack("<I4", g_data, 0x1c + 1)
    g_tilecfg.nbytes = 0
    set_tilecfg(g_tilecfg) -- set the fmt data back to ui
    
    log(string.format("[lua_util_bmp::pre] datasize=%d w=%d h=%d bpp=%d nbytes=%d", 
        g_data:len(), g_tilecfg.w, g_tilecfg.h, g_tilecfg.bpp, g_tilecfg.nbytes ))
    return true
end

function decode_pixel(i, x, y) -- callback for decoding tile i, (x, y) position
    local bytes_per_pixel = g_tilecfg.bpp // 8
    local tilew = g_tilecfg.w
    local tileh = g_tilecfg.h
    local nbytes = tilew * tileh * bytes_per_pixel
    y = tileh -1 - y
    local offset = g_bfOffbits + i * nbytes + (y * tilew + x) * bytes_per_pixel
    if(offset + bytes_per_pixel >= g_datasize) then return 0 end
    local b, g, r, a = string.unpack("<I1I1I1I1", g_data, offset + 1)
    return r + (g << 8) + (b << 16) + (a << 24)
end

function decode_post() -- callback for post process
    log("[lua_util_bmp::post] decode finished")
    set_tilenav({index=0, offset=-1})
    g_data = nil
    return true
end

log(description, version)