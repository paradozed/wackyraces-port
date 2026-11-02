#include "game_config.h"
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace wpc {
namespace {
std::string trim(const std::string& s) {
    const size_t a=s.find_first_not_of(" \t\r"),b=s.find_last_not_of(" \t\r");
    return a==std::string::npos?std::string():s.substr(a,b-a+1);
}
std::filesystem::path resolve(const std::string& value,const std::filesystem::path& base) {
    std::filesystem::path p;
    if(value=="~" || value.rfind("~/",0)==0) {
        const char* home=std::getenv("HOME");
        if(!home || !*home) throw std::runtime_error("'~' in a config path needs HOME");
        p=std::filesystem::path(home)/(value.size()>2?value.substr(2):std::string());
    } else p=value;
    return p.is_relative()?base/p:p;
}
}
GameConfig parse_config(const std::string& text,const std::filesystem::path& base,const std::string& source) {
    GameConfig config;
    std::istringstream in(text);
    std::string line,section;
    for(int number=1;std::getline(in,line);++number) {
        const auto fail=[&](const std::string& why) {
            throw std::runtime_error(source+":"+std::to_string(number)+": "+why);
        };
        const size_t comment=line.find_first_of("#;");
        line=trim(comment==std::string::npos?line:line.substr(0,comment));
        if(line.empty()) continue;
        if(line.front()=='[') {
            if(line.back()!=']') fail("unterminated section header");
            section=trim(line.substr(1,line.size()-2));
            if(section!="paths" && section!="game" && section!="video") fail("unknown section ["+section+"]");
            continue;
        }
        const size_t eq=line.find('=');
        if(eq==std::string::npos) fail("expected key = value");
        const std::string key=trim(line.substr(0,eq)),value=trim(line.substr(eq+1));
        if(value.empty()) fail("empty value for "+key);
        if(section=="paths" && key=="data") config.data=resolve(value,base);
        else if(section=="paths" && key=="disc") config.disc=resolve(value,base);
        else if(section=="game" && key=="language") {
            char* end=nullptr;
            const unsigned long n=std::strtoul(value.c_str(),&end,10);
            if(*end || value[0]=='-' || n>6) fail("language must be 0..6");
            config.language=unsigned(n);
        } else if((section=="game" && (key=="skip_intro" || key=="dust")) || (section=="video" && key=="fullscreen")) {
            std::string v;for(char ch:value) v+=char(std::tolower(static_cast<unsigned char>(ch)));
            bool flag;
            if(v=="true" || v=="yes" || v=="on" || v=="1") flag=true;
            else if(v=="false" || v=="no" || v=="off" || v=="0") flag=false;
            else fail(key+" must be true or false");
            (key=="dust"?config.dust:key=="fullscreen"?config.fullscreen:config.skip_intro)=flag;
        } else if(section=="video" && key=="resolution") {
            try {config.resolution=parse_resolution(value);}
            catch(const std::runtime_error& e) {fail(e.what());}
        } else fail("unknown key "+(section.empty()?key:section+"."+key));
    }
    return config;
}
std::pair<int,int> parse_resolution(const std::string& text) {
    const size_t x=text.find_first_of("xX");
    const auto bad=[&]{throw std::runtime_error("resolution must be WIDTHxHEIGHT (320..16384), not '"+text+"'");};
    if(x==std::string::npos || x==0 || x+1>=text.size()) bad();
    const auto number=[&](const std::string& part) {
        if(part.empty() || part.find_first_not_of("0123456789")!=std::string::npos || part.size()>5) bad();
        const int n=std::stoi(part);
        if(n<320 || n>16384) bad();
        return n;
    };
    return {number(trim(text.substr(0,x))),number(trim(text.substr(x+1)))};
}
GameConfig load_config(const std::filesystem::path& path) {
    std::ifstream file(path);
    if(!file) throw std::runtime_error("cannot read config "+path.string());
    std::ostringstream text;text<<file.rdbuf();
    return parse_config(text.str(),path.parent_path(),path.string());
}
std::filesystem::path default_config_path() {
    if(const char* p=std::getenv("WPC_CONFIG"); p && *p) return p;
    if(const char* x=std::getenv("XDG_CONFIG_HOME"); x && *x) return std::filesystem::path(x)/"wackyraces"/"config.ini";
    if(const char* h=std::getenv("HOME"); h && *h) return std::filesystem::path(h)/".config"/"wackyraces"/"config.ini";
    return {};
}
}
