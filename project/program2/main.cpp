#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

class Buffer {
private:
    std::string buffer; 
    std::mutex mutexSync;
    std::condition_variable conditionVariable;
    bool dataFlag = false;
    bool terminateFlag = false;

public:
    void push(const std::string& data) {
        std::unique_lock<std::mutex> lock(mutexSync);
        buffer = data;
        dataFlag = true;
        conditionVariable.notify_one();
    }

    bool pop(std::string& data) {
        std::unique_lock<std::mutex> lock(mutexSync);
        conditionVariable.wait(lock, [this] { return dataFlag || terminateFlag; });
        if (terminateFlag) return false;
        data = buffer;
        buffer.clear();
        dataFlag = false;
        return true;
    }

    void terminate() {
        std::unique_lock<std::mutex> lock(mutexSync);
        terminateFlag = true;
        conditionVariable.notify_all();
    }
};

void processorThread(Buffer& buffer) {
    while (true) {
        std::string data;
        if (!buffer.pop(data)) break;

        if (data.length() > 2) {
            if (data.length() % 32 == 0) {
                std::cout << "Received data: " << data << std::endl;
            }
            else {
                std::cerr << "Error: Data length is not a multiple of 32." << std::endl;
            }
        }
        else {
            std::cerr << "Error: Data length is less than 3 characters." << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void socket_thread(Buffer& buffer) {
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char bufferArray[1024] = { 0 };

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return;
    }

    while ((valread = read(sock, bufferArray, 1024)) > 0) {
        std::string receivedData(bufferArray, valread);
        buffer.push(receivedData);
        std::cout << "Received data: " << receivedData << std::endl;
    }

    close(sock);
}

int main() {
    Buffer sharedBuffer;

    std::thread processor(processorThread, std::ref(sharedBuffer));
    std::thread socketThread(socket_thread, std::ref(sharedBuffer));

    std::cout << "Program 2 started. Waiting for data..." << std::endl;

    std::cout << "Enter 'exit' to terminate the program." << std::endl;
    std::string command;
    while (std::cin >> command && command != "exit") {}

    sharedBuffer.terminate();
    socketThread.join();
    processor.join();

    return 0;
}
