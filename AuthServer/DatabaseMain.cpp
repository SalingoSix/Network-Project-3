#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include <conio.h>
#include <algorithm>

#include <cppconn\driver.h>
#include <cppconn\exception.h>
#include <cppconn\resultset.h>
#include <cppconn\statement.h>
#include <cppconn\prepared_statement.h>

#include "buffer.h"
#include "sha256.h"

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

//Buffers we'll be storing data in
buffer sendBuffer(512);
buffer recvBuffer(512);

SOCKET ConnectSocket = INVALID_SOCKET;
int iResult;

//A random pool of characters to create a salt out of
static const char alphanum[] =
"0123456789"
"!@#$%^&*"
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz";

int stringLength = sizeof(alphanum) - 1;

std::string gUserId = "";
int welcomeMessage = 0;

//MySQL things
sql::Driver* myDriver;
sql::Connection* myConn;
sql::Statement* statement;
sql::PreparedStatement* prepState;

std::string genSalt()
{
	std::string mySalt;

	for (int i = 0; i < 64; i++)
	{
		//int rando = rand() % 256;	Apparently, SQL doesn't like weird characters...
		//char c = char(rando);
		char c = alphanum[rand() % stringLength];
		mySalt += c;
	}

	return mySalt;
}

std::string hashPassword(std::string salt, std::string pass)
{
	std::string toHash = salt + pass;
	std::string output1 = sha256(toHash);
	return output1;
}

int createNewUser(std::string username, std::string password)
{
	if (password == "") //Some examples of invalid passwords, null and any string with spaces in it
		return 2;

	for (int i = 0; i < password.length(); i++)
	{
		if (password[i] == ' ')
			return 2;
	}

	//Check if the given username is already in the database
	std::string myQuery = "SELECT * FROM accounts WHERE username = '" + username + "';";
	sql::ResultSet* result;
	int numUpdates;

	try
	{
		prepState = myConn->prepareStatement(myQuery);
		result = prepState->executeQuery();
	}
	catch (sql::SQLException &exception)
	{
		std::cout << "Unable to query the database\n";
		return 2;
	}

	if (result->rowsCount() > 0) //This username was found in the database
	{
		return 1;
	}

	//Generate a salt and hash
	std::string mySalt = genSalt();

	std::cout << "\n";
	std::string myHash = hashPassword(mySalt, password);

	//Finally, put all the data we've generated into the accounts database
	std::string toExecute = "INSERT INTO accounts(username, salt, hashed_pass, last_login) VALUES ('" + username + "', '" + mySalt + "', '" + myHash + "', now());";

	try
	{
		prepState = myConn->prepareStatement(toExecute);
		numUpdates = prepState->executeUpdate();
	}
	catch (sql::SQLException &exception)
	{
		std::cout << "Unable to update the database\n";
		return 2;
	}

	if (numUpdates == 0)
	{
		return 3;
	}

	//gUserId = newID;
	return 0;
}

int loginUser(std::string username, std::string password)
{
	//Look for the given username in the database before anything
	std::string myQuery = "SELECT * FROM accounts WHERE username = '" + username + "';";
	sql::ResultSet* result;
	int numUpdates;

	try
	{
		prepState = myConn->prepareStatement(myQuery);
		result = prepState->executeQuery();
	}
	catch (sql::SQLException &exception)
	{
		std::cout << "Unable to query the database\n";
		return 2;
	}

	std::string userSalt, userHash, userID;


	if (result->rowsCount() == 0) //This username doesn't exist in the database
	{
		return 1;
	}

	else if (result->rowsCount() > 1) //The username exists more than once, so something is wrong.
	{
		return 2;
	}

	while (result->next())
	{
		userSalt = result->getString(3);
		userHash = result->getString(4);
		userID = result->getString(1);
	}

	std::string testHash = hashPassword(userSalt, password);

	if (testHash == userHash)
	{
		myQuery = "UPDATE accounts SET last_login = now() WHERE id = " + userID + ';';

		try
		{
			prepState = myConn->prepareStatement(myQuery);
			numUpdates = prepState->executeUpdate();
		}
		catch (sql::SQLException &exception)
		{
			std::cout << "Unable to update the database\n";
			return 2;
		}

		return 0;
	}

	else
		return 1;

}

bool sendMessage(std::string message, int sendLength)
{
	iResult = send(ConnectSocket, message.c_str(), sendLength, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 0;
	}

	return 1;
}

bool processMessage()
{
	std::cout << "Message Received";
	int packetLength = recvBuffer.readInt32BE();
	int messageID = recvBuffer.readInt32BE();
	int userSocket = recvBuffer.readInt32BE();
	int nameLength = recvBuffer.readInt32BE();
	std::string username = recvBuffer.readString(nameLength);
	int passLength = recvBuffer.readInt32BE();
	std::string password = recvBuffer.readString(passLength);
	int processResult = -1;
	std::cout << messageID << std::endl;
	if (messageID == 9)
	{
		processResult = createNewUser(username, password);
	}
	else if (messageID == 10)
	{
		processResult = loginUser(username, password);
	}

	int sendLength = 20 + nameLength;
	sendBuffer.writeInt32BE(sendLength);
	sendBuffer.writeInt32BE(messageID);
	sendBuffer.writeInt32BE(userSocket);
	sendBuffer.writeInt32BE(processResult);
	sendBuffer.writeInt32BE(nameLength);
	sendBuffer.writeString(username);

	std::string sendString = sendBuffer.readString(sendLength);

	sendMessage(sendString, sendLength);

	return 1;
}



int main(int argc, char **argv)
{
	try
	{
		myDriver = get_driver_instance();
		myConn = myDriver->connect("127.0.0.1:3306", "root", "root");
		myConn->setSchema("authentication");
	}
	catch (sql::SQLException &exception)
	{
		std::cout << "Could not connect to the server\n";
		return 1;
	}

	using namespace std::literals;
	WSADATA wsaData;

	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;

	// Validate the parameters
	if (argc != 2) {
		printf("usage: %s server-name\n", argv[0]);
		return 1;
	}

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
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
		return 1;
	}

	srand(time(NULL));

	std::string getData = "";

	while (true)
	{

		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0)
		{
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
			processMessage();
		}
		else if (iResult == 0)
			continue;
		else
			continue;
	}

	return 0;
}