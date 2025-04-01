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

namespace testing{

    testing_receiver::testing_receiver(){

        m_instance = this;

        // Create two mutex for event synchonization.
        // m_empty_slots signals an empty event queue.
        // m_full_slots signals new events in event queue.
        sem_init(&m_empty_slots, 0, 0); 
        sem_init(&m_full_slots, 0, 0);
    }

    testing_receiver::~testing_receiver(){

        m_instance = nullptr;

        // Delete mutexes.
        sem_destroy(&m_empty_slots);
        sem_destroy(&m_full_slots);

        // Free / data objects if exist.
        if(m_current_req.data != nullptr) free(m_current_req.data);
        if(m_current_res.data != nullptr) free(m_current_res.data);

        if(m_communication != nullptr) delete m_communication;
    }

    bool testing_receiver::set_communication(testing_communication* communication){
        if(communication == nullptr){
            return false;
        }

        if(!communication->is_started()) communication->start();
        
        m_communication = communication;
        return true;
    }

    bool testing_receiver::start_receiver_in_thread(){
        log_info_message("Receiver thread starting.");

        // Starts the receiver loop inside a new thread.
        m_receiver_thread = std::thread([this] {
            this->receiver_loop();
        });

        return true;
    }

    status testing_receiver::handle_do_run_shm(std::string start_breakpoint, std::string end_breakpoint, uint64_t mmio_address, size_t mmio_length, int shm_id, unsigned int offset, bool stop_after_string_termination, std::string &register_name)
    {
        log_info_message("Loading MMIO data from shared memory %d.", shm_id);

        // Attach the shared memory segment to the process's address space
        // Using shared memory directly for better performance. Copying would be safer, but we want performance here.
        char* mmio_data = static_cast<char*>(shmat(shm_id, nullptr, SHM_RDONLY));
        if (mmio_data == reinterpret_cast<char*>(-1)) {
            log_error_message("Failed to attach shared memory segment: %s", strerror(errno));
            return STATUS_ERROR;
        }

        struct shmid_ds shm_info;
        if (shmctl(shm_id, IPC_STAT, &shm_info) == -1) {
            log_error_message("Reading length of shared memory failed!");
            return STATUS_ERROR;
        }

        size_t data_length = shm_info.shm_segsz-offset;

        if(stop_after_string_termination){
            // Length including the termination character
            // To not be longer than data_length the -1 is required.
            data_length = strnlen(mmio_data+offset, data_length - 1) + 1;
        }

        status return_status = this->handle_do_run(start_breakpoint, end_breakpoint, mmio_address, mmio_length, data_length, mmio_data+offset, register_name);

        // Detach the shared memory
        if (shmdt(mmio_data) == -1) {
            log_error_message("Failed to detach shared memory: %s", strerror(errno));
            // Continue to return the read data even if detaching fails
        }

        return return_status;
    }

    status testing_receiver::handle_get_code_coverage_shm(int shm_id, unsigned int offset)
    {
        //TODO check if coverage was enabled !?

        log_info_message("Writing Code Coverage to %d with offset %d.", shm_id, offset);

        // Attach the shared memory segment
        char* shm_addr = static_cast<char*>(shmat(shm_id, nullptr, 0));
        if (shm_addr == reinterpret_cast<char*>(-1)) {
            log_error_message("Failed to attach code coverage shared memory: %s", strerror(errno));
            return STATUS_ERROR;
        }

        struct shmid_ds shm_info;
        if (shmctl(shm_id, IPC_STAT, &shm_info) == -1) {
            log_error_message("Reading length of coverage shared memory failed!");
            return STATUS_ERROR;
        }

        if(MAP_SIZE*sizeof(uint8_t) > (size_t)shm_info.shm_segsz-offset){
            log_error_message("Coverage map does not fit into the shared memory!");
            return STATUS_ERROR;
        }

        // Write the data to the shared memory
        std::memcpy(shm_addr+offset, m_bb_array, MAP_SIZE*sizeof(uint8_t));

        // Detach the shared memory
        if (shmdt(shm_addr) == -1) {
            log_error_message("Failed to detach code coverage shared memory: %s", strerror(errno));
            return STATUS_ERROR;
        }

        return  STATUS_OK;
    }

    testing_receiver* testing_receiver::m_instance = nullptr;

    void testing_receiver::notify_VP_ERROR_event(){
        if(m_instance == nullptr) return;
        m_instance->notify_event(event{VP_ERROR, nullptr, 0});
    }

    void testing_receiver::receiver_loop() {

        while (true) {

            if(m_communication->receive_request()){

                m_current_req = m_communication->get_request();
                m_current_res = response();

                log_info_message("Successfully received request with command: %d", (uint8_t)m_current_req.request_command);

                //Handling request
                handle_request(m_current_req, m_current_res);

                //TODO return status

                if(m_communication->send_response(m_current_res)){
                    log_info_message("Successfully sent response for command: %d", (uint8_t)m_current_req.request_command);
                }else{
                    log_error_message("Could not send response for command: %d", (uint8_t)m_current_req.request_command);
                }

                //Clearing response data. Request data is cleared by m_communication.
                if(m_current_res.data != nullptr){
                    free(m_current_res.data);
                    m_current_res.data = nullptr;
                }
            }
        }
    }

    void testing_receiver::log_info_message(const char* fmt, ...){
        //To be overwritten of message should be logged.    
    }

    void testing_receiver::log_error_message(const char* fmt, ...){
        //To be overwritten of message should be logged.    
    }

    void testing_receiver::continue_to_next_event(){
        // Let's greenlight the other threads, we're ready to process
        sem_post(&m_empty_slots);
    }

    void testing_receiver::wait_for_events_processes(){
        // Wait for all previous events were processes.
        sem_wait(&m_empty_slots);
    }

    void testing_receiver::notify_event(event new_event){
        m_event_queue.push_back(new_event);

        //Notify new event.
        sem_post(&m_full_slots);
    }

    void testing_receiver::notify_MMIO_READ_event(uint64_t address, uint32_t length){
        
        char* buffer = (char *)malloc(12);
        testing_communication::int64_to_bytes(address, buffer, 0);
        testing_communication::int32_to_bytes(length, buffer, 8);

        notify_event(event{MMIO_READ, buffer, 12});
    }

    void testing_receiver::notify_MMIO_WRITE_event(uint64_t address, uint32_t length, char* data){

        char* buffer = (char *)malloc(12+length);
        testing_communication::int64_to_bytes(address, buffer, 0);
        testing_communication::int32_to_bytes(length, buffer, 8);
        memcpy(buffer+12, data, length);

        notify_event(event{MMIO_READ, buffer, 12+length});
    }

    void testing_receiver::notify_VP_END_event(){
        notify_event(event{VP_END, nullptr, 0});
    }

    void testing_receiver::notify_BREAKPOINT_HIT_event(std::string &symbol_name){
        char* buffer = (char *)malloc(symbol_name.size()+1);
        memcpy(buffer, symbol_name.c_str(), symbol_name.size());

        // Termination character.
        buffer[symbol_name.size()] = 0;

        notify_event(event{BREAKPOINT_HIT, buffer, (uint32_t)symbol_name.size()+1});
    }

    void testing_receiver::wait_for_event(){
        //Wait until next suspending.
        sem_wait(&m_full_slots);
    }

    bool testing_receiver::is_event_queue_empty(){
        return m_event_queue.empty();
    }

    event testing_receiver::get_and_remove_first_event(){
        event last_event = m_event_queue.front();
        m_event_queue.pop_front();

        return last_event;
    }

    void testing_receiver::reset_code_coverage(){
        // Writes all zeros to the basic block tracing array.
        memset(m_bb_array, 0, MAP_SIZE*sizeof(uint8_t));
    }

    std::string testing_receiver::get_code_coverage(){
        // Copying the bb tracke map to a string.
        std::vector<uint8_t> v(m_bb_array, m_bb_array + sizeof m_bb_array / sizeof m_bb_array[0]);
        std::string s(v.begin(), v.end());
        return s;
    }

    void testing_receiver::set_block(uint64_t pc){
        //Writing to the basic block traching map. 
        uint64_t curr_bb_loc = (pc >> 4) ^ (pc << 8);
        curr_bb_loc &= MAP_SIZE - 1;

        m_bb_array[curr_bb_loc ^ m_prev_bb_loc]++;
        m_prev_bb_loc = curr_bb_loc >> 1;
    }

    bool testing_receiver::check_exact_request_length(request &req, response &res, size_t length){
        if(req.data_length != length){
            log_error_message("Request has a different length %d than the exepcted %d!", req.data_length, length);
            testing_communication::respond_malformed(res);
            return false;
        }

        return true;
    }

    bool testing_receiver::check_min_request_length(request &req, response &res, size_t length){
        if(req.data_length < length){
            log_error_message("Request has a different length %d than the exepcted >= %d!", req.data_length, length);
            testing_communication::respond_malformed(res);
            return false;
        }

        return true;
    }

    void testing_receiver::handle_request(request &req, response &res){

        // This switch statement calles the right handler functions.
        // When there is response data, it will be allocated and assigned to the response. This memory will be freed when the response is sent.

        switch(req.request_command){

            case CONTINUE:
            {
                // Checks request length to be as expected and if not also changes the response to contain STATUS_MALFORMED.
                if(!check_exact_request_length(req, res, 0)) return;
                
                // Runs he handle function and copys the data to the response data.
                event last_event;
                res.response_status = handle_continue(last_event);

                // Creates response data array with the required length.
                res.data_length = last_event.additional_data_length+1;
                res.data = (char*)malloc(res.data_length);
                
                // Write event type and additional data to response data.
                res.data[0] = (char)last_event.event;
                memcpy(res.data+1, last_event.addition_data, last_event.additional_data_length);
                
                // Freeing additional data.
                if(last_event.addition_data != nullptr) free(last_event.addition_data);

                break;
            }

            case KILL:
            {
                // Expect 1 byte of data: gracefully
                if(!check_exact_request_length(req, res, 1)) return;

                char gracefully = req.data[0];
                res.response_status = handle_kill((bool)gracefully);

                // No data to be returned.
                res.data_length = 0;
                res.data = nullptr;

                break;
            }

            case SET_BREAKPOINT:
            {
                // Expect minimum 2 bytes of data: offset, min. one character 
                if(!check_min_request_length(req, res, 2)) return;

                uint8_t offset = req.data[0];
                // The length of the symbol name is determined by the data length without the offset. So not additional length checking is required.
                std::string symbol_name(req.data + 1, req.data_length-1);

                res.response_status = handle_set_breakpoint(symbol_name, offset);
                res.data_length = 0;
                res.data = nullptr;

                break;
            }

            case REMOVE_BREAKPOINT:
            {
                // Expect minimum 1 bytes of data:  min. one character 
                if(!check_min_request_length(req, res, 1)) return;
                
                // The length of the symbol name is determined by the data length. So not additional length checking is required.
                std::string symbol_name(req.data, req.data_length);

                res.response_status = handle_remove_breakpoint(symbol_name);
                res.data_length = 0;
                res.data = nullptr;

                break;
            }

            case ENABLE_MMIO_TRACKING:
            {
                // Expect minimum 9 bytes of data: 8 bytes start address, 8 bytes end address, 1 byte mode.
                if(!check_exact_request_length(req, res, 17)) return;

                uint64_t start_address = testing_communication::bytes_to_int64(req.data, 0);
                uint64_t end_address = testing_communication::bytes_to_int64(req.data, 8);
                char mode = req.data[16];

                res.response_status = handle_enable_mmio_tracking(start_address, end_address, mode);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case DISABLE_MMIO_TRACKING:
            {
                if(!check_exact_request_length(req, res, 0)) return;

                res.response_status = handle_disable_mmio_tracking();
                res.data = nullptr;
                res.data_length = 0;
                
                break;
            }

            case SET_MMIO_VALUE:
            {   
                // Expect minimum 1 bytes of data: min. 1 byte of mmio data.
                if(!check_min_request_length(req, res, 1)) return;

                res.response_status = handle_set_mmio_value(res.data_length, &req.data[4]);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case ADD_TO_MMIO_READ_QUEUE:
            {   
                // Expect minimum 15 bytes of data: 8 bytes address, 4 bytes length, 4 byte data length + at least one byte data.
                if(!check_min_request_length(req, res, 17)) return;

                uint64_t address = testing_communication::bytes_to_int64(req.data, 0);
                uint32_t length = testing_communication::bytes_to_int32(req.data, 8);
                uint32_t data_length = testing_communication::bytes_to_int32(req.data, 12);

                // Check if the total length matches
                if(!check_exact_request_length(req, res, 16+data_length)) return;

                res.response_status = handle_add_to_mmio_read_queue(address, length, data_length, &req.data[16]);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case SET_CPU_INTERRUPT_TRIGGER:
            {   
                if(!check_exact_request_length(req, res, 16)) return;
                
                uint64_t interrupt_address = testing_communication::bytes_to_int64(req.data, 0);
                uint64_t trigger_address = testing_communication::bytes_to_int64(req.data, 8);

                res.response_status = handle_set_cpu_interrupt_trigger(interrupt_address, trigger_address);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case ENABLE_CODE_COVERAGE:
            {
                if(!check_exact_request_length(req, res, 0)) return;

                res.response_status = handle_enable_code_coverage();
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case DISABLE_CODE_COVERAGE:
            {
                if(!check_exact_request_length(req, res, 0)) return;

                res.response_status = handle_disable_code_coverage();
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case GET_CODE_COVERAGE:
            {
                if(!check_exact_request_length(req, res, 0)) return;

                std::string* coverage = nullptr;
                
                res.response_status = handle_get_code_coverage(coverage);

                if(coverage != nullptr){
                    res.data = (char*)malloc(coverage->size()+4);
                    testing_communication::int32_to_bytes(coverage->size(), res.data, 0);
                    memcpy(res.data+4, coverage->c_str(), coverage->size());
                    res.data_length = coverage->size()+4;
                }else{
                    log_error_message("The coverage string was a nullptr!");
                    res.response_status = STATUS_ERROR;
                }

                break;
            }

            case GET_CODE_COVERAGE_SHM:
            {
                // Min of 8 bytes length: shm_id and offset 4 bytes each.
                if(!check_exact_request_length(req, res, 8)) return;

                uint32_t shm_id = testing_communication::bytes_to_int32(req.data, 0);
                uint32_t offset = testing_communication::bytes_to_int32(req.data, 4);

                res.response_status = handle_get_code_coverage_shm(shm_id, offset);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case RESET_CODE_COVERAGE:
            {
                if(!check_exact_request_length(req, res, 0)) return;

                res.response_status = handle_reset_code_coverage();
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case SET_RETURN_CODE_ADDRESS:
            {   

                // Expect minimum 1 bytes of data: 4 bytes address, min. 1 byte reg name
                if(!check_min_request_length(req, res, 5)) return;

                uint64_t address = testing_communication::bytes_to_int64(req.data, 0);
                // The length of the symbol name is determined by the data length without the address. So not additional length checking is required.
                std::string reg_name(req.data + 4, req.data_length-4);

                res.response_status = handle_set_return_code_address(address, reg_name);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case GET_RETURN_CODE:
            {   
                if(!check_exact_request_length(req, res, 0)) return;

                uint64_t exit_code;
                res.response_status = handle_get_return_code(exit_code);               
                res.data_length = sizeof(uint64_t);
                res.data = (char*)malloc(res.data_length); 
                testing_communication::int64_to_bytes(exit_code, res.data, 0);

                break;
            }

            case DO_RUN:
            {

                // Content:
                // (8 Bytes) MMIO address +
                // (4 Bytes) MMIO length +
                // (4 Bytes) MMIO data length +
                // (1 Bytes) Start breakpoint name length +
                // (1 Bytes) End breakpoint name length +
                // (1 Bytes) Register name length +
                // (? Bytes) Start breakpoint name +
                // (? Bytes) End breakpoint name +
                // (? Bytes) Return register name +
                // (? Bytes) Data

                // Min of 20 bytes length (least one byte of data).
                if(!check_min_request_length(req, res, 20)) return;

                uint64_t address = testing_communication::bytes_to_int64(req.data, 0);
                uint32_t length = testing_communication::bytes_to_int32(req.data, 8);
                uint32_t data_length = testing_communication::bytes_to_int32(req.data, 12);

                uint8_t start_breakpoint_length = req.data[16];
                uint8_t end_breakpoint_length = req.data[17];
                uint8_t register_name_length = req.data[18];
                
                // Check again with all length combined.
                if(!check_exact_request_length(req, res, 19+start_breakpoint_length+end_breakpoint_length+register_name_length+data_length)) return;

                std::string start_breakpoint(&req.data[19], start_breakpoint_length);
                std::string end_breakpoint(&req.data[start_breakpoint_length+19], end_breakpoint_length);
                std::string register_name(&req.data[start_breakpoint_length+end_breakpoint_length+19], register_name_length);

                res.response_status = handle_do_run(start_breakpoint, end_breakpoint, address, length, data_length, &req.data[19+start_breakpoint_length+end_breakpoint_length+register_name_length], register_name);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case DO_RUN_SHM:
            {

                // Content:
                // (8 Bytes) MMIO address +
                // (4 Bytes) MMIO length +
                // (4 Bytes) Shared memory ID +
                // (4 Bytes) SHM offset +
                // (1 Bytes) Stop after string termination +
                // (1 Bytes) Start breakpoint name length +
                // (1 Bytes) End breakpoint name length +
                // (1 Bytes) Register name length +
                // (? Bytes) Start breakpoint name +
                // (? Bytes) End breakpoint name +
                // (? Bytes) Return register name +
                // (? Bytes) Data

                // Min of 25 bytes length (at least one byte of data).
                if(!check_min_request_length(req, res, 25)) return;

                uint64_t address = testing_communication::bytes_to_int64(req.data, 0);
                uint32_t length = testing_communication::bytes_to_int32(req.data, 8);
                uint32_t shm_id = testing_communication::bytes_to_int32(req.data, 12);
                uint32_t offset = testing_communication::bytes_to_int32(req.data, 16);

                char stop_after_string_termination = req.data[20];

                uint8_t start_breakpoint_length = req.data[21];
                uint8_t end_breakpoint_length = req.data[22];
                uint8_t register_name_length = req.data[23];

                // Check again with all length combined.
                if(!check_exact_request_length(req, res, 24+start_breakpoint_length+end_breakpoint_length+register_name_length)) return;

                std::string start_breakpoint(&req.data[24], start_breakpoint_length);
                std::string end_breakpoint(&req.data[start_breakpoint_length+24], end_breakpoint_length);
                std::string register_name(&req.data[start_breakpoint_length+end_breakpoint_length+24], register_name_length);

                res.response_status = handle_do_run_shm(start_breakpoint, end_breakpoint, address, length, shm_id, offset, (bool)stop_after_string_termination, register_name);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case SET_ERROR_SYMBOL:
            {   

                // Expect minimum 1 bytes of data: symbol name
                if(!check_min_request_length(req, res, 1)) return;

                // The length of the symbol name is determined by the data length. So not additional length checking is required.
                std::string symbol(req.data, req.data_length);

                res.response_status = handle_set_error_symbol(symbol);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case SET_FIXED_READ:
            {   
                // Expect minimum 10 bytes of data: count, one address and one byte of data.
                if(!check_min_request_length(req, res, 10)) return;

                // Number of fixed read definitions inside the data.
                uint8_t count = req.data[0];
                
                // For each fixed read there needs to be information about the addres (8 byte) and the data (1 byte).
                if(!check_exact_request_length(req, res, 1+count*9)) return;

                res.response_status = handle_set_fixed_read(count, &req.data[1]);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case GET_CPU_PC:
            {   
                if(!check_exact_request_length(req, res, 0)) return;

                uint64_t cpu_pc;
                res.response_status = handle_get_cpu_pc(cpu_pc);               
                res.data_length = sizeof(uint64_t);
                res.data = (char*)malloc(res.data_length); 
                testing_communication::int64_to_bytes(cpu_pc, res.data, 0);

                break;
            }

            case JUMP_CPU_TO:
            {   
                if(!check_exact_request_length(req, res, 8)) return;

                uint64_t address = testing_communication::bytes_to_int64(req.data, 0);

                res.response_status = handle_jump_cpu_to(address);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case STORE_CPU_REGISTERS:
            {   
                if(!check_exact_request_length(req, res, 0)) return;

                res.response_status = handle_store_cpu_register();
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case RESTORE_CPU_REGISTERS:
            {   
                if(!check_exact_request_length(req, res, 0)) return;

                res.response_status = handle_restore_cpu_register();
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            default:
            {
                log_info_message("Command %d not found!", req.request_command);
                break;
            }

        }
    }

    request testing_communication::get_request(){
        return m_current_req;
    }

};