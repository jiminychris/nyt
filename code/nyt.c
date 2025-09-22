#include <stddef.h>
#include <stdint.h>

#define PUZZLE_HEIGHT 8
#define PUZZLE_WIDTH 6
#define PUZZLE_SIZE (PUZZLE_HEIGHT * PUZZLE_WIDTH)
#define STRIDE (PUZZLE_WIDTH + 2)
#define MIN_LENGTH 4
#define MAX_WORDS ((PUZZLE_HEIGHT * PUZZLE_WIDTH) / MIN_LENGTH)
#define SPELLING_BEE_LETTER_BANK_SIZE 7

#define PushType(Arena, Type) ((Type *)ArenaPush(Arena, sizeof(Type)))
#define PushStruct(Arena, Type) ((struct Type *)ArenaPush(Arena, sizeof(struct Type)))

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

void *ArenaPush(struct memory_arena *Arena, size_t Size)
{
    void *Result = 0;
    size_t NewAllocated = Arena->Allocated + Size;
    if (NewAllocated <= Arena->Size)
    {
        Result = Arena->Memory + Arena->Allocated;
        Arena->Allocated = NewAllocated;
    }
#if 0
    else
    {
        printf("Pushed too much on arena! (%zu / %zu)\n", NewAllocated, Arena->Size);
    }
#endif
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
    size_t WordCount;
    char *Stream;
    struct trie_node Root;
    struct memory_arena Arena;
};

#pragma pack(push, 1)
struct spelling_bee_solution_builder
{
    size_t SolutionCount;
    struct memory_arena StringArena;
    struct memory_arena SolutionArena;
};

struct spelling_bee_solution
{
    char Length;
    char *Word;
    char Mask;
};
#pragma pack(pop)

struct trie_node *TrieFindChild(struct trie_node *Node, char Char)
{
    struct trie_node *Child = Node->FirstChild;
    while (Child && !TrieMatch(Child, Char))
    {
        Child = Child->Sibling;
    }
    return Child;
}

int Copy(void *Destination, void *Source, size_t Length)
{
    size_t Result = Length;
    char *D = (char*)Destination;
    char *S = (char*)Source;
    while (Length--)
    {
        *D++ = *S++;
    }
    return Result;
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
                Copy(Solution->Word, Buffer, Length);
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
            Node = &Builder->Root;
            OrMask = 0x80;
            Builder->WordCount++;
        }
        else
        {
            if ('a' <= Char && Char <= 'z')
            {
                Char = 'A' + (Char - 'a');
            }
            if ('A' <= Char && Char <= 'Z')
            {
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

#define PANGRAM_BITS 0x7F

void SolveSpellingBee_(struct spelling_bee_solution_builder *SolutionBuilder, char *LetterBank, struct trie_node *TrieNode, char Mask, char *Buffer, int Length, int CoreMask)
{
    char *At = LetterBank;
    if (TrieTerminal(TrieNode) && (Mask & CoreMask) == CoreMask && 4 <= Length)
    {
        struct spelling_bee_solution *Solution = PushStruct(&SolutionBuilder->SolutionArena, spelling_bee_solution);
        Solution->Mask = Mask;
        Solution->Length = Length;
        Solution->Word = ArenaPush(&SolutionBuilder->StringArena, Length);
        Copy(Solution->Word, Buffer, Length);
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
            SolveSpellingBee_(SolutionBuilder, LetterBank, Child, Mask | Flag, Buffer, Length + 1, CoreMask);
        }
        Flag = Flag << 1;
    }
}

static struct trie_builder BuildTrieFromLexicon(struct memory_arena *Arena, char *Lexicon)
{
    struct trie_builder Builder = {0};
    Builder.Stream = Lexicon;
    Builder.Arena.Size = 32 * 1024 * 1024;
    Builder.Arena.Memory = ArenaPush(Arena, Builder.Arena.Size);
    BuildTrie(&Builder);
    return Builder;
}

struct spelling_bee_solution_builder *SolveSpellingBee(char *Memory, size_t Size, char *Lexicon, char *SortedLetterBank, int CoreMask)
{
    struct memory_arena Arena = {0};
    Arena.Size = Size;
    Arena.Memory = Memory;

    char Buffer[256];
    struct spelling_bee_solution_builder *SolutionBuilder = PushStruct(&Arena, spelling_bee_solution_builder);
    struct trie_builder Builder = BuildTrieFromLexicon(&Arena, Lexicon);
    SolutionBuilder->SolutionCount = 0;
    SolutionBuilder->StringArena.Size = 16*1024;
    SolutionBuilder->StringArena.Memory = ArenaPush(&Arena, SolutionBuilder->StringArena.Size);
    SolutionBuilder->SolutionArena.Size = 16*1024;
    SolutionBuilder->SolutionArena.Memory = ArenaPush(&Arena, SolutionBuilder->SolutionArena.Size);

    SolveSpellingBee_(SolutionBuilder, SortedLetterBank, &Builder.Root, 0, Buffer, 0, CoreMask);
    return SolutionBuilder;
}
