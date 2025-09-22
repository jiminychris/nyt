#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nyt.c"

static inline void PrintArenaStats(struct memory_arena Arena)
{
    printf("%zu / %zu (%f)\n", Arena.Allocated, Arena.Size, (float)Arena.Allocated / (float)Arena.Size);
}

static char *ReadLexicon(char *ExecutablePath)
{
    FILE *ExecutableFile = fopen(ExecutablePath, "rb");
    if (!ExecutableFile)
    {
        printf("Failed to open executable %s\n", ExecutablePath);
    }
    size_t DataStart;
    fseek(ExecutableFile, -sizeof(size_t), SEEK_END);
    size_t DataEnd = ftell(ExecutableFile);
    fread(&DataStart, sizeof(DataStart), 1, ExecutableFile);
    fseek(ExecutableFile, DataStart, SEEK_SET);
    size_t LexiconMemorySize = DataEnd - DataStart;
    char *LexiconMemory = malloc(LexiconMemorySize + 1);
    fread(LexiconMemory, 1, LexiconMemorySize, ExecutableFile);
    LexiconMemory[LexiconMemorySize - 1] = 0;
    return LexiconMemory;
}

int main(int ArgCount, char *Args[])
{
    char *ExecutableName, *ExecutablePath;
    ExecutableName = ExecutablePath = Args[0];
    for (char *At = ExecutablePath; *At; At++)
    {
        if (*At == '/')
        {
            ExecutableName = ++At;
        }
    }
    if (ArgCount < 2)
    {
        printf("usage: %s strands|bee <param>\n", ExecutableName);
        return 1;
    }

    struct memory_arena Arena = {0};
    Arena.Size = 1024*1024*1024;
    Arena.Memory = malloc(Arena.Size);

    char *GameName = Args[1];
    if (0 == strcmp(GameName, "strands"))
    {
        if (ArgCount < 3)
        {
            printf("usage: %s %s <filename>\n", ExecutableName, GameName);
            return 1;
        }
        char *Filename = Args[2];
        char Puzzle[PUZZLE_SIZE];
        char FileContents[PUZZLE_SIZE + PUZZLE_HEIGHT - 1];
        FILE *PuzzleFile = fopen(Filename, "rb");
        if (!PuzzleFile)
        {
            printf("Failed to open file %s\n", Filename);
            return 1;
        }
        if (sizeof(FileContents) != fread(FileContents, 1, sizeof(FileContents), PuzzleFile))
        {
            printf("Expected file to contain eight lines of six characters each.\n");
            return 1;
        }

        struct trie_builder Builder = BuildTrieFromLexicon(&Arena, ReadLexicon(ExecutablePath));

        struct solution_builder SolutionBuilder = {0};
        SolutionBuilder.StringArena.Size = 16*1024;
        SolutionBuilder.StringArena.Memory = ArenaPush(&Arena, SolutionBuilder.StringArena.Size);
        SolutionBuilder.SolutionArena.Size = 64*1024;
        SolutionBuilder.SolutionArena.Memory = ArenaPush(&Arena, SolutionBuilder.SolutionArena.Size);
        SolutionBuilder.CombinationArena.Size = 64*1024*1024;
        SolutionBuilder.CombinationArena.Memory = ArenaPush(&Arena, SolutionBuilder.CombinationArena.Size);

#if 0
        printf("Allocated: %zu / %zu (%f)\n", Builder.Arena.Allocated, Builder.Arena.Size, (float)Builder.Arena.Allocated / (float)Builder.Arena.Size);
        struct trie_node *Node = Builder.Root.FirstChild;
        while (Node)
        {
            printf("%c", Node->Value & 0x7F);
            Node = Node->FirstChild;
        }
        printf("\n");
#endif
    
        char PuzzleWithApron[(PUZZLE_HEIGHT+2)*(PUZZLE_WIDTH+2)] = {0};
        char Visited[(PUZZLE_HEIGHT+2)*(PUZZLE_WIDTH+2)];
        char *Source = FileContents;
        char *Dest = PuzzleWithApron + PUZZLE_WIDTH + 3;
        for (int RowIndex = 0; RowIndex < PUZZLE_HEIGHT; ++RowIndex)
        {
            memcpy(Dest, Source, PUZZLE_WIDTH);
            Dest += STRIDE;
            Source += PUZZLE_WIDTH + 1;
            if (*Source == '\n')
            {
                Source++;
            }
        }

        int RowOffset = PUZZLE_WIDTH + 3;
        for (int RowIndex = 0; RowIndex < PUZZLE_HEIGHT; ++RowIndex)
        {
            for (int ColIndex = 0; ColIndex < PUZZLE_WIDTH; ++ColIndex)
            {
                int PuzzleIndex = RowOffset + ColIndex;
                char Buffer[256];
                memset(Visited, 0, sizeof(Visited));
                Solve(PuzzleWithApron, PuzzleIndex, Visited, Buffer, 0, &Builder.Root, &SolutionBuilder);
            }
            RowOffset += STRIDE;
        }

        struct solution *Solutions = (struct solution *)SolutionBuilder.SolutionArena.Memory;
#if 1
        printf("Solutions: %zu\n", SolutionBuilder.SolutionCount);
        for (int SolutionIndex = 0; SolutionIndex < SolutionBuilder.SolutionCount; ++SolutionIndex)
        {
            struct solution Solution = Solutions[SolutionIndex];
            printf("%.*s (%llu)\n", Solution.Length, Solution.Word, Solution.PuzzleMask);
        }
#endif
#if 1
        struct combination Combination = {0};
        Combine(&SolutionBuilder, &Combination, 0, 0);
        struct combination *Combinations = (struct combination *)SolutionBuilder.CombinationArena.Memory;
        printf("Combinations: %zu\n", SolutionBuilder.CombinationCount);
        int Most = 0;
        int Best = 0;
        for (int CombinationIndex = 0; CombinationIndex < SolutionBuilder.CombinationCount; ++CombinationIndex)
        {
            uint64_t Mask = 0;
            Combination = Combinations[CombinationIndex];
            for (int IndexIndex = 0; IndexIndex < Combination.Count; ++IndexIndex)
            {
                struct solution Solution = Solutions[Combination.Indices[IndexIndex]];
                printf("%.*s\n", Solution.Length, Solution.Word);
                Mask = Mask | Solution.PuzzleMask;
            }
            printf("====\n");
            int Test = CountSetBits(Mask);
            if (Most < Test)
            {
                Most = Test;
                Best = CombinationIndex;
            }
        }
        Combination = Combinations[Best];
        for (int IndexIndex = 0; IndexIndex < Combination.Count; ++IndexIndex)
        {
            struct solution Solution = Solutions[Combination.Indices[IndexIndex]];
            printf("%.*s\n", Solution.Length, Solution.Word);
        }
#endif
#if 1
        PrintArenaStats(Builder.Arena);
        PrintArenaStats(SolutionBuilder.SolutionArena);
        PrintArenaStats(SolutionBuilder.StringArena);
        PrintArenaStats(SolutionBuilder.CombinationArena);
#endif
    }
    else if (0 == strcmp(GameName, "bee"))
    {
        int ArgIndex = 2;
        char ShowPangrams = 0;
        char *LetterBank = 0;
        for (int ArgIndex = 2; ArgIndex < ArgCount; ++ArgIndex)
        {
            char *Arg = Args[ArgIndex];
            if ('-' == *Arg)
            {
                if (0 == strcmp("-p", Arg) || 0 == strcmp("--pangram", Arg) || 0 == strcmp("--pangrams", Arg))
                {
                    ShowPangrams = 1;
                }
                else
                {
                    printf("Unrecognized option %s\nValid options are: -p", Arg);
                    return 1;
                }
            }
            else
            {
                LetterBank = Arg;
            }
        }
        if (!LetterBank)
        {
            printf("usage: %s %s [-p] ABCDEFG\n", ExecutableName, GameName);
            return 1;
        }
        char SortedLetterBank[SPELLING_BEE_LETTER_BANK_SIZE + 1];
        char LetterHistogram[256] = {0};
        char DuplicateColors[256] = {0};
        char Colors[] = { 92, 93, 94, };
        int LetterBankSize = 0;
        char Char;

        char *Source = LetterBank;
        char *UpcasedLetterBank = ArenaPush(&Arena, 0);
        char NonLetterCount = 0;
        char ColorIndex = 0;
        while ((Char = *Source++))
        {
            if ('a' <= Char && Char <= 'z')
            {
                Char = 'A' + (Char - 'a');
            }
            if ('A' <= Char && Char <= 'Z')
            {
                if (LetterBankSize < SPELLING_BEE_LETTER_BANK_SIZE)
                {
                    char Count = LetterHistogram[Char] + 1;
                    LetterHistogram[Char] = Count;
                    if (Count == 2)
                    {
                        DuplicateColors[Char] = Colors[ColorIndex++];
                    }
                }
            }
            else
            {
                NonLetterCount++;
            }
            *PushType(&Arena, char) = Char;
            LetterBankSize++;
        }
        *PushType(&Arena, char) = 0;

        int MissingChars = SPELLING_BEE_LETTER_BANK_SIZE - LetterBankSize;
        if (NonLetterCount || MissingChars || ColorIndex)
        {
            printf("Spelling Bee requires a letter bank with %d distinct letters (", SPELLING_BEE_LETTER_BANK_SIZE);
            char *Upcase = UpcasedLetterBank;
            Source = LetterBank;
            char ValidLength = SPELLING_BEE_LETTER_BANK_SIZE < LetterBankSize ? SPELLING_BEE_LETTER_BANK_SIZE : LetterBankSize;
            while (ValidLength--)
            {
                Char = *Upcase++;
                int Color1, Color2;
                if ('A' <= Char && Char <= 'Z')
                {
                    Color1 = 0;
                    Color2 = DuplicateColors[Char];
                }
                else
                {
                    Color1 = 97;
                    Color2 = 105;
                }
                printf("\033[%d;%dm%c", Color1, Color2, *Source++);
            }
            if (MissingChars)
            {
                printf("\033[90;107m");
                while (0 < MissingChars)
                {
                    putchar('?');
                    MissingChars--;
                }
                while (MissingChars < 0)
                {
                    putchar(*Source++);
                    MissingChars++;
                }
            }
            puts("\033[0m)\n");
            return 1;
        }

        char *Dest = SortedLetterBank;
        char CoreChar = *UpcasedLetterBank;
        int CoreIndex = 0;
        int CoreMask = 0;
        for (int HistogramIndex = 0; HistogramIndex < sizeof(LetterHistogram); ++HistogramIndex)
        {
            if (LetterHistogram[HistogramIndex])
            {
                if (HistogramIndex == CoreChar)
                {
                    CoreMask = 1 << (Dest - SortedLetterBank);
                }
                *Dest++ = HistogramIndex;
            }
        }
        *Dest = 0;
        char *Lexicon = ReadLexicon(ExecutablePath);
        struct memory_arena SolveArena = {0};
        SolveArena.Size = Arena.Size - Arena.Allocated;
        SolveArena.Memory = ArenaPush(&Arena, SolveArena.Size);
        struct spelling_bee_solution_builder *SolutionBuilder = SolveSpellingBee(SolveArena.Memory, SolveArena.Size, Lexicon, SortedLetterBank, CoreMask);
        struct spelling_bee_solution *Solution = (struct spelling_bee_solution *)SolutionBuilder->SolutionArena.Memory;
        if (ShowPangrams)
        {
            for (int SolutionIndex = 0; SolutionIndex < SolutionBuilder->SolutionCount; ++SolutionIndex)
            {
                char IsPangram = (Solution->Mask & PANGRAM_BITS) == PANGRAM_BITS;
                char IsPerfect = IsPangram && Solution->Length == SPELLING_BEE_LETTER_BANK_SIZE;
                char Prefix = IsPerfect ? '!' : IsPangram ? '*' : ' ';
                printf("%c %.*s\n", Prefix, Solution->Length, Solution->Word);
                Solution++;
            }
        }
        else
        {
            for (int SolutionIndex = 0; SolutionIndex < SolutionBuilder->SolutionCount; ++SolutionIndex)
            {
                printf("%.*s\n", Solution->Length, Solution->Word);
                Solution++;
            }
        }
    }
    else
    {
        printf("usage: %s strands|bee <param>\n", ExecutableName);
        return 1;
    }

    return 0;
}
