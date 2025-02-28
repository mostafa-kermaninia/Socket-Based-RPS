#include <stdio.h>
#include <arpa/inet.h>
#include <cstdlib> // General utilities: atoi, exit, etc.
#include <cstring>
#include <poll.h>
#include <string>
#include <iostream>
#include <csignal> // Signal handling
#include "magic-value-client.hpp"

using namespace std;

char chosen[2];                                     // Buffer to store the player's choice
int server_file_description, room_file_description; // File descriptors for server and room connections

void create_broadcast_socket(sockaddr_in &server_addr, char *ipaddr, int &server_broadcast_fd, sockaddr_in &server_broadcast_addr)
{
    int broadcast = 1, opt = 1;
    server_addr.sin_family = AF_INET; // IPv4
    if (inet_pton(AF_INET, ipaddr, &(server_addr.sin_addr)) == -1)
        perror("FAILED: Input ipv4 address invalid");

    // Create a UDP socket for receiving broadcast messages from the server
    if ((server_broadcast_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        perror("FAILED: Socket was not created");
    setsockopt(server_broadcast_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)); // Allow broadcast
    setsockopt(server_broadcast_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));             // Reuse port

    memset(&server_broadcast_addr, 0, sizeof(server_broadcast_addr)); // Zero out the struct
    server_broadcast_addr.sin_family = AF_INET;                       // IPv4
    server_broadcast_addr.sin_port = htons(BROADCAST_PORT);           // Set broadcast port
    server_broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);  // Set broadcast IP address
    // Bind the broadcast socket
    if (bind(server_broadcast_fd, (struct sockaddr *)&server_broadcast_addr, sizeof(server_broadcast_addr)) < 0)
        perror("FAILED: binding failed");
}

void create_TCP_socket(int &server_fd)
{
    int opt = 1;
    // Create a TCP socket for connecting to the server
    if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        perror("FAILED: Socket was not created");

    // Set socket options to reuse address and port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        perror("FAILED: Making socket reusable failed");
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1)
        perror("FAILED: Making socket reusable port failed");
}

void server_TCP_handler(int &server_fd, char *buffer, int &port, char *&ipaddr, sockaddr_in &server_addr, int &room_fd, sockaddr_in &room_port, pollfd &pollf)
{
    int size_of_server_msg = recv(server_fd, buffer, BUFFER_SIZE, 0); // Receive server's message
    string str_buffer(buffer);
    // cout << "server said" << buffer << endl;
    if (size_of_server_msg > 0)
    {
        if (str_buffer.find(ROOMS_STATUS) != string::npos)
        {
            write(STDOUT, buffer, BUFFER_SIZE); // Display room status
            char buf[BUFFER_SIZE];
            memset(buf, 0, BUFFER_SIZE);
            int size = read(STDIN, buf, BUFFER_SIZE); // Read room number from standard input
            send(server_fd, buf, BUFFER_SIZE, 0);     // Send room number to the server
        }
        else if (str_buffer.substr(0, ROOM_FULL.length()) == ROOM_FULL)
        {                                       // Selected room is full
            write(STDOUT, buffer, BUFFER_SIZE); // Inform user and prompt to choose another room
            char buf[BUFFER_SIZE];
            memset(buf, 0, BUFFER_SIZE);
            int size = read(STDIN, buf, BUFFER_SIZE); // Read room number from standard input
            send(server_fd, buf, BUFFER_SIZE, 0);     // Send room number to the server
        }
        else if (str_buffer.substr(0, ROOM_CONNECT.length()) == ROOM_CONNECT) // Confirmation of room connection
        {
            port = stoi(str_buffer.substr(ROOM_CONNECT.length(), str_buffer.length())); // Extract room's port number
            // Prepare to connect to the room
            if (inet_pton(AF_INET, ipaddr, &(server_addr.sin_addr)) == -1)
                perror("FAILED: Input ipv4 address invalid");
            if ((room_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
                perror("FAILED: Socket was not created");
            room_port.sin_addr.s_addr = inet_addr("127.0.0.1");
            room_port.sin_family = AF_INET;
            room_port.sin_port = htons(port);
            if (connect(room_fd, (sockaddr *)(&room_port), sizeof(room_port)))
                perror("FAILED: Connect");

            pollf.fd = room_fd;              // Update polling structure with room connection
            room_file_description = room_fd; // Store room file descriptor globally
        }
        else if (str_buffer.substr(0, ROOM_RESETED.length()) == ROOM_RESETED)
        {                  // Room has been reset
            pollf.fd = -1; // Disable room connection in polling
            port = 0;
            room_file_description = -1;
        }
        else if (str_buffer.substr(0, WAITTO.length()) == WAITTO)
        {                                       // Waiting for second player
            write(STDOUT, buffer, BUFFER_SIZE); // Inform the user
        }
    }
}
int room_TCP_handler(char *buffer, int &room_fd)
{
    int size_of_room_msg = recv(room_fd, buffer, BUFFER_SIZE, 0); // Receive room's message
    string str_buffer(buffer);
    // cout << "room said" << buffer << endl;
    if (size_of_room_msg > 0)
    {

        if (str_buffer.substr(0, START_MESSAGE.length()) == START_MESSAGE)
        // Game start message received
        {
            write(STDOUT, buffer, BUFFER_SIZE); // Display room's message

            fd_set fds; // Set up for select() to handle timeout
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);

            struct timeval timeout;
            timeout.tv_sec = 10; // Set timeout to 10 seconds
            timeout.tv_usec = 0;

            memset(chosen, '4', 2); // Default choice if timeout occurs (represents timeout choice)
            int ret_val = select(STDIN_FILENO + 1, &fds, NULL, NULL, &timeout);
            if (ret_val == 0)
            {
                // Timeout occurred
                send(server_file_description, CHECK_ROOM.c_str(), CHECK_ROOM.length(), 0); // Inform server to check the room
                send(room_file_description, chosen, strlen(chosen), 0);                    // Send the player's choice to the room
                // continue;
                return 1;
            }

            int choice = read(STDIN, chosen, 2); // Read player's choice
            if (choice > 0)
            {
                send(server_file_description, CHECK_ROOM.c_str(), CHECK_ROOM.length(), 0); // Inform server to check the room
                send(room_file_description, chosen, strlen(chosen), 0);                    // Send the player's choice to the room
            }
        }
        else if (str_buffer.substr(0, PLAY_AGAIN.length()) == PLAY_AGAIN)
        // Game start message received
        {
            write(STDOUT, buffer, BUFFER_SIZE); // Display room's message
        }
    }
    return 0;
}

// void server_UDP_handler(int &server_broadcast_fd, char* buffer, sockaddr_in server_broadcast_addr, socklen_t addr_len, int&gameisgoing_flag)
// {
//     int size_of_broadcast = recvfrom(server_broadcast_fd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&server_broadcast_addr, &addr_len);
//     // cout << "server broad" << buffer << endl;
//     if (size_of_broadcast > 0)
//     {
//         write(STDOUT, buffer, size_of_broadcast); // Display broadcast message
//         if (string(buffer).find(END_GAME) != string::npos)
//         {
//             // If termination message received
//             write(STDOUT, GG.c_str(), GG.length()); // Say goodbye
//             gameisgoing_flag = 0;                      // Exit the main game loop
//         }
//     }
// }

int main(int argc, char *argv[])
{
    if (argc != 3)
        perror("Invalid Arguments"); // Ensure correct number of arguments

    char *ipaddr = argv[1];                                // Server IP address from command-line arguments
    struct sockaddr_in server_addr, server_broadcast_addr; // Address structures for server and broadcast
    int server_fd, server_broadcast_fd, opt = 1;           // File descriptors and option flag
    int broadcast = 1;
    int broadcast1 = 1, opt1 = 1;
    create_broadcast_socket(server_addr, ipaddr, server_broadcast_fd, server_broadcast_addr);
    socklen_t addr_len = sizeof(server_broadcast_addr);
    create_TCP_socket(server_fd);
    memset(server_addr.sin_zero, 0, sizeof(server_addr.sin_zero)); // Zero out the rest of the struct

    server_addr.sin_port = htons(strtol(argv[2], NULL, 10)); // Set server port from arguments

    // Connect to the server
    if (connect(server_fd, (sockaddr *)(&server_addr), sizeof(server_addr)))
        perror("FAILED: Connect");

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int recv_size = recv(server_fd, buffer, BUFFER_SIZE, 0); // Receive initial message from server(ENTER YOUR NAME PLEASE)
    if (recv_size > 0)
        write(STDOUT, buffer, BUFFER_SIZE); // Display server's message
    else
        write(STDOUT, "Failed\n", strlen("Failed\n"));

    char client_name[BUFFER_SIZE];
    memset(client_name, 0, BUFFER_SIZE);
    read(STDIN, client_name, BUFFER_SIZE); // Read player's name from standard input
    size_t len = strlen(client_name);
    if (client_name[len - 1] == '\n')
        client_name[len - 1] = '\0';                      // Remove newline character
    send(server_fd, client_name, strlen(client_name), 0); // Send player's name to server

    // Initialize polling structures
    struct pollfd fds[4];
    fds[0].fd = STDIN; // Standard input
    fds[0].events = POLLIN;
    fds[1].fd = server_fd; // Server connection
    fds[1].events = POLLIN;
    fds[2].fd = -1; // Room connection (initialized as inactive)
    fds[2].events = POLLIN;
    fds[3].fd = server_broadcast_fd; // Broadcast messages from server
    fds[3].events = POLLIN;
    server_file_description = server_fd; // Store server file descriptor globally

    int GAME_IS_GOING = 1;        // Flag to control the main game loop
    int port = 0;                 // Variable to store the room's port number
    int room_fd;                  // File descriptor for the room connection
    struct sockaddr_in room_port; // Address structure for the room

    while (GAME_IS_GOING)
    {
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        if (poll(fds, 4, -1) < 0)
        {
            perror("Polling failed!\n");
        }

        if (fds[0].revents & POLLIN) // If there's input from the user
        {
            int user_msg = read(STDIN, buffer, BUFFER_SIZE);
            // cout << "cliend stdins said" << buffer << endl;
            if (user_msg > 0)
                send(server_fd, buffer, user_msg, 0); // Send user input to the server
        }

        if (fds[1].revents & POLLIN) // If there's a message from the server
        {
            server_TCP_handler(server_fd, buffer, port, ipaddr, server_addr, room_fd, room_port, fds[2]);
        }

        if (fds[2].revents & POLLIN) // If there's a message from the room
        {
            int is_timeout = room_TCP_handler(buffer, room_fd);
            if (is_timeout)
                continue;
        }
        if (fds[3].revents & POLLIN) // If there's a broadcast message from the server
        {
            int size_of_broadcast = recvfrom(server_broadcast_fd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&server_broadcast_addr, &addr_len);
            // cout << "server broad" << buffer << endl;
            if (size_of_broadcast > 0)
            {
                write(STDOUT, buffer, size_of_broadcast); // Display broadcast message
                if (string(buffer).find(END_GAME) != string::npos)
                {
                    // If termination message received
                    write(STDOUT, GG.c_str(), GG.length()); // Say goodbye
                    GAME_IS_GOING = 0;                      // Exit the main game loop
                }
            }
            // server_UDP_handler(server_broadcast_fd, buffer, server_broadcast_addr, addr_len, GAME_IS_GOING);
        }
    }
}
