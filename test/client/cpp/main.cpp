#include "testing_client.h"

int main() {
    std::cout << "Testing client for vp-testing-interface!" << std::endl;

    testing::mq_testing_client client = testing::mq_testing_client("/test-request", "/test-response");

    client.start();

    client.wait_for_ready();

    testing::request req = testing::request();
    req.cmd = testing::CONTINUE;

    testing::response res = testing::response();

    client.send_request(&req, &res);

    return 0;
}