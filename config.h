#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <windows.h>

struct Config {
    std::string serverHost      = "play.yourserver.net";
    int         serverPort      = 25565;
    int         localPort       = 25566;
    float       drawDistance    = 128.f;
    float       fovOverride     = 70.f;
    int         toggleKey       = 0x74; // F5
    bool        showLabels      = true;
    bool        showHUD         = true;
    bool        soundOnFind     = true;
    bool        showCoal        = false;
    bool        showIron        = true;
    bool        showCopper      = true;
    bool        showGold        = true;
    bool        showLapis       = true;
    bool        showRedstone    = true;
    bool        showEmerald     = true;
    bool        showDiamond     = true;
    bool        showAncientDebris = true;

    bool Load(const std::string& path = "xray_config.ini") {
        std::ifstream f(path);
        if(!f.is_open()){ Save(path); return false; }
        std::string line;
        while(std::getline(f, line)){
            if(line.empty()||line[0]=='#'||line[0]==';') continue;
            auto eq = line.find('=');
            if(eq == std::string::npos) continue;
            std::string k = line.substr(0,eq);
            std::string v = line.substr(eq+1);
            auto trim=[](std::string& s){
                size_t a=s.find_first_not_of(" \t\r\n");
                size_t b=s.find_last_not_of(" \t\r\n");
                s=(a==std::string::npos)?"":s.substr(a,b-a+1);
            };
            trim(k); trim(v);
            values[k]=v;
        }
        auto G=[&](const std::string& k, const std::string& d)->std::string{
            auto it=values.find(k); return it!=values.end()?it->second:d;
        };
        serverHost        = G("server_host",         serverHost);
        serverPort        = std::stoi(G("server_port",        std::to_string(serverPort)));
        localPort         = std::stoi(G("local_port",         std::to_string(localPort)));
        drawDistance      = std::stof(G("draw_distance",      std::to_string(drawDistance)));
        fovOverride       = std::stof(G("fov",                std::to_string(fovOverride)));
        toggleKey         = std::stoi(G("toggle_key",         std::to_string(toggleKey)));
        showLabels        = G("show_labels",        "true") =="true";
        showHUD           = G("show_hud",           "true") =="true";
        soundOnFind       = G("sound_on_find",      "true") =="true";
        showCoal          = G("show_coal",          "false")=="true";
        showIron          = G("show_iron",          "true") =="true";
        showCopper        = G("show_copper",        "true") =="true";
        showGold          = G("show_gold",          "true") =="true";
        showLapis         = G("show_lapis",         "true") =="true";
        showRedstone      = G("show_redstone",      "true") =="true";
        showEmerald       = G("show_emerald",       "true") =="true";
        showDiamond       = G("show_diamond",       "true") =="true";
        showAncientDebris = G("show_ancient_debris","true") =="true";
        return true;
    }

    void Save(const std::string& path = "xray_config.ini") {
        std::ofstream f(path);
        if(!f.is_open()) return;
        f<<"# XRay Config\n";
        f<<"server_host         = "<<serverHost<<"\n";
        f<<"server_port         = "<<serverPort<<"\n";
        f<<"local_port          = "<<localPort<<"\n";
        f<<"draw_distance       = "<<drawDistance<<"\n";
        f<<"fov                 = "<<fovOverride<<"\n";
        f<<"toggle_key          = "<<toggleKey<<"\n";
        f<<"show_labels         = "<<(showLabels        ?"true":"false")<<"\n";
        f<<"show_hud            = "<<(showHUD           ?"true":"false")<<"\n";
        f<<"sound_on_find       = "<<(soundOnFind       ?"true":"false")<<"\n";
        f<<"show_coal           = "<<(showCoal          ?"true":"false")<<"\n";
        f<<"show_iron           = "<<(showIron          ?"true":"false")<<"\n";
        f<<"show_copper         = "<<(showCopper        ?"true":"false")<<"\n";
        f<<"show_gold           = "<<(showGold          ?"true":"false")<<"\n";
        f<<"show_lapis          = "<<(showLapis         ?"true":"false")<<"\n";
        f<<"show_redstone       = "<<(showRedstone      ?"true":"false")<<"\n";
        f<<"show_emerald        = "<<(showEmerald       ?"true":"false")<<"\n";
        f<<"show_diamond        = "<<(showDiamond       ?"true":"false")<<"\n";
        f<<"show_ancient_debris = "<<(showAncientDebris ?"true":"false")<<"\n";
    }

private:
    std::unordered_map<std::string,std::string> values;
};

inline Config g_cfg;
