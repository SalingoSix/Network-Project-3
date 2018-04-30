#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <future>
#include <errno.h> 
#include <sys/types.h> 
#include <map> 
#include <ctime>
#include <cstdlib>

#include "buffer.h"

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

#define TRUE   1 
#define FALSE  0 
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

SOCKET masterSocket = INVALID_SOCKET;
SOCKET clientSocket = INVALID_SOCKET;

int iResult;
char recvbuf[DEFAULT_BUFLEN];
int recvbuflen = DEFAULT_BUFLEN;

struct user
{
	SOCKET socketDescriptor;
	std::string username;
};

struct lobby
{
	user hostName;
	std::string mapName;
	std::string modeName;
	std::string lobbyName;
	int currentPlayers;
	std::vector<user> playersInLobby;
	int maxCapacity;
};

std::vector<lobby> gVecLobbies;
std::map<int, int> gMapClientToRequests;
std::vector<user*> gVecClientSockets;
buffer sendBuffer(512);
buffer recvBuffer(512);

void processMessage(user* client);
void closeClientConnection(SOCKET clientSocket, fd_set &readfds);
bool closeLobby(int lobbyID);
bool userJoinOrLeave(std::string username, int option, int lobbyIndex);

int main(int argc, char *argv[])
{
	int opt = TRUE;
	int activity, sd;
	int max_sd;

	WSADATA wsaData;
	int iResult;


	struct addrinfo *result = NULL;
	struct addrinfo hints;

	fd_set readfds;

	//a message 
	char *message = "Welcome to the wonderful game lobby. Sign in, or create an account";

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	masterSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (masterSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	//set master socket to allow multiple connections
	if (setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	// Setup the TCP listening socket
	iResult = bind(masterSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(masterSocket);
		WSACleanup();
		return 1;
	}

	iResult = listen(masterSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(masterSocket);
		WSACleanup();
		return 1;
	}

	//accept the incoming connection 
	//addrlen = sizeof(address);
	puts("Waiting for connections ...");

	while (TRUE)
	{
		//clear the socket set 
		FD_ZERO(&readfds);

		//add master socket to set 
		FD_SET(masterSocket, &readfds);
		max_sd = masterSocket;


		//add child sockets to set 
		for (int index = 0; index < gVecClientSockets.size(); index++)
		{
			//socket descriptor 
			sd = gVecClientSockets[index]->socketDescriptor;

			//if valid socket descriptor then add to read list 
			if (sd > 0)
				FD_SET(sd, &readfds);

			//highest file descriptor number, need it for the select function 
			if (sd > max_sd)
				max_sd = sd;
		}

		//wait for an activity on one of the sockets , timeout is NULL , 
		//so wait indefinitely 
		activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

		if ((activity < 0) && (errno != EINTR))
		{
			printf("select error");
		}

		//If something happened on the master socket , 
		//then its an incoming connection 
		if (FD_ISSET(masterSocket, &readfds))
		{
			clientSocket = accept(masterSocket, NULL, NULL);
			if (clientSocket == INVALID_SOCKET)
			{
				printf("accept failed with error: %d\n", WSAGetLastError());
			}

			//add new socket to array of sockets 
			user* tmpUser = new user;
			tmpUser->socketDescriptor = clientSocket;
			tmpUser->username = "";
			gVecClientSockets.push_back(tmpUser);
			printf("Adding to list of sockets to Vector \n");
			clientSocket = INVALID_SOCKET;
		}

		//Check each client for incoming data
		for (int index = 0; index < gVecClientSockets.size(); index++)
		{
			sd = gVecClientSockets[index]->socketDescriptor;

			if (FD_ISSET(sd, &readfds))
			{
				iResult = recv(sd, recvbuf, recvbuflen, 0);
				if (iResult > 0) 
				{	//Collect all data from the buffer
					printf("Bytes received: %d\n", iResult);
					std::string getData = "";
					for (int i = 0; i < 4; i++)
						getData += recvbuf[i];
					recvBuffer.writeString(getData);
					recvBuffer.displayIndices();
					int recvSize = recvBuffer.readInt32BE();
					recvBuffer.displayIndices();
					for (int i = 4; i < recvSize; i++)
						getData += recvbuf[i];
					recvBuffer.writeString(getData);
					processMessage(gVecClientSockets[index]);
				}
				else if (iResult == 0) {
					closeClientConnection(sd, readfds);
				}
				else {
					printf("recv failed with error: %d\n", WSAGetLastError());
					closeClientConnection(sd, readfds);
					//return 1;
				}
			}
		}
	}

	WSACleanup();
	return 0;
}

void processMessage(user* client)
{
	int packetLength = recvBuffer.readInt32BE();
	int messageID = recvBuffer.readInt32BE();

	std::cout << messageID << std::endl;

	if (messageID == 1) //Creating a lobby
	{
		int mapLength = recvBuffer.readInt32BE();
		std::string map = recvBuffer.readString(mapLength);
		int modeLength = recvBuffer.readInt32BE();
		std::string mode = recvBuffer.readString(modeLength);
		int roomLength = recvBuffer.readInt32BE();
		std::string roomName = recvBuffer.readString(roomLength);
		
		//Checking if the name exists
		int roomExists = 0;	
		for (int index = 0; index < ::gVecLobbies.size(); index++)
		{
			if (::gVecLobbies[index].lobbyName == roomName)
			{
				roomExists = 1;
				break;
			}
		}

		if (!roomExists)
		{	//Making a new lobby, and pushing it into the vector
			lobby newLobby;
			newLobby.currentPlayers = 1;
			newLobby.hostName = *client;
			newLobby.lobbyName = roomName;
			newLobby.mapName = map;
			newLobby.modeName = mode;

			if (mode == "TD")
				newLobby.maxCapacity = 8;
			else if (mode == "FFA")
				newLobby.maxCapacity = 20;
			else if (mode == "BV")
				newLobby.maxCapacity = 6;

			newLobby.playersInLobby.push_back(*client);
			::gVecLobbies.push_back(newLobby);
		}

		packetLength = 12;
		sendBuffer.writeInt32BE(packetLength);
		sendBuffer.writeInt32BE(messageID);
		sendBuffer.writeInt32BE(roomExists);

		std::string sendString = sendBuffer.readString(packetLength);
		int sendLength = sendString.length();

		int iResult = send(client->socketDescriptor, sendString.c_str(), sendString.length(), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(clientSocket);
			WSACleanup();
			return;
		}

	}

	else if (messageID == 2) //Joining a lobby
	{
		int nameLength = recvBuffer.readInt32BE();
		std::string lobbyName = recvBuffer.readString(nameLength);

		int result = 0;
		//Checking if the name exists
		int roomID = -1;
		for (int index = 0; index < ::gVecLobbies.size(); index++)
		{
			if (::gVecLobbies[index].lobbyName == lobbyName)
			{
				roomID = index;
				break;
			}
		}

		if (roomID == -1)
		{
			result = 2;
		}

		else
		{
			if (::gVecLobbies[roomID].maxCapacity == ::gVecLobbies[roomID].currentPlayers)
			{
				result = 1;
			}
			else
			{
				::gVecLobbies[roomID].currentPlayers++;
				::gVecLobbies[roomID].playersInLobby.push_back(*client);
				userJoinOrLeave(client->username, 0, roomID);
			}
		}

		packetLength = 12;
		sendBuffer.writeInt32BE(packetLength);
		sendBuffer.writeInt32BE(messageID);
		sendBuffer.writeInt32BE(result);

		std::string sendString = sendBuffer.readString(packetLength);
		int sendLength = sendString.length();

		int iResult = send(client->socketDescriptor, sendString.c_str(), sendString.length(), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(clientSocket);
			WSACleanup();
			return;
		}

	}

	else if (messageID == 3) //Refreshing lobby info
	{
		int numLobbies = ::gVecLobbies.size();
		int packetLength = 12;
		sendBuffer.writeInt32BE(packetLength); //Temporary, until we determine the real size
		sendBuffer.writeInt32BE(messageID);
		sendBuffer.writeInt32BE(numLobbies);
		//Pick up all information about each lobby in order
		for (int index = 0; index < ::gVecLobbies.size(); index++)
		{
			int mapLength = ::gVecLobbies[index].mapName.length();
			packetLength += mapLength + 4;
			sendBuffer.writeInt32BE(mapLength);
			sendBuffer.writeString(::gVecLobbies[index].mapName);

			int nameLength = ::gVecLobbies[index].lobbyName.length();
			packetLength += nameLength + 4;
			sendBuffer.writeInt32BE(nameLength);
			sendBuffer.writeString(::gVecLobbies[index].lobbyName);

			int modeLength = ::gVecLobbies[index].modeName.length();
			packetLength += modeLength + 4;
			sendBuffer.writeInt32BE(modeLength);
			sendBuffer.writeString(::gVecLobbies[index].modeName);

			int hostLength = ::gVecLobbies[index].hostName.username.length();
			packetLength += hostLength + 4;
			sendBuffer.writeInt32BE(hostLength);
			sendBuffer.writeString(::gVecLobbies[index].hostName.username);

			packetLength += 8;

			sendBuffer.writeInt32BE(::gVecLobbies[index].currentPlayers);
			sendBuffer.writeInt32BE(::gVecLobbies[index].maxCapacity);
		}
		//Rewrite the packetLength, now we have the final number
		sendBuffer.writeInt32BE(0, packetLength);

		std::string sendString = sendBuffer.readString(packetLength);
		int sendLength = sendString.length();

		int iResult = send(client->socketDescriptor, sendString.c_str(), sendString.length(), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(clientSocket);
			WSACleanup();
			return;
		}

	}

	else if (messageID == 4) //Creating an account
	{
		int userLength = recvBuffer.readInt32BE();
		std::string username = recvBuffer.readString(userLength);
		int passLength = recvBuffer.readInt32BE();
		std::string password = recvBuffer.readString(passLength);

		int packetLength = userLength + passLength + 20;
		//9 if messageID was 4, 10 if it was 5
		messageID = 9;
		int userSocket = client->socketDescriptor;

		sendBuffer.writeInt32BE(packetLength);
		sendBuffer.writeInt32BE(messageID);
		sendBuffer.writeInt32BE(userSocket);
		sendBuffer.writeInt32BE(userLength);
		sendBuffer.writeString(username);
		sendBuffer.writeInt32BE(passLength);
		sendBuffer.writeString(password);

		std::string sendString = sendBuffer.readString(packetLength);
		int sendLength = sendString.length();
		std::cout << sendString << std::endl;

		int iResult = send(::gVecClientSockets[0]->socketDescriptor, sendString.c_str(), sendString.length(), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(clientSocket);
			WSACleanup();
			return;
		}
		std::cout << "Now we're done\n";
	}

	else if (messageID == 5) //Creating or logging in to an account
	{
		int userLength = recvBuffer.readInt32BE();
		std::string username = recvBuffer.readString(userLength);
		int passLength = recvBuffer.readInt32BE();
		std::string password = recvBuffer.readString(passLength);

		//Checking if the name exists
		for (int index = 0; index < ::gVecClientSockets.size(); index++)
		{
			if (::gVecClientSockets[index]->username == username)
			{
				sendBuffer.writeInt32BE(12);
				sendBuffer.writeInt32BE(5);
				sendBuffer.writeInt32BE(3);

				std::string sendString = sendBuffer.readString(packetLength);
				int sendLength = sendString.length();

				int iResult = send(client->socketDescriptor, sendString.c_str(), sendString.length(), 0);
				if (iResult == SOCKET_ERROR) {
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(clientSocket);
					WSACleanup();
					return;
				}
				return;
			}
		}

		int packetLength = userLength + passLength + 20;
		//Sending ID 10 to the AuthServer
		messageID = 10;
		int userSocket = client->socketDescriptor;

		sendBuffer.writeInt32BE(packetLength);
		sendBuffer.writeInt32BE(messageID);
		sendBuffer.writeInt32BE(userSocket);
		sendBuffer.writeInt32BE(userLength);
		sendBuffer.writeString(username);
		sendBuffer.writeInt32BE(passLength);
		sendBuffer.writeString(password);

		std::string sendString = sendBuffer.readString(packetLength);
		int sendLength = sendString.length();

		int iResult = send(::gVecClientSockets[0]->socketDescriptor, sendString.c_str(), sendString.length(), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(clientSocket);
			WSACleanup();
			return;
		}

	}

	else if (messageID == 6) //Exiting a lobby
	{
		for (int lobbyIndex = 0; lobbyIndex < ::gVecLobbies.size(); lobbyIndex++)
		{
			for (int playerIndex = 0; playerIndex < ::gVecLobbies[lobbyIndex].currentPlayers; playerIndex++)
			{
				if (client->username == ::gVecLobbies[lobbyIndex].playersInLobby[playerIndex].username)
				{
					if (client->username == ::gVecLobbies[lobbyIndex].hostName.username)
					{
						closeLobby(lobbyIndex);
					}
					else
					{
						userJoinOrLeave(client->username, 1, lobbyIndex);
						::gVecLobbies[lobbyIndex].playersInLobby.erase(::gVecLobbies[lobbyIndex].playersInLobby.begin() + playerIndex);
						::gVecLobbies[lobbyIndex].currentPlayers--;
					}

					int packetLength = 12;
					int result = 0;

					sendBuffer.writeInt32BE(packetLength);
					sendBuffer.writeInt32BE(messageID);
					sendBuffer.writeInt32BE(result);

					std::string sendString = sendBuffer.readString(packetLength);
					int sendLength = sendString.length();

					int iResult = send(client->socketDescriptor, sendString.c_str(), sendString.length(), 0);
					if (iResult == SOCKET_ERROR) {
						printf("send failed with error: %d\n", WSAGetLastError());
						closesocket(clientSocket);
						WSACleanup();
						return;
					}
					return;
				}
			}
		}
		//Should never make it out here, but will send an error back to user if we do
		int packetLength = 12;
		int result = 1;

		sendBuffer.writeInt32BE(packetLength);
		sendBuffer.writeInt32BE(messageID);
		sendBuffer.writeInt32BE(result);

		std::string sendString = sendBuffer.readString(packetLength);
		int sendLength = sendString.length();

		int iResult = send(client->socketDescriptor, sendString.c_str(), sendString.length(), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(clientSocket);
			WSACleanup();
			return;
		}
	}

	else if (messageID == 7) //Logging out
	{
		//Kick the user from their lobby if they're in one
		for (int index = 0; index < ::gVecClientSockets.size(); index++)
		{	//Check each lobby to see if the user is in it
				for (int lobbyIndex = 0; lobbyIndex < ::gVecLobbies.size(); lobbyIndex++)
				{
					for (int playerIndex = 0; playerIndex < ::gVecLobbies[lobbyIndex].currentPlayers; playerIndex++)
					{
						if (client->username == ::gVecLobbies[lobbyIndex].playersInLobby[playerIndex].username)
						{	//if the player is in the lobby, check if they are the host
							if (client->username == ::gVecLobbies[lobbyIndex].hostName.username)
							{
								closeLobby(lobbyIndex);
							}
							else
							{
								std::string leavingName = ::gVecLobbies[lobbyIndex].playersInLobby[playerIndex].username;
								userJoinOrLeave(leavingName, 1, lobbyIndex);
								::gVecLobbies[lobbyIndex].playersInLobby.erase(::gVecLobbies[lobbyIndex].playersInLobby.begin() + playerIndex);
								::gVecLobbies[lobbyIndex].currentPlayers--;
							}
							break;
						}
					}
				}
		}
		client->username = "";

		int packetLength = 12;
		int result = 0;

		sendBuffer.writeInt32BE(packetLength);
		sendBuffer.writeInt32BE(messageID);
		sendBuffer.writeInt32BE(result);

		std::string sendString = sendBuffer.readString(packetLength);
		int sendLength = sendString.length();

		int iResult = send(client->socketDescriptor, sendString.c_str(), sendString.length(), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(clientSocket);
			WSACleanup();
			return;
		}
	}

	else if (messageID == 9) //Creating account (sent from Auth. Server)
	{
		int userSocket = recvBuffer.readInt32BE();
		int result = recvBuffer.readInt32BE();
		//Not using them here, but it simplifies the code in the auth server
		int userLength = recvBuffer.readInt32BE();
		recvBuffer.readString(userLength);

		int packetLength = 12;
		int messageID = 4;

		sendBuffer.writeInt32BE(packetLength);
		sendBuffer.writeInt32BE(messageID);
		sendBuffer.writeInt32BE(result);

		std::string sendString = sendBuffer.readString(packetLength);
		int sendLength = sendString.length();

		int iResult = send(userSocket, sendString.c_str(), sendString.length(), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(clientSocket);
			WSACleanup();
			return;
		}
	}

	else if (messageID == 10) //Logging in (sent from Auth. Server)
	{
		int userSocket = recvBuffer.readInt32BE();
		int result = recvBuffer.readInt32BE();
		int userLength = recvBuffer.readInt32BE();
		std::string username = recvBuffer.readString(userLength);

		int packetLength = 12;
		int messageID = 5;

		for (int index = 0; index < ::gVecClientSockets.size(); index++)
		{
			if (::gVecClientSockets[index]->socketDescriptor == userSocket)
			{
				::gVecClientSockets[index]->username = username;
				break;
			}
		}

		sendBuffer.writeInt32BE(packetLength);
		sendBuffer.writeInt32BE(messageID);
		sendBuffer.writeInt32BE(result);

		std::string sendString = sendBuffer.readString(packetLength);
		int sendLength = sendString.length();

		int iResult = send(userSocket, sendString.c_str(), sendString.length(), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(clientSocket);
			WSACleanup();
			return;
		}
	}

	return;
}

void closeClientConnection(SOCKET theClientSocket, fd_set &readfds) 
{
	int iResult = closesocket(theClientSocket);
	if (iResult == SOCKET_ERROR)
		printf("Error in closing socket...\n");
	else
		printf("Socket connection closed...\n");
	FD_CLR(theClientSocket, &readfds);
	printf("Removed Socket from FD SET...\n");
	for (int index = 0; index < ::gVecClientSockets.size(); index++) 
	{
		if (::gVecClientSockets[index]->socketDescriptor == theClientSocket) 
		{	//Check each lobby to see if the user is in it
			for (int lobbyIndex = 0; lobbyIndex < ::gVecLobbies.size(); lobbyIndex++)
			{
				for (int playerIndex = 0; playerIndex < ::gVecLobbies[lobbyIndex].currentPlayers; playerIndex++)
				{	
					if (::gVecClientSockets[index]->username == ::gVecLobbies[lobbyIndex].playersInLobby[playerIndex].username)
					{	//if the player is in the lobby, check if they are the host
						if (::gVecClientSockets[index]->username == ::gVecLobbies[lobbyIndex].hostName.username)
						{
							closeLobby(lobbyIndex);
						}
						else
						{
							std::string leavingName = ::gVecLobbies[lobbyIndex].playersInLobby[playerIndex].username;
							userJoinOrLeave(leavingName, 1, lobbyIndex);
							::gVecLobbies[lobbyIndex].playersInLobby.erase(::gVecLobbies[lobbyIndex].playersInLobby.begin() + playerIndex);
							::gVecLobbies[lobbyIndex].currentPlayers--;
						}
						break;
					}
				}
			}
			::gVecClientSockets.erase(::gVecClientSockets.begin() + index);
			break;
		}
	}

}

bool closeLobby(int lobbyID)
{
	int packetLength = 12;
	int messageID = 8;
	int result = 0;
	sendBuffer.writeInt32BE(packetLength);
	sendBuffer.writeInt32BE(messageID);
	sendBuffer.writeInt32BE(result);

	std::string sendString = sendBuffer.readString(packetLength);
	int sendLength = sendString.length();

	for (int index = 1; index < ::gVecLobbies[lobbyID].currentPlayers; index++)
	{
		int theSocket = ::gVecLobbies[lobbyID].playersInLobby[index].socketDescriptor;
		int iResult = send(theSocket, sendString.c_str(), sendString.length(), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(clientSocket);
			WSACleanup();
			return 0;
		}
	}
	::gVecLobbies.erase(::gVecLobbies.begin() + lobbyID);
}

bool userJoinOrLeave(std::string username, int option, int lobbyIndex)
{
	int userLength = username.length();
	int messageID = 11;
	int packetLength = 16 + userLength;

	sendBuffer.writeInt32BE(packetLength);
	sendBuffer.writeInt32BE(messageID);
	sendBuffer.writeInt32BE(userLength);
	sendBuffer.writeString(username);
	sendBuffer.writeInt32BE(option);

	std::string sendString = sendBuffer.readString(packetLength);
	int sendLength = sendString.length();

	for (int index = 0; index < ::gVecLobbies[lobbyIndex].currentPlayers; index++)
	{
		int theSocket = ::gVecLobbies[lobbyIndex].playersInLobby[index].socketDescriptor;
		int iResult = send(theSocket, sendString.c_str(), sendString.length(), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(clientSocket);
			WSACleanup();
			return 0;
		}
	}
	return 1;
}