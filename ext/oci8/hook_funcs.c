/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * hook.c
 *
 * Copyright (C) 2015 Kubo Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"
#include "plthook.h"
#ifndef WIN32
#include <unistd.h>
#include <sys/socket.h>
#endif

#define DEBUG_HOOK_FUNCS 1

#ifdef WIN32
static CRITICAL_SECTION lock;
#define LOCK(lock) EnterCriticalSection(lock)
#define UNLOCK(lock) LeaveCriticalSection(lock)
#else
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK(lock) pthread_mutex_lock(lock)
#define UNLOCK(lock) pthread_mutex_unlock(lock)
#define SOCKET int
#define INVALID_SOCKET (-1)
#endif

typedef struct {
    const char *func_name;
    void *func_addr;
    void *old_func_addr;
} hook_func_entry_t;

typedef struct socket_entry {
    struct socket_entry *next;
    struct socket_entry *prev;
    SOCKET sock;
} socket_entry_t;

static socket_entry_t sockets_in_use = {
    &sockets_in_use, &sockets_in_use, INVALID_SOCKET,
};

static void socket_entry_set(socket_entry_t *entry, SOCKET sock)
{
    LOCK(&lock);
    entry->next = sockets_in_use.next;
    entry->prev = &sockets_in_use;
    sockets_in_use.next->prev = entry;
    sockets_in_use.next = entry;
    entry->sock = sock;
    UNLOCK(&lock);
}

static void socket_entry_clear(socket_entry_t *entry)
{
    LOCK(&lock);
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
    UNLOCK(&lock);
}

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

static int locK_is_initialized;

static int WSAAPI hook_WSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

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

/* WSARecv() is used for TCP connections */
static int WSAAPI hook_WSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    socket_entry_t entry;
    int rv;

    socket_entry_set(&entry, s);
    rv = WSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
    socket_entry_clear(&entry);
    return rv;
}

void oci8_install_hook_functions()
{
    InitializeCriticalSectionAndSpinCount(&lock, 5000);
    locK_is_initialized = 1;

    if (replace_functions(tcp_func_files, tcp_functions) != 0) {
        rb_raise(rb_eRuntimeError, "No DLL is found to hook.");
    }
}

static void shutdown_socket(socket_entry_t *entry)
{
    /* This is dangerous. But I don't know how to cancel WSARecv().
     * This technique is usable only at the process termination.
     * Otherwise, Oracle client library may close sockets used by
     * others.
     */
    closesocket(entry->sock);
}

#else
static ssize_t hook_read(int fd, void *buf, size_t count);

#ifdef __APPLE__
#define SO_EXT "dylib"
#else
#define SO_EXT "so"
#endif

static const char * const files[] = {
    "libclntsh." SO_EXT ".12.1",
    "libclntsh." SO_EXT ".11.1",
    "libclntsh." SO_EXT ".10.1",
    "libclntsh." SO_EXT ".9.0",
    NULL,
};

static hook_func_entry_t functions[] = {
    {"read", (void*)hook_read, NULL},
    {NULL, NULL, NULL},
};

static ssize_t hook_read(int fd, void *buf, size_t count)
{
    socket_entry_t entry;
    ssize_t rv;

    socket_entry_set(&entry, fd);
    rv = read(fd, buf, count);
    socket_entry_clear(&entry);
    return rv;
}

void oci8_install_hook_functions(void)
{
    if (replace_functions(files, functions) != 0) {
        rb_raise(rb_eRuntimeError, "No shared library is found to hook.");
    }
}

static void shutdown_socket(socket_entry_t *entry)
{
    shutdown(entry->sock, SHUT_RDWR);
}
#endif

void oci8_shutdown_sockets(void)
{
    socket_entry_t *entry;

#ifdef WIN32
    if (!locK_is_initialized) {
        return;
    }
#endif

    LOCK(&lock);
    for (entry = sockets_in_use.next; entry != &sockets_in_use; entry = entry->next) {
        shutdown_socket(entry);
    }
    UNLOCK(&lock);
}
