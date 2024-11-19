/**
 * implement for main menu
 *   developed by devseed
*/

#include <wx/wx.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include "ui.hpp"
#include "core.hpp"

wxBEGIN_EVENT_TABLE(MainMenuBar, wxWindow)
    EVT_MENU(Menu_Open, MainMenuBar::OnOpen)
    EVT_MENU(Menu_Close,  MainMenuBar::OnClose)
    EVT_MENU(Menu_Save, MainMenuBar::OnSave)
    EVT_MENU_RANGE(Menu_Plugin, Menu_Plugin + MAX_PLUGIN, MainMenuBar::OnPlugin)
    EVT_MENU(Menu_ShowBoader, MainMenuBar::OnStyle)
    EVT_MENU(Menu_AutoRow, MainMenuBar::OnStyle)
    EVT_MENU(Menu_ScaleUp, MainMenuBar::OnScale)
    EVT_MENU(Menu_ScaleDown, MainMenuBar::OnScale)
    EVT_MENU(Menu_ScaleReset, MainMenuBar::OnScale)
    EVT_MENU(Menu_Log, MainMenuBar::OnLogcat)
    EVT_MENU(Menu_About, MainMenuBar::OnAbout)
wxEND_EVENT_TABLE()

MainMenuBar::MainMenuBar(wxFrame *parent) 
    : wxMenuBar()
{
    m_parent = parent;
    
    // file menu
    wxMenu *fileMenu = new wxMenu;
    fileMenu->Append(Menu_Open, "&Open...\tCtrl-O", "Open tile file");
    fileMenu->Append(Menu_Close, "&Close  \tCtrl-W", "Close tile file");
    fileMenu->Append(Menu_Save, "&Save...\tCtrl-S", "Save tile decode image to");
    fileMenu->AppendSeparator();
    
    // pixel fmt menu
    wxMenu *pluginMenu = new wxMenu;
    int i=0;
    for(auto file : wxGetApp().m_pluginfiles) 
    {
        if(file.Exists()) // check if this is extern plugin
        {
            pluginMenu->AppendRadioItem(Menu_Plugin + i,  
                "[extern] " + file.GetFullName(), // can not use the same id
                "Use extern plugin to decode tiles");
        }
        else
        {
            pluginMenu->AppendRadioItem(Menu_Plugin + i, 
                "[builtin] " + file.GetFullName(), 
                "Use builtin plugin to decode tiles");
        }

        i++;
        if (i > MAX_PLUGIN) 
        {
            wxLogWarning("[MainMenuBar::MainMenuBar] plugins %d > %d, truncate", 
                (int)wxGetApp().m_pluginfiles.size(), MAX_PLUGIN);
            break;
        }
    }
    fileMenu->AppendSubMenu(pluginMenu, "Plugin", "custom decode plugins are in ./plugin");
   
    // view menu
    wxMenu *viewMenu = new wxMenu;
    viewMenu->AppendCheckItem(Menu_AutoRow, "Auto Row\tCtrl-Q", "Automaticly set nrow when window changes");
    viewMenu->AppendCheckItem(Menu_ShowBoader, "Show Boader\tCtrl-B", "Show boader on each tile");
    viewMenu->AppendSeparator();
    viewMenu->Append(Menu_ScaleUp, "Scale Up\tCtrl-+", "Scale up tile view");
    viewMenu->Append(Menu_ScaleDown, "Scale Down\tCtrl--", "Scale down tile view");
    viewMenu->Append(Menu_ScaleReset, "Scale Reset\tCtrl-R", "reset the sacle and window size");

    // help menu
    wxMenu *helpMenu = new wxMenu;
    helpMenu->Append(Menu_About, "About\tF1", "Show about dialog");
    helpMenu->Append(Menu_Log, "&Logcat\tCtrl-L", "log view of this program");

    // menu bar and setting
    this->Append(fileMenu, "File");
    this->Append(viewMenu, "View");
    this->Append(helpMenu, "Help");
    pluginMenu->Check(Menu_Plugin + wxGetApp().m_pluginindex, true);
    this->FindItem(Menu_ShowBoader)->Check(g_tilestyle.style & TILE_STYLE_BOARDER);
    this->FindItem(Menu_AutoRow)->Check(g_tilestyle.style & TILE_STYLE_AUTOROW);
}

void MainMenuBar::OnOpen(wxCommandEvent& WXUNUSED(event))
{
    wxFileDialog filedialog(this, "Open tile file", "", "",
                       "tile file (*)|*",  wxFD_FILE_MUST_EXIST);
    if(filedialog.ShowModal()==wxID_CANCEL) return;
    auto inpath = filedialog.GetPath();
    wxLogMessage("[MainMenuBar::OnOpen] open %s", inpath);
    
    wxGetApp().m_tilesolver.Close();
    if(!wxGetApp().m_tilesolver.Open(inpath)) goto menu_open_failed;
    if(!wxGetApp().m_tilesolver.Decode(&g_tilecfg)) goto menu_open_failed;
    
    reset_tilenav(&g_tilenav);
    NOTIFY_UPDATE_TILENAV();
    NOTIFY_UPDATE_TILES(); // notify all
    return;

menu_open_failed:
    wxMessageBox(wxString::Format("open %s failed !", inpath), "error", wxICON_ERROR);
    reset_tilenav(&g_tilenav);
    NOTIFY_UPDATE_TILENAV();
    NOTIFY_UPDATE_TILES(); // notify all
}

void MainMenuBar::OnClose(wxCommandEvent& WXUNUSED(event))
{
    wxGetApp().m_tilesolver.Close();
    reset_tilenav(&g_tilenav);
    NOTIFY_UPDATE_TILENAV();
    NOTIFY_UPDATE_TILES(); // notify all
}

void MainMenuBar::OnSave(wxCommandEvent& WXUNUSED(event))
{
    wxString defaultName = wxString::Format("%s_%d_%d_%d", 
        wxGetApp().m_tilesolver.m_infile.GetName(), g_tilecfg.w, g_tilecfg.h, g_tilecfg.nrow);
    wxFileDialog filedialog(this, "Save tile decode image", "", defaultName,
        "png files (*.png)|*.png|bmp files (*.bmp)|*.bmp",  
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if(filedialog.ShowModal()==wxID_CANCEL) return;
    
    wxString outpath = filedialog.GetPath();
    wxLogMessage("[MainMenuBar::OnSave] open %s", outpath);

    bool res = wxGetApp().m_tilesolver.Save(outpath);
    if (!res)
    {
        wxMessageBox(wxString::Format("save %s failed !", outpath), "error", wxICON_ERROR);
    }           
}

void MainMenuBar::OnPlugin(wxCommandEvent& event)
{
    auto pluginidx = event.GetId() - Menu_Plugin;
    wxGetApp().m_pluginindex = pluginidx;
    auto pluginfile = wxGetApp().m_pluginfiles[pluginidx];

    wxLogMessage("[MainMenuBar::OnPlugin] change plugin index to %i (%s)", 
        pluginidx, pluginfile.GetFullName());
    if(wxGetApp().m_tilesolver.Decode(&g_tilecfg, pluginfile) < 0)  goto menu_plugin_failed;
    reset_tilenav(&g_tilenav);
    NOTIFY_UPDATE_TILENAV();
    NOTIFY_UPDATE_TILES(); // notify all
    return;

menu_plugin_failed:
    wxMessageBox(wxString::Format("plugin %s decode failed !", pluginfile.GetFullName()), "error", wxICON_ERROR);
    reset_tilenav(&g_tilenav);
    NOTIFY_UPDATE_TILENAV();
    NOTIFY_UPDATE_TILES(); // notify all
}

void MainMenuBar::OnStyle(wxCommandEvent& event)
{
    wxLogMessage("[MainMenuBar::OnStyle] %s %s", 
        this->FindItem(event.GetId())->GetItemLabel(), 
        event.GetSelection() ? "enable" : "disable");
    
    g_tilestyle.style = 0;
    if(this->FindItem(Menu_ShowBoader)->IsChecked()) g_tilestyle.style |= TILE_STYLE_BOARDER;
    if(this->FindItem(Menu_AutoRow)->IsChecked()) g_tilestyle.style |= TILE_STYLE_AUTOROW;

    NOTIFY_UPDATE_TILES(); // notify tilestyle
}

void MainMenuBar::OnScale(wxCommandEvent& event)
{
    wxLogMessage("[MainMenuBar::OnScale] %s", 
        event.GetId() == Menu_ScaleDown ? "scale down": "scale up");
    
    // scale range 0.25, 0.5, 0.75, 1, 2, 3, 4
    if(event.GetId() == Menu_ScaleDown)
    {
        if(g_tilestyle.scale > 1) g_tilestyle.scale -= 1;
        else g_tilestyle.scale -= 0.25;
        g_tilestyle.scale = wxMax<float>(g_tilestyle.scale, 0.25f);
    }
    else if(event.GetId() == Menu_ScaleUp)
    {
        if(g_tilestyle.scale >= 1) g_tilestyle.scale += 1;
        else g_tilestyle.scale += 0.25;
        g_tilestyle.scale = wxMin<float>(g_tilestyle.scale, 4.f);
    }
    else
    {
        g_tilestyle.scale = 1.f;
        g_tilestyle.reset_scale = true;
    }
    NOTIFY_UPDATE_TILES(); // notify tile style
}

void MainMenuBar::OnAbout(wxCommandEvent& WXUNUSED(event))
{
    wxMessageBox(
        wxString::Format(
            "A cross platform tool to view and analyze texture by tiles\n"
            "version %s, %s\n"
            "os %s\n"
            "(C) devseed <https://github.com/YuriSizuku>\n", 
            APP_VERSION, wxGetLibraryVersionInfo().GetVersionString(), wxGetOsDescription()),
        "About TileViewer",
        wxOK | wxICON_INFORMATION, this
    );
}

void MainMenuBar::OnLogcat(wxCommandEvent& WXUNUSED(event))
{
    wxGetApp().m_logwindow->Show(true);
}