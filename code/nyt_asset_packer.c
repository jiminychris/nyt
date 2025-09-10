#include <stdio.h>
#include <stdlib.h>

int main(int ArgCount, char *Args[])
{
    FILE *ExecutableFile = fopen(Args[1], "rb");
    FILE *DataFile = fopen(Args[2], "rb");

    fseek(ExecutableFile, 0, SEEK_END);
    size_t ExecutableFileSize = ftell(ExecutableFile);
    fseek(ExecutableFile, 0, SEEK_SET);
    
    fseek(DataFile, 0, SEEK_END);
    size_t DataFileSize = ftell(DataFile);
    fseek(DataFile, 0, SEEK_SET);

    size_t FileSize = ExecutableFileSize + DataFileSize;
    char *FileContents = malloc(FileSize);
    char *Dest = FileContents;
    Dest += fread(Dest, 1, ExecutableFileSize, ExecutableFile);
    fread(Dest, 1, DataFileSize, DataFile);

    fclose(ExecutableFile);
    ExecutableFile = fopen(Args[1], "wb");
    fwrite(FileContents, 1, FileSize, ExecutableFile);
    fwrite(&ExecutableFileSize, sizeof(ExecutableFileSize), 1, ExecutableFile);
    
    return 0;
}
