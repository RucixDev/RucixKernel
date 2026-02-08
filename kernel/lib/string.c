#include "string.h"

void* memset(void* ptr, int value, size_t num) {
     
    uint64_t* p64 = (uint64_t*)ptr;
    uint8_t* p8;
    uint64_t val64;

     
    val64 = (uint8_t)value;
    val64 |= val64 << 8;
    val64 |= val64 << 16;
    val64 |= val64 << 32;

     
    p8 = (uint8_t*)ptr;
    while ((uintptr_t)p8 & 0x7 && num > 0) {
        *p8++ = (uint8_t)value;
        num--;
    }

     
    p64 = (uint64_t*)p8;
    while (num >= 8) {
        *p64++ = val64;
        num -= 8;
    }

     
    p8 = (uint8_t*)p64;
    while (num--) {
        *p8++ = (uint8_t)value;
    }

    return ptr;
}

void* memcpy(void* dest, const void* src, size_t num) {
    uint8_t* d8 = (uint8_t*)dest;
    const uint8_t* s8 = (const uint8_t*)src;
    uint64_t* d64;
    const uint64_t* s64;

     
    if (num < 8) {
        while (num--) *d8++ = *s8++;
        return dest;
    }

     
    while ((uintptr_t)d8 & 0x7 && num > 0) {
        *d8++ = *s8++;
        num--;
    }

     
    d64 = (uint64_t*)d8;
    s64 = (const uint64_t*)s8;
    while (num >= 8) {
        *d64++ = *s64++;
        num -= 8;
    }

     
    d8 = (uint8_t*)d64;
    s8 = (const uint8_t*)s64;
    while (num--) {
        *d8++ = *s8++;
    }

    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    const uint8_t* p1 = (const uint8_t*)ptr1;
    const uint8_t* p2 = (const uint8_t*)ptr2;
    while (num--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    while (n > 0) {
        *d++ = '\0';
        n--;
    }
    return dest;
}

static char tolower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && (tolower(*s1) == tolower(*s2))) {
        s1++;
        s2++;
    }
    return (unsigned char)tolower(*s1) - (unsigned char)tolower(*s2);
}
