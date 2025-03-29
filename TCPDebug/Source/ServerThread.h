#pragma once

#include <JuceHeader.h>
using namespace juce;
// Forward declaration
class ClientConnection;

// Listener interface for server events
class ServerThreadListener
{
public:
    virtual ~ServerThreadListener() = default;
    
    virtual void clientConnected(ClientConnection* client) = 0;
    virtual void clientDisconnected(ClientConnection* client) = 0;
    virtual void messageReceived(ClientConnection* client, const String& message) = 0;
};

// Class to handle a single client connection
class ClientConnection : public Thread
{
public:
    ClientConnection(StreamingSocket* socket, ServerThreadListener* listener)
        : Thread("Client Connection"),
          clientSocket(socket),
          serverListener(listener),
          exitFlag(false)
    {
        // Store the client details
        clientHost = clientSocket->getHostName();
        clientPort = clientSocket->getPort();
        
        // Start the thread automatically
        startThread();
    }
    
    ~ClientConnection() override
    {
        stopThread(1000);
        delete clientSocket;
    }
    
    void run() override
    {
        // Notify listener about the new connection
        serverListener->clientConnected(this);
        
        char buffer[4096];
        
        // Keep reading while connected and not signaled to exit
        while (!threadShouldExit() && !exitFlag && clientSocket->isConnected())
        {
            // Poll with frequent checks for exit
            for (int poll = 0; poll < 10; ++poll)
            {
                if (threadShouldExit() || exitFlag)
                    break;
                
                // Try to read with small timeout
                int bytesRead = clientSocket->read(buffer, sizeof(buffer) - 1, false);
                
                if (bytesRead > 0)
                {
                    // Got data!
                    buffer[bytesRead] = '\0';  // Null-terminate
                    String message = String::fromUTF8(buffer, bytesRead);
                    
                    // Notify listener
                    serverListener->messageReceived(this, message);
                    break;  // Found data, reset poll count
                }
                else if (bytesRead < 0)
                {
                    // Socket error or disconnection
                    goto connectionClosed;
                }
                
                // Short sleep between polls
                sleep(10);
            }
            
            // Slightly longer sleep if no data found in poll loop
            sleep(50);
        }
        
    connectionClosed:
        // Notify listener that this client has disconnected
        serverListener->clientDisconnected(this);
    }
    
    // Send a message to this client
    bool sendMessage(const String& message)
    {
        if (!clientSocket->isConnected())
            return false;
            
        return clientSocket->write(message.toRawUTF8(), message.getNumBytesAsUTF8()) > 0;
    }
    
    // Signal the thread to exit
    void prepareToStop()
    {
        exitFlag = true;
    }
    
    // Get client information
    String getClientHost() const { return clientHost; }
    int getClientPort() const { return clientPort; }
    
    // Get a description of this client
    String getDescription() const { return clientHost + ":" + String(clientPort); }
    
private:
    StreamingSocket* clientSocket;
    ServerThreadListener* serverListener;
    std::atomic<bool> exitFlag;
    String clientHost;
    int clientPort;
};

// Main server thread that listens for connections
class ServerThread : public Thread
{
public:
    ServerThread(StreamingSocket* socket, ServerThreadListener* listener)
        : Thread("TCP Server Thread"), 
          serverSocket(socket),
          serverListener(listener),
          exitFlag(false)
    {
        jassert(serverSocket != nullptr);
        jassert(serverListener != nullptr);
    }
    
    ~ServerThread() override
    {
        // Make sure we're stopped
        signalThreadShouldExit();
        prepareToStop();
        waitForThreadToExit(1000);
    }
    
    void run() override
    {
        while (!threadShouldExit() && !exitFlag)
        {
            // Poll for connections with frequent exit checks
            bool hasNewConnection = false;
            
            for (int i = 0; i < 10; ++i)  // 10 * 5ms = 50ms max wait
            {
                // Check if we should exit
                if (threadShouldExit() || exitFlag)
                    return;  // Clean early return
                
                // Non-blocking check for connection
                auto client = serverSocket->waitForNextConnection();
                
                if (client != nullptr)
                {
                    // New client connection - create a ClientConnection object
                    // The serverListener will be notified by the ClientConnection itself
                    auto* connection = new ClientConnection(client, serverListener);
                    
                    // Don't need to track it here - the listener will handle it
                    hasNewConnection = true;
                    break;
                }
                
                // Very short sleep between polls
                sleep(5);
            }
            
            // If we didn't get a connection, do a slightly longer sleep
            if (!hasNewConnection)
            {
                // Check again before sleeping
                if (threadShouldExit() || exitFlag)
                    return;  // Clean early return
                    
                sleep(20);
            }
        }
    }
    
    // Set flag to ensure thread exits soon
    void prepareToStop()
    {
        exitFlag = true;
    }
    
private:
    StreamingSocket* serverSocket;
    ServerThreadListener* serverListener;
    std::atomic<bool> exitFlag;
};
