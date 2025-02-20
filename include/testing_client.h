#ifndef TESTING_CLIENT_H
#define TESTING_CLIENT_H

#include <iostream>
#include <string>
#include <cstring>
#include <fcntl.h> 
#include <mqueue.h>
#include "types.h"

#define MAX_MSG 10
#define REQUEST_LENGTH 256
#define RESPONSE_LENGTH 256

namespace testing{

    class testing_client {
        public:
            virtual void init() = 0;
            virtual void wait_for_ready() = 0;
            virtual bool send_request(request* req, response* res) = 0;
        protected:
            bool m_ready = false;
    };

    class mq_testing_client: public testing_client{
        public:
            mq_testing_client(std::string request_name, std::string m_response_name);
            ~mq_testing_client();
            void init() override;
            void wait_for_ready() override;
            bool send_request(request* req, response* res) override;
        private:
            void clear_mq(const char* queue_name);

            std::string m_request_name;
            std::string m_response_name;
            mq_attr m_attr;
            mqd_t m_mqt_requests;
            mqd_t m_mqt_responses;
    };
}

#endif