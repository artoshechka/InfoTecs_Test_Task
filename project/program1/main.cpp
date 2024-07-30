#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <algorithm>
#include <cctype>
#include <numeric>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 8080

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

bool isValidInput(const std::string& inputString) {
    return (inputString.length() <= 64 && all_of(inputString.begin(), inputString.end(), isdigit));
}

std::string orderAndRefactorInput(std::string inputString) {
    std::sort(inputString.begin(), inputString.end(), std::greater<char>());
    for (size_t i = 0; i < inputString.size(); ++i) {
        if ((inputString[i] - '0') % 2 == 0) {
            inputString[i] = 'K';
            inputString.insert(i + 1, 1, 'B');
            ++i;
        }
    }
    return inputString;
}

void readerThread(Buffer& buffer) {
    std::string inputString;
    while (true) {
        std::cout << "Enter string: ";
        std::getline(std::cin, inputString);

        if (inputString == "exit") break;

        if (isValidInput(inputString)) {
            std::string processedInput = orderAndRefactorInput(inputString);
            buffer.push(processedInput);
        }
        else {
            std::cerr << "Input error: String must contain only digits and its length must be lower or equal to 64" << std::endl;
        }
    }

    buffer.terminate();
}

int calculateSum(const std::string& input) {
    int sum = 0;
    for (char c : input) {
        if (isdigit(c)) {
            sum += c - '0';
        }
    }
    return sum;
}

void summaryThread(Buffer& buffer, int clientSocket) {
    while (true) {
        std::string data;
        if (!buffer.pop(data)) break;

        std::cout << "Getted data: " << data << std::endl;
        int sum = calculateSum(data);
        std::cout << "Digits' summary: " << sum << std::endl;
        send(clientSocket, data.c_str(), data.size(), 0);
    }
    close(clientSocket);
}

int main() {
    Buffer buffer;

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::thread reader(readerThread, std::ref(buffer));

    std::cout << "Waiting for a connection..." << std::endl;

    if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    std::thread summary(summaryThread, std::ref(buffer), new_socket);

    reader.join();
    summary.join();

    close(server_fd);
    return 0;
}