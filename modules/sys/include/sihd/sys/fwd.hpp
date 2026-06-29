#ifndef __SIHD_SYS_FWD_HPP__
#define __SIHD_SYS_FWD_HPP__

#include <sihd/sys/platform.hpp>

namespace sihd::sys
{

class Bitmap;
class Daemon;
class DynLib;
class File;
class FileCache;
class FileMutex;
class FilePoller;
class FileWatcher;
class LineReader;
class LoggerFile;
class LoggerSystem;
class PathManager;
class PluginLoader;
class Poll;
class Process;
class ProcessInfo;
class Pty;
class SharedMemory;
class SigHandler;
#if !defined(__SIHD_WINDOWS__)
class SigThreadBlocker;
#endif
class SigWaiter;
class SigWatcher;
class TmpDir;
class Uuid;

} // namespace sihd::sys

#endif
