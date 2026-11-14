#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Persisted between launches in settings.ini next to the executable.
struct Settings {
    int width=1440, height=900;
    bool fullscreen=false, vsync=true;
    int shadowQuality=2;      // 0 off, 1 low, 2 high
    int effects=2;            // 0 low, 1 medium, 2 high
    bool technique=true;      // headline technique (soft shadows)
    bool perspective=true;
    bool tiltShift=true;      // miniature (tilt-shift) depth of field
    float master=.8f, music=.6f, sfx=.8f;
    float cameraSensitivity=1.f;
    void load(const std::filesystem::path& path){
        std::ifstream in(path);std::string line;
        while(std::getline(in,line)){
            auto p=line.find('=');if(p==std::string::npos)continue;
            std::string k=line.substr(0,p),v=line.substr(p+1);
            try{
                if(k=="width")width=std::clamp(std::stoi(v),640,7680);
                else if(k=="height")height=std::clamp(std::stoi(v),480,4320);
                else if(k=="fullscreen")fullscreen=v=="1";
                else if(k=="vsync")vsync=v=="1";
                else if(k=="shadows")shadowQuality=std::clamp(std::stoi(v),0,2);
                else if(k=="effects")effects=std::clamp(std::stoi(v),0,2);
                else if(k=="technique")technique=v=="1";
                else if(k=="perspective")perspective=v=="1";
                else if(k=="tiltshift")tiltShift=v=="1";
                else if(k=="master")master=std::clamp(std::stof(v),0.f,1.f);
                else if(k=="music")music=std::clamp(std::stof(v),0.f,1.f);
                else if(k=="sfx")sfx=std::clamp(std::stof(v),0.f,1.f);
                else if(k=="camera")cameraSensitivity=std::clamp(std::stof(v),.2f,3.f);
            }catch(const std::exception&){}
        }
    }
    bool save(const std::filesystem::path& path) const{
        std::ofstream out(path,std::ios::trunc);
        out<<"width="<<width<<"\nheight="<<height<<"\nfullscreen="<<fullscreen<<"\nvsync="<<vsync
           <<"\nshadows="<<shadowQuality<<"\neffects="<<effects<<"\ntechnique="<<technique<<"\nperspective="<<perspective<<"\ntiltshift="<<tiltShift
           <<"\nmaster="<<master<<"\nmusic="<<music<<"\nsfx="<<sfx<<"\ncamera="<<cameraSensitivity<<"\n";
        return bool(out);
    }
};
