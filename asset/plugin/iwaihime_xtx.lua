---@diagnostic disable : lowercase-global, missing-fields, undefined-global, duplicate-doc-field, undefined-field

version = "v0.1"
description = "[lua_iwaihime_xtx::init] lua plugin to decode Iwaihime 4bpp swizzle font"

-- global declear 
g_data = nil --- @type string
g_tilecfg = {} ---@type tilecfg_t
g_uijson = [[
{
    "plugincfg" : [
        {"name": "block_size", "type": "int", "value": 48}
    ]
}
]]

g_palatte = {
    0, 17, 34, 51,
    68, 85, 102, 119,
    136, 153, 170, 187,
    204, 221, 238, 255
}
g_graymap = {}
g_blocksize = 0
g_grayw, g_grayh, g_graynrow = 0, 0, 0

function get_x(i, width, level)
    local v1 = (level >> 2) + (level >> 1 >> (level >> 2))
    local v2 = i << v1
    local v3 = (v2 & 0x3F) + ((v2 >> 2) & 0x1C0) + ((v2 >> 3) & 0x1FFFFE00)
    v4 = v3 ~ (v3 >> 1) & 0xF
    return ((((level << 3) - 1) & ((v3 >> 1) ~ ((v3 ~ (v3 >> 1)) & 0xF))) >> v1) +
            ((((((v2 >> 6) & 0xFF) + ((v3 >> (v1 + 5)) & 0xFE)) & 3) +
            (((v3 >> (v1 + 7)) % (((width + 31)) >> 5)) << 2)) << 3)
end

function get_y(i, width, level)
    local v1 = (level >> 2) + (level >> 1 >> (level >> 2))
    local v2 = i << v1
    local v3 = (v2 & 0x3F) + ((v2 >> 2) & 0x1C0) + ((v2 >> 3) & 0x1FFFFE00)
    return ((v3 >> 4) & 1) + ((((v3 & ((level << 6) - 1) & -0x20) + ((((v2 & 0x3F) +
            ((v2 >> 2) & 0xC0)) & 0xF) << 1)) >> (v1 + 3)) & -2) +
            ((((v2 >> 10) & 2) + ((v3 >> (v1 + 6)) & 1) +
            (((v3 >> (v1 + 7)) // ((width + 31) >> 5)) << 2)) << 3)
end

function decode_pre()
    -- get neccessory data for decoding
    g_data = get_rawdata()

    -- get tile information from font48.xtx
    alignw, alignh, imgw, imgh, x, y = string.unpack(">I4I4I4I4I4I4", g_data, 0x8 + 1)
    local block_size = g_blocksize
    g_grayw = 2 * imgh -- replace w, h for image
    g_grayh = 2 * imgw
    g_graynrow = g_grayw // block_size

    -- set the information for decoding
    g_tilecfg.w, g_tilecfg.h = block_size, block_size
    g_tilecfg.bpp = 4
    g_tilecfg.nbytes = 0
    g_tilecfg.nrow = 32
    set_tilecfg(g_tilecfg)

    log(string.format("[lua_iwaihime_xtx::pre] datasize=%d w=%d h=%d bpp=%d nbytes=%d",
        g_data:len(), g_tilecfg.w, g_tilecfg.h, g_tilecfg.bpp, g_tilecfg.nbytes))

    -- set other intormations
    for i=0, imgw * imgh do
        abs_x = get_x(i, imgw, 2)
        abs_y = get_y(i, imgw, 2)
        if abs_y < imgh and abs_x < imgw then
            -- each 2 byte containes 4 gray pixel
            idx = i * 2
            block_x = (abs_x // block_size) * block_size
            block_y = (abs_y // block_size) * block_size
            x = abs_x % block_size
            y = abs_y % block_size
            target_y = block_y + y
            target_x1 = block_x * 4 + x; -- each block(48X48) has 4 pixels
            target_x2 = block_x * 4 + x + block_size;
            target_x3 = block_x * 4 + x + block_size * 2;
            target_x4 = block_x * 4 + x + block_size * 3;

            if g_graymap[target_y] == nil then g_graymap[target_y] = {} end
            d = string.byte(g_data, 0x20 + idx + 1)
            g_graymap[target_y][target_x1] = g_palatte[(d >> 4) + 1]
            g_graymap[target_y][target_x2] = g_palatte[(d & 0xf) + 1]
            d = string.byte(g_data, 0x20 + (idx + 1) + 1)
            g_graymap[target_y][target_x3] = g_palatte[(d >> 4) + 1]
            g_graymap[target_y][target_x4] = g_palatte[(d & 0xf) + 1]
        end
    end

    return true
end

function decode_pixel(i, x, y)
    grayy = (i // g_graynrow) * g_tilecfg.h + y
    grayx = (i % g_graynrow) * g_tilecfg.w + x

    if (g_graymap[grayy] == nil) then return 0 end
    d = g_graymap[grayy][grayx]
    if d == nil then return 0 end

    return d + (d << 8) + (d << 16) + (255 << 24)
end

function decode_post()
    log("[lua_iwaihime_xtx::post] decode finished")
    set_tilenav({index=0, offset=-1})
    set_tilestyle({scale=0.5})
    g_data = nil
    return true
end

function decode_sendui()
    return g_uijson
end

function decode_recvui(cfg)
    for i, t in ipairs(cfg.plugincfg) do
        -- log(i, t.name, t.value)
        if(t.name == "block_size") then g_blocksize = math.floor(t.value) end
    end
    return true
end

log("   ", description, version)