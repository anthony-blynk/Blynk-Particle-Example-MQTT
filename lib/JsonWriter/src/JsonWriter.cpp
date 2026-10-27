#include "JsonWriter.h"

#ifdef JSON_WRITER_USE_UTF8_DECODER
#include "Utf8Decoder.h"
#endif

JsonWriter& JsonWriter::beginArray() {
    writeSeparator();
    write('[');
    _state = BEGIN;
    return *this;
}

JsonWriter& JsonWriter::endArray() {
    write(']');
    _state = NEXT;
    return *this;
}

JsonWriter& JsonWriter::beginObject() {
    writeSeparator();
    write('{');
    _state = BEGIN;
    return *this;
}

JsonWriter& JsonWriter::endObject() {
    write('}');
    _state = NEXT;
    return *this;
}

JsonWriter& JsonWriter::name(const char *name, size_t size) {
    writeSeparator();
    writeEscaped(name, size);
    _state = VALUE;
    return *this;
}

JsonWriter& JsonWriter::value(bool val) {
    writeSeparator();
    if (val) {
        write("true", 4);
    } else {
        write("false", 5);
    }
    _state = NEXT;
    return *this;
}

JsonWriter& JsonWriter::value(int val) {
    writeSeparator();
    printf("%d", val);
    _state = NEXT;
    return *this;
}

JsonWriter& JsonWriter::value(unsigned val) {
    writeSeparator();
    printf("%u", val);
    _state = NEXT;
    return *this;
}

JsonWriter& JsonWriter::value(long val) {
    writeSeparator();
    printf("%ld", val);
    _state = NEXT;
    return *this;
}

JsonWriter& JsonWriter::value(unsigned long val) {
    writeSeparator();
    printf("%lu", val);
    _state = NEXT;
    return *this;
}

JsonWriter& JsonWriter::value(double val, int precision) {
    writeSeparator();
    printf("%.*lf", precision, val);
    _state = NEXT;
    return *this;
}

JsonWriter& JsonWriter::value(double val) {
    writeSeparator();
    printf("%g", val);
    _state = NEXT;
    return *this;
}

JsonWriter& JsonWriter::value(const char *val, size_t size) {
    writeSeparator();
    writeEscaped(val, size);
    _state = NEXT;
    return *this;
}

JsonWriter& JsonWriter::nullValue() {
    writeSeparator();
    write("null", 4);
    _state = NEXT;
    return *this;
}

void JsonWriter::printf(const char *fmt, ...) {
    char buf[16];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if ((size_t)n >= sizeof(buf)) {
        char buf[n + 1]; // Use larger buffer
        va_start(args, fmt);
        n = vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        if (n > 0) {
            write(buf, n);
        }
    } else if (n > 0) {
        write(buf, n);
    }
}

void JsonWriter::writeSeparator() {
    switch (_state) {
    case NEXT:
        write(',');
        break;
    case VALUE:
        write(':');
        break;
    default:
        break;
    }
}

#ifdef JSON_WRITER_USE_UTF8_DECODER

void JsonWriter::writeEscaped(const char *str, size_t size) {
    Utf8Decoder decoder(str, size);
    const char* const HEXFMT = "\\u%04X";
    write('"');
    for (;;) {
        const int c = decoder.next();
        if (c < 0) {
            // Finish
            //if (c == UTF8_END) => no errors detected
            break;
        } else if (c == '"' || c == '\\' || c <= 0x1F) {
            // Basic escaping
            write('\\');
            switch (c) {
            case '"':                                // Double quote
            case '\\':  write((char)c);      break;  // Backslash
            case 0x08:  write('b');          break;  // Backspace
            case 0x09:  write('t');          break;  // Horizontal tab
            case 0x0A:  write('n');          break;  // Line feed
            case 0x0C:  write('f');          break;  // Form feed
            case 0x0D:  write('r');          break;  // Carriage return
            default:    printf(HEXFMT+1, c); break;
            }
        } else if (c < 0x7F) {
            // ASCII
            write((char)c);
        } else if (c <= 0x9F || c == 0x2028 || c == 0x2029) {
            // Control
            printf(HEXFMT, c);
        } else if (_asciiOnly) {
            if (c < 0x10000) {
                // Basic Multilingual Plane
                printf(HEXFMT, c);
            } else {
                // Beyond the Basic Multilingual Plane
                const int cp = c - 0x10000;
                const uint16_t w0 = (uint16_t)(0xD800 | (cp >> 10));
                const uint16_t w1 = (uint16_t)(0xDC00 | (cp & 0x3FF));
                printf(HEXFMT, w0);
                printf(HEXFMT, w1);
            }
        } else {
            // Pass-through UTF8 bytes
            const int off = decoder.at_byte();
            const int len = decoder.symbol_size();
            write(str + off, len);
        }
    }
    write('"');
}

#else

void JsonWriter::writeEscaped(const char *str, size_t size) {
    write('"');
    const char* const end = str + size;
    const char *s = str;
    while (s != end) {
        const char c = *s;
        if (c == '"' || c == '\\' || c <= 0x1F) {
            write(str, s - str); // Write preceeding characters
            write('\\');
            switch (c) {
            case '"':                                // Double quote
            case '\\':  write(c);            break;  // Backslash
            case 0x08:  write('b');          break;  // Backspace
            case 0x09:  write('t');          break;  // Horizontal tab
            case 0x0A:  write('n');          break;  // Line feed
            case 0x0C:  write('f');          break;  // Form feed
            case 0x0D:  write('r');          break;  // Carriage return
            default:    printf("u%04X", (unsigned)c); break;
            }
            str = s + 1;
        }
        ++s;
    }
    if (s != str) {
        write(str, s - str); // Write remaining characters
    }
    write('"');
}

#endif

// JsonBufferWriter
void JsonBufferWriter::write(const char *data, size_t size) {
    if (_n < _buf_size) {
        memcpy(_buf + _n, data, min(size, _buf_size - _n));
    }
    _n += size;
}

void JsonBufferWriter::printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const int n = vsnprintf(_buf + _n, (_n < _buf_size) ? _buf_size - _n : 0, fmt, args);
    va_end(args);
    _n += n;
}
