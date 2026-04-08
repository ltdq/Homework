# 学生信息管理系统

一个基于 C 语言实现的终端学生信息管理系统，使用双向链表、哈希表、前缀树和操作栈实现高效 CRUD、前缀检索和撤销操作，并使用 JSON 文件进行持久化。

## 核心特性

- 平均 O(1) 的 ID 查询、插入、删除（哈希表）
- 保留插入顺序的双向链表存储（便于遍历与保存）
- 基于姓名前缀的快速搜索（Trie）
- 支持撤销最近一次操作（单链表栈）
- TUI 交互界面，支持方向键与多步骤输入

## 数据结构设计

课程作业目标：围绕链表组织多个数据结构。

1. 双向链表 + 哈希表组合实现 CRUD
2. 哈希桶使用单向链表解决冲突
3. 操作历史栈使用单向链表实现
4. 已访问节点集合使用单向链表实现
5. 姓名索引使用多叉链表（前缀树）实现

## 模块图

```mermaid
flowchart LR
	MAIN[main.c] --> DISP[Display 模块<br>Display.c/.h]

	DISP --> DATA[Data 模块<br>Data.c/.h]
	DISP --> LOG[Log 模块<br>Log.c/.h]
	DISP --> TRIE[Trie 模块<br>Trie.c/.h]

	DATA --> HASH[Hash 模块<br>Hash.c/.h]
	DATA --> LIST[List 模块<br>List.c/.h]
	DATA --> STACK[Stack 模块<br>Stack.c/.h]
	DATA --> TRIE
	DATA --> LOG
	DATA --> MEM[memory 模块<br>memory.c/.h]
	DATA --> JSON[yyjson<br>yyjson.c/.h]

	HASH --> RH[rapidhash.h]
```

## 程序流程图

```mermaid
flowchart TD
	A[程序启动] --> B[main]
	B --> C[display_init]
	C --> C1[初始化终端/日志/数据结构]
	C1 --> D{display_run 循环}

	D --> E[重绘界面 + 读取按键]
	E --> F{当前状态}

	F -->|菜单状态| G{选择菜单项}
	G -->|Undo| G1[data_undo]
	G -->|Exit| Z[退出循环]
	G -->|其他操作| H[进入输入状态]

	F -->|输入状态| I[处理输入/方向键/ESC]
	I --> J{是否回车确认}
	J -->|否| D
	J -->|是| K{是否使用搜索候选}
	K -->|是| L[按候选执行 GET/DELETE/MODIFY]
	K -->|否| M[process_input]

	M --> N{操作类型}
	N -->|INSERT| N1[data_insert]
	N -->|GET| N2[data_get + 弹窗显示]
	N -->|DELETE| N3[data_delete]
	N -->|MODIFY| N4[data_modify]
	N -->|SAVE/LOAD/NEW| N5[data_save / data_load / data_new]

	G1 --> D
	L --> D
	N1 --> D
	N2 --> D
	N3 --> D
	N4 --> D
	N5 --> D

	Z --> P[display_cleanup]
	P --> Q[程序结束]
```

## 构建与运行

```bash
make
make run
make clean
```

## Third-Party Dependencies

本项目使用了以下第三方开源组件：

| Name | Purpose | Version | Upstream | License |
|---|---|---|---|---|
| yyjson | JSON 解析与序列化 | 0.12.0 | https://github.com/ibireme/yyjson | MIT |
| rapidhash | 高性能哈希算法 | V3 | https://github.com/Nicoshev/rapidhash | MIT |

### License Notice

- `yyjson` 和 `rapidhash` 的版权与许可证归其原作者所有。
- 本项目仅用于课程学习与实验，遵循各上游项目许可证要求。
- 如需发布或分发，请保留上游 `LICENSE` 声明并在发布说明中标注第三方来源。

## TODO

- [ ] 预测最想访问（计划尝试 ARC 类思路）
