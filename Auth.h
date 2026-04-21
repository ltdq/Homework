#ifndef AUTH_H
#define AUTH_H

#include <stddef.h>

/*
 * AuthStatus 用来描述认证和加解密流程的最终结果。
 * 显示层只需要读取这个状态，就能决定提示用户什么信息。
 */
typedef enum AuthStatus {
  /* 操作执行成功。 */
  AUTH_OK = 0,
  /* 参数为空、密码为空，或者缓冲区配置不合法。 */
  AUTH_INVALID_ARGUMENT,
  /* 文件无法打开、读取失败，或者写回磁盘失败。 */
  AUTH_IO_ERROR,
  /* 文件头不符合约定，或者密文容器结构损坏。 */
  AUTH_FORMAT_ERROR,
  /* flag 校验失败，说明密码错误。 */
  AUTH_PASSWORD_ERROR,
  /* libsodium 初始化失败，或者加解密内部出错。 */
  AUTH_CRYPTO_ERROR,
} AuthStatus;

/*
 * 把一段明文数据加密后写入指定文件。
 * password 是用户输入的明文密码。
 */
AuthStatus auth_save_file(const char* filename, const char* password,
                          const unsigned char* plaintext, size_t plaintext_len);

/*
 * 读取指定文件，完成密码校验和密文解密，并返回明文缓冲区。
 * 调用方负责在使用完明文后先清零，再调用 auth_free_buffer 释放。
 */
AuthStatus auth_load_file(const char* filename, const char* password,
                          unsigned char** out_plaintext,
                          size_t* out_plaintext_len);

/*
 * 释放 Auth 返回的动态缓冲区。
 */
void auth_free_buffer(void* ptr);

/*
 * 清零一段敏感文本缓冲区。
 * 显示层会用它擦除密码输入框的明文内容。
 */
void auth_wipe_text(char* buffer, size_t size);

#endif
