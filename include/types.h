#ifndef TESTING_TYPES_H
#define TESTING_TYPES_H

namespace testing{

     // Types of interface that exists.
     enum communication{
        MQ, PIPE, COMMUNICATION_COUNT
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
}

#endif