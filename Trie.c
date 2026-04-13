#include "Trie.h"

#include <stdlib.h>
#include <string.h>

#include "Data.h"
#include "memory.h"

/* root 是整棵前缀树的根节点。 */
static TrieNode* root = NULL;

static TrieNode* create_trie_node(void) {
  /* calloc 会把 children、data、data_count 全部初始化为 0。 */
  TrieNode* node = calloc(1, sizeof(*node));

  /* 统一检查分配是否成功。 */
  memory_check(node);

  /* 返回新建好的空节点。 */
  return node;
}

static TrieNode* trie_walk(TrieNode* node, const unsigned char* key) {
  /* 路径中断时直接返回 NULL，表示这个前缀不存在。 */
  if (!node) {
    return NULL;
  }

  /* 读到字符串结束符时，说明已经走到目标前缀对应的节点。 */
  if (*key == '\0') {
    return node;
  }

  /* 按当前字节继续向下一层递归。 */
  return trie_walk(node->children[*key], key + 1);
}

static TrieNode* trie_ensure_path(TrieNode* node, const unsigned char* key) {
  /* 路径全部建立完成后，当前节点就是最终落点。 */
  if (*key == '\0') {
    return node;
  }

  /* 当前字节对应的子节点不存在时，先创建出来。 */
  if (!node->children[*key]) {
    node->children[*key] = create_trie_node();
  }

  /* 继续为剩余字节确保路径存在。 */
  return trie_ensure_path(node->children[*key], key + 1);
}

static void free_trie_node(TrieNode* node) {
  /* 空指针直接返回，递归边界在这里结束。 */
  if (!node) {
    return;
  }

  /* 先递归释放所有子树。 */
  for (int i = 0; i < 256; ++i) {
    free_trie_node(node->children[i]);
  }

  /* 释放当前节点保存的结果数组。 */
  free(node->data);

  /* 最后释放当前节点本身。 */
  free(node);
}

static int trie_node_is_empty(TrieNode* node) {
  /* 当前节点上还挂着完整姓名记录时，这个节点就不能删。 */
  if (node->data_count != 0) {
    return 0;
  }

  /* 只要还有任意一个孩子存在，这个节点仍然承担路径作用。 */
  for (int i = 0; i < 256; ++i) {
    if (node->children[i]) {
      return 0;
    }
  }

  /* 没有数据也没有孩子，说明节点已经完全空了。 */
  return 1;
}

void trie_init(void) {
  /* 先释放旧树，避免重新初始化时发生内存泄漏。 */
  free_trie_node(root);

  /* 重新创建根节点，作为一棵空树的起点。 */
  root = create_trie_node();
}

void trie_insert(DataNode* node) {
  /* 姓名按 UTF-8 字节序列作为 Trie 的键。 */
  const unsigned char* key = (const unsigned char*)node->name;

  /* 确保整条路径存在，并拿到终点节点。 */
  TrieNode* cur = trie_ensure_path(root, key);

  /* 扩容终点节点的数据数组，为新记录腾出一个位置。 */
  cur->data = realloc(cur->data, sizeof(DataNode*) * (cur->data_count + 1));

  /* 检查 realloc 是否成功。 */
  memory_check(cur->data);

  /* 把记录指针放到数组末尾。 */
  cur->data[cur->data_count] = node;

  /* 更新有效数量。 */
  cur->data_count++;
}

static int trie_delete_recursive(TrieNode* current, const unsigned char* key,
                                 DataNode* node) {
  /* 当前路径已经断开，说明目标节点不存在。 */
  if (!current) {
    return 0;
  }

  /* 走到字符串结尾时，当前节点就是对应完整姓名的终点。 */
  if (*key == '\0') {
    /* 在终点节点的数据数组中找到目标记录指针。 */
    for (int i = 0; i < current->data_count; ++i) {
      if (current->data[i] == node) {
        /* 用最后一个元素覆盖当前位置，避免整体搬移数组。 */
        current->data[i] = current->data[current->data_count - 1];

        /* 数量减一，逻辑上删除成功。 */
        current->data_count--;

        /* 报告上层：删除动作已经完成。 */
        return 1;
      }
    }

    /* 到了终点但没找到指针，说明这条记录并不在这里。 */
    return 0;
  }

  /* index 保存当前这一层要走向哪个子节点。 */
  unsigned char index = *key;

  /* child 是递归进入的下一层。 */
  TrieNode* child = current->children[index];

  /* 继续向更深层删除。 */
  int deleted = trie_delete_recursive(child, key + 1, node);

  /* 更深层没有删掉目标时，当前层直接向上返回失败。 */
  if (!deleted) {
    return 0;
  }

  /* 子节点在删除后已经完全空闲时，可以顺手把整棵空子树释放掉。 */
  if (child && trie_node_is_empty(child)) {
    free_trie_node(child);
    current->children[index] = NULL;
  }

  /* 当前层向上报告删除成功。 */
  return 1;
}

void trie_delete(DataNode* node) {
  /* 删除路径仍然按姓名的 UTF-8 字节序列行走。 */
  const unsigned char* key = (const unsigned char*)node->name;

  /* 根节点开始递归删除。 */
  trie_delete_recursive(root, key, node);
}

static void collect(TrieNode* node, DataNode*** result, int* count, int* cap) {
  /* 空节点没有可收集内容。 */
  if (!node) {
    return;
  }

  /* 先收集当前节点挂着的完整姓名记录。 */
  for (int i = 0; i < node->data_count; i++) {
    /* 当前结果数组容量不够时进行扩容。 */
    if (*count >= *cap) {
      /* 初次扩容到 8，后续按 1.25 倍增长。 */
      *cap = (*cap < 8) ? 8 : (*cap + (*cap >> 2));

      /* 扩容结果数组。 */
      *result = realloc(*result, sizeof(DataNode*) * (*cap));

      /* 检查扩容结果。 */
      memory_check(*result);
    }

    /* 把当前记录指针放入结果数组。 */
    (*result)[*count] = node->data[i];

    /* 结果数量加一。 */
    (*count)++;
  }

  /* 再递归收集所有子树里的记录。 */
  for (int i = 0; i < 256; ++i) {
    collect(node->children[i], result, count, cap);
  }
}

DataNode** trie_search(const char* prefix, int* count) {
  /* 查询前缀同样按 UTF-8 字节序列处理。 */
  const unsigned char* key = (const unsigned char*)prefix;

  /* cur 用来保存前缀对应的终点节点。 */
  TrieNode* cur;

  /* 先把输出数量清零，保证失败路径也能返回稳定结果。 */
  *count = 0;

  /* 沿着前缀走到对应节点。 */
  cur = trie_walk(root, key);

  /* 前缀不存在时直接返回 NULL。 */
  if (!cur) {
    return NULL;
  }

  /* 结果数组一开始为空，collect 会按需扩容。 */
  DataNode** result = NULL;

  /* cap 记录 result 当前容量。 */
  int cap = 0;

  /* 从前缀节点开始，把整棵子树内的记录全部收集出来。 */
  collect(cur, &result, count, &cap);

  /* 返回动态分配的结果数组。 */
  return result;
}
