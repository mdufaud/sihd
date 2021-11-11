
#include <sihd/util/Term.hpp>

namespace sihd::util
{

const char *Term::Attr::UNDERLINE = "\033[4m";
const char *Term::Attr::BOLD = "\33[1m";
const char *Term::Attr::ITALIC = "\33[3m";
const char *Term::Attr::URL = "\33[4m";
const char *Term::Attr::BLINK = "\33[5m";
const char *Term::Attr::BLINK2 = "\33[6m";
const char *Term::Attr::SELECTED = "\33[7m";

const char *Term::Attr::BLACK = "\33[30m";
const char *Term::Attr::RED = "\33[31m";
const char *Term::Attr::GREEN = "\33[32m";
const char *Term::Attr::YELLOW = "\33[33m";
const char *Term::Attr::BLUE  = "\33[34m";
const char *Term::Attr::VIOLET = "\33[35m";
const char *Term::Attr::BEIGE = "\33[36m";
const char *Term::Attr::WHITE = "\33[37m";
const char *Term::Attr::GREY  = "\33[90m";

const char *Term::Attr::BLACKBG = "\33[40m";
const char *Term::Attr::REDBG = "\33[41m";
const char *Term::Attr::GREENBG = "\33[42m";
const char *Term::Attr::YELLOWBG = "\33[43m";
const char *Term::Attr::BLUEBG  = "\33[44m";
const char *Term::Attr::VIOLETBG = "\33[45m";
const char *Term::Attr::BEIGEBG = "\33[46m";
const char *Term::Attr::WHITEBG = "\33[47m";
const char *Term::Attr::GREYBG = "\33[100m";

const char *Term::Attr::RED2 = "\33[91m";
const char *Term::Attr::GREEN2 = "\33[92m";
const char *Term::Attr::YELLOW2 = "\33[93m";
const char *Term::Attr::BLUE2  = "\33[94m";
const char *Term::Attr::VIOLET2 = "\33[95m";
const char *Term::Attr::BEIGE2 = "\33[96m";
const char *Term::Attr::WHITE2 = "\33[97m";

const char *Term::Attr::REDBG2 = "\33[101m";
const char *Term::Attr::GREENBG2 = "\33[102m";
const char *Term::Attr::YELLOWBG2 = "\33[103m";
const char *Term::Attr::BLUEBG2  = "\33[104m";
const char *Term::Attr::VIOLETBG2 = "\33[105m";
const char *Term::Attr::BEIGEBG2 = "\33[106m";
const char *Term::Attr::WHITEBG2 = "\33[107m";

const char *Term::Attr::ENDC = "\033[0m";

}