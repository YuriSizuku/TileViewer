---@diagnostic disable : lowercase-global, missing-fields, undefined-global, duplicate-doc-field, undefined-field

ui = require("ui")

version = "v0.1"
description = "[lua_yomawari3_nltx::init] lua plugin to decode yomawari3 swizzle texture"

-- global declear 
g_data = nil --- @type string
g_tilecfg = nil ---@type tilecfg_t
g_uijson = [[
{
    "plugincfg" : [
        {"name": "block_height", "type": "int", "value": 16}
    ]
}
]]
g_blockheight = 0
g_bytesperpixel = 4
g_progdlg = nil

function DIV_ROUND_UP(n, d)
    return (n + d - 1) // d
end

function tegrax1_deswizzle(x, y, image_width, bytes_per_pixel, base_addr, block_height)
    image_width_in_gobs = DIV_ROUND_UP(image_width * bytes_per_pixel, 64)
    gob_addr = (base_addr
        + (y//(8*block_height))*512*block_height*image_width_in_gobs
        + (x*bytes_per_pixel//64)*512*block_height
        + (y%(8*block_height)//8)*512)
    x = x * bytes_per_pixel
    addr = (gob_addr + ((x % 64) // 32) * 256 + ((y % 8) // 2) * 64
               + ((x % 32) // 16) * 32 + (y % 2) * 16 + (x % 16))
    return addr
end

function decode_pre()
    -- get neccessory data for decoding
    g_data = get_rawdata()
    g_tilecfg = get_tilecfg()

    -- set necessory data back
    g_bytesperpixel = g_data:len() // g_tilecfg.w // g_tilecfg.h
    g_tilecfg.nrow = 1
    g_tilecfg.bpp = g_bytesperpixel * 8
    set_tilecfg(g_tilecfg)

    -- get tile information from ui_openmap_3.nltx.dec
    log(string.format("[lua_yomawari3_nltx::pre] datasize=%d w=%d h=%d bpp=%d nbytes=%d",
        g_data:len(), g_tilecfg.w, g_tilecfg.h, g_tilecfg.bpp, g_tilecfg.nbytes))

    -- set other information
    g_progdlg = ui.progress_new("progress", "decoding pixels", g_tilecfg.w * g_tilecfg.h)

    return true
end

function decode_pixel(i, x, y)
    offset = tegrax1_deswizzle(x, y, g_tilecfg.w, g_bytesperpixel, 0, g_blockheight)
    if(offset + g_bytesperpixel >= g_data:len()) then return 0 end
    pixel = 0
    if(g_bytesperpixel == 4) then
        pixel = string.unpack("<I4", g_data, offset + 1)
    elseif(g_bytesperpixel == 3) then
        pixel = string.unpack("<I3", g_data, offset + 1)
    end

    -- update progress
    local done = y * g_tilecfg.w + x
    local all = g_tilecfg.w * g_tilecfg.h
    if( done % (all // 100) == 0) then
        ui.progress_update(g_progdlg, done, string.format("decoding pixels %d/%d", done,  all))
    end

    return pixel
end

function decode_post()
    log("[lua_yomawari3_nltx::post] decode finished")
    set_tilenav({index=0, offset=-1})
    set_tilestyle({scale=0.42})
    ui.progress_del(g_progdlg)
    g_data = nil
    g_progdlg = nil
    return true
end

function decode_sendui()
    return g_uijson
end

function decode_recvui(cfg)
    for i, t in ipairs(cfg.plugincfg) do
        -- log(i, t.name, t.value)
        if(t.name == "block_height") then g_blockheight = math.floor(t.value) end
    end
    return true
end

log("   ", description, version)