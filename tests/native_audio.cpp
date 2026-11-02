#include <array>
#include "audio_mixer.h"
#include "game_audio.h"
#include "game_sound.h"
#include "ima.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <stdexcept>

namespace {
size_t checks=0;
void require(bool v,const char* text) {++checks;if(!v) throw std::runtime_error(text);}
void rejected(const std::function<void()>& action,const char* text) {
    bool failed=false;try{action();}catch(const std::runtime_error&){failed=true;}require(failed,text);
}
void put(std::vector<uint8_t>& b,uint32_t v) {for(int i=0;i<4;++i) b.push_back(uint8_t(v>>(i*8)));}
std::shared_ptr<wpc::PcmBuffer> pcm(std::vector<int16_t> samples,int channels,int rate) {
    auto p=std::make_shared<wpc::PcmBuffer>();p->samples=std::move(samples);p->channels=channels;p->rate=rate;return p;
}
void test_mixer() {
    wpc::AudioMixer mixer;
    int16_t out[16];
    // Same-rate mono plays sample-exact into both channels.
    wpc::AudioMixer::Params p;
    const uint32_t a=mixer.play(pcm({1000,2000,3000},1,44100),p);
    require(a!=0 && mixer.playing(a),"voice started");
    mixer.mix(out,4);
    require(out[0]==1000 && out[1]==1000 && out[2]==2000 && out[5]==3000 && out[6]==0,"mono at the mix rate");
    require(!mixer.playing(a),"one-shot voice removed at its end");
    // Half-rate source is linearly interpolated.
    mixer.play(pcm({0,1000},1,22050),p);
    mixer.mix(out,3);
    require(out[0]==0 && out[2]==500 && out[4]==1000,"linear resampling from 22050 Hz");
    mixer.mix(out,4); // drain the half-rate voice's final half-step
    // Looping wraps to the start; the bus level caps the volume (0x427f90:
    // min(0.75, 0.5) is -20 dB).
    p.loop=true;p.volume=0.75f;p.bus=wpc::AudioBus::Music;
    mixer.set_bus_volume(wpc::AudioBus::Music,0.5f);
    const uint32_t loop=mixer.play(pcm({4000,8000},1,44100),p);
    mixer.mix(out,4);
    const auto near=[](int a,int b){return std::abs(a-b)<=1;};
    require(near(out[0],400) && near(out[2],800) && near(out[4],400) && mixer.playing(loop),"loop wrap with the volume capped by the bus level");
    mixer.set_bus_volume(wpc::AudioBus::Music,1);
    mixer.mix(out,2);
    require(near(out[0],4000*wpc::directsound_gain(0.75f)) && near(out[2],8000*wpc::directsound_gain(0.75f)),"volume below the bus level is kept");
    mixer.set_bus_volume(wpc::AudioBus::Music,0.5f);
    mixer.set_pitch(loop,2);
    mixer.mix(out,2);
    require(near(out[0],400) && near(out[2],400),"pitch steps through the source");
    mixer.stop_bus(wpc::AudioBus::Music);
    require(!mixer.playing(loop),"bus stop");
    // Stereo keeps channels apart; pan attenuates the far side only.
    p={};p.pan=1;
    mixer.play(pcm({1000,3000},2,44100),p);
    mixer.mix(out,1);
    require(out[0]==0 && out[1]==3000,"full right pan");
    // 3D voices: the distance gain multiplies the volume and the pan.
    p={};const uint32_t far=mixer.play(pcm({1000},1,44100),p);
    mixer.set_spatial(far,0.5f,-1);
    mixer.mix(out,1);
    require(out[0]==500 && out[1]==0,"spatial gain and left pan");
    mixer.stop(far);
    // DirectSound3D law: full within min, (min/d)^5 beyond, held at max;
    // pan from the azimuth in the listener frame.
    {
        std::array<float,16> l{};l[0]=1;l[5]=1;l[10]=1;l[15]=1;
        const auto a=wpc::directsound3d(l,{{0,0,900}},1000,10000);
        const auto b=wpc::directsound3d(l,{{0,0,2000}},1000,10000);
        const auto c=wpc::directsound3d(l,{{0,0,50000}},1000,10000);
        const auto d=wpc::directsound3d(l,{{3000,0,0}},1000,10000);
        const auto e=wpc::directsound3d(l,{{-1000,0,1000}},1000,10000);
        require(a.gain==1 && a.pan==0,"within m_MinRange: full volume, centred");
        require(std::abs(b.gain-1/32.f)<1e-6f,"twice m_MinRange: rolloff 5 -> -30 dB");
        require(std::abs(c.gain-1e-5f)<1e-9f,"held at m_MaxRange");
        require(d.pan>0.99f && std::abs(d.gain-std::pow(1/3.f,5.f))<1e-6f,"right side: full right pan");
        require(std::abs(e.pan+1-std::pow(10.f,-2.5f))<1e-4f,"45 degrees left: -5000 mB on the right");
    }
    // Clipping and the movie clock.
    p={};
    const uint32_t loud=mixer.play(pcm(std::vector<int16_t>(44100,30000),1,44100),p);
    mixer.play(pcm(std::vector<int16_t>(10,30000),1,44100),p);
    mixer.mix(out,2);
    require(out[0]==32767,"sum clips to 16 bits");
    std::vector<int16_t> big(44100*2);
    mixer.mix(big.data(),22050-2);
    require(std::abs(mixer.position(loud)-0.5)<1e-6,"position reports the source clock");
    require(mixer.position(12345)<0,"stopped voice has no position");
}
void test_snd() {
    // Stereo IMA: one byte per frame, high nibble left.
    std::vector<uint8_t> snd{1,1,0x44,0xac,0x70,0x07};
    const wpc::PcmBuffer s=wpc::decode_snd(snd);
    require(s.channels==2 && s.rate==44100 && s.frames()==2,"stereo IMA header and framing");
    short expect[4];WpcImaState l{0,0},r{0,0};
    const signed char raw[2]={0x70,0x07};
    wpc_ima_decode(1,raw,expect,2,&l,&r);
    require(std::memcmp(expect,s.samples.data(),8)==0,"decodes through the recovered 0x40bdd0 routine");
    require(s.samples[0]>0 && s.samples[1]==0 && s.samples[3]>0,"nibble order and channel mapping");
    const wpc::PcmBuffer m=wpc::decode_snd({0,1,0x22,0x56,0x70,0x00});
    require(m.channels==1 && m.rate==22050 && m.frames()==4,"mono IMA: two samples per byte");
    const wpc::PcmBuffer raw16=wpc::decode_snd({0,0,0x22,0x56,0x34,0x12,0xff,0xff});
    require(raw16.frames()==2 && raw16.samples[0]==0x1234 && raw16.samples[1]==-1,"uncompressed 16-bit stream");
    rejected([]{wpc::decode_snd({0,1,0x10,0x00});},"implausible rate accepted");
    rejected([]{wpc::decode_snd({0,1});},"truncated header accepted");
}
std::vector<uint8_t> bank(uint32_t group,std::vector<std::array<uint32_t,5>> rows,std::vector<uint8_t> body) {
    std::vector<uint8_t> b;
    put(b,16+uint32_t(rows.size())*28);put(b,uint32_t(body.size()));
    put(b,group);put(b,0);put(b,uint32_t(rows.size()));put(b,0);
    for(const auto& r:rows) {for(uint32_t v:r) put(b,v);put(b,0);put(b,0);}
    b.insert(b.end(),body.begin(),body.end());
    return b;
}
void test_bank() {
    const auto data=bank(0x80,{{0x64,0,4,22050,2},{0x65,4,4,11025,3}},{1,0,2,0, 3,0,4,0});
    wpc::SoundBank b;b.load(data.data(),data.size());
    require(b.group()==0x80 && b.size()==2,"bank table");
    const auto mono=b.find(0x80000064);
    require(mono && mono->channels==1 && mono->rate==22050 && mono->frames()==2 && mono->samples[1]==2,"16-bit mono row");
    const auto stereo=b.find(0x80000065);
    require(stereo && stereo->channels==2 && stereo->frames()==1,"flag bit 0 selects stereo");
    require(!b.find(0x00000064) && !b.find(0x80000099),"lookup matches group byte and low 24 bits");
    auto bad=bank(0x80,{{0x64,6,4,22050,2}},{0,0,0,0,0,0,0,0});
    rejected([&]{b.load(bad.data(),bad.size());},"sample beyond the body accepted");
    bad=bank(0x80,{{0x64,0,2,22050,0}},{0,0});
    rejected([&]{b.load(bad.data(),bad.size());},"8-bit sample accepted");
    std::vector<uint8_t> sbpc{'S','B','P','C'};put(sbpc,2);put(sbpc,0);put(sbpc,24);put(sbpc,3);put(sbpc,24+uint32_t(data.size()));
    sbpc.insert(sbpc.end(),data.begin(),data.end());
    const auto other=bank(0x80,{{0x70,0,2,22050,2}},{9,0});
    sbpc.insert(sbpc.end(),other.begin(),other.end());
    b.load_sbpc(sbpc,3);
    require(b.find(0x80000070) && !b.find(0x80000064),"SBPC selects the exact language bank");
    rejected([&]{b.load_sbpc(sbpc,1);},"missing language accepted");
}
void test_names() {
    require(wpc::music_file("cc/cc_t1.wgm")=="Music/cc_t1.snd","course stem");
    require(wpc::music_file("ct\\ct_t3.WGM")=="Music/ct_t3.snd","Windows separators");
    require(wpc::music_file("")=="Music/Cc_t1.snd","menu default");
    require(wpc::music_file("dr/dr_t2.wgm")=="Music/Sh_t1.snd" && wpc::music_file("dr/dr_t4.wgm")=="Music/Ct_t1.snd","dr fallbacks");
    require(wpc::music_file("dr/dr_b.wgm")=="Music/Cc_b.snd","dr battle fallback");
    require(std::strcmp(wpc::voice_table[3][5][0],"GB-15.snd")==0 && std::strcmp(wpc::voice_table[6][0][0],"D-081.snd")==0,
            "0x527d20 voice table order");
}
void test_avi_audio() {
    wpc::Avi avi;
    avi.bytes={1,0,2,0,3,0,4,0,5,0,6,0};
    wpc::AviStream s;s.type={{'a','u','d','s'}};s.format_tag=1;s.bits_per_sample=16;s.channels=2;s.samples_per_second=22050;
    avi.streams.push_back(s);avi.chunks.push_back({{0,4},{4,8}});
    const wpc::PcmBuffer p=wpc::avi_pcm(avi,0);
    require(p.channels==2 && p.frames()==3 && p.samples[4]==5,"AVI PCM chunks concatenated");
    avi.streams[0].bits_per_sample=8;
    rejected([&]{wpc::avi_pcm(avi,0);},"8-bit AVI audio accepted");
}
void test_volume_law() {
    // 0x427f90: SetVolume(min(v,1)*4000-4000) hundredths of a dB.
    require(std::abs(wpc::directsound_gain(1)-1)<1e-6f && std::abs(wpc::directsound_gain(2)-1)<1e-6f,"full volume is 0 dB");
    require(std::abs(wpc::directsound_gain(0.5f)-0.1f)<1e-6f,"half volume is -20 dB");
    require(std::abs(wpc::directsound_gain(0)-0.01f)<1e-6f && std::abs(wpc::directsound_gain(-1)-0.01f)<1e-6f,"zero volume is -40 dB");
}
void corpus(const std::filesystem::path& data,const std::filesystem::path& disc) {
    for(uint32_t language=0;language<7;++language) {
        wpc::SoundBank b;b.load_sbpc(wpc::read_file((data/"Menu.GSW").string()),language);
        require(b.size()==37 && b.find(0x80000064) && b.find(0x80000066),"Menu.GSW language bank");
    }
    wpc::Archive general;general.load((data/"general.wgm").string());
    wpc::SoundBank g;g.load_sbnk(general);
    require(g.size()==46 && g.find(0x10339) && g.find(0x1033d),"general.wgm SBNK (engine, splash)");
    const wpc::PcmBuffer music=wpc::decode_snd(wpc::read_file((disc/"Music/Cc_t1.snd").string()));
    const double seconds=double(music.frames())/music.rate;
    require(music.channels==2 && music.rate==44100 && seconds>150 && seconds<240,"Cc_t1 stereo IMA music");
    wpc::GameAudio audio({data},{disc},3);
    require(audio.voice(7,101)==false,"narrator 901 waits for 900");
    require(audio.voice(7,100) && audio.voice(7,101) && audio.voice(7,102) && audio.voice(7,103),"narrator sequence");
    require(audio.voice(7,100),"sequence resets after 903");
    wpc::Archive menu;menu.load((data/"menu.WGM").string());
    wpc::SoundBank effects;effects.load_sbnk(menu);
    const auto click=effects.find(0x7f7);
    require(click && click->channels==1 && click->rate==22050 && click->frames()==2847,"menu.WGM SBNK cursor click (GSMP 0x7f7)");
    std::printf("Corpus: 7 GSW banks, 46 general samples, Cc_t1 %.1f s, narrator sequence\n",seconds);
}
}
int main(int argc,char** argv) {
    try {
        test_mixer();test_snd();test_bank();test_names();test_avi_audio();test_volume_law();
        std::printf("native audio: %zu synthetic checks passed\n",checks);
        if(argc==3) corpus(argv[1],argv[2]);
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"native audio: %s (%zu checks)\n",e.what(),checks);return 1;}
}
