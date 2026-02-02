---@diagnostic disable : lowercase-global, missing-fields, undefined-global, duplicate-doc-field, undefined-field

version = "v0.1"
description = "[lua_util_tm2::init] lua plugin to decode tim2 texture (including swizzle)"

-- global declear 
g_data = nil --- @type string
g_tilecfg = {} ---@type tilecfg_t
g_uijson = [[
{
    "plugincfg" : [
        {"name": "swizzle", "type": "bool", "value": 0}, 
        {"name": "blockw", "type": "int", "value": 16}, 
        {"name": "blockh", "type": "int", "value": 8}
    ]
}]]
g_plugincfg = {}

-- tm2 declear
---@class tm2pic_t
---@field size_total integer
---@field size_palette integer
---@field size_image integer
---@field size_header integer
---@field count_color integer
---@field format integer
---@field count_mipmap integer
---@field type_clutcolor integer
---@field type_imagecolor integer
---@field width integer
---@field height integer
---@field reg_gstex1 integer
---@field reg_gstex2 integer
---@field reg_gsflag integer
---@field reg_gsclut integer

---@class tm2_t
---@field magic string
---@field version integer
---@field format integer
---@field count integer
---@field reserved integer

COLOR_TYPE = {
    UNDEFINED = 0,
    A1B5G5R5 = 1,
    X8B8G8R8 = 2,
    A8B8G8R8 = 3,
    INDEX4 = 4,
    INDEX8 = 5,
}

g_tm2 = {} ---@type tm2_t
g_tm2pic = {} ---@type tm2pic_t
g_dataoffset = 0 ---@type integer
g_palatte = nil
g_swizzlemap = {}

function swizzle_tm2(x, y, w, blockw, blockh)
    idx = x + w*y
    blocksize = blockw * blockh
    blockline = w // blockw
    blockidx = idx // blocksize
    blocky = blockidx // blockline
    blockx = blockidx % blockline

    blockinneridx = idx % blocksize
    blockinnery = blockinneridx // blockw
    blockinnerx = blockinneridx % blockw

    x2 = blockx * blockw + blockinnerx
    y2 = blocky * blockh + blockinnery
    return x2, y2
end

function deinterlace_palatte(palatte)
    parts = (#palatte + 1 ) // 32
    stripes = 2
    colors = 8
    blocks = 2

    newpallate = {}
    i = 0
    for part = 0, parts - 1 do
        for block = 0, blocks - 1 do
            for stripe = 0, stripes - 1 do
                for color = 0, colors - 1 do
                    i2 = part * colors * stripes * blocks + block * colors + stripe * stripes * colors + color
                    newpallate[i] = palatte[i2]
                    -- print(part, block, stripe, color, i, i2)
                    i = i + 1
                end
            end
        end
    end

    return newpallate
end

function parse_tm2(data)
    TM2_FMT, TM2PIC_FMT = "<c4BBHI8", "<I4I4I4HHBBBBHHI8I8I4I4"
    local magic, version, format, count, reversed = string.unpack(TM2_FMT, data)
    local tm2  = {magic=magic, version=version, format=format, count=count, reversed=reversed}
    local tm2picoffset = string.packsize(TM2_FMT)
    local size_total, size_palette, size_image, size_header,
        count_color, format, count_mipmap,
        type_clutcolor, type_imagecolor, width, height,
        reg_gstex1, reg_gstex2, reg_gsflag, reg_gsclut = string.unpack(TM2PIC_FMT, data, tm2picoffset + 1)
    local tm2pic = {
        size_total=size_total, size_palette=size_palette,
        size_image=size_image, size_header=size_header,
        count_color=count_color, format=format, count_mipmap=count_mipmap,
        type_clutcolor=type_clutcolor, type_imagecolor=type_imagecolor,
        width=width, height=height,
        reg_gstex1=reg_gstex1, reg_gstex2=reg_gstex2,
        reg_gsflag=reg_gsflag, reg_gsclut=reg_gsclut
    }
    local dataoffset = tm2picoffset + string.packsize(TM2PIC_FMT)
    return tm2, tm2pic, dataoffset
end

function decode_pre()
    -- get neccessory data for decoding
    g_data = get_rawdata()

    -- get tile information from tm2
    g_tm2, g_tm2pic, g_dataoffset = parse_tm2(g_data)
    if g_tm2.magic ~= "TIM2" then
        log("Not a tim2 format file")
        return false
    end

    -- set the information for decoding
    g_tilecfg.w, g_tilecfg.h = g_tm2pic.width, g_tm2pic.height
    g_tilecfg.bpp = math.ceil(math.log(g_tm2pic.count_color, 2))
    g_tilecfg.nbytes = 0
    g_tilecfg.nrow = 1
    set_tilecfg(g_tilecfg)

    log(string.format("[lua_util_tm2::pre] datasize=%d w=%d h=%d bpp=%d nbytes=%d",
        g_data:len(), g_tilecfg.w, g_tilecfg.h, g_tilecfg.bpp, g_tilecfg.nbytes))

    -- set other information
    if g_tm2pic.count_color <= 256 then --palatte
        g_palatte = {}
        local offsetpalatte = g_dataoffset + g_tm2pic.size_image
        for i=0, g_tm2pic.count_color - 1 do
            a, r, g, b = string.unpack("<BBBB", g_data, offsetpalatte + i*4)
            g_palatte[i] = r + (g << 8) + (b << 16) + (a << 24)
            -- print(string.format(" palatte[%d] %x", i, g_palatte[i]))
        end

        if g_tm2pic.type_clutcolor & 0x80 == 0 then
           g_palatte = deinterlace_palatte(g_palatte)
        else

        end
    end
    for y= 0, g_tm2pic.height do -- swizzle map
        for x= 0, g_tm2pic.width do
            idx1 = y * g_tm2pic.width + x
            x2, y2 = swizzle_tm2(x, y, g_tm2pic.width, g_plugincfg.blockw, g_plugincfg.blockh)
            idx2 = y2 * g_tm2pic.width + x2
            g_swizzlemap[idx2] = idx
        end
    end

    return true
end

function decode_pixel(i, x, y)
    pixel = 0
    color_type = g_tm2pic.type_imagecolor
    idx = y * g_tm2pic.width + x

    -- check swizzle
    if g_plugincfg.swizzle then
        idx = g_swizzlemap[idx]
    end
    if idx == nil then return 0 end

    -- decode in different formats
    if(color_type == COLOR_TYPE.INDEX4) then -- not tested yet
        d = string.byte(g_data, g_dataoffset + idx / 2 + 1)
        if idx % 2 then d = d >> 4
        else  d = d & 0xf end
        pixel = g_palatte[d]
    elseif (color_type == COLOR_TYPE.INDEX8) then
        d = string.byte(g_data, g_dataoffset + idx + 1)
        pixel = g_palatte[d]
    elseif (color_type == COLOR_TYPE.A8B8G8R8) then -- not tested yet
        pixel = string.unpack(g_data, ">I4", g_dataoffset + idx * 4 + 1)
    end

    return pixel
end

function decode_post()
    log("[lua_util_tm2::post] decode finished")
    set_tilenav({index=0, offset=-1})
    set_tilestyle({scale=1.5})
    g_data = nil
    return true
end

function decode_sendui()
    return g_uijson
end

function decode_recvui(cfg)
    for i, t in ipairs(cfg.plugincfg) do
        -- log(i, t.name, t.value)
        if(t.name == "swizzle") then g_plugincfg.swizzle = t.value > 0 end
        if(t.name == "blockw") then g_plugincfg.blockw = math.floor(t.value) end
        if(t.name == "blockh") then g_plugincfg.blockh = math.floor(t.value) end
    end
    return true
end

log("   ", description, version)