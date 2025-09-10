#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

#define PUZZLE_HEIGHT 8
#define PUZZLE_WIDTH 6
#define PUZZLE_SIZE (PUZZLE_HEIGHT * PUZZLE_WIDTH)
#define STRIDE (PUZZLE_WIDTH + 2)
#define MIN_LENGTH 4
#define MAX_WORDS ((PUZZLE_HEIGHT * PUZZLE_WIDTH) / MIN_LENGTH)
#define SPELLING_BEE_LETTER_BANK_SIZE 7

#define PushStruct(Arena, Type) (struct Type *)ArenaPush(Arena, sizeof(struct Type))

char Offsets[] = {
    -STRIDE - 1,
    -STRIDE,
    -STRIDE + 1,
    -1,
    +1,
    STRIDE - 1,
    STRIDE,
    STRIDE + 1,
};

struct memory_arena
{
    size_t Size;
    size_t Allocated;
    char *Memory;
};

struct solution
{
    char Length;
    char *Word;
    uint64_t PuzzleMask;
};

struct combination
{
    char Count;
    int Indices[MAX_WORDS];
};

struct combine_memo
{
    int StartIndex;
    uint64_t PuzzleMask;
    
};

struct solution_builder
{
    size_t SolutionCount;
    size_t CombinationCount;
    struct memory_arena StringArena;
    struct memory_arena SolutionArena;
    struct memory_arena CombinationArena;
};

static inline void PrintArenaStats(struct memory_arena Arena)
{
    printf("%zu / %zu (%f)\n", Arena.Allocated, Arena.Size, (float)Arena.Allocated / (float)Arena.Size);
}

void *ArenaPush(struct memory_arena *Arena, size_t Size)
{
    void *Result = 0;
    size_t NewAllocated = Arena->Allocated + Size;
    if (Arena->Size < NewAllocated)
    {
        printf("Pushed too much on arena! (%zu / %zu)\n", NewAllocated, Arena->Size);
    }
    else
    {
        Result = Arena->Memory + Arena->Allocated;
        Arena->Allocated = NewAllocated;
    }
    return Result;
}

struct trie_node
{
    char Value;
    struct trie_node *FirstChild;
    struct trie_node *Sibling;
};

static inline char TrieMatch(struct trie_node *Node, char Char)
{
    return (Node->Value & 0x7F) == Char;
}

static inline char TrieTerminal(struct trie_node *Node)
{
    return Node->Value & 0x80;
}

struct trie_builder
{
    char *Stream;
    struct trie_node Root;
    struct memory_arena Arena;
};

struct spelling_bee_solution_builder
{
    size_t SolutionCount;
    struct memory_arena StringArena;
    struct memory_arena SolutionArena;
};

struct trie_node *TrieFindChild(struct trie_node *Node, char Char)
{
    struct trie_node *Child = Node->FirstChild;
    while (Child && !TrieMatch(Child, Char))
    {
        Child = Child->Sibling;
    }
    return Child;
}

void Solve(char *PuzzleWithApron, int PuzzleIndex, char *Visited, char *Buffer, int Length, struct trie_node *Node, struct solution_builder *Builder)
{
    char Char = PuzzleWithApron[PuzzleIndex];
    if (!Visited[PuzzleIndex])
    {
        Visited[PuzzleIndex] = 1;
        struct trie_node *Child = TrieFindChild(Node, Char);
        if (Child)
        {
            Buffer[Length++] = Char;
            if (Length >= MIN_LENGTH && TrieTerminal(Child))
            {
                struct solution *Solution = PushStruct(&Builder->SolutionArena, solution);
                Solution->PuzzleMask = 0;
                int MaskIndex = 0;
                char *Row = Visited + PUZZLE_WIDTH + 3;
                for (int RowIndex = 0; RowIndex < PUZZLE_HEIGHT; ++RowIndex)
                {
                    char *At = Row;
                    for (int ColIndex = 0; ColIndex < PUZZLE_WIDTH; ++ColIndex)
                    {
                        Solution->PuzzleMask = Solution->PuzzleMask | (*At++ << MaskIndex++);
                    }
                    Row += STRIDE;
                }
                Solution->Length = Length;
                Solution->Word = ArenaPush(&Builder->StringArena, Length);
                memcpy(Solution->Word, Buffer, Length);
                Builder->SolutionCount++;
            }
            for (int OffsetIndex = 0; OffsetIndex < sizeof(Offsets); ++OffsetIndex)
            {
                Solve(PuzzleWithApron, PuzzleIndex + Offsets[OffsetIndex], Visited, Buffer, Length, Child, Builder);
            }
        }
        Visited[PuzzleIndex] = 0;
    }
}

void BuildTrie(struct trie_builder *Builder)
{
    char Char = *Builder->Stream++;
    struct trie_node *Node = &Builder->Root;
    char OrMask = 0x80;
    while (Char)
    {
        if ('\n' == Char)
        {
            Node->Value = OrMask | Node->Value;
#if 0
            printf(" (%s)\n", TrieTerminal(Node) ? "Valid" : "Invalid");
#endif
            Node = &Builder->Root;
            OrMask = 0x80;
        }
        else
        {
            if ('a' <= Char && Char <= 'z')
            {
                Char = 'A' + (Char - 'a');
            }
            if ('A' <= Char && Char <= 'Z')
            {
#if 0
                printf("%c", Char);
#endif
                struct trie_node *Child = TrieFindChild(Node, Char);
                if (!Child)
                {
                    Child = PushStruct(&Builder->Arena, trie_node);
                    Child->Value = Char;
                    Child->FirstChild = 0;
                    Child->Sibling = Node->FirstChild;
                    Node->FirstChild = Child;
                }
                Node = Child;
            }
            else
            {
                OrMask = 0;
            }
        }
        Char = *Builder->Stream++;
    }
}

int CountSetBits(uint64_t Integer)
{
    unsigned int Result = 0;
    while (Integer > 0)
    {
        Integer &= (Integer - 1);
        Result++;
    }
    return Result;
}

void Combine(struct solution_builder *Builder, struct combination *Combination, int StartIndex, uint64_t Mask)
{
    struct solution *Solutions = (struct solution *)Builder->SolutionArena.Memory;
    for (int NextIndex = StartIndex; NextIndex < Builder->SolutionCount; ++NextIndex)
    {
        struct solution Solution = Solutions[NextIndex];
//        printf("%.*s %d %d %llu\n", Solution.Length, Solution.Word, StartIndex, NextIndex, Mask);
        if (!(Mask & Solution.PuzzleMask))
        {
            Combination->Indices[Combination->Count++] = NextIndex;
            Combine(Builder, Combination, NextIndex + 1, Mask | Solution.PuzzleMask);
            --Combination->Count;
        }
    }
    struct combination *Output = PushStruct(&Builder->CombinationArena, combination);
    *Output = *Combination;
    Builder->CombinationCount++;
}

enum nyt_game
{
    nyt_game_Strands,
    nyt_game_SpellingBee,
};

struct spelling_bee_solution
{
    char Length;
    char *Word;
    char Mask;
};

#define PANGRAM_BITS 0x7F

void SolveSpellingBee(struct spelling_bee_solution_builder *SolutionBuilder, char *LetterBank, struct trie_node *TrieNode, char Mask, char *Buffer, int Length, int CoreMask)
{
    char *At = LetterBank;
    if (TrieTerminal(TrieNode) && (Mask & CoreMask) == CoreMask && 4 <= Length)
    {
        struct spelling_bee_solution *Solution = PushStruct(&SolutionBuilder->SolutionArena, spelling_bee_solution);
        Solution->Mask = Mask;
        Solution->Length = Length;
        Solution->Word = ArenaPush(&SolutionBuilder->StringArena, Length);
        memcpy(Solution->Word, Buffer, Length);
        SolutionBuilder->SolutionCount++;
    }
    char Char;
    int Flag = 1;
    while ((Char = *At++))
    {
        struct trie_node *Child = TrieFindChild(TrieNode, Char);
        if (Child)
        {
            Buffer[Length] = Char;
            SolveSpellingBee(SolutionBuilder, LetterBank, Child, Mask | Flag, Buffer, Length + 1, CoreMask);
        }
        Flag = Flag << 1;
    }
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
    char *GameName = Args[1];
    enum nyt_game Game;
    if (0 == strcmp(GameName, "strands"))
    {
        Game = nyt_game_Strands;
    }
    else if (0 == strcmp(GameName, "bee"))
    {
        Game = nyt_game_SpellingBee;
    }
    else
    {
        printf("usage: %s strands|bee <param>\n", ExecutableName);
        return 1;
    }

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

    struct memory_arena Arena = {0};
    Arena.Size = 1024*1024*1024;
    Arena.Memory = malloc(Arena.Size);

    struct trie_builder Builder = {0};
    Builder.Stream = LexiconMemory;
    Builder.Arena.Size = 32 * 1024 * 1024;
    Builder.Arena.Memory = ArenaPush(&Arena, Builder.Arena.Size);
    BuildTrie(&Builder);

    switch (Game)
    {
    case nyt_game_Strands:
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
    } break;
    case nyt_game_SpellingBee:
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
        char LetterHistogram[26] = {0};
        char DuplicateColors[26] = {0};
        char Colors[] = { 32, 33, 34, };
        int LetterBankSize = 0;
        char Char;

        char *LetterBankAt = LetterBank;
        char NonLetterCount = 0;
        while ((Char = *LetterBankAt))
        {
            if ('a' <= Char && Char <= 'z')
            {
                Char = 'A' + (Char - 'a');
            }
            if ('A' <= Char && Char <= 'Z')
            {
                *LetterBankAt = Char;
            }
            else
            {
                NonLetterCount++;
            }
            LetterBankAt++;
            LetterBankSize++;
        }

        if (NonLetterCount)
        {
            printf("Spelling Bee requires a letter bank with %d distinct letters (", SPELLING_BEE_LETTER_BANK_SIZE);
            LetterBankAt = LetterBank;
            while ((Char = *LetterBankAt++))
            {
                char IsLetter = 'A' <= Char && Char <= 'Z';
                printf("\033[%dm%c", IsLetter ? 0 : 31, Char);
            }
            puts("\033[0m)\n");
            return 1;
        }

        if (LetterBankSize != SPELLING_BEE_LETTER_BANK_SIZE)
        {
            printf("Spelling Bee requires a letter bank with %d distinct letters, not %d (%.7s\033[31m", SPELLING_BEE_LETTER_BANK_SIZE, LetterBankSize, LetterBank);
            if (LetterBankSize < SPELLING_BEE_LETTER_BANK_SIZE)
            {
                printf("%.*s", SPELLING_BEE_LETTER_BANK_SIZE - LetterBankSize, "???????");
            }
            else if (SPELLING_BEE_LETTER_BANK_SIZE < LetterBankSize)
            {
                printf("%s", LetterBank + SPELLING_BEE_LETTER_BANK_SIZE);
            }
            puts("\033[0m)\n");
            return 1;
        }

        LetterBankAt = LetterBank;
        char ColorIndex = 0;
        while ((Char = *LetterBankAt++))
        {
            int Index = Char - 'A';
            char Count = LetterHistogram[Index] + 1;
            LetterHistogram[Index] = Count;
            if (1 < Count)
            {
                DuplicateColors[Index] = Colors[ColorIndex++];
            }
        }

        if (ColorIndex)
        {
            printf("Spelling Bee requires a letter bank with %d distinct letters (", SPELLING_BEE_LETTER_BANK_SIZE);
            LetterBankAt = LetterBank;
            while ((Char = *LetterBankAt++))
            {
                printf("\033[%dm%c", DuplicateColors[Char - 'A'], Char);
            }
            puts("\033[0m)\n");
            return 1;
        }

        LetterBankAt = SortedLetterBank;
        char CoreChar = *LetterBank;
        int CoreIndex = 0;
        for (int HistogramIndex = 0; HistogramIndex < sizeof(LetterHistogram); ++HistogramIndex)
        {
            Char = 'A' + HistogramIndex;
            char Count = LetterHistogram[HistogramIndex];
            if (Count == 1)
            {
                if (Char == CoreChar)
                {
                    CoreIndex = LetterBankAt - SortedLetterBank;
                }
                *LetterBankAt++ = Char;
            }
        }
        int CoreMask = 1 << CoreIndex;
        *LetterBankAt = 0;
        char Buffer[256];
        struct spelling_bee_solution_builder SolutionBuilder = {0};
        SolutionBuilder.StringArena.Size = 16*1024;
        SolutionBuilder.StringArena.Memory = ArenaPush(&Arena, SolutionBuilder.StringArena.Size);
        SolutionBuilder.SolutionArena.Size = 16*1024;
        SolutionBuilder.SolutionArena.Memory = ArenaPush(&Arena, SolutionBuilder.SolutionArena.Size);
        SolveSpellingBee(&SolutionBuilder, SortedLetterBank, &Builder.Root, 0, Buffer, 0, CoreMask);
        struct spelling_bee_solution *Solution = (struct spelling_bee_solution *)SolutionBuilder.SolutionArena.Memory;
        if (ShowPangrams)
        {
            for (int SolutionIndex = 0; SolutionIndex < SolutionBuilder.SolutionCount; ++SolutionIndex)
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
            for (int SolutionIndex = 0; SolutionIndex < SolutionBuilder.SolutionCount; ++SolutionIndex)
            {
                printf("%.*s\n", Solution->Length, Solution->Word);
                Solution++;
            }
        }
    } break;
    }
    return 0;
}
