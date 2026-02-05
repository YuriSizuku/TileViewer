/**
 * implement the major methods for tile viewer
 *   developed by devseed
 *
 *  dataflow: ->tilepath -(open)-> filebuf -(decode)-> tiles bytes
 *            -(render)-> logicial bitmap -(scale)-> window bitmap
 */

// #include <omp.h>
#include <map>
#include <wx/wx.h>
#include <wx/bitmap.h>
#include "core.hpp"
#include "ui.hpp"

// init decoders
extern "C" struct tile_decoder_t g_decoder_default;
extern "C" struct tile_decoder_t* STDCALL get_decoder_lua();
struct tilecfg_t g_tilecfg = {0, 0, 32, 24, 24, 8, 0};
std::map<wxString, struct tile_decoder_t> g_builtin_plugin_map = {
    std::pair<wxString, struct tile_decoder_t>("default plugin",  g_decoder_default)
};

extern void SetTilecfg(wxString& text, struct tilecfg_t &tilecfg);
extern void OverridePluginCfg(wxString& jtext, wxString& param);

bool TileSolver::LoadDecoder()
{
    return LoadDecoder(m_pluginfile);
}

bool TileSolver::LoadDecoder(wxFileName pluginfile)
{
    struct tile_decoder_t *decoder = nullptr;
    PLUGIN_STATUS status;

    // try to find decoder
    auto it = g_builtin_plugin_map.find(pluginfile.GetFullName());
    if(it != g_builtin_plugin_map.end()) // built-in decoder
    {
        decoder = &it -> second;
        const char *name = it->first.c_str().AsChar();
        status = decoder->open(name, &decoder->context);
    }
    else if(pluginfile.GetExt()=="lua")  // lua decoder
    {
        decoder = get_decoder_lua();
        auto filepath = pluginfile.GetFullPath();
        wxFile f(filepath);
        if(!f.IsOpened())
        {
            wxLogError("[TileSolver::LoadDecoder] lua %s, open file failed", filepath);
            return false;
        }
        wxString luastr;
        f.ReadAll(&luastr);
        status = decoder->open(luastr.c_str().AsChar(), &decoder->context);
    }
    else if ("." + pluginfile.GetExt() == wxDynamicLibrary::GetDllExt()) // c module decoder
    {
        auto filepath = pluginfile.GetFullPath();
        if(m_cmodule.IsLoaded()) m_cmodule.Unload();
        if(!m_cmodule.Load(pluginfile.GetFullPath()))
        {
            wxLogError("[TileSolver::LoadDecoder] cmodule %s, open file failed", filepath);
            return false;
        }
        decoder = (struct tile_decoder_t*)m_cmodule.GetSymbol("decoder"); // try find decoder struct
        if(!decoder) // try to find function get_decoder
        {
            auto get_decoder = (API_get_decoder)m_cmodule.GetSymbol("get_decoder");
            if(get_decoder) decoder = get_decoder();
        }
        if(!decoder)
        {
            m_cmodule.Unload();
            wxLogError("[TileSolver::LoadDecoder] cmodule %s, can not find decoder", filepath);
            return false;
        }
        status = decoder->open(m_pluginfile.GetFullName().mb_str(), &decoder->context);
    }

    // check decoder status
    if (!decoder)
    {
        wxLogError("[TileSolver::LoadDecoder] can not find %s", pluginfile.GetFullName());
        return false;
    }
    if(decoder->msg && decoder->msg[0])
    {
        wxLogMessage("[TileSolver::LoadDecoder] %s decoder->open msg: \n    %s", pluginfile.GetFullName(), decoder->msg);
    }
    else
    {
        wxLogMessage("[TileSolver::LoadDecoder] %s", pluginfile.GetFullName());
    }
    if(!PLUGIN_SUCCESS(status))
    {
        wxLogError(wxString::Format(
            "[TileSolver::LoadDecoder] %s decoder->open %s", pluginfile.GetFullName(), decode_status_str(status)));
        if(wxGetApp().m_usegui) wxMessageBox(decoder->msg, "decoder->open error", wxICON_ERROR);
        return false;
    }

    // check for ui setting if need
    if(!m_plugincfgfile.GetFullPath().Length())
    {
        m_plugincfgfile = pluginfile;
        m_plugincfgfile.ClearExt();
        m_plugincfgfile.SetExt("json");
    }
    wxString wxtext = LoadPlugincfg(); // find config at first
    if(!wxtext.Length() && decoder->sendui)
    {
        const char *text = nullptr;
        size_t textsize = 0;
        decoder->sendui(decoder->context, &text, &textsize);
        wxtext.Append(text);
        if(decoder->msg && decoder->msg[0])
        {
            wxLogMessage("[TileSolver::LoadDecoder] %s decoder->sendui msg: \n    %s",
                m_pluginfile.GetFullName(), decoder->msg);
        }
        OverridePluginCfg(wxtext, m_pluginparam);
    }
    if(wxGetApp().m_usegui)
    {
        wxGetApp().m_configwindow->SetPlugincfg(wxtext);
        NOTIFY_UPDATE_TILECFG();
        NOTIFY_UPDATE_TILENAV();
    }
    m_plugincfg = wxtext;

    // unload old decoder and use new decoder
    if(m_decoder) UnloadDecoder();
    m_decoder = decoder;

    return true;
}

bool TileSolver::UnloadDecoder()
{
    if(m_decoder)
    {
        m_decoder->close(m_decoder->context);
        if(m_decoder->msg && m_decoder->msg[0])
        {
            wxLogMessage("[TileSolver::Decode] %s decoder->close msg: \n    %s",
                m_pluginfile.GetFullName(), m_decoder->msg);
        }
        m_decoder = nullptr;
        m_plugincfgfile = wxString();
        if(wxGetApp().m_usegui)
        {
            wxGetApp().m_configwindow->ClearPlugincfg();
        }
    }
    if(m_cmodule.IsLoaded())
    {
        m_cmodule.Unload();
    }
    return true;
}

wxString TileSolver::LoadPlugincfg()
{
    wxString wxtext;
    if(wxFile::Exists(m_plugincfgfile.GetFullPath()))
    {
        wxFile f(m_plugincfgfile.GetFullPath());
        if(!f.IsOpened())
        {
            f.ReadAll(&wxtext);
            f.Close();
            wxLogMessage(wxString::Format(
                "[TileSolver::LoadPlugincfg] load config from %s",
                m_plugincfgfile.GetFullPath()));
        }
    }
    return wxtext;
}

TileSolver::TileSolver()
{
    m_decoder = nullptr;
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

size_t TileSolver::PrepareTilebuf()
{
    size_t start = m_tilecfg.start;
    size_t datasize = m_tilecfg.size;
    size_t nbytes = calc_tile_nbytes(&m_tilecfg.fmt);

    // check datasize avilable
    if(m_filebuf.GetDataLen() - start < 0)
    {
        wxLogError("[TileSolver::Decode] start(%u) is bigger than file (%zu)",
                    start, m_filebuf.GetDataLen());
        return 0;
    }

    if(!datasize) datasize = m_filebuf.GetDataLen() - start;
    else datasize = wxMin<size_t, size_t>(datasize, m_filebuf.GetDataLen() - start);

    // prepares tiles
    int ntile = datasize / nbytes;
    if(ntile <= 0) ntile = 1; // prevent data less than nbytes
    m_tiles.clear();
    for(int i=0; i < ntile; i++)
    {
        auto tile = wxImage(m_tilecfg.w, m_tilecfg.h);
        tile.InitAlpha();
        if (tile.IsOk())
        {
            m_tiles.push_back(tile);
        }
        else
        {
            m_tiles.clear();
            wxString msg = wxString::Format("[TileSolver::PrepareTilebuf] tile (%dX%d) is not ready, please reduce the tile size", m_tilecfg.w, m_tilecfg.h);
            if(wxGetApp().m_usegui) wxMessageBox(msg, "render error", wxICON_ERROR);
            return 0;
        }
    }
    return datasize;
}

int TileSolver::Decode(struct tilecfg_t *tilecfg, wxFileName pluginfile)
{
    m_bitmap = wxBitmap(); // disable render bitmap while decode
    if(tilecfg) m_tilecfg = *tilecfg;
    if(pluginfile.GetFullPath().Length() > 0)
    {
        if(m_decoder) UnloadDecoder();
        m_pluginfile = pluginfile; // force reload a new plugin
        m_decoder = nullptr;
    }

    // prepare decoder and parameters
    if(!m_decoder) LoadDecoder(m_pluginfile);
    auto decoder = m_decoder;
    if(!decoder)
    {
        wxLogError("[TileSolver::Decode] decoder %s is invalid", m_pluginfile.GetFullName());
        m_tiles.clear();
        return -1;
    }
    if(decoder->recvui)
    {
        wxString wxtext;
        if(!wxGetApp().m_usegui) wxtext = m_plugincfg;
        else wxtext = wxGetApp().m_configwindow->GetPlugincfg();
        decoder->recvui(decoder->context, wxtext.mb_str(), wxtext.size());
        if(decoder->msg && decoder->msg[0])
        {
            wxLogMessage("[TileSolver::Decode] %s decoder->recvui msg: \n    %s",
                m_pluginfile.GetFullName(), decoder->msg);
        }
    }

    // pre processing
    if(!m_filebuf.GetDataLen()) return 0;
    PLUGIN_STATUS status;
    size_t start = m_tilecfg.start;
    auto context = decoder->context;
    auto rawdata = (uint8_t*)m_filebuf.GetData();
    auto rawsize = m_filebuf.GetDataLen();
    auto time_start = wxDateTime::UNow();
    if(decoder->pre)
    {
        status = decoder->pre(context, rawdata, rawsize, tilecfg);
        if(decoder->msg && decoder->msg[0])
        {
            wxLogMessage("[TileSolver::Decode] decoder->pre msg: \n    %s", decoder->msg);
        }
        if(!PLUGIN_SUCCESS(status))
        {
            wxLogError("[TileSolver::Decode] decoder->pre %s", decode_status_str(status));
            if(wxGetApp().m_usegui) wxMessageBox(decoder->msg, "decoder->pre error", wxICON_ERROR);
            m_tiles.clear();
            return -1;
        }
        m_tilecfg = *tilecfg; // the pre process can change tilecfg
    }

    // decoding processing
    auto datasize  = PrepareTilebuf();
    size_t ntile = m_tiles.size();
    size_t nbytes = calc_tile_nbytes(&m_tilecfg.fmt);
    if(datasize)
    {
        if(decoder->decodeall)
        {
            size_t npixel;
            struct pixel_t *pixels;
            status = decoder->decodeall(context,
                rawdata + start, datasize, &m_tilecfg.fmt,  &pixels, &npixel, true);
            wxLogMessage(wxString::Format("[TileSolver::Decode] decoder->decodeall recv %zu pixels", npixel));
            if(decoder->msg && decoder->msg[0])
            {
                wxLogMessage("[TileSolver::Decode] decoder->decodeall msg: \n    %s", decoder->msg);
            }
            if(!PLUGIN_SUCCESS(status))
            {
                wxLogError("[TileSolver::Decode] decoder->decodeall %s", decode_status_str(status));
                if(wxGetApp().m_usegui) wxMessageBox(decoder->msg, "decoder->decodeall error", wxICON_ERROR);
                goto tilesolver_decode_post_start;
            }

            size_t ntilepixel = m_tilecfg.fmt.w * m_tilecfg.fmt.h;
            for(int i=0; i< ntile; i++)
            {
                size_t tilestart = i * ntilepixel;
                if(tilestart + ntilepixel > npixel) break;
                auto& tile = m_tiles[i];
                uint8_t *rgbdata = tile.GetData();
                uint8_t *adata = tile.GetAlpha();
                for(int pixeli=0; pixeli < ntilepixel; pixeli++)
                {
                    memcpy(rgbdata + pixeli*3, (void*)&pixels[tilestart + pixeli], 3);
                    adata[pixeli] = pixels[tilestart + pixeli].a; // alpah is in seperate channel
                }
            }
        }
        else if(decoder->decodeone)
        {
            for(int i=0; i< ntile; i++)
            {
                auto& tile = m_tiles[i];
                uint8_t *rgbdata = tile.GetData();
                uint8_t *adata = tile.GetAlpha();
                for(int y=0; y < m_tilecfg.h; y++)
                {
                    for(int x=0; x < m_tilecfg.w; x++)
                    {
                        struct tilepos_t pos = {i, x, y};
                        struct pixel_t pixel = {0};
                        status = decoder->decodeone(context, // lua function might not be in omp parallel
                            rawdata + start, datasize, &pos, &m_tilecfg.fmt, &pixel, false);
                        if(!PLUGIN_SUCCESS(status))
                        {
                            wxLogMessage("[TileSolver::Decode] decoder->decodeone msg: \n    %s", decoder->msg);
                            wxLogError("[TileSolver::Decode] decoder->decodeone %s", decode_status_str(status));
                            if(wxGetApp().m_usegui) wxMessageBox(decoder->msg, "decode_one error", wxICON_ERROR);
                            goto tilesolver_decode_post_start;
                        }
                        auto pixeli = y * m_tilecfg.w  + x;
                        memcpy(rgbdata + pixeli*3, &pixel, 3);
                        adata[pixeli] = pixel.a; // alpah is in seperate channel
                    }
                }
            }
        }
        else
        {
            wxLogError("[TileSolver::Decode] no decode function");
        }
    }
    else
    {
        wxLogWarning("[TileSolver::Decode] datasize is 0");
    }

    // post processing
tilesolver_decode_post_start:
    if(decoder->post)
    {
        status = decoder->post(decoder->context, rawdata, rawsize, tilecfg);
        if(decoder->msg && decoder->msg[0])
        {
            wxLogMessage("[TileSolver::Decode] decoder->post msg: \n    %s", decoder->msg);
        }
        if(!PLUGIN_SUCCESS(status))
        {
            wxLogError("[TileSolver::Decode] decoder->post %s", decode_status_str(status));
            if(wxGetApp().m_usegui) wxMessageBox(decoder->msg, "decoder->post error", wxICON_ERROR);
        }
    }
    auto time_end = wxDateTime::UNow();
    if(wxGetApp().m_usegui)
    {
        sync_tilenav(&g_tilenav, &g_tilecfg);
        NOTIFY_UPDATE_TILENAV();
        NOTIFY_UPDATE_TILECFG();
    }
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
    size_t imgw =  nrow * tilew;
    size_t imgh = (ntile + nrow - 1) / nrow * tileh ;

    auto time_start = wxDateTime::UNow();
    wxBitmap bitmap(imgw, imgh);
    if(!bitmap.IsOk())
    {
        // try reduce the nrow number
        nrow = ntile==1 ? 1 : ((int)sqrt(tilew * tileh * ntile) + tilew - 1) / tilew;
        imgw = nrow * tilew;
        imgh = (ntile + nrow - 1) / nrow * tileh;
        bitmap = wxBitmap(imgw, imgh);
        if(bitmap.IsOk())
        {
            m_tilecfg.nrow = nrow;
            g_tilecfg.nrow = nrow;
            if(wxGetApp().m_usegui)
            {
                sync_tilenav(&g_tilenav, &g_tilecfg);
                NOTIFY_UPDATE_TILECFG();
            }
        }
        else
        {
            m_bitmap = wxBitmap();
            wxString msg = wxString::Format("[TileSolver::Render] bitmap (%dX%d) is not ready, please reduce the tile size or nrow number", imgw, imgh);
            if(wxGetApp().m_usegui) wxMessageBox(msg, "render error", wxICON_ERROR);
            wxLogError(msg);
            return false;
        }
    }
    bitmap.UseAlpha();

    wxMemoryDC dstdc(bitmap);
    // #pragma omp parallel for, this might cause sementation fault after ?
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
    m_infile.Clear(); // inpath
    m_filebuf.Clear(); // inbuf
    m_tiles.clear(); // decode
    m_bitmap = wxBitmap(); // render
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