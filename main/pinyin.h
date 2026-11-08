#ifndef _PINYIN_H
#define _PINYIN_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PINYIN_RESULT 50   // 每个拼音最多返回的汉字数

// 拼音检索结果
typedef struct {
    char** hanzi_list;         // 汉字列表
    uint16_t count;            // 数量
    char buffer[MAX_PINYIN_RESULT * 2 + 2];  // 存储汉字字符串的缓冲区
} PinyinResult;

/**
 * @brief 根据拼音查找汉字
 * @param pinyin 小写拼音字符串，如 "bao", "zhong"
 * @param result 结果结构体
 * @return 找到的汉字数量，0表示未找到
 */
uint16_t search_by_pinyin(const char* pinyin, PinyinResult* result);

/**
 * @brief 获取第index个汉字
 * @param result 结果结构体
 * @param index 索引(0到count-1)
 * @return 汉字字符串指针
 */
const char* get_hanzi_at(PinyinResult* result, uint16_t index);

#endif