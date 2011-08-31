/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * thread_util.h - part of ruby-oci8
 *
 * Copyright (C) 2011 KUBO Takehiro <kubo@jiubao.org>
 */
#ifndef NATIVE_THREAD_H

/*
 * Prepare to execute thread-related functions.
 */
void Init_oci8_thread_util(void);

/*
 * Run the func in a new native thread.
 * Don't call any ruby functions in the func.
 * The return value is errno.
 */
int oci8_run_native_thread(rb_blocking_function_t func, void *arg);

#endif
