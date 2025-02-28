#include <string>
using namespace std;

typedef struct pollfd pollfd; // Shorthand for struct pollfd

// Define file descriptors for standard input and output
#define STDIN 0
#define STDOUT 1

// Define buffer size and broadcast configurations
#define BUFFER_SIZE 1024
#define BROADCAST_PORT 9909
#define BROADCAST_IP "127.255.255.255"

// Define messages used in communication with the server
const string ROOMS_STATUS = "Room MENU\n";
const string GG = "Goodbye!\n";
const string ROOM_FULL = "this room is full.\n try again!\n";
const string START_MESSAGE = "select a number please:\n1.rock\n2.paper\n3.scissors\n";
const string ROOM_CONNECT = "Now you are connected to this port's room : ";
const string END_GAME = "FINAL SCORES TABLE\n";
const string PLAY_AGAIN = "Do you want to play Again? (y/n)\n";
const string ROOM_RESETED = "RESET THE ROOM\n";
const string WAITTO = "waiting for second player... \n";
const string CHECK_ROOM = "Check the room";