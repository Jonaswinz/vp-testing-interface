#include "testing_client.h"

namespace testing{

    mq_testing_client::mq_testing_client(std::string request_name, std::string response_name){
        m_request_name = request_name;
        m_response_name = response_name; 
    }

    mq_testing_client::~mq_testing_client(){
        mq_close(m_mqt_requests);
        mq_close(m_mqt_responses);
    }

    void mq_testing_client::init(){

        m_attr.mq_flags = 0;
        m_attr.mq_maxmsg = MAX_MSG;
        m_attr.mq_msgsize = REQUEST_LENGTH;
        m_attr.mq_curmsgs = 0;

        // Clears "lost" data from both message queues.
        clear_mq(m_request_name.c_str());
        clear_mq(m_response_name.c_str());

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
        char* buffer = new char[RESPONSE_LENGTH];
        unsigned int priority;

        while (true) {
            ssize_t bytes_read = mq_receive(m_mqt_responses, buffer, RESPONSE_LENGTH, &priority);

            if (bytes_read == -1) {
                std::cout << "ERROR: An error occurred while waiting for ready message!" << std::endl;
                break;
            }

            buffer[bytes_read] = '\0'; // Ensure null-termination for valid C-string
            std::string message(buffer);
            if (message == "ready") {
                std::cout << "Received ready message!" << std::endl;
                break;
            }
        }

        delete[] buffer;

        m_ready = true;
    }

    bool mq_testing_client::send_request(request* req, response* res) {

        if(!m_ready){
            std::cout << "ERROR: communication not ready!" << std::endl;
            return false;
        }

        char send_buffer[req->data_length+1];
        send_buffer[0] = req->cmd;
        memcpy(send_buffer+1, req->data, req->data_length);

        if (mq_send(m_mqt_requests, send_buffer, req->data_length+1, 0) == -1) {
            std::cout << "ERROR: Error sending message " << strerror(errno) << std::endl;
            return false;
        }

        std::cout << "SENT: " << req->cmd << " with length " << req->data_length << std::endl;

        char receive_buffer[RESPONSE_LENGTH];
        ssize_t bytes_read = mq_receive(m_mqt_responses, receive_buffer, RESPONSE_LENGTH, NULL);
        if (bytes_read == -1) {
            std::cout << "ERROR: Error receiving message " << strerror(errno) << std::endl;
            return false;
        }

        res->response_status = (status)receive_buffer[0];

        std::cout << "RECEIVED: " << res->response_status << std::endl;
        
        //TODO data copy

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
