/**
 *  implement for lua plugin extra parts
 *   developed by devseed
 */

#include <wx/wx.h>
#include <wx/progdlg.h>
extern  "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// function ui.msgbox(msg, caption, style)
static int capi_ui_msgbox(lua_State *L)
{
    wxString msg;
    wxString caption;
    long style = 5;
    
    int nargs = lua_gettop(L);
    if(nargs > 0) msg.Append(lua_tostring(L, 1));
    if(nargs > 1) caption.Append(lua_tostring(L, 2));
    if(nargs > 2) style = lua_tointeger(L, 3);
    
    int res = wxMessageBox(msg, caption, style);
    lua_pushinteger(L, res);
    
    return 1;
}

// function  ui.progress_new(title, message, maximum, style)
static int capi_ui_progress_new(lua_State *L)
{
    wxString title;
    wxString message;
    long maximum = 100;
    int style = 6;
    
    int nargs = lua_gettop(L);
    if(nargs > 0) title.Append(lua_tostring(L, 1));
    if(nargs > 1) message.Append(lua_tostring(L, 2));
    if(nargs > 2) maximum = lua_tointeger(L, 3);
    if(nargs > 3) style = lua_tointeger(L, 4);
    auto *dlg = new wxProgressDialog(title, message, maximum, nullptr, style);
    lua_pushlightuserdata(L, static_cast<void*>(dlg));

    return 1;
}

// function ui.progress_update(p, value, message)
static int capi_ui_progress_update(lua_State *L)
{
    if(lua_gettop(L) < 1 && lua_islightuserdata(L, 1)) return 0;
    auto *dlg = static_cast<wxProgressDialog*>(lua_touserdata(L, 1));
    int value = 0;
    wxString message;

    int nargs = lua_gettop(L);
    if(nargs > 1) value = lua_tointeger(L, 2);
    if(nargs > 2) message.Append(lua_tostring(L, 3));
    bool res = dlg->Update(value, message);
    lua_pushboolean(L, res);

    return 0;
}

// function ui.progress_del(p)
static int capi_ui_progress_del(lua_State *L)
{
    if(lua_gettop(L) < 1 && lua_islightuserdata(L, 1))
    {
        lua_pushboolean(L, false);
    }
    else
    {
        auto *dlg = static_cast<wxProgressDialog*>(lua_touserdata(L, 1));
        delete dlg;
        lua_pushboolean(L, true);
    }
    return 1;
}

static const luaL_Reg uilib [] = 
{
    {"msgbox", capi_ui_msgbox},
    {"progress_new", capi_ui_progress_new},
    {"progress_update", capi_ui_progress_update},
    {"progress_del", capi_ui_progress_del},
    {NULL, NULL}
};

extern  "C" {
int luaopen_ui (lua_State *L) 
{
    luaL_newlib(L, uilib);
    return 1;
}
}