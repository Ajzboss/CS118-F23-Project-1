/*
* CS 118 Fall 2023 Project 1
* Aditya Gomatam and Aryan Janolkar
*/

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>
#include <regex>
#include <iostream>
#include <arpa/inet.h>
namespace fs = std::filesystem;

#define BUFFER_SIZE 1024
#define DEFAULT_SERVER_PORT 8081
#define DEFAULT_REMOTE_HOST "233.179.176.35"
#define DEFAULT_REMOTE_PORT 5001

struct server_app {
    // Parameters of the server
    // Local port of HTTP server
    uint16_t server_port;

    // Remote host and port of remote proxy
    char *remote_host;
    uint16_t remote_port;
};

// The following function is implemented for you and doesn't need
// to be change
void parse_args(int argc, char *argv[], struct server_app *app);

// server functions
void handle_request(struct server_app *app, int client_socket);
void serve_local_file(int client_socket, std::string path);
void proxy_remote_file(struct server_app *app, int client_socket, const std::string request);

// Utilities
std::string uri_decode(std::string uri);
std::string current_http_date();

enum filetype {
    ft_html,
    ft_jpg,
    ft_txt,
    ft_other,
};

filetype determine_filetype(std::string path);

/* Main function is provided */
int main(int argc, char *argv[]) {
    struct server_app app;
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int ret;

    parse_args(argc, argv, &app);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(app.server_port);

    // The following allows the program to immediately bind to the port in case
    // previous run exits recently
    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", app.server_port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("accept failed");
            continue;
        }
        
        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        handle_request(&app, client_socket);
        close(client_socket);
    }

    close(server_socket);
    return 0;
}

void parse_args(int argc, char *argv[], struct server_app *app) {
    int opt;

    app->server_port = DEFAULT_SERVER_PORT;
    app->remote_host = NULL;
    app->remote_port = DEFAULT_REMOTE_PORT;

    while ((opt = getopt(argc, argv, "b:r:p:")) != -1) {
        switch (opt) {
        case 'b':
            app->server_port = atoi(optarg);
            break;
        case 'r':
            app->remote_host = strdup(optarg);
            break;
        case 'p':
            app->remote_port = atoi(optarg);
            break;
        default: /* Unrecognized parameter or "-?" */
            fprintf(stderr, "Usage: server [-b local_port] [-r remote_host] [-p remote_port]\n");
            exit(-1);
            break;
        }
    }

    if (app->remote_host == NULL) {
        app->remote_host = strdup(DEFAULT_REMOTE_HOST);
    }
}

/* Server functions */

void handle_request(struct server_app *app, int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Read the request from HTTP client
    // Note: This code is not ideal in the real world because it
    // assumes that the request header is small enough and can be read
    // once as a whole.
    // However, the current version suffices for our testing.
    bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        // Connection closed or error
        return;
    }

    buffer[bytes_read] = '\0';
    // copy buffer to a new string
    // char *request_cstr = (char *)malloc(strlen(buffer) + 1);
    // strcpy(request_cstr, buffer);
    std::string request(buffer, strlen(buffer) + 1);
    const size_t n = request.size();

    /*
    * Assumption: we only need to handle GET requests (piazza @71)
    * HTTP GET Request:
    * GET /<PATH> <HTTP version which we don't care about>
    * Headers, which I don't think we actually care about
    */
    size_t i = 0, j = 0;

    // Extract the method
    // std::cout << request << std::endl;;
    while (j < n && request[j] != ' ' && request[j] != '\r' && request[j] != '\n') {
        j++;
    }
    std::string method = request.substr(i, j - i);

    if (method != "GET") {
        // ignore non-GET requests
        return;
    }

    // Extract the requested path
    i = ++j;
    while (j < n && request[j] != ' ' && request[j] != '\r' && request[j] != '\n') {
        j++;
    }
    std::string requested_path = uri_decode(request.substr(i, j - i));

    // serve index.html if root is requested
    if (requested_path == "/") {
        requested_path = "/index.html";
    }
    //std::cout<<"requested_path";
    //std::cout<<requested_path;
    // TODO: Implement proxy and call the function under condition
    // specified in the spec
    if (requested_path.length() >= 3) {
        if (requested_path.substr(requested_path.length() - 3, 3) == ".ts") {
            proxy_remote_file(app, client_socket, request);
        } else {
            serve_local_file(client_socket, requested_path);
        }
    } else {
        serve_local_file(client_socket, requested_path);
    }
}
void serve_local_file(int client_socket, std::string path) {
    // build response using a stream
    std::stringstream response_stream;

    // Per piazza @71, all file paths start with a /
    // so remove the first character
    path = path.substr(1);

    // search for path in current directory
    // NOTE: per the spec, we are not recursively searching cur dir
    const fs::path cur_dir = fs::current_path();
    bool file_found = false;

    for (const auto& entry : fs::directory_iterator(cur_dir)) {
        if (entry.is_regular_file() && entry.path().filename() == path) {
            file_found = true;
            break;
        }
    }

    if (file_found) {
        std::ifstream file(path);

        if (file.is_open()) {
            // read the file as bytes
            std::vector<char> file_buf;
            char cur_byte;

            while (file.get(cur_byte)) {
                file_buf.push_back(cur_byte);
            }

            size_t content_length = file_buf.size();

            // extract content type
            // reference: https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            filetype ftype = determine_filetype(path);
            std::string content_type;
            switch (ftype) {
                case ft_html:
                    content_type = "text/html; charset=UTF-8";
                    break;
                case ft_jpg:
                    content_type = "image/jpeg";
                    break;
                case ft_txt:
                    content_type = "text/plain; charset=UTF-8";
                    break;
                default:
                    content_type = "application/octet-stream";
            }

            response_stream << "HTTP/1.0 200 OK\r\n";
            response_stream << "Date: " << current_http_date() << "\r\n";
            response_stream << "Connection: close\r\n";
            response_stream << "Content-Type: " << content_type << "\r\n";
            response_stream << "Content-Length: " << content_length << "\r\n";
            // CRLF before entity body
            response_stream << "\r\n";

            // entity body: we have to use .write() as if file_buf contains a null
            // byte, it would just terminate the stream; .write() avoids this
            response_stream.write(file_buf.data(), file_buf.size());

        } else {
            // File can't be opened, this is an internal server error
            response_stream << "HTTP/1.0 500 Internal Server Error\r\n";
            response_stream << "Date: " << current_http_date() << "\r\n";
            response_stream << "Content-Length: 0\r\n";
            response_stream << "Connection: close\r\n";
            fprintf(stderr, "Requested un-openable file %s\n", path.c_str());
        }
    } else {
        response_stream << "HTTP/1.0 404 Not Found\r\n";
        response_stream << "Date: " << current_http_date() << "\r\n";
        response_stream << "Content-Length: 0\r\n";
        response_stream << "Connection: close\r\n";
        fprintf(stderr, "Requested nonexistent file %s\n", path.c_str());
    }

    // convert to string
    std::string response = response_stream.str();
    size_t size = response.size();

    send(client_socket, response.c_str(), size, 0);
}

void proxy_remote_file(struct server_app *app, int client_socket, const std::string request) {
    // TODO: Implement proxy request and replace the following code
    // What's needed:
    // * Connect to remote server (app->remote_server/app->remote_port)
    // * Forward the original request to the remote server
    // * Pass the response from remote server back
    // Bonus:
    // * When connection to the remote server fail, properly generate
    // HTTP 502 "Bad Gateway" response
    //std::cout<<"WBHFDOQHGFD)QO)\n";
    int remote_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (remote_socket == -1) {
        perror("socket failed");
        exit(1); // TODO: exit_failure?
    }

    struct sockaddr_in remote_server_addr;

    remote_server_addr.sin_family = AF_INET;
    inet_aton(app->remote_host, &remote_server_addr.sin_addr);
    remote_server_addr.sin_port = htons(app->remote_port);

    if(connect(remote_socket, (struct sockaddr*) &remote_server_addr, sizeof(remote_server_addr)) < 0) {
        std::stringstream response_stream;
        std::cout<<"Fail to connect";
        response_stream << "HTTP/1.0 502 Bad Gateway\r\n";
        response_stream << "Date: " << current_http_date() << "\r\n";
        response_stream << "Content-Length: 0\r\n";
        response_stream << "Connection: close\r\n";

        std::string response = response_stream.str();

        send(client_socket, response.c_str(), response.size(), 0);

        perror("connect");
        close(remote_socket);
        return;
    }
    
    if(send(remote_socket, request.c_str(), request.size(), 0)< 0){
        perror("forward request");
        close(remote_socket);
        exit (1);
    }

    int bytes_received;
    char response_buffer[BUFFER_SIZE];

    do {
        bzero(response_buffer, BUFFER_SIZE);
        bytes_received = recv(remote_socket, response_buffer, BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            perror("bad recv");
        } else if (bytes_received > 0) {
            int m = send(client_socket, response_buffer, bytes_received, 0);
            if (m < 0) {
                perror("bad send");
            }
        }

    } while (bytes_received > 0);

    if(close(remote_socket)<0){
        fprintf(stderr, "close\n");
        exit(1);
    }
}

/* Utilities */

// Adapted from: https://gist.github.com/arthurafarias/56fec2cd49a32f374c02d1df2b6c350f
const std::regex PERCENT_ENCODING("%[0-9A-F]{2}");

std::string uri_decode(std::string uri) {
    std::string output = "";

    size_t n = uri.size();

    if (n < 3) {
        return uri;
    }

    // whether or not this loop instance translated something
    bool translated = false;
    for (int i = 0; i < n - 2; i++) {
        translated = false;
        std::string next_3_chars = uri.substr(i, 3);
        if (std::regex_match(next_3_chars, PERCENT_ENCODING)) {
            // replace % symbol with 0x to convert from hex to int
            next_3_chars.replace(0, 1, "0x");
            char next = static_cast<char>(std::stoi(next_3_chars, nullptr, 16));
            output.push_back(next);
            i += 2;
            translated = true;
        } else {
            output.push_back(uri[i]);
        }
    }

    if (!translated) {
        output += uri.substr(n - 2);
    }

    return output;
}

// Wrote this function with the help of ChatGPT
std::string current_http_date() {
    // UTC current time
    auto now = std::chrono::system_clock::now();
    std::time_t current_time = std::chrono::system_clock::to_time_t(now);

    // Convert to struct tm
    struct std::tm* time_info = std::gmtime(&current_time);

    // Format according to HTTP date format
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", time_info);

    return std::string(buffer);
}

filetype determine_filetype(std::string path) {
    int i = path.size() - 1;

    // get first '.' from the end to determine file extension
    while (i >= 0 && path[i] != '.') {
        i--;
    }
    std::string ext = path.substr(i + 1);

    if (ext == "html" || ext == "htm") {
        return ft_html;
    } else if (ext == "jpg" || ext == "jpeg") {
        return ft_jpg;
    } else if (ext == "txt") {
        return ft_txt;
    }

    return ft_other;
}
