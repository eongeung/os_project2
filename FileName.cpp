#include <iostream>
#include <deque>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <mutex>
#include <string>
#include <future>

// Process 클래스 정의
class Process {
public:
    int id;
    char type; // 'F' for Foreground, 'B' for Background
    bool promoted;

    Process(int id, char type) : id(id), type(type), promoted(false) {}
};

//QueueManager 클래스 정의
class QueueManager {
private:
    std::deque<Process*> dynamic_queue;
    std::deque<std::pair<Process*, int>> wait_queue;
    Process* runningProcess = nullptr;
    size_t promoteIndex;
    std::mutex mtx;

//정수를 문자열로 변환하는 함수
    std::string to_string(int value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }

//옵션과 함께 명령어를 실행하는 함수
    void runCommandWithOptions(const std::string& cmd, int n, int d, int p, int m) {
        auto commandRunner = [this](const std::string& cmd, int duration, int period, int parallel) {
            //병렬 처리 옵션이 있는 경우
            if (parallel > 1 && cmd.find("sum") == 0) {
                std::istringstream iss(cmd);
                std::string command;
                int x;
                iss >> command >> x;

                int chunkSize = x / parallel;
                std::vector<std::future<long long>> futures;
                for (int i = 0; i < parallel; ++i) {
                    int start = i * chunkSize + 1;
                    int end = (i == parallel - 1) ? x : (i + 1) * chunkSize;
                    futures.push_back(std::async(std::launch::async, [start, end]() {
                        long long sum = 0;
                        for (int j = start; j <= end; ++j) {
                            sum += j;
                        }
                        return sum;
                        }));
                }

                long long totalSum = 0;
                for (auto& fut : futures) {
                    totalSum += fut.get();
                }
                std::cout << "Sum: " << totalSum % 1000000 << std::endl;
            }
            //일반 처리
            else {
                auto start = std::chrono::high_resolution_clock::now();
                while (true) {
                    std::this_thread::sleep_for(std::chrono::seconds(period));
                    executeCommand(cmd);
                    auto end = std::chrono::high_resolution_clock::now();
                    int elapsed = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
                    if (elapsed >= duration) {
                        break;
                    }
                }
            }
        };

//n번만큼 명령어 실행
        for (int i = 0; i < n; ++i) {
            std::thread(commandRunner, cmd, d, p, m).detach();
        }
    }

public:
    QueueManager() : promoteIndex(0) {}

//프로세스 추가
    void addProcess(Process* process) {
        std::lock_guard<std::mutex> lock(mtx);
        dynamic_queue.push_back(process);
    }

//프로세스 대기 시뮬레이션
    void simulateProcessSleep() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!dynamic_queue.empty()) {
            size_t index = rand() % dynamic_queue.size();
            int wait_time = rand() % 10 + 1;
            Process* process = dynamic_queue[index];
            dynamic_queue.erase(dynamic_queue.begin() + index);
            wait_queue.push_back(std::make_pair(process, wait_time));
            std::sort(wait_queue.begin(), wait_queue.end(), [](const std::pair<Process*, int>& a, const std::pair<Process*, int>& b) {
                return a.second < b.second;
                });
            if (promoteIndex >= dynamic_queue.size()) {
                promoteIndex = 0;
            }
        }
    }

//프로세스 승격
    void promoteProcess() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!wait_queue.empty()) {
            Process* process = wait_queue.front().first;
            process->promoted = true;
            dynamic_queue.push_back(process);
            wait_queue.pop_front();
        }
        if (!dynamic_queue.empty()) {
            promoteIndex = (promoteIndex + 1) % dynamic_queue.size();
        }
    }

//실행 중인 프로세스 시뮬레이션
    void simulateRunningProcess() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!dynamic_queue.empty()) {
            runningProcess = dynamic_queue.front();
            dynamic_queue.pop_front();
            if (promoteIndex >= dynamic_queue.size()) {
                promoteIndex = 0;
            }
        }
        else {
            runningProcess = nullptr;
        }
    }

//대기 시간 감소
    void decrementWaitTimes() {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto it = wait_queue.begin(); it != wait_queue.end(); ) {
            it->second--;
            if (it->second <= 0) {
                dynamic_queue.push_back(it->first);
                it = wait_queue.erase(it);
            }
            else {
                ++it;
            }
        }
    }

//큐 상태 출력
    void displayQueues() {
        std::lock_guard<std::mutex> lock(mtx);
        std::cout << "Running: [" << (runningProcess ? to_string(runningProcess->id) + runningProcess->type : "") << "]" << std::endl;
        std::cout << "---------------------------" << std::endl;

        std::cout << "DQ: ";
        if (!dynamic_queue.empty()) {
            for (size_t i = 0; i < dynamic_queue.size(); ++i) {
                if (i == 0) {
                    std::cout << "[";
                }
                std::cout << dynamic_queue[i]->id << dynamic_queue[i]->type;
                if (dynamic_queue[i]->promoted) {
                    std::cout << "*";
                }
                if (i == dynamic_queue.size() - 1) {
                    std::cout << "] (top)";
                }
                else {
                    std::cout << " ";
                }
            }
        }
        else {
            std::cout << "[bottom/top]";
        }
        std::cout << std::endl << "---------------------------" << std::endl;

        std::cout << "WQ: ";
        bool has_wq = false;
        for (auto& pair : wait_queue) {
            if (pair.second > 0) {
                has_wq = true;
                std::cout << "[" << pair.first->id << pair.first->type << ":" << pair.second << "] ";
            }
        }
        if (!has_wq) {
            std::cout << "[]";
        }
        std::cout << "\n..." << std::endl;
    }

//시뮬레이션 실행
    void runSimulation(int duration, int interval) {
        auto start = std::chrono::high_resolution_clock::now();
        int time_passed = 0;

        while (time_passed < duration) {
            std::this_thread::sleep_for(std::chrono::seconds(interval));
            simulateRunningProcess();
            simulateProcessSleep();
            decrementWaitTimes();
            promoteProcess();
            displayQueues();
            auto end = std::chrono::high_resolution_clock::now();
            time_passed = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(end - start).count());
        }
    }

//명령어 실행
    void executeCommand(const std::string& command) {
        std::lock_guard<std::mutex> lock(mtx);
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;

        if (cmd == "echo") {
            std::string message;
            iss >> message;
            std::cout << message << std::endl;
        }
        else if (cmd == "dummy") {
           
        }
        else if (cmd == "gcd") {
            int x, y;
            iss >> x >> y;
            while (y != 0) {
                int temp = y;
                y = x % y;
                x = temp;
            }
            std::cout << "GCD: " << x << std::endl;
        }
        else if (cmd == "prime") {
            int x;
            iss >> x;
            std::vector<bool> is_prime(x + 1, true);
            is_prime[0] = is_prime[1] = false;
            for (int i = 2; i * i <= x; ++i) {
                if (is_prime[i]) {
                    for (int j = i * i; j <= x; j += i) {
                        is_prime[j] = false;
                    }
                }
            }
            int prime_count = std::count(is_prime.begin(), is_prime.end(), true);
            std::cout << "Number of primes <= " << x << ": " << prime_count << std::endl;
        }
        else if (cmd == "sum") {
            int x;
            iss >> x;
            long long sum = 0;
            for (int i = 1; i <= x; ++i) {
                sum += i;
            }
            std::cout << "Sum: " << sum % 1000000 << std::endl;
        }
    }

//옵션과 함께 명령어 실행
    void executeCommandWithOptions(const std::string& command) {
        std::istringstream iss(command);
        std::string cmd;
        int n = 1, d = 100, p = 1, m = 1;

        std::string word;
        while (iss >> word) {
            if (word == "-n") {
                iss >> n;
            }
            else if (word == "-d") {
                iss >> d;
            }
            else if (word == "-p") {
                iss >> p;
            }
            else if (word == "-m") {
                iss >> m;
            }
            else {
                if (cmd.empty()) {
                    cmd = word;
                }
                else {
                    cmd += " " + word;
                }
            }
        }

        runCommandWithOptions(cmd, n, d, p, m);
    }

//쉘 실행
    void runShell(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) {
            std::cerr << "Error opening file: " << filename << std::endl;
            return;
        }

        std::string command;
        while (std::getline(file, command)) {
            size_t pos;
            while ((pos = command.find(';')) != std::string::npos) {
                std::string subcommand = command.substr(0, pos);
                std::cout << "prompt> ";
                executeCommandWithOptions(subcommand);
                command.erase(0, pos + 1);
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }

            if ((pos = command.find('&')) != std::string::npos) {
                std::string subcommand = command.substr(0, pos);
                std::cout << "prompt> ";
                std::thread bgThread(&QueueManager::executeCommandWithOptions, this, subcommand);
                bgThread.detach();
                command.erase(0, pos + 1);
                std::cout << "&" << subcommand << std::endl;
            }
            else {
                std::cout << "prompt> ";
                executeCommandWithOptions(command);
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
};

int main() {
    srand(static_cast<unsigned>(time(0)));
    QueueManager qm;

    for (int i = 1; i <= 10; i++) {
        Process* p = new Process(i, i % 2 == 0 ? 'F' : 'B');
        qm.addProcess(p);
    }

//쉘과 모니터 스레드 시작
    std::thread shellThread(&QueueManager::runShell, &qm, "commands.txt");
    std::thread monitorThread(&QueueManager::runSimulation, &qm, 60, 5);

    shellThread.join();
    monitorThread.join();

    return 0;
}
