#ifndef FUZZING_TEST_INTERFACE_H
#define FUZZING_TEST_INTERFACE_H

#include <mqueue.h>
#include <string>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <sys/ioctl.h>

#define MQ_REQUEST_LENGTH 256
#define MQ_RESPONSE_LENGTH 256

#define PIPE_READ_ERROR_MAX 5
#define PIPE_MAX_MSG 10

namespace testing{

    // Forward declaration of test_receiver.
    class testing_receiver;

    // Abstract definition of a test_interface. This need to be implemented for a specific communication interface.
    class testing_communication{
        public: 

            // Types of interface that exists.
            enum communication{
                MQ, PIPE, INTERFACE_COUNT
            };

            // Possible commands.
            enum command{
                CONTINUE, KILL, SET_BREAKPOINT, REMOVE_BREAKPOINT, ENABLE_MMIO_TRACKING, DISABLE_MMIO_TRACKING, SET_MMIO_READ, ADD_TO_MMIO_READ_QUEUE, WRITE_MMIO, TRIGGER_CPU_INTERRUPT, ENABLE_CODE_COVERAGE, DISABLE_CODE_COVERAGE, GET_CODE_COVERAGE, GET_CODE_COVERAGE_SHM, RESET_CODE_COVERAGE, SET_RETURN_CODE_ADDRESS, GET_RETURN_CODE, DO_RUN, DO_RUN_SHM
            };

            // Possible return status codes.
            enum status{
                STATUS_OK, STATUS_ERROR, STATUS_MALFORMED
            };

            // Possible events that the simulation can produce.
            enum event{
                MMIO_READ, MMIO_WRITE, VP_END, BREAKPOINT_HIT
            };
            
            // Represents a request send to the implemented testing interface with a command ID and flexible length data.
            struct request{
                command cmd;
                char* data = nullptr;
                size_t data_length = 0;
            };

            // Represents a response of the testing interface, which contains of an response status and the response data.
            struct response{
                status response_status;
                char* data = nullptr;
                size_t data_length = 0;
            };

            // Creates a testing interface, this requires a pointer to the test_receiver.
            testing_communication(testing_receiver* receiver):m_testing_receiver(receiver){};

            // Destructor, not required but may be overwriten by an implementation.
            virtual ~testing_communication() {}

            // Virtual function to start the interface. Needs to be overwritten.
            virtual bool start() = 0;

            // Virtual function that sends a response via the implemented interface. Needs to be overwritten.
            virtual bool send_response(response &req) = 0; 

            // Virtual function that indicates, that a new request was received (and also saves it to the m_current_req object). Needs to be overwritten.
            virtual bool receive_request() = 0;

            // Function to read the received request. Needs to be overwritten.
            request get_request();

            // Setting a response to STATUS_MALFORMED.
            static void respond_malformed(response &res);

            // Copying a 32bit integer to a buffer at a start index MSB.
            static void int32_to_bytes(int32_t value, char* buffer, size_t start);

            // Copying a 32bit integer from a buffer at a start index MSB.
            static int32_t bytes_to_int32(const char* buffer, size_t start);

            // Copying a 64bit integer to a buffer at a start index MSB.
            static void int64_to_bytes(int64_t value, char* buffer, size_t start);
            
            // Copying a 64bit integer from a buffer at a start index MSB.
            static int64_t bytes_to_int64(const char* buffer, size_t start);

        protected:

            // Pointer to the test_receiver that was specified during construction. With this functions like logging can be accessed of the test_receiver.
            testing_receiver* m_testing_receiver = nullptr;

            // Object holding the latest request.
            testing_communication::request m_current_req;

    };

    // test_interface implementation for message queues (MQ) communication.
    class mq_testing_communication: public testing_communication{
        public:

            // Creates a mq communication interface with a request and response queue name.
            mq_testing_communication(testing_receiver* testing_receiver, std::string mq_request_name, std::string mq_response_name);
            
            // Destructor of the mq communication interface. This closes both message queues.
            ~mq_testing_communication();

            // Implemented start function which openens both message queues, clears them and sends the string "ready" to the response message queue. If the queues does not exist yet, they will be created.
            bool start() override;

            // Implemented function to send a response. This function will send a message with the response status and then the data to the response message queue. If the data is longer than the configured message length, it will send the data in multiple messages.
            bool send_response(testing_communication::response &req) override;

            // Implemented function that checks for new requests. This function checks the request message queue for new messages and saves the first into the temporary m_current_req object.
            bool receive_request() override;

        private:

            // Clears a message queue by its name.
            void clear_mq(const char* queue_name);

            // Names of request and response message queues.
            char* m_mq_request_name;
            char* m_mq_response_name;

            // Obects that holds settings of both message queues. This object needs to exist the whole time message queues are open and used.
            mq_attr m_attr;

            // Message queues.
            mqd_t m_mqt_requests, m_mqt_responses;

            // Temporary buffer for the received data.
            char m_buffer[MQ_REQUEST_LENGTH];

    };

    // test_interface implementation for pipe communication.
    class pipe_testing_communication: public testing_communication{
        public:

            // Creates a pipe communication interface with a request and response file descriptor. Both pipes with the corresponding FDs must be created before.
            pipe_testing_communication(testing_receiver* testing_receiver, int fd_requests, int fd_response);

            // Destructor of the pipe communication interface. This closes both pipes.
            ~pipe_testing_communication();

            // Implemented start function which writes the string "ready" to response pipe.
            bool start() override;

            // Implemented function to send a response. This will write the response status, data length and then the data to the response pipe.
            bool send_response(testing_communication::response &req) override;

            // Implemented function that checks for new requests. This function checks the request pipe for new request and saves the first into the temporary m_current_req object.
            bool receive_request() override;

        private:

            // File desciptors of the request (read) and response (write) pipes.
            int m_fd_request;
            int m_fd_response;

    };

}  //namespace testing

#endif