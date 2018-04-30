#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <string>
#include <conio.h>
#include <algorithm>
#include <vector>
#include <iomanip>

#include "buffer.h"

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

bool shutIt = false;
bool amConnected = false;
bool loggedIn = false;	//Needs to be altered based on server response.
bool inLobby = false;

std::string userName;

//Buffers we'll be storing data in
buffer sendBuffer(512);
buffer recvBuffer(512);

//Socket is now global
SOCKET ConnectSocket = INVALID_SOCKET;
int iResult;
//For user input using _kbhit
std::string userIn = "";
char keyIn;

bool processMessage(std::string userMessage);
bool connectToServer(std::string userMessage);
bool validateIpAddress(const std::string &ipAddress);
bool registerUser(std::string input);
bool loginUser(std::string input);
bool createLobby(std::string userMessage);
bool refresh();
bool joinLobby(std::string userMessage);
bool leaveLobby();
bool logoutUser();
void createInfo();
bool recvMessage();

int getIntegerFromInputStream(std::istream & is)
{
	std::string input;
	std::getline(is, input);

	// C++11 version
	return stoi(input);
	// throws on failure

	// C++98 version
	/*
	std::istringstream iss(input);
	int i;
	if (!(iss >> i)) {
	// handle error somehow
	}
	return i;
	*/
}

int __cdecl main(int argc, char **argv)
{
	using namespace std::literals;

	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;

	// Receive until the peer closes the connection
	do
	{
		if (_kbhit())
		{
			char keyIn;
			keyIn = _getch();
			if (keyIn == '\r')
			{
				std::cout << '\r';
				for (int i = 0; i < userIn.length(); i++)
					std::cout << ' ';
				std::cout << '\r';
				shutIt = !processMessage(userIn);
				userIn = "";
			}
			else if (keyIn == 127 || keyIn == 8) //backspace OR delete
			{
				userIn = userIn.substr(0, userIn.length() - 1);
				std::cout << keyIn << ' ' << keyIn;
			}
			else
			{
				userIn += keyIn;
				std::cout << keyIn;
			}
		}
		if (amConnected)
		{
			iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
			if (iResult > 0)
			{
				if (iResult > 0)
				{	//Collect all data from the buffer
					std::string getData = "";
					for (int i = 0; i < 4; i++)
						getData += recvbuf[i];
					recvBuffer.writeString(getData);
					int recvSize = recvBuffer.readInt32BE();
					for (int i = 4; i < recvSize; i++)
						getData += recvbuf[i];
					recvBuffer.writeString(getData);
					recvMessage();
				}
						
			}
			else if (iResult == 0)
				//printf("Connection closed\n");
				continue;
		}
	} while (!shutIt);

	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();

	return 0;
}

bool processMessage(std::string userMessage)
{
	std::istringstream iss(userMessage);
	std::string s;
	iss >> s;
	if (s == "/connect")
	{
		if (amConnected)
		{
			std::cout << "Already connected to a game server.\n";
			return 1;
		}
		bool connected = connectToServer(userMessage);
		if (connected)
			std::cout << "Connected successfully!" << std::endl;
		return 1;
	}

	else if (s == "/register")
	{
		if (!amConnected)
		{
			std::cout << "Please connect to game server first.\n";
			return 1;
		}
		registerUser(userMessage);
		return 1;
	}

	else if (s == "/login")
	{
		if (!amConnected)
		{
			std::cout << "Please connect to game server first.\n";
			return 1;
		}
		loginUser(userMessage);
		return 1;
	}

	else if (s == "/create")
	{
		if (!amConnected)
		{
			std::cout << "Please connect to game server first.\n";
			return 1;
		}
		if (!loggedIn)
		{
			std::cout << "Please log in first.\n";
			return 1;
		}
		iss >> s;
		if (s == "?")
		{
			createInfo();
			return 1;
		}
		if (inLobby)
		{
			std::cout << "You are already in a lobby.\n";
			return 1;
		}
		createLobby(userMessage);
		return 1;
	}

	else if (s == "/refresh")
	{
		if (!amConnected)
		{
			std::cout << "Please connect to game server first.\n";
			return 1;
		}
		refresh();
		return 1;
	}

	else if (s == "/join")
	{
		if (!amConnected)
		{
			std::cout << "Please connect to game server first.\n";
			return 1;
		}
		if (!loggedIn)
		{
			std::cout << "Please log in first.\n";
			return 1;
		}
		if (inLobby)
		{
			std::cout << "You are already in a lobby.\n";
			return 1;
		}

		joinLobby(userMessage);
		return 1;
	}

	else if (s == "/leave")
	{
		if (!amConnected)
		{
			std::cout << "Please connect to game server first.\n";
			return 1;
		}
		if (!inLobby)
		{
			std::cout << "You are not currently in a lobby.\n";
			return 1;
		}
		leaveLobby();
		return 1;
	}

	else if (s == "/logout")
	{
		if (!amConnected)
		{
			std::cout << "Please connect to game server first.\n";
			return 1;
		}
		if (!loggedIn)
		{
			std::cout << "You are not logged in.\n";
			return 1;
		}
		logoutUser();
		return 1;
	}

	else if (s == "/quit") {
		if (amConnected)
		{
			closesocket(ConnectSocket);
			WSACleanup();
		}
		return 0;
	}
	else
	{
		std::cout << "Invalid command. Begin all lines with /join, /leave, /send followed by your room name, or /logout\n";
	}
	return 1;
}

bool connectToServer(std::string userMessage)
{
	std::istringstream iss(userMessage);
	std::string ipAddress, port;
	iss >> ipAddress; // should be /connect
	iss >> ipAddress;
	iss >> port;

	if (!validateIpAddress(ipAddress))
	{
		std::cout << "Invalid IP address. Try again\n";
		return 0;
	}	

	WSADATA wsaData;

	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 0;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(ipAddress.c_str(), port.c_str(), &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 0;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return 0;
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 0;
	}

	//This makes the socket non-blocking
	u_long iMode = 1;
	iResult = ioctlsocket(ConnectSocket, FIONBIO, &iMode);

	amConnected = true;
	return 1;
}

bool validateIpAddress(const std::string &ipAddress)
{
	struct sockaddr_in sa;
	int result = inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr));
	return result != 0;
}

bool registerUser(std::string input)
{
	std::istringstream iss(input);
	std::string user, password;
	iss >> user; // Should be /register
	iss >> user;
	iss >> password;

	int messageID = 4;
	int userLength = user.length();
	int passwordLength = password.length();
	int packetLength = userLength + passwordLength + 16;

	sendBuffer.writeInt32BE(packetLength);
	sendBuffer.writeInt32BE(messageID);
	sendBuffer.writeInt32BE(userLength);
	sendBuffer.writeString(user);
	sendBuffer.writeInt32BE(passwordLength);
	sendBuffer.writeString(password);

	std::string sendString = sendBuffer.readString(packetLength);
	int sendLength = sendString.length();

	iResult = send(ConnectSocket, sendString.c_str(), sendLength, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 0;
	}

	return 1;
}

bool loginUser(std::string input)
{
	std::istringstream iss(input);
	std::string user, password;
	iss >> user; // Should be /authenticate
	iss >> user;
	iss >> password;

	userName = user;

	int messageID = 5;
	int userLength = user.length();
	int passwordLength = password.length();
	int packetLength = userLength + passwordLength + 16;

	sendBuffer.writeInt32BE(packetLength);
	sendBuffer.writeInt32BE(messageID);
	sendBuffer.writeInt32BE(userLength);
	sendBuffer.writeString(user);
	sendBuffer.writeInt32BE(passwordLength);
	sendBuffer.writeString(password);

	std::string sendString = sendBuffer.readString(packetLength);
	int sendLength = sendString.length();

	iResult = send(ConnectSocket, sendString.c_str(), sendLength, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 0;
	}

	return 1;
}

bool createLobby(std::string userMessage)
{
	std::istringstream iss(userMessage);
	std::string map, mode, name;
	iss >> map; // Should be /create
	iss >> map;
	iss >> mode;
	iss >> name;

	for (int i = 0; i < name.length(); i++)
		if (name[i] == ' ')
			name[i] = '_';

	if (!(map == "Cuba" || map == "Cubeworld" || map == "Parking_Lot" || map == "ParkingLot"))
	{
		std::cout << "Map name is invalid. Type /create ? for more details\n";
		return 0;
	}

	if (!(mode == "TD" || mode == "FFA" || mode == "BV"))
	{
		std::cout << "Mode type is invalid. Type /create ? for more details\n";
		return 0;
	}

	int messageID = 1;
	int mapLength = map.length();
	int modeLength = mode.length();
	int nameLength = name.length();
	int packetLength = mapLength + modeLength + nameLength + 20;

	sendBuffer.writeInt32BE(packetLength);
	sendBuffer.writeInt32BE(messageID);
	sendBuffer.writeInt32BE(mapLength);
	sendBuffer.writeString(map);
	sendBuffer.writeInt32BE(modeLength);
	sendBuffer.writeString(mode);
	sendBuffer.writeInt32BE(nameLength);
	sendBuffer.writeString(name);

	std::string sendString = sendBuffer.readString(packetLength);
	int sendLength = sendString.length();

	iResult = send(ConnectSocket, sendString.c_str(), sendLength, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 0;
	}

	return 1;
}

bool refresh()
{
	int packetLength = 8;
	int messageID = 3;
	sendBuffer.writeInt32BE(packetLength);
	sendBuffer.writeInt32BE(messageID);

	std::string sendString = sendBuffer.readString(packetLength);
	int sendLength = sendString.length();

	iResult = send(ConnectSocket, sendString.c_str(), sendLength, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 0;
	}

	return 1;
}

bool joinLobby(std::string userMessage)
{
	std::istringstream iss(userMessage);
	std::string name;
	iss >> name; // Should be /join
	iss >> name;

	int nameLength = name.length();
	int packetID = 2;
	int packetLength = nameLength + 12;

	sendBuffer.writeInt32BE(packetLength);
	sendBuffer.writeInt32BE(packetID);
	sendBuffer.writeInt32BE(nameLength);
	sendBuffer.writeString(name);

	std::string sendString = sendBuffer.readString(packetLength);
	int sendLength = sendString.length();

	iResult = send(ConnectSocket, sendString.c_str(), sendLength, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 0;
	}

	return 1;
}

bool leaveLobby()
{
	int packetLength = 8;
	int messageID = 6;
	sendBuffer.writeInt32BE(packetLength);
	sendBuffer.writeInt32BE(messageID);

	std::string sendString = sendBuffer.readString(packetLength);
	int sendLength = sendString.length();

	iResult = send(ConnectSocket, sendString.c_str(), sendLength, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 0;
	}

	return 1;
}

bool logoutUser()
{
	int packetLength = 8;
	int messageID = 7;
	sendBuffer.writeInt32BE(packetLength);
	sendBuffer.writeInt32BE(messageID);

	std::string sendString = sendBuffer.readString(packetLength);
	int sendLength = sendString.length();

	iResult = send(ConnectSocket, sendString.c_str(), sendLength, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 0;
	}

	return 1;
}

void createInfo()
{
	std::cout << "Maps: Cuba, CubeWorld, Parking_Lot" << std::endl;
	std::cout << "Modes: Team Deathmatch (TD), Free For All(FFA), Beach Volleyball(BV)" << std::endl;
}

bool recvMessage()
{
	int packetLength = recvBuffer.readInt32BE();
	int messageID = recvBuffer.readInt32BE();

	if (messageID == 1) //Creating a lobby
	{
		int result = recvBuffer.readInt32BE();
		if (result == 0)
		{
			std::cout << "Created lobby successfully\n";
			inLobby = true;
		}
		else if (result == 1)
		{
			std::cout << "A lobby with this name already exists\n";
		}
	}

	else if (messageID == 2) //Joining a lobby
	{
		int result = recvBuffer.readInt32BE();
		if (result == 0)
		{
			std::cout << "Joined lobby successfully\n";
			inLobby = true;
		}
		else if (result == 1)
		{
			std::cout << "This lobby is already full\n";
		}
		else if (result == 2)
		{
			std::cout << "No lobby with this name exists\n";
		}
	}

	else if (messageID == 3) //Refreshing lobby info
	{
		int numLobbies = recvBuffer.readInt32BE();

		const char separator = ' ';
		const int nameWidth = 20;
		const int numWidth = 8;

		std::cout << std::left << std::setw(nameWidth) << std::setfill(separator) << "Map Name";
		std::cout << std::left << std::setw(nameWidth) << std::setfill(separator) << "Lobby Name";
		std::cout << std::left << std::setw(nameWidth) << std::setfill(separator) << "Game Mode";
		std::cout << std::left << std::setw(numWidth) << std::setfill(separator) << "Players";
		std::cout << std::left << std::setw(nameWidth) << std::setfill(separator) << "Host Name";
		std::cout << std::endl;

		for (int i = 0; i < numLobbies; i++)
		{
			int mapLength = recvBuffer.readInt32BE();
			std::string mapName = recvBuffer.readString(mapLength);
			int nameLength = recvBuffer.readInt32BE();
			std::string lobbyName = recvBuffer.readString(nameLength);
			int modeLength = recvBuffer.readInt32BE();
			std::string mode = recvBuffer.readString(modeLength);
			int hostLength = recvBuffer.readInt32BE();
			std::string hostName = recvBuffer.readString(hostLength);
			int playersIn = recvBuffer.readInt32BE();
			int maxPlayers = recvBuffer.readInt32BE();

			std::cout << std::left << std::setw(nameWidth) << std::setfill(separator) << mapName;
			std::cout << std::left << std::setw(nameWidth) << std::setfill(separator) << lobbyName;
			std::cout << std::left << std::setw(nameWidth) << std::setfill(separator) << mode;
			std::cout <<  playersIn << "/" << maxPlayers << "     ";
			std::cout << std::left << std::setw(nameWidth) << std::setfill(separator) << hostName;
			std::cout << std::endl;
		}

	}

	else if (messageID == 4) //Creating account
	{
		int result = recvBuffer.readInt32BE();
		if (result == 0)
		{
			std::cout << "Account created successfully. Please log in\n";
		}
		else if (result == 1)
		{
			std::cout << "This username has already been taken. Please try again\n";
		}
		else if (result == 2)
		{
			std::cout << "Invalid. Please try again\n";
		}
		else if (result == 3)
		{
			std::cout << "Internal server error. Please try again later\n";
		}
	}

	else if (messageID == 5) //Logging in
	{
		int result = recvBuffer.readInt32BE();
		if (result == 0)
		{
			std::cout << "Logged in successfully\n";
			loggedIn = true;
			refresh();
		}
		else if (result == 1)
		{
			std::cout << "Username or password is incorrect. Please try again\n";
		}
		else if (result == 2)
		{
			std::cout << "Internal server error. Please try again later\n";
		}
		else if (result == 3)
		{
			std::cout << "This username is already in use\n";
		}
	}

	else if (messageID == 6) //Exiting a lobby
	{
		int result = recvBuffer.readInt32BE();
		if (result == 0)
		{
			std::cout << "Left the lobby\n";
			inLobby = false;
		}
		else if (result == 1)
		{
			std::cout << "Something went really very wrong\n";
		}
	}

	else if (messageID == 7) //Logging out
	{
		int result = recvBuffer.readInt32BE();
		if (result == 0)
		{
			std::cout << "Logged out\n";
			loggedIn = false;
		}
		else if (result == 1)
		{
			std::cout << "Something went really very wrong\n";
		}
	}

	else if (messageID == 8) //Being removed from lobby
	{
		int result = recvBuffer.readInt32BE();
		if (result == 0)
		{
			std::cout << "Host has left the lobby. Returning to main menu\n";
		}
		else if (result == 1)
		{
			std::cout << "Connection error. Exiting lobby\n";
		}
		else if (result == 2)
		{
			std::cout << "You have been kicked from the lobby\n";
		}
		inLobby = false;
	}

	else if (messageID == 11) //Other user joining or leaving lobby
	{
		int newUserLength = recvBuffer.readInt32BE();
		std::string otherUser = recvBuffer.readString(newUserLength);
		int result = recvBuffer.readInt32BE();
		if (result == 0)
		{
			std::cout << otherUser << " has joined the lobby\n";
		}
		else if (result == 1)
		{
			std::cout << otherUser << " has left the lobby\n";
		}
	}

	return 1;
}