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

            // Virtual function to wait for the ready message on the interface. Needs to be overwritten.
            virtual void wait_for_ready() = 0;

            // Virtual function to send a request and wait for the response (and fill the response). Needs to be overwritten.
            virtual bool send_request(request* req, response* res) = 0;
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

            // Implemented wait_for_ready function, which waits (blocks) until the "ready" string is received on the response message queue.
            void wait_for_ready() override;

            // Implemented send_request function, which uses the message queues. For the received data, new memory will be allocated, so after res was used it needs to be freed propertly. If res.data is not a nullptr, the function will try to free it.
            bool send_request(request* req, response* res) override;
        private:

            // Function to clear lost data of a message queue.
            void clear_mq(const char* queue_name);

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
    };

    // testing_client implementation for pipe communication.
    class pipe_testing_client: public testing_client{
        public:
            // Creates a pipe testing client for specific request and response pipes.
            pipe_testing_client(int request_fd, int response_fd);
            ~pipe_testing_client();

            // Implemented start function, which openes the pipes. Both pipes will be cleared during starting.
            bool start() override;

            // Implemented wait_for_ready function, which waits (blocks) until the "ready" string is received on the response pipe.
            void wait_for_ready() override;

            // Implemented send_request function, which uses the pipes. For the received data, new memory will be allocated, so after res was used it needs to be freed propertly. If res.data is not a nullptr, the function will try to free it.
            bool send_request(request* req, response* res) override;
            
        private:

            // Function to clear lost data of a pipe.
            void clear_pipe(int fd);

            // File descriptor of the request pipe.
            int m_request_fd;

            // File descriptor of the response pipe.
            int m_response_fd;

            int m_request_pipe[2];
            
            int m_response_pipe[2];
    };
}

#endif