#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include <atomic>
#include <iomanip>

// Structure to represent traffic signal data
struct TrfcData {
    std::string timestamp;
    int lightID;
    int cars;
};

class TrfcControl {
private:
    std::queue<TrfcData> traffic_queue;
    std::mutex mutex;
    std::condition_variable producer_cv, consumer_cv;
    std::vector<TrfcData> top_congested_lights;
    std::atomic<bool> is_producer_done{false}; // Flag to indicate producer completion

    const int MaxQueueSize = 100; // Maximum capacity of the traffic queue
    const int TopConjetionLights = 3; // Top N congested traffic lights to track

public:
    // Producer function to read traffic data from file and push it to the queue
    void producer(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error opening file: " << filename << std::endl;
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            TrfcData data;
            int hour, minute, second;
            char delimiter;
            if (!(iss >> hour >> delimiter >> minute >> delimiter >> second >> data.lightID >> data.cars)) {
                std::cerr << "Error reading line: " << line << std::endl;
                continue;
            }
            std::ostringstream oss;
            oss << std::setw(2) << std::setfill('0') << hour << ":" << std::setw(2) << std::setfill('0') << minute << ":" << std::setw(2) << std::setfill('0') << second;
            data.timestamp = oss.str();

            std::unique_lock<std::mutex> lock(mutex);
            producer_cv.wait(lock, [this]() { return traffic_queue.size() < MaxQueueSize; });

            traffic_queue.push(data);
            std::cout << "Producer: Read traffic data - Timestamp: " << data.timestamp << ", Light ID: " << data.lightID << ", Num Cars: " << data.cars << std::endl;

            consumer_cv.notify_all();
        }
        is_producer_done = true;
        consumer_cv.notify_all(); 
    }
    void consumer(int num_consumers) {
        while (true) {
            std::unique_lock<std::mutex> lock(mutex);
            consumer_cv.wait(lock, [this]() { return !traffic_queue.empty() || is_producer_done; });

            if (traffic_queue.empty() && is_producer_done) {
               
                break;
            }

            TrfcData data = traffic_queue.front();
            traffic_queue.pop();
            lock.unlock();

            processTrfcData(data);

            producer_cv.notify_all();
        }
    }

    void processTrfcData(const TrfcData& data) {
        if (top_congested_lights.size() < TopConjetionLights) {
            top_congested_lights.push_back(data);
        } else {
            auto min_element = std::min_element(top_congested_lights.begin(), top_congested_lights.end(),
                [](const TrfcData& a, const TrfcData& b) { return a.cars < b.cars; });
            if (data.cars > min_element->cars) {
                *min_element = data;
            }
        }

        std::sort(top_congested_lights.begin(), top_congested_lights.end(),
            [](const TrfcData& a, const TrfcData& b) { return a.cars > b.cars; });

        std::cout << "Consumer: Updated top congested lights list - ";
        for (const auto& light : top_congested_lights) {
            std::cout << "(Light ID: " << light.lightID << ", Num Cars: " << light.cars << ") ";
        }
        std::cout << std::endl;
    }

    void startSimulation(const std::string& filename, int num_consumers) {
        std::vector<std::thread> producer_threads, consumer_threads;

        producer_threads.emplace_back([this, filename]() {
            this->producer(filename);
        });

        for (int i = 0; i < num_consumers; ++i) {
            consumer_threads.emplace_back([this, num_consumers]() {
                this->consumer(num_consumers);
            });
        }

        for (auto& thread : producer_threads) {
            thread.join();
        }

        for (auto& thread : consumer_threads) {
            thread.join();
        }
    }
};

int main() {
    TrfcControl simulator;
    std::string filename = "data.txt"; 
    int num_consumers = 5; 
    simulator.startSimulation(filename, num_consumers);
    return 0;
}