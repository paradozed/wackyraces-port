#include "menu_view.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace wpc {
namespace {
uint32_t word(ByteView b, size_t offset) {
    if (offset>b.size || b.size-offset<4) throw std::runtime_error("Truncated menu resource");
    auto *p=b.data+offset;
    return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
}
int signed_half(const uint8_t *p) { return int16_t(uint16_t(p[0])|uint16_t(p[1])<<8); }
uint32_t link(const Archive &archive, const Chunk &resource, const std::string &tag, size_t index) {
    auto data=archive.payload(resource);
    uint32_t count=word(data,4);
    if (count>(data.size-8)/8 || data.size!=8+size_t(count)*8 || index>=count)
        throw std::runtime_error("Invalid menu resource link table");
    if (std::string(reinterpret_cast<const char *>(data.data+8+index*8),4)!=tag)
        throw std::runtime_error("Unexpected menu resource link type");
    return word(data,12+index*8);
}
}
MenuView::Texture &MenuView::material(uint32_t id) {
    auto found=materials_.find(id);
    uint32_t texture_id;
    if (found==materials_.end()) {
        texture_id=archive_.material_texture(id); materials_[id]=texture_id;
    } else texture_id=found->second;
    auto it=textures_.find(texture_id);
    if (it!=textures_.end()) return it->second;
    Image decoded=archive_.texture(texture_id);
    Texture texture{}; texture.width=int(decoded.width); texture.height=int(decoded.height);
    if (!decoded.rgba.empty()) {
        texture.gpu=wpc_vk_texture_create(window_,decoded.width,decoded.height,decoded.rgba.data());
        if (!texture.gpu) throw std::runtime_error("Cannot upload menu texture " + std::to_string(texture_id));
    }
    return textures_.emplace(texture_id,texture).first->second;
}
std::vector<MenuView::Placement> MenuView::placements(const Chunk &resource) {
    auto data=archive_.payload(resource);
    uint32_t count=word(data,4);
    if (count>(data.size-8)/12 || data.size!=8+size_t(count)*12)
        throw std::runtime_error("Invalid menu placement table");
    std::vector<Placement> result;
    for (uint32_t i=0;i<count;++i) {
        auto *p=data.data+8+size_t(i)*12;
        if (std::string(reinterpret_cast<const char *>(p+4),4)!="GMAT")
            throw std::runtime_error("Invalid menu placement material type");
        Placement placement{signed_half(p),signed_half(p+2),word(data,16+size_t(i)*12)};
        material(placement.material); result.push_back(placement);
    }
    return result;
}
MenuView::MenuView(WpcWindow *window, const Archive &archive, const Scene &scene,
                   const Strings &strings, const MenuController &controller)
    :window_(window),archive_(archive),scene_(scene),strings_(strings),controller_(controller),
     models_(window,archive,scene,[this](uint32_t mat){return material(mat).gpu;}) {
    for (size_t i=0;i<3;++i) {
        highlight_[i]=std::clamp(controller.manager().integer(0x1a5+uint32_t(i)),0,255)/255.f;
        clear_[i]=std::clamp(scene.screen().integer(0xc5+uint32_t(i)),0,255)/255.f;
    }
    prepare();
}
void MenuView::collect(Page &page,const Node &node,int item,int cursor_x,int cursor_y,bool common) {
    if (node.class_name=="GDSprite" || node.class_name=="WMenuSprite") {
        uint32_t mat=node.link(0x9e,"GMAT");
        Texture &texture=material(mat);
        // The main common group's tall portraits are replaced by its four-piece
        // film compositions. Their original activation still needs comparison.
        if (common && texture.height>150) return;
        Layer layer{};
        bool positioned=item>=0 && node.class_name=="WMenuSprite" && node.integer(0x185);
        layer.x=positioned ? cursor_x : node.integer(0x9f)*2;
        layer.y=positioned ? cursor_y : node.integer(0xa0)*2;
        layer.priority=node.integer(0xa3); layer.item=item;
        // 0x488ba0/0x488b20 set/clear the selection mask on an item's plain
        // children (WMenuText/WMenuSprite have their own handlers), so item
        // GDSprites such as the car weapon/gadget icons show only when selected.
        layer.only_one=node.class_name=="WMenuSprite" ? node.integer(0x186)!=0 : item>=0;
        layer.scale=node.real(0xa9,1);
        for (const auto &action:node.actions) if (action.class_name=="WSpriteMove") {
            layer.road=node.id;
            if (!road_.count(node.id)) {
                SpriteMove move;
                move.start=action.integer(0x1b3,320);move.end=action.integer(0x1b4,0);
                move.interval=action.integer(0x1b5,10);move.minimum=action.integer(0x1b6,1);
                move.multiplier=action.real(0x1b7,1);
                if (move.interval<1 || move.interval>10000) throw std::runtime_error("Invalid WSpriteMove interval");
                move.init(node.integer(0x9f),road_random_);
                road_[node.id]=move;
            }
        }
        for (size_t i=0;i<3;++i) layer.color[i]=std::clamp(node.integer(0xa5+uint32_t(i),255),0,255)/255.f;
        if (volume_item_ && node.class_name=="WMenuSprite" && (node.integer(0x189)&0x200000)) {
            layer.notch_item=volume_item_;layer.notch=notch_++;
            for (size_t i=0;i<3;++i) layer.lit[i]=std::clamp(volume_item_->integer(0x190+uint32_t(i),255),0,255)/255.f;
        }
        layer.frames.push_back({{0,0,mat}}); page.layers.push_back(layer);
    } else if (node.class_name=="GDFilm2") {
        if (!films_.count(node.id)) {
            uint32_t filq=node.link(0x51,"FILQ");
            auto table=archive_.payload(archive_.find("FILQ",filq));
            uint32_t sequences=word(table,4);
            if (sequences>64) throw std::runtime_error("Invalid menu film sequence count");
            Film film;
            for (uint32_t i=0;i<sequences;++i) {
                auto resource=archive_.find("REEQ",link(archive_,archive_.find("FILQ",filq),"REEQ",i));
                uint32_t count=word(archive_.payload(resource),4);
                if (count>10000) throw std::runtime_error("Invalid menu film frame count");
                std::vector<std::vector<Placement>> frames;
                // FRMQ lists pieces front to back (Satanas: head, torso, coat,
                // shadow), so they are drawn in reverse: the shadow under the feet.
                for (uint32_t frame=0;frame<count;++frame) {
                    frames.push_back(placements(archive_.find("FRMQ",link(archive_,resource,"FRMQ",frame))));
                    std::reverse(frames.back().begin(),frames.back().end());
                }
                film.initial.lengths.push_back(int(count));
                film.sequences.push_back(std::move(frames));
            }
            // Serialized playback: 0x58 sequence, 0x5a m_AnimType, 0x5b/0x5c
            // m_FirstFrame/m_LastFrame, 0x5d m_Speed; flag 0x2 of 0x1a is the
            // active bit (the Satanas talking head starts inactive).
            FilmState &state=film.initial;
            state.sequence=node.integer(0x58); state.mode=node.integer(0x5a);
            state.first=node.integer(0x5b); state.last=node.integer(0x5c);
            state.speed=node.real(0x5d);
            const float setting=node.real(0x1a); uint32_t bits; std::memcpy(&bits,&setting,4);
            state.active=(bits&2)!=0;
            if (state.sequence<0 || uint32_t(state.sequence)>=sequences || !std::isfinite(state.speed) ||
                std::abs(state.speed)>1000 || state.first<0 || state.last<state.first ||
                state.last>=state.lengths[size_t(state.sequence)])
                throw std::runtime_error("Invalid menu film playback bounds");
            state.accumulator=float(state.first); state.frame=state.first;
            film.state=film.initial;
            films_.emplace(node.id,std::move(film));
            for (const auto &action:node.actions)
                if (action.class_name=="WMenuAnimation") animation_owner_=node.id;
        }
        Layer layer{};
        layer.x=node.integer(0x60)*2; layer.y=node.integer(0x61)*2;
        layer.priority=node.integer(0x62); layer.film=node.id;
        page.layers.push_back(layer);
    } else if (node.class_name=="WMenuText" || (node.class_name=="GDPsText" && node.integer(0x4a))) {
        // A page's GDPsText with a string draws like a fixed WMenuText (the
        // controls page's help #650, #551, #656; Wine-checked).
        // Runtime-provided text/counter remains separate, except an option's
        // value text (0x405f10 sets it; the pause's RETOUR DE FORCE has 0).
        if (!node.integer(0x4a) && !value_owner_) return;
        uint32_t film=node.link(0x35,"FILM");
        uint32_t reel=link(archive_,archive_.find("FILM",film),"REEL",0);
        if (!fonts_.count(reel)) fonts_[reel]=placements(archive_.find("REEL",reel));
        Label label{};
        bool positioned=item>=0 && node.integer(0x17f);
        label.x=positioned ? cursor_x : node.integer(0x45)*2;
        label.y=positioned ? cursor_y : node.integer(0x46)*2;
        label.item=item; label.only_one=node.integer(0x180); label.highlight=positioned || node.integer(0x17e);
        label.align=node.integer(0x4b); label.font=reel; label.scale=node.real(0x3b,1); label.kern=node.integer(0x4e);
        for (size_t i=0;i<3;++i) label.color[i]=std::clamp(node.integer(0x37+uint32_t(i),255),0,255)/255.f;
        if (node.integer(0x4a)) label.text=strings_.get(uint32_t(node.integer(0x4a)));
        {const float f=node.real(0x1a);uint32_t bits;std::memcpy(&bits,&f,4);label.inactive=(bits&2)==0;}
        label.value_item=value_owner_; value_owner_=nullptr; label.source=&node; page.labels.push_back(label);
    }
    if (node.class_name=="WMenuSetVolumeItem") {volume_item_=&node;notch_=0;}
    for (const Node &child:node.children) if (!menu_item_class(child.class_name)) {
        // The second child of an option item carries its value (0x405f10).
        // LUMINOSITE (WMenuSetVolumeItem type 3, no notches) shows its value
        // in its second child, an empty WMenuText (Wine: "50").
        const bool brightness=node.class_name=="WMenuSetVolumeItem" && node.integer(0x193,-1)==3;
        value_owner_=(node.class_name=="WMenuSetValueItem" || node.class_name=="WMenuControllerItem" || brightness) && node.children.size()>1 && &child==&node.children[1] ? &node : nullptr;
        collect(page,child,item,cursor_x,cursor_y,common);
        value_owner_=nullptr;
        if (child.class_name=="WMenuText") { cursor_x+=child.integer(0x181)*2; cursor_y+=child.integer(0x182)*2; }
        // Positioned sprites advance the cursor too (0x187/0x188, as the
        // texts' 0x181/0x182): the ten volume notches stand in a row.
        else if (child.class_name=="WMenuSprite" && child.integer(0x185)) { cursor_x+=child.integer(0x187)*2; cursor_y+=child.integer(0x188)*2; }
    }
    if (node.class_name=="WMenuSetVolumeItem") volume_item_=nullptr;
}
void MenuView::prepare() {
    if (!file_font_) {
        // 0x42e3a0: the first application object of the text class whose
        // descriptor +0x140 (property 0x4c) is 13.
        std::function<const Node*(const Node&)> find=[&](const Node &n) -> const Node* {
            if (n.class_name=="GDPsText" && n.integer(0x4c)==13) return &n;
            for (const Node &c:n.children) if (const Node *f=find(c)) return f;
            return nullptr;
        };
        // general.wgm (the pause) has no file screen and may lack the font.
        if (const Node *font=find(scene_.root())) {
            file_font_=link(archive_,archive_.find("FILM",font->link(0x35,"FILM")),"REEL",0);
            if (!fonts_.count(file_font_)) fonts_[file_font_]=placements(archive_.find("REEL",file_font_));
            // Textures upload outside frame recording: load the glyphs now.
            for (const Placement &glyph:fonts_.at(file_font_)) material(glyph.material);
        }
    }
    const Node *visual=controller_.shown_page();
    if (!visual) throw std::runtime_error("Active menu node has no renderable page");
    active_page_=visual->id;
    models_.prepare(*visual);
    if (pages_.count(active_page_)) return;
    Page page;
    uint32_t background=uint32_t(visual->integer(0x199));
    if (background) collect(page,scene_.find(background),-1,0,0,background==27);
    int x=visual->integer(0x197)*2,y=visual->integer(0x198)*2,index=0;
    for (const Node &child:visual->children) {
        if (menu_item_class(child.class_name)) {
            collect(page,child,index++,x,y);
            x+=child.integer(0x18a)*2; y+=child.integer(0x18b)*2;
        } else collect(page,child);
    }
    // 0x48ba10 -> 0x48bb20: a page's 0x19a = n > 0 sets the text of the
    // world's first child, a GDPsText (menu.WGM #279 at (160,220)), to
    // string 0xc8 + n: the key help line (202 on the main page, 204 on the
    // options; Wine-checked).
    const int help=visual->integer(0x19a);
    const auto &top=scene_.root().children;
    if (help>0 && !top.empty() && top.front().class_name=="GDPsText") {
        const Node &node=top.front();
        Label label{};
        label.font=link(archive_,archive_.find("FILM",node.link(0x35,"FILM")),"REEL",0);
        if (!fonts_.count(label.font)) fonts_[label.font]=placements(archive_.find("REEL",label.font));
        // Centred on x 160 as under Wine; its 0x4b is 0x80800, whose GDPsText
        // meaning (0x800 keeps x for WMenuText) is not traced.
        label.x=node.integer(0x45)*2; label.y=node.integer(0x46)*2; label.align=4;
        label.scale=node.real(0x3b,1); label.source=&node; label.text=strings_.get(uint32_t(0xc8+help));
        for (size_t i=0;i<3;++i) label.color[i]=std::clamp(node.integer(0x37+uint32_t(i),255),0,255)/255.f;
        page.labels.push_back(label);
    }
    std::stable_sort(page.layers.begin(),page.layers.end(),[](const Layer &a,const Layer &b){return a.priority>b.priority;});
    if (animation_owner_ && !animation_) restart_animation();
    wpc_log("Menu page 0x%x: %zu labels, %zu layers, %zu cached textures",active_page_,page.labels.size(),page.layers.size(),textures_.size());
    pages_.emplace(active_page_,std::move(page));
}
void MenuView::restart_animation() {
    for (auto &[id,film]:films_) film.state=film.initial;
    animation_.reset(); updates_=0;
    if (!animation_owner_) return;
    // WMenuAnimation reaches the next two objects after its film (+0x14 links):
    // in the common group, Muttley 0x21f and the Satanas head 0x21b.
    const Node *parent=nullptr;
    std::function<const Node *(const Node &)> find=[&](const Node &n) -> const Node * {
        for (const Node &c:n.children) { if (c.id==animation_owner_) return &n; if (auto *p=find(c)) return p; }
        return nullptr;
    };
    parent=find(scene_.root());
    if (!parent) throw std::runtime_error("Menu animation film has no parent");
    std::vector<uint32_t> next;
    bool after=false;
    for (const Node &c:parent->children) {
        if (after && next.size()<2) next.push_back(c.id);
        if (c.id==animation_owner_) after=true;
    }
    if (next.size()<2 || !films_.count(next[0]) || !films_.count(next[1]))
        throw std::runtime_error("Menu animation needs the two following films");
    animation_=std::make_unique<MenuAnimation>(films_.at(animation_owner_).state,films_.at(next[0]).state,films_.at(next[1]).state);
}
void MenuView::update(double elapsed,const std::function<void()>& per_update) {
    const auto due=uint64_t(std::max(0.0,elapsed)/menu_update_seconds);
    if (due<updates_) restart_animation();
    // Long stalls do not replay more than a second of updates.
    if (due>updates_+30) updates_=due-30;
    while (updates_<due) {
        ++updates_;
        if (per_update) per_update();
        if (animation_) animation_->update();
        for (auto &[id,film]:films_) if (film.state.active) film.state.update();
        for (auto &[id,move]:road_) move.update(road_random_);
    }
}
void MenuView::image(uint32_t mat,int x,int y,float r,float g,float b,float size) {
    const auto &texture=material(mat);
    if (!texture.gpu) return;
    wpc_vk_draw_texture_tinted(window_,offset_x_+int(std::lround(x*scale_)),offset_y_+int(std::lround(y*scale_)),
        int(std::lround(texture.width*scale_*size)),int(std::lround(texture.height*scale_*size)),texture.gpu,r,g,b,1);
}
void MenuView::text(const Label &label,bool selected) {
    const auto &glyphs=fonts_.at(label.font);
    if (glyphs.size()<=size_t('?'-0x20)) throw std::runtime_error("Menu font missing fallback glyph");
    auto index=[&glyphs](unsigned char byte) -> size_t {
        return byte>=0x20 && size_t(byte-0x20)<glyphs.size() ? byte-0x20 : '?'-0x20;
    };
    int width=0;
    for (unsigned char byte:label.text)
        width+=byte<=0x20 ? 10 : int((material(glyphs[index(byte)].material).width-label.kern-1)*label.scale);
    int cursor=label.x;
    // 0x42dfd6: 0x800 leaves X unchanged; otherwise 4 centers and 0x20000 right-aligns.
    if (!(label.align&0x800)) {
        if (label.align&4) cursor-=width/2;
        else if (label.align&0x20000) cursor-=width;
    }
    auto color=selected && label.highlight ? highlight_ : label.color;
    for (unsigned char byte:label.text) {
        if (byte<=0x20) { cursor+=10; continue; }
        const Placement &glyph=glyphs[index(byte)];
        image(glyph.material,cursor+int(glyph.x*label.scale),label.y+int(glyph.y*label.scale),color[0],color[1],color[2],label.scale);
        // 0x42dd83: the glyph width less the descriptor's +0x150 (property
        // 0x4e: 4 for JOUEUR 1/2's CLAVIER) less 1, times the scale.
        cursor+=int((material(glyph.material).width-label.kern-1)*label.scale);
    }
}
void MenuView::draw_files() {
    if (!file_font_) return;
    const FileScreen &f=controller_.files();
    const std::array<float,3> red{{1,0,0}},cream{{248/255.f,222/255.f,172/255.f}},yellow{{240/255.f,240/255.f,0}};
    // 0x49583d: the cursor colour's red and green step by 0x52c034 (+/-20)
    // between 60 and 240 at every draw.
    file_pulse_+=file_step_;
    if (file_pulse_>240) file_step_=-20;
    if (file_pulse_<60) file_step_=20;
    // 320x240 coordinates, doubled; align 4 centres, 0x800 keeps x.
    const auto say=[&](const std::string &s,int x,int y,int align,const std::array<float,3> &color) {
        Label l;l.x=x*2;l.y=y*2;l.align=align;l.font=file_font_;l.color=color;l.text=s;text(l,false);
    };
    const auto string=[&](uint32_t id){return strings_.get(id);};
    if (f.state==2) {say(string(f.mode==0 || f.mode==2 ? 0x8b : 0x8c),160,110,4,cream);say(string(0x91),160,150,4,cream);}
    if (f.state==1) {
        say(string(0x8d),160,40,4,red);
        say(string(0x75),160,90,4,f.answer==0?yellow:cream);
        say(string(0x76),160,110,4,f.answer==1?yellow:cream);
        say(string(0xca),160,220,4,cream);
    }
    if (f.state==4) {say(string(0xea),160,110,4,cream);say(string(0x91),160,150,4,cream);}
    if (f.state!=0 && f.state!=3) return;
    say(string(f.state==3 ? 0x92 : f.mode<2 ? 0x8e : 0x8f),160,10,4,red);
    for (int i=0;i<8;++i) {
        const int y=30+22*i;
        const bool here=i==f.cursor;
        if (here && f.state==3) {
            // Name entry: the text left of centre by a quarter of its width,
            // a '|' at the cursor in the pulsing colour.
            const std::string shown=f.edit+"|";
            Label l;l.font=file_font_;l.align=0x800;l.text=shown;
            int width=0;
            for (unsigned char byte:shown) width+=byte<=0x20 ? 10 : int(material(fonts_.at(file_font_).at(size_t(byte>=0x20 && size_t(byte-0x20)<fonts_.at(file_font_).size()?byte-0x20:'?'-0x20)).material).width-1);
            int x=320-width/2;
            for (size_t k=0;k<shown.size();++k) {
                const bool bar=k+1==shown.size();
                say(std::string(1,shown[k]),x/2,y,0x800,bar ? std::array<float,3>{{file_pulse_/255.f,file_pulse_/255.f,0}} : yellow);
                const unsigned char byte=static_cast<unsigned char>(shown[k]);
                x+=byte<=0x20 ? 10 : int(material(fonts_.at(file_font_).at(size_t(size_t(byte-0x20)<fonts_.at(file_font_).size()?byte-0x20:'?'-0x20)).material).width-1);
            }
            continue;
        }
        const bool used=i<int(f.names.size());
        // Colour 0 (0x495bed) draws white: Wine shows the free rows white.
        say(used ? f.names[size_t(i)] : string(0xe9),160,y,4,here ? yellow : std::array<float,3>{{1,1,1}});
    }
    if (f.save()) say(string(0xc2),160,206,4,cream);
    else if (f.cursor<int(f.names.size())) say(string(0xc1),160,206,4,cream);
    say(string(f.state==3 ? 0xcf : 0xcb),160,220,4,cream);
}
void MenuView::draw_credits() {
    if (!file_font_) return;
    // 0x47f910 with the colours of 0x47f7c0: style 0 white, 1 yellow, 2 red.
    static const std::array<float,3> colours[3]={{{1,1,1}},{{1,1,0}},{{1,0,0}}};
    const auto &lines=controller_.credits();
    const int scroll=controller_.credits_scroll();
    const int first=-(scroll/16);
    int y=scroll%16-16; // C remainder, as the original's signed fix-up
    for (int row=first;row<first+17;++row,y+=16) {
        if (row<0 || row>=int(lines.size())) continue;
        Label l;l.x=320;l.y=y*2;l.align=4;l.font=file_font_;
        l.color=colours[std::clamp(lines[size_t(row)].style,0,2)];l.text=lines[size_t(row)].text;
        text(l,false);
    }
}
void MenuView::draw(double elapsed) {
    int width,height; wpc_window_get_size(window_,&width,&height);
    scale_=std::min(width/640.f,height/480.f);
    offset_x_=(width-int(640*scale_))/2; offset_y_=(height-int(480*scale_))/2;
    if(!overlay_) {
        wpc_vk_clear(window_,0,0,0);
        wpc_vk_draw_rect(window_,offset_x_,offset_y_,int(640*scale_),int(480*scale_),clear_[0],clear_[1],clear_[2],1);
    }
    wpc_vk_set_clip_rect(window_,offset_x_,offset_y_,int(640*scale_),int(480*scale_));
    if (controller_.file_screen()) {draw_files();return;}
    if (controller_.credits_screen()) {draw_credits();return;}
    if (controller_.two_player_page()) {
        // Page 61 under Wine (2026-11-01): one viewport per player, the top
        // and bottom halves; each shows the page at its normal positions
        // for that player's cursor, cut at the half (player 2's moved down
        // 240). The 3D preview is squashed into the half (provisional).
        const int base=offset_y_,half=int(240*scale_);
        for (int p=0;p<2;++p) {
            offset_y_=base+p*half;
            wpc_vk_set_clip_rect(window_,offset_x_,offset_y_,int(640*scale_),half);
            models_.draw(active_page_,controller_.two_cursor(p),width,height,elapsed,0.5f,p?0.5f:-0.5f);
            draw_page(int(controller_.two_cursor(p)),controller_.two_ready_flag(p));
        }
        offset_y_=base;
        wpc_vk_set_clip_rect(window_,offset_x_,offset_y_,int(640*scale_),int(480*scale_));
        return;
    }
    const int cursor=controller_.cursor_shown()?int(controller_.selected()):-1;
    models_.draw(active_page_,size_t(std::max(cursor,0)),width,height,elapsed);
    draw_page(cursor);
}
void MenuView::draw_page(int selected,bool ready) {
    const bool two=controller_.two_player_page();
    const Page &page=pages_.at(active_page_);
    const auto chosen=[&](int item){return item==selected;};
    for (const Layer &layer:page.layers) {
        if (layer.only_one && layer.item>=0 && !chosen(layer.item)) continue;
        const std::vector<Placement> *frame=layer.frames.empty() ? nullptr : &layer.frames.front();
        if (layer.film) {
            const Film &film=films_.at(layer.film);
            if (!film.state.active) continue;
            const auto &frames=film.sequences.at(size_t(film.state.sequence));
            frame=&frames.at(size_t(std::clamp(film.state.frame,0,int(frames.size())-1)));
        }
        // 0x492f80 writes the sprite's X each update (logical units, x2 here).
        const int x=layer.road ? int(road_.at(layer.road).x)*2 : layer.x;
        if (!frame) continue;
        std::array<float,3> color=layer.color;
        if (const int grey=layer.item>=0 ? controller_.grey_level(size_t(layer.item)) : 0) color={{grey/255.f,grey/255.f,grey/255.f}};
        if (layer.notch_item) {
            const Node& it=*layer.notch_item;
            const float maximum=it.real(0x18e,1);
            const float value=maximum>0?controller_.volume(it)/maximum:0;
            if (float(layer.notch)*0.1f<value) color=layer.lit; // 0x511624 steps
        }
        for (const Placement &placement:*frame)
            image(placement.material,x+placement.x,layer.y+placement.y,color[0],color[1],color[2],layer.scale);
    }
    for (const Label &original:page.labels) {
        // A greyed row (grey_level) takes its grey on every text, values too.
        Label label=original;
        if (const int grey=label.item>=0 ? controller_.grey_level(size_t(label.item)) : 0) {
            label.color={{grey/255.f,grey/255.f,grey/255.f}};label.highlight=false;
        }
        if (label.only_one && label.item>=0 && !chosen(label.item)) continue;
        if (two && label.inactive && !ready) continue;
        if (const uint32_t changed=label.source ? controller_.text_override(*label.source) : 0) {
            Label shown=label; shown.text=strings_.get(changed);
            text(shown,label.item>=0 && chosen(label.item));
        } else if (label.value_item && label.value_item->class_name=="WMenuControllerItem") {
            // 0x48eebd: the key's name (DirectInput GetKeyName natively
            // replaced), "?" while the item waits for a key.
            Label shown=label; shown.text=controller_.control_name(*label.value_item);
            text(shown,label.item>=0 && chosen(label.item));
        } else if (label.value_item && label.value_item->class_name=="WMenuSetVolumeItem") {
            // The brightness as an integer (prefs +0x20, 0..100); the
            // original's formatting routine is not traced.
            Label shown=label; shown.text=std::to_string(std::lround(controller_.volume(*label.value_item)));
            text(shown,label.item>=0 && chosen(label.item));
        } else if (label.value_item && !controller_.device_label(*label.value_item).empty()) {
            Label shown=label; shown.text=controller_.device_label(*label.value_item);
            text(shown,label.item>=0 && chosen(label.item));
        } else if (const uint32_t value=label.value_item ? controller_.value_text(*label.value_item) : 0) {
            Label shown=label; shown.text=strings_.get(value);
            text(shown,label.item>=0 && chosen(label.item));
        } else text(label,label.item>=0 && chosen(label.item));
    }
}
}
