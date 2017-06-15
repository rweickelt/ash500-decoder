#ifndef ASH500PARSER_H_
#define ASH500PARSER_H_

#include <stdint.h>

class Ash500Parser
{
public:
    enum Error {
        NoError,
        ParityError
    };

    Ash500Parser(const void* data = 0);
    Error error() const;
    bool hasErrors() const;
    uint8_t humidity() const;
    void parseData(const void* data);
    uint8_t serial() const;
    int16_t temperature() const;

    static uint32_t decodeManchester(void* destination, const void* source, uint32_t bytes);

private:
    Error m_error;
    uint8_t m_humidity;
    int16_t m_temperature;
    uint8_t m_serial;
};

inline Ash500Parser::Error Ash500Parser::error() const { return m_error; }
inline bool Ash500Parser::hasErrors() const { return m_error != Ash500Parser::NoError; }
inline uint8_t Ash500Parser::humidity() const { return m_humidity; }
inline uint8_t Ash500Parser::serial() const { return m_serial; }
inline int16_t Ash500Parser::temperature() const { return m_temperature; }


#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')


#endif /* ASH500PARSER_H_ */
