#ifndef LOGGER_H
#define LOGGER_H

#include <cstddef>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace std;

// Severity levels used to tag every log line.
// They are ordered so that a higher value means a more serious message.
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

// Logger is a thread-safe singleton. Every component of the banking system
// writes through this one object so output from many threads never gets
// interleaved in the middle of a line.
//
// Design notes:
//   * A single mutex guards the console, the optional file, and the history.
//   * Messages below the minimum level are discarded before any formatting.
//   * The last kHistoryLimit lines are kept in memory for inspection or tests.
class Logger {
public:
    // Maximum number of formatted lines kept in memory.
    static const size_t kHistoryLimit = 200;

    // Returns the single Logger instance, created on first use.
    static Logger& instance();

    // Copying is forbidden so the singleton stays unique.
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Writes one message at the given level. Safe to call from any thread.
    void log(LogLevel level, const string& component, const string& message);

    // Convenience wrappers so call sites read naturally.
    void debug(const string& component, const string& message);
    void info(const string& component, const string& message);
    void warning(const string& component, const string& message);
    void error(const string& component, const string& message);

    // Writes to a file in addition to the console. Appends to existing content.
    // Returns false if the file could not be opened; the console still works.
    bool attachFile(const string& path);

    // Stops writing to the file (the console output continues).
    void detachFile();

    // True when a log file is currently attached.
    bool isFileAttached() const;

    // Forces buffered file content onto disk.
    void flush();

    // Messages below this level are silently discarded.
    void setMinimumLevel(LogLevel level);
    LogLevel minimumLevel() const;

    // Number of ERROR messages written since the program started.
    size_t errorCount() const;

    // Returns up to count of the most recent formatted lines, oldest first.
    vector<string> recent(size_t count) const;

    // Empties the in-memory history (the file is not touched).
    void clearHistory();

    // Textual name of a level, for example "INFO".
    static string levelName(LogLevel level);

private:
    Logger();
    ~Logger();

    // Builds a "YYYY-MM-DD HH:MM:SS" timestamp for the current moment.
    string timestamp() const;

    // Adds a line to the in-memory history, trimming the oldest entry if full.
    void remember(const string& line);

    LogLevel minimum_;
    size_t errors_;
    unique_ptr<ofstream> file_;
    deque<string> history_;
    mutable mutex mtx_;
};

// End of the Logger interface. Include this header wherever logging is needed.
#endif // LOGGER_H
