#pragma once

#include <JuceHeader.h>
#include "ServerThread.h" // Include our improved ServerThread

class MainComponent : public Component,
                      private Timer,
                      private ServerThreadListener
{
public:
    MainComponent()
        : serverPort(8888),
          serverRunning(false)
    {
        // Server controls
        addAndMakeVisible(portLabel);
        portLabel.setText("Port:", dontSendNotification);
        
        addAndMakeVisible(portField);
        portField.setText(String(serverPort), dontSendNotification);
        
        addAndMakeVisible(startButton);
        startButton.setButtonText("Start Server");
        startButton.onClick = [this]() { startServer(); };
        
        addAndMakeVisible(stopButton);
        stopButton.setButtonText("Stop Server");
        stopButton.onClick = [this]() { stopServer(); };
        stopButton.setEnabled(false);
        
        // Client list
        addAndMakeVisible(clientsLabel);
        clientsLabel.setText("Connected Clients:", dontSendNotification);
        
        addAndMakeVisible(clientListBox);
        clientListBox.setMultiLine(false);
        clientListBox.setReadOnly(true);
        
        // Log display
        addAndMakeVisible(logBox);
        logBox.setMultiLine(true);
        logBox.setReadOnly(true);
        logBox.setCaretVisible(false);
        
        // Message controls
        addAndMakeVisible(messageLabel);
        messageLabel.setText("Send Message:", dontSendNotification);
        
        addAndMakeVisible(messageField);
        messageField.setMultiLine(true);
        messageField.onReturnKey = [this]() { sendMessage(); return true; };
        
        addAndMakeVisible(sendButton);
        sendButton.setButtonText("Send");
        sendButton.onClick = [this]() { sendMessage(); };
        sendButton.setEnabled(false);
        
        // Start timer to process messages (not for connection handling)
        startTimer(100);
        
        setSize(800, 600);
        
        log("Debug server ready. Click Start Server to begin.");
    }

    ~MainComponent() override
    {
        // First, stop the timer
        stopTimer();
        
        // If server is still running, stop it
        if (serverRunning)
        {
            stopServer();
        }
        
        // Clean up any remaining clients
        clientListLock.enter();
        for (auto* client : clientConnections)
        {
            client->prepareToStop();
            client->signalThreadShouldExit();
        }
        
        // Wait briefly for clients to exit
        clientListLock.exit();
        Thread::sleep(100);
        
        // Delete clients
        clientListLock.enter();
        for (auto* client : clientConnections)
        {
            delete client;
        }
        clientConnections.clear();
        clientListLock.exit();
        
        log("Component destroyed");
    }

    void paint(Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        
        // Top row with port and server controls
        auto topRow = bounds.removeFromTop(30);
        portLabel.setBounds(topRow.removeFromLeft(40));
        portField.setBounds(topRow.removeFromLeft(60));
        
        auto buttons = topRow.removeFromRight(200);
        startButton.setBounds(buttons.removeFromLeft(100).reduced(2));
        stopButton.setBounds(buttons.reduced(2));
        
        bounds.removeFromTop(10); // gap
        
        // Client list on the right
        auto rightColumn = bounds.removeFromRight(200);
        clientsLabel.setBounds(rightColumn.removeFromTop(20));
        rightColumn.removeFromTop(5);
        
        auto messageSection = rightColumn.removeFromBottom(150);
        clientListBox.setBounds(rightColumn);
        
        // Message sending controls at bottom right
        messageLabel.setBounds(messageSection.removeFromTop(20));
        messageSection.removeFromTop(5);
        sendButton.setBounds(messageSection.removeFromBottom(30));
        messageSection.removeFromBottom(5);
        messageField.setBounds(messageSection);
        
        // Log box in the main area
        logBox.setBounds(bounds);
    }

private:
    // UI Components
    Label portLabel;
    TextEditor portField;
    TextButton startButton;
    TextButton stopButton;
    Label clientsLabel;
    TextEditor clientListBox;
    TextEditor logBox;
    Label messageLabel;
    TextEditor messageField;
    TextButton sendButton;
    
    // Server components
    std::unique_ptr<StreamingSocket> serverSocket;
    std::unique_ptr<ServerThread> serverThread;
    int serverPort;
    bool serverRunning;
    
    // Client management
    Array<ClientConnection*> clientConnections;
    CriticalSection clientListLock;
    
    // Message queue for UI updates
    Array<String> messageQueue;
    CriticalSection messageLock;
    
    // Logging
    void log(const String& message)
    {
        String timestamped = Time::getCurrentTime().toString(true, true) + ": " + message;
        
        // Add to queue for UI thread to process
        ScopedLock lock(messageLock);
        messageQueue.add(timestamped);
    }
    
    // Update the client list display
    void updateClientList()
    {
        String clientList;
        
        {
            ScopedLock lock(clientListLock);
            for (auto* client : clientConnections)
            {
                clientList += client->getDescription() + "\n";
            }
        }
        
        // This needs to be called on the message thread
        MessageManagerLock mml;
        if (mml.lockWasGained())
        {
            clientListBox.setText(clientList);
            sendButton.setEnabled(!clientConnections.isEmpty());
        }
    }
    
    // Process UI updates
    void timerCallback() override
    {
        // Process any messages in the queue
        Array<String> messages;
        
        {
            ScopedLock lock(messageLock);
            messages.swapWith(messageQueue);
        }
        
        for (const auto& msg : messages)
        {
            logBox.moveCaretToEnd();
            logBox.insertTextAtCaret(msg + "\n");
            logBox.moveCaretToEnd();
        }
    }
    
    // Send a message to all connected clients
    void sendMessage()
    {
        String message = messageField.getText();
        if (message.isEmpty())
            return;
            
        // Clear the message field
        messageField.clear();
        
        // Log what we're sending
        log("Sending to all clients: " + message);
        
        // Send to all clients
        ScopedLock lock(clientListLock);
        for (auto* client : clientConnections)
        {
            if (!client->sendMessage(message))
            {
                log("Failed to send to " + client->getDescription());
            }
        }
    }
    
    // Start the server
    void startServer()
    {
        if (serverRunning)
            return;
            
        // Parse the port
        int port = portField.getText().getIntValue();
        if (port <= 0 || port > 65535)
        {
            log("Invalid port number. Using default port 8888.");
            port = 8888;
            portField.setText("8888", false);
        }
        
        serverPort = port;
        
        // Create the server socket
        serverSocket = std::make_unique<StreamingSocket>();
        
        // Try to create the listener
        if (serverSocket->createListener(serverPort))
        {
            log("Server started on port " + String(serverPort));
            serverRunning = true;
            
            // Update UI
            startButton.setEnabled(false);
            stopButton.setEnabled(true);
            
            // Create and start a separate thread for accepting connections
            // Use explicit ServerThreadListener* cast to ensure correct type matching
            serverThread = std::make_unique<ServerThread>(
                serverSocket.get(),
                static_cast<ServerThreadListener*>(this)
            );
            serverThread->startThread();
        }
        else
        {
            log("Failed to start server on port " + String(serverPort));
            serverSocket.reset();
        }
    }
    
    // Stop the server
    void stopServer()
    {
        // Skip if already stopped
        if (!serverRunning)
            return;
        
        // First, mark as not running
        serverRunning = false;
        
        // Update UI
        startButton.setEnabled(true);
        stopButton.setEnabled(false);
        sendButton.setEnabled(false);
        
        log("Stopping server...");
        
        // First, close the socket so no new connections can come in
        if (serverSocket != nullptr)
        {
            log("Closing server socket...");
            serverSocket->close();
            log("Server socket closed");
        }
        
        // Stop the server thread
        if (serverThread != nullptr)
        {
            log("Preparing thread to stop...");
            serverThread->prepareToStop();
            serverThread->signalThreadShouldExit();
            
            // Give it a reasonable time to exit
            log("Waiting for thread to exit...");
            if (serverThread->waitForThreadToExit(1000))
            {
                log("Thread exited cleanly");
            }
            else
            {
                log("WARNING: Thread did not exit in time");
            }
            
            // Set to nullptr to prevent any further access
            serverThread = nullptr;
        }
        
        // Stop all client connections
        {
            ScopedLock lock(clientListLock);
            
            log("Disconnecting " + String(clientConnections.size()) + " clients...");
            
            // Signal all clients to stop
            for (auto* client : clientConnections)
            {
                client->prepareToStop();
                client->signalThreadShouldExit();
            }
        }
        
        // Wait briefly for clients to exit
        Thread::sleep(100);
        
        // Clean up client connections
        {
            ScopedLock lock(clientListLock);
            
            // Delete all clients
            for (auto* client : clientConnections)
            {
                delete client;
            }
            clientConnections.clear();
            
            // Update the client list
            updateClientList();
        }
        
        // Now null the socket
        serverSocket = nullptr;
        
        log("Server stopped");
    }
    
    // ServerThreadListener implementation
    void clientConnected(ClientConnection* client) override
    {
        {
            ScopedLock lock(clientListLock);
            clientConnections.add(client);
        }
        
        log("Client connected: " + client->getDescription());
        updateClientList();
    }
    
    void clientDisconnected(ClientConnection* client) override
    {
        String description = client->getDescription();
        
        {
            ScopedLock lock(clientListLock);
            clientConnections.removeFirstMatchingValue(client);
        }
        
        log("Client disconnected: " + description);
        updateClientList();
        
        // Note: We don't delete the client here because it's still executing
        // It will delete itself when its thread exits
    }
    
    void messageReceived(ClientConnection* client, const String& message) override
    {
        log("From " + client->getDescription() + ": " + message);
        
        // Echo the message back to the client
        client->sendMessage("Echo: " + message);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
