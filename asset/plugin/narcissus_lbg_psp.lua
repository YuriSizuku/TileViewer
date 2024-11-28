---@diagnostic disable : lowercase-global, missing-fields, undefined-global, duplicate-doc-field, undefined-field

version = "v0.2"
description = "[lua_narcissus_lbg::init] lua plugin to decode narcissus psp swizzle texture"

-- global declear 
--- @type string
g_data = nil
g_datasize = 0
---@type tilecfg_t
g_tilecfg = nil

g_uijson = [[
{
    "plugincfg" : [
        {"name": "blockw1", "type": "int", "value": 4}, 
        {"name": "blockh1", "type": "int", "value": 8}, 
        {"name": "blockw2", "type": "int", "value": 16}, 
        {"name": "blockh2", "type": "int", "value": 0}
    ]
}
]]
g_plugincfg = {}

g_lbgmap = {} -- (x, y) -> i

function lbg_deswizzle(i, w, h)
    blockw1 = g_plugincfg.blockw1 -- 4 -- inner
    blockh1 = g_plugincfg.blockh1 -- 8
    blockw2 = g_plugincfg.blockw2 -- 16 -- outer
    blockh2 = g_plugincfg.blockh2 -- 34
    if blockh2==0 then blockh2 = h//blockh1 end
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

function decode_pre()
    --- get neccessory data for decoding
    g_tilecfg = get_tilecfg()
    g_datasize = get_rawsize() - g_tilecfg.start
    if(g_tilecfg.size > 0) then g_datasize = math.min(g_datasize, g_tilecfg.size) end
    g_data = get_rawdata(0x20, 0);


    --- prepare decoding data
    w, h = 512, 272
    for i=0, w*h do
        local x, y = lbg_deswizzle(i, w, h)
        if g_lbgmap[x] == nil then
            g_lbgmap[x] = {}
        end
        g_lbgmap[x][y] = i
    end

    --- set the information for decoding, notice that lua index from 1
    g_tilecfg.w, g_tilecfg.h = w, h
    g_tilecfg.nrow = 1
    g_tilecfg.bpp = 32
    g_tilecfg.nbytes = g_tilecfg.w * g_tilecfg.h * 4
    set_tilecfg(g_tilecfg) -- set the fmt data back to ui
    
    log(string.format("[lua_narcissus_lbg::pre] datasize=%d w=%d h=%d bpp=%d nbytes=%d", 
        g_data:len(), g_tilecfg.w, g_tilecfg.h, g_tilecfg.bpp, g_tilecfg.nbytes ))
    return true
end

function decode_pixel(i, x, y)
    if g_lbgmap[x] == nil then
        return 0
    end
    if g_lbgmap[x][y] == nil then
        return 0
    end
    offset = 4 * g_lbgmap[x][y]
    if offset +4 >= g_datasize then return 0 end
    r, g, b, a = string.unpack("<I1I1I1I1", g_data, offset + 1)
    return r + (g << 8) + (b << 16) + (a << 24)
end

function decode_post()
    log("[lua_narcissus_lbg::post] decode finished")
    set_tilenav({index=0, offset=-1})
    set_tilestyle({style=1, scale=1.5})
    g_data = nil
    g_plugincfg = {}
    return true
end

function decode_sendui()
    return g_uijson
end

function decode_recvui(cfg)
    for i, t in ipairs(cfg.plugincfg) do
        -- log(i, t.name, t.value)
        g_plugincfg[t.name] = tonumber(t.value)
    end
    return true
end

log(description, version)