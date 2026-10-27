#ifndef JsonWriter_h
#define JsonWriter_h

#include <stdarg.h>
#include <Arduino.h>

class JsonWriter {
public:

    class AssignHelper {
    public:
        AssignHelper(JsonWriter& w) : _writer(w) {}

        template <typename T>
        void operator = (const T& val) {
            _writer.value(val);
        }

    private:
        JsonWriter& _writer;
    };

    JsonWriter();
    virtual ~JsonWriter() = default;

    void setAsciiOnly(bool value = true) {
        _asciiOnly = value;
    }

    JsonWriter& beginArray();
    JsonWriter& endArray();
    JsonWriter& beginObject();
    JsonWriter& endObject();
    JsonWriter& name(const char *name);
    JsonWriter& name(const char *name, size_t size);
    JsonWriter& name(const String &name);
    JsonWriter& value(bool val);
    JsonWriter& value(int val);
    JsonWriter& value(unsigned val);
    JsonWriter& value(long val);
    JsonWriter& value(unsigned long val);
    JsonWriter& value(double val, int precision);
    JsonWriter& value(double val);
    JsonWriter& value(const char *val);
    JsonWriter& value(const char *val, size_t size);
    JsonWriter& value(const String &val);
    JsonWriter& nullValue();

    AssignHelper operator[](const char* name) {
        this->name(name, strlen(name));
        return AssignHelper(*this);
    }

    AssignHelper operator[](const String &name) {
        this->name(name.c_str(), name.length());
        return AssignHelper(*this);
    }

protected:
    virtual void write(const char *data, size_t size) = 0;
    virtual void printf(const char *fmt, ...);

private:
    enum State {
        BEGIN, // Beginning of a document or a compound value
        NEXT,  // Expecting next element of a compound value
        VALUE  // Expecting value of an object's property
    };

    State _state;
    bool  _asciiOnly = false;

    void writeSeparator();
    void writeEscaped(const char *data, size_t size);
    void write(char c);
};

class JsonStreamWriter
  : public JsonWriter
{
public:
    explicit JsonStreamWriter(Print &stream);

    Print* stream() const;

protected:
    virtual void write(const char *data, size_t size) override;

private:
    Print &_stream;
};

class JsonBufferWriter
  : public JsonWriter
{
public:
    JsonBufferWriter(char *buf, size_t size);

    const char* c_str();
    char* buffer() const;
    size_t bufferSize() const;

    size_t dataSize() const; // Returned value can be greater than buffer size

protected:
    virtual void write(const char *data, size_t size) override;
    virtual void printf(const char *fmt, ...) override;

private:
    char*  _buf;
    size_t _buf_size, _n;
};


// JsonWriter
inline JsonWriter::JsonWriter()
  : _state(BEGIN)
{}

inline JsonWriter& JsonWriter::name(const char *name) {
    return this->name(name, strlen(name));
}

inline JsonWriter& JsonWriter::name(const String &name) {
    return this->name(name.c_str(), name.length());
}

inline JsonWriter& JsonWriter::value(const char *val) {
    return value(val, strlen(val));
}

inline JsonWriter& JsonWriter::value(const String &val) {
    return value(val.c_str(), val.length());
}

inline void JsonWriter::write(char c) {
    write(&c, 1);
}

// JsonStreamWriter
inline JsonStreamWriter::JsonStreamWriter(Print &stream)
  : _stream(stream)
{}

inline Print* JsonStreamWriter::stream() const {
    return &_stream;
}

inline void JsonStreamWriter::write(const char *data, size_t size) {
    _stream.write((const uint8_t*)data, size);
}

// JsonBufferWriter
inline JsonBufferWriter::JsonBufferWriter(char *buf, size_t size)
  : _buf(buf)
  , _buf_size(size)
  , _n(0)
{}

inline const char* JsonBufferWriter::c_str() {
    if (_n < _buf_size) {
        _buf[_n] = '\0';
    }
    return _buf;
}

inline char* JsonBufferWriter::buffer() const {
    return _buf;
}

inline size_t JsonBufferWriter::bufferSize() const {
    return _buf_size;
}

inline size_t JsonBufferWriter::dataSize() const {
    return _n;
}

#endif
