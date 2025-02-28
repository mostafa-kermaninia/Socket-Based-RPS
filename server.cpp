#include <stdio.h>
#include <iostream>
#include <string>
#include <csignal> // Signal handling
#include <unordered_map>
#include <arpa/inet.h>
#include <cstring>
#include <vector>
#include <poll.h>
#include "magic-value-server.hpp"

using namespace std;

void create_room_socket(int port, char *ipaddr, sockaddr_in &my_addr, int &this_room_fd, int &server_fd)
{
    int opt = 1;
    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, ipaddr, &(my_addr.sin_addr)) == -1)
        perror("FAILED: Input ipv4 address invalid");

    // Create a socket for the room
    if ((this_room_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        perror("FAILED: Socket was not created");

    // Set socket options to reuse address and port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1)
        perror("FAILED: Making socket or socket port reusable failed");

    // Configure the room's address
    my_addr.sin_family = AF_INET;         // IPv4
    my_addr.sin_addr.s_addr = INADDR_ANY; // Bind to any available interface
    my_addr.sin_port = htons(port);       // Set the port number

    // Bind the room's socket to the specified address and port
    if (bind(this_room_fd, (const struct sockaddr *)(&my_addr), sizeof(my_addr)) == -1)
        perror("FAILED:$$$ Bind unsuccessful");

    // Start listening for incoming connections
    if (listen(this_room_fd, 3) == -1)
        perror("FAILED: Listen unsuccessful");
}

void create_server_socket(char *ipaddr, char *argv, sockaddr_in &server_addr, int &server_fd, int &broadcast_fd, sockaddr_in &broadcast_addr)
{
    int opt = 1;

    server_addr.sin_family = AF_INET; // IPv4
    if (inet_pton(AF_INET, ipaddr, &(server_addr.sin_addr)) == -1)
        perror("FAILED: Input ipv4 address invalid");

    // Create a TCP socket for the server
    if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        perror("FAILED: Socket was not created");

    // Set socket options to reuse address
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        perror("FAILED: Making socket reusable failed");

    // Set socket options to reuse port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1)
        perror("FAILED: Making socket reusable port failed");

    server_addr.sin_port = htons(strtol(argv, NULL, 10)); // Set server port from arguments

    // Bind the server socket to the specified address and port
    if (bind(server_fd, (const struct sockaddr *)(&server_addr), sizeof(server_addr)) == -1)
        perror("FAILED: $$$$$Bind unsuccessful");

    // Start listening for incoming connections
    if (listen(server_fd, 20) == -1)
        perror("FAILED: Listen unsuccessful");

    write(1, SERVER_LAUNCHED.c_str(), SERVER_LAUNCHED.length()); // Indicate that the server has launched

    // Create a UDP socket for broadcasting messages to clients
    if ((broadcast_fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
        perror("FAILED: Socket was not created");
    int broadcast_permission = 1;
    // Set socket options to allow broadcast
    if (setsockopt(broadcast_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_permission, sizeof(broadcast_permission)) < 0)
        perror("FAILED: permission not granted");
    // Set socket options to reuse address and port
    if (setsockopt(broadcast_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0)
        perror("FAILED: permission not granted");
    memset(broadcast_addr.sin_zero, 0, sizeof(server_addr.sin_zero)); // Zero out the rest of the struct
    broadcast_addr.sin_family = AF_INET;                              // IPv4
    broadcast_addr.sin_port = htons(SERVER_BROADCAST_PORT);           // Set broadcast port
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);         // Set broadcast IP address
    // Bind the broadcast socket
    bind(broadcast_fd, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
}

// Class representing a game room
class Room
{
public:
    // Constructor for initializing a room
    Room(int server_fd_, int p, int num, char *ip)
    {
        port = p;               // Room's port number
        server_fd = server_fd_; // Server file descriptor

        first_player = 0;    // Socket descriptor for the first player
        second_player = 0;   // Socket descriptor for the second player
        players_count = num; // Number of players in the room (initially 0)

        char *ipaddr = ip; // IP address

        int opt = 1;                // Option value for setsockopt
        struct sockaddr_in my_addr; // Address structure for the room
        create_room_socket(port, ipaddr, my_addr, this_room_fd, server_fd);
    }

    int first_player, second_player; // Socket descriptors for players
    int p1_score, p2_score;          // Scores for players

    int players_count; // Number of players currently in the room

    int server_fd, this_room_fd; // File descriptors for the server and the room
    int port;                    // Port number for the room

    pollfd players[2];       // Array of pollfd structures for the players
    struct sockaddr_in addr; // Address structure for the room

    int game_status = 0;       // Status of the game (0: not started, 1: ongoing, 2: ended)
    int clients_situation = 0; // Variable to track clients' situations (unused in current code)

    // Function to send a play-again request to both players
    void play_again_request()
    {
        send(first_player, PLAY_AGAIN.c_str(), PLAY_AGAIN.length(), 0);
        send(second_player, PLAY_AGAIN.c_str(), PLAY_AGAIN.length(), 0);
    }

    // Function to add a player to the room
    void add_player(pollfd player, int score)
    {
        if (first_player == 0)
        {
            first_player = 1;    // Mark first player as connected
            p1_score = score;    // Initialize first player's score
            players[0] = player; // Add player to the players array

            // Send a message to the first player to wait for the second player
            const char *wait_message = "waiting for second player... \n";
            send(players[0].fd, wait_message, strlen(wait_message), 0);
            sleep(1); // Delay to prevent message overlapping
        }
        else
        {
            second_player = 1;   // Mark second player as connected
            p2_score = score;    // Initialize second player's score
            players[1] = player; // Add player to the players array
        }
        players_count++; // Increment the count of players in the room
        return;
    }

    // Function to handle a single game round
    int handle_a_game()
    {
        char choice1[2];       // Buffer to store first player's choice
        char choice2[2];       // Buffer to store second player's choice
        memset(choice1, 0, 2); // Initialize the buffers
        memset(choice2, 0, 2);
        recv(first_player, choice1, 2, 0);  // Receive first player's choice
        recv(second_player, choice2, 2, 0); // Receive second player's choice

        char p1_choice = choice1[0], p2_choice = choice2[0];
        // cout << "first player" << p1_choice << "second" << p2_choice << endl;
        game_status = 2; // Update the game status to indicate the game has ended
        int o = rock == '1';
        if (
            (p2_choice == paper && p1_choice == rock) || (p2_choice == rock && p1_choice == scissors) ||
            (p2_choice == scissors && p1_choice == paper) || (p1_choice == timeOut && p2_choice != timeOut))
        {
            // Second player wins
            // cout << "sec wonn" << players[1].fd << "fir lost" << players[0].fd << endl;

            return players[1].fd;
        }
        else if (p1_choice == p2_choice)
        {
            // It's a draw
            return 0;
        }
        else
        {
            // First player wins
            // cout << "first wonn" << players[0].fd << "second lost" << players[1].fd << endl;
            return players[0].fd;
        }
    }

private:
};

// Function to handle the end of the game and broadcast final scores
bool end_game(vector<pollfd> pfds, unordered_map<int, string> players_fd_and_name, struct sockaddr_in broadcast_addr,
              int broadcast_fd, vector<int> clients_scores)
{
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);   // Clear the buffer
    read(STDIN, buffer, BUFFER_SIZE); // Read from standard input

    if (strcmp(buffer, END_GAME.c_str()) == 0)
    {
        // If termination command is received, prepare the final scores table
        string res = PARTITIONER + "FINAL SCORES TABLE\n";
        for (int i = 2; i < pfds.size(); i++)
        {
            res += players_fd_and_name[pfds[i].fd] + " "; // Append player's name
            res += to_string(clients_scores[i]) + '\n';   // Append player's score
        }
        res += PARTITIONER;

        // Broadcast the final scores to all clients
        sendto(broadcast_fd, res.c_str(), strlen(res.c_str()), 0,
               (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
        sleep(1); // Delay to ensure message is sent

        return true; // Indicate that the game has ended
    }
    return false; // Game continues
}

// Function to generate the status of all rooms
string get_rooms_status(vector<Room> rooms)
{
    string status = PARTITIONER;
    status += ROOMS_STATUS; // Start with the room menu header
    status += "(Just select a room number)\n";
    for (int i = 0; i < rooms.size(); i++)
    {
        // For each room, if there are free places, add it to the status string
        if ((2 - rooms[i].players_count) > 0)
            status += "room " + to_string(i + 1) + " : " + to_string((2 - rooms[i].players_count)) + AVAILABLE;
    }
    status += PARTITIONER;
    return status; // Return the generated status string
}

int main(int argc, char *argv[])
{
    char *ipaddr = argv[1]; // IP address from command-line arguments

    int num_of_rooms = stoi(argv[3]);               // Number of game rooms from arguments
    struct sockaddr_in server_addr, broadcast_addr; // Address structures for server and broadcast
    int server_fd, broadcast_fd, opt = 1;           // File descriptors and option flag
    vector<pollfd> pfds;                            // Vector of pollfd structures for polling
    vector<int> player_stat;                        // Vector to track player statuses
    vector<int> clients_scores;                     // Vector to track clients' scores
    vector<int> players_room_number;                // Vector to map players to their room numbers
    vector<int> ready_players_in_room;              // Vector to track room inspections
    unordered_map<int, string> players_fd_and_name; // Map to store players' file descriptors and names

    memset(server_addr.sin_zero, 0, sizeof(server_addr.sin_zero)); // Zero out the rest of the struct
    create_server_socket(argv[1], argv[2], server_addr, server_fd, broadcast_fd, broadcast_addr);

    // Create game rooms
    vector<Room> rooms;
    for (int i = 1; i < num_of_rooms + 1; i++)
    {
        Room room = Room(server_fd, PORT + i, 0, ipaddr); // Initialize each room
        ready_players_in_room.push_back(0);               // Initialize ready players status for each room
        rooms.push_back(room);                            // Add room to the rooms vector
    }

    // Initialize polling structures
    pfds.push_back(pollfd{server_fd, POLLIN, 0}); // Add server socket to polling
    players_room_number.push_back(-1);            // Initialize player's room number
    clients_scores.push_back(-1);                 // Initialize client score
    player_stat.push_back(-1);                    // Initialize player status

    pfds.push_back(pollfd{STDIN, POLLIN, 0}); // Add standard input to polling
    players_room_number.push_back(-1);        // Initialize player's room number
    clients_scores.push_back(-1);             // Initialize client score
    player_stat.push_back(-1);                // Initialize player status

    while (1) // Main server loop
    {
        // Iterate over each room to check if the game should start
        for (int i = 0; i < num_of_rooms; i++)
        {
            // If the room is full and the game hasn't started yet
            if ((2 - rooms[i].players_count) == 0 && (rooms[i].game_status == 0))
            {
                // rooms[i].start_game(); // Start the game in the room
                // Send the start message to both players
                send(rooms[i].first_player, START_MESSAGE.c_str(), START_MESSAGE.length(), 0);
                send(rooms[i].second_player, START_MESSAGE.c_str(), START_MESSAGE.length(), 0);
                rooms[i].game_status = 1; // Update the game status to indicate the game has started
            }
        }

        // Poll the file descriptors to check for events
        if (poll(pfds.data(), (nfds_t)(pfds.size()), -1) == -1)
            perror("FAILED: Poll");

        // Check if there's input from the standard input (e.g., termination command)
        if (pfds[1].revents & POLLIN)
        {
            if (end_game(pfds, players_fd_and_name, broadcast_addr, broadcast_fd, clients_scores))
            {
                // If the game ends, close all player connections and exit
                for (int i = 2; i < pfds.size(); i++)
                    close(pfds[i].fd);
                return 0; // Exit the program
            }
        }

        // Iterate over all file descriptors to check for events
        for (size_t i = 0; i < pfds.size(); ++i)
        {
            // If there's an event on the file descriptor
            if ((pfds[i].revents & POLLIN) && i != 1)
            {
                if (pfds[i].fd == server_fd) // New incoming connection on the server socket
                {
                    struct sockaddr_in new_addr; // Address structure for the new connection
                    socklen_t new_size = sizeof(new_addr);
                    int new_fd = accept(server_fd, (struct sockaddr *)(&new_addr), &new_size); // Accept the new connection
                    write(STDOUT, ENTERING_ANOUNCE.c_str(), ENTERING_ANOUNCE.length());        // Notify about the new connection
                    send(new_fd, GIVE_NAME.c_str(), GIVE_NAME.length(), 0);                    // Prompt for player's name

                    // Add the new client to the polling structures
                    pfds.push_back(pollfd{new_fd, POLLIN, 0});
                    player_stat.push_back(0);         // Initialize player's status
                    clients_scores.push_back(0);      // Initialize player's score
                    players_room_number.push_back(0); // Initialize player's room number
                }
                else if (player_stat[i] != VIEWER) // If the player is active (not a viewer)
                {
                    char buffer[BUFFER_SIZE];
                    memset(buffer, 0, BUFFER_SIZE);
                    recv(pfds[i].fd, buffer, BUFFER_SIZE, 0); // Receive data from the player

                    // If it's the player's first message or status is 0 (initial state)
                    if ((players_fd_and_name.find(pfds[i].fd) == players_fd_and_name.end() || player_stat[i] == NEW_ENTERED))
                    {
                        // Store the player's name
                        players_fd_and_name[pfds[i].fd] = buffer;
                        string sign_in_message = "new player's name is " + players_fd_and_name[pfds[i].fd] + "\n";
                        write(STDOUT, sign_in_message.c_str(), sign_in_message.length()); // Output player's name to server console
                        string status = get_rooms_status(rooms);                          // Get the current status of rooms
                        send(pfds[i].fd, status.c_str(), status.length(), 0);             // Send rooms status to the player
                        player_stat[i] = ROOM_SELECTOR;
                        // Update player's status to indicate they should choose a room
                    }
                    else if (player_stat[i] == PLAY_AGAIN_DECIDER) // If the player is deciding whether to play again
                    {
                        string buffer_str(buffer);
                        if (strcmp(buffer, "y\n") == 0 || (buffer_str.size() >= 2 && buffer_str.substr(buffer_str.size() - 2) == "y\n"))
                        // the second case is when in last step user choose a number for rock,paper or scissors but he doesnt press \
                            enter and now that number comes to buffer befor y or n
                        {
                            // Player wants to play again
                            string status = get_rooms_status(rooms);            // Get the current status of rooms
                            send(pfds[i].fd, status.c_str(), status.size(), 0); // Send rooms status to the player
                            player_stat[i] = ROOM_SELECTOR;                     // Update player's status to room selection
                        }
                        else if (strcmp(buffer, "n\n") == 0)
                        {
                            // Player does not want to play again
                            player_stat[i] = VIEWER; // Change player's status to viewer
                        }
                        else
                        {
                            // Invalid input, change player's status to viewer
                            player_stat[i] = VIEWER;
                        }
                    }
                    else if (player_stat[i] == ROOM_SELECTOR) // Player is selecting a room
                    {
                        int room_number = atoi(buffer);                     // Get the selected room number
                        if ((2 - rooms[room_number - 1].players_count) > 0) // Check if the room is not full
                        {
                            // Add player to the selected room
                            rooms[room_number - 1].add_player(pfds[i], clients_scores[i]);
                            player_stat[i] = IN_GAME;                 // Update player's status to in-game
                            players_room_number[i] = room_number - 1; // Map player to the room number

                            // Send confirmation of room connection with the room's port number
                            send(pfds[i].fd, (ROOM_CONNECT + to_string(PORT + room_number) + '\n').c_str(),
                                 strlen((ROOM_CONNECT + to_string(PORT + room_number) + '\n').c_str()), 0);

                            // Accept the player's connection to the room
                            if ((2 - rooms[room_number - 1].players_count) == 1)
                            {
                                int new_socket1;
                                int len = sizeof(rooms[room_number - 1].addr);

                                // Accept a new connection for the first player
                                new_socket1 = accept(rooms[room_number - 1].this_room_fd, (struct sockaddr *)&(rooms[room_number - 1].addr), (socklen_t *)&len);
                                rooms[room_number - 1].first_player = new_socket1; // Assign the socket descriptor to the first player
                            }
                            // rooms[room_number - 1].p1_accept(); // Accept as first player
                            else
                            {
                                int new_socket2;
                                int len = sizeof(rooms[room_number - 1].addr);

                                // Accept a new connection for the second player
                                new_socket2 = accept(rooms[room_number - 1].this_room_fd, (struct sockaddr *)&(rooms[room_number - 1].addr), (socklen_t *)&len);
                                rooms[room_number - 1].second_player = new_socket2; // Assign the socket descriptor to the second player
                            }
                        }
                        else // If the room is full
                        {
                            string msg = ROOM_FULL;
                            msg += get_rooms_status(rooms);                 // Append the current status of rooms
                            send(pfds[i].fd, msg.c_str(), msg.length(), 0); // Notify the player that the room is full
                        }
                    }
                    else if (player_stat[i] == IN_GAME && (strcmp(buffer, CHECK_ROOM.c_str()) == 0))
                    {
                        // Player is ready to proceed in the game
                        player_stat[i] = PLAY_AGAIN_DECIDER;             // Update player's status to waiting
                        ready_players_in_room[players_room_number[i]]++; // Increment the room's inspection counter

                        // If both players in the room are ready
                        if (ready_players_in_room[players_room_number[i]] == 2)
                        {
                            int winner = rooms[players_room_number[i]].handle_a_game(); // Handle the game round
                            if (winner != 0)                                            // If there is a winner
                            {
                                for (int i = 0; i < pfds.size(); i++)
                                {
                                    if (pfds[i].fd == winner)
                                    {
                                        clients_scores[i]++; // Increment the winner's score
                                        string result = "player " + players_fd_and_name[winner] + " won in room " + to_string(players_room_number[i] + 1) + "\n";
                                        // Broadcast the result to all clients
                                        sendto(broadcast_fd, result.c_str(), strlen(result.c_str()), 0,
                                               (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));

                                        sleep(1);
                                    }
                                }
                            }
                            else // If it's a draw
                            {
                                string result = "Result of game in room " + to_string(players_room_number[i] + 1) + " is Draw\n";
                                // Broadcast the draw result to all clients
                                int byte = sendto(broadcast_fd, result.c_str(), strlen(result.c_str()), 0,
                                                  (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
                                sleep(1);
                            }
                            rooms[players_room_number[i]].play_again_request(); // Ask players if they want to play again
                            sleep(1);
                            send(pfds[i].fd, ROOM_RESETED.c_str(), ROOM_RESETED.length(), 0); // Notify player of room reset

                            // reset the room after a game
                            rooms[players_room_number[i]].players_count = 0;     // Reset players count
                            rooms[players_room_number[i]].game_status = 0;       // Reset game status
                            rooms[players_room_number[i]].first_player = 0;      // Reset first player's socket descriptor
                            rooms[players_room_number[i]].second_player = 0;     // Reset second player's socket descriptor
                            rooms[players_room_number[i]].clients_situation = 0; // Reset clients' situation

                            ready_players_in_room[players_room_number[i]] = 0; // Reset the inspection counter for the room
                        }
                    }
                }
            }
        }
    }
}
