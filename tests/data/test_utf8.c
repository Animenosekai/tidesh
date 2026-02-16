#include <stdlib.h>
#include <string.h>
#include "data/utf8.h"
#include "snow/snow.h"

describe(utf8) {
    it("should get correct charlen for ASCII") {
        asserteq_int(utf8_charlen('a'), 1);
        asserteq_int(utf8_charlen('Z'), 1);
        asserteq_int(utf8_charlen('0'), 1);
    }

    it("should get correct charlen for multi-byte characters") {
        // Two-byte character (110xxxxx)
        asserteq_int(utf8_charlen((char)0xC0), 2);
        asserteq_int(utf8_charlen((char)0xDF), 2);
        
        // Three-byte character (1110xxxx)
        asserteq_int(utf8_charlen((char)0xE0), 3);
        asserteq_int(utf8_charlen((char)0xEF), 3);
        
        // Four-byte character (11110xxx)
        asserteq_int(utf8_charlen((char)0xF0), 4);
        asserteq_int(utf8_charlen((char)0xF7), 4);
    }

    it("should get correct charlen for continuation bytes") {
        // Continuation bytes (10xxxxxx)
        asserteq_int(utf8_charlen((char)0x80), 1);
        asserteq_int(utf8_charlen((char)0xBF), 1);
    }

    it("should count ASCII string length") {
        asserteq_int(utf8_strlen("hello"), 5);
        asserteq_int(utf8_strlen("test"), 4);
        asserteq_int(utf8_strlen(""), 0);
    }

    it("should count mixed string length") {
        // "café" = 4 characters
        asserteq(utf8_strlen("café"), 4);
    }

    it("should work with CJK characters") {
        // "日本語" = 3 characters
        asserteq(utf8_strlen("日本語"), 3);
    }

    it("should navigate to next character in ASCII") {
        char str[] = "hello";
        char *p = str;
        
        p = utf8_next_char(p);
        asserteq_int(*p, 'e');
        
        p = utf8_next_char(p);
        asserteq_int(*p, 'l');
    }

    it("should navigate to next character with multi-byte") {
        char str[] = "hé";
        char *p = str;
        
        // Move from 'h' to 'é'
        p = utf8_next_char(p);
        // p should now be pointing to first byte of 'é'
        assertneq(p, str);
    }

    it("should navigate to previous character in ASCII") {
        char str[] = "hello";
        char *p = str + 5;
        char *start = str;
        
        p = utf8_prev_char(p, start);
        asserteq_int(*p, 'o');
        
        p = utf8_prev_char(p, start);
        asserteq_int(*p, 'l');
    }

    it("should return NULL at start of string for prev_char") {
        char str[] = "hello";
        char *p = str;
        char *start = str;
        
        char *result = utf8_prev_char(p, start);
        asserteq(result, NULL);
    }

    it("should handle empty string") {
        asserteq(utf8_strlen(""), 0);
    }

    it("should handle single character") {
        asserteq(utf8_strlen("a"), 1);
    }

    it("should handle UTF-8 emoji") {
        // Test with actual 4-byte UTF-8 character
        // Using "🎉" which is U+1F389 (F0 9F 8E 89)
        char str[] = "🎉";
        asserteq(utf8_strlen(str), 1);
    }

    it("should navigate correctly through UTF-8 string") {
        char str[] = "café";
        char *p = str;
        
        // Count characters by navigating
        int count = 0;
        while (*p != '\0') {
            count++;
            p = utf8_next_char(p);
        }
        
        asserteq(count, 4);
    }

    it("should correctly identify ASCII in charlen") {
        assert(utf8_charlen('a') == 1);
        assert(utf8_charlen(' ') == 1);
        assert(utf8_charlen('\n') == 1);
    }
}
