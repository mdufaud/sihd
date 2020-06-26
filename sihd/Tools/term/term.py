#!/usr/bin/python
# coding: utf-8

class TermColors:

    UNDERLINE = '\033[4m'

    BOLD     = '\33[1m'
    ITALIC   = '\33[3m'
    URL      = '\33[4m'
    BLINK    = '\33[5m'
    BLINK2   = '\33[6m'
    SELECTED = '\33[7m'

    BLACK  = '\33[30m'
    RED    = '\33[31m'
    GREEN  = '\33[32m'
    YELLOW = '\33[33m'
    BLUE   = '\33[34m'
    VIOLET = '\33[35m'
    BEIGE  = '\33[36m'
    WHITE  = '\33[37m'

    BLACKBG  = '\33[40m'
    REDBG    = '\33[41m'
    GREENBG  = '\33[42m'
    YELLOWBG = '\33[43m'
    BLUEBG   = '\33[44m'
    VIOLETBG = '\33[45m'
    BEIGEBG  = '\33[46m'
    WHITEBG  = '\33[47m'

    GREY    = '\33[90m'
    RED2    = '\33[91m'
    GREEN2  = '\33[92m'
    YELLOW2 = '\33[93m'
    BLUE2   = '\33[94m'
    VIOLET2 = '\33[95m'
    BEIGE2  = '\33[96m'
    WHITE2  = '\33[97m'

    GREYBG    = '\33[100m'
    REDBG2    = '\33[101m'
    GREENBG2  = '\33[102m'
    YELLOWBG2 = '\33[103m'
    BLUEBG2   = '\33[104m'
    VIOLETBG2 = '\33[105m'
    BEIGEBG2  = '\33[106m'
    WHITEBG2  = '\33[107m'

    ENDC    = '\033[0m'


    @staticmethod
    def violet(s):
        return TermColors.color(s, TermColors.VIOLET2)

    @staticmethod
    def gold(s):
        return TermColors.color(s, TermColors.YELLOW)

    @staticmethod
    def blue(s):
        return TermColors.color(s, TermColors.BLUE2)

    @staticmethod
    def green(s):
        return TermColors.color(s, TermColors.GREEN2)

    @staticmethod
    def bold(s):
        return TermColors.color(s, TermColors.BOLD)

    @staticmethod
    def underline(s):
        return TermColors.color(s, TermColors.UNDERLINE)

    @staticmethod
    def red(s):
        return TermColors.color(s, TermColors.RED2)

    @staticmethod
    def yellow(s):
        return TermColors.color(s, TermColors.YELLOW2)

    @staticmethod
    def color(s, color):
        return color + s + TermColors.ENDC

    @staticmethod
    def test():
        for key, value in TermColors.__dict__.items():
            if isinstance(value, basestring):
                print("{color}{name}{end}".format(color=value, name=key, end=TermColors.ENDC))

class Term(object):
    colors = TermColors

term = Term()
