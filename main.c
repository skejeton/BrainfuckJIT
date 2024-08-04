// clang-format off
#include <stdio.h>
#include <windows.h>
#include <timeapi.h>

int MP = 0;
char *Buf;
int I = 0;

void getCharacter() {
    *((char*)MP) = getchar();
}

void putCharacter() {
    Buf[I++] = *(char*)MP;
    Buf[I] = 0;
    // NOTE: To minimize I/O, we only flush the buffer when a newline appears, (also at the end).
    if (*(char*)MP == '\n') {
        fwrite(Buf, 1, I, stdout);
        I = 0;
    }
}

int emitRight(void *Code, char Count) {
    // ADD [memptr], Count
    char opc[7] = {0x83, 0x05};
    *(int*)(opc + 2) = (int)&MP;
    opc[6] = Count;
    memcpy(Code, opc, 7);
    return 7;
}

int emitLeft(void *Code, char Count) {
    // SUB [memptr], Count
    char opc[7] = {0x83, 0x2D};
    *(int*)(opc + 2) = (int)&MP;
    opc[6] = Count;
    memcpy(Code, opc, 7);
    return 7;
}

int emitPlus(void *Code, char Count) {
    // MOV eax, [memptr]
    // ADD [eax], Count
    char opc[8] = {0xa1};
    *(int*)(opc + 1) = (int)&MP;
    opc[5] = 0x80;
    opc[7] = Count;
    memcpy(Code, opc, 8);
    return 8;
}

int emitMinus(void *Code, char Count) {
    // MOV eax, [memptr]
    // ADD [eax], Count
    char opc[8] = {0xa1};
    *(int*)(opc + 1) = (int)&MP;
    opc[5] = 0x80;
    opc[6] = 0x28;
    opc[7] = Count;
    memcpy(Code, opc, 8);
    return 8;
}

int emitGetchar(void *Code) {
    // CALL getchar
    char opc[5] = {0};
    opc[0] = 0xE8;
    *(int*)(opc + 1) = (int)getCharacter - (int)Code - 5;
    memcpy(Code, opc, 5);
    return 5;
}

int emitPutchar(void *Code) {
    // CALL putchar
    char opc[5] = {0};
    opc[0] = 0xE8;
    *(int*)(opc + 1) = (int)putCharacter - (int)Code - 5;
    memcpy(Code, opc, 5);
    return 5;
}

int emitLoop(void *Code, int *Stk, int *Sp) {
    // MOV eax, [memptr]
    // CMP [eax], 0
    // JE [end]
    char opc[5 + 3 + 6] = {0x00};
    opc[0] = 0xA1;
    *(int*)(opc + 1) = (int)&MP;
    opc[5] = 0x80;
    opc[6] = 0x38;
    opc[7] = 0x00;
    opc[8] = 0x0F;
    opc[9] = 0x84;
    Stk[(*Sp)++] = (int)Code + 5 + 3 + 2;
    memcpy(Code, opc, 5 + 3 + 6);
    return 5 + 3 + 6;
}

int emitEndLoop(void *Code, int *Stk, int *Sp) {
    // JMP [start]
    (*Sp)--;
    char opc[5] = {0x00};
    opc[0] = 0xE9;
    *(int*)(Stk[*Sp]) = ((int)Code + 5) - (Stk[*Sp] + 4);
    *(int*)(opc + 1) = (Stk[*Sp] - (5+3+2)) - ((int)Code + 5);
    memcpy(Code, opc, 5);
    return 5;
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
    char *Code = VirtualAlloc(0, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    char *Input = VirtualAlloc(0, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Buf = VirtualAlloc(0, 0x100000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    MP = (int)Data+0x50000;

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

    int Offs = 0;
    char LastInstr = Input[0];
    char Count = 0;
    int Stk[256];
    int Sp = 0;

    for (int i = 0; i < Size; i++) {
        if (LastInstr != Input[i]) {
            switch (LastInstr) {
            case '>' : Offs += emitRight(Code+Offs, Count); break;
            case '<' : Offs += emitLeft(Code+Offs, Count); break;
            case '+' : Offs += emitPlus(Code+Offs, Count); break;
            case '-' : Offs += emitMinus(Code+Offs, Count); break;
            }
            Count = 0;
        }
        LastInstr = Input[i];
        switch (Input[i]) {
        case ',' : Offs += emitGetchar(Code+Offs); break;
        case '.' : Offs += emitPutchar(Code+Offs); break;
        case '[' : Offs += emitLoop(Code+Offs, Stk, &Sp); break;
        case ']' : Offs += emitEndLoop(Code+Offs, Stk, &Sp); break;
        case '>' : Count++; break;
        case '<' : Count++; break;
        case '+' : Count++; break;
        case '-' : Count++; break;
        }
    }
    switch (LastInstr) {
        case '>' : Offs += emitRight(Code+Offs, Count); break;
        case '<' : Offs += emitLeft(Code+Offs, Count); break;
        case '+' : Offs += emitPlus(Code+Offs, Count); break;
        case '-' : Offs += emitMinus(Code+Offs, Count); break;
    }

    if (Sp != 0) {
        fprintf(stderr, "Error: unmatched brackets\n");
        return 0;
    }

    Offs += emitReturn(Code+Offs);

    if (argc == 3) {
        FILE *OutFile = fopen(argv[2], "wb");
        if (!OutFile) {
            fprintf(stderr, "Error: could not open output file\n");
            return 0;
        }
        fwrite(Code, 1, Offs, OutFile);
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