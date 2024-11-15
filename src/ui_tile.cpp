/**
 * implement for tile window
 *   developed by devseed
 * 
 */

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
EVT_KEY_DOWN(TileView::OnKeyDown)
// EVT_CHAR_HOOK(TileView::OnKeyDown)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(TileWindow, wxWindow)
EVT_DROP_FILES(TileWindow::OnDropFile)
EVT_COMMAND(wxID_ANY, EVENT_UPDATE_TILES, TileWindow::OnUpdate)
wxEND_EVENT_TABLE()

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
    if(x >= startx && x <= endx && y >= starty && y <= endy)
    {
        return false;
    }
    else
    {
        Scroll(g_tilenav.x / scrollxu, g_tilenav.y / scrollyu);
        return true;
    }
}

void TileView::DrawBoarder()
{
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
}

TileView::TileView(wxWindow *parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 
            wxBORDER_NONE | wxHSCROLL | wxVSCROLL)
{
    this->SetScrollbars(20, 20, 35, 20);
    this->SetBackgroundColour(*wxLIGHT_GREY);
    // this->SetBackgroundStyle(wxBG_STYLE_PAINT); // use this to prevent automatic erase
}

void TileView::OnDraw(wxDC& dc) // when resize
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

    // draw tiles
    wxMemoryDC memdc(m_bitmap);
    wxRect rect(scrollx*scrollxu, scrolly*scrollyu, 
                dc.GetSize().GetWidth(), dc.GetSize().GetHeight());
    if(rect.x + rect.width > memdc.GetSize().GetWidth()) 
        rect.width = memdc.GetSize().GetWidth() - rect.x;
    if(rect.y + rect.height > memdc.GetSize().GetHeight()) 
        rect.height = memdc.GetSize().GetHeight() - rect.y;
    dc.Blit(rect.GetTopLeft(), rect.GetSize(), &memdc, rect.GetTopLeft());
    wxLogInfo(wxString::Format(
        "[TileView::OnDraw] draw rect (%d, %d, %d, %d)", rect.x, rect.y, rect.width, rect.height));

    // draw select box
    dc.SetPen(*wxGREEN_PEN);
    dc.SetBrush(wxBrush(*wxGREEN, wxTRANSPARENT));
    dc.DrawRectangle(g_tilenav.x, g_tilenav.y, g_tilecfg.w, g_tilecfg.h);
}

void TileView::OnSize(wxSizeEvent& event)
{
    wxLogInfo("[TileView::OnSize] %dx%d", 
        event.GetSize().GetWidth(), event.GetSize().GetHeight());
    
    auto windoww = this->GetClientSize().GetWidth();
    auto tilew = g_tilecfg.w;
    
    if(g_tilestyle.style & TILE_STYLE_AUTOROW)
    {
        if(!wxGetApp().m_tilesolver.DecodeOk()) return;
        auto nrow = windoww / tilew;
        if(!nrow) nrow++;
        wxGetApp().m_tilesolver.m_tilecfg.nrow = nrow;
        if(!wxGetApp().m_tilesolver.Render()) return;
        
        g_tilecfg.nrow = (uint16_t)nrow;
        wxGetApp().m_configwindow->m_pg->SetPropertyValue("tilecfg.nrow", (long)nrow);
        this->m_bitmap = wxGetApp().m_tilesolver.m_bitmap;
        this->SetVirtualSize(this->m_bitmap.GetSize());
        
        if(g_tilestyle.style & TILE_STYLE_BOARDER)
        {
            DrawBoarder();
        }

        g_tilenav.offset = -1;
        sync_tilenav(&g_tilenav, &g_tilecfg);

        this->Refresh();
    }
}

void TileView::OnMouseLeftDown(wxMouseEvent& event)
{
    if(!wxGetApp().m_tilesolver.RenderOk()) return;

    auto devicept = event.GetPosition();
    auto logicpt = CalcUnscrolledPosition(devicept);
    wxLogInfo("[TileView::OnMouseLeftDown] client (%d, %d), logic (%d, %d)", 
        devicept.x, devicept.y, logicpt.x, logicpt.y); 

    // send to config window for click tile
    int x = wxMin<int>(logicpt.x, wxGetApp().m_tilesolver.m_bitmap.GetWidth() - g_tilecfg.w);
    int y = wxMin<int>(logicpt.y, wxGetApp().m_tilesolver.m_bitmap.GetHeight() - g_tilecfg.h);
    g_tilenav.index = -1;
    g_tilenav.offset = -1;
    g_tilenav.x = x; g_tilenav.y = y;
    sync_tilenav(&g_tilenav, &g_tilecfg);
    g_tilenav.offset = -1; // to get the left corner of x, y
    sync_tilenav(&g_tilenav, &g_tilecfg); 
    NOTIFY_UPDATE_PGNAV();
    this->SetFocus();
    this->Refresh();
}

void TileView::OnKeyDown(wxKeyEvent& event)
{
    int index = g_tilenav.index;
    int nrow = g_tilecfg.nrow;
    if(event.CmdDown() || event.ControlDown() || event.AltDown()) goto on_key_down_next;
    
    switch(event.GetKeyCode())
    {
        case 'H':
        case WXK_LEFT:
        index = index / nrow * nrow + (index%nrow  -1) % nrow;
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
        NOTIFY_UPDATE_PGNAV();
        this->Refresh();
        return;
    }

on_key_down_next:
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
    NOTIFY_UPDATE_PGNAV();
    NOTIFY_UPDATE_TILES(); // notify all
    return;

drop_file_failed:
    wxMessageBox(wxString::Format("open %s failed !", infile.GetFullPath()), "error", wxICON_ERROR);
}

void TileWindow::OnUpdate(wxCommandEvent &event)
{
    if(wxGetApp().m_tilesolver.DecodeOk())
    {
        wxGetApp().m_tilesolver.Render();
    }

    if(!wxGetApp().m_tilesolver.RenderOk())
    {
        m_view->m_bitmap = wxBitmap();
        m_view->SetVirtualSize(0, 0);
        goto update_next;
    }

    m_view->m_bitmap = wxGetApp().m_tilesolver.m_bitmap;
    m_view->SetVirtualSize((m_view->m_bitmap.GetSize()));

    if(g_tilestyle.reset_scale)
    {
        auto tilewindow_w = m_view->m_bitmap.GetSize().GetWidth();
        auto configwindow_w =  wxGetApp().m_configwindow->GetSize().GetWidth();
        wxGetApp().GetTopWindow()->SetSize(tilewindow_w + configwindow_w + 40, 
            wxGetApp().GetTopWindow()->GetSize().GetHeight());
        g_tilestyle.reset_scale = false;
    }
    
    if(g_tilenav.scrollto)
    {
        m_view->ScrollPosition(g_tilenav.x, g_tilenav.y);
        g_tilenav.scrollto = false;
    }

    if(g_tilestyle.style & TILE_STYLE_BOARDER)
    {
        m_view->DrawBoarder();
    }

update_next:
    m_view->SetFocus();
    m_view->Refresh();
    NOTIFY_UPDATE_STATUS();
}