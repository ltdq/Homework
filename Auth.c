#include "Auth.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sodium.h>

#include "memory.h"

/* magic 固定写入文件头，用来识别本项目的加密存档格式。 */
#define AUTH_MAGIC "SSDBENC1"
/* magic 长度固定为 8 字节。 */
#define AUTH_MAGIC_SIZE 8
/* 当前文件格式版本。 */
#define AUTH_VERSION 1U
/* 派生出来的对称密钥长度。 */
#define AUTH_KEY_SIZE crypto_aead_xchacha20poly1305_ietf_KEYBYTES
/* XChaCha20-Poly1305 的 nonce 长度。 */
#define AUTH_NONCE_SIZE crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
/* AEAD 认证标签长度。 */
#define AUTH_TAG_SIZE crypto_aead_xchacha20poly1305_ietf_ABYTES
/* version 字段在文件头中的偏移。 */
#define AUTH_VERSION_OFFSET AUTH_MAGIC_SIZE
/* salt 字段在文件头中的偏移。 */
#define AUTH_SALT_OFFSET (AUTH_VERSION_OFFSET + 4)
/* flag_nonce 字段在文件头中的偏移。 */
#define AUTH_FLAG_NONCE_OFFSET (AUTH_SALT_OFFSET + crypto_pwhash_SALTBYTES)
/* payload_nonce 字段在文件头中的偏移。 */
#define AUTH_PAYLOAD_NONCE_OFFSET (AUTH_FLAG_NONCE_OFFSET + AUTH_NONCE_SIZE)
/* flag_size 字段在文件头中的偏移。 */
#define AUTH_FLAG_SIZE_OFFSET (AUTH_PAYLOAD_NONCE_OFFSET + AUTH_NONCE_SIZE)
/* payload_size 字段在文件头中的偏移。 */
#define AUTH_PAYLOAD_SIZE_OFFSET (AUTH_FLAG_SIZE_OFFSET + 8)
/* 文件头总长度。 */
#define AUTH_HEADER_SIZE (AUTH_PAYLOAD_SIZE_OFFSET + 8)

/* flag 明文固定写成这一串，用来判断密码是否正确。 */
static const unsigned char AUTH_FLAG_TEXT[] = "STUDENT_SYSTEM_AUTH_V1";

/* AuthHeader 表示文件头的逻辑字段。 */
typedef struct AuthHeader {
  /* salt 用于 Argon2id 派生。 */
  unsigned char salt[crypto_pwhash_SALTBYTES];
  /* flag_nonce 用于 flag 密文。 */
  unsigned char flag_nonce[AUTH_NONCE_SIZE];
  /* payload_nonce 用于真正数据密文。 */
  unsigned char payload_nonce[AUTH_NONCE_SIZE];
  /* flag 密文长度。 */
  uint64_t flag_size;
  /* payload 密文长度。 */
  uint64_t payload_size;
} AuthHeader;

/* AuthFileView 表示从整文件缓冲区切出来的只读视图。 */
typedef struct AuthFileView {
  /* header 保存解析后的文件头字段。 */
  AuthHeader header;
  /* header_bytes 指向原始头部字节，同时也作为 AAD。 */
  const unsigned char* header_bytes;
  /* flag_ciphertext 指向 flag 密文。 */
  const unsigned char* flag_ciphertext;
  /* payload_ciphertext 指向 payload 密文。 */
  const unsigned char* payload_ciphertext;
} AuthFileView;

/* auth_initialized 记录 sodium_init 是否已经成功执行过。 */
static int auth_initialized = 0;

static void write_u32_le(unsigned char* out, uint32_t value) {
  /* 逐字节写出 32 位无符号整数的小端表示。 */
  out[0] = (unsigned char)(value & 0xFFU);
  out[1] = (unsigned char)((value >> 8) & 0xFFU);
  out[2] = (unsigned char)((value >> 16) & 0xFFU);
  out[3] = (unsigned char)((value >> 24) & 0xFFU);
}

static void write_u64_le(unsigned char* out, uint64_t value) {
  /* 逐字节写出 64 位无符号整数的小端表示。 */
  for (int i = 0; i < 8; ++i) {
    out[i] = (unsigned char)((value >> (i * 8)) & 0xFFU);
  }
}

static uint32_t read_u32_le(const unsigned char* src) {
  /* 把 4 个字节按小端顺序拼成 32 位整数。 */
  return (uint32_t)src[0] | ((uint32_t)src[1] << 8) |
         ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

static uint64_t read_u64_le(const unsigned char* src) {
  /* 把 8 个字节按小端顺序拼成 64 位整数。 */
  uint64_t value = 0;

  for (int i = 0; i < 8; ++i) {
    value |= (uint64_t)src[i] << (i * 8);
  }
  return value;
}

static size_t auth_flag_plaintext_len(void) {
  /* flag 明文长度固定为字符串字节数。 */
  return sizeof(AUTH_FLAG_TEXT) - 1;
}

static size_t auth_flag_ciphertext_len(void) {
  /* flag 密文长度固定等于明文长度加认证标签长度。 */
  return auth_flag_plaintext_len() + AUTH_TAG_SIZE;
}

static size_t auth_min_file_size(void) {
  /* 合法文件至少要放下头部、flag 密文和空 payload 的认证标签。 */
  return AUTH_HEADER_SIZE + auth_flag_ciphertext_len() + AUTH_TAG_SIZE;
}

static void auth_wipe_buffer(void* buffer, size_t size) {
  /* 空缓冲区或空长度都不需要擦除。 */
  if (!buffer || size == 0) {
    return;
  }

  /* 使用 libsodium 的安全清零函数擦除敏感字节。 */
  sodium_memzero(buffer, size);
}

static void auth_wipe_key(unsigned char key[AUTH_KEY_SIZE]) {
  /* 对称密钥统一走这一层清零。 */
  auth_wipe_buffer(key, AUTH_KEY_SIZE);
}

static void auth_free_sensitive_buffer(unsigned char** buffer, size_t size) {
  /* 指针为空时，说明当前没有需要释放的缓冲区。 */
  if (!buffer || !*buffer) {
    return;
  }

  /* 先擦除，再释放。 */
  auth_wipe_buffer(*buffer, size);
  free(*buffer);
  *buffer = NULL;
}

static AuthStatus auth_init_internal(void) {
  /* 已经初始化成功时，直接返回。 */
  if (auth_initialized) {
    return AUTH_OK;
  }

  /* sodium_init 返回负数时，说明运行时初始化失败。 */
  if (sodium_init() < 0) {
    return AUTH_CRYPTO_ERROR;
  }

  /* 标记初始化完成。 */
  auth_initialized = 1;
  return AUTH_OK;
}

static AuthStatus derive_key(const char* password,
                             const unsigned char salt[crypto_pwhash_SALTBYTES],
                             unsigned char key[AUTH_KEY_SIZE]) {
  /* status 保存初始化结果。 */
  AuthStatus status;

  /* 密码为空指针或空串都没有派生意义。 */
  if (!password || password[0] == '\0') {
    return AUTH_INVALID_ARGUMENT;
  }

  /* 先确保 libsodium 已经准备好。 */
  status = auth_init_internal();
  if (status != AUTH_OK) {
    return status;
  }

  /* 用 Argon2id 从密码和 salt 派生固定长度的对称密钥。 */
  if (crypto_pwhash(key, AUTH_KEY_SIZE, password,
                    (unsigned long long)strlen(password), salt,
                    crypto_pwhash_OPSLIMIT_MODERATE,
                    crypto_pwhash_MEMLIMIT_MODERATE,
                    crypto_pwhash_ALG_ARGON2ID13) != 0) {
    return AUTH_CRYPTO_ERROR;
  }

  /* 返回成功。 */
  return AUTH_OK;
}

static void auth_prepare_header(AuthHeader* header, size_t payload_plaintext_len) {
  /* 生成随机 salt 和随机 nonce。 */
  randombytes_buf(header->salt, sizeof(header->salt));
  randombytes_buf(header->flag_nonce, sizeof(header->flag_nonce));
  randombytes_buf(header->payload_nonce, sizeof(header->payload_nonce));

  /* 写入固定的 flag 密文长度。 */
  header->flag_size = (uint64_t)auth_flag_ciphertext_len();

  /* 写入 payload 密文长度。 */
  header->payload_size = (uint64_t)(payload_plaintext_len + AUTH_TAG_SIZE);
}

static void auth_serialize_header(const AuthHeader* header,
                                  unsigned char out[AUTH_HEADER_SIZE]) {
  /* 先写入 magic 和版本号。 */
  memcpy(out, AUTH_MAGIC, AUTH_MAGIC_SIZE);
  write_u32_le(out + AUTH_VERSION_OFFSET, AUTH_VERSION);

  /* 再写入 salt 和两个 nonce。 */
  memcpy(out + AUTH_SALT_OFFSET, header->salt, sizeof(header->salt));
  memcpy(out + AUTH_FLAG_NONCE_OFFSET, header->flag_nonce,
         sizeof(header->flag_nonce));
  memcpy(out + AUTH_PAYLOAD_NONCE_OFFSET, header->payload_nonce,
         sizeof(header->payload_nonce));

  /* 最后写入两段密文长度。 */
  write_u64_le(out + AUTH_FLAG_SIZE_OFFSET, header->flag_size);
  write_u64_le(out + AUTH_PAYLOAD_SIZE_OFFSET, header->payload_size);
}

static AuthStatus auth_parse_file_view(const unsigned char* file_data,
                                       size_t file_size, AuthFileView* out_view) {
  /* flag_size 和 payload_size 保存头部中记录的长度。 */
  uint64_t flag_size;
  uint64_t payload_size;

  /* 输入参数必须完整有效。 */
  if (!file_data || !out_view) {
    return AUTH_INVALID_ARGUMENT;
  }

  /* 合法文件至少要满足最小长度要求。 */
  if (file_size < auth_min_file_size()) {
    return AUTH_FORMAT_ERROR;
  }

  /* magic 和版本号都要与当前程序的约定一致。 */
  if (memcmp(file_data, AUTH_MAGIC, AUTH_MAGIC_SIZE) != 0 ||
      read_u32_le(file_data + AUTH_VERSION_OFFSET) != AUTH_VERSION) {
    return AUTH_FORMAT_ERROR;
  }

  /* 从原始头部字节里读出两段密文长度。 */
  flag_size = read_u64_le(file_data + AUTH_FLAG_SIZE_OFFSET);
  payload_size = read_u64_le(file_data + AUTH_PAYLOAD_SIZE_OFFSET);

  /* flag 密文长度和 payload 密文长度都需要满足格式约束。 */
  if (flag_size != (uint64_t)auth_flag_ciphertext_len() ||
      payload_size < AUTH_TAG_SIZE ||
      flag_size > SIZE_MAX - AUTH_HEADER_SIZE ||
      payload_size > SIZE_MAX - AUTH_HEADER_SIZE - (size_t)flag_size ||
      AUTH_HEADER_SIZE + (size_t)flag_size + (size_t)payload_size != file_size) {
    return AUTH_FORMAT_ERROR;
  }

  /* header_bytes 指向整文件里的原始头部。 */
  out_view->header_bytes = file_data;

  /* 把文件头的逻辑字段复制出来。 */
  memcpy(out_view->header.salt, file_data + AUTH_SALT_OFFSET,
         sizeof(out_view->header.salt));
  memcpy(out_view->header.flag_nonce, file_data + AUTH_FLAG_NONCE_OFFSET,
         sizeof(out_view->header.flag_nonce));
  memcpy(out_view->header.payload_nonce, file_data + AUTH_PAYLOAD_NONCE_OFFSET,
         sizeof(out_view->header.payload_nonce));
  out_view->header.flag_size = flag_size;
  out_view->header.payload_size = payload_size;

  /* 给两段密文切出只读视图。 */
  out_view->flag_ciphertext = file_data + AUTH_HEADER_SIZE;
  out_view->payload_ciphertext =
      file_data + AUTH_HEADER_SIZE + (size_t)flag_size;

  /* 返回成功。 */
  return AUTH_OK;
}

static AuthStatus auth_encrypt_segment(
    const unsigned char key[AUTH_KEY_SIZE],
    const unsigned char nonce[AUTH_NONCE_SIZE], const unsigned char* aad,
    size_t aad_len, const unsigned char* plaintext, size_t plaintext_len,
    unsigned char** out_ciphertext, size_t* out_ciphertext_len) {
  /* ciphertext 保存加密输出缓冲区。 */
  unsigned char* ciphertext;

  /* ciphertext_len 保存实际加密后的长度。 */
  unsigned long long ciphertext_len = 0;

  /* 输出参数必须可写。 */
  if (!key || !nonce || !aad || !out_ciphertext || !out_ciphertext_len ||
      (plaintext_len > 0 && !plaintext)) {
    return AUTH_INVALID_ARGUMENT;
  }

  /* 先清空输出参数。 */
  *out_ciphertext = NULL;
  *out_ciphertext_len = 0;

  /* 为密文分配足够的空间。 */
  ciphertext = malloc(plaintext_len + AUTH_TAG_SIZE);
  memory_check(ciphertext);

  /* 调用 libsodium 完成 AEAD 加密。 */
  if (crypto_aead_xchacha20poly1305_ietf_encrypt(
          ciphertext, &ciphertext_len, plaintext,
          (unsigned long long)plaintext_len, aad, (unsigned long long)aad_len,
          NULL, nonce, key) != 0) {
    auth_free_sensitive_buffer(&ciphertext, plaintext_len + AUTH_TAG_SIZE);
    return AUTH_CRYPTO_ERROR;
  }

  /* 把结果返回给调用方。 */
  *out_ciphertext = ciphertext;
  *out_ciphertext_len = (size_t)ciphertext_len;
  return AUTH_OK;
}

static AuthStatus auth_decrypt_segment(
    const unsigned char key[AUTH_KEY_SIZE],
    const unsigned char nonce[AUTH_NONCE_SIZE], const unsigned char* aad,
    size_t aad_len, const unsigned char* ciphertext, size_t ciphertext_len,
    AuthStatus decrypt_failure_status, unsigned char** out_plaintext,
    size_t* out_plaintext_len) {
  /* plaintext 保存解密后的明文缓冲区。 */
  unsigned char* plaintext;

  /* plaintext_len 保存实际解密出的明文长度。 */
  unsigned long long plaintext_len = 0;

  /* 至少需要带上认证标签。 */
  if (!key || !nonce || !aad || !ciphertext || ciphertext_len < AUTH_TAG_SIZE ||
      !out_plaintext || !out_plaintext_len) {
    return AUTH_INVALID_ARGUMENT;
  }

  /* 先清空输出参数。 */
  *out_plaintext = NULL;
  *out_plaintext_len = 0;

  /* 为明文缓冲区分配空间，并额外预留一个 '\0'。 */
  plaintext = malloc(ciphertext_len - AUTH_TAG_SIZE + 1);
  memory_check(plaintext);

  /* 调用 libsodium 完成 AEAD 解密。 */
  if (crypto_aead_xchacha20poly1305_ietf_decrypt(
          plaintext, &plaintext_len, NULL, ciphertext,
          (unsigned long long)ciphertext_len, aad, (unsigned long long)aad_len,
          nonce, key) != 0) {
    auth_free_sensitive_buffer(&plaintext, ciphertext_len - AUTH_TAG_SIZE + 1);
    return decrypt_failure_status;
  }

  /* 手动补一个字符串结束符，方便调用方当作文本使用。 */
  plaintext[plaintext_len] = '\0';

  /* 把结果返回给调用方。 */
  *out_plaintext = plaintext;
  *out_plaintext_len = (size_t)plaintext_len;
  return AUTH_OK;
}

static AuthStatus auth_verify_flag(const unsigned char key[AUTH_KEY_SIZE],
                                   const AuthFileView* view) {
  /* flag_plaintext 保存解密后的 flag 明文。 */
  unsigned char flag_plaintext[sizeof(AUTH_FLAG_TEXT)] = {0};

  /* flag_plaintext_len 保存实际解密出的长度。 */
  unsigned long long flag_plaintext_len = 0;

  /* 视图为空时无法继续校验。 */
  if (!key || !view) {
    return AUTH_INVALID_ARGUMENT;
  }

  /* 先解密 flag 密文。 */
  if (crypto_aead_xchacha20poly1305_ietf_decrypt(
          flag_plaintext, &flag_plaintext_len, NULL, view->flag_ciphertext,
          (unsigned long long)view->header.flag_size, view->header_bytes,
          (unsigned long long)AUTH_HEADER_SIZE, view->header.flag_nonce,
          key) != 0) {
    auth_wipe_buffer(flag_plaintext, sizeof(flag_plaintext));
    return AUTH_PASSWORD_ERROR;
  }

  /* 长度和内容都匹配时，说明密码正确。 */
  if (flag_plaintext_len != auth_flag_plaintext_len() ||
      sodium_memcmp(flag_plaintext, AUTH_FLAG_TEXT,
                    auth_flag_plaintext_len()) != 0) {
    auth_wipe_buffer(flag_plaintext, sizeof(flag_plaintext));
    return AUTH_PASSWORD_ERROR;
  }

  /* 校验完成后擦除临时明文。 */
  auth_wipe_buffer(flag_plaintext, sizeof(flag_plaintext));
  return AUTH_OK;
}

static AuthStatus read_entire_file(const char* filename, unsigned char** out_data,
                                   size_t* out_size) {
  /* file 保存打开后的文件句柄。 */
  FILE* file;

  /* file_size_long 用来接收 ftell 返回的长度。 */
  long file_size_long;

  /* data 保存读取出来的整文件缓冲区。 */
  unsigned char* data;

  /* 输出参数为空时，直接返回非法参数。 */
  if (!filename || !out_data || !out_size) {
    return AUTH_INVALID_ARGUMENT;
  }

  /* 先把输出参数清零。 */
  *out_data = NULL;
  *out_size = 0;

  /* 以二进制读模式打开文件。 */
  file = fopen(filename, "rb");
  if (!file) {
    return AUTH_IO_ERROR;
  }

  /* 先跳到文件尾，计算总长度。 */
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return AUTH_IO_ERROR;
  }

  /* 读取文件总长度。 */
  file_size_long = ftell(file);
  if (file_size_long < 0) {
    fclose(file);
    return AUTH_IO_ERROR;
  }

  /* 再回到文件开头，准备读取完整内容。 */
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return AUTH_IO_ERROR;
  }

  /* 为空文件单独返回一块最小缓冲区，方便后续统一释放。 */
  if (file_size_long == 0) {
    data = malloc(1);
    memory_check(data);
    fclose(file);
    *out_data = data;
    *out_size = 0;
    return AUTH_OK;
  }

  /* 为整文件内容分配缓冲区。 */
  data = malloc((size_t)file_size_long);
  memory_check(data);

  /* 一次性读取完整文件。 */
  if (fread(data, 1, (size_t)file_size_long, file) != (size_t)file_size_long) {
    free(data);
    fclose(file);
    return AUTH_IO_ERROR;
  }

  /* 文件已经读完，可以关闭句柄。 */
  fclose(file);

  /* 把结果交给调用方。 */
  *out_data = data;
  *out_size = (size_t)file_size_long;
  return AUTH_OK;
}

static AuthStatus write_encrypted_file(const char* filename,
                                       const unsigned char header[AUTH_HEADER_SIZE],
                                       const unsigned char* flag_ciphertext,
                                       size_t flag_ciphertext_len,
                                       const unsigned char* payload_ciphertext,
                                       size_t payload_ciphertext_len) {
  /* file 保存目标文件句柄。 */
  FILE* file;

  /* write_ok 汇总三段写入是否全部成功。 */
  int write_ok;

  /* 输入参数需要完整有效。 */
  if (!filename || !header || !flag_ciphertext || !payload_ciphertext) {
    return AUTH_INVALID_ARGUMENT;
  }

  /* 以二进制写模式打开目标文件。 */
  file = fopen(filename, "wb");
  if (!file) {
    return AUTH_IO_ERROR;
  }

  /* 按固定顺序写入头部、flag 密文和 payload 密文。 */
  write_ok = fwrite(header, 1, AUTH_HEADER_SIZE, file) == AUTH_HEADER_SIZE &&
             fwrite(flag_ciphertext, 1, flag_ciphertext_len, file) ==
                 flag_ciphertext_len &&
             fwrite(payload_ciphertext, 1, payload_ciphertext_len, file) ==
                 payload_ciphertext_len;

  /* 关闭文件句柄，同时检查系统写回结果。 */
  if (fclose(file) != 0 || !write_ok) {
    remove(filename);
    return AUTH_IO_ERROR;
  }

  /* 返回成功。 */
  return AUTH_OK;
}

void auth_free_buffer(void* ptr) {
  /* 直接释放调用方传回来的缓冲区。 */
  free(ptr);
}

void auth_wipe_text(char* buffer, size_t size) {
  /* 明文文本也统一走安全清零。 */
  auth_wipe_buffer(buffer, size);
}

AuthStatus auth_save_file(const char* filename, const char* password,
                          const unsigned char* plaintext,
                          size_t plaintext_len) {
  /* status 保存每一步的执行结果。 */
  AuthStatus status;

  /* header 保存逻辑文件头。 */
  AuthHeader header;

  /* header_bytes 保存真正写入磁盘的头部字节。 */
  unsigned char header_bytes[AUTH_HEADER_SIZE];

  /* key 保存派生出来的对称密钥。 */
  unsigned char key[AUTH_KEY_SIZE] = {0};

  /* flag_ciphertext 保存加密后的 flag 密文。 */
  unsigned char* flag_ciphertext = NULL;

  /* payload_ciphertext 保存加密后的 payload 密文。 */
  unsigned char* payload_ciphertext = NULL;

  /* flag_ciphertext_len 和 payload_ciphertext_len 保存两段密文长度。 */
  size_t flag_ciphertext_len = 0;
  size_t payload_ciphertext_len = 0;

  /* 输入参数需要满足最基本的有效性要求。 */
  if (!filename || !password || password[0] == '\0' ||
      (plaintext_len > 0 && !plaintext)) {
    return AUTH_INVALID_ARGUMENT;
  }

  /* 先确保 libsodium 已经准备完成。 */
  status = auth_init_internal();
  if (status != AUTH_OK) {
    return status;
  }

  /* 生成随机文件头字段，并序列化成字节流。 */
  auth_prepare_header(&header, plaintext_len);
  auth_serialize_header(&header, header_bytes);

  /* 用密码和 salt 派生出对称密钥。 */
  status = derive_key(password, header.salt, key);
  if (status != AUTH_OK) {
    auth_wipe_key(key);
    return status;
  }

  /* 先加密 flag。 */
  status = auth_encrypt_segment(key, header.flag_nonce, header_bytes,
                                sizeof(header_bytes), AUTH_FLAG_TEXT,
                                auth_flag_plaintext_len(), &flag_ciphertext,
                                &flag_ciphertext_len);
  if (status == AUTH_OK && flag_ciphertext_len != (size_t)header.flag_size) {
    status = AUTH_CRYPTO_ERROR;
  }

  /* 再加密真正的 JSON 负载。 */
  if (status == AUTH_OK) {
    status = auth_encrypt_segment(key, header.payload_nonce, header_bytes,
                                  sizeof(header_bytes), plaintext, plaintext_len,
                                  &payload_ciphertext, &payload_ciphertext_len);
  }
  if (status == AUTH_OK &&
      payload_ciphertext_len != (size_t)header.payload_size) {
    status = AUTH_CRYPTO_ERROR;
  }

  /* 两段密文准备完成后，再统一写入文件。 */
  if (status == AUTH_OK) {
    status = write_encrypted_file(filename, header_bytes, flag_ciphertext,
                                  flag_ciphertext_len, payload_ciphertext,
                                  payload_ciphertext_len);
  }

  /* 返回前统一清理敏感缓冲区。 */
  auth_wipe_key(key);
  auth_free_sensitive_buffer(&flag_ciphertext, flag_ciphertext_len);
  auth_free_sensitive_buffer(&payload_ciphertext, payload_ciphertext_len);
  return status;
}

AuthStatus auth_load_file(const char* filename, const char* password,
                          unsigned char** out_plaintext,
                          size_t* out_plaintext_len) {
  /* status 保存每一步的执行结果。 */
  AuthStatus status;

  /* file_data 保存整个加密文件内容。 */
  unsigned char* file_data = NULL;

  /* file_size 保存整文件字节数。 */
  size_t file_size = 0;

  /* view 保存解析后的只读文件视图。 */
  AuthFileView view;

  /* key 保存派生出来的对称密钥。 */
  unsigned char key[AUTH_KEY_SIZE] = {0};

  /* payload_plaintext 保存解密后的 JSON 明文。 */
  unsigned char* payload_plaintext = NULL;

  /* payload_plaintext_len 保存 JSON 明文长度。 */
  size_t payload_plaintext_len = 0;

  /* 输出参数为空时，没有办法把结果返回给调用方。 */
  if (!filename || !password || password[0] == '\0' || !out_plaintext ||
      !out_plaintext_len) {
    return AUTH_INVALID_ARGUMENT;
  }

  /* 先把输出参数清零。 */
  *out_plaintext = NULL;
  *out_plaintext_len = 0;

  /* 整个加载流程都依赖 libsodium 运行时。 */
  status = auth_init_internal();
  if (status != AUTH_OK) {
    return status;
  }

  /* 先把整个文件读入内存。 */
  status = read_entire_file(filename, &file_data, &file_size);
  if (status != AUTH_OK) {
    return status;
  }

  /* 再解析文件头，切出两段密文视图。 */
  status = auth_parse_file_view(file_data, file_size, &view);

  /* 文件头通过后，用密码和 salt 派生密钥。 */
  if (status == AUTH_OK) {
    status = derive_key(password, view.header.salt, key);
  }

  /* 先解密 flag，只有校验成功才会继续读真正数据。 */
  if (status == AUTH_OK) {
    status = auth_verify_flag(key, &view);
  }

  /* 最后解密 payload，拿到 JSON 明文。 */
  if (status == AUTH_OK) {
    status = auth_decrypt_segment(
        key, view.header.payload_nonce, view.header_bytes, AUTH_HEADER_SIZE,
        view.payload_ciphertext, (size_t)view.header.payload_size,
        AUTH_CRYPTO_ERROR, &payload_plaintext, &payload_plaintext_len);
  }

  /* 返回前统一清理密钥和整文件缓冲区。 */
  auth_wipe_key(key);
  auth_free_sensitive_buffer(&file_data, file_size > 0 ? file_size : 1);

  /* 解密失败时，明文缓冲区也一并清理。 */
  if (status != AUTH_OK) {
    auth_free_sensitive_buffer(&payload_plaintext, payload_plaintext_len + 1);
    return status;
  }

  /* 成功时把明文交给调用方。 */
  *out_plaintext = payload_plaintext;
  *out_plaintext_len = payload_plaintext_len;
  return AUTH_OK;
}
