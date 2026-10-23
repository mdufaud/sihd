#ifndef __SIHD_SYS_FWD_HPP__
#define __SIHD_SYS_FWD_HPP__

#include <sihd/sys/platform.hpp>

namespace sihd::sys
{

class Bitmap;
enum class Cap;
class CapabilitySet;
class CrashHandler;
class Daemon;
class DesktopNotifier;
class DynLib;
class File;
class FileCache;
class FileMutex;
class FilePoller;
class FileWatcher;
class Impersonation;
class LineReader;
class LoggerFile;
class LoggerSystem;
class MemLock;
class MessageQueue;
class PathManager;
class PluginLoader;
class Poll;
class Process;
class ProcessInfo;
class ProcessMonitor;
class Pty;
class Semaphore;
class SharedMemory;
class SigHandler;
#if !defined(__SIHD_WINDOWS__)
class SigThreadBlocker;
#endif
class SigWaiter;
class SigWatcher;
class TmpDir;
class Uuid;

namespace info
{
class CpuUsage;
}

namespace linux_
{
class EventFd;
class TimerFd;
} // namespace linux_

namespace windows_
{
class ComInit;
class Job;

namespace registry
{
class Key;
}

namespace scm
{
class Service;
}

} // namespace windows_

} // namespace sihd::sys

#endif
