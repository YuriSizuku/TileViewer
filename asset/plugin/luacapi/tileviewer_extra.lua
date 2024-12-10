---@diagnostic disable : lowercase-global, missing-fields, undefined-global, duplicate-doc-field

---@param message string
---@param caption? string
---@param style? integer
---@return integer ...
function ui.msgbox(message, caption, style) end --c api

---@param title string
---@param message? string
---@param maximum? integer
---@param style? integer
---@return lightuserdata ...
function ui.progress_new(title, message, maximum, style) end --c api

---@param p lightuserdata
---@param value integer
---@param message? string
---@return boolean ...
function ui.progress_update(p, value, message) end --c api

---@param p lightuserdata
---@return boolean ...
function ui.progress_del(p) end --c api