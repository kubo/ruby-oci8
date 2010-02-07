/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * oranumber_util.h - part of ruby-oci8
 *
 * Copyright (C) 2010 KUBO Takehiro <kubo@jiubao.org>
 */
#ifndef ORANUMBER_UTIL_H
#define ORANUMBER_UTIL_H 1
#include <orl.h>

#define ORANUMBER_SUCCESS 0
#define ORANUMBER_INVALID_NUMBER 1722
#define ORANUMBER_NUMERIC_OVERFLOW 1426

int oranumber_to_str(const OCINumber *on, char *buf);
int oranumber_from_str(OCINumber *on, const char *buf, int buflen);

#define ORANUMBER_DUMP_BUF_SIZ 99
int oranumber_dump(const OCINumber *on, char *buf);

#endif
