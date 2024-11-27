/**
 * Implement for the main app
 *   developed by devseed
 */

#include <map>
#include <wx/wx.h>
#include <wx/cmdline.h>
#include "ui.hpp"
#include "core.hpp"

using std::pair;
using std::map;

extern std::map<wxString, struct tile_decoder_t> g_builtin_plugin_map;

static const wxCmdLineEntryDesc s_cmd_desc[] =
{
    { wxCMD_LINE_SWITCH, "n", "nogui", "decode tiles without gui",
        wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL},
    { wxCMD_LINE_OPTION, "i", "inpath", "tile file inpath",
        wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_OPTION, "o", "outpath", "outpath for decoded file",
        wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_OPTION, "p", "plugin", "use extern plugin to decode",
        wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_OPTION, "", "plugincfg", "select plugin cfg path",
        wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_OPTION, "", "start", "tile start offset",
        wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_OPTION, "", "size", "whole tile size",
        wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_OPTION, "", "nrow", "how many tiles in a row",
        wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_OPTION, "", "width", "tile width",
        wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_OPTION, "", "height", "tile height",
        wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_OPTION, "", "bpp", "tile bpp",
        wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_OPTION, "", "nbytes", "bytes number in a tile",
        wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL },
    wxCMD_LINE_DESC_END
};

wxIMPLEMENT_APP(MainApp); // program entry

void MainApp::OnInitCmdLine(wxCmdLineParser& parser)
{
    parser.SetDesc(s_cmd_desc);
    wxApp::OnInitCmdLine(parser);
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
    std::cout << parser.GetUsageString() << std::flush;
    
    wxString val;
    long num;
    if(parser.FoundSwitch("nogui") == wxCMD_SWITCH_ON) m_usegui = false;
    else m_usegui = true;
    if(parser.Found("inpath", &val)) m_tilesolver.m_infile = val;
    if(parser.Found("outpath", &val)) m_tilesolver.m_outfile = val;
    if(parser.Found("plugin", &val)) m_tilesolver.m_pluginfile = val;
    if(parser.Found("plugincfg", &val)) m_tilesolver.m_plugincfgfile = val;
    if(parser.Found("start", &num)) g_tilecfg.start = num;
    if(parser.Found("size", &num)) g_tilecfg.size = num;
    if(parser.Found("nrow", &num)) g_tilecfg.nrow = num;
    if(parser.Found("width", &num)) g_tilecfg.w = num;
    if(parser.Found("height", &num)) g_tilecfg.h = num;
    if(parser.Found("bpp", &num)) g_tilecfg.bpp = num;
    if(parser.Found("nbytes", &num)) g_tilecfg.nbytes = num;
    
    return wxApp::OnCmdLineParsed(parser);
}

int MainApp::SearchPlugins(wxString dirpath)
{
    m_pluginfiles.clear();
    m_pluginindex = 0;
    
    // add build-in plugin
    for(auto &it : g_builtin_plugin_map)
    {
        m_pluginfiles.push_back(it.first);
    }

    if(wxDirExists(dirpath))
    {
        // add lua plugins
        wxString path = wxFindFirstFile(dirpath + "/*.lua");
        while (!path.empty())
        {
            m_pluginfiles.push_back(path);
            path = wxFindNextFile();
        }

        // add C plugins
        path = wxFindFirstFile(dirpath + "/*" + wxDynamicLibrary::GetDllExt());
        while (!path.empty())
        {
            m_pluginfiles.push_back(path);
            path = wxFindNextFile();
        }
    }

    // add cmd selected plugin
    auto& curplugin = wxGetApp().m_tilesolver.m_pluginfile;
    if(curplugin.GetFullPath().Length() > 0)
    {
        int i=0;
        for(i=0; i< m_pluginfiles.size(); i++)
        {
            if(m_pluginfiles[i] == curplugin) 
            {
                m_pluginindex = i;
                break;
            }
        }
        if(i == m_pluginfiles.size())
        {
            m_pluginindex = m_pluginfiles.size();
            m_pluginfiles.push_back(curplugin);
        }
    }

    return m_pluginfiles.size();
}

bool MainApp::Gui(wxString cmdline)
{
    auto logwindow = new wxLogWindow(nullptr, "TileViewer Logcat", false);
    m_logwindow = logwindow;
#ifdef _WIN32
    logwindow->GetFrame()->SetIcon(wxICON(IDI_ICON1));
#endif
    logwindow->GetFrame()->SetSize(wxSize(720, 540));
    logwindow->PassMessages(false); // disable pass to mesasgebox
    wxLog::SetActiveTarget(logwindow);
    wxLogMessage("[MainApp::OnInit] TileViewer " APP_VERSION " start, " + cmdline);
    wxLog::SetLogLevel(wxLOG_Message);

    auto *frame = new TopFrame();
    SetTopWindow(frame);
    frame->Show(true);

    // try to open and decode
    if(m_tilesolver.m_pluginfile.GetFullPath().Length())
    {
        m_tilesolver.m_pluginfile = m_pluginfiles[m_pluginindex];
    }
    m_tilesolver.LoadDecoder();
    if(m_tilesolver.m_infile.Exists())
    {
        m_tilesolver.Open();
        m_tilesolver.Decode(&g_tilecfg);
        m_tilesolver.Render();
        NOTIFY_UPDATE_TILES(); // notify all
        m_tilesolver.Save();
    }

    NOTIFY_UPDATE_STATUS();

    return true;
}

bool MainApp::Cli(wxString cmdline)
{
    wxLog::SetActiveTarget(new wxLogStream(&std::cout));
    wxLogMessage("[MainApp::Cli] TileViewer " APP_VERSION " start, " + cmdline);

    if(!m_tilesolver.Open())
    {
        wxLogError(wxString::Format("[MainApp::Cli] open %s failed", 
            m_tilesolver.m_infile.GetFullPath()));
        return false;
    }
    if(m_tilesolver.Decode(&g_tilecfg) <= 0)
    {
        wxLogError(wxString::Format("[MainApp::Cli] decode %s with %s failed", 
            m_tilesolver.m_infile.GetFullPath(), 
            m_tilesolver.m_pluginfile.GetFullPath()));
        return false;
    }
    if(!m_tilesolver.Render())
    {
        auto ntiles = m_tilesolver.m_tiles.size();
        auto w = m_tilesolver.m_tilecfg.w;
        auto h = m_tilesolver.m_tilecfg.h;
        wxLogError(wxString::Format("[MainApp::Cli] render %s with %zu ntiles, (%u, %u) failed", 
            m_tilesolver.m_infile.GetFullPath(), ntiles, w, h));
        return false;
    }
    if(!m_tilesolver.Save())
    {
        wxLogError(wxString::Format("[MainApp::Cli] save %s failed", 
            m_tilesolver.m_outfile.GetFullPath()));
        return false;
    }
    return true;
}

bool MainApp::OnInit()
{
    if (!wxApp::OnInit()) return false;
    wxString cmdline;
    for(int i=1; i < argc; i++)
    {
        cmdline += argv[i] + " ";
    }

    // load decode methods
    wxImage::AddHandler(new wxPNGHandler);
    // wxImage::AddHandler(new wxJPEGHandler); // on linux libjpeg.so.8 not work
    SearchPlugins("plugin"); 
    if(!m_tilesolver.m_pluginfile.GetFullPath().Length()) 
        m_tilesolver.m_pluginfile = m_pluginfiles[0];
    
    bool res = true;
    if(!m_usegui) res = Cli(cmdline);
    else res = Gui(cmdline);
    if(!res)
    {
        wxLogError("[MainApp::OnInit] init failed!");
    }
    if(!m_usegui)  Exit();

    return res;
}