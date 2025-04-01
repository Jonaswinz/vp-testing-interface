/*
* Copyright (C) 2025 ICE RWTH-Aachen
*
* This file is part of Virtual Platform Testing Interface (VPTI).
*
* Virtual Platform Testing Interface (VPTI) is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 2 of the License, or
* (at your option) any later version.
*
* Virtual Platform Testing Interface (VPTI) is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with AFL++ VP-Mode. If not, see <https://www.gnu.org/licenses/>.
*/

#include "testing_communication.h"

namespace testing{

    bool testing_communication::is_started(){
        return m_started;
    }

    void testing_communication::respond_malformed(response &res){
        res.response_status = STATUS_MALFORMED;
        res.data = nullptr;
        res.data_length = 0;
    }

    void testing_communication::int32_to_bytes(int32_t value, char* buffer, size_t start){
        buffer[start]     = (char)((value >> 24) & 0xFF);
        buffer[start + 1] = (char)((value >> 16) & 0xFF);
        buffer[start + 2] = (char)((value >> 8) & 0xFF);
        buffer[start + 3] = (char)(value & 0xFF);    
    }

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

    bool testing_communication::check_cast_to_uint32(uint64_t value) {
        return value <= std::numeric_limits<uint32_t>::max();
    }
}