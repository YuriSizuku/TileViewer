/**
 * implement for main frame
 *   developed by devseed
*/

#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/fswatcher.h>
#include "ui.hpp"
#include "core.hpp"

wxDEFINE_EVENT(EVENT_UPDATE_STATUS, wxCommandEvent);

wxBEGIN_EVENT_TABLE(TopFrame, wxFrame)
EVT_FSWATCHER(wxID_ANY, TopFrame::OnFileSystemEvent)
EVT_COMMAND(wxID_ANY, EVENT_UPDATE_STATUS, TopFrame::OnUpdateStatus)
wxEND_EVENT_TABLE()

struct tilenav_t g_tilenav = {.index=0};
struct tilestyle_t g_tilestyle = {.scale = 1.f, .style = TILE_STYLE_BOARDER};

TopFrame::TopFrame() :
    wxFrame(NULL, wxID_ANY, "TileViewer " APP_VERSION , // if init base here, can not use XRCCTRL
            wxDefaultPosition, wxSize(960, 720)) 
{
    // load resource
    wxString pluginDir = "./plugin";
#ifdef _WIN32
    auto icon = wxICON(IDI_ICON1);
    SetIcon(icon); // load build-in icon
#endif
    wxGetApp().SearchPlugins(pluginDir); // find decode plugins

    // main view
    auto topSizer = new wxBoxSizer(wxVERTICAL);
	auto topSpliter = new wxSplitterWindow(this, wxID_ANY, 
        wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_NOBORDER);
    topSpliter->SetMinimumPaneSize(1);
    topSizer->Add(topSpliter, 1, wxEXPAND, 5);
    this->SetSizer(topSizer);
    auto leftPanel = new ConfigWindow(topSpliter); 
    auto rightPanel = new TileWindow(topSpliter);
    wxGetApp().m_configwindow = leftPanel;
    wxGetApp().m_tilewindow = rightPanel;
    topSpliter->SplitVertically(leftPanel, rightPanel, 160);
    this->Layout();
    this->Centre(wxBOTH);

    // extra part
#ifdef _WIN32
    wxStaticBitmap* downimage = new wxStaticBitmap(leftPanel, wxID_ANY, icon);
    leftPanel->GetSizer()->Add(downimage, 0, wxEXPAND);
#endif
    
    // menu bar (file, view, help)
    auto *menuBar = new MainMenuBar(this);
    SetMenuBar(menuBar);

    // status bar (help, plugin, tile statistic)
    CreateStatusBar(3);
    SetStatusText("TileViewer " APP_VERSION);
    NOTIFY_UPDATE_STATUS();
}

void TopFrame::OnFileSystemEvent(wxFileSystemWatcherEvent &event)
{
    int type = event.GetChangeType();
    if(type==wxFSW_EVENT_MODIFY)
    {
        wxGetApp().m_tilesolver.Decode(&g_tilecfg, wxGetApp().m_tilesolver.m_pluginfile);
        wxGetApp().m_tilesolver.Render();
        NOTIFY_UPDATE_TILES();
    }
}

void TopFrame::OnUpdateStatus(wxCommandEvent &event)
{
    auto nameplugin = wxGetApp().m_tilesolver.m_pluginfile.GetFullName();
    auto nametile = wxGetApp().m_tilesolver.m_infile.GetFullName();
    SetStatusText(wxString::Format(
        "%s | %s", nametile, nameplugin), 1);

    auto ntile = wxGetApp().m_tilesolver.m_tiles.size();
    int imgw = 0, imgh = 0;
    if(wxGetApp().m_tilesolver.RenderOk())
    {
        imgw = wxGetApp().m_tilesolver.m_bitmap.GetWidth();
        imgh = wxGetApp().m_tilesolver.m_bitmap.GetHeight();
    }
    auto scale = g_tilestyle.scale;
    SetStatusText(wxString::Format(
        "%zu tiles | %dx%d image | %.0f%% scale", ntile, imgw, imgh, scale*100.f), 2);
}
