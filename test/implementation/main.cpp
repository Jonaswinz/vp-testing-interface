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

#include "testing_receiver.h"
#include "types.h"

#include <iostream>
#include <cstdarg>
#include <cstdio>

class test_testing_receiver: public testing::testing_receiver{

    public:

        test_testing_receiver(){

        }

    protected:

        void log_info_message(const char* fmt, ...){
            std::cout << "[VP INFO]: ";
            va_list args;
            va_start(args, fmt);
            char buffer[1024];  // Buffer for formatted message
            vsnprintf(buffer, sizeof(buffer), fmt, args);
            va_end(args);
            std::cout << buffer << std::endl;
        }

        void log_error_message(const char* fmt, ...){
            std::cout << "[VP ERROR]: ";
            va_list args;
            va_start(args, fmt);
            char buffer[1024];  // Buffer for formatted message
            vsnprintf(buffer, sizeof(buffer), fmt, args);
            va_end(args);
            std::cout << buffer << std::endl;
        }

        testing::status handle_continue(testing::event &last_event){

            char* test = (char*)malloc(5);

            last_event = testing::event{testing::BREAKPOINT_HIT, test, 5};
            return testing::STATUS_OK;
        }

        testing::status handle_kill(bool gracefully){
            log_info_message("HANDLE_KILL: gracefully %d.", gracefully);
            return testing::STATUS_OK;
        }

        testing::status handle_set_breakpoint(std::string &symbol, int offset){
            log_info_message("SET_BREAKPOINT: symbol %s, offset %d.", symbol, offset);
            return testing::STATUS_OK;
        }

        testing::status handle_remove_breakpoint(std::string &symbol){
            log_info_message("REMOVE_BREAKPOINT: symbol %s.", symbol);
            return testing::STATUS_OK;
        }

        testing::status handle_enable_mmio_tracking(uint64_t start_address, uint64_t end_address, char mode){
            log_info_message("ENABLE_MMIO_TRACKING: start_address %d, end_address %d, mode %d.", start_address, end_address, mode);
            return testing::STATUS_OK;
        }

        testing::status handle_disable_mmio_tracking(){
            log_info_message("DISABLE_MMIO_TRACKING");
            return testing::STATUS_OK;
        }

        testing::status handle_set_mmio_read(size_t length, char* value){
            log_info_message("SET_MMIO_READ: length %s, value %s.", length, value);
            return testing::STATUS_OK;
        }

        testing::status handle_add_to_mmio_read_queue(uint64_t address, size_t length, size_t element_count, char* value){
            log_info_message("ADD_TO_MMIO_READ_QUEUE: address %d, length %d, element_count %d, value %s.", address, length, element_count, value);
            return testing::STATUS_OK;
        }

        testing::status handle_write_mmio(uint64_t address, uint64_t length, char* value){
            log_info_message("WRITE_MMIO: address %d, length %d, value %s.", address, length, value);
            return testing::STATUS_OK;
        }

        testing::status handle_trigger_cpu_interrupt(uint8_t interrupt){
            log_info_message("TRIGGER_CPI_INTERRUPT: interrupt %d.", interrupt);
            return testing::STATUS_OK;
        }

        testing::status handle_enable_code_coverage(){
            log_info_message("ENABLE_CODE_COVERAGE");
            return testing::STATUS_OK;
        }

        testing::status handle_reset_code_coverage(){
            log_info_message("RESET_CODE_COVERAGE");
            return testing::STATUS_OK;
        }

        testing::status handle_disable_code_coverage(){
            log_info_message("DISABLE_CODE_COVERAGE");
            return testing::STATUS_OK;
        }

        std::string handle_get_code_coverage(){
            log_info_message("GET_CODE_COVERAGE");
            return "adsf";
        }

        testing::status handle_set_return_code_address(uint64_t address, std::string reg_name){
            log_info_message("SET_RETURN_CODE_ADDRESS: address %d, reg_name %s.", address, reg_name);
            return testing::STATUS_OK;
        }

        uint64_t handle_get_return_code(){
            log_info_message("GET_RETURN_CODE");
            return 1;
        }

        testing::status handle_do_run(std::string start_breakpoint, std::string end_breakpoint, uint64_t mmio_address, size_t mmio_length, size_t mmio_element_count, char* mmio_value){
            log_info_message("DO_RUN: start_breakpoint %s, end_breakpoint %s, address %d, mmio_address %d, mmio_length %d, element_count %d, value %s.", start_breakpoint, end_breakpoint, mmio_address, mmio_length, mmio_element_count, mmio_value);
            return testing::STATUS_OK;      
        }



};


int main() {
    std::cout << "Testing implementation for vp-testing-interface library!" << std::endl;

    testing::testing_receiver* receiver = new test_testing_receiver();
    //testing::mq_testing_communication* communication = new testing::mq_testing_communication(receiver, "/test-request", "/test-response");
    testing::pipe_testing_communication* communication = new testing::pipe_testing_communication(receiver, 10, 11);

    if(!communication->start()) return 0;

    receiver->set_communication(communication);
    //receiver->start_receiver_in_thread();

    receiver->receiver_loop();

    return 0;
}