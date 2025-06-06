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

#ifndef TESTING_CLIENT_H
#define TESTING_CLIENT_H

#include <iostream>
#include <string>
#include <cstring>
#include <fcntl.h> 
#include <mqueue.h>

#include "types.h"
#include "testing_communication.h"

namespace testing{

    // Abstract definition of a testing_client (opposite of test_communication). This need to be implemented for specific communication interface.
    class testing_client {
        public:
            // Virtual function to start the interface. Needs to be overwritten.
            virtual bool start() = 0;

            // Virtual function to check if the ready message is there on the interface. Needs to be overwritten.
            virtual bool check_for_ready() = 0;

            // Virtual function to wait for the ready message on the interface. Needs to be overwritten.
            virtual bool wait_for_ready() = 0;

            // Resets the ready state, so requests cannot be send and wait_for_ready must be called again.
            void reset_ready(){
                m_started = false;
            }

            // Virtual function to send a request and wait for the response (and fill the response). Needs to be overwritten.
            virtual bool send_request(request* req, response* res) = 0;

            // Function that does not do any logging.
            static void no_logging(const char* fmt, ...){};

            // Pointer to a function for info logging. It points to the no_logging function by default.
            void (*log_info_message)(const char* fmt, ...) = no_logging;

            // Pointer to a function for error logging. It points to the no_logging function by default.
            void (*log_error_message)(const char* fmt, ...) = no_logging;

        protected:

            // Indicates if the communication was started.
            bool m_started = false;
    };

    // testing_client implementation for message queue communication.
    class mq_testing_client: public testing_client{
        public:
            // Creates a mq testing client for specific request and response message queues.
            mq_testing_client(std::string request_name, std::string response_name);
            ~mq_testing_client();

            // Implemented start function, which openes the message queues. Both message queues will be cleared during starting.
            bool start() override;

            // Implemented check_for_ready function, which check for the "ready" message once, without blocking.
            bool check_for_ready() override;

            // Implemented wait_for_ready function, which waits (blocks) until the "ready" string is received on the response message queue.
            bool wait_for_ready() override;

            // Implemented send_request function, which uses the message queues. For the received data, new memory will be allocated, so after res was used it needs to be freed propertly. If res.data is not a nullptr, the function will try to free it.
            bool send_request(request* req, response* res) override;

            // Sets the receiver pid.
            void set_receiver(pid_t receiver_id);

            // Function to clear lost data of a message queue.
            void clear_mq(const char* queue_name);

        private:

            // String name of the request message queue.
            std::string m_request_name;

            // String name of the response message queue.
            std::string m_response_name;

            // Settings of both message queues.
            mq_attr m_attr;

            // Request message queue
            mqd_t m_mqt_requests;

            // Response message queue.
            mqd_t m_mqt_responses;

            // Process id of the receiver (because multiple may listen).
            pid_t m_receiver_id;
    };

    // testing_client implementation for pipe communication.
    class pipe_testing_client: public testing_client{
        public:
            // Creates a pipe testing client for specific request and response pipes.
            pipe_testing_client(int request_fd, int response_fd);

            // Creates a pipe testing client without specific request and reponse pipe fds.
            pipe_testing_client();
            ~pipe_testing_client();

            // Implemented start function, which openes the pipes. Both pipes will be cleared during starting.
            bool start() override;

            // Implemented check_for_ready function, which check for the "ready" message once, without blocking.
            bool check_for_ready() override;

            // Implemented wait_for_ready function, which waits (blocks) until the "ready" string is received on the response pipe.
            bool wait_for_ready() override;

            // Implemented send_request function, which uses the pipes. For the received data, new memory will be allocated, so after res was used it needs to be freed propertly. If res.data is not a nullptr, the function will try to free it.
            bool send_request(request* req, response* res) override;

            // Getter for the used read FD of the request queue.
            int get_request_fd();

            // Getter for the used write FD of the response queue.
            int get_response_fd();
            
        private:

            // Function to clear lost data of a pipe.
            void clear_pipe(int fd);

            // Indicating if specific m_request_fd and m_response_fd should be used as specific fds.
            bool m_specific_fds = false;

            // File descriptor of the request pipe.
            int m_request_fd;

            // File descriptor of the response pipe.
            int m_response_fd;

            int m_request_pipe[2];
            
            int m_response_pipe[2];
    };
}

#endif