#include "testing_receiver.h"

namespace testing{

    testing_receiver::testing_receiver(){

        // Create two mutex for event synchonization.
        // m_empty_slots signals an empty event queue.
        // m_full_slots signals new events in event queue.
        sem_init(&m_empty_slots, 0, 0); 
        sem_init(&m_full_slots, 0, 0);
    }

    testing_receiver::~testing_receiver(){

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

    status testing_receiver::handle_do_run_shm(std::string start_breakpoint, std::string end_breakpoint, uint64_t mmio_address, uint64_t mmio_length, int shm_id, unsigned int offset, bool stop_after_string_termination)
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
            data_length = strnlen(mmio_data, data_length - 1) + 1;
        }

        size_t elements = data_length / mmio_length;

        if(data_length % mmio_length > 0){
            log_error_message("The shared memory segment of size %d cannot be devided by element length %d without remainder! Continuing.");
        }

        status return_status = this->handle_do_run(start_breakpoint, end_breakpoint, mmio_address, mmio_length, elements, mmio_data+offset);

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

    void testing_receiver::receiver_loop() {

        while (true) {

            if(m_communication->receive_request()){

                m_current_req = m_communication->get_request();
                m_current_res = response();

                log_info_message("Successfully received request with command: %d", (uint8_t)m_current_req.cmd);

                //Handling request
                handle_request(m_current_req, m_current_res);

                //TODO return status

                if(m_communication->send_response(m_current_res)){
                    log_info_message("Successfully sent response for command: %d", (uint8_t)m_current_req.cmd);
                }else{
                    log_error_message("Could not send response for command: %d", (uint8_t)m_current_req.cmd);
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

        switch(req.cmd){

            case CONTINUE:
            {
                // Checks request length to be as expected and if not also changes the response to contain STATUS_MALFORMED.
                if(check_exact_request_length(req, res, 0)) return;
                
                // Creates response data array with the required length.
                res.data_length = sizeof(event);
                res.data = (char*)malloc(res.data_length);
                
                // Runs he handle function and copys the data to the response data.
                event last_event;
                res.response_status = handle_continue(last_event);
                memcpy(res.data, &last_event, sizeof(event));

                break;
            }

            case KILL:
            {
                // Expect 1 byte of data: gracefully
                if(check_exact_request_length(req, res, 1)) return;

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
                if(check_min_request_length(req, res, 2)) return;

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
                if(check_min_request_length(req, res, 1)) return;
                
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
                if(check_exact_request_length(req, res, 17)) return;

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
                if(check_exact_request_length(req, res, 0)) return;

                res.response_status = handle_disable_mmio_tracking();
                res.data = nullptr;
                res.data_length = 0;
                
                break;
            }

            case SET_MMIO_READ:
            {   
                // Expect minimum 1 bytes of data: min. 1 byte of mmio data.
                if(check_min_request_length(req, res, 1)) return;

                res.response_status = handle_set_mmio_read(res.data_length, &req.data[4]);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case ADD_TO_MMIO_READ_QUEUE:
            {   
                // Expect minimum 16 bytes of data: 8 bytes address, 4 bytes length, 4 byte element count.
                if(check_min_request_length(req, res, 16)) return;

                uint64_t address = testing_communication::bytes_to_int64(req.data, 0);
                uint32_t length = testing_communication::bytes_to_int32(req.data, 8);
                uint32_t elements = testing_communication::bytes_to_int32(req.data, 12);

                // Check if the total length matches
                if(check_exact_request_length(req, res, 16+length*elements)) return;

                res.response_status = handle_add_to_mmio_read_queue(address, length, elements, &req.data[16]);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case WRITE_MMIO:
            {   
                // Expect minimum 9 bytes of data: 8 bytes address, 8 bytes length.
                if(check_min_request_length(req, res, 16)) return;
                
                uint64_t address = testing_communication::bytes_to_int64(req.data, 0);
                uint64_t length = testing_communication::bytes_to_int64(req.data, 8);

                // Check if the total length matches
                if(check_exact_request_length(req, res, 16+length)) return;

                res.response_status = handle_write_mmio(address, length, &req.data[16]);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case TRIGGER_CPU_INTERRUPT:
            {   
                // Expect minimum 1 bytes of data: interrupt index
                if(check_min_request_length(req, res, 1)) return;
                
                uint8_t interrupt = res.data[0];

                res.response_status = handle_trigger_cpu_interrupt(interrupt);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case ENABLE_CODE_COVERAGE:
            {
                if(check_exact_request_length(req, res, 0)) return;

                res.response_status = handle_enable_code_coverage();
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case DISABLE_CODE_COVERAGE:
            {
                if(check_exact_request_length(req, res, 0)) return;

                res.response_status = handle_disable_code_coverage();
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case GET_CODE_COVERAGE:
            {
                if(check_exact_request_length(req, res, 0)) return;

                std::string coverage = handle_get_code_coverage();

                res.data = (char*)malloc(coverage.size()+4);
                testing_communication::int32_to_bytes(coverage.size(), res.data, 0);
                memcpy(res.data+4, coverage.c_str(), coverage.size());
                res.data_length = coverage.size()+4;

                break;
            }

            case GET_CODE_COVERAGE_SHM:
            {
                // Min of 8 bytes length: shm_id and offset 4 bytes each.
                if(check_exact_request_length(req, res, 8)) return;

                uint32_t shm_id = testing_communication::bytes_to_int32(req.data, 0);
                uint32_t offset = testing_communication::bytes_to_int32(req.data, 4);

                res.response_status = handle_get_code_coverage_shm(shm_id, offset);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case RESET_CODE_COVERAGE:
            {
                if(check_exact_request_length(req, res, 0)) return;

                res.response_status = handle_reset_code_coverage();
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            case SET_RETURN_CODE_ADDRESS:
            {   

                // Expect minimum 1 bytes of data: 4 bytes address, min. 1 byte reg name
                if(check_min_request_length(req, res, 5)) return;

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
                if(check_exact_request_length(req, res, 0)) return;

                res.data_length = sizeof(uint64_t);
                res.data = (char*)malloc(res.data_length);
                uint64_t exit_code = handle_get_return_code();                
                testing_communication::int32_to_bytes(exit_code, res.data, 0);

                break;
            }

            case DO_RUN:
            {

                // Content:
                // (8 Bytes) MMIO address +
                // (4 Bytes) MMIO length +
                // (4 Bytes) MMIO elements +
                // (1 Bytes) Start breakpoint name length +
                // (1 Bytes) End breakpoint name length +
                // (? Bytes) Start breakpoint name +
                // (? Bytes) End breakpoint name +
                // (? Bytes) Data

                // Min of 20 bytes length (both names at least one characters).
                if(check_min_request_length(req, res, 20)) return;

                uint64_t address = testing_communication::bytes_to_int64(req.data, 0);
                uint32_t length = testing_communication::bytes_to_int32(req.data, 8);
                uint32_t elements = testing_communication::bytes_to_int32(req.data, 12);

                uint8_t start_breakpoint_length = req.data[16];
                uint8_t end_breakpoint_length = req.data[17];
                
                // Check again with all length combined.
                if(check_exact_request_length(req, res, 18+start_breakpoint_length+end_breakpoint_length+(length*elements))) return;

                std::string start_breakpoint(&req.data[18], start_breakpoint_length);
                std::string end_breakpoint(&req.data[start_breakpoint_length+18], end_breakpoint_length);

                res.response_status = handle_do_run(start_breakpoint, end_breakpoint, address, length, elements, &req.data[18+start_breakpoint_length+end_breakpoint_length]);
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
                // (? Bytes) Start breakpoint name +
                // (? Bytes) End breakpoint name 

                // Min of 25 bytes length (both names at least one characters).
                if(check_min_request_length(req, res, 25)) return;

                uint64_t address = testing_communication::bytes_to_int64(req.data, 0);
                uint32_t length = testing_communication::bytes_to_int32(req.data, 8);
                uint32_t shm_id = testing_communication::bytes_to_int32(req.data, 12);
                uint32_t offset = testing_communication::bytes_to_int32(req.data, 16);

                char stop_after_string_termination = req.data[20];

                uint8_t start_breakpoint_length = req.data[21];
                uint8_t end_breakpoint_length = req.data[22];

                // Check again with all length combined.
                if(check_exact_request_length(req, res, 23+start_breakpoint_length+end_breakpoint_length)) return;

                std::string start_breakpoint(&req.data[23], start_breakpoint_length);
                std::string end_breakpoint(&req.data[start_breakpoint_length+23], end_breakpoint_length);


                res.response_status = handle_do_run_shm(start_breakpoint, end_breakpoint, address, length, shm_id, offset, (bool)stop_after_string_termination);
                res.data = nullptr;
                res.data_length = 0;

                break;
            }

            default:
            {
                log_info_message("Command %d not found!", req.cmd);
                break;
            }

        }
    }

    request testing_communication::get_request(){
        return m_current_req;
    }

};