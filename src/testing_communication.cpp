#include "testing_communication.h"

namespace testing{

    void testing_communication::respond_malformed(response &res){
        res.response_status = STATUS_MALFORMED;
        res.data = nullptr;
        res.data_length = 0;
    }

    // Copying a 32bit integer to a buffer at a start index MSB.
    void testing_communication::int32_to_bytes(int32_t value, char* buffer, size_t start){
        buffer[start]     = (char)((value >> 24) & 0xFF);
        buffer[start + 1] = (char)((value >> 16) & 0xFF);
        buffer[start + 2] = (char)((value >> 8) & 0xFF);
        buffer[start + 3] = (char)(value & 0xFF);    
    }

    // Copying a 32bit integer from a buffer at a start index MSB.
    int32_t testing_communication::bytes_to_int32(const char* buffer, size_t start){
        return  ((int32_t)(uint8_t)buffer[start] << 24) |
                ((int32_t)(uint8_t)buffer[start + 1] << 16) |
                ((int32_t)(uint8_t)buffer[start + 2] << 8)  |
                ((int32_t)(uint8_t)buffer[start + 3]);
    }

    void testing_communication::int64_to_bytes(int64_t value, char* buffer, size_t start) {
        buffer[start]     = (char)((value >> 56) & 0xFF); // MSB
        buffer[start + 1] = (char)((value >> 48) & 0xFF);
        buffer[start + 2] = (char)((value >> 40) & 0xFF);
        buffer[start + 3] = (char)((value >> 32) & 0xFF);
        buffer[start + 4] = (char)((value >> 24) & 0xFF);
        buffer[start + 5] = (char)((value >> 16) & 0xFF);
        buffer[start + 6] = (char)((value >> 8)  & 0xFF);
        buffer[start + 7] = (char)(value & 0xFF);         // LSB
    }

    // Convert 8-byte big-endian buffer to int64_t from a specific index
    int64_t testing_communication::bytes_to_int64(const char* buffer, size_t start) {
        return ((int64_t)(uint8_t)buffer[start] << 56) |
            ((int64_t)(uint8_t)buffer[start + 1] << 48) |
            ((int64_t)(uint8_t)buffer[start + 2] << 40) |
            ((int64_t)(uint8_t)buffer[start + 3] << 32) |
            ((int64_t)(uint8_t)buffer[start + 4] << 24) |
            ((int64_t)(uint8_t)buffer[start + 5] << 16) |
            ((int64_t)(uint8_t)buffer[start + 6] << 8)  |
            ((int64_t)(uint8_t)buffer[start + 7]);
    }

}