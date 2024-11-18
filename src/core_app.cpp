/**
 * implement the major methods for tile viewer
 *   developed by devseed
 * 
 *  dataflow: filebuf -(decode)-> tiles bytes -(render)-> 
 *            logicial bitmap -(scale)-> window bitmap
 */

#include <map>
#include <wx/wx.h>
#include <wx/textfile.h>
#include <wx/bitmap.h>
#include <wx/cmdline.h>
#include "ui.hpp"
#include "core_type.h"
#include "core_decode.h"
#include "core_plugin.h"
#include "core_app.hpp"

using std::pair;
using std::map;

struct tilecfg_t g_tilecfg = {0, 0, 32, 24, 24, 8, 0};

static bool s_nogui = false;
static std::map<wxString, struct tile_decoder_t> s_builtin_plugin_map = {
    std::pair<wxString, struct tile_decoder_t>("default decode plugin",  g_decoder_default)
};

static const wxCmdLineEntryDesc s_cmd_desc[] =
{
    { wxCMD_LINE_SWITCH, "n", "nogui", "decode tiles without gui",
        wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL},
    { wxCMD_LINE_OPTION, "i", "inpath", "tile file inpath",
        wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_OPTION, "o", "outpath", "outpath for decoded file",
        wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_OPTION, "p", "plugin", "use lua plugin to decode",
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

struct tile_decoder_t* TileSolver::LoadDecoder(wxFileName pluginfile)
{
    struct tile_decoder_t *decoder = nullptr;
    auto it = s_builtin_plugin_map.find(pluginfile.GetFullName());
    DECODE_STATUS status;
    if(it != s_builtin_plugin_map.end()) // built-in decode
    {
        decoder = &it -> second; 
        const char *name = it->first.c_str().AsChar();
        status = decoder->open(name, &decoder->context);
        if(!DECODE_SUCCESS(status)) 
        {
            wxLogError(wxString::Format(
                "[TileSolver::LoadDecoder] builtin %s open failed, msg: \n    %s", name, decoder->msg));
            return nullptr;
        }
    }
    else // lua decode
    {
        decoder = &g_decoder_lua;
        auto filepath = pluginfile.GetFullPath();       
        wxFile f(filepath);
        if(!f.IsOpened())
        {
            wxLogError("[TileSolver::LoadDecoder] open %s file failed !", filepath);
            return nullptr;
        }
        wxString luastr;
        f.ReadAll(&luastr);
        auto status = decoder->open(luastr.c_str().AsChar(), &decoder->context);
        if(!DECODE_SUCCESS(status)) 
        {
            wxLogError("[TileSolver::LoadDecoder] lua %s open failed, msg: \n    %s", filepath, decoder->msg);
            return nullptr;
        }
    }
    if (!decoder) 
    {
        wxLogError("[TileSolver::LoadDecoder] decoder %s is invalid !", pluginfile.GetFullName());
        return nullptr;
    }

    wxLogMessage("[TileSolver::LoadDecoder] %s msg: \n    %s", pluginfile.GetFullName(), decoder->msg);
    
    return decoder;
}

TileSolver::TileSolver()
{

}

size_t TileSolver::PrepareTilebuf()
{
    size_t start = m_tilecfg.start;
    size_t datasize = m_tilecfg.size - start;
    size_t nbytes = calc_tile_nbytes(&m_tilecfg.fmt);
    if(!datasize) 
    {
        if(m_filebuf.GetDataLen() - start < 0)
        {
            wxLogError("[TileSolver::Decode] start(%u) is bigger than file (%zu)!", 
                        start, m_filebuf.GetDataLen());
            return 0;
        }
        datasize = m_filebuf.GetDataLen() - start;
    }
    else 
    {
        datasize = wxMin<size_t, size_t>(datasize, m_filebuf.GetDataLen());
    }
    int ntile = datasize / nbytes;
    if(ntile <= 0) ntile = 1; // prevent data less than nbytes
    for(int i=0; i < ntile; i++)
    {
        auto tile = wxImage(m_tilecfg.w, m_tilecfg.h);
        tile.InitAlpha();
        if(i >= m_tiles.size()) m_tiles.push_back(tile);
        else m_tiles[i] = tile;
    }
    m_tiles.erase(m_tiles.begin() + ntile, m_tiles.end());
    return datasize;
}

size_t TileSolver::Open(wxFileName infile)
{
    if(infile.Exists()) m_infile = infile;
    auto inpath = m_infile.GetFullPath();
    if(inpath.Length() == 0) return 0;
    
    wxFile f(inpath, wxFile::read);
    size_t readsize = 0;
    if(!f.IsOpened())
    {
        wxLogError(wxString::Format("[TileSolver::Open] open %s failed",  inpath));
        return 0;
    }
    readsize = f.Read(m_filebuf.GetWriteBuf(f.Length()), f.Length());
    m_filebuf.SetDataLen(readsize); // DataSize is not the valid size
    f.Close();

    wxLogMessage(wxString::Format("[TileSolver::Open] open %s with %zu bytes", inpath, readsize));

    return readsize;
}

int TileSolver::Decode(struct tilecfg_t *cfg, wxFileName pluginfile)
{
    m_bitmap = wxBitmap(); // disable render bitmap while decode

    if(!m_filebuf.GetDataLen()) return 0;
    if(pluginfile.GetFullPath().Length() > 0) m_pluginfile = pluginfile;
    if(cfg) m_tilecfg = *cfg;

    // prepare decoder
    auto decoder = LoadDecoder(m_pluginfile);
    if(!decoder) 
    {
        wxLogError("[TileSolver::Decode] decoder %s is invalid", m_pluginfile.GetFullName());
        m_tiles.clear();
        return -1;
    }

    // pre processing
    DECODE_STATUS status;
    size_t start = m_tilecfg.start;
    auto context = decoder->context;
    auto rawdata = (uint8_t*)m_filebuf.GetData();
    auto rawsize = m_filebuf.GetDataLen();
    if(decoder->pre)
    {
        status = decoder->pre(context, rawdata, rawsize, &g_tilecfg);
        if(decoder->msg)
        {
            wxLogMessage("[TileSolver::Decode] decoder->pre msg: \n    %s", decoder->msg);
        }
        if(status == DECODE_FAIL)
        {
            wxLogError("[TileSolver::Decode] decoder->pre %s", decode_status_str(status));
            if(wxGetApp().m_usgui) wxMessageBox(decoder->msg, "decode error", wxICON_ERROR);
            decoder->close(decoder->context);
            m_tiles.clear();
            return -1;
        }
        m_tilecfg = g_tilecfg; // the pre process can change tilecfg
    }
    
    // decoding
    auto datasize  = PrepareTilebuf();
    auto time_start = wxDateTime::UNow();
    size_t ntile = m_tiles.size();
    if(datasize) 
    {
        struct tilepos_t pos;
        struct pixel_t pixel;
        for(int i=0; i< ntile; i++)
        {
            auto& tile = m_tiles[i];
            for(int y=0; y < m_tilecfg.h; y++)
            {
                for(int x=0; x < m_tilecfg.w; x++)
                {
                    pos = {i, x, y};
                    status = decoder->decode(context, 
                        rawdata + start, datasize, &pos, &m_tilecfg.fmt, &pixel, false);
                    if(!DECODE_SUCCESS(status))
                    {
                        wxLogWarning(wxString::Format(
                            "TileSolver::Decode] decode failed at (%d, %d, %d)", 
                            pos.i, pos.x, pos.y));
                        continue;
                    }
                    tile.SetRGB(pos.x, pos.y, pixel.r, pixel.g, pixel.b);
                    tile.SetAlpha(pos.x, pos.y, pixel.a);
                }
            }
        }
    }
    else
    {
        wxLogWarning("[TileSolver::Decode] datasize is 0");
    }
    auto time_end = wxDateTime::UNow();
    
    // post processing
    if(decoder->post)
    {
        status = decoder->post(decoder->context, rawdata, rawsize, &g_tilecfg);
        if(!DECODE_SUCCESS(status))
        {
            wxLogWarning("[TileSolver::Decode] decoder->post %s", decode_status_str(status));
        }
        else
        {
            if(decoder->msg)
            {
                wxLogMessage("[TileSolver::Decode] decoder->post msg: \n    %s", decoder->msg);
            }
            sync_tilenav(&g_tilenav, &g_tilecfg);
            NOTIFY_UPDATE_TILENAV();
            NOTIFY_UPDATE_TILECFG();
        }
    }
    decoder->close(decoder->context);
    
    size_t nbytes = calc_tile_nbytes(&m_tilecfg.fmt);
    wxLogMessage(wxString::Format(
        "[TileSolver::Decode] decode %zu tiles with %zu bytes, in %llu ms", 
        ntile, nbytes, (time_end - time_start).GetMilliseconds()));
    
    return ntile;
}

bool TileSolver::Render()
{
    if(!DecodeOk()) 
    {
        wxLogError("[TileSolver::Render] no tiles to render");
        return false;
    }

    size_t nrow = m_tilecfg.nrow;
    if(!nrow) 
    {
        m_bitmap = wxBitmap();
        wxLogError("[TileSolver::Render] nrow can not be 0");
        return false;
    }
    
    size_t tilew = m_tilecfg.w;
    size_t tileh = m_tilecfg.h;
    size_t ntile = m_tiles.size();
    size_t imgw =  m_tilecfg.nrow * tilew;
    size_t imgh = (ntile + nrow - 1) / nrow * tileh ;

    auto time_start = wxDateTime::UNow();
    wxBitmap bitmap(imgw, imgh);
    if(!bitmap.IsOk())
    {
        m_bitmap = wxBitmap();
        wxLogError("[TileSolver::Render] bitmap is not ready");
        return false;
    }
    bitmap.UseAlpha();
    wxMemoryDC dstdc(bitmap);
    for(int i=0; i < m_tiles.size(); i++)
    {
        int x = (i % nrow) * tilew;
        int y = (i / nrow) * tileh;
        auto tilebitmap = wxBitmap(m_tiles[i]);
        tilebitmap.UseAlpha();
        wxMemoryDC srcdc(tilebitmap);
        dstdc.Blit(wxPoint(x, y), wxSize(tilew, tileh), &srcdc, wxPoint(0, 0));
    }
    auto time_end = wxDateTime::UNow();

    wxLogMessage(wxString::Format(
        "[TileSolver::Render] tile (%zux%zu), image (%zux%zu), in %llu ms", 
        tilew, tileh, imgw, imgh, (time_end - time_start).GetMilliseconds()));
    m_bitmap = bitmap; // seems automaticly release previous

    return true;
}

bool TileSolver::Save(wxFileName outfile)
{
    if(outfile.GetFullPath().Length() > 0) m_outfile = outfile;
    auto outpath = m_outfile.GetFullPath();
    if(outpath.Length() == 0) return false;

    if(!m_bitmap.IsOk()) return false;

    wxImage image(m_bitmap.ConvertToImage());

    return image.SaveFile(m_outfile.GetFullPath());
}

bool TileSolver::Close()
{
    m_filebuf.Clear();
    m_tiles.clear();
    m_bitmap = wxBitmap();
    m_infile.Clear();
    return true;
}

bool TileSolver::DecodeOk()
{
    return m_tiles.size() > 0;
}

bool TileSolver::RenderOk()
{
    return m_bitmap.IsOk();
}

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
    if(parser.FoundSwitch("nogui") == wxCMD_SWITCH_ON) s_nogui = true;
    if(parser.Found("inpath", &val)) m_tilesolver.m_infile = val;
    if(parser.Found("outpath", &val)) m_tilesolver.m_outfile = val;
    if(parser.Found("plugin", &val)) m_tilesolver.m_pluginfile = val;
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
    
    // add build-in plugin
    for(auto &it : s_builtin_plugin_map)
    {
        m_pluginfiles.push_back(it.first);
    }
    
    // add extra plugin
    int i=0;
    if(wxFileName(dirpath).Exists())
    {
        wxString path = wxFindFirstFile(dirpath + "/*.lua");
        while (!path.empty())
        {
            m_pluginfiles.push_back(path);
            path = wxFindNextFile();
            i++;
        }
    }

    m_pluginindex = 0;

    return i;
}

bool MainApp::Gui(wxString cmdline)
{
    m_usgui = true;
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
    m_usgui = false;
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
    if(m_tilesolver.Save())
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
    if(s_nogui) res = Cli(cmdline);
    else res = Gui(cmdline);
    if(!res)
    {
        wxLogError("[MainApp::OnInit] init failed!");
    }

    return res;
}