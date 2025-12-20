#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <random>
#include <iomanip>

using namespace std;

// 性能测试配置
struct Config {
    string host = "127.0.0.1";
    int port = 8096;
    int num_threads = 4;
    int num_ops = 10000;
    int key_size = 16;
    int value_size = 100;
    string mode = "put"; // put, get, mixed
};

class Benchmark {
public:
    Benchmark(Config config) : config_(config), total_ops_(0), total_latency_(0) {}

    void Run() {
        vector<thread> threads;
        auto start_time = chrono::high_resolution_clock::now();

        for (int i = 0; i < config_.num_threads; ++i) {
            threads.emplace_back(&Benchmark::Worker, this, i);
        }

        for (auto& t : threads) {
            t.join();
        }

        auto end_time = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time).count();

        Report(duration);
    }

private:
    void Worker(int id) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            cerr << "Thread " << id << ": Failed to create socket" << endl;
            return;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(config_.port);
        inet_pton(AF_INET, config_.host.c_str(), &server_addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            cerr << "Thread " << id << ": Failed to connect to server" << endl;
            close(sock);
            return;
        }

        mt19937 gen(id + time(0));
        uniform_int_distribution<> dis(0, 25);

        int ops_per_thread = config_.num_ops / config_.num_threads;
        for (int i = 0; i < ops_per_thread; ++i) {
            string key = GenerateRandomString(config_.key_size, gen);
            string value = GenerateRandomString(config_.value_size, gen);
            
            string request;
            if (config_.mode == "put") {
                request = "put " + key + " " + value + "\n";
            } else if (config_.mode == "get") {
                request = "get " + key + "\n";
            } else {
                // Mixed mode: 50% put, 50% get
                if (i % 2 == 0) {
                    request = "put " + key + " " + value + "\n";
                } else {
                    request = "get " + key + "\n";
                }
            }

            auto op_start = chrono::high_resolution_clock::now();
            
            if (send(sock, request.c_str(), request.size(), 0) < 0) {
                cerr << "Thread " << id << ": Send failed" << endl;
                break;
            }

            char buf[1024];
            int n = recv(sock, buf, sizeof(buf), 0);
            if (n <= 0) {
                cerr << "Thread " << id << ": Recv failed" << endl;
                break;
            }

            auto op_end = chrono::high_resolution_clock::now();
            total_latency_ += chrono::duration_cast<chrono::microseconds>(op_end - op_start).count();
            total_ops_++;
        }

        close(sock);
    }

    string GenerateRandomString(int length, mt19937& gen) {
        string s;
        s.reserve(length);
        for (int i = 0; i < length; ++i) {
            s += (char)('a' + (gen() % 26));
        }
        return s;
    }

    void Report(long long duration_ms) {
        double duration_sec = duration_ms / 1000.0;
        double qps = total_ops_ / duration_sec;
        double avg_latency = total_ops_ > 0 ? (double)total_latency_ / total_ops_ : 0;

        cout << "------------------------------------" << endl;
        cout << "Benchmark Results (" << config_.mode << "):" << endl;
        cout << "Threads:      " << config_.num_threads << endl;
        cout << "Total Ops:    " << total_ops_ << endl;
        cout << "Duration:     " << fixed << setprecision(2) << duration_sec << " s" << endl;
        cout << "Throughput:   " << fixed << setprecision(2) << qps << " ops/s" << endl;
        cout << "Avg Latency:  " << fixed << setprecision(2) << avg_latency << " us" << endl;
        cout << "------------------------------------" << endl;
    }

    Config config_;
    atomic<long long> total_ops_;
    atomic<long long> total_latency_;
};

int main(int argc, char* argv[]) {
    Config config;
    // 参数顺序: ./db_bench [threads] [ops] [mode]
    // 例如: ./db_bench 4 1000 put
    if (argc > 1) config.num_threads = stoi(argv[1]);
    if (argc > 2) config.num_ops = stoi(argv[2]);
    if (argc > 3) config.mode = argv[3];

    cout << "Starting benchmark on " << config.host << ":" << config.port << "..." << endl;
    cout << "Mode: " << config.mode << ", Threads: " << config.num_threads << ", Ops: " << config.num_ops << endl;
    Benchmark bench(config);
    bench.Run();

    return 0;
}
