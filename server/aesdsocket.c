#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 1024*1024
#define FILENAME "/var/tmp/aesdsocketdata"

int is_app_running = 1;

struct fd_t {
    int socket;
    int accept;
    FILE* file;
};

struct fd_t create_fds() {
    struct fd_t rv = {
        .socket = -1,
        .accept = -1,
        .file = NULL,
    };
    return rv;
}

struct buffer_t {
    char* data;
    int capacity;
    int bytes_received;
    int offset;
    char client_name[256];
};

struct buffer_t create_buffer() {
    struct buffer_t rv = {
        .data = malloc(BUFFER_SIZE),
        .capacity = BUFFER_SIZE,
        .bytes_received = 0,
        .offset = 0
    };
    return rv;
}

void cleanup(struct fd_t* fd, struct buffer_t* buffer)
{
    if (fd->socket != -1) {
        close(fd->socket);
    }
    if (fd->accept != -1) {
        close(fd->accept);
    }
    if (fd->file) {
        fclose(fd->file);
    }
    if (buffer->data) {
        free(buffer->data);
    }
    remove(FILENAME);
    closelog();
}

void close_app_interrupted(struct fd_t* fd, struct buffer_t* buffer)
{
    syslog(LOG_NOTICE, "Caught signal, exiting\n");
    cleanup(fd, buffer);
    exit(EXIT_FAILURE);
}

void send_back(struct fd_t* fd, struct buffer_t* buffer)
{
    int file = open(FILENAME, O_RDONLY);
    if (file == -1) {
        syslog(LOG_ERR, "Failed to open file %s. %m.\n", FILENAME);
        cleanup(fd, buffer);
        exit(EXIT_FAILURE);
    }

    char read_buffer[4096];

    while (1) {
        memset(read_buffer, 0, 4096);
        ssize_t bytes_read = read(file, read_buffer, sizeof(read_buffer));

        if (bytes_read == 0) {
            break;
        }

        if (bytes_read == -1) {
            syslog(LOG_ERR, "Failed to read from file %s. %m.\n", FILENAME);
            close(file);
            cleanup(fd, buffer);
            exit(EXIT_FAILURE);
        }

        char* out = read_buffer;

        do {
            ssize_t bytes_written = send(fd->accept, out, bytes_read, 0);

            if (bytes_written >= 0) {
                bytes_read -= bytes_written;
                out += bytes_written;
            } else if (errno != EINTR) {
                syslog(LOG_ERR, "Failed to write to socket. %m.\n");
                close(file);
                cleanup(fd, buffer);
                exit(EXIT_FAILURE);
            }
        } while (bytes_read > 0);
    }
    close(file);
}

void handle_signal(int signum)
{
    is_app_running = 0;
}

struct sockaddr get_addr(struct fd_t* fd, struct buffer_t* buffer)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints)); 
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *res;

    int status = getaddrinfo(NULL, "9000", &hints, &res);
    if (status != 0) {
        syslog(LOG_ERR, "getaddrinfo fauled. %s. %m.\n",  gai_strerror(status));
        cleanup(fd, buffer);
        exit(EXIT_FAILURE);
    }

    struct sockaddr addr;
    memcpy(&addr, res->ai_addr, sizeof(struct sockaddr));
    freeaddrinfo(res);
    return addr;
}

void write_to_file(char* src, int count, struct fd_t* fd, struct buffer_t* buffer)
{
    if (!fd->file) {
        fd->file = fopen(FILENAME, "w");
        if (fd->file == NULL) {
            syslog(LOG_ERR, "Failed to open file %s. %m.\n", FILENAME);
            cleanup(fd, buffer);
            exit(EXIT_FAILURE);
        }
    }

    size_t bytes_written = fwrite(src, 1, count, fd->file);
    if (bytes_written != count) {
        syslog(LOG_ERR, "Failed to write to file %s. %m.\n", FILENAME);
        cleanup(fd, buffer);
        exit(EXIT_FAILURE);
    }
    fflush(fd->file);
}

void process_data(struct buffer_t* buffer, struct fd_t* fd)
{
    char* begin = buffer->data;
    char* end;
    int remaining = buffer->offset + buffer->bytes_received;

    while (end = memchr(begin, '\n', remaining)) {
        int packet_length = end - begin + 1;
        write_to_file(begin, packet_length, fd, buffer);
        send_back(fd, buffer);
        begin += packet_length;
        remaining -= packet_length;
    }

    buffer->offset = 0;

    if (remaining == buffer->bytes_received) {
        return;
    }
    if (remaining) {
        memmove(buffer->data, begin, remaining);
        buffer->offset = remaining;
    }
}

void receive_data(struct fd_t* fd, struct buffer_t* buffer)
{
    // Accept connection
    {
        struct sockaddr client;
        socklen_t client_size = sizeof(client);

        fd->accept = accept(fd->socket, &client, &client_size);

        if (!is_app_running) {
            close_app_interrupted(fd, buffer);
        }

        if (fd->accept == -1) {
            syslog(LOG_ERR, "Failed to accept connection. %m.\n");
            cleanup(fd, buffer);
            exit(EXIT_FAILURE);
        }

        char port[16];
        getnameinfo(&client, client_size, buffer->client_name, sizeof(buffer->client_name), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);

        syslog(LOG_NOTICE, "Accepted connection from %s\n", buffer->client_name);
    }

    // Receive data
    while (is_app_running) {
        ssize_t bytes_received = recv(fd->accept, buffer->data + buffer->offset, buffer->capacity - buffer->offset, 0);

        if (!is_app_running) {
            close_app_interrupted(fd, buffer);
        }
    
        if (bytes_received == -1) {
            syslog(LOG_ERR, "recv failed. %m.\n");
            exit(EXIT_FAILURE);
        }
        if (bytes_received == 0) {
            syslog(LOG_NOTICE, "Closed connection from %s\n", buffer->client_name);
            break;
        }
        buffer->bytes_received = bytes_received;
        process_data(buffer, fd);
    }
}

int main(int argc, char* argv[])
{
    openlog(NULL, 0, LOG_USER);

    int should_daemonize = 0;

    if (argc == 2) {
        if ((strlen(argv[1]) != 2) || (argv[1][0] != '-') || (argv[1][1] != 'd')) {
            printf("Usage: %s [-d]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
        should_daemonize = 1;
    }
    else if (argc > 2) {
        printf("Usage: %s [-d]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct fd_t fd = create_fds();
    struct buffer_t buffer = create_buffer();

    // Install signal handler
    {
        int status;
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handle_signal;
        sigemptyset(&sa.sa_mask);

        status = sigaction(SIGINT, &sa, NULL);
        if (status == -1) {
            syslog(LOG_ERR, "Failed to register signal. %m.\n");
            exit(EXIT_FAILURE);
        }
    
        status = sigaction(SIGTERM, &sa, NULL);
        if (status == -1) {
            syslog(LOG_ERR, "Failed to register signal. %m.\n");
            exit(EXIT_FAILURE);
        }
    }

    // Create socket
    {
        fd.socket = socket(AF_INET, SOCK_STREAM, 0);
        if (fd.socket == -1) {
            syslog(LOG_ERR, "Failed to open socket. %m.\n");
            exit(EXIT_FAILURE);
        }
    }

    // Set socket option to SO_REUSEADDR
    {
        int enable = 1;
        int status = setsockopt(fd.socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
        if (status == -1) {
            syslog(LOG_ERR, "Failed to set socket option. %m.\n");
            cleanup(&fd, &buffer);
            exit(EXIT_FAILURE);
        }
        status = setsockopt(fd.socket, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable));
        if (status == -1) {
            syslog(LOG_ERR, "Failed to set socket option. %m.\n");
            cleanup(&fd, &buffer);
            exit(EXIT_FAILURE);
        }
    } 

    // Bind socket to address
    {
        struct sockaddr addr = get_addr(&fd, &buffer);
        int status = bind(fd.socket, &addr, sizeof(struct sockaddr));
        if (status == -1) {
            syslog(LOG_ERR, "Failed to bind socket to address. %m.\n");
            cleanup(&fd, &buffer);
            exit(EXIT_FAILURE);
        }
    }

    if (should_daemonize) {
        int status = daemon(0, 0);
        if (status == -1) {
            syslog(LOG_ERR, "Failed to daemonize process. %m.\n");
            cleanup(&fd, &buffer);
            exit(EXIT_FAILURE);
        }
    }

    // Listen on socket
    {
        int status = listen(fd.socket, 10);
        if (status == -1) {
            syslog(LOG_ERR, "Failed to listen on socket. %m.\n");
            cleanup(&fd, &buffer);
            exit(EXIT_FAILURE);
        }
    }

    // Receiver loop
    while (is_app_running) {
        receive_data(&fd, &buffer);
        close(fd.accept);
        fd.accept = -1;
    }

    cleanup(&fd, &buffer);

    return EXIT_SUCCESS;
}
