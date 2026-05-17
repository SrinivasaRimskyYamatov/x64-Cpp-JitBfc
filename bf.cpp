#include <iostream>
#include <vector>
#include <fstream>
#include <stack>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cout << "usage: bf program.bf\n";
        return 0;
    }

    std::ifstream f(argv[1]);
    std::string program(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>()
    );

    unsigned char tape[30000] = {0};

    size_t code_size = 1024 * 1024;

    unsigned char* code = (unsigned char*)mmap(
        nullptr,
        code_size,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (code == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    unsigned char* p = code;

    std::stack<unsigned char*> loop_stack;

    for (char c : program) {

        switch (c) {

        case '>':
            *p++ = 0x49;
            *p++ = 0xff;
            *p++ = 0xc5;
            break;

        case '<':
            *p++ = 0x49;
            *p++ = 0xff;
            *p++ = 0xcd;
            break;

        case '+':
            *p++ = 0x41;
            *p++ = 0xfe;
            *p++ = 0x45;
            *p++ = 0x00;
            break;

        case '-':
            *p++ = 0x41;
            *p++ = 0xfe;
            *p++ = 0x4d;
            *p++ = 0x00;
            break;

        case '.':
            *p++ = 0x48; *p++ = 0xc7; *p++ = 0xc0;
            *(int*)p = 1; p += 4;

            *p++ = 0x48; *p++ = 0xc7; *p++ = 0xc7;
            *(int*)p = 1; p += 4;

            *p++ = 0x4c; *p++ = 0x89; *p++ = 0xee;

            *p++ = 0x48; *p++ = 0xc7; *p++ = 0xc2;
            *(int*)p = 1; p += 4;

            *p++ = 0x0f; *p++ = 0x05;

            break;

        case ',':
            *p++ = 0x48; *p++ = 0x31; *p++ = 0xc0;
            *p++ = 0x48; *p++ = 0x31; *p++ = 0xff;
            *p++ = 0x4c; *p++ = 0x89; *p++ = 0xee;

            *p++ = 0x48; *p++ = 0xc7; *p++ = 0xc2;
            *(int*)p = 1; p += 4;

            *p++ = 0x0f; *p++ = 0x05;

            break;

        case '[': {
            unsigned char* start = p;

            *p++ = 0x41;
            *p++ = 0x80;
            *p++ = 0x7d;
            *p++ = 0x00;
            *p++ = 0x00;

            *p++ = 0x0f;
            *p++ = 0x84;

            *(int*)p = 0;
            p += 4;

            loop_stack.push(start);

            break;
        }

        case ']': {

            auto start = loop_stack.top();
            loop_stack.pop();

            unsigned char* end = p;

            *p++ = 0x41;
            *p++ = 0x80;
            *p++ = 0x7d;
            *p++ = 0x00;
            *p++ = 0x00;

            *p++ = 0x0f;
            *p++ = 0x85;

            int back = start - p - 4;
            *(int*)p = back;
            p += 4;

            int forward = p - start - 11;
            *(int*)(start + 7) = forward;

            break;
        }

        }
    }

    *p++ = 0xc3;

    typedef void (*JITFUNC)(unsigned char*);
    JITFUNC func = (JITFUNC)code;

    asm volatile("mov %0, %%r13" : : "r"(tape));

    func(tape);

    munmap(code, code_size);
}
