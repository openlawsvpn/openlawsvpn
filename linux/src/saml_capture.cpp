// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 openlawsvpn contributors
// See LICENSE and LICENSE_USAGE_EXCEPTION for terms.
#include "saml_capture.h"
#include <iostream>
#include <thread>
#include <sstream>
#include <asio.hpp>
#include <algorithm>

using asio::ip::tcp;

namespace openlawsvpn {

class SAMLServer {
public:
    SAMLServer(int port, std::promise<std::string>& promise)
        : port_(port), promise_(promise), io_context_(), acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)) {}

    void run() {
        try {
            tcp::socket socket(io_context_);
            acceptor_.accept(socket);

            asio::streambuf sb;
            asio::read_until(socket, sb, "\r\n\r\n");
            
            std::string header_data;
            std::istream is(&sb);
            std::string line;
            size_t content_length = 0;
            while (std::getline(is, line) && line != "\r") {
                if (line.compare(0, 16, "Content-Length: ") == 0) {
                    content_length = std::stoul(line.substr(16));
                }
            }

            std::string body;
            if (content_length > 0) {
                size_t remaining = content_length;
                if (sb.size() > 0) {
                    char buf[sb.size()];
                    is.read(buf, sb.size());
                    body.append(buf, is.gcount());
                    remaining -= is.gcount();
                }
                if (remaining > 0) {
                    std::vector<char> buf(remaining);
                    asio::read(socket, asio::buffer(buf));
                    body.append(buf.data(), buf.size());
                }
            } else {
                // Fallback for GET or small POST already in buffer
                char buf[sb.size()];
                is.read(buf, sb.size());
                body.append(buf, is.gcount());
            }

            auto url_decode = [](const std::string& str) {
                std::string res;
                for (size_t i = 0; i < str.length(); ++i) {
                    if (str[i] == '+') res += ' ';
                    else if (str[i] == '%' && i + 2 < str.length()) {
                        int value;
                        std::stringstream ss;
                        ss << std::hex << str.substr(i + 1, 2);
                        if (ss >> value) {
                            res += (char)value;
                        } else {
                            res += str.substr(i, 3);
                        }
                        i += 2;
                    } else res += str[i];
                }
                return res;
            };

            auto normalize_base64 = [](std::string str) {
                std::string res;
                res.reserve(str.length());
                for (char c : str) {
                    if (c == ' ') {
                        res += '+'; // Common URL-decoding artifact for Base64 '+'
                    } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                               (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '-' || c == '_') {
                        res += c;
                    }
                    // Strip all other characters including newlines, null bytes, etc.
                }
                // Handle URL-safe base64
                std::replace(res.begin(), res.end(), '-', '+');
                std::replace(res.begin(), res.end(), '_', '/');
                
                // Fix padding
                while (res.length() % 4 != 0) {
                    res += '=';
                }
                return res;
            };

            size_t pos = body.find("SAMLResponse=");
            if (pos != std::string::npos) {
                size_t start = pos + 13;
                size_t end = body.find('&', start);
                std::string token;
                if (end == std::string::npos) {
                    token = body.substr(start);
                } else {
                    token = body.substr(start, end - start);
                }
                token = url_decode(token);
                token = normalize_base64(token);

                // Respond to browser
                std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                                       "<html><body><h1>Login successful!</h1><p>You can close this window now.</p></body></html>";
                asio::write(socket, asio::buffer(response));
                
                promise_.set_value(token);
                return;
            }
            promise_.set_exception(std::make_exception_ptr(std::runtime_error("Failed to capture SAML response: SAMLResponse key not found")));
        } catch (std::exception& e) {
            promise_.set_exception(std::current_exception());
        }
    }

private:
    int port_;
    std::promise<std::string>& promise_;
    asio::io_context io_context_;
    tcp::acceptor acceptor_;
};

SAMLCapture::SAMLCapture(int port) : port_(port), running_(false) {}
SAMLCapture::~SAMLCapture() { stop(); }

std::future<std::string> SAMLCapture::start() {
    auto promise = std::make_shared<std::promise<std::string>>();
    std::thread([this, promise]() {
        SAMLServer server(port_, *promise);
        server.run();
    }).detach();
    return promise->get_future();
}

void SAMLCapture::stop() {
    // Basic implementation: let it finish or just exit
}

} // namespace openlawsvpn
