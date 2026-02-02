---@diagnostic disable : lowercase-global, missing-fields, undefined-global, duplicate-doc-field, undefined-field

io = require("io")
ui = require("ui")

version = "v0.2"
description = "[lua_9nine_fnt::init] lua plugin to decode 9nine fnt lz77 format"

-- global declear 
g_datap = nil --- @type lightuserdata
g_tilecfg = {} ---@type tilecfg_t
g_ntile = 0 ---@type integer
g_tilesp = nil ---@type lightuserdata

---@class fnt_t
---@field magic string
---@field version integer
---@field fsize integer
---@field distop integer -- distance between baseline and top/bottom
---@field disbottom integer

---@class fntglphy_t
---@field bearingx integer
---@field bearingy integer
---@field actualw integer
---@field actualh integer
---@field advancew integer
---@field reserve1 integer
---@field texturew integer
---@field textureh integer
---@field zsize integer
---@field ref_glphyaddr integer

g_fnt = {}
g_fntglphys = {} -- start from 1
g_glphylist = {} -- list for render tiles, remove duplicate

---@param indata lightuserdata
---@param outdata lightuserdata
---@param inoffset? integer
---@param outoffset? integer
---@param offsetbits? integer
---@return integer
function lz77_decode(indata, outdata, insize, inoffset, outoffset, offsetbits)
--[[
    FNT4 v1
    MSB  XXXXXXXX          YYYYYYYY    LSB
    val  len               backOffset
    size (16-OFFSET_BITS)  OFFSET_BITS

    index8, 8 * [val16 | val8]
--]]

    if insize == nil then insize = memsize(indata) - inoffset end
    if inoffset == nil then inoffset = 0 end
    if outoffset == nil then outoffset = 0 end
    if offsetbits == nil then offsetbits = 10 end

    local incur, outcur = inoffset, outoffset
    local outsize = memsize(outdata) - outoffset
    while incur - inoffset < insize  do
        local index8 = memreadi(indata, 1, incur)
        incur = incur + 1
        for j = 0, 8 - 1 do
            local flag = (index8 >> j) & 1
            if flag == 0 then -- direct byte output
                if(incur - inoffset >= insize) then break end -- sometimes no other bytes in the end
                local d = memreadi(indata, 1, incur)
                memwrite(outdata, d, 1, outcur)
                incur = incur + 1
                outcur = outcur + 1
            else -- seek from output
                local backseekval = memreadi(indata, 2, incur)
                backseekval = ((backseekval & 0xff) << 8) + ((backseekval >> 8) & 0xff) -- big endian
                local backoffsetmask = (1<<offsetbits) - 1
                local backlength = (backseekval >> offsetbits) + 3 -- length must larger than 3
                local backoffset = (backseekval & backoffsetmask) + 1
                for _ = 1, backlength do -- push char to output one by one
                    memwrite(outdata, outdata, 1, outcur, outcur - backoffset)
                    outcur = outcur + 1
                end
                incur = incur + 2
            end
        end
    end
    return outcur - outoffset
end

---@param p lightuserdata
---@return fnt_t, table<fntglphy_t>
function parse_fnt(p)
    FNT_FMT, FNTGLPHY_FMT = "<c4I4I4HH", "<bbBBBBBBH"
    FNT_FMT_SIZE = string.packsize(FNT_FMT)
    FNTGLPHY_FMT_SIZE = string.packsize(FNTGLPHY_FMT)
    local data = memreads(p, FNT_FMT_SIZE + 0x10000 * 4)
    local magic, version, fsize, distop, disbottom = string.unpack(FNT_FMT, data)
    local fnt = {magic=magic, version=version, fsize=fsize, distop=distop, disbottom=disbottom}
    local fntglphys = {}
    for i = 1, 0x10000  do
        local offset = string.unpack("<I4", data, FNT_FMT_SIZE + (i - 1) * 4 + 1)
        local data2 = memreads(p, FNTGLPHY_FMT_SIZE, offset)
        -- print(string.format("%d %x %x", i, offset, data2:len()))
        local bearingx, bearingy, actualw, actualh, advancew, reserve1,
            texturew, textureh, zsize = string.unpack(FNTGLPHY_FMT, data2)
        fntglphys[i] = {bearingx=bearingx, bearingy=bearingy, actualw=actualw,
            actualh=actualh, advancew=advancew, reserve1=reserve1, texturew=texturew,
            textureh=textureh, reserve2=reserve2, zsize=zsize, ref_glphyaddr=offset + FNTGLPHY_FMT_SIZE}
        -- print(string.format("%d %d %d %x", i, texturew, textureh, zsize))
    end
    return fnt, fntglphys
end

function decode_pre()
    -- get neccessory data for decoding
    g_datap = get_rawdatap()

    -- get tile information from font_00.fnt
    g_fnt, g_fntglphys = parse_fnt(g_datap)
    if(g_fnt.magic ~= "FNT4") then return false end
    local count = 0
    local tmpmap = {}
    for i, t in pairs(g_fntglphys) do -- paris index start from 1
        if tmpmap[t.ref_glphyaddr] == nil then
            tmpmap[t.ref_glphyaddr] = i
            count = count + 1
            table.insert(g_glphylist, i)
        end
    end

    g_ntile = count

    -- set the information for decoding
    g_tilecfg = {w=80, h=80, bpp=8, nbytes=1, nrow=64, size=g_ntile}
    set_tilecfg(g_tilecfg)

    log(string.format("[lua_9nine_fnt::pre] datasize=%d, w=%d h=%d bpp=%d nbytes=%d ntiles=%d",
        memsize(g_datap), g_tilecfg.w, g_tilecfg.h, g_tilecfg.bpp, g_tilecfg.nbytes, g_ntile))
    -- ui.msgbox("this plugin might take long time to decode all pixels", "notice")

    return true
end

function decode_pixels()
    local tilesize = g_tilecfg.w * g_tilecfg.h * 4
    g_tilesp = memnew(g_ntile * tilesize)
    local i = 0
    local outdatap = memnew(128 * 128 * 4)
    
    local progdlg = ui.progress_new("progress", "decode", g_ntile)
    for _, glphyi in ipairs(g_glphylist) do -- iparis index start from 1
        local insize = g_fntglphys[glphyi].zsize
        local inoffset = g_fntglphys[glphyi].ref_glphyaddr
        local outoffset = i * tilesize
        local outsize = nil
        if insize>0 then
            outsize = lz77_decode(g_datap, outdatap, insize, inoffset)
        else -- insize==0
            local initial_mip_size = g_fntglphys[glphyi].texturew*g_fntglphys[glphyi].textureh
            outsize = initial_mip_size + (initial_mip_size//4) + (initial_mip_size//16) + (initial_mip_size//64)
            if outsize~=0 then
                local unzip_bytes = memreads(g_datap, outsize, inoffset)
                memwrite(outdatap, unzip_bytes, outsize, 0)
            end
        end
        print(string.format("i=%d glphyi=%d inoffset=%x insize=%x outsize=%x %dx%d",
            i, glphyi, inoffset, insize, outsize,
            g_fntglphys[glphyi].textureh, g_fntglphys[glphyi].texturew))

        -- blit pixels
        for y = 0, g_fntglphys[glphyi].textureh -1 do
            for x = 0, g_fntglphys[glphyi].texturew - 1 do
                if x >= g_tilecfg.w then goto continue end
                if y >= g_tilecfg.h then goto continue end
                local offset1 = 4 * (y * g_tilecfg.w + x)
                local offset2 = y * g_fntglphys[glphyi].texturew + x
                local d = memreadi(outdatap, 1, offset2)
                local pixel = d + (d << 8) + (d << 16) + (255 << 24)
                memwrite(g_tilesp, pixel, 4, outoffset + offset1)
                ::continue::
            end
        end

        ui.progress_update(progdlg, i, string.format("decoding tile %d/%d", i+1, g_ntile));
        i = i + 1
        -- if i > 10 then break end
    end
    ui.progress_del(progdlg)

    memdel(outdatap)
    return g_tilesp, g_ntile * g_tilecfg.w * g_tilecfg.h, 0
end

function decode_post()
    log("[lua_9nine_fnt::post] decode finished")
    set_tilenav({index=0, offset=-1})
    set_tilestyle({scale=1.0})
    g_datap = nil
    g_ntile = 0
    g_fntglphys = {}
    g_glphylist = {}
    if g_tilesp ~= nil then
        memdel(g_tilesp)
        g_tilesp = nil
    end
    return true
end

log("   ", description, version)