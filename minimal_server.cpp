// minimal_server.cpp - A minimal HTTP server that doesn't depend on external libraries
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <vector>
#include <memory>
#include <functional>
#include <signal.h>

const int PORT = 8080;
bool keep_running = true;

void signal_handler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    keep_running = false;
}

class MinimalHttpServer {
public:
    MinimalHttpServer(int port) : port_(port), server_fd_(-1) {}

    bool start() {
        // Create socket
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        // Set socket options
        int opt = 1;
        if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
            std::cerr << "Failed to set socket options" << std::endl;
            return false;
        }

        // Set up address structure
        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);

        // Bind socket to port
        if (bind(server_fd_, (struct sockaddr *)&address, sizeof(address)) < 0) {
            std::cerr << "Failed to bind to port " << port_ << std::endl;
            return false;
        }

        // Start listening
        if (listen(server_fd_, 32) < 0) {
            std::cerr << "Failed to listen" << std::endl;
            return false;
        }

        std::cout << "Server is listening on port " << port_ << std::endl;
        return true;
    }

    void run() {
        if (server_fd_ < 0) {
            std::cerr << "Server not initialized. Call start() first." << std::endl;
            return;
        }

        while (keep_running) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_fd = accept(server_fd_, (struct sockaddr *)&client_addr, &client_len);
            if (client_fd < 0) {
                if (keep_running) {
                    std::cerr << "Failed to accept connection" << std::endl;
                }
                continue;
            }

            // Handle the connection in a new thread
            std::thread handler([this, client_fd, client_addr]() {
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                std::cout << "New connection from " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;
                
                handle_client(client_fd);
                close(client_fd);
            });
            handler.detach();
        }

        if (server_fd_ >= 0) {
            close(server_fd_);
            server_fd_ = -1;
        }
    }

    ~MinimalHttpServer() {
        if (server_fd_ >= 0) {
            close(server_fd_);
        }
    }

private:
    void handle_client(int client_fd) {
        char buffer[4096] = {0};
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        
        if (bytes_read <= 0) {
            return;
        }
        
        buffer[bytes_read] = '\0';
        
        // Parse the request line
        std::string request(buffer);
        size_t first_line_end = request.find("\r\n");
        if (first_line_end == std::string::npos) {
            return;
        }
        
        std::string request_line = request.substr(0, first_line_end);
        
        // Extract method, path, version
        size_t method_end = request_line.find(" ");
        if (method_end == std::string::npos) {
            return;
        }
        
        std::string method = request_line.substr(0, method_end);
        
        size_t path_start = method_end + 1;
        size_t path_end = request_line.find(" ", path_start);
        if (path_end == std::string::npos) {
            return;
        }
        
        std::string path = request_line.substr(path_start, path_end - path_start);
        
        std::cout << "Request: " << method << " " << path << std::endl;
        
        // Generate response based on path
        std::string response;
        
        if (path == "/health") {
            response = "HTTP/1.1 200 OK\r\n"
                       "Content-Type: application/json\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                       "Content-Length: 15\r\n"
                       "\r\n"
                       "{\"status\":\"ok\"}";
        } else if (path == "/") {
            std::string html = 
                "<!DOCTYPE html>\n"
                "<html>\n"
                "<head>\n"
                "    <title>LogAI-CPP Minimal Server</title>\n"
                "</head>\n"
                "<body>\n"
                "    <h1>LogAI-CPP Minimal Server</h1>\n"
                "    <p>Server is running!</p>\n"
                "    <p><a href=\"/health\">Health Check</a></p>\n"
                "</body>\n"
                "</html>";
                
            response = "HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/html\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                       "Content-Length: " + std::to_string(html.length()) + "\r\n"
                       "\r\n" + html;
        } else {
            response = "HTTP/1.1 404 Not Found\r\n"
                       "Content-Type: text/plain\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                       "Content-Length: 9\r\n"
                       "\r\n"
                       "Not Found";
        }
        
        // Send response
        write(client_fd, response.c_str(), response.length());
    }

    int port_;
    int server_fd_;
};

int main() {
    try {
        // Set up signal handling
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        std::cout << "Starting minimal HTTP server..." << std::endl;
        
        MinimalHttpServer server(PORT);
        if (!server.start()) {
            std::cerr << "Failed to start server" << std::endl;
            return 1;
        }
        
        server.run();
        
        std::cout << "Server shutdown complete" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}