
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>

class SerialFillerException : public std::runtime_error {
public:
    SerialFillerException(std::string errorMsg) : std::runtime_error(errorMsg) {}
};

class CobsDecodingFailed : public SerialFillerException {
public:
    CobsDecodingFailed(ByteArray packet) :
            SerialFillerException("COBS decoding failed for packet.") {
        packet_ = packet;
    };
    ByteArray packet_;
};

