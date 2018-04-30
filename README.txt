Project by Alex Medos

Import the table 'accounts' from FinalProject.sql

To run the project, first run GameServer in Visual Studio, in 32bit Debug mode.

In the project files, navigate to Networking_Final/Debug, and run AuthServer.exe in a command window
Now the server should be prepared to make connections with the clients.

The Authentication server needs the server IP as an argument, so run it as:
AuthServer.exe 127.0.0.1 (unless you're running your server remotely, of course)

In the same folder as above, run GameClient.exe in another command window. No arguments required.
You can run several GameClient applications at once. The server should handle it all.

To connect to the server type in GameClient:
/connect 127.0.0.1 27015
The user has to type in the IP address and port number that the server is on

From here, the user must log in.

To register a new username:
/register username password

And to login:
/login username password

Once you're logged in, you have access to all the rest of the commands

To see a list of lobbies:
/refresh
A lobby list is also generated automatically upon logging in

To get info on maps and game modes:
/create ?

And to create a lobby:
/create map mode roomName

To join a lobby:
/join roomName
As long as the room isn't full. You'll be stopped if you're already in a room

To exit the lobby you're in:
/leave
If the host leaves a lobby, everyone gets kicked, and the lobby gets deleted

To log out from your current account:
/logout
After logging out, you can log back in with the same, or a different user name

To close the client:
/exit
