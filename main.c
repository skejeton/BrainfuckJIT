// clang-format off

// Uses: fopen, fseek, ftell, fread, fclose, fprintf
//       memset, memcpy
//       printf, getchar
#include <stdint.h>
#include <stdio.h>

// Uses: VirtualAlloc, GetTickCount
#include <windows.h>


char *Code;
int CodePtr;

int Stk[256];
int StkPtr = 0;

char *Buf;
int BufPtr = 0;

char *Data;

char *Input;
int InputSz;


#define OP(C) Code[CodePtr++] = C
#define DW(C) *(DWORD*)(Code + CodePtr) = C; CodePtr += 4
#define AT(I) (DWORD)(Code + CodePtr + I)


// To minimize I/O, we provide a custom putchar function, that only flushes 
// the buffer when a newline appears, (also at the end).
void flushBuf() {
    fwrite(Buf, 1, BufPtr, stdout);
    BufPtr = 0;
}

void putCharacter(char chr) {
    Buf[BufPtr++] = chr;
    Buf[BufPtr] = 0;
    if (chr == '\n') flushBuf();
}

struct {
    int v;
    int i;
}
typedef encounter;

encounter encounters[30000];

void trace(int i) {
    encounters[i].i = i;
    encounters[i].v++;
}

void emitPrelude(int Addr) {
    // MOV ebx, Addr
    OP(0xBB); DW(Addr);
}

void emitRight(char Count) {
    // ADD ebx, Count
    OP(0x83); OP(0xC3); OP(Count);
}

void emitLeft(char Count) {
    // SUB ebx, Count
    OP(0x83); OP(0xEB); OP(Count);
}

void emitPlus(char Count) {
    // ADD [ebx], Count
    OP(0x80); OP(0x03); OP(Count);
}

void emitMinus(char Count) {
    // SUB [ebx], Count
    OP(0x80); OP(0x2B); OP(Count);
}

void emitGetchar() {
    // PUSH ebx
    OP(0x53);
    // CALL getchar
    OP(0xE8); DW((int)getchar - AT(4));
    // POP ebx
    OP(0x5B);
    // MOV byte [ebx], al
    OP(0x88); OP(0x03);
}

void emitPutchar() {
    // PUSH ebx
    OP(0x53);
    // PUSH [ebx]
    OP(0xFF); OP(0x33);
    // CALL putCharacter 
    OP(0xE8); DW((int)putCharacter - AT(4));
    // POP ebx -- This one to clean up the stack
    OP(0x5B);
    // POP ebx
    OP(0x5B);
}

void emitTrace(int i) {
    /*
    // PUSH ebx
    OP(0x53);
    // PUSH i
    OP(0x68); DW(i);
    // CALL putCharacter 
    OP(0xE8); DW((int)trace - AT(4));
    // POP ebx -- This one to clean up the stack
    OP(0x5B);
    // POP ebx
    OP(0x5B);
    */
}

void emitLoop() {
    // CMP [ebx], 0
    OP(0x80); OP(0x3B); OP(0x00);
    // JE [end]
    OP(0x0F); OP(0x84); Stk[StkPtr++] = AT(0); DW(0);
}

void emitEndLoop() {
    StkPtr--;
    // JMP [start]
    OP(0xE9); DW(Stk[StkPtr] - (3+2) - AT(4));
    // Write the jump offset
    *(int*)(Stk[StkPtr]) = AT(0) - (Stk[StkPtr] + 4);
}

void emitReturn() {
    // RET
    OP(0xC3);
}

void inputNext(int *i, char *Input) {
    while (1) {
        (*i)++;
        switch (Input[*i]) {
            case '>':
            case '<':
            case '+':
            case '-':
            case '.':
            case ',':
            case '[':
            case ']':
            case '\0':
                return;
        }
    }
}

int n = 0;

int detectZero(int *i, char *Input) {
    int width = 0;

    int j = *i;
    while (1) {
        if (Input[j] != '[') return 0;
        inputNext(&j, Input);
        if (Input[j] != '-') return 0;
        inputNext(&j, Input);
        if (Input[j] != ']') return 0;
        inputNext(&j, Input);
        width++;
        int pj = j;
        if (Input[j] != '>') break;
        inputNext(&j, Input);
        if (Input[j] != '[') {
            j = pj;
            break;
        }
    }

    if (width == 1) {
        // MOV BYTE [EBX], 0
        OP(0xC6); OP(0x43); OP(0); OP(0x00);
    } else {
        int towrite = width;
        int dwords = towrite/4;
        int idx = 0;
        for (int w = 0; w < dwords; w++) {
            // MOV DWORD [EBX], 0
            OP(0xC7); OP(0x43); OP(idx); DW(0x00);
            idx += 4;
            towrite -= 4;
        }

        int words = towrite/2;
        for (int w = 0; w < words; w++) {
            // MOV WORD [EBX], 0
            OP(0x66); OP(0xC7); OP(0x43); OP(idx); OP(0x00); OP(0x00);
            idx += 2;
            towrite -= 2;
        }

        for (int w = 0; w < towrite; w++) {
            // MOV BYTE [EBX], 0
            OP(0xC6); OP(0x43); OP(idx); OP(0x00);
            idx++;
        }

        emitRight(width-1);
    }

    *i = j;

    return 1;
}

int detectMove(int *i, char *Input) {
    int out = 0;
    int back = 0;
    int sub = 0;

    int j = *i;
    if (Input[j] != '[') return 0;
    inputNext(&j, Input);
    if (Input[j] == '-') {inputNext(&j, Input);}
    if (Input[j] != '>' && Input[j] != '<') return 0;
    if (Input[j] == '>')
        while (Input[j] == '>') { out++; inputNext(&j, Input); }
    else if (Input[j] == '<')
        while (Input[j] == '<') { out--; inputNext(&j, Input); }
    if (Input[j] == '+') ;
    else if (Input[j] == '-') sub = 1;
    else return 0;
    inputNext(&j, Input);
    if (Input[j] != '<' && out > 0) return 0;
    if (Input[j] != '>' && out < 0) return 0;
    if (out > 0)
        while (Input[j] == '<') { back++; inputNext(&j, Input); }
    else if (out < 0)
        while (Input[j] == '>') { back--; inputNext(&j, Input); }
    if (Input[j] != ']') return 0;
    inputNext(&j, Input);

    if (out != back) {
        return 0;
    }

    // MOVE AL, BYTE [EBX]
    OP(0x8A); OP(0x03);
    if (sub) {
        // SUB [EBX+out], AL
        OP(0x28); OP(0x43); OP((uint8_t)out);
    } else {
        // ADD [EBX+out], AL
        OP(0x00); OP(0x43); OP((uint8_t)out);
    }
    // MOV [EBX], 0
    OP(0xC6); OP(0x03); OP(0x00);

    *i = j;
    return 1;
}

int getOpWidth(int *i, char *Input) {
    if (Input[*i] == '+') {
        int Cnt = 0;
        while (Input[*i] == '+') {inputNext(i, Input); Cnt++;}
        return Cnt;
    } else if (Input[*i] == '-') {
        int Cnt = 0;
        while (Input[*i] == '-') {inputNext(i, Input); Cnt++;}
        return -Cnt;
    } else {
        return 0;
    }
}

// <<<->>> or >>+<< and such
int detectQuickOp(int *i, char *Input) {
    int out = 0;
    int back = 0;

    int j = *i;
    if (Input[j] != '>' && Input[j] != '<') return 0;
    if (Input[j] == '>')
        while (Input[j] == '>') { out++; inputNext(&j, Input); }
    else if (Input[j] == '<')
        while (Input[j] == '<') { out--; inputNext(&j, Input); }

    int width = getOpWidth(&j, Input);
    if (width == 0) return 0;
    if (Input[j] != '<' && out > 0) return 0;
    if (Input[j] != '>' && out < 0) return 0;
    if (out > 0)
        while (Input[j] == '<') { back++; inputNext(&j, Input); }
    else if (out < 0)
        while (Input[j] == '>') { back--; inputNext(&j, Input); }

    if (out != back) {
        emitRight(out);
        if (width > 0)
            emitPlus(width);
        else
            emitMinus(-width);
        emitLeft(back);
        *i = j;
        return 1;
    }

    if (width > 0) {
        // ADD [EBX+out], width
        OP(0x80); OP(0x43); OP((uint8_t)out); OP((uint8_t)width);
    } else {
        // SUB [EBX+out], width
        OP(0x80); OP(0x6B); OP((uint8_t)out); OP((uint8_t)-width);
    }

    // printf("Span: %d - %d (%d)\n", *i, j, width);
    *i = j;
    return 1;
}

int detectFind0(int *i, char *Input) {
    int move = 0;
    int j = *i;

    if (Input[j] != '[') return 0;
    inputNext(&j, Input);
    if (Input[j] == '>')
        while (Input[j] == '>') { move++; inputNext(&j, Input); }
    else if (Input[j] == '<')
        while (Input[j] == '<') { move--; inputNext(&j, Input); }
    else 
        return 0;
    if (Input[j] != ']') return 0;
    inputNext(&j, Input);

    if (move > 0) {
        *i = j;
        emitLeft(move);
        // start:
        // CMP BYTE [EBX+move], 0
        OP(0x80); OP(0x7B); OP((int8_t)move); OP(0);
        // LEA EBX, [EBX+move]
        OP(0x8D); OP(0x5B); OP((int8_t)move);
        // JNE [start]
        OP(0x75); OP(0xF7);
        return 1;
    } else if (move < 0) {
        move *= -1;
        *i = j;
        emitRight(move);
        // start:
        // CMP BYTE [EBX-move], 0
        OP(0x80); OP(0x7B); OP(-(int8_t)move); OP(0);
        // LEA EBX, [EBX-move]
        OP(0x8D); OP(0x5B); OP(-(int8_t)move);
        // JNE [start]
        OP(0x75); OP(0xF7);
        return 1;
    }

    return 0;
}

int cmpEnc(const void *a, const void *b) {
    return *(int*)b - *(int*)a;
}

int main(int argc, const char *argv[]) {
    Data  = VirtualAlloc(0, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Code  = VirtualAlloc(0, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    Input = VirtualAlloc(0, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Buf   = VirtualAlloc(0, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    memset(Data, 0, 0x100000);

    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Usage: %s <file> [dump]\n", argv[0]);
        return 0;
    }

    FILE *File = fopen(argv[1], "rb");
    if (!File) {
        fprintf(stderr, "Error: could not open file\n");
        return 0;
    }

    fseek(File, 0, SEEK_END);
    InputSz = ftell(File);
    fseek(File, 0, SEEK_SET);
    fread(Input, 1, InputSz, File);
    fclose(File);

    char LastInstr = Input[0];
    char Count = 0;

    emitPrelude((int)Data+0x50000);
    for (int i = 0; i < InputSz; inputNext(&i, Input)) {
        if (LastInstr != Input[i]) {
            emitTrace(InputSz-Count);
            switch (LastInstr) {
            case '>' : emitRight(Count); break;
            case '<' : emitLeft(Count); break;
            case '+' : emitPlus(Count); break;
            case '-' : emitMinus(Count); break;
            }
            Count = 0;
        }
        emitTrace(i);
        while (detectFind0(&i, Input) || detectMove(&i, Input) || detectZero(&i, Input) || detectQuickOp(&i, Input))
            emitTrace(i);

        LastInstr = Input[i];
        switch (Input[i]) {
        case ',' : emitTrace(i); emitGetchar(); break;
        case '.' : emitTrace(i); emitPutchar(); break;
        case '[' : emitTrace(i); emitLoop(); break;
        case ']' : emitTrace(i); emitEndLoop(); break;
        case '>' : Count++; break;
        case '<' : Count++; break;
        case '+' : Count++; break;
        case '-' : Count++; break;
        }
    }

    emitTrace(InputSz-Count);
    switch (LastInstr) {
        case '>' : emitRight(Count); break;
        case '<' : emitLeft(Count); break;
        case '+' : emitPlus(Count); break;
        case '-' : emitMinus(Count); break;
    }

    if (StkPtr != 0) {
        fprintf(stderr, "Error: unmatched brackets\n");
        return 0;
    }

    emitReturn();

    printf("Emit OK\n");

    if (argc == 3) {
        FILE *OutFile = fopen(argv[2], "wb");
        if (!OutFile) {
            fprintf(stderr, "Error: could not open output file\n");
            return 0;
        }
        fwrite(Code, 1, CodePtr, OutFile);
        fclose(OutFile);
    }
    
    void (*f)() = (void(*)())Code;
    unsigned int start = GetTickCount();
    f();
    unsigned int end = GetTickCount();


    // qsort(encounters, 30000, sizeof(encounter), cmpEnc);
    // for (int i = 0; i < 10; i++) {
    //     printf("%d: %d [%d]\n", i, encounters[i].v, encounters[i].i);
    // }

    flushBuf();

    printf("\nOK (%dms)\n", end-start);
}