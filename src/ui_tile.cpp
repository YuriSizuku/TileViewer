/**
 * implement for tile window, can scroll and scale, flexsible size
 *   developed by devseed
 * 
 */

#include <cmath>
#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include "core_decode.h"
#include "ui.hpp"

extern struct tilecfg_t g_tilecfg;
extern struct tilenav_t g_tilenav;

wxDEFINE_EVENT(EVENT_UPDATE_TILES, wxCommandEvent);

wxBEGIN_EVENT_TABLE(TileView, wxWindow)
EVT_SIZE(TileView::OnSize)
EVT_LEFT_DOWN(TileView::OnMouseLeftDown)
EVT_MOUSEWHEEL(TileView::OnMouseWheel)
EVT_KEY_DOWN(TileView::OnKeyDown)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(TileWindow, wxWindow)
EVT_DROP_FILES(TileWindow::OnDropFile)
EVT_COMMAND(wxID_ANY, EVENT_UPDATE_TILES, TileWindow::OnUpdate)
wxEND_EVENT_TABLE()

int TileView::ScaleVal(int val)
{
    return (int)((float)val * m_scale);
}

wxSize TileView::ScaleVal(const wxSize &val)
{
    return wxSize(ScaleVal(val.GetWidth()), ScaleVal(val.GetHeight()));
}

int TileView::DeScaleVal(int val)
{
    return (int)((float)val / m_scale);
}

wxSize TileView::DeScaleVal(const wxSize &val)
{
    return wxSize(DeScaleVal(val.GetWidth()), DeScaleVal(val.GetHeight()));
}

bool TileView::ScrollPosition(int x, int y)
{
    int scrollxu, scrollyu; 
    GetScrollPixelsPerUnit(&scrollxu, &scrollyu);
    int scrollx = GetScrollPos(wxHORIZONTAL);
    int scrolly = GetScrollPos(wxVERTICAL);
    int clientw = GetClientSize().GetWidth();
    int clienth = GetClientSize().GetHeight();
    int startx = scrollx * scrollxu;
    int endx = startx + clientw;
    int starty = scrolly * scrollyu;
    int endy = starty + clienth;
    
    // check if the target pixel is in current client window
    if(ScaleVal(x) >= startx && ScaleVal(x) <= endx && ScaleVal(y) >= starty && ScaleVal(y) <= endy)
    {
        return false;
    }
    else
    {
        Scroll(ScaleVal(x) / scrollxu, ScaleVal(y) / scrollyu);
        return true;
    }
}

bool TileView::PreRender()
{
    // try decode and render at first
    if(wxGetApp().m_tilesolver.DecodeOk())
    {
        wxGetApp().m_tilesolver.Render();
    }

    if(!wxGetApp().m_tilesolver.RenderOk())
    {
        m_bitmap = wxBitmap();
        SetVirtualSize(0, 0);
        return false;
    }
    m_bitmap = wxGetApp().m_tilesolver.m_bitmap;
    return true;
}

int TileView::AutoRow()
{
    if(!wxGetApp().m_tilesolver.DecodeOk()) return -1;
    
    // calculate the auto nrow
    auto windoww = GetClientSize().GetWidth();
    auto tilew = g_tilecfg.w;
    auto nrow = DeScaleVal(windoww) / tilew;
    if(!nrow) nrow++;
    
    // resize nrow and prepare image
    wxGetApp().m_tilesolver.m_tilecfg.nrow = nrow;
    if(!wxGetApp().m_tilesolver.Render()) return -1;
    
    // update with new nrow
    g_tilecfg.nrow = (uint16_t)nrow;
    wxGetApp().m_configwindow->m_pg->SetPropertyValue("tilecfg.nrow", (long)nrow);
    m_bitmap = wxGetApp().m_tilesolver.m_bitmap;
    SetVirtualSize(m_bitmap.GetSize());

    // sync the nav values
    g_tilenav.offset = -1;
    sync_tilenav(&g_tilenav, &g_tilecfg);
    
    return nrow;
}

bool TileView::DrawBoarder()
{
    if(!m_bitmap.IsOk()) return false;
    wxMemoryDC memdc(m_bitmap);
    memdc.SetPen(*wxGREY_PEN);
    memdc.SetBrush(wxBrush(*wxGREEN, wxTRANSPARENT));
    int imgw = m_bitmap.GetWidth();
    int imgh = m_bitmap.GetHeight();
    int tilew = g_tilecfg.w;
    int tileh = g_tilecfg.h;
    for(int x=0; x < imgw; x += tilew)
    {
        for(int y=0; y < imgh; y += tileh)
        {
            memdc.DrawRectangle(x, y, tilew, tileh);
        }
    }
    return true;
}

bool TileView::DrawStyle()
{
    if(!m_bitmap.IsOk()) return false;
    
    // check reset window
    if(g_tilestyle.reset_scale)
    {
        auto tilewindow_w = m_bitmap.GetSize().GetWidth();
        auto configwindow_w =  wxGetApp().m_configwindow->GetSize().GetWidth();
        wxGetApp().GetTopWindow()->SetSize(tilewindow_w + configwindow_w + 40, 
            wxGetApp().GetTopWindow()->GetSize().GetHeight());
        g_tilestyle.reset_scale = false;
        m_scale = 1.f;
    }
    else
    {
        m_scale = g_tilestyle.scale;
    }

    // check tile style and render them
    SetVirtualSize(ScaleVal(m_bitmap.GetSize()));
    if(g_tilestyle.style & TILE_STYLE_AUTOROW)
    {
        AutoRow();
    }
    if(g_tilestyle.style & TILE_STYLE_BOARDER)
    {
        DrawBoarder();
    }

    return true;
}

TileView::TileView(wxWindow *parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 
            wxBORDER_NONE | wxHSCROLL | wxVSCROLL)
{
    this->SetScrollbars(20, 20, 35, 20);
    this->SetBackgroundColour(*wxLIGHT_GREY);
    // this->SetBackgroundStyle(wxBG_STYLE_PAINT); // use this to prevent automatic erase
}

void TileView::OnDraw(wxDC& dc)
{
    int scrollxu, scrollyu; 
    int scrollx = this->GetScrollPos(wxHORIZONTAL);
    int scrolly = this->GetScrollPos(wxVERTICAL);
    this->GetScrollPixelsPerUnit(&scrollxu, &scrollyu);
    if(!m_bitmap.IsOk())
    {
        this->SetVirtualSize(0, 0);
        return;
    }

    // blit tiles, logic coord is for window
    wxMemoryDC memdc(m_bitmap);
    int logicw = memdc.GetSize().GetWidth();
    int logich = memdc.GetSize().GetHeight();
    int logicx = DeScaleVal(scrollx*scrollxu);
    int logicy = DeScaleVal(scrolly*scrollyu);
    int clientw = dc.GetSize().GetWidth();
    int clienth = dc.GetSize().GetHeight();
    if(logicx + DeScaleVal(clientw) > logicw) clientw = ScaleVal(logicw - logicx);
    if(logicy + DeScaleVal(clienth) > logich) clienth = ScaleVal(logich - logicy);
    if(fabs(m_scale - 1.f) < 0.01)
    {
        dc.Blit(logicx, logicy, clientw, clienth, &memdc, logicx, logicy);
    }
    else
    {
        dc.StretchBlit(ScaleVal(logicx), ScaleVal(logicy), clientw, clienth, &memdc, 
            logicx, logicy, DeScaleVal(clientw), DeScaleVal(clienth));
    }
    
    wxLogInfo(wxString::Format(
        "[TileView::OnDraw] draw client_rect (%d, %d, %d, %d)", 
            logicx, logicy, clientw, clienth));

    // draw select box
    dc.SetPen(*wxGREEN_PEN);
    dc.SetBrush(wxBrush(*wxGREEN, wxTRANSPARENT));
    dc.DrawRectangle(ScaleVal(g_tilenav.x), ScaleVal(g_tilenav.y), 
        ScaleVal(g_tilecfg.w), ScaleVal(g_tilecfg.h));
}

void TileView::OnSize(wxSizeEvent& event)
{
    wxLogInfo("[TileView::OnSize] %dx%d", 
        event.GetSize().GetWidth(), event.GetSize().GetHeight());
    
    if(g_tilestyle.style & TILE_STYLE_AUTOROW)
    {
        DrawStyle();
        this->Refresh();
    }
}

void TileView::OnMouseLeftDown(wxMouseEvent& event)
{
    if(!m_bitmap.IsOk()) return;

    auto clientpt = event.GetPosition();
    auto unscollpt = CalcUnscrolledPosition(clientpt);
    wxLogInfo("[TileView::OnMouseLeftDown] client (%d, %d), logic (%d, %d)", 
        clientpt.x, clientpt.y, unscollpt.x, unscollpt.y); 

    // send to config window for click tile
    int imgw = wxGetApp().m_tilesolver.m_bitmap.GetWidth();
    int imgh = wxGetApp().m_tilesolver.m_bitmap.GetHeight();
    int x = wxMin<int>(DeScaleVal(unscollpt.x), imgw - g_tilecfg.w);
    int y = wxMin<int>(DeScaleVal(unscollpt.y), imgh - g_tilecfg.h);
    g_tilenav.index = -1;
    g_tilenav.offset = -1;
    g_tilenav.x = x; g_tilenav.y = y;
    sync_tilenav(&g_tilenav, &g_tilecfg);
    g_tilenav.offset = -1; // to get the left corner of x, y
    sync_tilenav(&g_tilenav, &g_tilecfg); 
    NOTIFY_UPDATE_TILENAV();
    this->SetFocus();
    this->Refresh();
}

void TileView::OnMouseWheel(wxMouseEvent& event)
{

    if(!event.ControlDown()) goto mouse_wheel_next;
    if(event.GetWheelRotation() < 0)
    {
        if(g_tilestyle.scale > 1) g_tilestyle.scale -= 1;
        else g_tilestyle.scale -= 0.25;
        g_tilestyle.scale = wxMax<float>(g_tilestyle.scale, 0.25f);
    }
    else
    {
        if(g_tilestyle.scale >= 1) g_tilestyle.scale += 1;
        else g_tilestyle.scale += 0.25;
        g_tilestyle.scale = wxMin<float>(g_tilestyle.scale, 4.f);
    }
    DrawStyle();
    NOTIFY_UPDATE_STATUS(); 
    this->Refresh();

mouse_wheel_next:
    event.Skip();
}

void TileView::OnKeyDown(wxKeyEvent& event)
{
    int index = g_tilenav.index;
    int nrow = g_tilecfg.nrow;
    int col = (index - 1) % nrow;
    if(event.CmdDown() || event.ControlDown() || event.AltDown()) goto key_down_next;
    
    switch(event.GetKeyCode())
    {
        case 'H':
        case WXK_LEFT:
        if(col < 0) col += nrow;
        index = index / nrow * nrow +  col;
        break;
        case 'J':
        case WXK_DOWN:
        index += nrow;
        break;
        case 'K':
        case WXK_UP:        
        index -= nrow;
        break;
        case 'L':
        case WXK_RIGHT:
        index = index / nrow * nrow + (index%nrow + 1) % nrow;
        break;
        case WXK_PAGEDOWN:
        ScrollPages(1);
        break;
        case WXK_PAGEUP:
        ScrollPages(-1);
        break;
    }

    if(index != g_tilenav.index)
    {
        auto ntiles = wxGetApp().m_tilesolver.m_tiles.size();
        index = wxMax<int>(index, 0);
        index = wxMin<int>(index, ntiles);
        g_tilenav.index = index;
        g_tilenav.offset = -1;
        sync_tilenav(&g_tilenav, &g_tilecfg);
        ScrollPosition(g_tilenav.x, g_tilenav.y); 
        NOTIFY_UPDATE_TILENAV();
        this->Refresh(); // draw select boarder on window
        return;
    }

key_down_next:
    event.Skip();
}

TileWindow::TileWindow(wxWindow *parent) 
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
{
    this->DragAcceptFiles(true);
    auto sizer = new wxBoxSizer(wxVERTICAL);
    auto view = new TileView(this);
    m_view = view;
    sizer->Add(view, 1, wxEXPAND, 0);
    this->SetSizer(sizer);
}

void TileWindow::OnDropFile(wxDropFilesEvent& event)
{
    auto infile = wxFileName(event.GetFiles()[0]);
    wxLogMessage("[TileWindow::OnDropFile] open %s", infile.GetFullPath());
    wxGetApp().m_tilesolver.Close();
    if(!wxGetApp().m_tilesolver.Open(infile)) goto drop_file_failed;
    if(!wxGetApp().m_tilesolver.Decode(&g_tilecfg)) goto drop_file_failed;
    
    reset_tilenav(&g_tilenav);
    NOTIFY_UPDATE_TILENAV();
    NOTIFY_UPDATE_TILES(); // notify all
    return;

drop_file_failed:
    wxMessageBox(wxString::Format("open %s failed !", infile.GetFullPath()), "error", wxICON_ERROR);
}

void TileWindow::OnUpdate(wxCommandEvent &event)
{
    m_view->PreRender();
    m_view->DrawStyle();
    if(g_tilenav.scrollto) // go to the target position
    {
        m_view->ScrollPosition(g_tilenav.x, g_tilenav.y);
        g_tilenav.scrollto = false;
    }

update_next:
    m_view->SetFocus();
    m_view->Refresh();
    NOTIFY_UPDATE_STATUS();
}