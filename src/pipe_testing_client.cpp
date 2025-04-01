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

#include "testing_client.h"

namespace testing{

    pipe_testing_client::pipe_testing_client(){

        // Do not use specific fds.
        m_specific_fds = false;
    }

    pipe_testing_client::pipe_testing_client(int request_fd, int response_fd){

        // Enable specific fds.
        m_specific_fds = true;

        // Copies pipes file descriptors to local variables.
        m_request_fd = request_fd;
        m_response_fd = response_fd; 
    }

    pipe_testing_client::~pipe_testing_client(){
 
        // Closes both pipes.
        mq_close(m_request_fd);
        mq_close(m_response_fd);

        mq_close(m_request_pipe[0]);
        mq_close(m_request_pipe[1]);

        mq_close(m_response_pipe[0]);
        mq_close(m_response_pipe[1]);
    }

    bool pipe_testing_client::start(){

        if (pipe(m_request_pipe) == -1 || pipe(m_response_pipe) == -1) {
            log_error_message("Error creating new pipes: %s", strerror(errno));
            return false;
        }

        if(m_specific_fds){
            if(dup2(m_request_pipe[0], m_request_fd)  == -1 || dup2(m_response_pipe[1], m_response_fd) == -1){
                log_error_message("Error setting file descriptor of pipes: %s", strerror(errno));
                return false;
            }

            log_info_message("Used specific file descriptors for request pipe: %d and response pipe: %d.", m_request_fd, m_response_fd);
        }

        return true;
    }

    bool pipe_testing_client::check_for_ready(){

        int bytes_available = 0;
    
        // Use ioctl to check available bytes
        if (ioctl(m_response_pipe[0], FIONREAD, &bytes_available) == -1) {
            perror("ioctl");
            return false;
        }

        if(bytes_available < 6){
            return false;
        }

        char buffer[6];

        ssize_t bytes_read = read(m_response_pipe[0], buffer, 6);

        if (bytes_read == -1) {
            log_error_message("An error occurred while waiting for ready message: %s", strerror(errno));
            return false;
        }

        buffer[bytes_read] = '\0'; // Ensure null-termination for valid C-string
        std::string message(buffer);
        if (message == "ready") {
            log_info_message("Received ready message");

            // Indicate ready.
            m_started = true;
            return true;
        }

        return false;
    }

    bool pipe_testing_client::wait_for_ready(){
        char buffer[6];

        while (true) {
            if(check_for_ready()) break;
        }

        return true;
    }

    bool pipe_testing_client::send_request(request* req, response* res) {

        // Request structure:
        // 0     Byte: Command
        // 1 - 4 Byte: Data length (uint32)
        // 5 - ? Byte: Data

        // Response structure:
        // 0     Byte: Status
        // 1 - 4 Byte: Data length (uint32)
        // 5 - ? Byte: Data

        // Check if communication started.
        if(!m_started){
            log_error_message("Communication not started!");
            return false;
        }

        // Creating a buffer for the command and data length.
        char buffer[sizeof(uint32_t)+1];

        // Copy command and data length into one buffer.
        buffer[0] = req->request_command;
        testing_communication::int32_to_bytes(req->data_length, buffer, 1);

        // Send this buffer.
        ssize_t written = write(m_request_pipe[1], buffer, sizeof(uint32_t)+1);
        if (written == -1) {
            log_error_message("Could not send command and data length to the request pipe: %s", strerror(errno));
            return false;
        }

        // Send data.
        written = write(m_request_pipe[1], req->data, req->data_length);
        if (written == -1) {
            log_error_message("Could not send data to the request pipe: %s", strerror(errno));
            return false;
        }

        log_info_message("SENT: %d with length %d.", req->request_command, req->data_length);

        // Waiting for status and data length and write it to the same buffer.
        ssize_t bytes_read = read(m_response_pipe[0], buffer, sizeof(uint32_t)+1); 
        if(bytes_read != sizeof(uint32_t)+1){
            log_error_message("There was an error reading the status and data length from the request pipe: %s", strerror(errno));
            return false;
        }

        
        if(bytes_read < 1){
            log_error_message("Received data is to short for a valid response!");
            return false;
        }

        // Clear old data if existed.
        if(res->data != nullptr){
            free(res->data);
            res->data = nullptr;
        }

        // Extract status and data length.
        res->response_status = (testing::status)buffer[0];

        // Error checking of the response status.
        if(res->response_status == STATUS_ERROR){
            log_error_message("The status of the request indicated an error!");
            return false;
        }else if(res->response_status == STATUS_MALFORMED){
            log_error_message("The the request was malformed!");
            return false;
        }

        res->data_length = testing_communication::bytes_to_int32(buffer, 1);

        // Receive data if data is expected.
        if(res->data_length > 1){
            // Allocating memory for data according to the length.
            res->data = (char*)malloc(res->data_length);

            // Receive data in a while loop to ensure that only partally receiving works.
            // This loop will terminate if there were 5 errors while receiving.
            int error_count = 0;
            size_t received_length = 0;
            while(received_length < res->data_length){
                
                // Read as much data as possible (up to the wanted length).
                bytes_read = read(m_response_pipe[0], res->data+received_length, res->data_length-received_length);

                // Error handling
                if (bytes_read == -1) {
                    log_error_message("There was an error reading the data of the request from the request pipe: %s", strerror(errno));
                    error_count ++;
                } else if (bytes_read == 0) {
                    log_error_message("Request pipe end of data reached, but not full data received!");
                    error_count ++;
                }else{
                    received_length += bytes_read;
                }

                if(error_count >= PIPE_READ_ERROR_MAX){
                    log_error_message("Maximum errors reached while receiving data.");
                    
                    // Resetting 
                    res->response_status = STATUS_ERROR;
                    res->data_length = 0;
                    free(res->data);
                    res->data = nullptr;

                    return false;
                }

            }
        }

        log_info_message("RECEIVED: %d with length %d.", res->response_status, res->data_length);

        return true;
    }

    int pipe_testing_client::get_request_fd(){
        if(m_specific_fds){
            return m_request_fd;
        }else{
            return m_request_pipe[0];
        }
    }

    int pipe_testing_client::get_response_fd(){
        if(m_specific_fds){
            return m_response_fd;
        }else{
            return m_response_pipe[1];
        }
    }

    void pipe_testing_client::clear_pipe(int fd) {
        char buffer[1024];  // Temporary buffer for clearing
        ssize_t bytesRead;

        // Set the pipe to non-blocking mode to avoid hanging on empty reads
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        // Read until the pipe is empty
        while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
            log_info_message("RCleared %d bytes from pipe with fd %d.", bytesRead, fd);

        }

        if (bytesRead == -1 && errno != EAGAIN) {
            log_error_message("Error reading from pipe while clearing: %s", strerror(errno));
        }

        // Restore original pipe flags
        fcntl(fd, F_SETFL, flags);
    }
}
