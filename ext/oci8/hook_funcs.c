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

#ifdef WIN32

static WSAEVENT hCancelWSAEvent;

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

static BOOL WINAPI hook_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
    /* TODO: */
    return ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
}

void oci8_cancel_read(void)
{
    WSASetEvent(hCancelWSAEvent);
}

static const char * const sock_func_files[] = {
    /* full client */
    "orantcp12.dll",
    "orantcp11.dll",
    "orantcp10.dll",
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

static hook_func_entry_t sock_functions[] = {
    {"WSARecv", (void*)hook_WSARecv, NULL},
    {NULL, NULL, NULL},
};

static const char * const beq_func_files[] = {
    /* full client */
    "oranbeq12.dll",
    "oranbeq11.dll",
    "oranbeq10.dll",
    NULL,
};

static hook_func_entry_t beq_functions[] = {
    {"ReadFile", (void*)hook_ReadFile, NULL},
    {NULL, NULL, NULL},
};

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
        /* emulate EOF to stop the query. */
        return 0;
    }
    return read(fd, buf, count);
}

void oci8_cancel_read(void)
{
    static int write_len = 0;
    if (write_len == 0) {
        write_len = write(pipefd[1], "!", 1);
    }
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

#endif

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
                        plthook_replace(ph, function->func_name, function->old_func_addr, NULL);
                    }
                    plthook_close(ph);
                    rb_raise(rb_eRuntimeError, "Could not replace function %s in %s", function->func_name, file);
                }
            }
            plthook_close(ph);
            return rv;
        }
    }
    return -1;
}

void oci8_hook_functions(void)
{
#ifdef WIN32
    static int initialized = 0;
    static int sock_func_replaced = 0;
    static int beq_func_replaced = 0;

    if (!initialized) {
        hCancelWSAEvent = WSACreateEvent();
        initialized = 1;
    }
    if (!sock_func_replaced) {
        if (replace_functions(sock_func_files, sock_functions) == 0) {
            sock_func_replaced = 1;
        }
    }
    if (!beq_func_replaced) {
        if (replace_functions(beq_func_files, beq_functions) == 0) {
            beq_func_replaced = 1;
        }
    }
#else
    static int initialized = 0;

    if (!initialized) {
        if (pipe(pipefd) != 0) {
            rb_sys_fail("pipe");
        }
        replace_functions(files, functions);
        initialized = 1;
    }
#endif
}
