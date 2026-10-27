#ifndef Utf8Decoder_h
#define Utf8Decoder_h

#include <string.h>

enum {
  UTF8_END    = -1,
  UTF8_ERROR  = -2,
};

class Utf8Decoder {
public:
  Utf8Decoder(const char* p, int length)
    : _length(length), _input(p) {}
  Utf8Decoder(const char* p)
    : Utf8Decoder(p, strlen(p)) {}

  int   next();

  int   at_byte()       const;
  int   at_character()  const;
  int   symbol_size()   const { return _index - _byte; }

private:
  int   get();
  int   cont();

private:
  int   _index = 0;
  int   _char = 0;
  int   _byte = 0;
  int   _length;
  const char* _input;
};

#endif
