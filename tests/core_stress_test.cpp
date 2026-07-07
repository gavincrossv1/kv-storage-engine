#include "db_impl.h"

#include <unistd.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Config {
    std::filesystem::path dir = "./core_stress_data";
    int threads = 4;
    int ops_per_thread = 50000;
    int value_size = 1024;
    int read_every = 1000;
    int post_ops = 2000;
    bool clean = false;
};

std::string MakeKey(int thread_id, int index) {
    std::ostringstream oss;
    oss << "key_t" << std::setw(3) << std::setfill('0') << thread_id
        << "_" << std::setw(12) << std::setfill('0') << index;
    return oss.str();
}

std::string MakePostKey(int index) {
    std::ostringstream oss;
    oss << "post_key_" << std::setw(12) << std::setfill('0') << index;
    return oss.str();
}

std::string MakeValue(int thread_id, int index, int value_size) {
    std::string prefix = "value_t" + std::to_string(thread_id) + "_" + std::to_string(index) + "_";
    std::string value = prefix;
    if (static_cast<int>(value.size()) < value_size) {
        value.append(value_size - value.size(), static_cast<char>('a' + (thread_id + index) % 26));
    } else {
        value.resize(value_size);
    }
    return value;
}

int ParseIntArg(const std::string &arg, const std::string &name, int old_value) {
    const std::string prefix = name + "=";
    if (arg.rfind(prefix, 0) != 0) {
        return old_value;
    }
    return std::stoi(arg.substr(prefix.size()));
}

Config ParseArgs(int argc, char **argv) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--clean") {
            cfg.clean = true;
        } else if (arg.rfind("--dir=", 0) == 0) {
            cfg.dir = arg.substr(std::string("--dir=").size());
        } else if (arg.rfind("--threads=", 0) == 0) {
            cfg.threads = ParseIntArg(arg, "--threads", cfg.threads);
        } else if (arg.rfind("--ops_per_thread=", 0) == 0) {
            cfg.ops_per_thread = ParseIntArg(arg, "--ops_per_thread", cfg.ops_per_thread);
        } else if (arg.rfind("--value_size=", 0) == 0) {
            cfg.value_size = ParseIntArg(arg, "--value_size", cfg.value_size);
        } else if (arg.rfind("--read_every=", 0) == 0) {
            cfg.read_every = ParseIntArg(arg, "--read_every", cfg.read_every);
        } else if (arg.rfind("--post_ops=", 0) == 0) {
            cfg.post_ops = ParseIntArg(arg, "--post_ops", cfg.post_ops);
        } else {
            throw std::invalid_argument("unknown argument: " + arg);
        }
    }
    if (cfg.threads <= 0 || cfg.ops_per_thread <= 0 || cfg.value_size <= 0 || cfg.read_every <= 0 || cfg.post_ops < 0) {
        throw std::invalid_argument("invalid numeric argument");
    }
    return cfg;
}

void PrintConfig(const Config &cfg) {
    std::cout << "core_stress_test config:\n"
              << "  dir            = " << cfg.dir << "\n"
              << "  threads        = " << cfg.threads << "\n"
              << "  ops_per_thread = " << cfg.ops_per_thread << "\n"
              << "  total_puts     = " << static_cast<long long>(cfg.threads) * cfg.ops_per_thread << "\n"
              << "  value_size     = " << cfg.value_size << "\n"
              << "  read_every     = " << cfg.read_every << "\n"
              << "  post_ops       = " << cfg.post_ops << "\n"
              << "  clean          = " << (cfg.clean ? "true" : "false") << "\n";
}

void CheckGet(koishidb::DBimpl &db, const std::string &key, const std::string &expected) {
    std::string got;
    bool ok = db.Get(key, &got);
    if (!ok || got != expected) {
        std::cerr << "GET check failed, key=" << key
                  << ", found=" << ok
                  << ", expected_size=" << expected.size()
                  << ", got_size=" << got.size() << std::endl;
        std::abort();
    }
}

void VerifySamples(koishidb::DBimpl &db, const Config &cfg) {
    for (int t = 0; t < cfg.threads; ++t) {
        std::vector<int> samples = {0, cfg.ops_per_thread / 2, cfg.ops_per_thread - 1};
        for (int index : samples) {
            CheckGet(db, MakeKey(t, index), MakeValue(t, index, cfg.value_size));
        }
    }
}

}  // namespace

int main(int argc, char **argv) {
    try {
        Config cfg = ParseArgs(argc, argv);
        PrintConfig(cfg);

        if (cfg.clean) {
            std::filesystem::remove_all(cfg.dir);
        }
        std::filesystem::create_directories(cfg.dir);
        if (::chdir(cfg.dir.c_str()) != 0) {
            perror("chdir");
            return 1;
        }

        long long ms = 0;
        long long total_ops = static_cast<long long>(cfg.threads) * cfg.ops_per_thread + cfg.post_ops;

        {
            koishidb::DBimpl db;
            std::atomic<int> next_thread{0};
            std::atomic<long long> finished_puts{0};
            std::atomic<bool> failed{false};

            auto begin = std::chrono::steady_clock::now();
            std::vector<std::thread> workers;
            workers.reserve(cfg.threads);

            for (int i = 0; i < cfg.threads; ++i) {
                workers.emplace_back([&]() {
                    int thread_id = next_thread.fetch_add(1);
                    for (int j = 0; j < cfg.ops_per_thread && !failed.load(); ++j) {
                        std::string key = MakeKey(thread_id, j);
                        std::string value = MakeValue(thread_id, j, cfg.value_size);
                        db.Put(key, value);

                        if (j % cfg.read_every == 0) {
                            std::string got;
                            if (!db.Get(key, &got) || got != value) {
                                std::cerr << "inline GET failed, key=" << key << std::endl;
                                failed.store(true);
                                break;
                            }
                        }

                        long long done = ++finished_puts;
                        if (done % 10000 == 0) {
                            std::cout << "finished puts: " << done << std::endl;
                        }
                    }
                });
            }

            for (auto &worker : workers) {
                worker.join();
            }
            if (failed.load()) {
                return 2;
            }

            VerifySamples(db, cfg);

            std::string hot_value(cfg.value_size, 'h');
            for (int i = 0; i < cfg.post_ops; ++i) {
                std::string key = MakePostKey(i);
                db.Put(key, hot_value);
                CheckGet(db, key, hot_value);
            }

            auto end = std::chrono::steady_clock::now();
            ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
        }

        std::cout << "PASS_AFTER_CLOSE: total puts=" << total_ops
                  << ", elapsed_ms=" << ms
                  << ", throughput=" << (ms == 0 ? 0 : total_ops * 1000 / ms) << " ops/s" << std::endl;
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "core_stress_test failed: " << ex.what() << std::endl;
        return 1;
    }
}
