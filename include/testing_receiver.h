#ifndef TESTING_RECEIVER_H
#define TESTING_RECEIVER_H

#include <semaphore.h>
#include <sys/shm.h>
#include <deque>
#include <thread>
#include <cstring>
#include <vector>
#include <unistd.h>

#include "testing_communication.h"
#include "types.h"

#define MAP_SIZE_POW2 16
#define MAP_SIZE (1 << MAP_SIZE_POW2)

namespace testing{

    // Abstract definition of the test receiver. This class manages the different commands that are received via the testing communication. This class need to be implemented for the specific virtual platform.
    class testing_receiver{

        public:   

            // Constructor of the class. Initialized the required mutexes.
            testing_receiver();

            // Destructor of the class. This deletes mutexes and frees the current request and response and deletes the used communication object.
            ~testing_receiver();

            // Virtual function for info logging. This function is also used by the selected communication. Needs to be overwritten.
            virtual void log_info_message(const char* fmt, ...);

            // Virtual function for error logging. This function is also used by the selected communication. Needs to be overwritten.
            virtual void log_error_message(const char* fmt, ...);

            // Sets the communication object that should be for communication.
            bool set_communication(testing_communication* communcation);

            // Starts the receiver loop inside a new thread, thus starts the receiving of requests.
            bool start_receiver_in_thread();

            // Infinite loop which checks the communication interface for new requests and then calls the corresponding handlers. After a request is handeled it will send a response back.
            void receiver_loop();

            // Handler for the DO_RUN_SHM command, which reads the test case from the given shared memory region and then calls the handle_do_run function. If stop_after_string_termination is enabled it will stop read the shared memory after the first "\0" (termination character).
            status handle_do_run_shm(std::string start_breakpoint, std::string end_breakpoint, uint64_t mmio_address, size_t mmio_length, int shm_id, unsigned int offset, bool stop_after_string_termination, std::string &register_name);

            // Handler for the GET_CODE_COVERAGE_SHM command, which writes the coverage map (m_bb_array) to the given shared memory region with a given offset.
            status handle_get_code_coverage_shm(int shm_id, unsigned int offset);
            
        protected:

            // Function that signals the receiver to continue to the next events (that the current event was handeled), via the m_empty_slots mutex.
            void continue_to_next_event();

            // Function that blocks until all events are processes, via the m_empty_slots mutex.
            void wait_for_events_processes();

            // Function that notifies the receiver an occourance of an new event, via the m_full_slots mutex. The event will also be added to the event queue. The malloc and freeing of the additional data is not managed inside testing_receiver!
            void notify_event(event new_event);
            
            // Helper for notifiying and adding MMIO_READ event. Allocats the memory for the additional data.
            void notify_MMIO_READ_event(uint64_t address, uint32_t length);

            // Helper for notifiying and adding MMIO_WRITE event. Allocats the memory for the additional data.
            void notify_MMIO_WRITE_event(uint64_t address, uint32_t length, char* data);

            // Helper for notifiying and adding VP_END event.
            void notify_VP_END_event();

            // Helper for notifiying and adding BREAKPOINT_HIT event. Allocats the memory for the additional data.
            void notify_BREAKPOINT_HIT_event(std::string &symbol_name);

            // Function that blocks until a new event occoured, via the m_full_slots mutex.
            void wait_for_event();

            // Checks if the event queue is empty.
            bool is_event_queue_empty();

            // Getter for the first event of the event queue. This will also remove this first event. Freeing of the additional data is not managed inside testing_receiver and must called after the dat is used!
            event get_and_remove_first_event();

            // Function to reset the code coverage, by writing zeros to m_bb_array.
            void reset_code_coverage();

            // Getter for the code coverage array as a string.
            std::string get_code_coverage();

            // Setter for a specific entry (determined by the process counter) in the code coverage array.
            void set_block(uint64_t pc);

        private:   

            // Check if the request has exactly the same length as the given length. If not it also changes the response to be STATUS_MALFORMED.
            bool check_exact_request_length(request &req, response &res, size_t length);

            // Check if the request has minimum the length as the given length. If not it also changes the response to be STATUS_MALFORMED.
            bool check_min_request_length(request &req, response &res, size_t length);

            // Function to handle a request by its pointer and filling the given response. This function will call the handler that corresponds to the request command.
            void handle_request(request &req, response &res);

            // Virtual function to handle a CONTINUE command. Needs to be overwritten. This function will write the first element in the event queue to last_event and removes it from the queue. If the queue is empty after this call the simulation will be resumed, because all events were handeled / returned. The additional data of the event will be freed by the handle_request function.
            virtual status handle_continue(event &last_event) = 0;

            // Virtual function to handle a KILL command. Needs to be overwritten. The boolean gracefully determines if the simulation and whole vp gets killed immediately or shuts down gracefully. If gracefully is set to true, it may take longer for the VP to shut down. It may also be the case that, if the simulation does not terminated, the VP never shuts down.
            virtual status handle_kill(bool gracefully) = 0;

            // Virtual function to handle a SET_BREAKPOINT command. Needs to be overwritten. This function will set a breakpoint by the given symbol name and offset. If the breakpoint is reached a BREAKPOINT_HIT event will triggered.
            virtual status handle_set_breakpoint(std::string &symbol, int offset) = 0;

            // Virtual function to handle a REMOVE_BREAKPOINT command. Needs to be overwritten. This function will remove a breakpoint by the given symbol name.
            virtual status handle_remove_breakpoint(std::string &sym_name) = 0;

            // Virtual function to handle a ENABLE_MMIO_TRACKING command. Needs to be overwritten. This will enable the MMIO interception for a specific address range. The mode determines the tracking type. 0: READ/WRITE, 1: READ, 2: WRITE. If the function is called a second time the new parameters will overwrite the old.
            virtual status handle_enable_mmio_tracking(uint64_t start_address, uint64_t end_address, char mode) = 0;

            // Virtual function to handle a DISABLE_MMIO_TRACKING command. Needs to be overwritten. This will disalbe the mmio tracking.
            virtual status handle_disable_mmio_tracking() = 0;

            // Virtual function to handle a SET_MMIO_VALUE command. Needs to be overwritten. This function sets the read (and intercepted) MMIO data after a MMIO_READ event. For this mmio tracking must be enabled. If multiple events occoured it will set the value according to the occourance.
            virtual status handle_set_mmio_value(size_t length, char* value) = 0;

            // Virtual function to handle a ADD_TO_MMIO_READ_QUEUE command. Needs to be overwritten. This function adds data according to an address to the MMIO read queue. When a read occures and the address fits data and length inside the read queue, it will use the data and the simulation will not be suspended and MMIO_READ event not be triggered. If data in the read queue is shorter than the request length, the MMIO_READ event will be triggered for the remaining data.
            virtual status handle_add_to_mmio_read_queue(uint64_t address, size_t length, size_t data_legnth, char* data) = 0;
            
            // Virtual function to handle a SET_CPU_INTERRUPT_TRIGGER command. Needs to be overwritten. With this function a interrupt (by its address) can be triggered, if the given address is encountered during simulation.
            virtual status handle_set_cpu_interrupt_trigger(uint64_t interrupt_address, uint64_t trigger_address) = 0;

            // Virtual function to handle a ENABLE_CODE_COVERAGE command. This will enable the code coverage tracking.
            virtual status handle_enable_code_coverage() = 0;

            // Virtual function to handle a RESET_CODE_COVERAGE command. This will reset the code coverage, by writing zeros to the tracking array.
            virtual status handle_reset_code_coverage() = 0;

            // Virtual function to handle a DISABLE_CODE_COVERAGE commnd. This will disable the code coverage tracking.
            virtual status handle_disable_code_coverage() = 0;

            // Virtual function to handle a GET_CODE_COVERAGE command. This will return the code coverage array as a string.
            virtual status handle_get_code_coverage(std::string* coverage) = 0;

            // Virtual function to handle a SET_RETURN_CODE_ADDRESS command. This will set the address and register name of the return code that should be saved. A breakpoint will be set to this address and if the breakpoint is hit, the value of the register witht the given name is saved.
            virtual status handle_set_return_code_address(uint64_t address, std::string &reg_name) = 0;

            // Virtual function to handle a GET_RETURN_CODE command. This will return the saved return code. Where and which register should be saved as the return code can be set via SET_RETURN_CODE_ADDRESS. The recording of the return code will be resetted after this call.
            virtual status handle_get_return_code(uint64_t &code) = 0;

            // Virtual function to handle a DO_RUN command. This does one run from a start breakpoint to an endbreakpoint with mmio read queue data. It also records the given register at the end breakpoint and watches an error symbol, if specified.
            virtual status handle_do_run(std::string &start_breakpoint, std::string &end_breakpoint, uint64_t mmio_address, size_t mmio_length, size_t mmio_data_length, char* mmio_data, std::string &register_name) = 0;

            // Virtual function to handle a SET_ERROR_SYMBOL command. This sets a specific symbol that should be watched during the execution. If this symbol is encountered, the simulation is stopped and the event ERROR_SYMBOL_HIT triggered.
            virtual status handle_set_error_symbol(std::string &symbol) = 0;

            // Virtual function to handle a SET_FIXED_READ command. Tthis sets one or multiple fixed read returnes when the MMIO read with the given id event is catched.
            virtual status handle_set_fixed_read(size_t count, char* data) = 0;

            // Virtual function to handle a GET_CPU_PC command, for getting the CPU program counter.
            virtual status handle_get_cpu_pc(uint64_t &pc) = 0;

            // Virtual function to handle a JUMP_CPU_TO command. This lets the CPU jump to a specific address.
            virtual status handle_jump_cpu_to(uint64_t address) = 0;

            // Virtual function to handle a STORE_CPU_REGISTER command. This stores the CPU registers, except the PC.
            virtual status handle_store_cpu_register() = 0;

            // Virtual function to handle RESTORE_CPU_REGISTER command. THis restores the CPU registers, except the PC.
            virtual status handle_restore_cpu_register() = 0;

            // Event queue
            std::deque<event> m_event_queue;

            // Mutexes for synchonization of events.
            sem_t m_full_slots;
            sem_t m_empty_slots;

            // Thread with the receiver loop.
            std::thread m_receiver_thread;

            // Pointer to the communcation object used.
            testing_communication* m_communication;

            // Current active request and response.
            request m_current_req;
            response m_current_res;

            // Array and pointer for code coverage tracking.
            uint8_t m_bb_array [MAP_SIZE];
            uint64_t m_prev_bb_loc = 0;
    };

}


#endif