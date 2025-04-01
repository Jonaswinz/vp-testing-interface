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

    mq_testing_client::mq_testing_client(std::string request_name, std::string response_name){

        // Copies message queue names to local variables.
        m_request_name = request_name;
        m_response_name = response_name; 

        // Default set to broadcast.
        m_receiver_id = 0;
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
        //clear_mq(m_request_name.c_str());
        //clear_mq(m_response_name.c_str());

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

        pid_t received_pid;
        memcpy(&received_pid, buffer, sizeof(pid_t));

        // Check if this process is the receiver.
        if(received_pid != m_receiver_id){
            // Put message back into the queue.
            mq_send(m_mqt_responses, buffer, bytes_read, 0);
            return false;
        }

        buffer[bytes_read] = '\0'; // Ensure null-termination for valid string.
        std::string message(buffer+sizeof(pid_t));
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
        // 0 - 3                 Byte: Process ID
        // 4                     Byte: Command
        // 5 - (MQ_MAX_LENGTH-1) Byte: Data

        // Response structure:
        // 0 - 3                 Byte: Process ID
        // 4                     Byte: Status
        // 5 - (MQ_MAX_LENGTH-2) Byte: Data
        // MQ_MAX_LENGTH         Byte: More data

        // Check if communication started.
        if(!m_started){
            log_error_message("Communication not started!");
            return false;
        }

        // Check if request (command and data) fits in one message (currently only supported that the request is only one message).
        if(sizeof(pid_t)+req->data_length+1 > MQ_MAX_LENGTH){
            log_error_message("When using MQ the request cannot be larger than the defined MQ_MAX_LENGTH length of %d! Please increase MQ_MAX_LENGTH or use pipe communication instead.", MQ_MAX_LENGTH);
        }

        // Creating a buffer for sending and receiving data.
        char buffer[MQ_MAX_LENGTH];

        // Copy pid, command and data into one buffer.
        memcpy(buffer, &m_receiver_id, sizeof(pid_t));
        buffer[sizeof(pid_t)] = req->request_command;
        memcpy(sizeof(pid_t)+buffer+1, req->data, req->data_length);

        // Send this buffer.
        if (mq_send(m_mqt_requests, buffer, sizeof(pid_t)+req->data_length+1, 0) == -1) {
            log_error_message("Error sending message: %s", strerror(errno));
            return false;
        }

        log_info_message("SENT: %d with length %d.", req->request_command, req->data_length);

        ssize_t bytes_read = 0;

        do{
            // Waiting for a message and writing it to the same buffer.
            bytes_read = mq_receive(m_mqt_responses, buffer, MQ_MAX_LENGTH, NULL);
            if (bytes_read == -1) {
                log_error_message("Error receiving message: %s", strerror(errno));
                return false;
            }

            if (bytes_read < sizeof(pid_t)+1) {
                log_error_message("Received message was too short for a valid request!");
                return false;
            }

            // Extract the receiver process id.
            pid_t received_pid;
            memcpy(&received_pid, buffer, sizeof(pid_t));

            // Check if this is the right response (from the right receiver).
            if(received_pid != m_receiver_id){
                // Put message back into the queue.
                mq_send(m_mqt_responses, buffer, bytes_read, 0);
            }else{
                break;
            }
        }while(true);

        // Clear old data if existed.
        if(res->data != nullptr){
            free(res->data);
            res->data = nullptr;
        }

        // Reading the response status from the buffer
        res->response_status = (status)buffer[sizeof(pid_t)];

        // Error checking of the response status.
        if(res->response_status == STATUS_ERROR){
            log_error_message("The status of the request indicated an error!");
            return false;
        }else if(res->response_status == STATUS_MALFORMED){
            log_error_message("The the request was malformed!");
            return false;
        }

        res->data_length = bytes_read-1-sizeof(pid_t);
        if(bytes_read-sizeof(pid_t) > 1){
            res->data = (char*)malloc(bytes_read-1-sizeof(pid_t));
            std::memcpy(res->data, buffer+1+sizeof(pid_t), bytes_read-1-sizeof(pid_t));
        }

        log_info_message("RECEIVED: %d with length %d.", res->response_status, res->data_length);

        return true;
    }

    void mq_testing_client::set_receiver(pid_t receiver_id){
        m_receiver_id = receiver_id;
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
