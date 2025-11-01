#pragma once
#include <string>
#include <future>
#include <memory>

namespace openlawsvpn {

class SAMLCapture {
public:
    SAMLCapture(int port = 35001);
    ~SAMLCapture();

    // Starts the server and returns a future that will contain the captured SAML response
    std::future<std::string> start();

    void stop();

private:
    int port_;
    bool running_;
    // Implementation details would go here (using asio)
};

} // namespace openlawsvpn
