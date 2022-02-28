#include <stdio.h>
#include <string.h>

typedef char byte;

typedef union {
    short s;
    byte  bytes[2];
} ShortBytes;


int
EncodeShort(short s, byte *bytes) {
    ShortBytes sb;
    sb.s = s;
    memcpy(bytes, sb.bytes, 2);
    return 2;
}

short
DecodeShort(byte *bytes) {
    ShortBytes sb;
    memcpy(sb.bytes, bytes, 2);
    return sb.s;
}

int
EncodeCString(char *str, byte *bytes, int max_len) {
    int len = strlen(str);
    if (len + 2 > max_len) {
	len = max_len - 2;
    }
    EncodeShort((short)len, bytes);
    memcpy(bytes+2, str, len);
    return len+2;
}




int
DecodeCString(byte *bytes, char *str, int max_len) {
    int len = DecodeShort(bytes);
    if (len + 1 > max_len) { // account for null terminator.
	len = max_len - 1; 
    }
    printf("%d\n", len);
    memcpy(str, bytes+2, len);
    str[len] = '\0';
    return len;
}


int main(){
    char s[40];
    char str[] = "Hello";
    char m[] = "gealo";
    int u = EncodeCString(str, s, 20);
    int k = DecodeCString(s, m, 20);
    printf("%d\n%d\n", u, k);
    printf("%s\n", s);
}
