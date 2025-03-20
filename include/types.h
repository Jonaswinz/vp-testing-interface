#ifndef TESTING_TYPES_H
#define TESTING_TYPES_H

#define MQ_MAX_LENGTH 256
#define MQ_MAX_MSG 10

#define PIPE_READ_ERROR_MAX 5

namespace testing{

    // Types of interface that exists.
    enum communication{
        MQ, PIPE, COMMUNICATION_COUNT
    };

    // Possible commands.
    enum command{
        CONTINUE, KILL, SET_BREAKPOINT, REMOVE_BREAKPOINT, ENABLE_MMIO_TRACKING, DISABLE_MMIO_TRACKING, SET_MMIO_VALUE, ADD_TO_MMIO_READ_QUEUE, SET_CPU_INTERRUPT_TRIGGER, ENABLE_CODE_COVERAGE, DISABLE_CODE_COVERAGE, GET_CODE_COVERAGE, GET_CODE_COVERAGE_SHM, RESET_CODE_COVERAGE, SET_RETURN_CODE_ADDRESS, GET_RETURN_CODE, DO_RUN, DO_RUN_SHM, SET_ERROR_SYMBOL, SET_FIXED_READ, GET_CPU_PC, JUMP_CPU_TO, STORE_CPU_REGISTERS, RESTORE_CPU_REGISTERS 
    };

    // Possible return status codes.
    enum status{
        STATUS_OK, STATUS_ERROR, STATUS_MALFORMED
    };

    // Possible events that the simulation can produce.
    enum event_type{
        MMIO_READ, MMIO_WRITE, VP_END, BREAKPOINT_HIT, ERROR_SYMBOL_HIT
    };

    struct event{
        event_type event;
        char* addition_data = nullptr;
        uint32_t additional_data_length = 0;
    };
    
    // Represents a request send to the implemented testing interface with a command ID and flexible length data.
    struct request{
        command request_command;
        char* data = nullptr;
        uint32_t data_length = 0;
    };

    // Represents a response of the testing interface, which contains of an response status and the response data.
    struct response{
        status response_status;
        char* data = nullptr;
        uint32_t data_length = 0;
    };
}

#endif