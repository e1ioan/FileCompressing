//--------------------------------------------------------------------------
//  Project      : instal.prj
//  File Name    : instal.cpp
//  Created      : 14.03.1994
//  Updated      : 14.03.1994
//--------------------------------------------------------------------------
//  Dest. System : MS-DOS
//--------------------------------------------------------------------------
//  Libraries    :
//--------------------------------------------------------------------------
//  Remarks: Ioan Ghip, Huffman Coding implementation
//--------------------------------------------------------------------------
//  1993,1994 by FUTURA Software s.r.l.
//--------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

#define MAGIC_NUMBER 0x1234  // For recognizing the archive
#define BYTE_SIZE 8

typedef unsigned char byte;
typedef unsigned short int word;

typedef struct TreeNode {
    struct TreeNode *left;
    struct TreeNode *right;
    byte leftVal;
    byte rightVal;
} TreeNode;

TreeNode *treeNodes[256];
long frequencies[256];
byte *encodings[256];
word magic = MAGIC_NUMBER;
long fileLength = 0L;
void *root;

void writeBit(byte bit, FILE *output) {
    static byte bits = 0;
    static byte counter = 0;
    bits = (bits << 1) | (bit ? 1 : 0);
    if (++counter == BYTE_SIZE) {
        putc(bits, output);
        bits = counter = 0;
    }
}

void writeByte(byte bits, FILE *output) {
    byte mask = 0x80;
    while (mask) {
        writeBit(bits & mask, output);
        mask >>= 1;
    }
}

int readBit(FILE *input) {
    static word bits = 0;
    static byte counter = 0;
    if (counter == 0) {
        bits = getc(input);
        counter = BYTE_SIZE;
    }
    counter--;
    bits <<= 1;
    return (bits & 0x100) ? 1 : 0;
}

byte readByte(FILE *input) {
    byte result = 0;
    int counter = BYTE_SIZE;
    while (counter--) {
        result <<= 1;
        result |= readBit(input);
    }
    return result;
}

void storePath(int character, byte position, byte *path) {
    encodings[character] = (byte *) malloc(position + 1);
    if (!encodings[character]) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    encodings[character][0] = position;
    for (int i = 0; i < position; i++) {
        encodings[character][i + 1] = path[i];
    }
}

void decodeTree(TreeNode *address) {
    static byte path[256];
    static byte position = 0;
    path[position++] = 0;
    if (address->left) {
        decodeTree(address->left);
    } else {
        storePath(address->leftVal, position, path);
    }
    path[position - 1] = 1;
    if (address->right) {
        decodeTree(address->right);
    } else {
        storePath(address->rightVal, position, path);
    }
    position--;
}

int buildTree(FILE *input) {
    TreeNode *tempNode;
    unsigned long minFreq1, minFreq2;
    short minIdx1, minIdx2;
    int i, c;

    // Calculate character frequencies in the file
    while ((c = getc(input)) != EOF) {
        frequencies[c]++;
        fileLength++;
    }

    // Build the encoding tree
    while (true) {
        minFreq1 = minFreq2 = -1L;
        minIdx1 = minIdx2 = -1;

        // Find the two smallest elements in the frequency table
        for (i = 0; i < 256; i++) {
            if (frequencies[i] != 0L) {
                if (frequencies[i] < minFreq1) {
                    minFreq2 = minFreq1;
                    minIdx2 = minIdx1;
                    minFreq1 = frequencies[i];
                    minIdx1 = i;
                } else if (frequencies[i] < minFreq2) {
                    minFreq2 = frequencies[i];
                    minIdx2 = i;
                }
            }
        }

        // Found the root?
        if (minFreq2 == -1) {
            break;
        }

        // Create a tree node
        tempNode = (TreeNode *) malloc(sizeof(TreeNode));
        if (!tempNode) {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        tempNode->left = treeNodes[minIdx1];
        tempNode->right = treeNodes[minIdx2];
        tempNode->leftVal = minIdx1;
        tempNode->rightVal = minIdx2;
        treeNodes[minIdx1] = tempNode;

        // Update the frequency table
        frequencies[minIdx1] = minFreq1 + minFreq2;
        frequencies[minIdx2] = 0L;
    }
    return minIdx1;
}

void writeToFile(FILE *input, FILE *output) {
    int c, length;
    byte *ptr;
    fseek(input, 0L, SEEK_SET);
    while ((c = getc(input)) != EOF) {
        ptr = encodings[c];
        length = (ptr) ? (int) (*ptr++) : 0;
        while (length--) {
            writeBit(*ptr++, output);
        }
    }
    writeByte(0, output);
}

void writeTree(FILE *output) {
    byte *ptr;
    int i, length;
    fwrite(&magic, sizeof(magic), 1, output);
    fwrite(&fileLength, sizeof(fileLength), 1, output);
    for (i = 0; i < 256; i++) {
        ptr = encodings[i];
        length = (ptr) ? (int) (*ptr++) : 0;
        if (length) {
            writeBit(1, output);
            writeByte(length, output);
            while (length--) {
                writeBit(*ptr++, output);
            }
        } else {
            writeBit(0, output);
        }
    }
}

void readPath(int character, FILE *input) {
    TreeNode *ptr;
    int i, length;
    int position;
    length = readByte(input);
    ptr = (TreeNode *) root;
    for (i = 0; i < length; i++) {
        position = readBit(input);
        if (position && !ptr->right) {
            ptr->right = (TreeNode *) calloc(1, sizeof(TreeNode));
            if (!ptr->right) {
                perror("Memory allocation failed");
                exit(EXIT_FAILURE);
            }
        }
        if (!position && !ptr->left) {
            ptr->left = (TreeNode *) calloc(1, sizeof(TreeNode));
            if (!ptr->left) {
                perror("Memory allocation failed");
                exit(EXIT_FAILURE);
            }
        }
        ptr = (position) ? ptr->right : ptr->left;
    }
    ptr->leftVal = character;
    ptr->rightVal = 0xff;
}

int decodeByte(FILE *input) {
    TreeNode *ptr;
    ptr = (TreeNode *) root;
    do {
        ptr = readBit(input) ? ptr->right : ptr->left;
    } while (!ptr->rightVal);
    return ptr->leftVal;
}

void decodeFile(FILE *input, FILE *output) {
    while (fileLength--) {
        putc(decodeByte(input), output);
    }
}

void readEncoding(FILE *input) {
    int i;
    fread(&magic, sizeof(magic), 1, input);
    if (magic != MAGIC_NUMBER) {
        printf("The input file is not compressed!\n");
        exit(EXIT_FAILURE);
    }
    fread(&fileLength, sizeof(fileLength), 1, input);
    root = calloc(1, sizeof(TreeNode));
    if (!root) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    for (i = 0; i < 256; i++) {
        if (readBit(input)) {
            readPath(i, input);
        }
    }
}

int main(int argc, char **argv) {
    FILE *input, *output;
    if (argc < 4 || argv[1][0] == '?') {
        printf("Compression program AARC, created by [...]");
        printf("Location: [...]");
        printf("Use only with permission from the above-mentioned company.\n");
        printf("                   (c) 1994\n");
        printf("Usage: aarc c / e file_to_archive archive_name\n");
        printf("   c - for compression\n");
        printf("   e - for decompression\n");
        return 1;
    }
    if (toupper(argv[1][0]) != 'C' && toupper(argv[1][0]) != 'E') {
        printf("Incorrect command: %s\n", argv[1]);
        return 1;
    }
    if ((input = fopen(argv[2], "rb")) == NULL) {
        perror("Cannot open input file");
        return 1;
    }
    if ((output = fopen(argv[3], "wb")) == NULL) {
        perror("Cannot open output file");
        return 1;
    }
    if (toupper(argv[1][0]) == 'C') {
        decodeTree(treeNodes[buildTree(input)]);
        writeTree(output);
        writeToFile(input, output);
    } else {
        readEncoding(input);
        decodeFile(input, output);
    }
    fclose(input);
    fclose(output);
    return 0;
}
//--------------------------------------------------------------------------
//  End of File  : instal.cpp
//--------------------------------------------------------------------------
