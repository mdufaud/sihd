from sihd.Tools.sys.const import LINUX

def get_current_mem():
    ''' Memory usage in kB '''

    if LINUX:
        with open('/proc/self/status') as f:
            memusage = f.read().split('VmRSS:')[1].split('\n')[0][:-3]

        return int(memusage.strip())
    return 0
