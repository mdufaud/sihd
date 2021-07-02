#ifndef __SIHD_UTIL_TERM_HPP__
# define __SIHD_UTIL_TERM_HPP__

# include <stdio.h>
# include <unistd.h>
# include <sihd/util/Str.hpp>

namespace sihd::util
{

class Term
{
    public:

        static bool is_interactive()
        {
            return isatty(fileno(stdin));
        }

        struct Attr
        {
            static const char *UNDERLINE;
            static const char *BOLD;
            static const char *ITALIC;
            static const char *URL;
            static const char *BLINK;
            static const char *BLINK2;
            static const char *SELECTED;
            static const char *BLACK;
            static const char *RED;
            static const char *GREEN;
            static const char *YELLOW;
            static const char *BLUE;
            static const char *VIOLET;
            static const char *BEIGE;
            static const char *WHITE;
            static const char *BLACKBG;
            static const char *REDBG;
            static const char *GREENBG;
            static const char *YELLOWBG;
            static const char *BLUEBG;
            static const char *VIOLETBG;
            static const char *BEIGEBG;
            static const char *WHITEBG;
            static const char *GREY;
            static const char *RED2;
            static const char *GREEN2;
            static const char *YELLOW2;
            static const char *BLUE2;
            static const char *VIOLET2;
            static const char *BEIGE2;
            static const char *WHITE2;
            static const char *GREYBG;
            static const char *REDBG2;
            static const char *GREENBG2;
            static const char *YELLOWBG2;
            static const char *BLUEBG2;
            static const char *VIOLETBG2;
            static const char *BEIGEBG2;
            static const char *WHITEBG2;
            static const char *ENDC;
        };

        static std::string fmt(const std::string & str, const char *attr)
        {
            return Str::format("%s%s%s", attr, str.c_str(), Attr::ENDC);
        }

        static std::string fmt2(const std::string & str, const char *attr, const char *attr2)
        {
            return Str::format("%s%s%s%s", attr, attr2, str.c_str(), Attr::ENDC);
        }

        static std::string underline(const std::string & str) { return Term::fmt(str, Attr::UNDERLINE); }
        static std::string bold(const std::string & str) { return Term::fmt(str, Attr::BOLD); }
        static std::string selected(const std::string & str) { return Term::fmt(str, Attr::SELECTED); }
        static std::string blink(const std::string & str) { return Term::fmt(str, Attr::BLINK); }

        static std::string red(const std::string & str) { return Term::fmt(str, Attr::RED2); }
        static std::string green(const std::string & str) { return Term::fmt(str, Attr::GREEN2); }

        static std::string white_bg(const std::string & str) { return Term::fmt2(str, Attr::WHITEBG, Attr::BLACK); }


    private:
        Term();
        virtual ~Term() {};
};

const char *Term::Attr::UNDERLINE = "\033[4m";
const char *Term::Attr::BOLD     = "\33[1m";
const char *Term::Attr::ITALIC   = "\33[3m";
const char *Term::Attr::URL      = "\33[4m";
const char *Term::Attr::BLINK    = "\33[5m";
const char *Term::Attr::BLINK2   = "\33[6m";
const char *Term::Attr::SELECTED = "\33[7m";

const char *Term::Attr::BLACK  = "\33[30m";
const char *Term::Attr::RED    = "\33[31m";
const char *Term::Attr::GREEN  = "\33[32m";
const char *Term::Attr::YELLOW = "\33[33m";
const char *Term::Attr::BLUE   = "\33[34m";
const char *Term::Attr::VIOLET = "\33[35m";
const char *Term::Attr::BEIGE  = "\33[36m";
const char *Term::Attr::WHITE  = "\33[37m";
const char *Term::Attr::GREY   = "\33[90m";

const char *Term::Attr::BLACKBG  = "\33[40m";
const char *Term::Attr::REDBG    = "\33[41m";
const char *Term::Attr::GREENBG  = "\33[42m";
const char *Term::Attr::YELLOWBG = "\33[43m";
const char *Term::Attr::BLUEBG   = "\33[44m";
const char *Term::Attr::VIOLETBG = "\33[45m";
const char *Term::Attr::BEIGEBG  = "\33[46m";
const char *Term::Attr::WHITEBG  = "\33[47m";
const char *Term::Attr::GREYBG    = "\33[100m";

const char *Term::Attr::RED2    = "\33[91m";
const char *Term::Attr::GREEN2  = "\33[92m";
const char *Term::Attr::YELLOW2 = "\33[93m";
const char *Term::Attr::BLUE2   = "\33[94m";
const char *Term::Attr::VIOLET2 = "\33[95m";
const char *Term::Attr::BEIGE2  = "\33[96m";
const char *Term::Attr::WHITE2  = "\33[97m";

const char *Term::Attr::REDBG2    = "\33[101m";
const char *Term::Attr::GREENBG2  = "\33[102m";
const char *Term::Attr::YELLOWBG2 = "\33[103m";
const char *Term::Attr::BLUEBG2   = "\33[104m";
const char *Term::Attr::VIOLETBG2 = "\33[105m";
const char *Term::Attr::BEIGEBG2  = "\33[106m";
const char *Term::Attr::WHITEBG2  = "\33[107m";

const char *Term::Attr::ENDC    = "\033[0m";

}

#endif 