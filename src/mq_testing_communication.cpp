#include "testing_communication.h"
#include "testing_receiver.h"

namespace testing{

    mq_testing_communication::mq_testing_communication(testing_receiver* receiver, std::string mq_request_name, std::string mq_response_name):testing_communication(receiver){
        
        // Copies message queue names to local variables.
        m_mq_request_name = new char[mq_request_name.length() + 1];
        std::strcpy(m_mq_request_name, mq_request_name.c_str());
        m_mq_response_name = new char[mq_response_name.length() + 1];
        std::strcpy(m_mq_response_name, mq_response_name.c_str());
    }

    mq_testing_communication::~mq_testing_communication(){

        // Closes both message queues
        mq_close(m_mqt_requests);
        mq_close(m_mqt_responses);
    }

    bool mq_testing_communication::start(){

        // Settings of message queues.
        m_attr.mq_flags = 0;
        m_attr.mq_maxmsg = PIPE_MAX_MSG;
        m_attr.mq_msgsize = MQ_REQUEST_LENGTH;
        m_attr.mq_curmsgs = 0;

        // Clears "lost" data from both message queues.
        clear_mq(m_mq_request_name);
        clear_mq(m_mq_response_name);

        // Openens both message queues.
        if ((m_mqt_requests = mq_open(m_mq_request_name, O_RDONLY | O_CREAT, 0660, &m_attr)) == -1) {
            m_testing_receiver->log_error_message("Error opening request message queue %s.", m_mq_request_name);
            return false;
        }

        if ((m_mqt_responses = mq_open(m_mq_response_name, O_WRONLY | O_CREAT, 0644, &m_attr)) == -1) {
            m_testing_receiver->log_error_message("Error opening response message queue %s.", m_mq_response_name);
            return false;
        }

        // Sends "ready" string to response message queue, to signal that requests can be sent.
        std::string ready_signal = "ready";
        if(mq_send(m_mqt_responses, ready_signal.c_str(), ready_signal.size(), 0) == 0){
            m_testing_receiver->log_info_message("Communication ready, waiting for requests.");
        }else{
            m_testing_receiver->log_error_message("Error sending ready message: %s.", strerror(errno));  
            return false;
        }

        return true;
    }

    bool mq_testing_communication::send_response(testing_communication::response &res){

        size_t sent_data_length = 0;
        size_t total_length = res.data_length + sizeof(res.response_status);
        
        // Send the data or maximum RESPONSE_LENGTH much.
        size_t next_chunk = std::min(total_length, (size_t)MQ_RESPONSE_LENGTH);

        // Temp buffer for the response code and the data, because the message should be send as a whole and not parts.
        char temp[next_chunk];
        memcpy(temp, &res.response_status, sizeof(res.response_status));
        memcpy(temp+sizeof(res.response_status), res.data, next_chunk-sizeof(res.response_status));

        // Send the response code and data.
        if(mq_send(m_mqt_responses, temp, next_chunk, 0) == -1){
            m_testing_receiver->log_error_message("Error sending response data: %s", strerror(errno));
            return false;
        }

        sent_data_length += next_chunk-sizeof(res.response_status);

        // Send more messages if not the full data was sent (longer than RESPONSE_LENGTH).
        while(sent_data_length < res.data_length){

            // Send the rest data or maximum RESPONSE_LENGTH much.
            size_t next_chunk = std::min(res.data_length - sent_data_length, (size_t)MQ_RESPONSE_LENGTH);

            if(mq_send(m_mqt_responses, res.data+sent_data_length, next_chunk, 0) == -1){
                m_testing_receiver->log_error_message("Error sending response data: %s", strerror(errno));
                return false;
            }

            sent_data_length += next_chunk;
            
        }
        
        return true;
    }

    bool mq_testing_communication::receive_request(){
        // Clear buffer
        memset(m_buffer, 0, sizeof(m_buffer));

        // Receive message
        size_t bytes_read = mq_receive(m_mqt_requests, m_buffer, MQ_REQUEST_LENGTH, NULL);
        if (bytes_read < 1) {
            m_testing_receiver->log_error_message("Message was too short for a valid request!");  
            return false;
        }

        // Clear old data if existed.
        if(m_current_req.data != nullptr){
            free(m_current_req.data);
            m_current_req.data = nullptr;
        }

        m_current_req = request();
        m_current_req.data_length = bytes_read-1;
        m_current_req.cmd = (testing_communication::command)m_buffer[0];
        if(bytes_read > 1){
            m_current_req.data = (char*)malloc(bytes_read-1);
            std::memcpy(m_current_req.data, m_buffer+1, bytes_read-1);
        }

        return true;
    }

    void mq_testing_communication::clear_mq(const char* queue_name) {
        mqd_t mqd = mq_open(queue_name, O_RDONLY | O_NONBLOCK);
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
                    m_testing_receiver->log_info_message("Message queue %s is now empty.", queue_name);
                    break;
                }
            }
        }

        delete[] buffer;
        mq_close(mqd);
    }
};