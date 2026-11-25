#pragma once

// Minimaler JSON-Schreiber ohne Abhaengigkeiten - reicht fuer Benchmark-Ausgaben.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace sb {

class JsonWriter {
 public:
  void beginObject(const char* key = nullptr) { open(key, '{'); }
  void endObject() { close('}'); }
  void beginArray(const char* key = nullptr) { open(key, '['); }
  void endArray() { close(']'); }

  void value(const char* key, const std::string& v) {
    prefix(key);
    out_ << '"' << escape(v) << '"';
  }
  void value(const char* key, const char* v) { value(key, std::string(v)); }
  void value(const char* key, bool v) {
    prefix(key);
    out_ << (v ? "true" : "false");
  }
  void value(const char* key, double v) {
    prefix(key);
    if (!std::isfinite(v)) {
      out_ << "null";
      return;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6g", v);
    out_ << buf;
  }
  void value(const char* key, std::uint64_t v) {
    prefix(key);
    out_ << v;
  }
  void value(const char* key, std::int64_t v) {
    prefix(key);
    out_ << v;
  }
  void value(const char* key, int v) { value(key, static_cast<std::int64_t>(v)); }

  std::string str() const { return out_.str() + "\n"; }

 private:
  void open(const char* key, char bracket) {
    prefix(key);
    out_ << bracket;
    first_.push_back(true);
  }
  void close(char bracket) {
    const bool empty = first_.back();
    first_.pop_back();
    if (!empty) newline();
    out_ << bracket;
  }
  void prefix(const char* key) {
    if (!first_.empty()) {
      if (!first_.back()) out_ << ',';
      first_.back() = false;
      newline();
    }
    if (key) out_ << '"' << escape(key) << "\": ";
  }
  void newline() {
    out_ << '\n';
    for (std::size_t i = 0; i < first_.size(); ++i) out_ << "  ";
  }
  static std::string escape(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (unsigned char c : s) {
      switch (c) {
        case '"': r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\n': r += "\\n"; break;
        case '\r': r += "\\r"; break;
        case '\t': r += "\\t"; break;
        default:
          if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            r += buf;
          } else {
            r += static_cast<char>(c);
          }
      }
    }
    return r;
  }

  std::ostringstream out_;
  std::vector<bool> first_;
};

}  // namespace sb
