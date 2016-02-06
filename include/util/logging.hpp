#ifndef __UTIL_LOGGING_HPP
#define __UTIL_LOGGING_HPP

#include <map>
#include <string>
#include <fstream>

namespace util
{

typedef std::ostream LoggingStream;

enum LoggingLevel {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARNING = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_DEBUG = 3
};

class LoggingManager
{
public:
    typedef std::map<std::string, LoggingStream*> LoggerMap;

public:
    LoggingManager();
    ~LoggingManager();

private:
    LoggingStream& _lookup_logger(const std::string& key);

public:
    LoggingStream& log();
    LoggingStream& log(const char* key);
    LoggingStream& log(const std::string& key);
    LoggingStream& error();
    LoggingStream& error(const char* key);
    LoggingStream& error(const std::string& key);
    LoggingStream& warning();
    LoggingStream& warning(const char* key);
    LoggingStream& warning(const std::string& key);
    LoggingStream& info();
    LoggingStream& info(const char* key);
    LoggingStream& info(const std::string& key);
    LoggingStream& debug();
    LoggingStream& debug(const char* key);
    LoggingStream& debug(const std::string& key);
    void make_logger(const char* key, const char* path);
    void make_logger(const std::string& key, const std::string& path);
    void make_default(const char* key);
    void make_default(const std::string& key);
    void make_cout_default();
    void make_cerr_default();
    void make_null_default();
    void set_logging_level(LoggingLevel lvl);

private:
    LoggerMap _loggers;
    LoggingStream* _default_logger;
    LoggingStream _null_logger;
    LoggingLevel _logging_level;
};

extern LoggingManager logging;

}

// vim: set sts=2 sw=2 expandtab:
#endif
