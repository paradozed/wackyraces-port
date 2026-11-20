#include "linux_joystick.h"
#include <cmath>
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace wpc {
Joystick::~Joystick() {if(fd_>=0) ::close(fd_);}
bool Joystick::open(int index) {
    if(index<0) {open_path("");return false;}
    return open_path("/dev/input/js"+std::to_string(index));
}
bool Joystick::open_path(const std::string& path) {
    if(fd_>=0) {::close(fd_);fd_=-1;}
    axes_={};buttons_={};name_.clear();
    if(path.empty()) return false;
    fd_=::open(path.c_str(),O_RDONLY|O_NONBLOCK);
    if(fd_<0) return false;
    char name[128]={0};
    if(ioctl(fd_,JSIOCGNAME(sizeof name-1),name)<0) name_=path; else name_=name;
    poll();
    return true;
}
void Joystick::poll() {
    if(fd_<0) return;
    js_event e;
    for(;;) {
        const ssize_t n=::read(fd_,&e,sizeof e);
        if(n==ssize_t(sizeof e)) {
            const unsigned type=e.type&~unsigned(JS_EVENT_INIT);
            if(type==JS_EVENT_AXIS && e.number<axes_.size()) axes_[e.number]=e.value;
            else if(type==JS_EVENT_BUTTON && e.number<buttons_.size()) buttons_[e.number]=e.value!=0;
            continue;
        }
        if(n==0 || (n<0 && (errno==EAGAIN || errno==EWOULDBLOCK))) return;
        ::close(fd_);fd_=-1;axes_={};buttons_={};return;   // unplugged
    }
}
int Joystick::axis_byte(size_t axis) const {
    if(axis>=axes_.size()) return 0;
    // DirectInput as the game sets it up (0x4169bd..0x416a80): absolute axes,
    // cooked calibration, range 0..0xffff, saturation 9500 (full scale at 95% of
    // the travel), no dead zone set (DirectInput's default, 0). The joydev value
    // (-32767..32767) is taken as the cooked position; DirectInput's own rounding
    // is not known (provisional). Then 0x42650a/0x426544: (lX - 0x7fff) x 95 /
    // 0x7fff, truncated (the 0x80010003 magic, sar 14): -95..95.
    const double s=std::clamp(double(axes_[axis])/32767.0/0.95,-1.0,1.0);
    const int lx=int(std::floor(32767.5+32767.5*s+0.5));
    return (lx-0x7fff)*95/0x7fff;
}
}
