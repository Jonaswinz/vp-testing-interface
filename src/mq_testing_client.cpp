#include "testing_client.h"

namespace testing{

    mq_testing_client::mq_testing_client(std::string request_name, std::string response_name){

        // Copies message queue names to local variables.
        m_request_name = request_name;
        m_response_name = response_name; 
    }

    mq_testing_client::~mq_testing_client(){

        // Closes both message queues
        mq_close(m_mqt_requests);
        mq_close(m_mqt_responses);
    }

    bool mq_testing_client::start(){

        // Settings of message queues.
        m_attr.mq_flags = 0;
        m_attr.mq_maxmsg = MQ_MAX_MSG;
        m_attr.mq_msgsize = MQ_MAX_LENGTH;
        m_attr.mq_curmsgs = 0;

        // Clears "lost" data from both message queues.
        clear_mq(m_request_name.c_str());
        clear_mq(m_response_name.c_str());

        // Openens both message queues and create them if not exist.
        if ((m_mqt_requests = mq_open(m_request_name.c_str(), O_WRONLY | O_CREAT, 0644, &m_attr)) == -1) {
            log_error_message("Error opening request message queue: %s", strerror(errno));
            return false;
        }

        if ((m_mqt_responses = mq_open(m_response_name.c_str(), O_RDONLY | O_CREAT, 0660, &m_attr)) == -1) {
            log_error_message("Error opening response message queue: %s", strerror(errno));
            return false;
        }

        return true;
    }

    bool mq_testing_client::check_for_ready(){

        struct mq_attr attr;
        if (mq_getattr(m_mqt_responses, &attr) == -1) {
            log_error_message("An error occurred while checking for ready message: %s", strerror(errno));
            return false;
        }

        if(attr.mq_curmsgs <= 0){
            return false;
        }

        char buffer[MQ_MAX_LENGTH];

        ssize_t bytes_read = mq_receive(m_mqt_responses, buffer, MQ_MAX_LENGTH, 0);

        if (bytes_read == -1) {
            log_error_message("An error occurred while checking for ready message: %s", strerror(errno));
            return false;
        }

        buffer[bytes_read] = '\0'; // Ensure null-termination for valid string.
        std::string message(buffer);
        if (message == "ready") {
            log_info_message("Received ready message!");

            // Indicate ready.
            m_started = true;
            return true;
        }

        return false;
    }

    bool mq_testing_client::wait_for_ready(){
        char buffer[MQ_MAX_LENGTH];

        // Waiting for "ready" string of response message queue.
        while (true) {
            if(check_for_ready()) break;
        }

        return true;
    }

    bool mq_testing_client::send_request(request* req, response* res) {

        // Request structure:
        // 0                     Byte: Command
        // 1 - (MQ_MAX_LENGTH-1) Byte: Data

        // Response structure:
        // 0                     Byte: Status
        // 1 - (MQ_MAX_LENGTH-2) Byte: Data
        // MQ_MAX_LENGTH         Byte: More data

        // Check if communication started.
        if(!m_started){
            log_error_message("Communication not started!");
            return false;
        }

        // Check if request (command and data) fits in one message (currently only supported that the request is only one message).
        if(req->data_length+1 > MQ_MAX_LENGTH){
            log_error_message("When using MQ the request cannot be larger than the defined MQ_MAX_LENGTH length of %d! Please increase MQ_MAX_LENGTH or use pipe communication instead.", MQ_MAX_LENGTH);
        }

        // Creating a buffer for sending and receiving data.
        char buffer[MQ_MAX_LENGTH];

        // Copy command and data into one buffer.
        buffer[0] = req->request_command;
        memcpy(buffer+1, req->data, req->data_length);

        // Send this buffer.
        if (mq_send(m_mqt_requests, buffer, req->data_length+1, 0) == -1) {
            log_error_message("Error sending message: %s", strerror(errno));
            return false;
        }

        log_info_message("SENT: %d with length %d.", req->request_command, req->data_length);

        // Waiting for a message and writing it to the same buffer.
        ssize_t bytes_read = mq_receive(m_mqt_responses, buffer, MQ_MAX_LENGTH, NULL);
        if (bytes_read == -1) {
            log_error_message("Error receiving message: %s", strerror(errno));
            return false;
        }

        if (bytes_read < 1) {
            log_error_message("Received message was too short for a valid request!");
            return false;
        }

        // Clear old data if existed.
        if(res->data != nullptr){
            free(res->data);
            res->data = nullptr;
        }

        // Reading the response status from the buffer
        res->response_status = (status)buffer[0];

        // Error checking of the response status.
        if(res->response_status == STATUS_ERROR){
            log_error_message("The status of the request indicated an error!");
            return false;
        }else if(res->response_status == STATUS_MALFORMED){
            log_error_message("The the request was malformed!");
            return false;
        }

        res->data_length = bytes_read-1;
        if(bytes_read > 1){
            res->data = (char*)malloc(bytes_read-1);
            std::memcpy(res->data, buffer+1, bytes_read-1);
        }

        log_info_message("RECEIVED: %d with length %d.", res->response_status, res->data_length);

        return true;
    }

    void mq_testing_client::clear_mq(const char* queue_name) {
        mqd_t mqd = mq_open(queue_name, O_RDONLY | O_NONBLOCK | O_CREAT);
        if (mqd == -1) {
            return;
        }

        char buffer[MQ_MAX_LENGTH];

        while (true) {
            ssize_t bytes_read = mq_receive(mqd, buffer, MQ_MAX_LENGTH, NULL);
            if (bytes_read == -1) {
                if (errno == EAGAIN) {
                    log_info_message("Message queue %s is now empty!", queue_name);
                    break;
                }
            }
        }

        mq_close(mqd);
    }
}
