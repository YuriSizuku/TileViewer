/**
 * implement for tile config window
 *   developed by devseed
 */

#include <wx/wx.h>
#include "ui.hpp"
#include "core.hpp"

extern struct tilecfg_t g_tilecfg;
extern struct tilenav_t g_tilenav;

wxDEFINE_EVENT(EVENT_UPDATE_TILECFG, wxCommandEvent);
wxDEFINE_EVENT(EVENT_UPDATE_TILENAV, wxCommandEvent);
wxBEGIN_EVENT_TABLE(ConfigWindow, wxWindow)
    EVT_PG_CHANGED(wxID_ANY, ConfigWindow::OnPropertyGridChanged)
    EVT_COMMAND(wxID_ANY, EVENT_UPDATE_TILECFG, ConfigWindow::OnUpdateTilecfg)
    EVT_COMMAND(wxID_ANY, EVENT_UPDATE_TILENAV, ConfigWindow::OnUpdateTilenav)
wxEND_EVENT_TABLE()

void ConfigWindow::LoadTilecfg(struct tilecfg_t &cfg)
{
    m_pg->SetPropertyValue("tilecfg.start", (long)cfg.start);
    m_pg->SetPropertyValue("tilecfg.size", (long)cfg.size);
    m_pg->SetPropertyValue("tilecfg.nrow", (long)cfg.nrow);
    m_pg->SetPropertyValue("tilecfg.w", (long)cfg.w);
    m_pg->SetPropertyValue("tilecfg.h", (long)cfg.h);
    m_pg->SetPropertyValue("tilecfg.bpp", (long)cfg.bpp);
    m_pg->SetPropertyValue("tilecfg.nbytes", (long)cfg.nbytes);
}

void ConfigWindow::SaveTilecfg(struct tilecfg_t &cfg)
{
    cfg.start = m_pg->GetPropertyValue("tilecfg.start").GetLong();
    cfg.size = m_pg->GetPropertyValue("tilecfg.size").GetLong();
    cfg.nrow = m_pg->GetPropertyValue("tilecfg.nrow").GetLong();
    cfg.w = m_pg->GetPropertyValue("tilecfg.w").GetLong();
    cfg.h = m_pg->GetPropertyValue("tilecfg.h").GetLong();
    cfg.bpp = m_pg->GetPropertyValue("tilecfg.bpp").GetLong();
    cfg.nbytes = m_pg->GetPropertyValue("tilecfg.nbytes").GetLong();
}

ConfigWindow::ConfigWindow(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
{
    //  directly use derived wxPropertyGrid not work
    auto sizer = new wxBoxSizer(wxVERTICAL);
    auto pg = new wxPropertyGrid(this, wxID_ANY, 
        wxDefaultPosition, wxDefaultSize,
        wxPG_SPLITTER_AUTO_CENTER | wxPG_DEFAULT_STYLE);
     m_pg = pg;
    sizer->Add(pg, 1, wxEXPAND, 0);
    this->SetSizer(sizer);

    // tilecfg
    auto tilecfg = new wxPropertyCategory("tilecfg");
    pg->Append(tilecfg);
    pg->AppendIn(tilecfg, new wxUIntProperty("start"));
    pg->AppendIn(tilecfg, new wxUIntProperty("size"));
    pg->AppendIn(tilecfg, new wxUIntProperty("nrow"));
    pg->AppendIn(tilecfg, new wxUIntProperty("w"));
    pg->AppendIn(tilecfg, new wxUIntProperty("h"));
    pg->AppendIn(tilecfg, new wxUIntProperty("bpp"));
    pg->AppendIn(tilecfg, new wxUIntProperty("nbytes"));
    pg->SetPropertyHelpString("tilecfg.start", "the start address of file");
    pg->SetPropertyHelpString("tilecfg.size", "the size of tiles (0 for all file)");
    pg->SetPropertyHelpString("tilecfg.nrow", "n tiles in a row");
    pg->SetPropertyHelpString("tilecfg.w", "tile width");
    pg->SetPropertyHelpString("tilecfg.h", "tile height");
    pg->SetPropertyHelpString("tilecfg.bpp", "tile bpp per pixel");
    pg->SetPropertyHelpString("tilecfg.nbytes", "tile size in byte (0 for w*h*bpp/8)");

    // tilenav
    auto tilenav = new wxPropertyCategory("tilenav");
    pg->Append(tilenav);
    pg->AppendIn(tilenav, new wxUIntProperty("offset"));
    pg->AppendIn(tilenav, new wxUIntProperty("index"));
    pg->SetPropertyHelpString("tilenav.offset", "current selected tile offset in file");
    pg->SetPropertyHelpString("tilenav.index", "current selected tile index");

    LoadTilecfg(g_tilecfg);
}

void ConfigWindow::OnPropertyGridChanged(wxPropertyGridEvent& event)
{
    auto prop = event.GetProperty();
    if(prop->GetParent()->GetName() == "tilecfg")
    {
        SaveTilecfg(g_tilecfg);
        
        // check non zero
        if(!g_tilecfg.bpp || !g_tilecfg.w || !g_tilecfg.h || !g_tilecfg.nrow)
        {
            wxMessageBox(wxString::Format(
                "value can not be 0"), "warning", wxICON_WARNING);
            if(!g_tilecfg.bpp) g_tilecfg.bpp = 8;
            if(!g_tilecfg.w) g_tilecfg.w = 24;
            if(!g_tilecfg.h) g_tilecfg.h = 24;
            if(!g_tilecfg.nrow) g_tilecfg.nrow = 32;
        }
        
        // check bpp
        if(g_tilecfg.bpp!=2 && g_tilecfg.bpp!=4 && g_tilecfg.bpp!=8 
            && g_tilecfg.bpp!=16 && g_tilecfg.bpp!=24 && g_tilecfg.bpp!=32)
        {
            wxMessageBox(wxString::Format(
                "bpp %d might failed, should be 2,4,8,16,24,32", g_tilecfg.bpp), 
                "warning", wxICON_WARNING);
        }

        // check nbytes
        if(g_tilecfg.nbytes !=0 && g_tilecfg.nbytes < g_tilecfg.w *  g_tilecfg.h * g_tilecfg.bpp / 8)
        {
            wxMessageDialog dlg(this, wxString::Format(
                "nbytes %u is less than %ux%ux%u/8, will reset to 0", 
                g_tilecfg.nbytes,  g_tilecfg.w, g_tilecfg.h, g_tilecfg.bpp), "warning", 
                wxOK | wxCANCEL | wxICON_WARNING | wxCENTRE);

            if(dlg.ShowModal() == wxID_OK) 
            {
                g_tilecfg.nbytes = 0;
                LoadTilecfg(g_tilecfg);
            }
        }

        wxGetApp().m_tilesolver.Decode(&g_tilecfg);
        g_tilenav.offset = -1; // prevent nrow chnages
        sync_tilenav(&g_tilenav, &g_tilecfg); 
        NOTIFY_UPDATE_TILES(); // notify tilecfg
    }
    else if(prop->GetParent()->GetName() == "tilenav") 
    {
        if(prop->GetName()=="offset")
        {
            g_tilenav.index = -1;
            g_tilenav.offset = prop->GetValue().GetLong();
        }
        else if(prop->GetName()=="index")
        {
            g_tilenav.offset = -1;
            g_tilenav.index = prop->GetValue().GetLong();
        }
        sync_tilenav(&g_tilenav, &g_tilecfg);
        if(wxGetApp().m_tilesolver.DecodeOk())
        {
            auto ntiles =wxGetApp().m_tilesolver.m_tiles.size(); // make sure not larger than file
            g_tilenav.index = wxMin<size_t>(g_tilenav.index, ntiles - 1);
            sync_tilenav(&g_tilenav, &g_tilecfg);
        }

        NOTIFY_UPDATE_TILENAV();
        g_tilenav.scrollto = true; 
        NOTIFY_UPDATE_TILES(); // notify tilenav
    }
    wxLogMessage(wxString::Format("[ConfigWindow::OnPropertyGridChanged] %s.%s=%s", 
        prop->GetParent()->GetName(), prop->GetName(), prop->GetValue().MakeString()));
}

void ConfigWindow::OnUpdateTilecfg(wxCommandEvent &event)
{
    LoadTilecfg(g_tilecfg);
}

void ConfigWindow::OnUpdateTilenav(wxCommandEvent &event)
{
    m_pg->SetPropertyValue("tilenav.index", (long)g_tilenav.index);
    m_pg->SetPropertyValue("tilenav.offset", (long)g_tilenav.offset);
}