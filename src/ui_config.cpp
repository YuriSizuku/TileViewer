/**
 * implement for tile config window
 *   developed by devseed
 */

#include <cstdlib>
#include <cstring>
#include <map>
#include <wx/wx.h>
#include <cJSON.h>
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

void SetTilecfg(wxString& text, struct tilecfg_t &cfg)
{
    cJSON *root = cJSON_Parse(text.mb_str());
    if(root)
    {
        const cJSON* prop = cJSON_GetObjectItem(root, "tilecfg");
        const cJSON* v = nullptr;
        v = cJSON_GetObjectItem(prop, "start"); if(v) cfg.start = v->valueint;
        v = cJSON_GetObjectItem(prop, "size"); if(v) cfg.size = v->valueint;
        v = cJSON_GetObjectItem(prop, "nrow"); if(v) cfg.nrow = v->valueint;
        v = cJSON_GetObjectItem(prop, "w"); if(v) cfg.w = v->valueint;
        v = cJSON_GetObjectItem(prop, "h"); if(v) cfg.h = v->valueint;
        v = cJSON_GetObjectItem(prop, "bpp"); if(v) cfg.bpp = v->valueint;
        v = cJSON_GetObjectItem(prop, "nbytes"); if(v) cfg.nbytes = v->valueint;
        cJSON_Delete(root);
    }
}

/**
 * override json value by text2
 * @param jtext1 json text
 * @param jtext2 {"name1": value1, "name2": value2}
 */
void OverridePluginCfg(wxString& jtext1, wxString& jtext2)
{
    if(!jtext1.Length() || !jtext2.Length()) return;

    jtext2.Replace('\'', '"');
    cJSON *root1 = cJSON_Parse(jtext1.mb_str());
    if(!root1) return;
    cJSON *root2 = cJSON_Parse(jtext2.mb_str());
    if(!root2) {cJSON_Delete(root1); return;}

    const cJSON* prop;
    const cJSON* props = cJSON_GetObjectItem(root1, "plugincfg");
    cJSON_ArrayForEach(prop, props)
    {
        const cJSON* name = cJSON_GetObjectItem(prop, "name");
        const cJSON* type = cJSON_GetObjectItem(prop, "type");
        cJSON* value = cJSON_GetObjectItem(prop, "value");
        if(!name) continue;
        if(!type) continue;

        const cJSON* value2 = cJSON_GetObjectItem(root2, name->valuestring);
        if(!value2) continue;
        if(!strcmp(type->valuestring, "string"))
        {
            cJSON_SetValuestring(value, value2->valuestring);
        }
        else
        {
            value->type = value2->type;
            value->valuedouble = value2->valuedouble;
            value->valueint = value2->valueint;
        }
    }
    jtext1.Clear();
    jtext1.Append(cJSON_PrintUnformatted(root1));

    cJSON_Delete(root1);
    cJSON_Delete(root2);
}

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

void ConfigWindow::SetPlugincfg(wxString& text)
{
    auto plugincfg = m_pg->GetPropertyByName("plugincfg");
    wxLogMessage(wxString::Format("[ConfigWindow::SetPlugincfg] %s", text));
    cJSON *root = cJSON_Parse(text.mb_str());
    if(root)
    {
        const cJSON* prop;
        const cJSON* props = cJSON_GetObjectItem(root, "plugincfg");
        cJSON_ArrayForEach(prop, props)
        {
            const cJSON* name = cJSON_GetObjectItem(prop, "name");
            const cJSON* type = cJSON_GetObjectItem(prop, "type");
            const cJSON* help = cJSON_GetObjectItem(prop, "help");
            const cJSON* value = cJSON_GetObjectItem(prop, "value");
            wxPGProperty *wxprop = nullptr;
            if(!name) continue;
            if(!type) continue;

            if(!strcmp(type->valuestring, "enum"))
            {
                auto enumprop = new wxEnumProperty(name->valuestring);
                const cJSON* option = nullptr;
                const cJSON* options = cJSON_GetObjectItem(prop, "options");
                cJSON_ArrayForEach(option, options)
                {
                    enumprop->AddChoice(option->valuestring);
                }
                if(value) enumprop->SetChoiceSelection(value->valueint);
                wxprop = enumprop;
            }
            else if(!strcmp(type->valuestring, "int"))
            {
                wxprop = new wxIntProperty(name->valuestring);
                if(value) wxprop->SetValue(value->valueint);
            }
            else if(!strcmp(type->valuestring, "string"))
            {
                wxprop = new wxStringProperty(name->valuestring);
                if(value) wxprop->SetValue(value->valuestring);
            }
            else if(!strcmp(type->valuestring, "bool"))
            {
                wxprop = new wxBoolProperty(name->valuestring);
                wxprop->SetAttribute(wxPG_BOOL_USE_CHECKBOX, true);
                if(value) wxprop->SetValue(value->valueint);
            }

            if(wxprop) 
            {
                if(help) wxprop->SetHelpString(help->valuestring);
                m_pg->AppendIn(plugincfg, wxprop);
            }
        }

        cJSON_Delete(root);
    }
    this->Refresh();
}

wxString ConfigWindow::GetPlugincfg()
{
    wxString text;

    cJSON *root = cJSON_CreateObject(); 
    cJSON *props = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "plugincfg", props);
    for (auto it = m_pg->GetIterator(); !it.AtEnd(); it++)
    {
        wxPGProperty* p = *it;
        if(p->GetParent()->GetName() != "plugincfg") continue;
        cJSON *prop = cJSON_CreateObject();
        cJSON *name = cJSON_CreateString(p->GetName());
        cJSON *value = nullptr;
        if(p->GetValueType() != "string") value = cJSON_CreateNumber(p->GetValue());
        else value = cJSON_CreateString(p->GetValueAsString());

        cJSON_AddItemToObject(prop, "name", name);
        cJSON_AddItemToObject(prop, "value", value);
        cJSON_AddItemToArray(props, prop);
    }
    text.Append(cJSON_PrintUnformatted(root));
    cJSON_Delete(root);
    
    wxLogMessage(wxString::Format("[ConfigWindow::GetPlugincfg] %s", text));
    return text;
}

void ConfigWindow::ClearPlugincfg()
{
    auto plugincfg = m_pg->GetPropertyByName("plugincfg");
    plugincfg->DeleteChildren();
    this->Refresh();
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
    LoadTilecfg(g_tilecfg);

    // tilenav
    auto tilenav = new wxPropertyCategory("tilenav");
    pg->Append(tilenav);
    pg->AppendIn(tilenav, new wxUIntProperty("offset"));
    pg->AppendIn(tilenav, new wxUIntProperty("index"));
    pg->SetPropertyHelpString("tilenav.offset", "current selected tile offset in file");
    pg->SetPropertyHelpString("tilenav.index", "current selected tile index");

    // plugincfg
    auto plugincfg = new wxPropertyCategory("plugincfg");
    pg->Append(plugincfg);
}

void ConfigWindow::OnPropertyGridChanged(wxPropertyGridEvent& event)
{
    auto prop = event.GetProperty();
    if(prop->GetParent()->GetName() == "tilecfg")
    {
        SaveTilecfg(g_tilecfg);
        
        if(prop->GetName()=="nrow") // only render when change nrow
        {
            wxGetApp().m_tilesolver.m_tilecfg.nrow =  g_tilecfg.nrow;
            wxGetApp().m_tilesolver.Render();
        }
        else // decode all tiles when setting tilecfg
        {
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
            if(g_tilecfg.bpp!=2 && g_tilecfg.bpp!=3 && g_tilecfg.bpp!=4 && g_tilecfg.bpp!=8 
                && g_tilecfg.bpp!=16 && g_tilecfg.bpp!=24 && g_tilecfg.bpp!=32)
            {
                wxMessageBox(wxString::Format(
                    "bpp %d might failed, should be 2,3,4,8,16,24,32", g_tilecfg.bpp), 
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
        }
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
    else if(prop->GetParent()->GetName() == "plugincfg")
    {
        wxGetApp().m_tilesolver.Decode(&g_tilecfg);
        g_tilenav.offset = -1; // prevent nrow chnages
        sync_tilenav(&g_tilenav, &g_tilecfg); 
        NOTIFY_UPDATE_TILES(); // notify tilecfg
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