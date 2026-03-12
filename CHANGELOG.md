# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Breaking Changes

#### 触觉数据API重构 (Tactile Data API Refactor)

**移除的API (Removed APIs):**
- `int DexHand::GetResultantForce(FingerType finger_type, std::vector<Force>* resultant_forces)`
- `int DexHand::GetSampleForce(FingerType finger_type, std::vector<Force>* sample_forces)`
- `void DexHand::SetForceCallback(ForceCallback callback)`

**新增的API (New APIs):**
- `void DexHand::SetTactileDataCallback(TactileDataCallback callback)`
- `struct TactileData` - 包含数据类型、手指类型和力向量的统一数据结构
- `enum class ForceType` - 用于区分合力 (RESULTANT) 和分布力 (SAMPLE)

**影响 (Impact):**
- 使用旧API获取触觉数据的代码将无法编译
- 需要从 Pull 模式（主动查询）迁移到 Push 模式（事件驱动回调）

### Added

#### 触觉数据实时推送功能
- 新增 `SetTactileDataCallback()` 方法，支持基于回调的触觉数据推送
- 新增 `ForceType` 枚举，用于区分合力和分布力数据
- 新增 `TactileData` 结构体，统一封装触觉数据：
  - `type`: 数据类型（合力/分布力）
  - `finger`: 手指类型
  - `forces`: 力数据向量
  - 便利方法：`IsResultant()`, `IsSample()`, `SensorCount()`
- 新增 `ToString(ForceType)` 和 `ToString(FingerType)` 工具函数

#### 示例代码
- 新增 `examples/tactile_callback_example.cc` - 演示触觉回调的完整示例
- 更新 `examples/basic_connection.cc` - 添加触觉回调使用演示

#### 文档
- 新增 `docs/MIGRATION_GUIDE.md` - 详细的API迁移指南
- 更新 README.md - 添加触觉回调功能说明

### Changed

#### 架构改进
- 从 Pull 模式（轮询）改为 Push 模式（事件驱动）
- 改进实时性：数据到达立即触发回调，无需等待轮询
- 降低 CPU 使用率：移除轮询开销
- 提升扩展性：通过 `ForceType` 枚举便于添加新的力数据类型

### Migration Guide

如果您的代码使用了旧的触觉数据API，请参考迁移指南：

**📄 完整迁移指南:** [docs/MIGRATION_GUIDE.md](docs/MIGRATION_GUIDE.md)

**快速示例：**

```cpp
// ❌ 旧代码（已移除）
std::vector<Force> forces;
hand.GetResultantForce(THUMB, &forces);

// ✅ 新代码
hand.SetTactileDataCallback([](const TactileData& data) {
    if (data.type == ForceType::RESULTANT && data.finger == THUMB) {
        // 处理合力数据
    }
});
```

---

## [Previous Versions]

*Previous versions were not tracked in the changelog.*
