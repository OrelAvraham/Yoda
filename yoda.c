#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/user.h>
#include <sys/types.h>

#define SYS_recvfrom 45
#define BUF_SIZE 8

int read_buffer_from_memory(pid_t pid, unsigned long address, size_t size, int b_stop_at_null_byte, unsigned char* o_buffer)
{
    char buffer[size];
    size_t read_size = 0;
    
    printf("Reading %lld bytes from %x\n", size, address);

    while (read_size < size ) {
        // Read data in words (8 bytes)
        long data = ptrace(PTRACE_PEEKDATA, pid, address + read_size, NULL);
        // printf("0x%08x  ", data);
        if (data == -1) {
            perror("ptrace peekdata error");
            //exit(1);
        }
        *(long*)(buffer + read_size) = data;
        memcpy(o_buffer + read_size, &data, BUF_SIZE);
        read_size += BUF_SIZE;
    }
    printf("\nRead %lld bytes\n", read_size);


    // Print the received data
    printf("Received data: %s\n", buffer);
    return 0;
}

int write_string_to_memory(pid_t pid, unsigned long address, const char *buffer) {
    size_t len = strlen(buffer) + 1; // Include null terminator
    size_t written_size;
    long data;

    for (written_size = 0; written_size < len; written_size += BUF_SIZE) {
        memcpy(&data, buffer + written_size, BUF_SIZE);
        printf("Injecting %08x, to %08x\n", data, address + written_size);
        if (ptrace(PTRACE_POKEDATA, pid, address + written_size, data) == -1) {
            perror("ptrace pokedata error");
            //exit(1);
        }
    }

    return 0;
}

void reverse_string(char *str) {
    if (str == NULL) return;

    int start = 0;
    int end = strlen(str) - 2; // -2: -1 for null byte and -1 for new line

    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;

        start++;
        end--;
    }
}

void handle_recvfrom(pid_t pid){
    struct user_regs_struct regs;
        if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        perror("ptrace getregs");
        exit(1);
    }
    
    char* buf[regs.rdx];
    read_buffer_from_memory(pid, regs.rsi, regs.rdx, 0, buf);
    printf("Regular is: %s;  ", buf);
    reverse_string(buf);
    printf("Reverse is: %s\n", buf);
    write_string_to_memory(pid, regs.rsi, buf);
}


void trace_syscalls(pid_t pid) {
    int status;
    struct user_regs_struct regs;
    int recvfrom_count = 0;
    // Attach to the process
    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1) {
        perror("ptrace attach");
        exit(1);
    }

    waitpid(pid, &status, 0);  // Wait for the process to stop

    // Trace the syscalls in the process
    while (1) {
        // Wait for the next syscall entry (first stop)
        if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) == -1) {
            perror("ptrace syscall");
            exit(1);
        }

        waitpid(pid, &status, 0);  // Wait for the syscall to enter

        // Retrieve the syscall number (before the syscall is executed)
        if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
            perror("ptrace getregs");
            exit(1);
        }

        //printf("SYSCALL %lld\n", regs.orig_rax);

        // Check if the syscall is 'recvfrom' (syscall number 45 on x86_64)
        if (regs.orig_rax == 45) {
            recvfrom_count += 1;
            printf("==== recvfrom %d ====\n", recvfrom_count);
            // Optionally, print arguments passed to the syscall (before it executes)
            //printf("recvfrom(%lld, %lld, %lld, %lld, %lld, %lld)\n", regs.rdi, regs.rsi, regs.rdx, regs.r10, regs.r8, regs.r9);
            printf("  File descriptor: %lld\n", regs.rdi);
            printf("  Buffer pointer: %lld\n", regs.rsi);
            printf("  Length: %lld\n", regs.rdx);
            printf("  Flags: %lld\n", regs.r10);
            printf("  Address: %lld\n", regs.r8);
            printf("  Address length: %lld\n", regs.r9);
            printf("\n  regular rax: %08x\n", regs.rax);
            printf("  status: %08x\n", status);
            handle_recvfrom(pid);

            printf("==================\n");
        }

        // // Wait for the syscall to exit (second stop)
        // if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) == -1) {
        //     perror("ptrace syscall");
        //     exit(1);
        // }

        // waitpid(pid, &status, 0);  // Wait for the syscall to exit

        // // Retrieve the syscall number (after the syscall has been executed)
        // if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == -1) {
        //     perror("ptrace getregs");
        //     exit(1);
        // }

        // printf("EXIT %lld\n", regs.orig_rax);


    }

    // Detach after tracing
    ptrace(PTRACE_DETACH, pid, NULL, NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid of the server to trace>\n", argv[0]);
        exit(1);
    }

    pid_t pid = atoi(argv[1]);  // Convert the PID argument to an integer

    trace_syscalls(pid);

    return 0;
}
