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
        if ((m_mqt_requests = mq_open(m_mq_request_name.c_str(), O_RDWR, 0660, &m_attr)) == -1) {
            m_testing_receiver->log_error_message("Error opening request message queue %s.", m_mq_request_name);
            return false;
        }

        if ((m_mqt_responses = mq_open(m_mq_response_name.c_str(), O_WRONLY, 0644, &m_attr)) == -1) {
            m_testing_receiver->log_error_message("Error opening response message queue %s.", m_mq_response_name);
            return false;
        }

        // Sends "ready" string to response message queue with the current process id, to signal that requests can be sent.
        pid_t this_process = getpid();
        std::string ready_signal = "ready";

        char buffer[sizeof(pid_t)+ready_signal.size()];
        memcpy(buffer, &this_process, sizeof(pid_t));
        memcpy(buffer+sizeof(pid_t), ready_signal.c_str(),  ready_signal.size());

        if(mq_send(m_mqt_responses, buffer, sizeof(pid_t)+ready_signal.size(), 0) == 0){
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
        // 0 - 3                 Byte: Process ID
        // 4                     Byte: Status
        // 5 - (MQ_MAX_LENGTH-1) Byte: Data

        // Check if communication started.
        if(!m_started){
            m_testing_receiver->log_error_message("Communication not started!");
            return false;
        }

        // Check if response (command and data) fits in one message (currently only supported that the response is only one message).
        if(sizeof(pid_t)+res.data_length+1 > MQ_MAX_LENGTH){
            m_testing_receiver->log_error_message("When using MQ the request cannot be larger than the defined MQ_MAX_LENGTH length of %d! Please increase MQ_MAX_LENGTH or use pipe communication instead.", MQ_MAX_LENGTH);
        }

        // Creating a buffer for sending data.
        char buffer[sizeof(pid_t)+res.data_length+1];

        pid_t this_process = getpid();

        // Write the current process id, response status and the data to the buffer.
        memcpy(buffer, &this_process, sizeof(pid_t));
        buffer[sizeof(pid_t)] = res.response_status;
        memcpy(buffer+sizeof(pid_t)+1, res.data, res.data_length);

        // Send the response code and data.
        if(mq_send(m_mqt_responses, buffer, sizeof(pid_t)+res.data_length+1, 0) == -1){
            m_testing_receiver->log_error_message("Error sending response data: %s", strerror(errno));
            return false;
        }

        return true;
    }

    bool mq_testing_communication::receive_request(){

        // Request structure:
        // 0 - 3                 Byte: Process ID
        // 4                     Byte: Command
        // 5 - (MQ_MAX_LENGTH-1) Byte: Data

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

        if (bytes_read < sizeof(pid_t)+1) {
            m_testing_receiver->log_error_message("Message was too short for a valid request!");  
            return false;
        }

        // Extract the receiver process id.
        pid_t received_pid;
        memcpy(&received_pid, buffer, sizeof(pid_t));

        // Check if this process is the receiver.
        if(received_pid != 0 && received_pid != getpid()){
            // Put message back into the queue.
            mq_send(m_mqt_requests, buffer, bytes_read, 0);
            return false;
        }

        // Clear old data if existed.
        if(m_current_req.data != nullptr){
            free(m_current_req.data);
            m_current_req.data = nullptr;
        }

        // Updating the m_current_req variable with the new request.
        m_current_req = request();
        m_current_req.data_length = bytes_read-1-sizeof(pid_t);
        m_current_req.request_command = (command)buffer[sizeof(pid_t)];
        if(bytes_read-sizeof(pid_t) > 1){
            m_current_req.data = (char*)malloc(bytes_read-1-sizeof(pid_t));
            std::memcpy(m_current_req.data, buffer+1+sizeof(pid_t), bytes_read-1-sizeof(pid_t));
        }

        return true;
    }
};