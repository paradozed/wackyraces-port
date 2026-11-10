#ifndef WPC_GAME_CONFIG_H
#define WPC_GAME_CONFIG_H
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace wpc {
// Native port settings file (INI style):
//
//   [paths]
//   data = ~/.wine/drive_c/Program Files (x86)/Infogrames/Wacky Races
//   disc = /run/media/you/Wacky
//   [game]
//   language = 3
//   skip_intro = true      # legal.avi, the L2/L3 cards and the intro movie
//   dust = true            # the POUSSIERE option
//   [video]
//   resolution = 1280x960  # window size; unset keeps the default 640x480
//   fullscreen = true      # the window covers the screen (picture stays 4:3)
//
// '#' and ';' start comments. A leading '~' is the home directory; relative
// paths are taken from the config file's directory. Unknown sections or keys
// are errors, so a misspelt key is not silently ignored.
struct GameConfig {
    std::optional<std::filesystem::path> data, disc;
    std::optional<unsigned> language;
    // true/false, yes/no, on/off or 1/0.
    std::optional<bool> skip_intro;
    // The menu's POUSSIERE option (0x6f4650, off by default): WCarFx's extra
    // rear-wheel dust (0x47b750).
    std::optional<bool> dust;
    std::optional<std::pair<int,int>> resolution;
    std::optional<bool> fullscreen;
    // [input] joystick1 / joystick2: the Linux joystick (/dev/input/jsN) a
    // player drives with, as the original's MenuPrefs device +0x18 / +0x1c
    // (-1, the default, is the keyboard). Native: the menu cannot pick it.
    std::optional<int> joystick1,joystick2;
};
// "WIDTHxHEIGHT", each 320..16384; throws std::runtime_error otherwise.
std::pair<int,int> parse_resolution(const std::string& text);
GameConfig parse_config(const std::string& text,const std::filesystem::path& base,
                        const std::string& source="config");
GameConfig load_config(const std::filesystem::path& path);
// $WPC_CONFIG, else $XDG_CONFIG_HOME/wackyraces/config.ini, else
// $HOME/.config/wackyraces/config.ini; empty without HOME or XDG_CONFIG_HOME.
std::filesystem::path default_config_path();
}
#endif
