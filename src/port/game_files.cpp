#include "game_files.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace wpc {
namespace fs=std::filesystem;
namespace {
std::string folded(std::string value) {
    for(char& c:value) c=char(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
}
fs::path game_file(const std::vector<fs::path>& roots,const std::string& relative) {
    std::string normalized=relative;
    std::replace(normalized.begin(),normalized.end(),'\\','/');
    const fs::path name(normalized);
    if(name.empty() || name.is_absolute() || normalized.find(':')!=std::string::npos)
        throw std::runtime_error("Invalid relative game asset name: "+relative);
    for(const auto& part:name) if(part==".." || part=="." || part.empty())
        throw std::runtime_error("Invalid game asset path component: "+relative);
    for(const auto& root:roots) {
        if(root.empty()) continue;
        fs::path current=root;
        bool missing=false;
        for(const auto& part:name) {
            if(!fs::is_directory(current)) {missing=true;break;}
            fs::path match;
            const std::string wanted=folded(part.string());
            for(const auto& entry:fs::directory_iterator(current)) {
                if(folded(entry.path().filename().string())!=wanted) continue;
                if(!match.empty()) throw std::runtime_error("Ambiguous case-insensitive game asset: "+(current/part).string());
                match=entry.path();
            }
            if(match.empty()) {missing=true;break;}
            current=match;
        }
        if(!missing && fs::is_regular_file(current)) return current;
    }
    throw std::runtime_error("Missing original game asset: "+relative+" (set --data/WPC_DATA and --disc/WPC_DISC)");
}
std::string course_file(int mode,int environment,int course) {
    static const char* codes[]={"cc","sh","dt","ct","dr"};
    if(mode<0 || mode>3 || environment<0 || environment>=5 || course<0 || course>=4)
        throw std::runtime_error("Course selection is outside the original filename tables");
    const std::string code=codes[environment];
    // Original 0x486f90: battle uses its five-entry table; other modes use
    // environment*4+course in the twenty-entry table at 0x52bef0.
    return code+"/"+code+(mode==1?"_b.wgm":"_t"+std::to_string(course+1)+".wgm");
}
CourseChoice championship_course(int championship,int race) {
    if(championship<0 || championship>4 || race<0 || race>=championship_races)
        throw std::runtime_error("Championship race is outside the original progression");
    if(championship==4) return {4,race};
    return {race,championship};
}
}
