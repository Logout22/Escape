#include <stdio.h>
#include <inttypes.h>

uint32_t crc_table[256];

/* from RFC 2083 */
void make_crc_table() {
    uint32_t c;
    int n, k;
    for (n = 0; n < 256; n++) {
        c = (uint32_t) n;
        for (k = 0; k < 8; k++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
}

void print_crc_table() {
    int i, j;
    printf("uint32_t crc_table[] = {\n");
    for (i = 0; i < 32; i++) {
        printf("    ");
        for (j = 0; j < 8; j++) {
            printf("0x%08X,", crc_table[i * 8 + j]);
            if (j != 15) printf(" ");
        }
        printf("\n");
    }
    printf("};\n");
}

int main(int argc, char** argv) {
    make_crc_table();
    print_crc_table();
    return 0;
}

