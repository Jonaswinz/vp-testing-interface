#include "testing_client.h"

#include <cstdarg>

void info_logging(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::cout << "[Client INFO]: ";
    vprintf(fmt, args);
    std::cout << std::endl;
    va_end(args); 
}

void error_logging(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::cout << "[Client ERROR]: ";
    vprintf(fmt, args);
    std::cout << std::endl;
    va_end(args); 
}

int main() {
    std::cout << "Testing client for vp-testing-interface!" << std::endl;

    //testing::mq_testing_client client = testing::mq_testing_client("/test-request", "/test-response");
    testing::pipe_testing_client client = testing::pipe_testing_client(10, 11);

    client.log_error_message = error_logging;
    client.log_info_message = info_logging;
    client.start();

    // This client needs to start the VP process in order to allow pipe communication. This is not needed with message queues (there both process can be started seperatly).
    std::cout << "Starting VP child process." << std::endl;
    pid_t pid = fork();
    if (pid == 0) { // Child process
        execl("../../../implementation/build/test", NULL);
        exit(127); // only if exec fails

    } else if (pid < 0) {
        std::cout << "Failed to start VP process!" << std::endl;
        exit(1);
    }
    std::cout << "Started VP child process." << std::endl;

    client.wait_for_ready();

    testing::request req = testing::request();
    req.request_command = testing::CONTINUE;
    req.data = nullptr;
    req.data_length = 0;

    testing::response res = testing::response();

    client.send_request(&req, &res);

    // Important!
    free(req.data);

    return 0;
}