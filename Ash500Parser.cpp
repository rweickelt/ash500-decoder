#include <Ash500Parser.h>
#include <stdlib.h>


bool isParityOk(uint8_t byte, bool parity)
{
    bool calculatedParity = false;
    for (uint8_t i = 0; i < 8; i++)
    {
        if ((byte >> i) & 0x01)
        {
            calculatedParity = !calculatedParity;
        }
    }

    return (calculatedParity == parity);
}

uint32_t Ash500Parser::decodeManchester(void* destination, const void* source, uint32_t bytes)
{
    uint8_t* dst = reinterpret_cast<uint8_t*>(destination);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(source);

    for (uint8_t byte = 0; byte < bytes; byte += 2)
    {
        uint8_t result = 0;
        for (uint8_t bit = 0; bit < 4; bit++)
        {
            uint8_t bitPair = ((src[byte] >> (bit * 2)) & 0x03);
            if (bitPair == 0x01)
            {
                result |= (1 << (bit + 4));
            }
            else if (bitPair == 0x02)
            {
                // ok = 0
            }
            else {
                return byte;
            }
        }
        for (uint8_t bit = 0; bit < 4; bit++)
        {
            uint8_t duo = ((src[byte + 1] >> (bit * 2)) & 0x03);
            if (duo == 0x01)
            {
                result |= (1 << (bit));
            }
            else if (duo == 0x02)
            {
                // ok = 0
            }
            else {
                return byte + 1;
            }
        }
        dst[byte / 2] = result;
    }
    return bytes;
}

void Ash500Parser::parseData(const void* data)
{
    m_error = NoError;
    m_serial = 0;
    m_temperature = 0;
    m_humidity = 0;

    uint8_t* raw = (uint8_t*)data;
    uint8_t packet[9];
    uint8_t parities[2];

    packet[0] = raw[0];
    packet[1] = (raw[1] << 1) | (raw[2] >> 7);
    packet[2] = (raw[2] << 2) | (raw[3] >> 6);
    packet[3] = (raw[3] << 3) | (raw[4] >> 5);
    packet[4] = (raw[4] << 4) | (raw[5] >> 4);
    packet[5] = (raw[5] << 5) | (raw[6] >> 3);
    packet[6] = (raw[6] << 6) | (raw[7] >> 2);
    packet[7] = (raw[7] << 7) | (raw[8] >> 1);
    packet[8] = raw[9];

    parities[0] =
              (raw[1] & 128)
            | (raw[2] & 64)
            | (raw[3] & 32)
            | (raw[4] & 16)
            | (raw[5] & 8)
            | (raw[6] & 4)
            | (raw[7] & 2)
            | (raw[8] & 1);
    parities[1] = (raw[10] & 128);

    for (int byte = 0; byte < 8; byte++)
    {
        bool parity = (((parities[0] >> (7 - byte)) & 0x01) == 0x01);
        if ((!isParityOk(packet[byte], parity)))
        {
            m_error = ParityError;
            return;
        }
    }

    uint8_t decoded[9];
    decoded[0] = packet[0] ^ 0x89;
    for (int i = 1; i < 9; i++)
    {
        decoded[i] = (packet[i-1]+0x24) ^ packet[i];
    }

    m_serial = decoded[4];
    m_humidity = decoded[7];

    union {
        int16_t temperature;
        uint8_t rawBytes[2];
    };

    rawBytes[0] = decoded[6];
    rawBytes[1] = decoded[5] & 0x7F;
    m_temperature = temperature;
}


Ash500Parser::Ash500Parser(const void* rawData)
{
    if (rawData != NULL)
    {
        parseData(rawData);
    }
}
