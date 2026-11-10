#ifndef WPC_LINUX_JOYSTICK_H
#define WPC_LINUX_JOYSTICK_H
#include <array>
#include <cstdint>
#include <string>

namespace wpc {
// A Linux joystick (/dev/input/jsN, linux/joystick.h), read without blocking.
// The original reads DirectInput joysticks (device entries +0x18/+0x1c in
// MenuPrefs): the first two axes as signed bytes (0x4399b0: beyond +-64 they
// give the direction bits 0x8000/0x2000 left/right and 0x1000/0x4000 up/down)
// and its buttons through the control table (entry + 3, 0x40ca65..).
class Joystick {
public:
    ~Joystick();
    // Opens /dev/input/js<index>; false (and closed) when it cannot.
    bool open(int index);
    bool open_path(const std::string& path);   // the same for a device path (tests: an event file)
    bool is_open() const {return fd_>=0;}
    // Drains the pending events; on a read error the device closes.
    void poll();
    // The original's byte scale (-128..127) of axis 0 (x) and 1 (y).
    int axis_byte(size_t axis) const;
    bool button(int number) const {return number>=0 && number<int(buttons_.size()) && buttons_[size_t(number)];}
    const std::string& name() const {return name_;}
private:
    int fd_=-1;
    std::array<int,8> axes_{};
    std::array<bool,32> buttons_{};
    std::string name_;
};
// 0x4966f0's joystick defaults for the control table (accelerate, brake,
// left, right, view, look back, gadget, weapon): buttons 4, 5, none (100: the
// x axis steers), 0, 1, 2, 3.
constexpr std::array<int,8> joystick_default_buttons{{4,5,100,100,0,1,2,3}};
}
#endif
