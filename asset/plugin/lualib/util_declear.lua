---@diagnostic disable : lowercase-global, missing-fields, undefined-global, duplicate-doc-field

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

---@class tilestyle_t
---@field style integer
---@field scale integer
---@field reset_scale boolean

-- c apis declear 

-- use log(...) to redirect to log window
function log(...) end

-- memory manipulate with c

---@param size integer
---@return lightuserdata ...
function memnew(size) end --c api

---@param p lightuserdata
---@return boolean ...
function memdel(p) end --c api

---@param p lightuserdata
---@return integer ...
function memsize(p) end --c api

---@param p lightuserdata
---@param size? integer
---@param offset? integer
---@return integer ...
function memreadi(p, size, offset) end --c api

---@param p lightuserdata
---@param size? integer
---@param offset? integer
---@return string ...
function memreads(p, size, offset) end --c api

---@param p lightuserdata
---@param data integer|string|lightuserdata
---@param size? integer
---@param offset1 ? integer p offset
---@param offset2 ? integer data offset
---@return integer ...
function memwrite(p, data, size, offset1, offset2) end --c api

-- set or get tile informations
---@return tilecfg_t ...
function get_tilecfg() end -- c api

---@param cfg tilecfg_t
function set_tilecfg(cfg) end -- c api

---@return tilenav_t ...
function get_tilenav() end -- c api

---@param nav tilenav_t
function set_tilenav(nav) end -- c api

---@return tilestyle_t ...
function get_tilestyle() end -- c api

---@param style tilestyle_t
function set_tilestyle(style) end -- ca pi

---@return integer ...
function get_rawsize() end -- c api

-- get tiles bytes, usually used in decode_pre, and then use this to decode pixel
---@param offset? integer
---@param size? integer
---@return string ...
function get_rawdata(offset, size) end --c api

-- get the lightuserdata pointer for userptr 
---@return lightuserdata ...
function get_rawdatap() end --c api

---@return boolean ...
function decode_pre() end -- c callback

---@param i integer ith tile
---@param x integer x pos in a tile
---@param y integer y pos in a tile
---@return integer ... pixel value packed in rgba
function decode_pixel(i, x, y)  end -- c callback

---@return lightuserdata | string ... pixels memory 
---@return integer ... npixels
---@return integer ... offset
function decode_pixels() end -- c callback

---@return boolean ...
function decode_post() end -- c callback

---@return table ...
function decode_sendui() end -- c callback

---@param uicfg table
---@return boolean ...
function decode_recvui(uicfg) end -- c callback