# Time
import time
build_start_time = time.time()

###############################################################################
# Addons
###############################################################################

try:
    import addon.scripts
except ImportError:
    pass
