#include "game_scene.h"

#include <cstdio>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <stdexcept>

namespace {
using Bytes = std::vector<uint8_t>;
size_t checks = 0;
void require(bool value, const char* message) {
    ++checks; if (!value) throw std::runtime_error(message);
}
void rejected(const std::function<void()>& action, const char* message) {
    bool failed = false;
    try { action(); } catch (const std::runtime_error&) { failed = true; }
    require(failed, message);
}
void put32(Bytes& b, size_t p, uint32_t n) {
    for (int i = 0; i < 4; ++i) b.at(p+i) = uint8_t(n >> (8*i));
}
void append(Bytes& b, const Bytes& c) { b.insert(b.end(), c.begin(), c.end()); }
Bytes word(uint32_t n) { Bytes b(4); put32(b, 0, n); return b; }
Bytes chunk(const char* tag, const Bytes& payload) {
    Bytes b(8); std::memcpy(b.data(), tag, 4); put32(b, 4, uint32_t(payload.size())); append(b, payload); return b;
}
Bytes property(uint32_t key, const Bytes& value) {
    auto b = word(key | 0x08000000); append(b, value); return chunk("PROP", b);
}
Bytes node(uint32_t id, uint32_t type, const Bytes& properties) {
    auto b = word(type); append(b, word(id)); append(b, chunk("PRPS", properties)); return b;
}
Bytes classes(std::initializer_list<const char*> names = {"GDWorld", "WMenuPage"}) {
    auto b = word(uint32_t(names.size()));
    for (const char* name : names) {
        const uint32_t length = uint32_t(std::strlen(name)+1);
        append(b, word(length)); b.insert(b.end(), name, name+length);
    }
    while (b.size() & 3) b.push_back(0);
    return b;
}
wpc::Archive archive(const Bytes& tree, const Bytes& names = classes(),
                     const Bytes& action_names = {}, const Bytes& screen = {}) {
    Bytes b(8); std::memcpy(b.data(), "WPCG", 4); put32(b, 4, 0x02020000);
    auto class_chunk = chunk("BRCM", names);
    if (!action_names.empty()) append(class_chunk, chunk("ACCM", action_names));
    append(b, chunk("RSRC", {})); append(b, chunk("CLAS", class_chunk)); append(b, chunk("BRTR", tree));
    if (!screen.empty()) append(b, chunk("SCRT", screen));
    wpc::Archive result; result.load_bytes(std::move(b)); return result;
}

void test_scene() {
    auto props = property(0x197, word(uint32_t(-12)));
    append(props, property(0x57, word(0x3fc00000)));
    Bytes link{'F','I','L','M'}; append(link, word(99)); append(props, property(0x35, link));
    auto tree = node(1, 0x01075000, props);
    append(tree, chunk("CHBR", node(4, 0x0104c001, {})));
    append(tree, chunk("AUXI", {1,2,3,4}));
    const auto ar = archive(tree);
    wpc::Scene scene; scene.load(ar);
    require(scene.root().id == 1 && scene.root().class_name == "GDWorld", "scene root/class index");
    require(scene.find(4).class_name == "WMenuPage", "scene child/class index");
    require(scene.root().integer(0x197) == -12, "signed scene integer");
    require(scene.root().integer(0x999, 88) == 88, "missing integer default");
    require(scene.root().real(0x57) == 1.5f && scene.root().real(0x998, 2.0f) == 2.0f, "float property/default");
    require(scene.root().link(0x35, "FILM") == 99, "typed resource link");
    require(scene.root().extra_chunks.size() == 1 && scene.root().extra_chunks[0].second == Bytes({1,2,3,4}),
            "unknown auxiliary bytes retained");
    rejected([&] { scene.screen(); }, "absent SCRT tree accepted");
    rejected([&] { scene.find(999); }, "missing scene ID accepted");
    rejected([&] { scene.root().link(0x35, "GMAT"); }, "resource-link type mismatch accepted");
    rejected([&] { scene.root().link(0x123, "FILM"); }, "missing resource link accepted");
    rejected([&] { scene.root().integer(0x35); }, "non-scalar integer property accepted");
    auto invalid = node(1, 0, {}); append(invalid, chunk("CHBR", node(1, 1, {})));
    rejected([&] { scene.load(archive(invalid)); }, "duplicate scene ID accepted");
    require(scene.find(4).class_name == "WMenuPage", "failed scene reload replaced state");
    invalid = node(1, 2, {});
    rejected([&] { scene.load(archive(invalid)); }, "unknown class index accepted");
    invalid = node(1, 0, property(7, word(1))); append(invalid, chunk("PRPS", {}));
    rejected([&] { scene.load(archive(invalid)); }, "duplicate PRPS accepted");
    invalid = node(1, 0, property(7, word(1)));
    auto repeated = property(7, word(1)); append(repeated, property(7, word(2))); invalid = node(1, 0, repeated);
    rejected([&] { scene.load(archive(invalid)); }, "duplicate property key accepted");
    invalid = node(1, 0, chunk("NOPE", {}));
    rejected([&] { scene.load(archive(invalid)); }, "unexpected PRPS entry accepted");
    invalid = node(1, 0, chunk("PROP", {1,2,3}));
    rejected([&] { scene.load(archive(invalid)); }, "short property token accepted");
    invalid = word(0); append(invalid, word(1));
    rejected([&] { scene.load(archive(invalid)); }, "missing PRPS accepted");
    invalid = word(0); append(invalid, word(1)); append(invalid, chunk("CHBR", node(2, 1, {})));
    append(invalid, chunk("PRPS", {}));
    rejected([&] { scene.load(archive(invalid)); }, "child preceding PRPS accepted");
    invalid = node(1, 0, {}); invalid.push_back(0);
    rejected([&] { scene.load(archive(invalid)); }, "unframed node tail accepted");
    // Every prefix shorter than the required PRPS is invalid.
    const auto minimal = node(1, 0, {});
    for (size_t n = 0; n < minimal.size(); ++n) {
        invalid.assign(minimal.begin(), minimal.begin()+n);
        rejected([&] { scene.load(archive(invalid)); }, "truncated node accepted");
    }
    auto names = classes(); put32(names, 0, 0xffffffff);
    rejected([&] { scene.load(archive(tree, names)); }, "class count overflow accepted");
    names = classes(); put32(names, 4, 0xffffffff);
    rejected([&] { scene.load(archive(tree, names)); }, "class length overflow accepted");
    names = classes(); names[15] = 'x';
    rejected([&] { scene.load(archive(tree, names)); }, "unterminated class name accepted");
    invalid = node(1000, 0, {});
    for (uint32_t i = 0; i < 130; ++i) {
        auto parent = node(i, 0, {}); append(parent, chunk("CHBR", invalid)); invalid = std::move(parent);
    }
    rejected([&] { scene.load(archive(invalid)); }, "excessive scene depth accepted");
}

void test_actions_and_screen() {
    const auto names = classes({"GDWorld", "WMenuPage", "GDScreen", "GDViewport"});
    const auto action_names = classes({"WMenu", "WRot"});
    const auto action = node(0x13f, 0x0202c000, property(0x1a5, word(240)));
    auto tree = node(0x13e, 0, {}); append(tree, chunk("ACTN", action));
    auto screen = node(3, 2, {}); append(screen, chunk("CHBR", node(0x25, 3, {})));
    wpc::Scene scene; scene.load(archive(tree, names, action_names, screen));
    require(scene.root().actions.size() == 1 && scene.root().actions[0].class_name == "WMenu",
            "ACTN uses ACCM class namespace");
    require(scene.find(0x13f).integer(0x1a5) == 240, "action properties/find");
    require(scene.root().extra_chunks.size() == 1 && scene.root().extra_chunks[0].first == "ACTN" &&
            scene.root().extra_chunks[0].second == action, "decoded ACTN also preserves original bytes");
    require(scene.screen().class_name == "GDScreen" && scene.find(0x25).class_name == "GDViewport",
            "SCRT uses BRCM class namespace/find");
    require(scene.screen().id == 3 && scene.screen().children.size() == 1, "screen child tree");
    auto invalid = node(0x13e, 0, {}); append(invalid, chunk("ACTN", node(0x13e, 0, {})));
    rejected([&] { scene.load(archive(invalid, names, action_names, screen)); }, "duplicate brick/action ID accepted");
    invalid = node(0x13e, 0, {}); append(invalid, chunk("ACTN", node(0x13f, 2, {})));
    rejected([&] { scene.load(archive(invalid, names, action_names, screen)); }, "action index from BRCM accepted");
    invalid = node(0x13e, 0, {}); append(invalid, chunk("ACTN", node(0x13f, 0, {})));
    rejected([&] { scene.load(archive(invalid, names, {}, screen)); }, "action without ACCM accepted");
    rejected([&] { scene.load(archive(tree, names, action_names, node(0x13f, 2, {}))); },
             "duplicate action/screen ID accepted");
    rejected([&] { scene.load(archive(tree, names, action_names, node(3, 4, {}))); },
             "out-of-range screen class accepted");
    for (size_t n = 0; n < action.size(); ++n) {
        invalid = node(0x13e, 0, {});
        append(invalid, chunk("ACTN", Bytes(action.begin(), action.begin()+n)));
        rejected([&] { scene.load(archive(invalid, names, action_names)); }, "truncated ACTN accepted");
    }
    const auto minimal_screen = node(3, 2, {});
    for (size_t n = 1; n < minimal_screen.size(); ++n) {
        const Bytes prefix(minimal_screen.begin(), minimal_screen.begin()+n);
        rejected([&] { scene.load(archive(tree, names, action_names, prefix)); }, "truncated SCRT accepted");
    }
    require(scene.find(0x13f).class_name == "WMenu" && scene.screen().id == 3,
            "failed action/screen reload retains state");
}

Bytes strings() {
    // Two languages, both with ID 14; first also has a duplicate 14 and ID 7.
    Bytes b(28+3*8+8, 0); put32(b, 0, 2);
    put32(b, 4, 3); put32(b, 8, 3); put32(b, 12, 28);
    put32(b, 16, 0); put32(b, 20, 1); put32(b, 24, 52);
    const auto text = [&](const Bytes& value) { const uint32_t offset = uint32_t(b.size()); append(b,value); b.push_back(0); return offset; };
    put32(b, 28, 14); const auto french = text({'D'^0x7f,'E'^0x7f,'M'^0x7f}); put32(b, 32, french);
    put32(b, 36, 14); const auto duplicate = text({'X'^0x7f}); put32(b, 40, duplicate);
    put32(b, 44, 7); const auto special = text({0x7f,0xff}); put32(b, 48, special);
    put32(b, 52, 14); const auto english = text({'S'^0x7f,'T'^0x7f,'A'^0x7f,'R'^0x7f,'T'^0x7f}); put32(b, 56, english);
    return b;
}

void test_strings() {
    const auto b = strings();
    wpc::Strings table; table.load_bytes(b, 3);
    require(table.language() == 3 && table.size() == 2, "selected language/distinct keys");
    require(table.get(14) == "DEM", "XOR decoding and first duplicate key wins");
    require(table.get(7).size() == 2 && uint8_t(table.get(7)[0]) == 0x7f && uint8_t(table.get(7)[1]) == 0x80,
            "raw 0x7f preservation/non-ASCII decoding");
    table.load_bytes(b, 0);
    require(table.get(14) == "START", "language ID selection");
    table.load_bytes(b, 99);
    require(table.language() == 3 && table.get(14) == "DEM", "unknown language falls back to first record");
    rejected([&] { table.get(999); }, "missing string accepted");
    for (size_t n = 0; n < b.size(); ++n) {
        const Bytes prefix(b.begin(), b.begin()+n);
        rejected([&] { table.load_bytes(prefix, 3); }, "truncated string table accepted");
    }
    require(table.get(14) == "DEM", "failed string reload replaced state");
    auto invalid = b; put32(invalid, 0, 0xffffffff);
    rejected([&] { table.load_bytes(invalid, 3); }, "language count overflow accepted");
    invalid = b; put32(invalid, 8, 0xffffffff);
    rejected([&] { table.load_bytes(invalid, 3); }, "language entry count overflow accepted");
    invalid = b; put32(invalid, 12, 0xffffffff);
    rejected([&] { table.load_bytes(invalid, 3); }, "language table offset overflow accepted");
    invalid = b; put32(invalid, 24, 28);
    rejected([&] { table.load_bytes(invalid, 3); }, "overlapping language tables accepted");
    invalid = b; put32(invalid, 16, 3);
    rejected([&] { table.load_bytes(invalid, 3); }, "duplicate language ID accepted");
    invalid = b; put32(invalid, 32, 4);
    rejected([&] { table.load_bytes(invalid, 3); }, "string pointing into header accepted");
    invalid = b; put32(invalid, 32, 0xffffffff);
    rejected([&] { table.load_bytes(invalid, 3); }, "string offset overflow accepted");
    invalid = b; invalid.back() = 1;
    rejected([&] { table.load_bytes(invalid, 3); }, "unterminated unselected language string accepted");
}

size_t count_nodes(const wpc::Node& node) {
    size_t count = 1; for (const auto& child : node.children) count += count_nodes(child); return count;
}
size_t count_actions(const wpc::Node& node) {
    size_t count = node.actions.size(); for (const auto& child : node.children) count += count_actions(child); return count;
}
} // namespace

int main(int argc, char** argv) {
    try {
        test_scene(); test_actions_and_screen(); test_strings();
        std::printf("native scene/strings: %zu checks passed\n", checks);
        if (argc > 1) {
            wpc::Archive archive; archive.load(argv[1]); wpc::Scene scene; scene.load(archive);
            std::printf("%s: %zu brick nodes, %zu actions, root %u %s\n", argv[1], count_nodes(scene.root()), count_actions(scene.root()),
                        scene.root().id, scene.root().class_name.c_str());
            std::printf("SCRT: %zu nodes, root %u %s\n", count_nodes(scene.screen()), scene.screen().id, scene.screen().class_name.c_str());
            // The fixed property IDs the port reads, against the archive's own
            // CLAS/PRIM member table.
            const auto members = wpc::member_names(archive);
            const std::pair<uint32_t, const char*> named[] = {
                {0x09,"m_MaterialID"},{0x0d,"m_Scale"},{0x0e,"m_AspectR"},{0x10,"m_RenderSetting"},{0x1a,"m_nFlags"},
                {0x36,"m_RenderSetting"},{0x4a,"m_TextID"},{0x65,"m_MotionLib"},{0x67,"m_3dModelID"},{0x68,"m_RenderSetting"},
                {0x6a,"m_VcRadius"},{0x87,"m_SensorGroup"},{0x1cf,"m_MaxRotDegX"},{0x1d5,"m_lType"},{0x28b,"m_poweruptype"}};
            std::string shifted;
            for (const auto& [id, name] : named)
                if (id >= members.size() || members[id].first != name) shifted += " 0x" + [](uint32_t v){char b[16];std::snprintf(b,16,"%x",v);return std::string(b);}(id);
            // Disc Win/*.WGM lack seven test-class members, so their IDs from
            // 0xd5 on are shifted: with a reference archive (argv[3], such as
            // menu.WGM) the scene maps them by name.
            if (!shifted.empty()) {
                if (argc < 4) throw std::runtime_error("member table differs from the port's fixed IDs at" + shifted + " (pass a reference archive)");
                wpc::Archive reference; reference.load(argv[3]);
                const auto canonical = wpc::member_names(reference);
                wpc::Scene mapped; mapped.load(archive, &canonical);
                size_t sprites = 0, movies = 0;
                std::function<void(const wpc::Node&)> walk = [&](const wpc::Node& n) {
                    if (n.class_name == "GDSprite") { n.link(0x9e, "GMAT"); ++sprites; }
                    // WWinnerMovie's members sit past the gap (0x1b8..0x1bb).
                    if (n.class_name == "WWinnerMovie") { require(n.properties.count(0x1b8) && n.properties.count(0x1bb), "WWinnerMovie members mapped"); ++movies; }
                    for (const auto& c : n.children) walk(c);
                    for (const auto& a : n.actions) walk(a);
                };
                walk(mapped.root());
                require(sprites > 0 && movies == 1, "mapped GDSprites keep their material links (0x9e); one WWinnerMovie");
                std::printf("member table: %zu entries, mapped by name onto %zu; %zu GDSprites resolve 0x9e\n", members.size(), canonical.size(), sprites);
                return 0;
            }
            std::printf("member table: %zu entries; 0x68 = %s::%s\n", members.size(), members[0x68].second.c_str(), members[0x68].first.c_str());
        }
        if (argc > 2) {
            for (uint32_t language = 0; language < 7; ++language) {
                wpc::Strings table; table.load(argv[2], language);
                std::printf("language %u: %zu distinct IDs; string14=%s\n", table.language(), table.size(), table.get(14).c_str());
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "native scene/strings: %s (after %zu checks)\n", error.what(), checks); return 1;
    }
}
