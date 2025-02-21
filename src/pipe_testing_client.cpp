#include "testing_client.h"

namespace testing{

    pipe_testing_client::pipe_testing_client(int request_fd, int response_fd){

        // Copies pipes file descriptors to local variables.
        m_request_fd = request_fd;
        m_response_fd = response_fd; 
    }

    pipe_testing_client::~pipe_testing_client(){
 
        // Closes both pipes.
        mq_close(m_request_fd);
        mq_close(m_response_fd);
    }

    bool pipe_testing_client::start(){

        if (pipe(m_request_pipe) == -1 || pipe(m_response_pipe) == -1) {
            std::cout << "ERROR: Error creating new pipes!" << std::endl;
            return false;
        }

        if(dup2(m_request_pipe[0], m_request_fd)  == -1 || dup2(m_response_pipe[1], m_response_fd) == -1){
            std::cout << "ERROR: Error setting file descriptor of pipes." << std::endl;
            return false;
        }

        return true;
    }

    void pipe_testing_client::wait_for_ready(){
        char buffer[6];

        while (true) {
            ssize_t bytes_read = read(m_response_pipe[0], buffer, 6);

            if (bytes_read == -1) {
                std::cout << "ERROR: An error occurred while waiting for ready message " << strerror(errno) << std::endl;
                break;
            }

            buffer[bytes_read] = '\0'; // Ensure null-termination for valid C-string
            std::string message(buffer);
            if (message == "ready") {
                std::cout << "Received ready message" << std::endl;

                // Indicate ready.
                m_started = true;

                break;
            }
        }
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
            std::cout << "ERROR: communication not started!" << std::endl;
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
            std::cout << "ERROR: Could not send command and data length to the request pipe!";
            return false;
        }

        // Send data.
        written = write(m_request_pipe[1], req->data, req->data_length);
        if (written == -1) {
            std::cout << "ERROR: Could not send data to the request pipe!";
            return false;
        }

        std::cout << "SENT: " << req->request_command << " with length " << req->data_length << std::endl;

        // Waiting for status and data length and write it to the same buffer.
        ssize_t bytes_read = read(m_response_pipe[0], buffer, sizeof(uint32_t)+1); 
        if(bytes_read != sizeof(uint32_t)+1){
            std::cout << "ERROR: There was an error reading the status and data length from the request pipe.";
            return false;
        }

        if(bytes_read < 1){
            std::cout << "ERROR: Received data is to short for a valid response!";
            return false;
        }

        // Extract status and data length.
        res->response_status = (testing::status)buffer[0];
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
                    std::cout << "ERROR: There was an error reading the data of the request from the request pipe.";
                    error_count ++;
                } else if (bytes_read == 0) {
                    std::cout << "ERROR: Request pipe end of data reached, but not full data received!";
                    error_count ++;
                }else{
                    received_length += bytes_read;
                }

                if(error_count >= PIPE_READ_ERROR_MAX){
                    std::cout << "ERROR: Maximum errors reached while receiving data.";
                    
                    // Resetting 
                    res->response_status = STATUS_ERROR;
                    res->data_length = 0;
                    free(res->data);
                    res->data = nullptr;

                    return false;
                }

            }
        }

        std::cout << "RECEIVED: " << res->response_status << " with length " << res->data_length << std::endl;

        return true;
    }

    void pipe_testing_client::clear_pipe(int fd) {
        char buffer[1024];  // Temporary buffer for clearing
        ssize_t bytesRead;

        // Set the pipe to non-blocking mode to avoid hanging on empty reads
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        // Read until the pipe is empty
        while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
            std::cout << "Cleared " << bytesRead << " bytes from pipe." << std::endl;
        }

        if (bytesRead == -1 && errno != EAGAIN) {
            std::cout << "ERROR: Error reading from pipe while clearing!" << std::endl;
        }

        // Restore original pipe flags
        fcntl(fd, F_SETFL, flags);
    }
}
