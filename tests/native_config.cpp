#include "game_config.h"
#include "game_exe.h"
#include <cstdint>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>

namespace {
size_t checks=0;
void require(bool v,const char* text) {++checks;if(!v) throw std::runtime_error(text);}
void rejected(const std::function<void()>& action,const char* text) {
    bool failed=false;try{action();}catch(const std::runtime_error&){failed=true;}require(failed,text);
}
}
int main() {
    try {
        setenv("HOME","/home/tester",1);
        const auto c=wpc::parse_config(
            "# Wacky Races\n[paths]\ndata = ~/games/Wacky Races  ; installed files\ndisc=/run/media/tester/Wacky\n"
            "\n[game]\nlanguage = 3\n","/etc/wackyraces");
        require(c.data && *c.data=="/home/tester/games/Wacky Races","~ and trailing comment in data");
        require(c.disc && *c.disc=="/run/media/tester/Wacky","absolute disc path");
        require(c.language && *c.language==3,"language");
        const auto r=wpc::parse_config("[paths]\ndisc = disc-root\n","/etc/wackyraces");
        require(r.disc && *r.disc=="/etc/wackyraces/disc-root" && !r.data && !r.language,"relative path from the config directory");
        require(wpc::parse_config("[game]\nskip_intro = Yes\n","/").skip_intro==true,"skip_intro yes");
        require(wpc::parse_config("[game]\nskip_intro = off\n","/").skip_intro==false,"skip_intro off");
        require(!wpc::parse_config("[game]\nlanguage = 0\n","/").skip_intro,"skip_intro unset by default");
        require(wpc::parse_config("[game]\ndust = on\n","/").dust==true && !wpc::parse_config("[game]\n","/").dust,"dust option");
        rejected([]{wpc::parse_config("[game]\ndust = maybe\n","/");},"dust must be a boolean");
        rejected([]{wpc::parse_config("[game]\nskip_intro = maybe\n","/");},"invalid skip_intro accepted");
        rejected([]{wpc::parse_config("[paths]\nskip_intro = true\n","/");},"skip_intro outside [game] accepted");
        require(wpc::parse_config("[video]\nresolution = 1280x960\n","/").resolution==std::make_pair(1280,960),"resolution");
        require(wpc::parse_resolution("1920 X 1080")==std::make_pair(1920,1080),"resolution with spaces and X");
        require(!wpc::parse_config("[game]\nlanguage = 0\n","/").resolution,"resolution unset by default");
        rejected([]{wpc::parse_config("[video]\nresolution = 1280\n","/");},"resolution without height accepted");
        rejected([]{wpc::parse_config("[video]\nresolution = 100x100\n","/");},"tiny resolution accepted");
        require(wpc::parse_config("[video]\nfullscreen = yes\n","/").fullscreen==true && !wpc::parse_config("[game]\nlanguage = 0\n","/").fullscreen,"fullscreen flag");
        rejected([]{wpc::parse_config("[video]\nfullscreen = maybe\n","/");},"fullscreen must be a flag");
        rejected([]{wpc::parse_resolution("-640x480");},"negative resolution accepted");
        rejected([]{wpc::parse_config("[paths]\ncd = /mnt\n","/");},"unknown key accepted");
        rejected([]{wpc::parse_config("[network]\n","/");},"unknown section accepted");
        rejected([]{wpc::parse_config("[game]\nlanguage = 9\n","/");},"language out of range accepted");
        rejected([]{wpc::parse_config("[paths]\ndisc\n","/");},"line without = accepted");
        rejected([]{wpc::parse_config("[paths]\ndisc =\n","/");},"empty value accepted");
        rejected([]{wpc::load_config("/nonexistent/wackyraces.ini");},"missing explicit config accepted");
        setenv("WPC_CONFIG","/tmp/explicit.ini",1);
        require(wpc::default_config_path()=="/tmp/explicit.ini","WPC_CONFIG wins");
        unsetenv("WPC_CONFIG");setenv("XDG_CONFIG_HOME","/home/tester/.cfg",1);
        require(wpc::default_config_path()=="/home/tester/.cfg/wackyraces/config.ini","XDG_CONFIG_HOME");
        unsetenv("XDG_CONFIG_HOME");
        require(wpc::default_config_path()=="/home/tester/.config/wackyraces/config.ini","HOME fallback");
        // ExeImage maps virtual addresses through the section table: a minimal
        // PE32 with one section (va 0x1000, file offset 0x200) holding "HI".
        {
            std::vector<uint8_t> pe(0x210,0);
            const auto put32=[&](size_t at,uint32_t v){for(int i=0;i<4;++i) pe[at+i]=uint8_t(v>>(8*i));};
            pe[0]='M';pe[1]='Z';put32(0x3c,0x80);put32(0x80,0x4550);
            pe[0x86]=1;pe[0x94]=0xe0;pe[0x98]=0x0b;pe[0x99]=0x01;put32(0x80+52,0x400000);
            const size_t sec=0x80+24+0xe0;put32(sec+12,0x1000);put32(sec+16,0x10);put32(sec+20,0x200);
            pe[0x204]='H';pe[0x205]='I';
            wpc::ExeImage exe;exe.load_bytes(pe);
            require(exe.string_at(0x401004)=="HI","string at a section address");
            require(exe.string_at(0x401004+0x10).empty(),"address past the section");
            require(exe.string_at(0x300000).empty(),"address below the image");
            rejected([]{wpc::ExeImage().load_bytes(std::vector<uint8_t>(0x40,0));},"non-MZ image accepted");
        }
        std::printf("native config: %zu checks passed\n",checks);
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"native config: %s (%zu checks)\n",e.what(),checks);return 1;}
}
