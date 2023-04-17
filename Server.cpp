#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 8192

void log_message(char *message, char *log_file_path)
{
    FILE *log_file = fopen(log_file_path, "a");
    if (log_file == nullptr)
    {
        perror("Could not open log file");
        exit(EXIT_FAILURE);
    }
    fprintf(log_file, "%s\n", message);
    fclose(log_file);
}

void send_response(int socket_fd, int response_status_code, const char *response_reason_phrase, const char *response_content_type, const char *response_body)
{
    char buffer[BUFFER_SIZE] = {0};
    snprintf(buffer, sizeof(buffer), "HTTP/1.1 %d %s\nContent-Type: %s\n\n%s", response_status_code, response_reason_phrase, response_content_type, response_body);
    send(socket_fd, buffer, strlen(buffer), 0);
    std::cout << "Response sent:\n"
              << buffer << std::endl;
}

int main(int argc, char const *argv[])
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <HTTP PORT> <Log File> <DocumentRootFolder>" << std::endl;
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    char *log_file_path = (char *)argv[2];
    char *document_root_folder = (char *)argv[3];

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Create a socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to a port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0)
        {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }

        char buffer[BUFFER_SIZE];
        ssize_t bytes_read = read(new_socket, buffer, BUFFER_SIZE);
        if (bytes_read < 0)
        {
            perror("read failed");
            close(new_socket);
            continue;
        }
        buffer[bytes_read] = '\0';

        char *method = strtok(buffer, " ");
        char *resource_path = strtok(NULL, " ");
        char *full_resource_path = (char *)malloc(strlen(document_root_folder) + strlen(resource_path) + 1);
        strcpy(full_resource_path, document_root_folder);
        strcat(full_resource_path, resource_path);

        int response_status_code = 200;
        const char *response_reason_phrase = "OK";
        const char *response_content_type = "text/html";
        const char *response_body = "";

        if (strcmp(method, "GET") == 0)
        {
            FILE *resource_file = fopen(full_resource_path, "r");
            if (resource_file == NULL)
            {
                response_status_code = 404;
                response_reason_phrase = "Not Found";
                response_body = "<html><body><h1>404 Not Found</h1></body></html>";
            }
            else
            {
                char file_buffer[BUFFER_SIZE];
                size_t file_size = fread(file_buffer, 1, BUFFER_SIZE, resource_file);
                fclose(resource_file);
                if (file_size == 0)
                {
                    response_status_code = 500;
                    response_reason_phrase = "Internal Server Error";
                    response_body = "<html><body><h1>500 Internal Server Error</h1></body></html>";
                }
                else
                {
                    response_body = file_buffer;
                }
            }
        }
        else if (strcmp(method, "HEAD") == 0)
        {
            FILE *resource_file = fopen(full_resource_path, "r");
            if (resource_file == NULL)
            {
                response_status_code = 404;
                response_reason_phrase = "Not Found";
            }
            else
            {
                fclose(resource_file);
            }
        }
        else if (strcmp(method, "POST") == 0)
        {
            int content_length = 0;
            char *content_length_header = strstr(buffer, "Content-Length:");
            if (content_length_header != NULL)
            {
                content_length = atoi(content_length_header + strlen("Content-Length:"));
            }

            char *body = (char *)malloc(content_length + 1);
            int body_len = 0;
            while (body_len < content_length)
            {
                ssize_t n = read(new_socket, body + body_len, content_length - body_len);
                if (n < 0)
                {
                    perror("read failed");
                    close(new_socket);
                    free(body);
                    continue;
                }
                body_len += n;
            }
            body[content_length] = '\0';

            // Process the request body, in this case we simply echo it back to the client
            response_body = body;
            free(body);
        }
        else
        {
            response_status_code = 400;
            response_reason_phrase = "Bad Request";
            response_body = "<html><body><h1>400 Bad Request</h1></body></html>";
        }

        send_response(new_socket, response_status_code, response_reason_phrase, response_content_type, response_body);
        close(new_socket);
    }
}