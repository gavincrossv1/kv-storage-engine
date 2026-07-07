# core_stress_test 使用方法

这个测试绕过 `network`/`epoll`，直接压测 `DBimpl`，用于复现和验证“大量 `put` 后继续 `put/get` 卡住”的问题。

## 构建

在项目根目录执行：

```bash
cmake --build build --target core_stress_test -j4
```

如果还没有 `build` 目录：

```bash
cmake -S . -B build
cmake --build build --target core_stress_test -j4
```

## 推荐压力测试

从项目根目录执行：

```bash
./build/core_stress_test --clean --dir=./core_stress_data --threads=4 --ops_per_thread=100000 --value_size=2048 --read_every=1000 --post_ops=5000
```

含义：

- `--threads=4`：4 个线程并发写入。
- `--ops_per_thread=100000`：每个线程写 10 万条，总共 40 万条。
- `--value_size=2048`：每条 value 约 2KB，总写入数据量约 800MB。
- `--read_every=1000`：写入过程中每 1000 条做一次立即 `get` 校验。
- `--post_ops=5000`：大量写入结束后继续执行 5000 次 `put + get`，专门验证原来卡住的场景。
- `--clean`：运行前清空 `--dir` 指定的数据目录。

测试通过时最后会输出：

```text
PASS_AFTER_CLOSE: total puts=..., elapsed_ms=..., throughput=... ops/s
```

`PASS_AFTER_CLOSE` 会在 `DBimpl` 析构、落盘 `manifest` 之后打印，所以它是最明确的成功标志。

## 更大数据量

如果想更激进一些：

```bash
./build/core_stress_test --clean --dir=./core_stress_data_big --threads=8 --ops_per_thread=200000 --value_size=4096 --read_every=2000 --post_ops=10000
```

这会写入约 6GB 逻辑数据量，请确认磁盘空间足够。

## 注意

`DBimpl` 当前使用相对路径存储 `manifest` 和 `sstable_x` 文件，所以测试程序会先 `chdir` 到 `--dir` 指定目录，再创建数据库文件，避免污染项目根目录。
