#include <iostream>
#include <util/logging.hpp>

namespace util
{

LoggingManager logging;

LoggingManager::LoggingManager()
    : _default_logger(&std::cout)
    , _null_logger(0)
    , _logging_level(LOG_LEVEL_ERROR)
{
}

LoggingManager::~LoggingManager()
{
    LoggerMap::iterator i;
    for (i = _loggers.begin(); i != _loggers.end(); ++i) {
        delete i->second;
    }
    _loggers.clear();
}

LoggingStream& LoggingManager::_lookup_logger(const std::string& key)
{
    LoggerMap::iterator i = _loggers.find(key);
    if (i == _loggers.end()) {
        return *_default_logger;
    }
    return *(i->second);
}

// Normal logging
LoggingStream& LoggingManager::log()
{
    return *_default_logger;
}

LoggingStream& LoggingManager::log(const char* key)
{
    return log(std::string(key));
}

LoggingStream& LoggingManager::log(const std::string& key)
{
    return _lookup_logger(key);
}

// Error logging
LoggingStream& LoggingManager::error()
{
    return *_default_logger;
}

LoggingStream& LoggingManager::error(const char* key)
{
    return log(std::string(key));
}

LoggingStream& LoggingManager::error(const std::string& key)
{
    return log(key);
}

// Warning logging
LoggingStream& LoggingManager::warning()
{
    if (_logging_level < LOG_LEVEL_WARNING) {
        return _null_logger;
    }
    return *_default_logger;
}

LoggingStream& LoggingManager::warning(const char* key)
{
    if (_logging_level < LOG_LEVEL_WARNING) {
        return _null_logger;
    }
    return log(std::string(key));
}

LoggingStream& LoggingManager::warning(const std::string& key)
{
    if (_logging_level < LOG_LEVEL_WARNING) {
        return _null_logger;
    }
    return log(key);
}

// Info logging
LoggingStream& LoggingManager::info()
{
    if (_logging_level < LOG_LEVEL_INFO) {
        return _null_logger;
    }
    return *_default_logger;
}

LoggingStream& LoggingManager::info(const char* key)
{
    if (_logging_level < LOG_LEVEL_INFO) {
        return _null_logger;
    }
    return log(std::string(key));
}

LoggingStream& LoggingManager::info(const std::string& key)
{
    if (_logging_level < LOG_LEVEL_INFO) {
        return _null_logger;
    }
    return log(key);
}

// Debug logging
LoggingStream& LoggingManager::debug()
{
    if (_logging_level < LOG_LEVEL_DEBUG) {
        return _null_logger;
    }
    return *_default_logger;
}

LoggingStream& LoggingManager::debug(const char* key)
{
    if (_logging_level < LOG_LEVEL_DEBUG) {
        return _null_logger;
    }
    return log(std::string(key));
}

LoggingStream& LoggingManager::debug(const std::string& key)
{
    if (_logging_level < LOG_LEVEL_DEBUG) {
        return _null_logger;
    }
    return log(key);
}

void LoggingManager::make_logger(const char* key, const char* path)
{
    make_logger(std::string(key), std::string(path));
}

void LoggingManager::make_logger(const std::string& key,
                                 const std::string& path)
{
    std::ofstream* new_logger = new std::ofstream();
    new_logger->open(path.c_str());
    _loggers[key] = new_logger;
}

void LoggingManager::make_default(const char* key)
{
    make_default(std::string(key));
}

void LoggingManager::make_default(const std::string& key)
{
    LoggerMap::iterator i = _loggers.find(key);
    if (i == _loggers.end()) {
        return;
    }
    _default_logger = i->second;
}

void LoggingManager::make_cout_default()
{
    _default_logger = &std::cout;
}

void LoggingManager::make_cerr_default()
{
    _default_logger = &std::cerr;
}

void LoggingManager::make_null_default()
{
    _default_logger = &_null_logger;
}

void LoggingManager::set_logging_level(LoggingLevel lvl)
{
    _logging_level = lvl;
}

}

// vim: set sts=2 sw=2 expandtab:
