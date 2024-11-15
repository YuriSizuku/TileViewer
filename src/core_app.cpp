/**
 * implement the major methods for tile viewer
 *   developed by devseed
 */

#include <map>
#include <wx/wx.h>
#include <wx/bitmap.h>
#include <wx/cmdline.h>
#include "ui.hpp"
#include "core_type.h"
#include "core_decode.h"
#include "core_app.hpp"

struct tilecfg_t g_tilecfg = {0, 0, 32, 24, 24, 8, 0};

static bool s_nogui = false;
static struct tile_decoder_t s_default_decoder = {
    decode_pixel_default, nullptr, nullptr
};
static std::map<wxString, struct tile_decoder_t> s_builtin_plugin_map = {
    std::pair<wxString, struct tile_decoder_t>("default decode plugin",  s_default_decoder)
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

struct tile_decoder_t* TileSolver::FindDecoder(wxFileName pluginfile)
{
    struct tile_decoder_t *decoder = nullptr;
    auto it = s_builtin_plugin_map.find(pluginfile.GetFullName());
    if(it != s_builtin_plugin_map.end()) // built-in decode
    {
        decoder = &it -> second; 
    }
    else // lua decode
    {
        return nullptr;
    }
    if (!decoder) 
    {
        wxLogError("[TileSolver::Decode] decoder is invalid !");
        return nullptr;
    }
    return decoder;
}

TileSolver::TileSolver()
{

}

size_t TileSolver::PrepareTilebuf()
{
    size_t start = m_tilecfg.start;
    size_t insize = m_tilecfg.size - start;
    size_t nbytes = calc_tile_nbytes(&m_tilecfg.fmt);
    if(!insize) 
    {
        if(m_filebuf.GetDataLen() - start < 0)
        {
            wxLogError("[TileSolver::Decode] start(%u) is bigger than file (%zu)!", 
                        start, m_filebuf.GetDataLen());
            return 0;
        }
        insize = m_filebuf.GetDataLen() - start;
    }
    else 
    {
        insize = wxMin<size_t, size_t>(insize, m_filebuf.GetDataLen());
    }
    int ntile = insize / nbytes;
    for(int i=0; i < ntile; i++)
    {
        auto tile = wxImage(m_tilecfg.w, m_tilecfg.h);
        tile.InitAlpha();
        if(i >= m_tiles.size()) m_tiles.push_back(tile);
        else m_tiles[i] = tile;
    }
    m_tiles.erase(m_tiles.begin() + ntile, m_tiles.end());
    return insize;
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
    if(!m_filebuf.GetDataLen()) return 0;
    if(pluginfile.GetFullPath().Length() > 0) m_pluginfile = pluginfile;
    if(cfg) m_tilecfg = *cfg;

    auto decoder = FindDecoder(m_pluginfile);
    if(!decoder) return -1;
    auto insize = PrepareTilebuf();
    if(!insize) return -1;
    size_t start = m_tilecfg.start;
    
    // decode_pre
    auto inbuf = m_filebuf;
    auto outbuf = m_filebuf;
    auto indata = (uint8_t*)inbuf.GetData();
    auto outsize = insize;
    if(decoder->decode_alloc)
    {
        auto outsize = decoder->decode_alloc(indata, insize);
        if(!outsize)
        {
            wxLogError("[TileSolver::Decode] decode_alloc falied");
            return -1;
        }
        outbuf = wxMemoryBuffer(outsize);
        outbuf.SetBufSize(outsize);
    }
    if(decoder->decode_pre)
    {
        if(!decoder->decode_pre(indata, insize, (uint8_t*)outbuf.GetData(), outsize))
        {
            wxLogError("[TileSolver::Decode] decode_pre falied");
            return -1;
        }
        insize = outsize;
        m_filebuf = outbuf;
        indata = (uint8_t*)m_filebuf.GetData();
    }

    // decode each pixel
    struct tilepos_t pos;
    struct pixel_t pixel;
    size_t ntile = m_tiles.size();
    auto time_start = wxDateTime::UNow();
    for(int i=0; i< ntile; i++)
    {
        auto& tile = m_tiles[i];
        for(int y=0; y < m_tilecfg.h; y++)
        {
            for(int x=0; x < m_tilecfg.w; x++)
            {
                pos = {i, x, y};
                bool res = decoder->decode_pixel(indata + start, insize, &pos, &m_tilecfg.fmt, &pixel, false);
                if(!res)
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
    
    size_t nbytes = calc_tile_nbytes(&m_tilecfg.fmt);
    auto time_end = wxDateTime::UNow();
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
    wxImage::AddHandler(new wxJPEGHandler);
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