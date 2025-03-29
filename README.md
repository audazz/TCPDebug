# TCPDebug

	#pragma once
	
	#include <JuceHeader.h>
	
	//==============================================================================
	// Handy debug macros for quick logging
	#define NDBG(str, num) TCPDebugClient::getGlobalInstance()->debug(String(str) + ": " + String(num))
	#define NDBG_STR(str, val) TCPDebugClient::getGlobalInstance()->debug(String(str) + ": " + String(val))
	#define NDBG_WARN(str, num) TCPDebugClient::getGlobalInstance()->warning(String(str) + ": " + String(num))
	#define NDBG_ERR(str, num) TCPDebugClient::getGlobalInstance()->error(String(str) + ": " + String(num))
	
	//==============================================================================
	// A client class that can be included in your application for remote debugging
	class TCPDebugClient : private Thread
	{
	public:
	    // Singleton access for global debugging
	    static TCPDebugClient* getGlobalInstance()
	    {
	        static TCPDebugClient instance;
	        return &instance;
	    }
    
    TCPDebugClient() : Thread("TCP Debug Client")
    {
        lastConnectionAttempt = Time::getCurrentTime();
    }
    
    // Implement other methods...
};

1. Improved ServerThread Architecture
The new ServerThread.h file contains:

ServerThreadListener: An interface for components to receive server events

clientConnected() - Called when a new client connects
clientDisconnected() - Called when a client disconnects
messageReceived() - Called when a message is received from a client


ClientConnection: A dedicated class for each connected client

Runs in its own thread to handle communication without blocking
Handles reading from and writing to the client socket
Uses non-blocking polling with frequent exit checks
Automatically notifies the listener about events


ServerThread: Main server thread that accepts connections

Creates a new ClientConnection for each incoming connection
Uses non-blocking polling to prevent freezing
Contains proper shutdown mechanisms



2. Updated MainComponent
The MainComponent now:

Implements ServerThreadListener to receive notifications

Keeps track of connected clients
Displays client list in the UI
Logs all client activity


Adds Client Management UI

Shows a list of connected clients
Provides controls to send messages to clients
Updates the UI when clients connect/disconnect


Handles Safe Shutdown

Properly stops and cleans up client connections
Ensures threads are terminated correctly
Prevents memory leaks and crashes



3. Basic Debugging Features
The implementation now supports:

Message Echo: Messages from clients are echoed back
Broadcast Messages: Send messages to all connected clients
Connection Monitoring: Track connections/disconnections

How to Use

Place ServerThread.h and the updated MainComponent.h in your project
Build and run the application
Click "Start Server" to begin listening for connections
Connected clients will appear in the client list
Use the message field to send broadcast messages to all clients

With this implementation, clients can connect and stay connected, exchange messages with the server, and be properly managed through the UI. You now have a solid foundation for building more advanced debugging features.
