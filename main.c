// clang-format off
#include <stdio.h>
#include <windows.h>
#include <timeapi.h>

char *Code;
int CodePtr;
int Stk[256];
int Sp = 0;
char *Buf;
int I = 0;

#define OP(c) Code[CodePtr++] = c
#define DW(c) *(DWORD*)(Code + CodePtr) = c; CodePtr += 4
#define AT(i) (DWORD)(Code + CodePtr + i)

int getCharacter() {
    return getchar();
}

void putCharacter(char chr) {
    Buf[I++] = chr;
    Buf[I] = 0;
    // NOTE: To minimize I/O, we only flush the buffer when a newline appears, (also at the end).
    if (chr == '\n') {
        fwrite(Buf, 1, I, stdout);
        I = 0;
    }
}

void emitPreulde(int Addr) {
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
    // CALL getCharacter
    OP(0xE8); DW((int)getCharacter - AT(4));
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

void emitLoop() {
    // CMP [ebx], 0
    OP(0x80); OP(0x3B); OP(0x00);
    // JE [end]
    OP(0x0F); OP(0x84); Stk[Sp++] = AT(0); DW(0);
}

void emitEndLoop() {
    Sp--;
    // JMP [start]
    OP(0xE9); DW(Stk[Sp] - (3+2) - AT(4));
    // Write the jump offset
    *(int*)(Stk[Sp]) = AT(0) - (Stk[Sp] + 4);
}

int emitReturn(void *Code) {
    // RET
    char opc[1] = {0xC3};
    memcpy(Code, opc, 1);
    return 1;
}

int main(int argc, const char *argv[]) {
    char *Data = VirtualAlloc(0, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    memset(Data, 0, 0x100000);
    Code = VirtualAlloc(0, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    CodePtr = 0;
    char *Input = VirtualAlloc(0, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Buf = VirtualAlloc(0, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

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
    size_t Size = ftell(File);
    fseek(File, 0, SEEK_SET);
    fread(Input, 1, Size, File);
    fclose(File);

    char LastInstr = Input[0];
    char Count = 0;

    emitPreulde((int)Data+0x50000);
    for (int i = 0; i < Size; i++) {
        if (LastInstr != Input[i]) {
            switch (LastInstr) {
            case '>' : emitRight(Count); break;
            case '<' : emitLeft(Count); break;
            case '+' : emitPlus(Count); break;
            case '-' : emitMinus(Count); break;
            }
            Count = 0;
        }
        LastInstr = Input[i];
        switch (Input[i]) {
        case ',' : emitGetchar(); break;
        case '.' : emitPutchar(); break;
        case '[' : emitLoop(); break;
        case ']' : emitEndLoop(); break;
        case '>' : Count++; break;
        case '<' : Count++; break;
        case '+' : Count++; break;
        case '-' : Count++; break;
        }
    }
    switch (LastInstr) {
        case '>' : emitRight(Count); break;
        case '<' : emitLeft(Count); break;
        case '+' : emitPlus(Count); break;
        case '-' : emitMinus(Count); break;
    }

    if (Sp != 0) {
        fprintf(stderr, "Error: unmatched brackets\n");
        return 0;
    }

    CodePtr += emitReturn(Code+CodePtr);

    if (argc == 3) {
        FILE *OutFile = fopen(argv[2], "wb");
        if (!OutFile) {
            fprintf(stderr, "Error: could not open output file\n");
            return 0;
        }
        fwrite(Code, 1, CodePtr, OutFile);
        fclose(OutFile);
    }

    DWORD (*timeGetTime)() = (DWORD(*)())GetProcAddress(LoadLibrary("winmm.dll"), "timeGetTime");

    void (*f)() = (void(*)())Code;
    unsigned int start = timeGetTime();
    f();
    int end = timeGetTime();
    
    fwrite(Buf, 1, I, stdout);

    printf("\nOK (%dms)\n", end-start);
}