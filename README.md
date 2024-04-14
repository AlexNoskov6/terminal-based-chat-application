# Terminal-Based Chat System

The Terminal-Based Chat System is a command-line application that allows users to communicate with each other in real-time through a terminal interface. It provides a simple and efficient way for users to exchange messages and stay connected.

## Features

- User registration: Users can create an account to access the chat system.
- Login/Logout: Registered users can log in to their accounts to start chatting. If a client is logged-out while receiving messages, they will see the messages upon login.
- Real-time messaging: Users can send and receive messages instantly.
- User presence: Users can see the online status of other users.
- Private messaging: Users can send private messages to specific users.
- Broadcast: Users can broadcast messages to all connected hosts
- Notifications: Users receive notifications for new messages and other important events.
- Command-line interface: The chat system is designed to be used in a terminal environment.
- Server-side functionality: The server keeps track of all the things that are going on in the chat, it can provide lists of currently logged-in clients, statistics, and blocked lists


## Usage

1. Compile the code on machines that are going to be used for communications
2. Decide which computer will serve as a server and launch the application with ./app s 'desired port number'
2. use ./app c 'desired port number' to launch the app on clients
3. Start sending and receiving messages with other users with this set of commands: LOGIN 'SERVER IP' 'SERVER PORT' (client-side command only), SEND 'IP' 'MESSAGE', BROADCAST 'MESSAGE', LIST (will list currently logged-in clients), STATISTICS (server-side command only), LOGOUT (client-side command only), IP, PORT, EXIT (client-side).

## Contributing

Contributions are welcome! If you have any ideas, suggestions, or bug reports, please open an issue or submit a pull request. (Note: commands like BLOCK, UNBLOCK, REFRESH are still in the work).