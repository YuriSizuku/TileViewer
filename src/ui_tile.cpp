/**
 * implement for tile window, can scroll and scale, flexsible size
 *   developed by devseed
 * 
 */

#include <cmath>
#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include "core.hpp"
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

int TileView::ScaleV(int val)
{
    return (int)round((float)val * m_scale);
}

wxSize TileView::ScaleV(const wxSize &val)
{
    return wxSize(ScaleV(val.GetWidth()), ScaleV(val.GetHeight()));
}

int TileView::DeScaleV(int val)
{
    return (int)round((float)val / m_scale);
}

wxSize TileView::DeScaleV(const wxSize &val)
{
    return wxSize(DeScaleV(val.GetWidth()), DeScaleV(val.GetHeight()));
}

bool TileView::ScrollPos(int x, int y, enum wxOrientation orient)
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
    if(ScaleV(x) >= startx && ScaleV(x) <= endx && ScaleV(y) >= starty && ScaleV(y) <= endy)
    {
        return false;
    }
    else
    {
        if(orient==wxBOTH)
        {
            Scroll(ScaleV(x) / scrollxu, ScaleV(y) / scrollyu);
        }
        else
        {
            if(orient & wxHORIZONTAL) Scroll(ScaleV(x) / scrollxu, scrolly);
            if(orient & wxVERTICAL) Scroll(scrollx, ScaleV(y) / scrollyu);
        }
        return true;
    }
}

bool TileView::PreRender()
{
    // try decode and render at first
    if(!wxGetApp().m_tilesolver.RenderOk())
    {
        if(wxGetApp().m_tilesolver.DecodeOk())
        {
            wxGetApp().m_tilesolver.Render();
        }
        else
        {
            goto prerender_failed;
        }
    }

    if(!wxGetApp().m_tilesolver.RenderOk()) goto prerender_failed;
    m_bitmap = wxGetApp().m_tilesolver.m_bitmap;
    return true;

prerender_failed:
    m_bitmap = wxBitmap();
    SetVirtualSize(0, 0);
    return false;
}

int TileView::PreRow()
{
    if(!wxGetApp().m_tilesolver.DecodeOk()) return -1;
    
    // calculate the auto nrow
    auto windoww = GetClientSize().GetWidth();
    auto tilew = g_tilecfg.w;
    auto nrow = DeScaleV(windoww) / tilew;
    if(!nrow) nrow++;
    
    // resize nrow and prepare image
    wxGetApp().m_tilesolver.m_tilecfg.nrow = nrow;
    if(!wxGetApp().m_tilesolver.Render()) return -1;
    
    // update with new nrow
    g_tilecfg.nrow = (uint16_t)nrow;
    wxGetApp().m_configwindow->m_pg->SetPropertyValue("tilecfg.nrow", (long)nrow);
    m_bitmap = wxGetApp().m_tilesolver.m_bitmap;
    SetVirtualSize(ScaleV(m_bitmap.GetSize()));

    // sync the nav values
    g_tilenav.offset = -1;
    sync_tilenav(&g_tilenav, &g_tilecfg);
    
    return nrow;
}

bool TileView::PreBoarder()
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

bool TileView::PreStyle()
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
    SetVirtualSize(ScaleV(m_bitmap.GetSize()));
    if(g_tilestyle.style & TILE_STYLE_AUTOROW)
    {
        PreRow();
    }
    if(g_tilestyle.style & TILE_STYLE_BOARDER)
    {
        PreBoarder();
    }

    return true;
}

TileView::TileView(wxWindow *parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 
            wxBORDER_NONE | wxHSCROLL | wxVSCROLL | wxRETAINED)
{
    SetScrollbars(20, 20, 35, 20);
    SetBackgroundColour(*wxLIGHT_GREY);
    SetBackgroundStyle(wxBG_STYLE_PAINT); // use this to prevent automatic erase
}

void TileView::OnDraw(wxDC& dc)
{
    dc.Clear(); // this may cause flicker, but without clear, then blit will overlap

    int scrollxu, scrollyu; 
    int scrollx = GetScrollPos(wxHORIZONTAL);
    int scrolly = GetScrollPos(wxVERTICAL);
    GetScrollPixelsPerUnit(&scrollxu, &scrollyu);
    if(!m_bitmap.IsOk())
    {
        SetVirtualSize(0, 0);
        return;
    }

    // blit tiles, logic coord is for window
    wxMemoryDC memdc(m_bitmap);
    int logicw = memdc.GetSize().GetWidth();
    int logich = memdc.GetSize().GetHeight();
    int logicx = DeScaleV(scrollx*scrollxu);
    int logicy = DeScaleV(scrolly*scrollyu);
    int clientw = dc.GetSize().GetWidth();
    int clienth = dc.GetSize().GetHeight();
    if(fabs(m_scale - 1.f) < 0.001)
    {
        clientw = wxMin<int>(clientw, ScaleV(logicw - logicx));
        clienth = wxMin<int>(clienth, ScaleV(logich - logicy));
        dc.Blit(logicx, logicy, clientw, clienth, &memdc, logicx, logicy);
    }
    else
    {
        // blit only the window area can reduce blinking, but may cause a lit bit shift
        // slightly blit larger than the window, to prevent update remain problem
        int radius = 200; 
        logicx = wxMax<int>(logicx - radius, 0);
        logicy = wxMax<int>(logicy - radius, 0);
        clientw = wxMin<int>(clientw + ScaleV(2*radius), ScaleV(logicw - logicx));
        clienth = wxMin<int>(clienth + ScaleV(2*radius), ScaleV(logich - logicy));
        dc.StretchBlit(ScaleV(logicx), ScaleV(logicy), clientw, clienth, &memdc, 
            logicx, logicy, DeScaleV(clientw), DeScaleV(clienth));
        // dc.StretchBlit(0, 0, ScaleV(logicw), ScaleV(logich), &memdc, 0, 0, logicw, logich);
    }
    
    wxLogInfo(wxString::Format(
        "[TileView::OnDraw] draw client_rect (%d, %d, %d, %d)", 
            logicx, logicy, clientw, clienth));

    // draw select box
    dc.SetPen(*wxGREEN_PEN);
    dc.SetBrush(wxBrush(*wxGREEN, wxTRANSPARENT));
    dc.DrawRectangle(
        ScaleV(g_tilenav.x), ScaleV(g_tilenav.y), 
        ScaleV(g_tilecfg.w), ScaleV(g_tilecfg.h));
}

void TileView::OnSize(wxSizeEvent& event)
{
    wxLogInfo("[TileView::OnSize] %dx%d", 
        event.GetSize().GetWidth(), event.GetSize().GetHeight());
    
    if(g_tilestyle.style & TILE_STYLE_AUTOROW)
    {
        PreStyle();
        Refresh();
    }
}

void TileView::OnMouseLeftDown(wxMouseEvent& event)
{
    if(!m_bitmap.IsOk() || !wxGetApp().m_tilesolver.RenderOk()) return;

    auto clientpt = event.GetPosition();
    auto unscollpt = CalcUnscrolledPosition(clientpt);
    wxLogInfo("[TileView::OnMouseLeftDown] client (%d, %d), logic (%d, %d)", 
        clientpt.x, clientpt.y, unscollpt.x, unscollpt.y); 

    // send to config window for click tile
    int imgw = wxGetApp().m_tilesolver.m_bitmap.GetWidth();
    int imgh = wxGetApp().m_tilesolver.m_bitmap.GetHeight();
    int x = wxMin<int>(DeScaleV(unscollpt.x), imgw - g_tilecfg.w);
    int y = wxMin<int>(DeScaleV(unscollpt.y), imgh - g_tilecfg.h);
    int preindex = g_tilenav.index;
    g_tilenav.index = -1;
    g_tilenav.offset = -1;
    g_tilenav.x = x; g_tilenav.y = y;
    sync_tilenav(&g_tilenav, &g_tilecfg);
    g_tilenav.offset = -1; // to get the left corner of x, y
    sync_tilenav(&g_tilenav, &g_tilecfg); 
    if(preindex != g_tilenav.index)
    {
        SetFocus();
        Refresh();
        NOTIFY_UPDATE_TILENAV();
    }
}

void TileView::OnMouseWheel(wxMouseEvent& event)
{
    if(!event.ControlDown()) goto mouse_wheel_next;
    
    if(event.GetWheelRotation() < 0)
    {
        if(g_tilestyle.scale > 1) g_tilestyle.scale -= 1;
        else g_tilestyle.scale -= 0.25;
        if(g_tilestyle.scale < 0.24f)
        {
            g_tilestyle.scale = 0.25;
            return;
        }
    }
    else
    {
        if(g_tilestyle.scale >= 1) g_tilestyle.scale += 1;
        else g_tilestyle.scale += 0.25;
        if(g_tilestyle.scale > 4.1f)
        {
            g_tilestyle.scale = 4.f;
            return;
        }
    }
    PreStyle();
    Refresh();
    ScrollPos(g_tilenav.x, g_tilenav.y);
    NOTIFY_UPDATE_STATUS();

mouse_wheel_next:
    event.Skip();
}

void TileView::OnKeyDown(wxKeyEvent& event)
{
    int index = g_tilenav.index;
    int nrow = g_tilecfg.nrow;
    int col = (index - 1) % nrow;
    enum wxOrientation orient = wxBOTH;
    if(event.CmdDown() || event.ControlDown() || event.AltDown()) goto key_down_next;
    
    switch(event.GetKeyCode())
    {
        case 'H':
        case WXK_LEFT:
        if(col < 0) col += nrow;
        index = index / nrow * nrow +  col;
        orient = wxHORIZONTAL;
        break;
        case 'J':
        case WXK_DOWN:
        index += nrow;
        orient = wxVERTICAL;
        break;
        case 'K':
        case WXK_UP:        
        index -= nrow;
        orient = wxVERTICAL;
        break;
        case 'L':
        case WXK_RIGHT:
        orient = wxHORIZONTAL;
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
        ScrollPos(g_tilenav.x, g_tilenav.y, orient); 
        Refresh(); // draw select boarder on window
        NOTIFY_UPDATE_TILENAV();
        return;
    }

key_down_next:
    event.Skip();
}

TileWindow::TileWindow(wxWindow *parent) 
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
{
    DragAcceptFiles(true);
    auto sizer = new wxBoxSizer(wxVERTICAL);
    auto view = new TileView(this);
    m_view = view;
    sizer->Add(view, 1, wxEXPAND, 0);
    SetSizer(sizer);
}

void TileWindow::OnDropFile(wxDropFilesEvent& event)
{
    auto infile = wxFileName(event.GetFiles()[0]);
    wxLogMessage("[TileWindow::OnDropFile] open %s", infile.GetFullPath());
    wxGetApp().m_tilesolver.Close();
    if(!wxGetApp().m_tilesolver.Open(infile)) goto drop_file_failed;
    if(!wxGetApp().m_tilesolver.Decode(&g_tilecfg)) goto drop_file_failed;
    if(!wxGetApp().m_tilesolver.Render()) goto drop_file_failed;
    
    reset_tilenav(&g_tilenav);
    NOTIFY_UPDATE_TILENAV();
    NOTIFY_UPDATE_TILES(); // notify all
    return;

drop_file_failed:
    reset_tilenav(&g_tilenav);
    NOTIFY_UPDATE_TILENAV();
    NOTIFY_UPDATE_TILES(); // notify all
    wxMessageBox(wxString::Format("open %s failed !", infile.GetFullPath()), "error", wxICON_ERROR);
}

void TileWindow::OnUpdate(wxCommandEvent &event)
{
    m_view->PreRender();
    m_view->PreStyle();
    if(g_tilenav.scrollto) // go to the target position
    {
        m_view->ScrollPos(g_tilenav.x, g_tilenav.y);
        g_tilenav.scrollto = false;
    }

update_next:
    m_view->SetFocus();
    m_view->Refresh();
    NOTIFY_UPDATE_STATUS();
}