#include "Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

using namespace std;

Logger& Logger::instance() {
    // A function-local static is constructed exactly once. Since C++11 the
    // initialisation itself is guaranteed to be thread-safe.
    static Logger theLogger;
    return theLogger;
}

Logger::Logger()
    : minimum_(LogLevel::INFO), errors_(0), file_(nullptr) {
}

Logger::~Logger() {
    // Make sure any buffered file content reaches the disk at shutdown.
    if (file_ && file_->is_open()) {
        file_->flush();
        file_->close();
    }
}

string Logger::levelName(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
    }
    return "UNKNOWN";
}

string Logger::timestamp() const {
    auto now = chrono::system_clock::now();
    time_t raw = chrono::system_clock::to_time_t(now);

    // localtime is not thread-safe on every platform, so use the
    // reentrant variant that writes into our own struct.
    tm localTm{};
#ifdef _WIN32
    localtime_s(&localTm, &raw);
#else
    localtime_r(&raw, &localTm);
#endif

    ostringstream out;
    out << put_time(&localTm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

void Logger::remember(const string& line) {
    // Called with mtx_ already held by log().
    history_.push_back(line);
    while (history_.size() > kHistoryLimit) {
        history_.pop_front();
    }
}

void Logger::log(LogLevel level, const string& component, const string& message) {
    lock_guard<mutex> guard(mtx_);

    // Discard messages that are less serious than the configured minimum.
    if (static_cast<int>(level) < static_cast<int>(minimum_)) {
        return;
    }

    if (level == LogLevel::ERROR) {
        ++errors_;
    }

    string line = "[" + timestamp() + "] [" + levelName(level) + "] ["
                + component + "] " + message;

    // Errors go to stderr so they can be separated from normal output.
    if (level == LogLevel::ERROR) {
        cerr << line << endl;
    } else {
        cout << line << endl;
    }

    if (file_ && file_->is_open()) {
        *file_ << line << '\n';
        file_->flush();
    }

    remember(line);
}

void Logger::debug(const string& component, const string& message) {
    log(LogLevel::DEBUG, component, message);
}

void Logger::info(const string& component, const string& message) {
    log(LogLevel::INFO, component, message);
}

void Logger::warning(const string& component, const string& message) {
    log(LogLevel::WARNING, component, message);
}

void Logger::error(const string& component, const string& message) {
    log(LogLevel::ERROR, component, message);
}

bool Logger::attachFile(const string& path) {
    lock_guard<mutex> guard(mtx_);

    // Append mode keeps history from previous runs.
    auto stream = make_unique<ofstream>(path, ios::app);
    if (!stream->is_open()) {
        return false;
    }

    // Replacing the pointer closes any previously attached file automatically.
    file_ = move(stream);
    return true;
}

void Logger::detachFile() {
    lock_guard<mutex> guard(mtx_);
    if (file_) {
        file_->flush();
        file_->close();
        file_.reset();
    }
}

bool Logger::isFileAttached() const {
    lock_guard<mutex> guard(mtx_);
    return file_ != nullptr && file_->is_open();
}

void Logger::flush() {
    lock_guard<mutex> guard(mtx_);
    if (file_ && file_->is_open()) {
        file_->flush();
    }
    cout.flush();
    cerr.flush();
}

void Logger::setMinimumLevel(LogLevel level) {
    lock_guard<mutex> guard(mtx_);
    minimum_ = level;
}

LogLevel Logger::minimumLevel() const {
    lock_guard<mutex> guard(mtx_);
    return minimum_;
}

size_t Logger::errorCount() const {
    lock_guard<mutex> guard(mtx_);
    return errors_;
}

vector<string> Logger::recent(size_t count) const {
    lock_guard<mutex> guard(mtx_);

    size_t start = 0;
    if (count < history_.size()) {
        start = history_.size() - count;
    }

    vector<string> result;
    result.reserve(history_.size() - start);
    for (size_t i = start; i < history_.size(); ++i) {
        result.push_back(history_[i]);
    }
    return result;
}

void Logger::clearHistory() {
    lock_guard<mutex> guard(mtx_);
    history_.clear();
}
