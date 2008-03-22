/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
#include "oci8.h"

static ID id_plus;
static ID id_dir_alias;
static ID id_filename;
static VALUE cOCI8LOB;
static VALUE cOCI8CLOB;
static VALUE cOCI8NCLOB;
static VALUE cOCI8BLOB;
static VALUE cOCI8BFILE;
static VALUE seek_set;
static VALUE seek_cur;
static VALUE seek_end;

enum state {
    S_NO_OPEN_CLOSE,
    S_OPEN,
    S_CLOSE,
    S_BFILE_CLOSE,
    S_BFILE_OPEN,
};
typedef struct {
    oci8_base_t base;
    VALUE svc;
    ub4 pos;
    int char_width;
    ub1 csfrm;
    enum state state;
} oci8_lob_t;

static VALUE oci8_lob_write(VALUE self, VALUE data);

static VALUE oci8_make_lob(VALUE klass, oci8_svcctx_t *svcctx, OCILobLocator *s)
{
    oci8_lob_t *lob;
    VALUE lob_obj;

    lob_obj = rb_funcall(klass, oci8_id_new, 1, svcctx->base.self);
    lob = DATA_PTR(lob_obj);
    /* If 's' is a temporary lob, use OCILobLocatorAssign instead. */
    oci_lc(OCILobAssign(oci8_envhp, oci8_errhp, s, &lob->base.hp.lob));
    return lob_obj;
}

VALUE oci8_make_clob(oci8_svcctx_t *svcctx, OCILobLocator *s)
{
    return oci8_make_lob(cOCI8CLOB, svcctx, s);
}

VALUE oci8_make_nclob(oci8_svcctx_t *svcctx, OCILobLocator *s)
{
    return oci8_make_lob(cOCI8NCLOB, svcctx, s);
}

VALUE oci8_make_blob(oci8_svcctx_t *svcctx, OCILobLocator *s)
{
    return oci8_make_lob(cOCI8BLOB, svcctx, s);
}

VALUE oci8_make_bfile(oci8_svcctx_t *svcctx, OCILobLocator *s)
{
    return oci8_make_lob(cOCI8BFILE, svcctx, s);
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
    oci8_svcctx_t *svcctx = oci8_get_svcctx(lob->svc);
    ub4 len;

    oci_lc(OCILobGetLength_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, &len));
    return len;
}

static void lob_open(oci8_lob_t *lob)
{
    if (lob->state == S_CLOSE) {
        if (have_OCILobOpen_nb) {
            oci8_svcctx_t *svcctx = oci8_get_svcctx(lob->svc);

            oci_lc(OCILobOpen_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, OCI_DEFAULT));
        }
        lob->state = S_OPEN;
    }
}

static void lob_close(oci8_lob_t *lob)
{
    if (lob->state == S_OPEN) {
        if (have_OCILobClose_nb) {
            oci8_svcctx_t *svcctx = oci8_get_svcctx(lob->svc);

            oci_lc(OCILobClose_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob));
        }
        lob->state = S_CLOSE;
    }
}

static void bfile_close(oci8_lob_t *lob)
{
    if (lob->state == S_BFILE_OPEN) {
        oci8_svcctx_t *svcctx = oci8_get_svcctx(lob->svc);

        oci_lc(OCILobFileClose_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob));
        lob->state = S_BFILE_CLOSE;
    }
}

static VALUE oci8_lob_close(VALUE self)
{
    oci8_lob_t *lob = DATA_PTR(self);
    lob_close(lob);
    oci8_base_free(DATA_PTR(self));
    return self;
}

static VALUE oci8_lob_do_initialize(int argc, VALUE *argv, VALUE self, ub1 csfrm, ub1 lobtype)
{
    oci8_lob_t *lob = DATA_PTR(self);
    VALUE svc;
    VALUE val;
    sword rv;

    rb_scan_args(argc, argv, "11", &svc, &val);
    TO_SVCCTX(svc); /* check argument type */
    rv = OCIDescriptorAlloc(oci8_envhp, &lob->base.hp.ptr, OCI_DTYPE_LOB, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    lob->base.type = OCI_DTYPE_LOB;
    lob->svc = svc;
    lob->pos = 0;
    lob->char_width = 1;
    lob->csfrm = csfrm;
    lob->state = S_NO_OPEN_CLOSE;
    oci8_link_to_parent((oci8_base_t*)lob, (oci8_base_t*)DATA_PTR(svc));
    if (!NIL_P(val)) {
        if (have_OCILobCreateTemporary_nb) {
            oci8_svcctx_t *svcctx = oci8_get_svcctx(svc);
            StringValue(val);
            oci_lc(OCILobCreateTemporary_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, 0, csfrm, lobtype, TRUE, OCI_DURATION_SESSION));
            oci8_lob_write(self, val);
        } else {
            rb_raise(rb_eRuntimeError, "creating a temporary lob is not supported on this Oracle version");
        }
    }
    return Qnil;
}

static VALUE oci8_clob_initialize(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_do_initialize(argc, argv, self, SQLCS_IMPLICIT, OCI_TEMP_CLOB);
    return Qnil;
}

static VALUE oci8_nclob_initialize(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_do_initialize(argc, argv, self, SQLCS_NCHAR, OCI_TEMP_CLOB);
    return Qnil;
}

static VALUE oci8_blob_initialize(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_do_initialize(argc, argv, self, SQLCS_IMPLICIT, OCI_TEMP_BLOB);
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

    oci_lc(OCILobLocatorIsInit(oci8_envhp, oci8_errhp, lob->base.hp.lob, &is_initialized));
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
                     FIX2INT(whence));
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
    oci8_svcctx_t *svcctx = oci8_get_svcctx(lob->svc);

    lob_open(lob);
    oci_lc(OCILobTrim_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, NUM2INT(len)));
    return self;
}

static VALUE oci8_lob_set_size(VALUE self, VALUE len)
{
    oci8_lob_truncate(self, len);
    return len;
}

static VALUE oci8_lob_read(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_t *lob = DATA_PTR(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(lob->svc);
    ub4 length;
    ub4 nchar;
    ub4 amt;
    sword rv;
    char buf[4096];
    size_t buf_size_in_char;
    VALUE size;
    VALUE v = Qnil;

    rb_scan_args(argc, argv, "01", &size);
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
        if (lob->state == S_BFILE_CLOSE) {
            rv = OCILobFileOpen_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, OCI_FILE_READONLY);
            if (rv == OCI_ERROR && oci8_get_error_code(oci8_errhp) == 22290) {
                /* ORA-22290: operation would exceed the maximum number of opened files or LOBs */
                /* close all opened BFILE implicitly. */
                oci8_base_t *base;
                for (base = &lob->base; base != &lob->base; base = base->next) {
                    if (base->type == OCI_DTYPE_LOB) {
                        oci8_lob_t *tmp = (oci8_lob_t *)base;
                        if (tmp->state == S_BFILE_OPEN) {
                            tmp->state = S_BFILE_CLOSE;
                        }
                    }
                }
                oci_lc(OCILobFileCloseAll_nb(svcctx, svcctx->base.hp.svc, oci8_errhp));
                continue;
            }
            if (rv != OCI_SUCCESS)
                oci8_raise(oci8_errhp, rv, NULL);
            lob->state = S_BFILE_OPEN;
        }
        rv = OCILobRead_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, &amt, lob->pos + 1, buf, sizeof(buf), NULL, NULL, 0, lob->csfrm);
        if (rv == OCI_ERROR && oci8_get_error_code(oci8_errhp) == 22289) {
            /* ORA-22289: cannot perform FILEREAD operation on an unopened file or LOB */
            if (lob->state == S_BFILE_CLOSE)
                continue;
        }
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
    if (nchar == length) {
        lob_close(lob);
        bfile_close(lob);
    }
    return v;
}

static VALUE oci8_lob_write(VALUE self, VALUE data)
{
    oci8_lob_t *lob = DATA_PTR(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(lob->svc);
    ub4 amt;

    lob_open(lob);
    StringValue(data);
    amt = RSTRING_LEN(data);
    oci_lc(OCILobWrite_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, &amt, lob->pos + 1, RSTRING_PTR(data), amt, OCI_ONE_PIECE, NULL, NULL, 0, lob->csfrm));
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
    oci8_lob_t *lob = DATA_PTR(self);
    lob_close(lob);
    return self;
}

static VALUE oci8_lob_get_chunk_size(VALUE self)
{
    if (have_OCILobGetChunkSize_nb) {
        oci8_lob_t *lob = DATA_PTR(self);
        oci8_svcctx_t *svcctx = oci8_get_svcctx(lob->svc);
        ub4 len;

        oci_lc(OCILobGetChunkSize_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, &len));
        return INT2FIX(len);
    } else {
        rb_notimplement();
    }
}

static VALUE oci8_lob_clone(VALUE self)
{
    oci8_lob_t *lob = DATA_PTR(self);
    oci8_lob_t *newlob;
    VALUE newobj;
    sword rv;
    boolean is_temporary;

    newobj = rb_funcall(CLASS_OF(self), oci8_id_new, 1, lob->svc);
    newlob = DATA_PTR(newobj);
    if (have_OCILobLocatorAssign_nb && have_OCILobIsTemporary
            && OCILobIsTemporary(oci8_envhp, oci8_errhp, lob->base.hp.lob, &is_temporary) == OCI_SUCCESS
            && is_temporary) {
        oci8_svcctx_t *svcctx = oci8_get_svcctx(lob->svc);
        rv = OCILobLocatorAssign_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, &newlob->base.hp.lob);
    } else {
        rv = OCILobAssign(oci8_envhp, oci8_errhp, lob->base.hp.lob, &newlob->base.hp.lob);
    }
    if (rv != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, rv, NULL);
    }
    return newobj;
}

static void oci8_bfile_get_name(VALUE self, VALUE *dir_alias_p, VALUE *filename_p)
{
    int need_get = 0;
    if (dir_alias_p != NULL) {
        *dir_alias_p = rb_ivar_get(self, id_dir_alias);
        if (NIL_P(*dir_alias_p))
            need_get = 1;
    }
    if (filename_p != NULL) {
        *filename_p = rb_ivar_get(self, id_filename);
        if (NIL_P(*filename_p))
            need_get = 1;
    }
    if (need_get) {
        oci8_lob_t *lob = DATA_PTR(self);
        char d_buf[31];
        ub2 d_length = sizeof(d_buf);
        char f_buf[256];
        ub2 f_length = sizeof(f_buf);
        VALUE dir_alias;
        VALUE filename;

        oci_lc(OCILobFileGetName(oci8_envhp, oci8_errhp, lob->base.hp.lob, TO_ORATEXT(d_buf), &d_length, TO_ORATEXT(f_buf), &f_length));
        dir_alias = rb_str_new(d_buf, d_length);
        filename = rb_str_new(f_buf, f_length);
        rb_ivar_set(self, id_dir_alias, dir_alias);
        rb_ivar_set(self, id_filename, filename);
        if (dir_alias_p != NULL) {
            *dir_alias_p = dir_alias;
        }
        if (filename_p != NULL) {
            *filename_p = filename;
        }
    }
}

static void oci8_bfile_set_name(VALUE self, VALUE dir_alias, VALUE filename)
{
    oci8_lob_t *lob = DATA_PTR(self);

    bfile_close(lob);
    oci_lc(OCILobFileSetName(oci8_envhp, oci8_errhp, &lob->base.hp.lob,
                             RSTRING_ORATEXT(dir_alias), RSTRING_LEN(dir_alias),
                             RSTRING_ORATEXT(filename), RSTRING_LEN(filename)));
}

static VALUE oci8_bfile_initialize(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_t *lob = DATA_PTR(self);
    VALUE svc;
    VALUE dir_alias;
    VALUE filename;

    rb_scan_args(argc, argv, "12", &svc, &dir_alias, &filename);
    TO_SVCCTX(svc); /* check argument type */
    oci_lc(OCIDescriptorAlloc(oci8_envhp, &lob->base.hp.ptr, OCI_DTYPE_LOB, 0, NULL));
    lob->base.type = OCI_DTYPE_LOB;
    lob->svc = svc;
    lob->pos = 0;
    lob->char_width = 1;
    lob->csfrm = SQLCS_IMPLICIT;
    lob->state = S_BFILE_CLOSE;
    if (argc != 1) {
        StringValue(dir_alias);
        StringValue(filename);
        oci8_bfile_set_name(self, dir_alias, filename);
    }
    oci8_link_to_parent((oci8_base_t*)lob, (oci8_base_t*)DATA_PTR(svc));
    return Qnil;
}

static VALUE oci8_bfile_get_dir_alias(VALUE self)
{
    VALUE dir_alias;

    oci8_bfile_get_name(self, &dir_alias, NULL);
    return dir_alias;
}

static VALUE oci8_bfile_get_filename(VALUE self)
{
    VALUE filename;

    oci8_bfile_get_name(self, NULL, &filename);
    return filename;
}

static VALUE oci8_bfile_set_dir_alias(VALUE self, VALUE dir_alias)
{
    VALUE filename;

    StringValue(dir_alias);
    oci8_bfile_get_name(self, NULL, &filename);
    oci8_bfile_set_name(self, dir_alias, filename);
    rb_ivar_set(self, id_dir_alias, dir_alias);
    return dir_alias;
}

static VALUE oci8_bfile_set_filename(VALUE self, VALUE filename)
{
    VALUE dir_alias;

    StringValue(filename);
    oci8_bfile_get_name(self, &dir_alias, NULL);
    oci8_bfile_set_name(self, dir_alias, filename);
    rb_ivar_set(self, id_filename, filename);
    return filename;
}

static VALUE oci8_bfile_exists_p(VALUE self)
{
    oci8_lob_t *lob = DATA_PTR(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(lob->svc);
    boolean flag;

    oci_lc(OCILobFileExists_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, &flag));
    return flag ? Qtrue : Qfalse;
}

static VALUE oci8_bfile_error(VALUE self, VALUE dummy)
{
    rb_raise(rb_eRuntimeError, "cannot modify a read-only BFILE object");
}

/*
 * bind_clob/blob/bfile
 */

typedef struct {
    oci8_bind_class_t bind;
    VALUE *klass;
} oci8_bind_lob_class_t;

static VALUE bind_lob_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)data;
    return oci8_lob_clone(oho->obj);
}

static void bind_lob_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)data;
    const oci8_bind_lob_class_t *klass = (const oci8_bind_lob_class_t *)obind->base.klass;
    oci8_base_t *h;
    if (!rb_obj_is_kind_of(val, *klass->klass))
        rb_raise(rb_eArgError, "Invalid argument: %s (expect %s)", rb_class2name(CLASS_OF(val)), rb_class2name(*klass->klass));
    h = DATA_PTR(val);
    oho->hp = h->hp.ptr;
    oho->obj = val;
}

static void bind_lob_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length)
{
    obind->value_sz = sizeof(void *);
    obind->alloc_sz = sizeof(oci8_hp_obj_t);
}

static void bind_lob_init_elem(oci8_bind_t *obind, VALUE svc)
{
    const oci8_bind_lob_class_t *klass = (const oci8_bind_lob_class_t *)obind->base.klass;
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)obind->valuep;
    oci8_base_t *h;
    ub4 idx = 0;

    do {
        oho[idx].obj = rb_funcall(*klass->klass, oci8_id_new, 1, svc);
        h = DATA_PTR(oho[idx].obj);
        oho[idx].hp = h->hp.ptr;
    } while (++idx < obind->maxar_sz);
}

static const oci8_bind_lob_class_t bind_clob_class = {
    {
        {
            oci8_bind_hp_obj_mark,
            oci8_bind_free,
            sizeof(oci8_bind_t)
        },
        bind_lob_get,
        bind_lob_set,
        bind_lob_init,
        bind_lob_init_elem,
        NULL,
        NULL,
        NULL,
        SQLT_CLOB
    },
    &cOCI8CLOB
};

static const oci8_bind_lob_class_t bind_nclob_class = {
    {
        {
            oci8_bind_hp_obj_mark,
            oci8_bind_free,
            sizeof(oci8_bind_t)
        },
        bind_lob_get,
        bind_lob_set,
        bind_lob_init,
        bind_lob_init_elem,
        NULL,
        NULL,
        NULL,
        SQLT_CLOB,
        SQLCS_NCHAR,
    },
    &cOCI8NCLOB
};

static const oci8_bind_lob_class_t bind_blob_class = {
    {
        {
            oci8_bind_hp_obj_mark,
            oci8_bind_free,
            sizeof(oci8_bind_t)
        },
        bind_lob_get,
        bind_lob_set,
        bind_lob_init,
        bind_lob_init_elem,
        NULL,
        NULL,
        NULL,
        SQLT_BLOB
    },
    &cOCI8BLOB
};

static const oci8_bind_lob_class_t bind_bfile_class = {
    {
        {
            oci8_bind_hp_obj_mark,
            oci8_bind_free,
            sizeof(oci8_bind_t)
        },
        bind_lob_get,
        bind_lob_set,
        bind_lob_init,
        bind_lob_init_elem,
        NULL,
        NULL,
        NULL,
        SQLT_BFILE
    },
    &cOCI8BFILE
};

void Init_oci8_lob(VALUE cOCI8)
{
    id_plus = rb_intern("+");
    id_dir_alias = rb_intern("@dir_alias");
    id_filename = rb_intern("@filename");
    seek_set = rb_eval_string("::IO::SEEK_SET");
    seek_cur = rb_eval_string("::IO::SEEK_CUR");
    seek_end = rb_eval_string("::IO::SEEK_END");

    cOCI8LOB = oci8_define_class_under(cOCI8, "LOB", &oci8_lob_class);
    cOCI8CLOB = rb_define_class_under(cOCI8, "CLOB", cOCI8LOB);
    cOCI8NCLOB = rb_define_class_under(cOCI8, "NCLOB", cOCI8LOB);
    cOCI8BLOB = rb_define_class_under(cOCI8, "BLOB", cOCI8LOB);
    cOCI8BFILE = rb_define_class_under(cOCI8, "BFILE", cOCI8LOB);

    rb_define_method(cOCI8CLOB, "initialize", oci8_clob_initialize, -1);
    rb_define_method(cOCI8NCLOB, "initialize", oci8_nclob_initialize, -1);
    rb_define_method(cOCI8BLOB, "initialize", oci8_blob_initialize, -1);
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
    rb_define_method(cOCI8LOB, "read", oci8_lob_read, -1);
    rb_define_method(cOCI8LOB, "write", oci8_lob_write, 1);
    rb_define_method(cOCI8LOB, "close", oci8_lob_close, 0);
    rb_define_method(cOCI8LOB, "sync", oci8_lob_get_sync, 0);
    rb_define_method(cOCI8LOB, "sync=", oci8_lob_set_sync, 1);
    rb_define_method(cOCI8LOB, "flush", oci8_lob_flush, 0);
    rb_define_method(cOCI8LOB, "chunk_size", oci8_lob_get_chunk_size, 0);

    rb_define_method(cOCI8BFILE, "initialize", oci8_bfile_initialize, -1);
    rb_define_method(cOCI8BFILE, "dir_alias", oci8_bfile_get_dir_alias, 0);
    rb_define_method(cOCI8BFILE, "filename", oci8_bfile_get_filename, 0);
    rb_define_method(cOCI8BFILE, "dir_alias=", oci8_bfile_set_dir_alias, 1);
    rb_define_method(cOCI8BFILE, "filename=", oci8_bfile_set_filename, 1);
    rb_define_method(cOCI8BFILE, "exists?", oci8_bfile_exists_p, 0);
    rb_define_method(cOCI8BFILE, "truncate", oci8_bfile_error, 1);
    rb_define_method(cOCI8BFILE, "size=", oci8_bfile_error, 1);
    rb_define_method(cOCI8BFILE, "write", oci8_bfile_error, 1);

    oci8_define_bind_class("CLOB", &bind_clob_class.bind);
    oci8_define_bind_class("NCLOB", &bind_nclob_class.bind);
    oci8_define_bind_class("BLOB", &bind_blob_class.bind);
    oci8_define_bind_class("BFILE", &bind_bfile_class.bind);
}
