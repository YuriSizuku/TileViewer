#ifndef _UI_H
#define _UI_H

#include <wx/wx.h>
#include <wx/propgrid/propgrid.h>
#include "core_app.hpp"
#include "core_type.h"

enum UI_ID
{
    Menu_Log = 1000,
    Menu_Plugin,
    Menu_ScaleUp= Menu_Plugin + MAX_PLUGIN + 1,
    Menu_ScaleDown, 
    Menu_ScaleReset,
    Menu_ShowBoader, 
    Menu_AutoRow, 
    Menu_Open = wxID_OPEN,
    Menu_Close = wxID_CLOSE,
    Menu_Save = wxID_SAVE,
    Menu_About = wxID_ABOUT
};

enum UI_TILE_STYLE
{
    TILE_STYLE_DEFAULT = 0,
    TILE_STYLE_BOARDER = 1,
    TILE_STYLE_AUTOROW = 2
};

// current tile
struct tilenav_t
{
    int index; // current select position
    int offset; // tile offset in file (include start)
    int x, y; // tile start position
    bool scrollto; // scroll to the target position
};

// tile render style
struct tilestyle_t
{
    float scale; // tile scale
    long style;
    bool reset_scale; 
};

extern struct tilenav_t g_tilenav;
extern struct tilestyle_t g_tilestyle;

wxDECLARE_EVENT(EVENT_UPDATE_TILES, wxCommandEvent); // tilefmt, tilenav, tilestyle
wxDECLARE_EVENT(EVENT_UPDATE_STATUS, wxCommandEvent); // infile, pluginfile, ntile, (imgw, imgh)
wxDECLARE_EVENT(EVENT_UPDATE_PGNAV, wxCommandEvent); // tilenav
#define NOTIFY_UPDATE_TILES() wxPostEvent(wxGetApp().m_tilewindow, wxCommandEvent(EVENT_UPDATE_TILES))
#define NOTIFY_UPDATE_STATUS() wxPostEvent(wxGetApp().GetTopWindow(), wxCommandEvent(EVENT_UPDATE_STATUS))
#define NOTIFY_UPDATE_PGNAV() wxPostEvent(wxGetApp().m_configwindow, wxCommandEvent(EVENT_UPDATE_PGNAV))

class TopFrame : public wxFrame
{
public:
    TopFrame();
    void OnUpdateStatus(wxCommandEvent &event);

private:
    wxDECLARE_EVENT_TABLE();
};

class MainMenuBar : public wxMenuBar
{
public:
    MainMenuBar(wxFrame *parent);
    void OnOpen(wxCommandEvent& event);
    void OnClose(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnPlugin(wxCommandEvent& event);
    void OnScale(wxCommandEvent& event);
    void OnStyle(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnLogcat(wxCommandEvent& event);
    
private:
    wxFrame *m_parent;
    wxDECLARE_EVENT_TABLE();
};

class ConfigWindow : public wxPanel
{
public:
    void LoadTilecfg(struct tilecfg_t &cfg);
    void SaveTilecfg(struct tilecfg_t &cfg);

    ConfigWindow(wxWindow* parent);
    void OnPropertyGridChanged(wxPropertyGridEvent& event);
    void OnUpdatePgnav(wxCommandEvent& event);

    wxPropertyGrid* m_pg;

private:
    wxDECLARE_EVENT_TABLE();
};

class TileWindow;
class TileView: public wxScrolledWindow
{
public:
    // scale logic pos -> client pos
    int ScaleVal(int val);
    wxSize ScaleVal(const wxSize &val);
    // descale client pos -> logic pos
    int DeScaleVal(int val);
    wxSize DeScaleVal(const wxSize &val);
    bool ScrollPosition(int x, int y); // the pos in logical bitmap

    TileView(wxWindow *parent);
    virtual void OnDraw(wxDC& dc) wxOVERRIDE; // blit logical bitmap to window
    void OnMouseLeftDown(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnSize(wxSizeEvent& event);

    wxBitmap m_bitmap; // logical bitmap, dynamicly blit when OnDraw
    float m_scale = 1.f; // bitmap scale to window

private:
    friend class TileWindow;
    bool PreRender(); // try to get logical bitmap pre render
    int AutoRow(); // auto set nrow to fit the window on logical bitmap
    bool DrawBoarder(); // draw boarders for every tiles on logical bitmap
    bool DrawStyle();  // draw all the tile styles

    wxDECLARE_EVENT_TABLE();
};

class TileWindow: public wxPanel
{
public:
    TileWindow(wxWindow *parent);
    void OnDropFile(wxDropFilesEvent& event);
    void OnUpdate(wxCommandEvent &event);

    TileView* m_view;

private:
    wxDECLARE_EVENT_TABLE();
};

inline bool reset_tilenav(struct tilenav_t *nav)
{
    if(!nav) return false;
    nav->index = nav->offset = nav->x = nav->y = 0;
    return true;
}

inline bool sync_tilenav(struct tilenav_t *nav, struct tilecfg_t *cfg)
{
    if(!nav || !cfg) return false;
    size_t nrow = cfg->nrow;
    size_t nbytes = calc_tile_nbytes(&cfg->fmt);

    if(!nrow || !cfg->w || !cfg->h || !cfg->bpp)
    {
        nav->index = nav->offset = nav->y = nav->x = 0;
        return false;
    }

sync_tile_disp_start: 
    if(nav->index < 0 && nav->offset < 0)
    {
        nav->index = nav->y / cfg->h * nrow  + nav->x / cfg->w;
        int offset = nav->index * nbytes;
        nav->offset = offset + cfg->start;
    }
    else
    {
        if(nav->index < 0) // offset -> idx
        {
            int offset = nav->offset - cfg->start;
            nav->index = offset / nbytes;
        }
        else if(nav->offset < 0) // idx -> offset
        {
            int offset = nav->index * nbytes;
            nav->offset = offset + cfg->start;
        }
        nav->x = (nav->index % nrow) * cfg->w;
        nav->y = (nav->index / nrow) * cfg->h;
    }

    if(cfg->size > 0 && nav->offset + nbytes > cfg->size + cfg->start)
    {
        int n = cfg->size / nbytes;
        if(n <= 0) n = 1;
        nav->index = -1;
        nav->offset = cfg->start + (n -1) * nbytes;
        goto sync_tile_disp_start;
    }
    return true;
}

#endif