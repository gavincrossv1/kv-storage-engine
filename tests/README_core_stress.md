# Core Data-Layer Stress Test

`core_stress_test` 直接调用 `DBimpl::Put()` 和 `DBimpl::Get()`，不经过 TCP、epoll、协议解析和线程池，用于观察数据库核心读写路径本身的性能。

该测试与 `db_bench` 的定位不同：

| 工具 | 覆盖范围 | 适用场景 |
| --- | --- | --- |
| `core_stress_test` | DB、MemTable、WAL、SSTable | 定位数据层性能和 WAL 开销 |
| `db_bench` | 客户端、TCP、epoll、线程池、协议解析、DB | 测试完整服务端端到端性能 |

## 构建

`core_stress_test` 在根目录的 `CMakeLists.txt` 中使用了 `EXCLUDE_FROM_ALL`，因此普通的全量构建不会自动生成它，需要显式指定目标：

```bash
cd build
cmake ..
cmake --build . --target core_stress_test -j
```

构建完成后，可执行文件位于 `build/core_stress_test`。

## 基本用法

```bash
./core_stress_test [线程数] [总操作数] [put|get|mixed] [选项]
```

位置参数均可省略，默认值为：

- 线程数：`4`
- 总操作数：`10000`，表示所有线程合计执行的操作数，而不是每个线程的操作数
- 模式：`put`
- value 大小：`100` 字节
- Mixed 模式读比例：`50%`
- WAL：启用

查看程序内置帮助：

```bash
./core_stress_test --help
```

## 测试模式

### Put

多线程直接调用 `DBimpl::Put()`，每次写入使用唯一、确定性的 key。

```bash
./core_stress_test 4 10000 put
```

默认启用 WAL，因此每一批写在返回前都会完成 WAL 同步。该模式适合测量完整的核心持久化写路径。

### Get

程序先预装指定数量的记录，再并发读取这些已存在的记录。预装过程不计入 benchmark 时间，读取结果会与预期 value 比较。

```bash
./core_stress_test 8 100000 get --disable-wal
```

当前预装数据保留在同一个 `DBimpl` 实例中，因此主要测量 MemTable 查询性能。WAL 开关只影响预装速度，不影响 `Get()` 本身。

### Mixed

通过 `--read-ratio N` 设置 Mixed 模式的读操作百分比，Put 比例自动为 `100-N`。Get 使用预装的已知 key，Put 使用另一组唯一 key；默认比例为 50% Get 和 50% Put。

```bash
# 默认 50% Get + 50% Put
./core_stress_test 8 100000 mixed --disable-wal

# 80% Get + 20% Put
./core_stress_test 8 100000 mixed --read-ratio 80 --disable-wal

# 20% Get + 80% Put
./core_stress_test 8 100000 mixed --read-ratio 20 --disable-wal
```

`--read-ratio 0` 表示全部执行 Put，`--read-ratio 100` 表示全部执行 Get。比例按照操作编号每 100 次为一组进行确定性分配，不使用随机数，因此相同参数的测试负载可重复。

## 可选参数

| 参数 | 说明 |
| --- | --- |
| `--disable-wal` | 关闭 WAL，用于测量不含日志落盘的写路径 |
| `--enable-wal` | 显式启用 WAL，也是默认行为 |
| `--value-size N` | 设置 value 大小，单位为字节 |
| `--read-ratio N` | Mixed 模式中的 Get 百分比，范围为 0 到 100，默认 50 |
| `--db-dir PATH` | 设置测试数据库目录，默认是当前目录下的 `core_stress_data` |
| `--keep-data` | 测试结束后保留 manifest、SSTable 等数据库文件 |
| `--help` | 显示帮助信息 |

示例：使用 16 个线程写入 20 万条 1 KiB value，并保留生成的数据文件：

```bash
./core_stress_test 16 200000 put \
	--value-size 1024 \
	--disable-wal \
	--db-dir ./core_stress_1k \
	--keep-data
```

## WAL 性能对比

使用完全相同的线程数、操作数和 value 大小分别运行两次：

```bash
# 强持久化写路径：Put 返回前同步 WAL
./core_stress_test 4 10000 put --enable-wal

# 纯 MemTable/SSTable 写路径：不写 WAL
./core_stress_test 4 10000 put --disable-wal
```

重点比较输出中的 `Throughput` 和 `Avg latency`。两次测试的差异主要反映 WAL 追加和 `fsync` 的成本，不包含网络层开销。

关闭 WAL 后，Put 返回仅表示数据已写入 MemTable；如果进程在 MemTable 刷成 SSTable 前异常退出，这部分数据无法恢复。因此 `--disable-wal` 只应用于性能分析，不应作为需要崩溃持久性的生产配置。

## 输出指标

```text
Benchmark Results (put):
Completed:     10000
Failures:      0
Duration:      12.34 s
Throughput:    810.37 ops/s
Avg latency:   4921.15 us
Max latency:   18120.42 us
Shutdown/flush: 0.08 s (excluded from throughput)
```

- `Completed`：实际完成的总操作数。
- `Failures`：Get 未找到数据或 value 校验失败的次数；Put 接口当前不返回状态，因此无法统计内部写失败。
- `Duration`：所有工作线程同时起跑后，到全部操作结束的墙钟时间。
- `Throughput`：`Completed / Duration`。
- `Avg latency`：所有操作的平均调用耗时。
- `Max latency`：单次操作的最大调用耗时。
- `Shutdown/flush`：`DBimpl` 析构时将剩余 MemTable 刷成 SSTable 并写 manifest 的耗时，不计入吞吐和操作延迟。

## 数据目录与清理

数据库实现使用相对路径访问 `manifest`、`wal.log` 和 `sstable_*`。为了避免污染服务端的数据，测试程序会：

1. 创建独立的测试目录，默认是 `./core_stress_data`。
2. 启动测试前清空该目录，保证每次测试从空数据库开始。
3. 在该目录中运行整个 DB 生命周期。
4. 默认在测试完成后删除该目录；添加 `--keep-data` 可保留文件供检查。

注意：`--db-dir` 指定的目录会在测试开始前被递归清空，不要指向包含重要文件的目录。

## 测试建议

1. 使用 Release 构建，避免断言和调试日志影响结果：

	 ```bash
	 cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release
	 cmake --build . --target core_stress_test -j
	 ```

2. 每组参数至少运行 3 次，忽略第一次预热结果，观察中位数而不是单次峰值。
3. 对比测试时只改变一个变量，例如只切换 WAL，其他参数保持一致。
4. 在 WSL 中优先把项目放在 Linux 文件系统（例如 `~/simple-leveldb`），不要放在 `/mnt/c`；`fsync` 在 Windows 挂载盘上通常明显更慢。
5. Put 启用 WAL 时，吞吐主要受同步落盘延迟限制；增加线程数不一定线性提升性能。
6. 若需观察 SSTable 文件大小和数量，使用 `--keep-data`，测试结束后查看指定的数据目录。
