#include "db_impl.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string MakeKey(int i) {
    return "wal_key_" + std::to_string(i);
}

std::string MakeValue(int i) {
    return "wal_value_" + std::to_string(i) + std::string(64, static_cast<char>('a' + (i % 26)));
}

void Check(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "wal_recovery_test failed: " << message << std::endl;
        std::exit(1);
    }
}

void CrashWriteChild(const fs::path &dir, int count) {
    if (::chdir(dir.c_str()) != 0) {
        _exit(10);
    }

    koishidb::DBimpl *db = new koishidb::DBimpl();
    for (int i = 0; i < count; ++i) {
        db->Put(MakeKey(i), MakeValue(i));
        if (i % 17 == 0) {
            std::string got;
            if (!db->Get(MakeKey(i), &got) || got != MakeValue(i)) {
                _exit(11);
            }
        }
    }

    // 故意不 delete db，跳过 DBimpl 析构，模拟进程崩溃时 memtable 未落盘。
    _exit(0);
}

void RunCrashWriter(const fs::path &dir, int count) {
    pid_t pid = ::fork();
    Check(pid >= 0, "fork failed");
    if (pid == 0) {
        CrashWriteChild(dir, count);
    }

    int status = 0;
    Check(::waitpid(pid, &status, 0) == pid, "waitpid failed");
    Check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "crash writer child failed");
}

void AppendBrokenWalTail(const fs::path &dir) {
    fs::path wal = dir / "wal.log";
    int fd = ::open(wal.c_str(), O_WRONLY | O_APPEND);
    Check(fd >= 0, "open wal.log for broken tail failed");
    const char broken_tail[] = {'x', 'y', 'z'};
    Check(::write(fd, broken_tail, sizeof(broken_tail)) == static_cast<ssize_t>(sizeof(broken_tail)),
          "append broken wal tail failed");
    ::close(fd);
}

void VerifyRecoveredData(const fs::path &dir, int count) {
    fs::path old = fs::current_path();
    fs::current_path(dir);
    {
        koishidb::DBimpl db;
        for (int i = 0; i < count; ++i) {
            std::string got;
            Check(db.Get(MakeKey(i), &got), "missing recovered key " + MakeKey(i));
            Check(got == MakeValue(i), "bad recovered value for " + MakeKey(i));
        }
    }
    fs::current_path(old);
}

void VerifyCleanCloseWithoutWal(const fs::path &dir, int count) {
    Check(!fs::exists(dir / "wal.log"), "wal.log should be removed after clean close");

    fs::path old = fs::current_path();
    fs::current_path(dir);
    {
        koishidb::DBimpl db;
        for (int i = 0; i < count; i += 13) {
            std::string got;
            Check(db.Get(MakeKey(i), &got), "missing key after clean reopen " + MakeKey(i));
            Check(got == MakeValue(i), "bad value after clean reopen " + MakeKey(i));
        }
    }
    fs::current_path(old);
}

} // namespace

int main() {
    const int count = 128;
    fs::path dir = fs::absolute("./wal_recovery_data");
    fs::remove_all(dir);
    fs::create_directories(dir);

    RunCrashWriter(dir, count);
    Check(fs::exists(dir / "wal.log"), "wal.log should exist after crash");

    AppendBrokenWalTail(dir);
    VerifyRecoveredData(dir, count);
    VerifyCleanCloseWithoutWal(dir, count);

    fs::remove_all(dir);
    std::cout << "wal_recovery_test PASS" << std::endl;
    return 0;
}
