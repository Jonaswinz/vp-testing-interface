#include "testing_communication.h"
#include "testing_receiver.h"

namespace testing{

    mq_testing_communication::mq_testing_communication(testing_receiver* receiver, std::string mq_request_name, std::string mq_response_name):testing_communication(receiver){
        
        // Copies message queue names to local variables.
        m_mq_request_name = mq_request_name;
        m_mq_response_name = mq_response_name;
    }

    mq_testing_communication::~mq_testing_communication(){

        // Closes both message queues
        mq_close(m_mqt_requests);
        mq_close(m_mqt_responses);
    }

    bool mq_testing_communication::start(){

        // Settings of message queues.
        m_attr.mq_flags = 0;
        m_attr.mq_maxmsg = MQ_MAX_MSG;
        m_attr.mq_msgsize = MQ_MAX_LENGTH;
        m_attr.mq_curmsgs = 0;

        // Openens both message queues.
        if ((m_mqt_requests = mq_open(m_mq_request_name.c_str(), O_RDONLY, 0660, &m_attr)) == -1) {
            m_testing_receiver->log_error_message("Error opening request message queue %s.", m_mq_request_name);
            return false;
        }

        if ((m_mqt_responses = mq_open(m_mq_response_name.c_str(), O_WRONLY, 0644, &m_attr)) == -1) {
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

        m_started = true;

        return true;
    }

    bool mq_testing_communication::send_response(response &res){

        // Response structure:
        // 0                     Byte: Status
        // 1 - (MQ_MAX_LENGTH-1) Byte: Data

        // Check if communication started.
        if(!m_started){
            m_testing_receiver->log_error_message("Communication not started!");
            return false;
        }

        // Check if response (command and data) fits in one message (currently only supported that the response is only one message).
        if(res.data_length+1 > MQ_MAX_LENGTH){
            m_testing_receiver->log_error_message("When using MQ the request cannot be larger than the defined MQ_MAX_LENGTH length of %d! Please increase MQ_MAX_LENGTH or use pipe communication instead.", MQ_MAX_LENGTH);
        }

        // Creating a buffer for sending data.
        char buffer[res.data_length+1];

        // Write response status and the data to the buffer.
        buffer[0] = res.response_status;
        memcpy(buffer+1, res.data, res.data_length);

        // Send the response code and data.
        if(mq_send(m_mqt_responses, buffer, res.data_length+1, 0) == -1){
            m_testing_receiver->log_error_message("Error sending response data: %s", strerror(errno));
            return false;
        }

        return true;
    }

    bool mq_testing_communication::receive_request(){

        // Request structure:
        // 0                     Byte: Command
        // 1 - (MQ_MAX_LENGTH-1) Byte: Data

        // Check if communication started.
        if(!m_started){
            m_testing_receiver->log_error_message("Communication not started!");
            return false;
        }

        // Creating a buffer for receiving data.
        char buffer[MQ_MAX_LENGTH];

        // Receive message
        size_t bytes_read = mq_receive(m_mqt_requests, buffer, MQ_MAX_LENGTH, NULL);
        if (bytes_read == -1) {
            m_testing_receiver->log_error_message("Error receiving message %s.", strerror(errno));  
            return false;
        }

        if (bytes_read < 1) {
            m_testing_receiver->log_error_message("Message was too short for a valid request!");  
            return false;
        }

        // Clear old data if existed.
        if(m_current_req.data != nullptr){
            free(m_current_req.data);
            m_current_req.data = nullptr;
        }

        // Updating the m_current_req variable with the new request.
        m_current_req = request();
        m_current_req.data_length = bytes_read-1;
        m_current_req.request_command = (command)buffer[0];
        if(bytes_read > 1){
            m_current_req.data = (char*)malloc(bytes_read-1);
            std::memcpy(m_current_req.data, buffer+1, bytes_read-1);
        }

        return true;
    }
};