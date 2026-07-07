# WAL 教学版说明

本项目新增单文件教学版 WAL：`wal.log`。

## 写入流程

`DBimpl::Write()` 合并批量写并分配 sequence 后，会先把完整 `WriteBatch` 原始二进制内容追加到 `wal.log`，再 `Flush + Sync`，最后写入 `Memtable`。

WAL record 格式：

```text
fixed32 payload_length
fixed32 checksum
payload = WriteBatch::rep_
```

## 恢复流程

数据库启动时先读取 `manifest` 恢复 SSTable 元数据和 `last_sequence_`，再顺序读取 `wal.log`：

- 已被 manifest 覆盖的 batch 会跳过；
- 新 batch 会重放到 `Memtable`；
- 尾部半条记录或 checksum 错误会安全停止，并截断到最后一条完整记录；
- 正常析构时会先 flush memtable、写入 manifest，然后删除 `wal.log`。

## 运行测试

```bash
cmake --build build --target wal_recovery_test
./build/wal_recovery_test
```

测试会使用子进程写入后 `_exit()`，跳过 `DBimpl` 析构，模拟进程崩溃；父进程重新打开数据库，验证 WAL replay 可以恢复数据。
