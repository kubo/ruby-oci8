/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * hook.c
 *
 * Copyright (C) 2014 KUBO Takehiro <kubo@jiubao.org>
 */
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#endif
#include "oci8.h"
#include "plthook.h"

typedef struct {
    const char *func_name;
    void *func_addr;
    void *old_func_addr;
} hook_func_entry_t;

static int replace_functions(const char * const *files, hook_func_entry_t *functions)
{
    int i;

    for (i = 0; files[i] != NULL; i++) {
        const char *file = files[i];
        plthook_t *ph;
        if (plthook_open(&ph, file) == 0) {
            int j;
            int rv = 0;

            /* install hooks */
            for (j = 0; functions[j].func_name != NULL ; j++) {
                hook_func_entry_t *function = &functions[j];
                rv = plthook_replace(ph, function->func_name, function->func_addr, &function->old_func_addr);
                if (rv != 0) {
                    while (--j >= 0) {
                        /*restore hooked fuction address */
                        plthook_replace(ph, functions[j].func_name, functions[j].old_func_addr, NULL);
                    }
                    plthook_close(ph);
                    rb_raise(rb_eRuntimeError, "Could not replace function %s in %s", function->func_name, file);
                }
            }
            plthook_close(ph);
            return 0;
        }
    }
    return -1;
}

#ifdef WIN32

/* CancelIoEx is available in Windows Vista or later. */
typedef BOOL (WINAPI *CancelIoEx_t)(HANDLE hFile, LPOVERLAPPED lpOverlapped);

typedef struct list_entry {
    struct list_entry *next;
    struct list_entry *prev;
    HANDLE hFile;
} list_entry_t;

#define NUM_ALLOC 20

static CancelIoEx_t CancelIoEx_func;

static CRITICAL_SECTION lock;

static list_entry_t handles_in_use = {
    &handles_in_use, &handles_in_use, NULL,
};

static list_entry_t *free_list;

static WSAEVENT hCancelWSAEvent;

static list_entry_t *get_list_entry(HANDLE hFile)
{
    list_entry_t *entry;

    EnterCriticalSection(&lock);
    /* get an entry from free_list */
    entry = free_list;
    if (entry != NULL) {
        free_list = entry->next;
    } else {
        int i;
        entry = (list_entry_t *)calloc(NUM_ALLOC, sizeof(list_entry_t));
        if (entry == NULL) {
            LeaveCriticalSection(&lock);
            return NULL;
        }
        for (i = 1; i < NUM_ALLOC - 1; i++) {
            entry[i].next = &entry[i + 1];
        }
        entry[NUM_ALLOC - 1].next = NULL;
        free_list = entry + 1;
    }
    /* link to handles_in_use */
    entry->next = handles_in_use.next;
    entry->prev = &handles_in_use;
    handles_in_use.next->prev = entry;
    handles_in_use.next = entry;
    entry->hFile = hFile;
    LeaveCriticalSection(&lock);
    return entry;
}

static void free_list_entry(list_entry_t *entry)
{
    if (entry != NULL) {
        EnterCriticalSection(&lock);
        /* unlink from handles_in_use */
        entry->next->prev = entry->prev;
        entry->prev->next = entry->next;
        /* link to free_list */
        entry->next = free_list;
        free_list = entry;
        LeaveCriticalSection(&lock);
    }
}

/* WSARecv() is used for TCP connections */
static int WSAAPI hook_WSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    if (lpOverlapped == NULL) {
        WSAEVENT hEvents[2];
        DWORD rv;

        hEvents[0] = WSACreateEvent();
        hEvents[1] = hCancelWSAEvent;

        WSAEventSelect(s, hEvents[0], FD_READ | FD_CLOSE);
        rv = WSAWaitForMultipleEvents(2, hEvents, FALSE, WSA_INFINITE, FALSE);
        WSACloseEvent(hEvents[0]);
        if (rv != WAIT_OBJECT_0) {
            if (lpNumberOfBytesRecvd != NULL) {
                *lpNumberOfBytesRecvd = 0;
            }
            return 0;
        }
    }
    return WSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
}

/* ReadFile() is used for BEQ connections */
static BOOL WINAPI hook_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
    list_entry_t *entry = get_list_entry(hFile);
    BOOL ok;

    ok = ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    free_list_entry(entry);
    return ok;
}

static const char * const tcp_func_files[] = {
    /* full client */
    "orantcp12.dll",
    "orantcp11.dll",
    "orantcp10.dll",
    "orantcp9.dll",
    /* instant client basic */
    "oraociei12.dll",
    "oraociei11.dll",
    "oraociei10.dll",
    /* instant client basic lite */
    "oraociicus12.dll",
    "oraociicus11.dll",
    "oraociicus10.dll",
    NULL,
};

static hook_func_entry_t tcp_functions[] = {
    {"WSARecv", (void*)hook_WSARecv, NULL},
    {NULL, NULL, NULL},
};

static const char * const beq_func_files[] = {
    /* full client */
    "oranbeq12.dll",
    "oranbeq11.dll",
    "oranbeq10.dll",
    "oranbeq9.dll",
    NULL,
};

static hook_func_entry_t beq_functions[] = {
    {"ReadFile", (void*)hook_ReadFile, NULL},
    {NULL, NULL, NULL},
};

void oci8_install_hook_functions()
{
    InitializeCriticalSectionAndSpinCount(&lock, 4000);
    hCancelWSAEvent = WSACreateEvent();
    /* CancelIoEx() is available in Windows Vista or later. */
    CancelIoEx_func = (CancelIoEx_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "CancelIoEx");

    if (replace_functions(tcp_func_files, tcp_functions) != 0) {
        rb_raise(rb_eRuntimeError, "No DLL is found to hook.");
    }
}

void oci8_check_win32_beq_functions()
{
    BOOL beq_func_replaced = FALSE;

    if (CancelIoEx_func != NULL && !beq_func_replaced) {
        /* oranbeq??.dll is not loaded until a beq connection is used. */
        if (replace_functions(beq_func_files, beq_functions) == 0) {
            beq_func_replaced = TRUE;
        }
    }
}

void oci8_cancel_read(void)
{
    list_entry_t *entry;
    WSASetEvent(hCancelWSAEvent);

    EnterCriticalSection(&lock);
    for (entry = handles_in_use.next; entry != &handles_in_use; entry = entry->next) {
        CancelIoEx_func(entry->hFile, NULL);
    }
    LeaveCriticalSection(&lock);
}

#else

static int pipefd[2] = {-1, -1};

static ssize_t hook_read(int fd, void *buf, size_t count)
{
    fd_set rfds;
    int rv;
    int nfd;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    FD_SET(pipefd[0], &rfds);
    if (fd > pipefd[0]) {
        nfd = fd + 1;
    } else {
        nfd = pipefd[0] + 1;
    }
    do {
        rv = select(nfd, &rfds, NULL, NULL, NULL);
    } while (rv == -1 && errno == EINTR);
    if (rv > 0 && FD_ISSET(pipefd[0], &rfds)) {
        /* emulate EOF to cancel this read request. */
        return 0;
    }
    return read(fd, buf, count);
}

static const char * const files[] = {
    "libclntsh.so.12.1",
    "libclntsh.so.11.1",
    "libclntsh.so.10.1",
    "libclntsh.so.9.0",
    NULL,
};

static hook_func_entry_t functions[] = {
    {"read", (void*)hook_read, NULL},
    {NULL, NULL, NULL},
};

void oci8_install_hook_functions()
{
    if (pipe(pipefd) != 0) {
        rb_sys_fail("pipe");
    }
    if (replace_functions(files, functions) != 0) {
        rb_raise(rb_eRuntimeError, "No shared library is found to hook.");
    }
}

/* This function is called on ruby process termination
 * to cancel all read requests issued by libclntsh.so.
 */
void oci8_cancel_read(void)
{
    static int write_len = 0;
    if (write_len == 0) {
        write_len = write(pipefd[1], "!", 1);
    }
}

#endif
