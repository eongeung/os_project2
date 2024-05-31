#include <iostream>
#include <deque>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <sstream>

class Process {
public:
    int id;
    char type;
    bool promoted;

    Process(int id, char type) : id(id), type(type), promoted(false) {}
};

class QueueManager {
private:
    std::deque<Process*> dynamic_queue;
    std::deque<std::pair<Process*, int>> wait_queue;
    Process* runningProcess = nullptr;

    std::string to_string(int value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }

public:
    void addProcess(Process* process) {
        dynamic_queue.push_back(process);
    }

    void simulateProcessSleep() {
        if (!dynamic_queue.empty()) {
            size_t index = rand() % dynamic_queue.size();
            int wait_time = rand() % 10 + 1;
            Process* process = dynamic_queue[index];
            dynamic_queue.erase(dynamic_queue.begin() + index);
            wait_queue.push_back(std::make_pair(process, wait_time));
        }
    }

    void promoteProcess() {
        if (!wait_queue.empty()) {
            Process* process = wait_queue.front().first;
            process->promoted = true;
            dynamic_queue.push_back(process);
            wait_queue.pop_front();
        }
    }

    void simulateRunningProcess() {
        if (!dynamic_queue.empty()) {
            runningProcess = dynamic_queue.front();
            dynamic_queue.pop_front();
        }
        else {
            runningProcess = nullptr;
        }
    }

    void decrementWaitTimes() {
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

    void displayQueues() {
        std::cout << "Running: [" << (runningProcess ? to_string(runningProcess->id) + runningProcess->type : "") << "]" << std::endl;
        std::cout << "---------------------------" << std::endl;
        std::cout << "DQ: ";
        if (!dynamic_queue.empty()) {
            std::cout << "P => ";
            for (size_t i = 0; i < dynamic_queue.size(); ++i) {
                std::cout << "[" << dynamic_queue[i]->id << dynamic_queue[i]->type;
                if (dynamic_queue[i]->promoted) {
                    std::cout << "*";
                }
                std::cout << "] ";
                if (i == dynamic_queue.size() - 1) {
                    std::cout << "(bottom/top)";
                }
            }
        }
        else {
            std::cout << "[]";
        }
        std::cout << std::endl << "---------------------------" << std::endl;
        std::cout << "WQ: ";
        if (!wait_queue.empty()) {
            for (auto& wp : wait_queue) {
                std::cout << "[" << wp.first->id << wp.first->type << ":" << wp.second << "] ";
            }
        }
        else {
            std::cout << "[]";
        }
        std::cout << "\n..." << std::endl;
    }

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
};

int main() {
    srand(static_cast<unsigned>(time(0)));
    QueueManager qm;

    for (int i = 1; i <= 10; i++) {
        Process* p = new Process(i, i % 2 == 0 ? 'F' : 'B');
        qm.addProcess(p);
    }

    std::cout << "Initial Queue States:" << std::endl;
    qm.displayQueues();

    qm.runSimulation(60, 5);

    return 0;
}