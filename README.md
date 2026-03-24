# 学生信息管理系统

#### 使用链表 + 哈希表实现查询O(1)，添加O(1)，删除O(1)；使用基于链表的栈维护历史操作；使用基于多节点链表的前缀树维护姓名索引；使用 Json 存储用户文件。

## 引入了 `rapidhash V3` 算法，或许是当前速度最快、碰撞率最低的不加密 Hash 算法
### 引入了 `yyjson 0.12.0` 处理 Json，也是极小极快的 Json 库

学校的作业：链表的使用
1、使用双向链表 + 哈希表来 `CRUD`
2、哈希表的桶使用单向链表
3、维护历史操作记录的栈使用单向链表实现
4、维护已访问节点使用单向链表储存
5、维护姓名索引使用前缀树（字典树），属于多节点链表

## TODO：
- [x] ~~GUI~~ TUI，GUI还是太吃操作了 AI Power!
- [ ] 最近访问记录/预测最想访问（预计使用类似 ARC 的算法）

## Third-Party Dependencies

**本项目使用了以下第三方开源组件：**

| Name | Purpose | Version | Upstream | License |
|---|---|---|---|---|
| yyjson | JSON 解析与序列化 | 0.12.0 | https://github.com/ibireme/yyjson | MIT |
| rapidhash | 高性能哈希算法 | V3 | https://github.com/Nicoshev/rapidhash | MIT |

### License Notice

- `yyjson` 和 `rapidhash` 的版权与许可证归其原作者所有。
- 本项目仅用于课程学习与实验，遵循各上游项目许可证要求。
- 如需发布/分发，请保留上游 `LICENSE` 声明并在发布说明中标注第三方来源。

### WIP
