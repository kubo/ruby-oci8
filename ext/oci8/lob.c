/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
#include "oci8.h"

static ID id_plus;
static VALUE cOCI8LOB;
static VALUE cOCI8BLOB;
static VALUE cOCI8CLOB;
static VALUE seek_set;
static VALUE seek_cur;
static VALUE seek_end;

enum state {S_NO_OPEN_CLOSE, S_OPEN, S_CLOSE};
typedef struct {
    oci8_base_t base;
    VALUE svc;
    ub4 pos;
    int char_width;
    enum state state;
} oci8_lob_t;

static VALUE oci8_make_lob(VALUE klass, oci8_svcctx_t *svcctx, OCILobLocator *s)
{
    oci8_lob_t *lob;
    VALUE lob_obj;
    sword rv;

    lob_obj = rb_funcall(klass, oci8_id_new, 1, svcctx->base.self);
    lob = DATA_PTR(lob_obj);
#ifdef HAVE_OCILOBLOCATORASSIGN
    /* Oracle 8.1 or upper */
    rv = OCILobLocatorAssign(svcctx->base.hp, oci8_errhp, s, (OCILobLocator**)&lob->base.hp);
#else
    /* Oracle 8.0 */
    rv = OCILobAssign(oci8_envhp, oci8_errhp, s, (OCILobLocator**)&lob->base.hp);
#endif
    if (rv != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, rv, NULL);
    }
    return lob_obj;
}

VALUE oci8_make_clob(oci8_svcctx_t *svcctx, OCILobLocator *s)
{
    return oci8_make_lob(cOCI8CLOB, svcctx, s);
}

VALUE oci8_make_blob(oci8_svcctx_t *svcctx, OCILobLocator *s)
{
    return oci8_make_lob(cOCI8BLOB, svcctx, s);
}

static void oci8_lob_mark(oci8_base_t *base)
{
    oci8_lob_t *lob = (oci8_lob_t *)base;
    rb_gc_mark(lob->svc);
}

static void oci8_lob_free(oci8_base_t *base)
{
    oci8_lob_t *lob = (oci8_lob_t *)base;
    lob->svc = Qnil;
}

static oci8_base_class_t oci8_lob_class = {
    oci8_lob_mark,
    oci8_lob_free,
    sizeof(oci8_lob_t),
};

static ub4 oci8_lob_get_length(oci8_lob_t *lob)
{
    ub4 len;

    oci_lc(OCILobGetLength(TO_SVCCTX(lob->svc), oci8_errhp, lob->base.hp, &len));
    return len;
}

static void lob_open(oci8_lob_t *lob)
{
#ifdef HAVE_OCILOBOPEN
    if (lob->state == S_CLOSE) {
        sword rv;

        rv = OCILobOpen(TO_SVCCTX(lob->svc), oci8_errhp, lob->base.hp, OCI_DEFAULT);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        lob->state = S_OPEN;
    }
#endif
}

static void lob_close(oci8_lob_t *lob)
{
#ifdef HAVE_OCILOBCLOSE
    if (lob->state == S_OPEN) {
        sword rv;

        rv = OCILobClose(TO_SVCCTX(lob->svc), oci8_errhp, lob->base.hp);
        if (rv != OCI_SUCCESS)
            oci8_raise(oci8_errhp, rv, NULL);
        lob->state = S_CLOSE;
    }
#endif
}

static VALUE oci8_lob_close(VALUE self)
{
    oci8_lob_t *lob = DATA_PTR(self);
    lob_close(lob);
    oci8_base_free(DATA_PTR(self));
    return self;
}

static VALUE oci8_lob_initialize(VALUE self, VALUE svc)
{
    oci8_lob_t *lob = DATA_PTR(self);
    sword rv;

    TO_SVCCTX(svc); /* check argument type */
    rv = OCIDescriptorAlloc(oci8_envhp, &lob->base.hp, OCI_DTYPE_LOB, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    lob->base.type = OCI_DTYPE_LOB;
    lob->svc = svc;
    lob->pos = 0;
    lob->char_width = 1;
    lob->state = S_NO_OPEN_CLOSE;
    return Qnil;
}

static VALUE oci8_lob_set_char_width(VALUE self, VALUE vsize)
{
    oci8_lob_t *lob = DATA_PTR(self);
    int size;

    size = NUM2INT(vsize); /* 1 */

    if (size <= 0)
        rb_raise(rb_eArgError, "size must be more than one.");
    lob->char_width = size;
    return vsize;
}

static VALUE oci8_lob_available_p(VALUE self)
{
    oci8_lob_t *lob = DATA_PTR(self);
    boolean is_initialized;
    sword rv;

    rv = OCILobLocatorIsInit(oci8_envhp, oci8_errhp, lob->base.hp, &is_initialized);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return is_initialized ? Qtrue : Qfalse;
}

static VALUE oci8_lob_get_size(VALUE self)
{
    return UB4_TO_NUM(oci8_lob_get_length(DATA_PTR(self)));
}

static VALUE oci8_lob_get_pos(VALUE self)
{
    oci8_lob_t *lob = DATA_PTR(self);
    return UB4_TO_NUM(lob->pos);
}

static VALUE oci8_lob_eof_p(VALUE self)
{
    oci8_lob_t *lob = DATA_PTR(self);
    if (oci8_lob_get_length(lob) < lob->pos)
        return Qfalse;
    else
        return Qtrue;
}

static VALUE oci8_lob_seek(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_t *lob = DATA_PTR(self);
    VALUE position, whence;

    rb_scan_args(argc, argv, "11", &position, &whence);
    if (argc == 2 && (whence != seek_set && whence != seek_cur && whence != seek_end)) {
        if (FIXNUM_P(whence)) {
            rb_raise(rb_eArgError, "expect IO::SEEK_SET, IO::SEEK_CUR or IO::SEEK_END but %d",
                     INT2FIX(whence));
        } else {
            rb_raise(rb_eArgError, "expect IO::SEEK_SET, IO::SEEK_CUR or IO::SEEK_END but %s",
                     rb_class2name(CLASS_OF(whence)));
        }
    }
    if (whence == seek_cur) {
        position = rb_funcall(UB4_TO_NUM(lob->pos), id_plus, 1, position);
    } else if (whence == seek_end) {
        position = rb_funcall(UB4_TO_NUM(oci8_lob_get_length(lob)), id_plus, 1, position);
    }
    lob->pos = NUM2UINT(position);
    return self;
}

static VALUE oci8_lob_rewind(VALUE self)
{
    oci8_lob_t *lob = DATA_PTR(self);
    lob->pos = 0;
    return self;
}

static VALUE oci8_lob_truncate(VALUE self, VALUE len)
{
    oci8_lob_t *lob = DATA_PTR(self);
    sword rv;

    lob_open(lob);
    rv = OCILobTrim(TO_SVCCTX(lob->svc), oci8_errhp, lob->base.hp, NUM2INT(len));
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return self;
}

static VALUE oci8_lob_set_size(VALUE self, VALUE len)
{
    oci8_lob_truncate(self, len);
    return len;
}

static VALUE oci8_lob_read(VALUE self, VALUE size)
{
    oci8_lob_t *lob = DATA_PTR(self);
    ub4 length;
    ub4 nchar;
    ub4 amt;
    sword rv;
    char buf[4096];
    size_t buf_size_in_char;
    VALUE v = Qnil;

    length = oci8_lob_get_length(lob);
    if (length <= lob->pos) /* EOF */
        return Qnil;
    length -= lob->pos;
    if (NIL_P(size)) {
        nchar = length; /* read until EOF */
    } else {
        nchar = NUM2UINT(size);
        if (nchar > length)
            nchar = length;
    }
    amt = nchar;
    buf_size_in_char = sizeof(buf) / lob->char_width;
    do {
        rv = OCILobRead(TO_SVCCTX(lob->svc), oci8_errhp, lob->base.hp, &amt, lob->pos + 1, buf, sizeof(buf), NULL, NULL, 0, SQLCS_IMPLICIT);
        if (rv != OCI_SUCCESS && rv != OCI_NEED_DATA)
            oci8_raise(oci8_errhp, rv, NULL);
        if (amt == 0)
            break;
        /* for fixed size charset, amt is the number of characters stored in buf. */
        if (amt > buf_size_in_char)
            rb_raise(eOCIException, "Too large buffer fetched or you set too large size of a character.");
        amt *= lob->char_width;
        if (v == Qnil)
            v = rb_str_new(buf, amt);
        else
            v = rb_str_cat(v, buf, amt);
    } while (rv == OCI_NEED_DATA);
    lob->pos += nchar;
    return v;
}

static VALUE oci8_lob_write(VALUE self, VALUE data)
{
    oci8_lob_t *lob = DATA_PTR(self);
    ub4 amt;
    sword rv;

    lob_open(lob);
    StringValue(data);
    amt = RSTRING_LEN(data);
    rv = OCILobWrite(TO_SVCCTX(lob->svc), oci8_errhp, lob->base.hp, &amt, lob->pos + 1, RSTRING_PTR(data), amt, OCI_ONE_PIECE, NULL, NULL, 0, SQLCS_IMPLICIT);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    lob->pos += amt;
    return INT2FIX(amt);
}

static VALUE oci8_lob_get_sync(VALUE self)
{
    oci8_lob_t *lob = DATA_PTR(self);
    return (lob->state == S_NO_OPEN_CLOSE) ? Qtrue : Qfalse;
}

static VALUE oci8_lob_set_sync(VALUE self, VALUE b)
{
    oci8_lob_t *lob = DATA_PTR(self);
    if (RTEST(b)) {
        lob_close(lob);
        lob->state = S_NO_OPEN_CLOSE;
    } else {
        if (lob->state == S_NO_OPEN_CLOSE)
            lob->state = S_CLOSE;
    }
    return b;
}

static VALUE oci8_lob_flush(VALUE self)
{
#ifdef HAVE_OCILOBCLOSE
    oci8_lob_t *lob = DATA_PTR(self);
    lob_close(lob);
    return self;
#else
    rb_notimplement();
#endif
}

static VALUE oci8_lob_get_chunk_size(VALUE self)
{
#ifdef HAVE_OCILOBGETCHUNKSIZE
    oci8_lob_t *lob = DATA_PTR(self);
    ub4 len;
    sword rv;

    rv = OCILobGetChunkSize(TO_SVCCTX(lob->svc), oci8_errhp, lob->base.hp, &len);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return INT2FIX(len);
#else
    rb_notimplement();
#endif
}

static VALUE oci8_lob_clone(VALUE self)
{
    oci8_lob_t *lob = DATA_PTR(self);
    oci8_lob_t *newlob;
    VALUE newobj;
    sword rv;

    newobj = rb_funcall(CLASS_OF(self), oci8_id_new, 1, lob->svc);
    newlob = DATA_PTR(newobj);
#ifdef HAVE_OCILOBLOCATORASSIGN
    /* Oracle 8.1 or upper */
    rv = OCILobLocatorAssign(TO_SVCCTX(lob->svc), oci8_errhp, lob->base.hp, (OCILobLocator**)&newlob->base.hp);
#else
    /* Oracle 8.0 */
    rv = OCILobAssign(oci8_envhp, oci8_errhp, lob->base.hp, (OCILobLocator**)&newlob->base.hp);
#endif
    if (rv != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, rv, NULL);
    }
    return newobj;
}

/*
 * bind_clob/blob
 */

typedef struct {
    oci8_bind_class_t bind;
    VALUE *klass;
} oci8_bind_lob_class_t;

static VALUE bind_lob_get(oci8_bind_t *base)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)base->valuep;
    return oci8_lob_clone(oho->obj);
}

static void bind_lob_set(oci8_bind_t *base, VALUE val)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)base->valuep;
    oci8_bind_lob_class_t *klass = (oci8_bind_lob_class_t *)base->base.klass;
    oci8_base_t *h;
    if (!rb_obj_is_kind_of(val, *klass->klass))
        rb_raise(rb_eArgError, "Invalid argument: %s (expect %s)", rb_class2name(CLASS_OF(val)), rb_class2name(*klass->klass));
    h = DATA_PTR(val);
    oho->hp = h->hp;
    oho->obj = val;
}

static void bind_lob_init(oci8_bind_t *base, VALUE svc, VALUE *val, VALUE length)
{
    oci8_bind_lob_class_t *klass = (oci8_bind_lob_class_t *)base->base.klass;

    base->value_sz = sizeof(void *);
    base->alloc_sz = sizeof(oci8_hp_obj_t);
    if (NIL_P(*val)) {
        *val = rb_funcall(*klass->klass, oci8_id_new, 1, svc);
    }
}

static oci8_bind_lob_class_t bind_clob_class = {
    {
        {
            oci8_bind_hp_obj_mark,
            oci8_bind_free,
            sizeof(oci8_bind_t)
        },
        bind_lob_get,
        bind_lob_set,
        bind_lob_init,
        NULL,
        NULL,
        NULL,
        SQLT_CLOB
    },
    &cOCI8CLOB
};

static oci8_bind_lob_class_t bind_blob_class = {
    {
        {
            oci8_bind_hp_obj_mark,
            oci8_bind_free,
            sizeof(oci8_bind_t)
        },
        bind_lob_get,
        bind_lob_set,
        bind_lob_init,
        NULL,
        NULL,
        NULL,
        SQLT_BLOB
    },
    &cOCI8BLOB
};

void Init_oci8_lob(VALUE cOCI8)
{
    id_plus = rb_intern("+");
    seek_set = rb_eval_string("::IO::SEEK_SET");
    seek_cur = rb_eval_string("::IO::SEEK_CUR");
    seek_end = rb_eval_string("::IO::SEEK_END");

    cOCI8LOB = oci8_define_class_under(cOCI8, "LOB", &oci8_lob_class);
    cOCI8BLOB = rb_define_class_under(cOCI8, "BLOB", cOCI8LOB);
    cOCI8CLOB = rb_define_class_under(cOCI8, "CLOB", cOCI8LOB);
    rb_define_method(cOCI8LOB, "initialize", oci8_lob_initialize, 1);
    rb_define_private_method(cOCI8LOB, "__char_width=", oci8_lob_set_char_width, 1);
    rb_define_method(cOCI8LOB, "available?", oci8_lob_available_p, 0);
    rb_define_method(cOCI8LOB, "size", oci8_lob_get_size, 0);
    rb_define_method(cOCI8LOB, "pos", oci8_lob_get_pos, 0);
    rb_define_alias(cOCI8LOB, "tell", "pos");
    rb_define_method(cOCI8LOB, "eof?", oci8_lob_eof_p, 0);
    rb_define_method(cOCI8LOB, "seek", oci8_lob_seek, -1);
    rb_define_method(cOCI8LOB, "rewind", oci8_lob_rewind, 0);
    rb_define_method(cOCI8LOB, "truncate", oci8_lob_truncate, 1);
    rb_define_method(cOCI8LOB, "size=", oci8_lob_set_size, 1);
    rb_define_method(cOCI8LOB, "read", oci8_lob_read, 1);
    rb_define_method(cOCI8LOB, "write", oci8_lob_write, 1);
    rb_define_method(cOCI8LOB, "close", oci8_lob_close, 0);
    rb_define_method(cOCI8LOB, "sync", oci8_lob_get_sync, 0);
    rb_define_method(cOCI8LOB, "sync=", oci8_lob_set_sync, 1);
    rb_define_method(cOCI8LOB, "flush", oci8_lob_flush, 0);
    rb_define_method(cOCI8LOB, "chunk_size", oci8_lob_get_chunk_size, 0);

    oci8_define_bind_class("CLOB", &bind_clob_class.bind);
    oci8_define_bind_class("BLOB", &bind_blob_class.bind);
}
