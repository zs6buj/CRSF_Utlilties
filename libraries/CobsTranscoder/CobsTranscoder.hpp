//   author 			Geoffrey Hunter <gbmhunter@gmail.com> (www.mbedded.ninja)#include <cstdint>
#include <memory>
#include <queue>
#include <vector>

using ByteArray = std::vector<uint8_t>;

class CobsTranscoder 
{
public:
    enum class decodeStatus {
        SUCCESS,
        ERROR_ZERO_BYTE_NOT_EXPECTED
    };

    /// \details    The encoding process cannot fail.
    static void encode(
            const ByteArray &rawData,
            ByteArray &encodedData);

    /// \brief      decode data using "Consistent Overhead Byte Stuffing" (COBS).
    /// \details    Provided encodedData is expected to be a single, valid COBS encoded packet. If not, method
    ///             will return #decodeStatus::ERROR_ZERO_BYTE_NOT_EXPECTED.
    ///             #decodedData is emptied of any pre-existing data. If the decode fails, decodedData is left empty.
    static void decode(const ByteArray &encodedData, ByteArray &decodedData);

};
