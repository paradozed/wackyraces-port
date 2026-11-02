#ifndef WPC_GAME_FILES_H
#define WPC_GAME_FILES_H
#include <filesystem>
#include <string>
#include <vector>

namespace wpc {
// Resolve original Windows asset names component by component on Linux.
// Roots are searched in order; archive resource IDs have a separate policy.
std::filesystem::path game_file(const std::vector<std::filesystem::path>& roots,
                                const std::string& relative);
std::string course_file(int mode, int environment, int course);
// Championship race 0..3 -> {environment, course}, from 0x48de20/0x4928f0
// (first race) and 0x491cf7 (next race): championships 0..3 keep course =
// championship and step the environment cc, sh, dt, ct; championship 4 stays
// in dr (environment 4) and steps the course. Championship I is cc_t1, sh_t1,
// dt_t1, ct_t1.
struct CourseChoice { int environment=0, course=0; };
CourseChoice championship_course(int championship, int race);
constexpr int championship_races=4;
}
#endif
