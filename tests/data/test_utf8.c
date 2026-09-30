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
        // Two-byte character (110xxxxx: 0xC2 to 0xDF)
        asserteq_int(utf8_charlen((char)0xC2), 2);
        asserteq_int(utf8_charlen((char)0xDF), 2);
        
        // Three-byte character (1110xxxx: 0xE0 to 0xEF)
        asserteq_int(utf8_charlen((char)0xE0), 3);
        asserteq_int(utf8_charlen((char)0xEF), 3);
        
        // Four-byte character (11110xxx: 0xF0 to 0xF4)
        asserteq_int(utf8_charlen((char)0xF0), 4);
        asserteq_int(utf8_charlen((char)0xF4), 4);
    }

    it("should reject invalid lead bytes in charlen") {
        // Overlong 2-byte lead bytes (0xC0, 0xC1)
        asserteq_int(utf8_charlen((char)0xC0), 1);
        asserteq_int(utf8_charlen((char)0xC1), 1);

        // Out-of-range 4-byte lead bytes (0xF5 to 0xF7)
        asserteq_int(utf8_charlen((char)0xF5), 1);
        asserteq_int(utf8_charlen((char)0xF7), 1);

        // Obsolete 5/6 byte lead bytes
        asserteq_int(utf8_charlen((char)0xF8), 1);
        asserteq_int(utf8_charlen((char)0xFF), 1);
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

    it("should handle truncated strings safely without buffer overread") {
        // Incomplete 2-byte sequence
        char trunc2[] = {'h', 'i', (char)0xC2, '\0'};
        asserteq(utf8_strlen(trunc2), 3);
        asserteq_ptr(utf8_next_char(trunc2 + 2), trunc2 + 3);

        // Incomplete 3-byte sequence
        char trunc3[] = {(char)0xE0, (char)0xA0, '\0'};
        asserteq(utf8_strlen(trunc3), 2);

        // Incomplete 4-byte sequence
        char trunc4[] = {(char)0xF0, (char)0x90, '\0'};
        asserteq(utf8_strlen(trunc4), 2);
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
        asserteq_ptr(p, str + 1);

        // Move past 'é'
        p = utf8_next_char(p);
        asserteq_ptr(p, str + strlen("hé"));
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

    it("should navigate to previous character with multi-byte and emoji") {
        char str[] = "café";
        char *start = str;
        char *p = str + strlen(str); // at '\0'

        p = utf8_prev_char(p, start);
        asserteq_ptr(p, str + 3); // start of 'é'

        p = utf8_prev_char(p, start);
        asserteq_int(*p, 'f');

        // Emoji test (4-byte)
        char emoji_str[] = "a🎉b";
        char *ep = emoji_str + strlen(emoji_str); // at '\0'
        ep = utf8_prev_char(ep, emoji_str);
        asserteq_int(*ep, 'b');

        ep = utf8_prev_char(ep, emoji_str);
        asserteq_ptr(ep, emoji_str + 1); // start of emoji 🎉

        ep = utf8_prev_char(ep, emoji_str);
        asserteq_int(*ep, 'a');
    }

    it("should handle orphaned continuation bytes gracefully in prev_char") {
        char malformed[] = {'A', (char)0x80, 'B', '\0'};
        char *p = malformed + 2; // at 'B'
        char *prev = utf8_prev_char(p, malformed);
        // Should land on the invalid 0x80 byte, NOT skip 'A'
        asserteq_ptr(prev, malformed + 1);
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

    it("should calculate terminal display width correctly") {
        // ASCII
        asserteq_int(utf8_width("hello"), 5);
        asserteq_int(utf8_charwidth("a"), 1);

        // Accents (single width)
        asserteq_int(utf8_width("café"), 4);

        // CJK characters (double width each)
        asserteq_int(utf8_width("日本語"), 6);
        asserteq_int(utf8_charwidth("日"), 2);

        // Emoji (double width)
        asserteq_int(utf8_width("🎉"), 2);
        asserteq_int(utf8_charwidth("🎉"), 2);
        asserteq_int(utf8_width("hello 🎉 world"), 14);

        // Zero-width
        asserteq_int(utf8_charwidth("\x20\x0D"), 1); // space
    }
}
