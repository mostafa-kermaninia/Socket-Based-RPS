#include <string>
using namespace std;
// Define file descriptors for standard input and output
#define STDIN 0
#define STDOUT 1

// player statuses
enum PlayerStatus
{
    NEW_ENTERED,
    PLAY_AGAIN_DECIDER,
    ROOM_SELECTOR,
    IN_GAME,
    VIEWER,
};

// players choices
enum PlayerChoices
{
    invalid = '0',
    rock = '1',
    paper = '2',
    scissors = '3',
    timeOut = '4',
};

// Define constants for server and broadcast configurations
#define PORT 10909                     // Base port number for the server
#define SERVER_BROADCAST_PORT 9909     // Port number for server broadcasts
#define BROADCAST_IP "127.255.255.255" // Broadcast IP address

// Define player status codes
#define WAITING_FOR_SECOND_PLAYER 4 // Status code indicating waiting for the second player

// Define buffer size and number of clients
#define BUFFER_SIZE 1024 // Size of the buffer for data transmission
#define NUM_OF_CLIENTS 2 // Number of clients per game room

// Define messages used in communication with clients
const string GIVE_NAME = "Please enter your name\n";
const string PARTITIONER = "__________________________\n";
const string START_MESSAGE = "select a number please:\n1.rock\n2.paper\n3.scissors\n";
const string SERVER_LAUNCHED = "manager is here\n";
const string ENTERING_ANOUNCE = "We have a new player\n";
const string CHECK_ROOM = "Check the room";
const string PLAY_AGAIN = "Do you want to play Again? (y/n)\n";
const string ROOMS_STATUS = "Room MENU\n";
const string END_GAME = "end_game\n";
const string ROOM_RESETED = "RESET THE ROOM\n";
const string ROOM_CONNECT = "Now you are connected to this port's room : ";
const string AVAILABLE = " EMPTY places\n";
const string ROOM_FULL = "this room is full.\n try again!\n";

// Define a shorthand for the pollfd structure
typedef struct pollfd pollfd;