#include "testing_receiver.h"
#include "testing_communication.h"
#include "types.h"

#include <iostream>
#include <cstdarg>
#include <cstdio>

using testing::testing_communication;

class test_testing_receiver: public testing::testing_receiver{

    public:

        test_testing_receiver(){

        }

    protected:

        void log_info_message(const char* fmt, ...){
            std::cout << "[INFO]: ";
            va_list args;
            va_start(args, fmt);
            char buffer[1024];  // Buffer for formatted message
            vsnprintf(buffer, sizeof(buffer), fmt, args);
            va_end(args);
            std::cout << buffer << std::endl;
        }

        void log_error_message(const char* fmt, ...){
            std::cout << "[ERROR]: ";
            va_list args;
            va_start(args, fmt);
            char buffer[1024];  // Buffer for formatted message
            vsnprintf(buffer, sizeof(buffer), fmt, args);
            va_end(args);
            std::cout << buffer << std::endl;
        }

        testing_communication::status handle_continue(testing_communication::event &last_event){
            last_event = testing_communication::BREAKPOINT_HIT;
            return testing_communication::STATUS_OK;
        }

        testing_communication::status handle_kill(bool gracefully){
            return testing_communication::STATUS_OK;
        }

        testing_communication::status handle_set_breakpoint(std::string &symbol, int offset){
            return testing_communication::STATUS_OK;
        }

        testing_communication::status handle_remove_breakpoint(std::string &sym_name){
            return testing_communication::STATUS_OK;
        }

        testing_communication::status handle_enable_mmio_tracking(testing::u64 start_address, testing::u64 end_address, char mode){
            return testing_communication::STATUS_OK;
        }

        testing_communication::status handle_disable_mmio_tracking(){
            return testing_communication::STATUS_OK;
        }

        testing_communication::status handle_set_mmio_read(size_t length, char* value){
            return testing_communication::STATUS_OK;
        }

        testing_communication::status handle_add_to_mmio_read_queue(testing::u64 address, size_t length, size_t element_count, char* value){
            return testing_communication::STATUS_OK;
        }

        testing_communication::status handle_write_mmio(testing::u64 address, testing::u64 length, char* value){
            return testing_communication::STATUS_OK;
        }

        testing_communication::status handle_trigger_cpu_interrupt(uint8_t interrupt){
            return testing_communication::STATUS_OK;
        }

        testing_communication::status handle_enable_code_coverage(){
            return testing_communication::STATUS_OK;
        }

        testing_communication::status handle_reset_code_coverage(){
            return testing_communication::STATUS_OK;
        }

        testing_communication::status handle_disable_code_coverage(){
            return testing_communication::STATUS_OK;
        }

        std::string handle_get_code_coverage(){
            return "adsf";
        }

        testing_communication::status handle_set_return_code_address(testing::u64 address, std::string reg_name){
            return testing_communication::STATUS_OK;
        }

        testing::u64 handle_get_return_code(){
            return 1;
        }

        testing_communication::status handle_do_run(std::string start_breakpoint, std::string end_breakpoint, testing::u64 mmio_address, size_t mmio_length, size_t mmio_element_count, char* mmio_value){
            return testing_communication::STATUS_OK;      
        }



};


int main() {
    std::cout << "Testing implementation for vp-testing-interface library!" << std::endl;

    testing::testing_receiver* receiver = new test_testing_receiver();
    testing::mq_testing_communication* communication = new testing::mq_testing_communication(receiver, "/test-request", "/test-response");

    communication->start();

    receiver->set_communication(communication);
    //receiver->start_receiver_in_thread();

    receiver->receiver_loop();

    return 0;
}