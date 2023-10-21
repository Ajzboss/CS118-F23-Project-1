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

namespace fs = std::filesystem;

#define BUFFER_SIZE 1024
#define DEFAULT_SERVER_PORT 8081
#define DEFAULT_REMOTE_HOST "131.179.176.34"
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
void proxy_remote_file(struct server_app *app, int client_socket, const char *path);

// Utilities
std::string uri_decode(std::string uri);
std::string current_http_date();

enum filetype {
    ft_html,
    ft_jpg,
    ft_txt,
    ft_other,
};


// TODO: get extension

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
    // TODO: get rid of this
    fprintf(stderr, "%s", request.c_str());
    const size_t n = request.size();

    /*
    * Assumption: we only need to handle GET requests (piazza @71)
    * HTTP GET Request:
    * GET /<PATH> <HTTP version which we don't care about>
    * Headers, which I don't think we actually care about
    */
    size_t i = 0, j = 0;

    // Extract the method
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
    // TODO: uri_decode
    std::string requested_path = request.substr(i, j);

    // serve index.html if root is requested
    if (requested_path == "/") {
        requested_path = "index.html";
    }

    // TODO: Implement proxy and call the function under condition
    // specified in the spec
    // if (need_proxy(...)) {
    //    proxy_remote_file(app, client_socket, file_name);
    // } else {
    serve_local_file(client_socket, requested_path);
    //}
}

void serve_local_file(int client_socket, std::string path) {
    // TODO: Properly implement serving of local files
    // The following code returns a dummy response for all requests
    // but it should give you a rough idea about what a proper response looks like
    // What you need to do 
    // (when the requested file exists):
    // * Open the requested file
    // * Build proper response headers (see details in the spec), and send them
    // * Also send file content
    // (When the requested file does not exist):
    // * Generate a correct response

    // build response using vector
    std::stringstream response_stream;

    // NOTE: per the spec, we are not recursively searching cur dir
    // TODO: per piazza @71, all file paths start with a / anything special to be done here?

    // search for path in current directory
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
            filetype ftype = determine_ftype(path);
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

            // entity body
            response_stream.write(file_buf.data(), file_buf.size());

        } else {
            // File can't be opened, this is an internal server error
            response_stream << "HTTP/1.0 500 Internal Server Error\r\n";
            response_stream << "Date: " << current_http_date() << "\r\n";
            response_stream << "Content-Length: 0\r\n";
            response_stream << "Connection: close\r\n";
            fprintf(stderr, "Requested un-openable file %s\n", path);
        }
    } else {
        response_stream << "HTTP/1.0 404 Not Found\r\n";
        response_stream << "Date: " << current_http_date() << "\r\n";
        response_stream << "Content-Length: 0\r\n";
        response_stream << "Connection: close\r\n";
    }

    // convert to string
    std::string response = response_stream.str();
    size_t size = response.size();

    send(client_socket, response.c_str(), size, 0);
}

void proxy_remote_file(struct server_app *app, int client_socket, const char *request) {
    // TODO: Implement proxy request and replace the following code
    // What's needed:
    // * Connect to remote server (app->remote_server/app->remote_port)
    // * Forward the original request to the remote server
    // * Pass the response from remote server back
    // Bonus:
    // * When connection to the remote server fail, properly generate
    // HTTP 502 "Bad Gateway" response

    char response[] = "HTTP/1.0 501 Not Implemented\r\n\r\n";
    send(client_socket, response, strlen(response), 0);
}

/* Utilities */

std::string uri_decode(std::string uri) {
    std::string output = "";

    size_t i = 0;
    size_t j = 0;
    size_t n = uri.size();

    while (i < n && j < n) {
        // just append when you don't see a URI encoding
        if (uri[i] != '%') {
            output.push_back(uri[i]);
            i++;
        }
        
        j = ++i;
        while (j < n && isdigit(uri[j])) {
            j++;
        }

        // append encoding converted to char to output 
        output.push_back('\0' + std::stoi(uri.substr(i, j)));
        i = j;
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

filetype determine_ftype(std::string path) {
    int j = path.size();
    int i = j - 1;

    // get first '.'
    while (i >= 0 && path[i] != '.') {
        i--;
    }
    std::string ext = path.substr(i + 1, j);

    if (ext == "html" || ext == "htm") {
        return ft_html;
    } else if (ext == "jpg" || ext == "jpeg") {
        return ft_jpg;
    } else if (ext == "txt") {
        return ft_txt;
    }

    return ft_other;
}
