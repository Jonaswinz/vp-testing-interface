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

    void mq_testing_client::start(){

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
            std::cout << "ERROR: Error opening request message queue." << std::endl;
            exit(1);
        }

        if ((m_mqt_responses = mq_open(m_response_name.c_str(), O_RDONLY | O_CREAT, 0660, &m_attr)) == -1) {
            std::cout << "ERROR: Error opening response message queue." << std::endl;
            exit(1);
        }
    }

    void mq_testing_client::wait_for_ready(){
        char* buffer = new char[MQ_MAX_LENGTH];
        unsigned int priority;

        // Waiting for "ready" string of response message queue.
        while (true) {
            ssize_t bytes_read = mq_receive(m_mqt_responses, buffer, MQ_MAX_LENGTH, &priority);

            if (bytes_read == -1) {
                std::cout << "ERROR: An error occurred while waiting for ready message!" << std::endl;
                break;
            }

            buffer[bytes_read] = '\0'; // Ensure null-termination for valid string.
            std::string message(buffer);
            if (message == "ready") {
                std::cout << "Received ready message!" << std::endl;
                break;
            }
        }

        delete[] buffer;

        // Indicate ready.
        m_started = true;
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
            std::cout << "ERROR: communication not started!" << std::endl;
            return false;
        }

        // Check if request (command and data) fits in one message (currently only supported that the request is only one message).
        if(req->data_length+1 > MQ_MAX_LENGTH){
            std::cout << "ERROR: When using MQ the request cannot be larget than the defined max length!" << std::endl;
        }

        // Creating a buffer for sending and receiving data.
        char buffer[MQ_MAX_LENGTH];

        // Copy command and data into one buffer.
        buffer[0] = req->cmd;
        memcpy(buffer+1, req->data, req->data_length);

        // Send this buffer.
        if (mq_send(m_mqt_requests, buffer, req->data_length+1, 0) == -1) {
            std::cout << "ERROR: Error sending message " << strerror(errno) << std::endl;
            return false;
        }

        std::cout << "SENT: " << req->cmd << " with length " << req->data_length << std::endl;

        // Waiting for a message and writing it to the same buffer.
        ssize_t bytes_read = mq_receive(m_mqt_responses, buffer, MQ_MAX_LENGTH, NULL);
        if (bytes_read == -1) {
            std::cout << "ERROR: Error receiving message " << strerror(errno) << std::endl;
            return false;
        }

        if (bytes_read < 1) {
            std::cout << "ERROR: Message was too short for a valid request!" << std::endl;
            return false;
        }

        // Clear old data if existed.
        if(res->data != nullptr){
            free(res->data);
            res->data = nullptr;
        }

        // Reading the response status from the buffer
        res->response_status = (status)buffer[0];

        res->data_length = bytes_read-1;
        if(bytes_read > 1){
            res->data = (char*)malloc(bytes_read-1);
            std::memcpy(res->data, buffer+1, bytes_read-1);
        }

        std::cout << "RECEIVED: " << res->response_status << " with length " << res->data_length << std::endl;

        return true;
    }

    void mq_testing_client::clear_mq(const char* queue_name) {
        mqd_t mqd = mq_open(queue_name, O_RDONLY | O_NONBLOCK | O_CREAT);
        if (mqd == -1) {
            return;
        }

        struct mq_attr attr;
        mq_getattr(mqd, &attr);
        char* buffer = new char[attr.mq_msgsize];

        while (true) {
            ssize_t bytes_read = mq_receive(mqd, buffer, attr.mq_msgsize, NULL);
            if (bytes_read == -1) {
                if (errno == EAGAIN) {
                    std::cout << "Message queue " << queue_name << " is now empty." << std::endl;
                    break;
                }
            }
        }

        delete[] buffer;
        mq_close(mqd);
    }
}
