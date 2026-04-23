#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assemble.h"

/**
 * 顯示版本信息
 */
void printVersion() {
    printf("SIC/XE Assembler v1.0.0\n");
    printf("\nThis assembler supports both SIC and XE modes. Use -h or --help for more information.\n");
    printf("\n\033[1;31mSegmentation fault (core dumped)\033[0m\n");
    printf("🎵 Never gonna give you up\n");
    printf("🎵 Never gonna let you down\n");
    printf("🎵 Never gonna run around and desert you...\n");

#ifdef _WIN32  // Windows
    system("start https://www.youtube.com/watch?v=dQw4w9WgXcQ");
#elif __APPLE__  // macOS
    system("open https://www.youtube.com/watch?v=dQw4w9WgXcQ");
#elif __linux__  // Linux
    system("xdg-open https://www.youtube.com/watch?v=dQw4w9WgXcQ");
#else
    printf("Please visit: https://www.youtube.com/watch?v=dQw4w9WgXcQ\n");
#endif
}

/**
 * 顯示使用說明
 */
void printHelp(char* progName) {
    printf("Usage: %s [options] <inputfile> [outputfile]\n", progName);
    printf("\nOptions:\n");
    printf("  -h, --help       Show this help message and exit\n");
    printf("  -v, --version    Show version information and exit\n");
    printf("  -x, --xe         Assemble in XE mode (default)\n");
    printf("  -s, --sic        Assemble in SIC mode\n");
    printf("\nIf outputfile is not specified, the default output file will be 'output.obj'.\n");
}

int main(int argc, char* argv[]) {
    int isXEMode = 1;       // 預設使用 XE 模式
    char* filename = NULL;  // 要讀取的文件名
    char* output = NULL;    // 輸出的文件名

    // 解析命令行參數
    for (int i = 1; i < argc; i++) {
        // 簡單選項解析
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
                printVersion();
                return 0;
            } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                printHelp(argv[0]);
                return 0;
            } else if (strcmp(argv[i], "--xe") == 0 || strcmp(argv[i], "-x") == 0) {
                isXEMode = 1;
            } else if (strcmp(argv[i], "--sic") == 0 || strcmp(argv[i], "-s") == 0) {
                isXEMode = 0;
            } else {
                printf("Unknown option: %s\n", argv[i]);
                return 1;
            }
        } else {  // 非選項參數，應該是文件名
            if (filename == NULL) {
                filename = argv[i];
            } else if (output == NULL) {
                output = argv[i];
            } else {
                printf("Too many arguments. Only input and output filenames are expected.\n");
                printHelp(argv[0]);
                return 1;
            }
        }
    }

    if (filename == NULL) {
        printf("Error: No input file specified.\n");
        printHelp(argv[0]);
        return 1;
    }

    if (output == NULL) {
        output = "output.obj";  // 預設輸出文件名
    }

    assemble(filename, output, isXEMode);  // 執行組合過程

    return 0;
}
