/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * bind.c
 *
 * Copyright (C) 2002-2014 Kubo Takehiro <kubo@jiubao.org>
 */
#include "oci8.h"

#ifndef OCI_ATTR_MAXCHAR_SIZE
#define OCI_ATTR_MAXCHAR_SIZE 164
#endif

static ID id_bind_type;
static ID id_charset_form;
static VALUE sym_length;
static VALUE sym_length_semantics;
static VALUE sym_char;
static VALUE sym_nchar;

static VALUE cOCI8BindTypeBase;

typedef struct {
    oci8_bind_t obind;
    sb4 bytelen;
    sb4 charlen;
    ub1 csfrm;
} oci8_bind_string_t;

static ub4 initial_chunk_size = 32 * 1024;
static ub4 max_chunk_size = 8 * 1024 * 1024;

typedef struct chunk {
    struct chunk *next;
    ub4 alloc_len;
    ub4 used_len;
    char buf[1];
} chunk_t;

typedef struct {
    chunk_t *head;
    chunk_t **tail;
    chunk_t **inpos;
} chunk_buf_t;

typedef struct {
    oci8_bind_t obind;
    ub1 csfrm;
} oci8_bind_long_t;

#define IS_BIND_LONG(obind) (((oci8_bind_data_type_t*)obind->base.data_type)->dty == SQLT_CHR)

const oci8_handle_data_type_t oci8_bind_data_type = {
    {
        "OCI8::BindType::Base",
        {
            NULL,
            oci8_handle_cleanup,
            oci8_handle_size,
        },
        &oci8_handle_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
        RUBY_TYPED_WB_PROTECTED,
#endif
    },
    NULL,
    0,
};

/*
 * bind_string
 */
static VALUE bind_string_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    oci8_vstr_t *vstr = (oci8_vstr_t *)data;
    return rb_external_str_new_with_enc(vstr->buf, vstr->size, oci8_encoding);
}

static void bind_string_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    oci8_bind_string_t *obs = (oci8_bind_string_t *)obind;
    oci8_vstr_t *vstr = (oci8_vstr_t *)data;

    OCI8StringValue(val);
    if (RSTRING_LEN(val) > obs->bytelen) {
        rb_raise(rb_eArgError, "too long String to set. (%ld for %d)", RSTRING_LEN(val), obs->bytelen);
    }
    memcpy(vstr->buf, RSTRING_PTR(val), RSTRING_LEN(val));
    vstr->size = RSTRING_LENINT(val);
}

static void bind_string_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE param)
{
    oci8_bind_string_t *obs = (oci8_bind_string_t *)obind;
    VALUE length;
    VALUE length_semantics;
    VALUE nchar;
    sb4 sz;

    Check_Type(param, T_HASH);
    length = rb_hash_aref(param, sym_length);
    length_semantics = rb_hash_aref(param, sym_length_semantics);
    nchar = rb_hash_aref(param, sym_nchar);

    sz = NUM2INT(length);
    if (sz < 0) {
        rb_raise(rb_eArgError, "invalid bind length %d", sz);
    }
    if (length_semantics == sym_char) {
        /* character semantics */
        obs->charlen = sz;
        obs->bytelen = sz = sz * oci8_nls_ratio;
        if (oci8_nls_ratio == 1) {
            /* sz must be bigger than charlen to suppress ORA-06502.
             * I don't know the reason...
             */
            sz *= 2;
        }
    } else {
        /* byte semantics */
        obs->bytelen = sz;
        obs->charlen = 0;
    }
    if (RTEST(nchar)) {
        obs->csfrm = SQLCS_NCHAR; /* bind as NCHAR/NVARCHAR2 */
    } else {
        obs->csfrm = SQLCS_IMPLICIT; /* bind as CHAR/VARCHAR2 */
    }
    if (sz == 0) {
        sz = 1; /* to avoid ORA-01459. */
    }
    sz += sizeof(sb4);
    obind->value_sz = sz;
    obind->alloc_sz = (sz + (sizeof(sb4) - 1)) & ~(sizeof(sb4) - 1);
}

static void bind_string_post_bind_hook(oci8_bind_t *obind)
{
    oci8_bind_string_t *obs = (oci8_bind_string_t *)obind;

    if (obs->charlen != 0) {
        chker2(OCIAttrSet(obind->base.hp.ptr, obind->base.type, (void*)&obs->charlen, 0, OCI_ATTR_MAXCHAR_SIZE, oci8_errhp),
               &obind->base);
    }
    chker2(OCIAttrSet(obind->base.hp.ptr, obind->base.type, (void*)&obs->csfrm, 0, OCI_ATTR_CHARSET_FORM, oci8_errhp),
           &obind->base);
}

static const oci8_bind_data_type_t bind_string_data_type = {
    {
        {
            "OCI8::BindType::String",
            {
                NULL,
                oci8_handle_cleanup,
                oci8_handle_size,
            },
            &oci8_bind_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
            RUBY_TYPED_WB_PROTECTED,
#endif
        },
        oci8_bind_free,
        sizeof(oci8_bind_string_t)
    },
    bind_string_get,
    bind_string_set,
    bind_string_init,
    NULL,
    NULL,
    SQLT_LVC,
    bind_string_post_bind_hook,
};

static VALUE bind_string_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_string_data_type.base);
}

/*
 * bind_raw
 */
static VALUE bind_raw_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    oci8_vstr_t *vstr = (oci8_vstr_t *)data;
    return rb_str_new(vstr->buf, vstr->size);
}

static void bind_raw_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    oci8_bind_string_t *obs = (oci8_bind_string_t *)obind;
    oci8_vstr_t *vstr = (oci8_vstr_t *)data;

    StringValue(val);
    if (RSTRING_LEN(val) > obs->bytelen) {
        rb_raise(rb_eArgError, "too long String to set. (%ld for %d)", RSTRING_LEN(val), obs->bytelen);
    }
    memcpy(vstr->buf, RSTRING_PTR(val), RSTRING_LEN(val));
    vstr->size = RSTRING_LENINT(val);
}

static const oci8_bind_data_type_t bind_raw_data_type = {
    {
        {
            "OCI8::BindType::RAW",
            {
                NULL,
                oci8_handle_cleanup,
                oci8_handle_size,
            },
            &oci8_bind_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
            RUBY_TYPED_WB_PROTECTED,
#endif
        },
        oci8_bind_free,
        sizeof(oci8_bind_string_t)
    },
    bind_raw_get,
    bind_raw_set,
    bind_string_init,
    NULL,
    NULL,
    SQLT_LVB
};

static VALUE bind_raw_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_raw_data_type.base);
}

/*
 * bind_binary_double
 */
static VALUE bind_binary_double_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    return rb_float_new(*(double*)data);
}

static void bind_binary_double_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    /* val is converted to Float if it isn't Float. */
    *(double*)data = RFLOAT_VALUE(rb_Float(val));
}

static void bind_binary_double_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length)
{
    obind->value_sz = sizeof(double);
    obind->alloc_sz = sizeof(double);
}

static const oci8_bind_data_type_t bind_binary_double_data_type = {
    {
        {
            "OCI8::BindType::BinaryDouble",
            {
                NULL,
                oci8_handle_cleanup,
                oci8_handle_size,
            },
            &oci8_bind_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
            RUBY_TYPED_WB_PROTECTED,
#endif
        },
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    bind_binary_double_get,
    bind_binary_double_set,
    bind_binary_double_init,
    NULL,
    NULL,
    SQLT_BDOUBLE
};

static VALUE bind_binary_double_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_binary_double_data_type.base);
}

/*
 * bind_boolean
 */
static VALUE bind_boolean_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    return *(int*)data ? Qtrue : Qfalse;
}

static void bind_boolean_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    *(int*)data = RTEST(val) ? -1 : 0;
}

static void bind_boolean_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length)
{
    obind->value_sz = sizeof(int);
    obind->alloc_sz = sizeof(int);
}

#ifndef SQLT_BOL
#define SQLT_BOL 252
#endif
static const oci8_bind_data_type_t bind_boolean_data_type = {
    {
        {
            "OCI8::BindType::Boolean",
            {
                NULL,
                oci8_handle_cleanup,
                oci8_handle_size,
            },
            &oci8_bind_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
            RUBY_TYPED_WB_PROTECTED,
#endif
        },
        oci8_bind_free,
        sizeof(oci8_bind_t)
    },
    bind_boolean_get,
    bind_boolean_set,
    bind_boolean_init,
    NULL,
    NULL,
    SQLT_BOL,
};

static VALUE bind_boolean_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_boolean_data_type.base);
}

/*
 * bind_long
 */
static chunk_t *next_chunk(chunk_buf_t *cb)
{
   chunk_t *chunk;

   if (*cb->tail != NULL) {
       chunk = *cb->tail;
   } else {
       ub4 alloc_len;
       if (cb->head == NULL) {
           alloc_len = initial_chunk_size;
       } else {
           alloc_len = ((chunk_t*)((size_t)cb->tail - offsetof(chunk_t, next)))->alloc_len * 2;
           if (alloc_len > max_chunk_size) {
               alloc_len = max_chunk_size;
           }
       }
       chunk = xmalloc(offsetof(chunk_t, buf) + alloc_len);
       chunk->next = NULL;
       chunk->alloc_len = alloc_len;
       *cb->tail = chunk;
   }
   cb->tail = &chunk->next;
   return chunk;
}

static sb4 define_callback(void *octxp, OCIDefine *defnp, ub4 iter, void **bufpp, ub4 **alenp, ub1 *piecep, void **indp, ub2 **rcodep)
{
    oci8_bind_t *obind = (oci8_bind_t *)octxp;
    chunk_buf_t *cb = ((chunk_buf_t*)obind->valuep) + iter;
    chunk_t *chunk;

    if (*piecep == OCI_FIRST_PIECE) {
        cb->tail = &cb->head;
    }
    chunk = next_chunk(cb);
    chunk->used_len = chunk->alloc_len;
    *bufpp = chunk->buf;
    *alenp = &chunk->used_len;
    *indp = (void*)&obind->u.inds[iter];
    *rcodep = NULL;
    return OCI_CONTINUE;
}

static sb4 in_bind_callback(void *ictxp, OCIBind *bindp, ub4 iter, ub4 index, void **bufpp, ub4 *alenp, ub1 *piecep, void **indp)
{
    oci8_bind_t *obind = (oci8_bind_t *)ictxp;
    chunk_buf_t *cb = ((chunk_buf_t*)obind->valuep) + iter;

    if (cb->tail == &cb->head) {
        /* empty string */
        *bufpp = (void *)"";
        *alenp = 0;
        *piecep = OCI_ONE_PIECE;
    } else {
        chunk_t *chunk = *cb->inpos;
        *bufpp = chunk->buf;
        *alenp = chunk->used_len;
        if (cb->tail == &cb->head->next) {
            *piecep = OCI_ONE_PIECE;
        } else if (cb->inpos == &cb->head) {
            *piecep = OCI_FIRST_PIECE;
            cb->inpos = &chunk->next;
        } else if (&chunk->next != cb->tail) {
            *piecep = OCI_NEXT_PIECE;
            cb->inpos = &chunk->next;
        } else {
            *piecep = OCI_LAST_PIECE;
            cb->inpos = &cb->head;
        }
    }
    *indp = (void*)&obind->u.inds[iter];
    return OCI_CONTINUE;
}

static sb4 out_bind_callback(void *octxp, OCIBind *bindp, ub4 iter, ub4 index, void **bufpp, ub4 **alenp, ub1 *piecep, void **indp, ub2 **rcodep)
{
    oci8_bind_t *obind = (oci8_bind_t *)octxp;
    chunk_buf_t *cb = ((chunk_buf_t*)obind->valuep) + iter;
    chunk_t *chunk;

    if (*piecep == OCI_ONE_PIECE) {
        *piecep = OCI_FIRST_PIECE;
        cb->tail = &cb->head;
    }
    chunk = next_chunk(cb);
    chunk->used_len = chunk->alloc_len;
    *bufpp = chunk->buf;
    *alenp = &chunk->used_len;
    *indp = (void*)&obind->u.inds[iter];
    *rcodep = NULL;
    return OCI_CONTINUE;
}

static void bind_long_free(oci8_base_t *base)
{
    oci8_bind_t *obind = (oci8_bind_t *)base;
    chunk_buf_t *cb = (chunk_buf_t *)obind->valuep;

    if (cb != NULL) {
        ub4 idx = 0;
        do {
            chunk_t *chunk, *chunk_next;
            for (chunk = cb[idx].head; chunk != NULL; chunk = chunk_next) {
                chunk_next = chunk->next;
                xfree(chunk);
            }
        } while (++idx < obind->maxar_sz);
    }
    oci8_bind_free(base);
}

static VALUE bind_long_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    chunk_buf_t *cb = (chunk_buf_t *)data;
    chunk_t *chunk;
    long len = 0;
    VALUE str;
    char *buf;

    for (chunk = cb->head; chunk != *cb->tail; chunk = chunk->next) {
        len += chunk->used_len;
    }
    str = rb_str_buf_new(len);
    buf = RSTRING_PTR(str);
    for (chunk = cb->head; chunk != *cb->tail; chunk = chunk->next) {
        memcpy(buf, chunk->buf, chunk->used_len);
        buf += chunk->used_len;
    }
    rb_str_set_len(str, len);
    if (IS_BIND_LONG(obind)) {
        rb_encoding *enc = rb_default_internal_encoding();

        rb_enc_associate(str, oci8_encoding);
        if (enc != NULL) {
            str = rb_str_conv_enc(str, oci8_encoding, enc);
        }
    }
    return str;
}

static void bind_long_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    chunk_buf_t *cb = (chunk_buf_t *)data;
    ub4 len;
    const char *buf;

    if (IS_BIND_LONG(obind)) {
        OCI8StringValue(val);
    } else {
        StringValue(val);
    }
    len = (ub4)RSTRING_LEN(val);
    buf = RSTRING_PTR(val);
    cb->tail = &cb->head;
    while (1) {
        chunk_t *chunk = next_chunk(cb);
        if (len <= chunk->alloc_len) {
            memcpy(chunk->buf, buf, len);
            chunk->used_len = len;
            break;
        }
        memcpy(chunk->buf, buf, chunk->alloc_len);
        chunk->used_len = chunk->alloc_len;
        len -= chunk->alloc_len;
        buf += chunk->alloc_len;
    }
}

static void bind_long_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE param)
{
    if (IS_BIND_LONG(obind)) {
        oci8_bind_long_t *obl = (oci8_bind_long_t *)obind;
        VALUE nchar;

        if (rb_respond_to(param, id_charset_form)) {
            VALUE csfrm = rb_funcall(param, id_charset_form, 0);
            nchar = (csfrm == sym_nchar) ? Qtrue : Qfalse;
        } else {
            Check_Type(param, T_HASH);
            nchar = rb_hash_aref(param, sym_nchar);
        }

        if (RTEST(nchar)) {
            obl->csfrm = SQLCS_NCHAR; /* bind as NCHAR/NVARCHAR2 */
        } else {
            obl->csfrm = SQLCS_IMPLICIT; /* bind as CHAR/VARCHAR2 */
        }
    }
    obind->value_sz = SB4MAXVAL;
    obind->alloc_sz = sizeof(chunk_buf_t);
}

static void bind_long_init_elem(oci8_bind_t *obind, VALUE svc)
{
    chunk_buf_t *cb = (chunk_buf_t *)obind->valuep;
    ub4 idx = 0;

    do {
        cb[idx].tail = &cb[idx].head;
        cb[idx].inpos = &cb[idx].head;
    } while (++idx < obind->maxar_sz);
}

static void bind_long_post_bind_hook(oci8_bind_t *obind)
{
    oci8_bind_long_t *ds = (oci8_bind_long_t *)obind;

    if (IS_BIND_LONG(obind)) {
        chker2(OCIAttrSet(obind->base.hp.ptr, obind->base.type, (void*)&ds->csfrm, 0, OCI_ATTR_CHARSET_FORM, oci8_errhp),
               &obind->base);
    }
    switch (obind->base.type) {
    case OCI_HTYPE_DEFINE:
        chker2(OCIDefineDynamic(obind->base.hp.dfn, oci8_errhp, obind, define_callback),
               &obind->base);
        break;
    case OCI_HTYPE_BIND:
        chker2(OCIBindDynamic(obind->base.hp.bnd, oci8_errhp, obind, in_bind_callback,
                              obind, out_bind_callback),
               &obind->base);
        break;
    }
}

static const oci8_bind_data_type_t bind_long_data_type = {
    {
        {
            "OCI8::BindType::Long",
            {
                NULL,
                oci8_handle_cleanup,
                oci8_handle_size,
            },
            &oci8_bind_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
            RUBY_TYPED_WB_PROTECTED,
#endif
        },
        bind_long_free,
        sizeof(oci8_bind_long_t)
    },
    bind_long_get,
    bind_long_set,
    bind_long_init,
    bind_long_init_elem,
    NULL,
    SQLT_CHR,
    bind_long_post_bind_hook,
};

static VALUE bind_long_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_long_data_type.base);
}

static const oci8_bind_data_type_t bind_long_raw_data_type = {
    {
        {
            "OCI8::BindType::LongRaw",
            {
                NULL,
                oci8_handle_cleanup,
                oci8_handle_size,
            },
            &oci8_bind_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
            RUBY_TYPED_WB_PROTECTED,
#endif
        },
        bind_long_free,
        sizeof(oci8_bind_long_t)
    },
    bind_long_get,
    bind_long_set,
    bind_long_init,
    bind_long_init_elem,
    NULL,
    SQLT_BIN,
    bind_long_post_bind_hook,
};

static VALUE bind_long_raw_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_long_raw_data_type.base);
}

static VALUE oci8_bind_get(VALUE self)
{
    oci8_bind_t *obind = TO_BIND(self);
    const oci8_bind_data_type_t *data_type = (const oci8_bind_data_type_t *)obind->base.data_type;
    ub4 idx = obind->curar_idx;
    void **null_structp = NULL;

    if (NIL_P(obind->tdo)) {
        if (obind->u.inds[idx] != 0)
            return Qnil;
    }
    return data_type->get(obind, (void*)((size_t)obind->valuep + obind->alloc_sz * idx), null_structp);
}

static VALUE oci8_bind_get_data(int argc, VALUE *argv, VALUE self)
{
    oci8_bind_t *obind = TO_BIND(self);
    VALUE index;

    rb_scan_args(argc, argv, "01", &index);
    if (!NIL_P(index)) {
        ub4 idx = NUM2UINT(index);
        if (idx >= obind->maxar_sz) {
            rb_raise(rb_eRuntimeError, "data index is too big. (%u for %u)", idx, obind->maxar_sz);
        }
        obind->curar_idx = idx;
        return rb_funcall(self, oci8_id_get, 0);
    } else if (obind->maxar_sz == 0) {
        obind->curar_idx = 0;
        return rb_funcall(self, oci8_id_get, 0);
    } else {
        volatile VALUE ary = rb_ary_new2(obind->curar_sz);
        ub4 idx;

        for (idx = 0; idx < obind->curar_sz; idx++) {
            obind->curar_idx = idx;
            rb_ary_store(ary, idx, rb_funcall(self, oci8_id_get, 0));
        }
        return ary;
    }
}

static VALUE oci8_bind_set(VALUE self, VALUE val)
{
    oci8_bind_t *obind = TO_BIND(self);
    const oci8_bind_data_type_t *data_type = (const oci8_bind_data_type_t *)obind->base.data_type;
    ub4 idx = obind->curar_idx;

    if (NIL_P(val)) {
        if (NIL_P(obind->tdo)) {
            obind->u.inds[idx] = -1;
        } else {
            *(OCIInd*)obind->u.null_structs[idx] = -1;
        }
    } else {
        void **null_structp = NULL;

        if (NIL_P(obind->tdo)) {
            null_structp = NULL;
            obind->u.inds[idx] = 0;
        } else {
            null_structp = &obind->u.null_structs[idx];
            *(OCIInd*)obind->u.null_structs[idx] = 0;
        }
        data_type->set(obind, (void*)((size_t)obind->valuep + obind->alloc_sz * idx), null_structp, val);
    }
    return self;
}

static VALUE oci8_bind_set_data(VALUE self, VALUE val)
{
    oci8_bind_t *obind = TO_BIND(self);

    if (obind->maxar_sz == 0) {
        obind->curar_idx = 0;
        rb_funcall(self, oci8_id_set, 1, val);
    } else {
        ub4 size;
        ub4 idx;
        Check_Type(val, T_ARRAY);

        size = RARRAY_LENINT(val);
        if (size > obind->maxar_sz) {
            rb_raise(rb_eRuntimeError, "over the max array size");
        }
        for (idx = 0; idx < size; idx++) {
            obind->curar_idx = idx;
            rb_funcall(self, oci8_id_set, 1, RARRAY_AREF(val, idx));
        }
        obind->curar_sz = size;
    }
    return self;
}

static VALUE get_initial_chunk_size(VALUE klass)
{
    return UINT2NUM(initial_chunk_size);
}

static VALUE set_initial_chunk_size(VALUE klass, VALUE arg)
{
    ub4 size = NUM2UINT(arg);
    if (size == 0) {
        rb_raise(rb_eArgError, "Could not set zero");
    }
    initial_chunk_size = size;
    return arg;
}

static VALUE get_max_chunk_size(VALUE klass)
{
    return UINT2NUM(max_chunk_size);
}

static VALUE set_max_chunk_size(VALUE klass, VALUE arg)
{
    ub4 size = NUM2UINT(arg);
    if (size == 0) {
        rb_raise(rb_eArgError, "Could not set zero");
    }
    max_chunk_size = size;
    return arg;
}

static VALUE oci8_bind_initialize(VALUE self, VALUE svc, VALUE val, VALUE length, VALUE max_array_size)
{
    oci8_bind_t *obind = TO_BIND(self);
    const oci8_bind_data_type_t *bind_class = (const oci8_bind_data_type_t *)obind->base.data_type;
    ub4 cnt = 1;

    obind->tdo = Qnil;
    obind->maxar_sz = NIL_P(max_array_size) ? 0 : NUM2UINT(max_array_size);
    obind->curar_sz = 0;
    if (obind->maxar_sz > 0)
        cnt = obind->maxar_sz;
    bind_class->init(obind, svc, val, length);
    if (obind->alloc_sz > 0) {
        obind->valuep = xmalloc(obind->alloc_sz * cnt);
        memset(obind->valuep, 0, obind->alloc_sz * cnt);
    } else {
        obind->valuep = NULL;
    }
    if (NIL_P(obind->tdo)) {
        obind->u.inds = xmalloc(sizeof(sb2) * cnt);
        memset(obind->u.inds, -1, sizeof(sb2) * cnt);
    } else {
        obind->u.null_structs = xmalloc(sizeof(void *) * cnt);
        memset(obind->u.null_structs, 0, sizeof(void *) * cnt);
    }
    if (bind_class->init_elem != NULL) {
        bind_class->init_elem(obind, svc);
    }
    if (!NIL_P(val)) {
        oci8_bind_set_data(self, val);
    }
    return Qnil;
}

void oci8_bind_free(oci8_base_t *base)
{
    oci8_bind_t *obind = (oci8_bind_t *)base;
    if (obind->valuep != NULL) {
        xfree(obind->valuep);
        obind->valuep = NULL;
    }
    if (obind->u.inds != NULL) {
        xfree(obind->u.inds);
        obind->u.inds = NULL;
    }
}

void oci8_bind_hp_obj_mark(oci8_base_t *base)
{
    oci8_bind_t *obind = (oci8_bind_t *)base;
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)obind->valuep;

    if (oho != NULL) {
        ub4 idx = 0;

        do {
            rb_gc_mark(oho[idx].obj);
        } while (++idx < obind->maxar_sz);
    }
}

void Init_oci8_bind(VALUE klass)
{
    cOCI8BindTypeBase = klass;
    id_bind_type = rb_intern("bind_type");
    id_charset_form = rb_intern("charset_form");
    sym_length = ID2SYM(rb_intern("length"));
    sym_length_semantics = ID2SYM(rb_intern("length_semantics"));
    sym_char = ID2SYM(rb_intern("char"));
    sym_nchar = ID2SYM(rb_intern("nchar"));

    rb_define_method(cOCI8BindTypeBase, "initialize", oci8_bind_initialize, 4);
    rb_define_method(cOCI8BindTypeBase, "get", oci8_bind_get, 0);
    rb_define_method(cOCI8BindTypeBase, "set", oci8_bind_set, 1);
    rb_define_private_method(cOCI8BindTypeBase, "get_data", oci8_bind_get_data, -1);
    rb_define_private_method(cOCI8BindTypeBase, "set_data", oci8_bind_set_data, 1);

    rb_define_singleton_method(klass, "initial_chunk_size", get_initial_chunk_size, 0);
    rb_define_singleton_method(klass, "initial_chunk_size=", set_initial_chunk_size, 1);
    rb_define_singleton_method(klass, "max_chunk_size", get_max_chunk_size, 0);
    rb_define_singleton_method(klass, "max_chunk_size=", set_max_chunk_size, 1);

    /* register primitive data types. */
    oci8_define_bind_class("String", &bind_string_data_type, bind_string_alloc);
    oci8_define_bind_class("RAW", &bind_raw_data_type, bind_raw_alloc);
    oci8_define_bind_class("BinaryDouble", &bind_binary_double_data_type, bind_binary_double_alloc);
    if (oracle_client_version >= ORAVER_12_1) {
        oci8_define_bind_class("Boolean", &bind_boolean_data_type, bind_boolean_alloc);
    }
    klass = oci8_define_bind_class("Long", &bind_long_data_type, bind_long_alloc);
    klass = oci8_define_bind_class("LongRaw", &bind_long_data_type, bind_long_raw_alloc);
}
