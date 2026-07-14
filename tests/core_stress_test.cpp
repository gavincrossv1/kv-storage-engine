#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "db.h"
#include "db_impl.h"

namespace
{
	using Clock = std::chrono::steady_clock;

	struct Config
	{
		int threads = 4;
		uint64_t operations = 10000;
		std::string mode = "put";
		size_t value_size = 100;
		uint64_t read_ratio = 50;
		bool enable_wal = true;
		bool keep_data = false;
		std::filesystem::path db_dir = "core_stress_data";
	};

	struct ThreadStats
	{
		uint64_t operations = 0;
		uint64_t failures = 0;
		uint64_t latency_ns = 0;
		uint64_t max_latency_ns = 0;
	};

	void PrintUsage(const char *program)
	{
		std::cout
			<< "Usage: " << program << " [threads] [operations] [put|get|mixed] [options]\n"
			<< "Options:\n"
			<< "  --disable-wal       Disable WAL for write-path comparison\n"
			<< "  --enable-wal        Enable WAL (default)\n"
			<< "  --value-size N      Value size in bytes (default: 100)\n"
			<< "  --read-ratio N      Read percentage in mixed mode, 0-100 (default: 50)\n"
			<< "  --db-dir PATH       Isolated database directory (default: core_stress_data)\n"
			<< "  --keep-data         Keep generated database files after the test\n"
			<< "  --help              Show this help\n";
	}

	uint64_t ParseUnsigned(const std::string &text, const char *name)
	{
		size_t parsed = 0;
		uint64_t value = std::stoull(text, &parsed);
		if (parsed != text.size())
		{
			throw std::invalid_argument(std::string("invalid ") + name + ": " + text);
		}
		return value;
	}

	Config ParseArgs(int argc, char *argv[])
	{
		Config config;
		int positional = 0;
		for (int i = 1; i < argc; ++i)
		{
			const std::string arg = argv[i];
			if (arg == "--help")
			{
				PrintUsage(argv[0]);
				std::exit(0);
			}
			if (arg == "--disable-wal")
			{
				config.enable_wal = false;
			}
			else if (arg == "--enable-wal")
			{
				config.enable_wal = true;
			}
			else if (arg == "--keep-data")
			{
				config.keep_data = true;
			}
			else if (arg == "--value-size" || arg == "--read-ratio" || arg == "--db-dir")
			{
				if (++i >= argc)
				{
					throw std::invalid_argument(arg + " requires a value");
				}
				if (arg == "--value-size")
				{
					config.value_size = static_cast<size_t>(ParseUnsigned(argv[i], "value size"));
				}
				else if (arg == "--read-ratio")
				{
					config.read_ratio = ParseUnsigned(argv[i], "read ratio");
				}
				else
				{
					config.db_dir = argv[i];
				}
			}
			else if (!arg.empty() && arg[0] == '-')
			{
				throw std::invalid_argument("unknown option: " + arg);
			}
			else if (positional == 0)
			{
				config.threads = static_cast<int>(ParseUnsigned(arg, "thread count"));
				++positional;
			}
			else if (positional == 1)
			{
				config.operations = ParseUnsigned(arg, "operation count");
				++positional;
			}
			else if (positional == 2)
			{
				config.mode = arg;
				++positional;
			}
			else
			{
				throw std::invalid_argument("too many positional arguments");
			}
		}

		if (config.threads <= 0 || config.operations == 0 || config.value_size == 0)
		{
			throw std::invalid_argument("threads, operations and value size must be greater than zero");
		}
		if (config.mode != "put" && config.mode != "get" && config.mode != "mixed")
		{
			throw std::invalid_argument("mode must be put, get or mixed");
		}
		if (config.read_ratio > 100)
		{
			throw std::invalid_argument("read ratio must be between 0 and 100");
		}
		return config;
	}

	std::string MakeKey(const std::string &prefix, uint64_t id)
	{
		std::ostringstream out;
		out << prefix << '_' << std::setw(20) << std::setfill('0') << id;
		return out.str();
	}

	std::string MakeValue(size_t size, uint64_t id)
	{
		std::string value(size, static_cast<char>('a' + (id % 26)));
		return value;
	}

	void PrepareDirectory(const std::filesystem::path &directory)
	{
		const auto absolute = std::filesystem::absolute(directory);
		if (absolute == absolute.root_path())
		{
			throw std::invalid_argument("refusing to use a filesystem root as db directory");
		}
		std::filesystem::remove_all(absolute);
		std::filesystem::create_directories(absolute);
	}
} // namespace

int main(int argc, char *argv[])
{
	try
	{
		const Config config = ParseArgs(argc, argv);
		const std::filesystem::path original_dir = std::filesystem::current_path();
		const std::filesystem::path db_dir = std::filesystem::absolute(config.db_dir);
		PrepareDirectory(db_dir);
		std::filesystem::current_path(db_dir);

		std::cout << "Core data-layer stress test\n"
				  << "Mode:       " << config.mode << '\n'
				  << "Threads:    " << config.threads << '\n'
				  << "Operations: " << config.operations << '\n'
				  << "Value size: " << config.value_size << " bytes\n"
				  << "WAL:        " << (config.enable_wal ? "enabled" : "disabled") << '\n'
				  << "Read ratio: " << (config.mode == "mixed" ? std::to_string(config.read_ratio) + "%" : "N/A") << '\n'
				  << "DB dir:     " << db_dir.string() << "\n\n";

		std::vector<ThreadStats> stats(static_cast<size_t>(config.threads));
		std::mutex start_mutex;
		std::condition_variable start_cv;
		int ready_threads = 0;
		bool start = false;
		Clock::time_point measured_start;
		Clock::time_point measured_end;
		Clock::time_point shutdown_start;
		Clock::time_point shutdown_end;

		{
			std::unique_ptr<koishidb::DB> db = std::make_unique<koishidb::DBimpl>(config.enable_wal);

			// Get and mixed modes read known existing keys; setup operations are not measured.
			if (config.mode != "put")
			{
				std::cout << "Preparing " << config.operations << " records...\n";
				for (uint64_t i = 0; i < config.operations; ++i)
				{
					const std::string key = MakeKey("read", i);
					const std::string value = MakeValue(config.value_size, i);
					db->Put(key, value);
				}
			}

			std::vector<std::thread> workers;
			workers.reserve(static_cast<size_t>(config.threads));
			for (int thread_id = 0; thread_id < config.threads; ++thread_id)
			{
				workers.emplace_back([&, thread_id]
				{
					const uint64_t begin = config.operations * static_cast<uint64_t>(thread_id) /
										   static_cast<uint64_t>(config.threads);
					const uint64_t end = config.operations * static_cast<uint64_t>(thread_id + 1) /
										 static_cast<uint64_t>(config.threads);
					ThreadStats &local = stats[static_cast<size_t>(thread_id)];

					{
						std::unique_lock<std::mutex> lock(start_mutex);
						++ready_threads;
						start_cv.notify_all();
						start_cv.wait(lock, [&] { return start; });
					}

					for (uint64_t i = begin; i < end; ++i)
					{
						const bool do_put = config.mode == "put" ||
											(config.mode == "mixed" && (i % 100 >= config.read_ratio));
						const auto op_start = Clock::now();
						if (do_put)
						{
							const std::string key = MakeKey("write", i);
							const std::string value = MakeValue(config.value_size, i);
							db->Put(key, value);
						}
						else
						{
							const std::string key = MakeKey("read", i);
							const std::string expected = MakeValue(config.value_size, i);
							std::string actual;
							if (!db->Get(key, &actual) || actual != expected)
							{
								++local.failures;
							}
						}
						const uint64_t latency = static_cast<uint64_t>(
							std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - op_start).count());
						local.latency_ns += latency;
						local.max_latency_ns = std::max(local.max_latency_ns, latency);
						++local.operations;
					}
				});
			}

			{
				std::unique_lock<std::mutex> lock(start_mutex);
				start_cv.wait(lock, [&] { return ready_threads == config.threads; });
				measured_start = Clock::now();
				start = true;
			}
			start_cv.notify_all();

			for (auto &worker : workers)
			{
				worker.join();
			}
			measured_end = Clock::now();
			shutdown_start = measured_end;
		}
		shutdown_end = Clock::now();
		std::filesystem::current_path(original_dir);

		uint64_t completed = 0;
		uint64_t failures = 0;
		uint64_t total_latency_ns = 0;
		uint64_t max_latency_ns = 0;
		for (const ThreadStats &local : stats)
		{
			completed += local.operations;
			failures += local.failures;
			total_latency_ns += local.latency_ns;
			max_latency_ns = std::max(max_latency_ns, local.max_latency_ns);
		}

		const double duration_s = std::chrono::duration<double>(measured_end - measured_start).count();
		const double shutdown_s = std::chrono::duration<double>(shutdown_end - shutdown_start).count();
		const double throughput = duration_s > 0.0 ? static_cast<double>(completed) / duration_s : 0.0;
		const double average_us = completed > 0
									  ? static_cast<double>(total_latency_ns) / completed / 1000.0
									  : 0.0;

		std::cout << "\n------------------------------------\n"
				  << "Benchmark Results (" << config.mode << "):\n"
				  << "Completed:     " << completed << '\n'
				  << "Failures:      " << failures << '\n'
				  << std::fixed << std::setprecision(2)
				  << "Duration:      " << duration_s << " s\n"
				  << "Throughput:    " << throughput << " ops/s\n"
				  << "Avg latency:   " << average_us << " us\n"
				  << "Max latency:   " << static_cast<double>(max_latency_ns) / 1000.0 << " us\n"
				  << "Shutdown/flush: " << shutdown_s << " s (excluded from throughput)\n"
				  << "------------------------------------\n";

		if (!config.keep_data)
		{
			std::filesystem::remove_all(db_dir);
		}
		return failures == 0 ? 0 : 2;
	}
	catch (const std::exception &error)
	{
		std::cerr << "core_stress_test: " << error.what() << '\n';
		return 1;
	}
}
