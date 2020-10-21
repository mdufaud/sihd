import sys as __sys

def is_interactive():
    return __sys.stdin and __sys.stdin.isatty()
