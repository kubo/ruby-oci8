/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * lob.c - part of ruby-oci8
 *
 * Copyright (C) 2002-2015 Kubo Takehiro <kubo@jiubao.org>
 */
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

#define TO_LOB(obj) ((oci8_lob_t *)oci8_check_typeddata((obj), &oci8_lob_data_type, 1))

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

enum state {
    S_NO_OPEN_CLOSE,
    S_BFILE_CLOSE,
    S_BFILE_OPEN,
};
typedef struct {
    oci8_base_t base;
    oci8_svcctx_t *svcctx;
    ub8 pos;
    ub1 csfrm;
    ub1 lobtype;
    enum state state;
} oci8_lob_t;

static oci8_svcctx_t *check_svcctx(oci8_lob_t *lob)
{
    oci8_svcctx_t *svcctx = lob->svcctx;
    if (svcctx == NULL || svcctx->base.type != OCI_HTYPE_SVCCTX) {
        rb_raise(rb_eRuntimeError, "Invalid Svcctx");
    }
    return svcctx;
}

static VALUE oci8_lob_write(VALUE self, VALUE data);

static void oci8_lob_mark(oci8_base_t *base)
{
    oci8_lob_t *lob = (oci8_lob_t *)base;
    if (lob->svcctx != NULL) {
        rb_gc_mark(lob->svcctx->base.self);
    }
}

static void oci8_lob_free(oci8_base_t *base)
{
    oci8_lob_t *lob = (oci8_lob_t *)base;
    boolean is_temporary;
    oci8_svcctx_t *svcctx = lob->svcctx;

    if (svcctx != NULL
        && OCILobIsTemporary(oci8_envhp, oci8_errhp, lob->base.hp.lob, &is_temporary) == OCI_SUCCESS
        && is_temporary) {
        oci8_temp_lob_t *temp_lob = ALLOC(oci8_temp_lob_t);

        temp_lob->next = svcctx->temp_lobs;
        temp_lob->lob = lob->base.hp.lob;
        svcctx->temp_lobs = temp_lob;
        lob->base.type = 0;
        lob->base.closed = 1;
        lob->base.hp.ptr = NULL;
    }
    lob->svcctx = NULL;
}

static const oci8_handle_data_type_t oci8_lob_data_type = {
    {
        "OCI8::LOB",
        {
            NULL,
            NULL,
            NULL,
        },
        &oci8_handle_data_type.rb_data_type, NULL,
    },
    NULL,
    sizeof(oci8_lob_t),
};

static VALUE oci8_lob_alloc(VALUE klass)
{
    rb_raise(rb_eNameError, "private method `new' called for %s:Class", rb_class2name(klass));
}

static const oci8_handle_data_type_t oci8_clob_data_type = {
    {
        "OCI8::CLOB",
        {
            (RUBY_DATA_FUNC)oci8_lob_mark,
            oci8_handle_cleanup,
            oci8_handle_size,
        },
        &oci8_lob_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
        RUBY_TYPED_WB_PROTECTED,
#endif
    },
    oci8_lob_free,
    sizeof(oci8_lob_t),
};

static VALUE oci8_clob_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &oci8_clob_data_type);
}

static const oci8_handle_data_type_t oci8_nclob_data_type = {
    {
        "OCI8::NCLOB",
        {
            (RUBY_DATA_FUNC)oci8_lob_mark,
            oci8_handle_cleanup,
            oci8_handle_size,
        },
        &oci8_lob_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
        RUBY_TYPED_WB_PROTECTED,
#endif
    },
    oci8_lob_free,
    sizeof(oci8_lob_t),
};

static VALUE oci8_nclob_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &oci8_nclob_data_type);
}

static const oci8_handle_data_type_t oci8_blob_data_type = {
    {
        "OCI8::BLOB",
        {
            (RUBY_DATA_FUNC)oci8_lob_mark,
            oci8_handle_cleanup,
            oci8_handle_size,
        },
        &oci8_lob_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
        RUBY_TYPED_WB_PROTECTED,
#endif
    },
    oci8_lob_free,
    sizeof(oci8_lob_t),
};

static VALUE oci8_blob_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &oci8_blob_data_type);
}

static const oci8_handle_data_type_t oci8_bfile_data_type = {
    {
        "OCI8::BFILE",
        {
            (RUBY_DATA_FUNC)oci8_lob_mark,
            oci8_handle_cleanup,
            oci8_handle_size,
        },
        &oci8_lob_data_type.rb_data_type, NULL,
#ifdef RUBY_TYPED_WB_PROTECTED
        RUBY_TYPED_WB_PROTECTED,
#endif
    },
    oci8_lob_free,
    sizeof(oci8_lob_t),
};

static VALUE oci8_bfile_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &oci8_bfile_data_type);
}

static VALUE oci8_make_lob(VALUE klass, oci8_svcctx_t *svcctx, OCILobLocator *s)
{
    oci8_lob_t *lob;
    boolean is_temp;
    VALUE lob_obj;

    lob_obj = rb_class_new_instance(1, &svcctx->base.self, klass);
    lob = TO_LOB(lob_obj);
    /* If 's' is a temporary lob, use OCILobLocatorAssign instead. */
    chker2(OCILobIsTemporary(oci8_envhp, oci8_errhp, s, &is_temp), &svcctx->base);
    if (is_temp)
        chker2(OCILobLocatorAssign_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, s, &lob->base.hp.lob),
               &svcctx->base);
    else
        chker2(OCILobAssign(oci8_envhp, oci8_errhp, s, &lob->base.hp.lob),
               &svcctx->base);
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

static void oci8_assign_lob(VALUE klass, oci8_svcctx_t *svcctx, VALUE lob, OCILobLocator **dest)
{
    oci8_base_t *base = oci8_check_typeddata(lob, &oci8_lob_data_type, 1);
    if (!rb_obj_is_kind_of(lob, klass)) {
        rb_raise(rb_eTypeError, "wrong argument %s (expect %s)",
                 rb_obj_classname(lob), rb_class2name(klass));
    }
    chker2(OCILobLocatorAssign_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, base->hp.lob, dest), base);
}

void oci8_assign_clob(oci8_svcctx_t *svcctx, VALUE lob, OCILobLocator **dest)
{
    oci8_assign_lob(cOCI8CLOB, svcctx, lob, dest);
}

void oci8_assign_nclob(oci8_svcctx_t *svcctx, VALUE lob, OCILobLocator **dest)
{
    oci8_assign_lob(cOCI8NCLOB, svcctx, lob, dest);
}

void oci8_assign_blob(oci8_svcctx_t *svcctx, VALUE lob, OCILobLocator **dest)
{
    oci8_assign_lob(cOCI8BLOB, svcctx, lob, dest);
}

void oci8_assign_bfile(oci8_svcctx_t *svcctx, VALUE lob, OCILobLocator **dest)
{
    oci8_assign_lob(cOCI8BFILE, svcctx, lob, dest);
}

static ub8 oci8_lob_get_length(oci8_lob_t *lob)
{
    oci8_svcctx_t *svcctx = check_svcctx(lob);
    ub8 len;

    chker2(OCILobGetLength2_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, &len),
           &svcctx->base);
    return len;
}

static void bfile_close(oci8_lob_t *lob)
{
    if (lob->state == S_BFILE_OPEN) {
        oci8_svcctx_t *svcctx = check_svcctx(lob);

        chker2(OCILobFileClose_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob),
               &svcctx->base);
        lob->state = S_BFILE_CLOSE;
    }
}

/*
 *  Document-class: OCI8::LOB
 *
 *  OCI8::LOB is an I/O object. LOB contents are read from and written
 *  to the Oracle server via an associating connection. When a connection
 *  is closed, associating LOBs are also closed.
 *
 *  This is the abstract base class of large-object data types; {BFILE}, {BLOB}, {CLOB} and {NCLOB}.
 *
 */

/*
 *  Document-class: OCI8::CLOB
 *
 *  This class is a ruby-side class of {Oracle NCLOB datatype}[http://docs.oracle.com/database/121/SQLRF/sql_elements001.htm#sthref175].
 */

/*
 *  Document-class: OCI8::NCLOB
 *
 *  This class is a ruby-side class of {Oracle CLOB datatype}[http://docs.oracle.com/database/121/SQLRF/sql_elements001.htm#sthref172].
 */

/*
 *  Document-class: OCI8::BLOB
 *
 *  This class is a ruby-side class of {Oracle BLOB datatype}[http://docs.oracle.com/database/121/SQLRF/sql_elements001.htm#sthref168].
 *
 */

/*
 *  Document-class: OCI8::BFILE
 *
 *  This class is a ruby-side class of {Oracle BFILE datatype}[http://docs.oracle.com/database/121/SQLRF/sql_elements001.htm#sthref164].
 *  It is a read-only {LOB}. You cannot change the contents.
 *
 *  You can read files on the server-side as follows:
 *
 *  1. Connect to the Oracle server as a user who has CREATE DIRECTORY privilege.
 *
 *       # create a directory object on the Oracle server.
 *       CREATE DIRECTORY file_storage_dir AS '/opt/file_storage';
 *       # grant a privilege to read files on file_storage_dir directory to a user.
 *       GRANT READ ON DIRECTORY file_storage_dir TO username;
 *
 *  2. Create a file 'hello_world.txt' in the directory '/opt/file_storage' on the server filesystem.
 *
 *       echo 'Hello World!' > /opt/file_storage/hello_world.txt
 *
 *  3. Read the file by ruby-oci8.
 *
 *       require 'oci8'
 *       # The user must have 'READ ON DIRECTORY file_storage_dir' privilege.
 *       conn = OCI8.new('username/password')
 *
 *       # The second argument is usually an uppercase string unless the directory
 *       # object is explicitly created as *double-quoted* lowercase characters.
 *       bfile = OCI8::BFILE.new(conn, 'FILE_STORAGE_DIR', 'hello_world.txt')
 *       bfile.read # => "Hello World!\n"
 */

/*
 *  Closes the lob.
 *
 *  @return [self]
 */
static VALUE oci8_lob_close(VALUE self)
{
    oci8_lob_t *lob = TO_LOB(self);
    oci8_base_free(&lob->base);
    return self;
}

static VALUE oci8_lob_do_initialize(int argc, VALUE *argv, VALUE self, ub1 csfrm, ub1 lobtype)
{
    oci8_lob_t *lob = TO_LOB(self);
    VALUE svc;
    VALUE val;
    oci8_svcctx_t *svcctx;
    sword rv;

    rb_scan_args(argc, argv, "11", &svc, &val);
    svcctx = oci8_get_svcctx(svc);
    rv = OCIDescriptorAlloc(oci8_envhp, &lob->base.hp.ptr, OCI_DTYPE_LOB, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    lob->base.type = OCI_DTYPE_LOB;
    lob->pos = 0;
    lob->csfrm = csfrm;
    lob->lobtype = lobtype;
    lob->state = S_NO_OPEN_CLOSE;
    oci8_link_to_parent(&lob->base, &svcctx->base);
    lob->svcctx = svcctx;
    RB_OBJ_WRITTEN(self, Qundef, svc);
    if (!NIL_P(val)) {
        OCI8StringValue(val);
        chker2(OCILobCreateTemporary_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, 0, csfrm, lobtype, TRUE, OCI_DURATION_SESSION),
               &svcctx->base);
        oci8_lob_write(self, val);
        lob->pos = 0; /* reset the position */
    }
    return Qnil;
}

/*
 *  call-seq:
 *    initialize(conn, contents = nil)
 *
 *  Creates a temporary CLOB when <i>contents</i> is not nil.
 *  Otherwise, it creates an uninitialized lob, which is used internally
 *  to fetch CLOB column data.
 *
 *  @example
 *    # Inserts a file name and its contents as CLOB.
 *    clob = OCI8::CLOB.new(conn, File.read(file_name))
 *    conn.exec('insert into file_contents values (:1, :2)', file_name, clob)
 *
 *  @param [OCI8] conn connection
 *  @param [String] contents
 *  @return [OCI8::CLOB]
 */
static VALUE oci8_clob_initialize(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_do_initialize(argc, argv, self, SQLCS_IMPLICIT, OCI_TEMP_CLOB);
    return Qnil;
}

/*
 *  call-seq:
 *    initialize(conn, contents = nil)
 *
 *  Creates a temporary NCLOB when <i>contents</i> is not nil.
 *  Otherwise, it creates an uninitialized lob, which is used internally
 *  to fetch NCLOB column data.
 *
 *  @example
 *    # Inserts a file name and its contents as NCLOB.
 *    clob = OCI8::NCLOB.new(conn, File.read(file_name))
 *    conn.exec('insert into file_contents values (:1, :2)', file_name, clob)
 *
 *  @param [OCI8] conn
 *  @param [String] contents
 *  @return [OCI8::NCLOB]
 */
static VALUE oci8_nclob_initialize(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_do_initialize(argc, argv, self, SQLCS_NCHAR, OCI_TEMP_CLOB);
    return Qnil;
}

/*
 *  call-seq:
 *    initialize(conn, contents = nil)
 *
 *  Creates a temporary BLOB when <i>contents</i> is not nil.
 *  Otherwise, it creates an uninitialized lob, which is used internally
 *  to fetch BLOB column data.
 *
 *  @example
 *    # Inserts a file name and its contents as BLOB.
 *    clob = OCI8::BLOB.new(conn, File.read(file_name, :mode => 'rb'))
 *    conn.exec('insert into file_contents values (:1, :2)', file_name, clob)
 *
 *  @param [OCI8] conn
 *  @param [String] contents
 *  @return [OCI8::BLOB]
 */
static VALUE oci8_blob_initialize(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_do_initialize(argc, argv, self, SQLCS_IMPLICIT, OCI_TEMP_BLOB);
    return Qnil;
}

/*
 *  Returns +true+ when <i>self</i> is initialized.
 *
 *  @return [true or false]
 */
static VALUE oci8_lob_available_p(VALUE self)
{
    oci8_lob_t *lob = TO_LOB(self);
    boolean is_initialized;

    chker2(OCILobLocatorIsInit(oci8_envhp, oci8_errhp, lob->base.hp.lob, &is_initialized),
           &lob->base);
    return is_initialized ? Qtrue : Qfalse;
}

/*
 *  Returns the size.
 *  For CLOB and NCLOB it is the number of characters,
 *  for BLOB and BFILE it is the number of bytes.
 *
 *  @return [Integer]
 */
static VALUE oci8_lob_get_size(VALUE self)
{
    return ULL2NUM(oci8_lob_get_length(TO_LOB(self)));
}

/*
 *  Returns the current offset.
 *  For CLOB and NCLOB it is the number of characters,
 *  for BLOB and BFILE it is the number of bytes.
 *
 *  @return [Integer]
 */
static VALUE oci8_lob_get_pos(VALUE self)
{
    oci8_lob_t *lob = TO_LOB(self);
    return ULL2NUM(lob->pos);
}

/*
 *  Returns true if the current offset is at end of lob.
 *
 *  @return [true or false]
 */
static VALUE oci8_lob_eof_p(VALUE self)
{
    oci8_lob_t *lob = TO_LOB(self);
    if (oci8_lob_get_length(lob) < lob->pos)
        return Qfalse;
    else
        return Qtrue;
}

/*
 *  call-seq:
 *     seek(amount, whence=IO::SEEK_SET)
 *
 *  Seeks to the given offset in the stream. The new position, measured in characters,
 *  is obtained by adding offset <i>amount</i> to the position specified by <i>whence</i>.
 *  If <i>whence</i> is set to IO::SEEK_SET, IO::SEEK_CUR, or IO::SEEK_END,
 *  the offset is relative to the start of the file, the current position
 *  indicator, or end-of-file, respectively.
 *
 *  @param [Integer] amount
 *  @param [IO::SEEK_SET, IO::SEEK_CUR or IO::SEEK_END] whence
 *  @return [self]
 */
static VALUE oci8_lob_seek(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_t *lob = TO_LOB(self);
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
        position = rb_funcall(ULL2NUM(lob->pos), id_plus, 1, position);
    } else if (whence == seek_end) {
        position = rb_funcall(ULL2NUM(oci8_lob_get_length(lob)), id_plus, 1, position);
    }
    lob->pos = NUM2ULL(position);
    return self;
}

/*
 *  Sets the current offset at the beginning.
 *
 *  @return [true or false]
 */
static VALUE oci8_lob_rewind(VALUE self)
{
    oci8_lob_t *lob = TO_LOB(self);
    lob->pos = 0;
    return self;
}

/*
 *  call-seq:
 *    truncate(length)
 *
 *  @param [Integer] length length in characters if +self+ is a {CLOB} or a {NCLOB}.
 *    length in bytes if +self+ is a {BLOB} or a {BFILE}.
 *  @return [self]
 *  @see #size=
 */
static VALUE oci8_lob_truncate(VALUE self, VALUE len)
{
    oci8_lob_t *lob = TO_LOB(self);
    oci8_svcctx_t *svcctx = check_svcctx(lob);

    chker2(OCILobTrim2_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, NUM2ULL(len)),
           &svcctx->base);
    return self;
}

/*
 * @overload size=(length)
 *
 *  Changes the lob size to the given <i>length</i>.
 *
 *  @param [Integer] length length in characters if +self+ is a {CLOB} or a {NCLOB}.
 *    length in bytes if +self+ is a {BLOB} or a {BFILE}.
 *  @see #truncate
 */
static VALUE oci8_lob_set_size(VALUE self, VALUE len)
{
    oci8_lob_truncate(self, len);
    return len;
}

static void open_bfile(oci8_svcctx_t *svcctx, oci8_lob_t *lob, OCIError *errhp)
{
    while (1) {
        sword rv = OCILobFileOpen_nb(svcctx, svcctx->base.hp.svc, errhp, lob->base.hp.lob, OCI_FILE_READONLY);
        if (rv == OCI_ERROR && oci8_get_error_code(oci8_errhp) == 22290) {
            /* ORA-22290: operation would exceed the maximum number of opened files or LOBs */
            /* close all opened BFILE implicitly. */
            oci8_base_t *base;
            for (base = &lob->base; base != &lob->base; base = base->next) {
                if (base->type == OCI_DTYPE_LOB) {
                    oci8_lob_t *tmp = (oci8_lob_t *)base;
                    if (tmp->state == S_BFILE_OPEN) {
                        chker2(OCILobFileClose_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, tmp->base.hp.lob),
                               &svcctx->base);
                        tmp->state = S_BFILE_CLOSE;
                    }
                }
            }
        } else {
            chker2(rv, &svcctx->base);
            lob->state = S_BFILE_OPEN;
            return;
        }
    }
}

/*
 * @overload read
 *
 *  
 *
 *  @param [Integer] length number of characters if +self+ is a {CLOB} or a {NCLOB}.
 *    number of bytes if +self+ is a {BLOB} or a {BFILE}.
 *  @return [String or nil] data read. <code>nil</code> means it
 *    met EOF at beginning. It returns an empty string '' as a special exception
 *    when <i>length</i> is <code>nil</code> and the lob is empty.
 *
 * @overload read(length)
 *
 *  Reads <i>length</i> characters for {CLOB} and {NCLOB} or <i>length</i>
 *  bytes for {BLOB} and {BFILE} from the current position.
 *  If <i>length</i> is <code>nil</code>, it reads data until EOF.
 *
 *  @param [Integer] length number of characters if +self+ is a {CLOB} or a {NCLOB}.
 *    number of bytes if +self+ is a {BLOB} or a {BFILE}.
 *  @return [String or nil] data read. <code>nil</code> means it
 *    met EOF at beginning. It returns an empty string '' as a special exception
 *    when <i>length</i> is <code>nil</code> and the lob is empty.
 */
static VALUE oci8_lob_read(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_t *lob = TO_LOB(self);
    oci8_svcctx_t *svcctx = check_svcctx(lob);
    ub8 pos = lob->pos;
    long strbufsiz = oci8_initial_chunk_size;
    ub8 sz;
    ub8 byte_amt;
    ub8 char_amt;
    sword rv;
    VALUE size;
    VALUE v = rb_ary_new();
    OCIError *errhp = oci8_errhp;
    ub1 piece = OCI_FIRST_PIECE;

    rb_scan_args(argc, argv, "01", &size);
    if (NIL_P(size)) {
        sz = UB4MAXVAL;
    } else {
        sz = NUM2ULL(size);
    }
    if (lob->state == S_BFILE_CLOSE) {
        open_bfile(svcctx, lob, errhp);
    }
read_more_data:
    if (lob->lobtype == OCI_TEMP_CLOB) {
        byte_amt = 0;
        char_amt = sz;
    } else {
        byte_amt = sz;
        char_amt = 0;
    }
    do {
        VALUE strbuf = rb_str_buf_new(strbufsiz);
        char *buf = RSTRING_PTR(strbuf);

        rv = OCILobRead2_nb(svcctx, svcctx->base.hp.svc, errhp, lob->base.hp.lob, &byte_amt, &char_amt, pos + 1, buf, strbufsiz, piece, NULL, NULL, 0, lob->csfrm);
        svcctx->suppress_free_temp_lobs = 0;
        switch (rv) {
        case OCI_SUCCESS:
            break;
        case OCI_NEED_DATA:
            /* prevent OCILobFreeTemporary() from being called.
             * See: https://github.com/kubo/ruby-oci8/issues/20
             */
            svcctx->suppress_free_temp_lobs = 1;
            piece = OCI_NEXT_PIECE;
            break;
        default:
            chker2(rv, &svcctx->base);
        }
        if (byte_amt == 0)
            break;
        if (lob->lobtype == OCI_TEMP_CLOB) {
            pos += char_amt;
        } else {
            pos += byte_amt;
        }
        rb_str_set_len(strbuf, byte_amt);
        rb_ary_push(v, strbuf);
        if (strbufsiz < oci8_max_chunk_size) {
            strbufsiz *= 2;
        }
    } while (rv == OCI_NEED_DATA);

    if (NIL_P(size) && pos - lob->pos == sz) {
        lob->pos = pos;
        piece = OCI_FIRST_PIECE;
        goto read_more_data;
    }
    lob->pos = pos;
    switch (RARRAY_LEN(v)) {
    case 0:
        if (NIL_P(size) && pos == 0) {
            return rb_usascii_str_new("", 0);
        } else {
            return Qnil;
        }
    case 1:
        v = RARRAY_AREF(v, 0);
        break;
    default:
        v = rb_ary_join(v, Qnil);
    }
    OBJ_TAINT(v);
    if (lob->lobtype == OCI_TEMP_CLOB) {
        /* set encoding */
        rb_enc_associate(v, oci8_encoding);
        return rb_str_conv_enc(v, oci8_encoding, rb_default_internal_encoding());
    } else {
        /* ASCII-8BIT */
        return v;
    }
}

/*
 * @overload write(data)
 *
 *  Writes +data+ to LOB.
 *
 *  @param [String] data
 *  @return [Integer] number of characters written if +self+ is a {CLOB} or a {NCLOB}.
 *    number of bytes written if +self+ is a {BLOB} or a {BFILE}.
 */
static VALUE oci8_lob_write(VALUE self, VALUE data)
{
    oci8_lob_t *lob = TO_LOB(self);
    oci8_svcctx_t *svcctx = check_svcctx(lob);
    volatile VALUE str;
    ub8 byte_amt;
    ub8 char_amt;

    if (TYPE(data) != T_STRING) {
        str = rb_obj_as_string(data);
    } else {
        str = data;
    }
    if (lob->lobtype == OCI_TEMP_CLOB) {
        str = rb_str_export_to_enc(str, oci8_encoding);
    }
    byte_amt = RSTRING_LEN(str);
    if (byte_amt == 0) {
        /* to avoid ORA-24801: illegal parameter value in OCI lob function */
        return INT2FIX(0);
    }
    char_amt = 0;
    chker2(OCILobWrite2_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, &byte_amt, &char_amt, lob->pos + 1, RSTRING_PTR(str), byte_amt, OCI_ONE_PIECE, NULL, NULL, 0, lob->csfrm),
           &svcctx->base);
    RB_GC_GUARD(str);
    if (lob->lobtype == OCI_TEMP_CLOB) {
        lob->pos += char_amt;
        return UINT2NUM(char_amt);
    } else {
        lob->pos += byte_amt;
        return UINT2NUM(byte_amt);
    }
}

/*
 *  @deprecated LOB#sync had not worked by mistake. Do nothing now.
 *  @private
 */
static VALUE oci8_lob_get_sync(VALUE self)
{
    rb_warning("LOB#sync had not worked by mistake. Do nothing now.");
    return Qfalse;
}

/*
 *  @deprecated LOB#sync had not worked by mistake. Do nothing now.
 *  @private
 */
static VALUE oci8_lob_set_sync(VALUE self, VALUE b)
{
    rb_warning("LOB#sync had not worked by mistake. Do nothing now.");
    return b;
}

/*
 *  @deprecated LOB#flush had not worked by mistake. Do nothing now.
 *  @private
 */
static VALUE oci8_lob_flush(VALUE self)
{
    rb_warning("LOB#flush had not worked by mistake. Do nothing now.");
    return self;
}

/*
 *  Returns the chunk size of a LOB.
 *
 *  @see http://docs.oracle.com/database/121/ARPLS/d_lob.htm#ARPLS66706 DBMS_LOB.GETCHUNKSIZE
 *  @return [Integer]
 */
static VALUE oci8_lob_get_chunk_size(VALUE self)
{
    oci8_lob_t *lob = TO_LOB(self);
    oci8_svcctx_t *svcctx = check_svcctx(lob);
    ub4 len;

    chker2(OCILobGetChunkSize_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, &len),
           &svcctx->base);
    return UINT2NUM(len);
}

static VALUE oci8_lob_clone(VALUE self)
{
    oci8_lob_t *lob = TO_LOB(self);
    oci8_lob_t *newlob;
    VALUE newobj = lob->svcctx ? lob->svcctx->base.self : Qnil;
    boolean is_temporary;

    newobj = rb_class_new_instance(1, &newobj, CLASS_OF(self));
    newlob = DATA_PTR(newobj);
    if (OCILobIsTemporary(oci8_envhp, oci8_errhp, lob->base.hp.lob, &is_temporary) == OCI_SUCCESS
        && is_temporary) {
        oci8_svcctx_t *svcctx = check_svcctx(lob);
        chker2(OCILobLocatorAssign_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, &newlob->base.hp.lob),
               &svcctx->base);
    } else {
        chker2(OCILobAssign(oci8_envhp, oci8_errhp, lob->base.hp.lob, &newlob->base.hp.lob), &lob->base);
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
        oci8_lob_t *lob = TO_LOB(self);
        char d_buf[31];
        ub2 d_length = sizeof(d_buf);
        char f_buf[256];
        ub2 f_length = sizeof(f_buf);
        VALUE dir_alias;
        VALUE filename;

        chker2(OCILobFileGetName(oci8_envhp, oci8_errhp, lob->base.hp.lob, TO_ORATEXT(d_buf), &d_length, TO_ORATEXT(f_buf), &f_length),
               &lob->base);
        dir_alias = rb_external_str_new_with_enc(d_buf, d_length, oci8_encoding);
        filename = rb_external_str_new_with_enc(f_buf, f_length, oci8_encoding);
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
    oci8_lob_t *lob = TO_LOB(self);

    bfile_close(lob);
    if (RSTRING_LEN(dir_alias) > UB2MAXVAL) {
        rb_raise(rb_eRuntimeError, "dir_alias is too long.");
    }
    if (RSTRING_LEN(filename) > UB2MAXVAL) {
        rb_raise(rb_eRuntimeError, "filename is too long.");
    }
    chker2(OCILobFileSetName(oci8_envhp, oci8_errhp, &lob->base.hp.lob,
                             RSTRING_ORATEXT(dir_alias), (ub2)RSTRING_LEN(dir_alias),
                             RSTRING_ORATEXT(filename), (ub2)RSTRING_LEN(filename)),
           &lob->base);
}

/*
 * @overload initialize(conn, directory = nil, filename = nil)
 *
 *  Creates a BFILE object.
 *  This is correspond to {BFILENAME}[https://docs.oracle.com/database/121/SQLRF/functions020.htm].
 *
 *  @param [OCI8] conn
 *  @param [String] directory  a directory object created by
 *    {"CREATE DIRECTORY"}[http://docs.oracle.com/database/121/SQLRF/statements_5008.htm].
 *  @param [String] filename
 *  @return [OCI8::BFILE]
 */
static VALUE oci8_bfile_initialize(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_t *lob = TO_LOB(self);
    VALUE svc;
    VALUE dir_alias;
    VALUE filename;
    oci8_svcctx_t *svcctx;
    int rv;

    rb_scan_args(argc, argv, "12", &svc, &dir_alias, &filename);
    svcctx = oci8_get_svcctx(svc);
    rv = OCIDescriptorAlloc(oci8_envhp, &lob->base.hp.ptr, OCI_DTYPE_LOB, 0, NULL);
    if (rv != OCI_SUCCESS) {
        oci8_env_raise(oci8_envhp, rv);
    }
    lob->base.type = OCI_DTYPE_LOB;
    lob->pos = 0;
    lob->csfrm = SQLCS_IMPLICIT;
    lob->lobtype = OCI_TEMP_BLOB;
    lob->state = S_BFILE_CLOSE;
    if (argc != 1) {
        OCI8SafeStringValue(dir_alias);
        OCI8SafeStringValue(filename);
        oci8_bfile_set_name(self, dir_alias, filename);
    }
    oci8_link_to_parent(&lob->base, &svcctx->base);
    lob->svcctx = svcctx;
    return Qnil;
}

/*
 * @overload dir_alias
 *
 *  Returns the directory object name.
 *
 *  @return [String]
 */
static VALUE oci8_bfile_get_dir_alias(VALUE self)
{
    VALUE dir_alias;

    oci8_bfile_get_name(self, &dir_alias, NULL);
    return dir_alias;
}

/*
 * @overload filename
 *
 *  Returns the file name.
 *
 *  @return [String]
 */
static VALUE oci8_bfile_get_filename(VALUE self)
{
    VALUE filename;

    oci8_bfile_get_name(self, NULL, &filename);
    return filename;
}

/*
 * @overload dir_alias=(dir_alias)
 *
 *  Changes the directory object name.
 *
 *  @param [String] dir_alias
 */
static VALUE oci8_bfile_set_dir_alias(VALUE self, VALUE dir_alias)
{
    VALUE filename;

    OCI8SafeStringValue(dir_alias);
    oci8_bfile_get_name(self, NULL, &filename);
    oci8_bfile_set_name(self, dir_alias, filename);
    rb_ivar_set(self, id_dir_alias, dir_alias);
    return dir_alias;
}

/*
 * @overload filename=(filename)
 *
 *  Changes the file name.
 *
 *  @param [String] filename
 */
static VALUE oci8_bfile_set_filename(VALUE self, VALUE filename)
{
    VALUE dir_alias;

    OCI8SafeStringValue(filename);
    oci8_bfile_get_name(self, &dir_alias, NULL);
    oci8_bfile_set_name(self, dir_alias, filename);
    rb_ivar_set(self, id_filename, filename);
    return filename;
}

/*
 * @overload exists?
 *
 *  Returns <code>true</code> when the BFILE exists on the server's operating system.
 */
static VALUE oci8_bfile_exists_p(VALUE self)
{
    oci8_lob_t *lob = TO_LOB(self);
    oci8_svcctx_t *svcctx = check_svcctx(lob);
    boolean flag;

    chker2(OCILobFileExists_nb(svcctx, svcctx->base.hp.svc, oci8_errhp, lob->base.hp.lob, &flag),
           &svcctx->base);
    return flag ? Qtrue : Qfalse;
}

/*
 *  Raises <code>RuntimeError</code> always.
 *
 *  @raise [RuntimeError] cannot modify a read-only BFILE object
 */
static VALUE oci8_bfile_error(VALUE self, VALUE dummy)
{
    rb_raise(rb_eRuntimeError, "cannot modify a read-only BFILE object");
}

/*
 * bind_clob/blob/bfile
 */

typedef struct {
    oci8_bind_data_type_t bind;
    VALUE *klass;
} oci8_bind_lob_data_type_t;

static VALUE bind_lob_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)data;
    return oci8_lob_clone(oho->obj);
}

static void bind_lob_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)data;
    const oci8_handle_data_type_t *bind_data_type = obind->base.data_type;
    const oci8_handle_data_type_t *lob_data_type = bind_data_type->rb_data_type.data;
    oci8_base_t *h = oci8_check_typeddata(val, lob_data_type, 1);
    oho->hp = h->hp.ptr;
    RB_OBJ_WRITE(obind->base.self, &oho->obj, val);
}

static void bind_lob_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length)
{
    obind->value_sz = sizeof(void *);
    obind->alloc_sz = sizeof(oci8_hp_obj_t);
}

static void bind_lob_init_elem(oci8_bind_t *obind, VALUE svc)
{
    const oci8_bind_lob_data_type_t *data_type = (const oci8_bind_lob_data_type_t *)obind->base.data_type;
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)obind->valuep;
    oci8_base_t *h;
    ub4 idx = 0;

    do {
        oho[idx].obj = rb_class_new_instance(1, &svc, *data_type->klass);
        RB_OBJ_WRITTEN(obind->base.self, Qundef, oho[idx].obj);
        h = DATA_PTR(oho[idx].obj);
        oho[idx].hp = h->hp.ptr;
    } while (++idx < obind->maxar_sz);
}

static void bind_lob_post_bind_hook_for_nclob(oci8_bind_t *obind)
{
    ub1 csfrm = SQLCS_NCHAR;

    chker2(OCIAttrSet(obind->base.hp.ptr, obind->base.type, (void*)&csfrm, 0, OCI_ATTR_CHARSET_FORM, oci8_errhp),
           &obind->base);
}

static const oci8_bind_lob_data_type_t bind_clob_data_type = {
    {
        {
            {
                "OCI8::BindType::CLOB",
                {
                    (RUBY_DATA_FUNC)oci8_bind_hp_obj_mark,
                    oci8_handle_cleanup,
                    oci8_handle_size,
                },
                &oci8_bind_data_type.rb_data_type, (void*)&oci8_clob_data_type,
#ifdef RUBY_TYPED_WB_PROTECTED
                RUBY_TYPED_WB_PROTECTED,
#endif
            },
            oci8_bind_free,
            sizeof(oci8_bind_t)
        },
        bind_lob_get,
        bind_lob_set,
        bind_lob_init,
        bind_lob_init_elem,
        NULL,
        SQLT_CLOB
    },
    &cOCI8CLOB
};

static VALUE bind_clob_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_clob_data_type.bind.base);
}

static const oci8_bind_lob_data_type_t bind_nclob_data_type = {
    {
        {
            {
                "OCI8::BindType::NCLOB",
                {
                    (RUBY_DATA_FUNC)oci8_bind_hp_obj_mark,
                    oci8_handle_cleanup,
                    oci8_handle_size,
                },
                &oci8_bind_data_type.rb_data_type, (void*)&oci8_nclob_data_type,
#ifdef RUBY_TYPED_WB_PROTECTED
                RUBY_TYPED_WB_PROTECTED,
#endif
            },
            oci8_bind_free,
            sizeof(oci8_bind_t)
        },
        bind_lob_get,
        bind_lob_set,
        bind_lob_init,
        bind_lob_init_elem,
        NULL,
        SQLT_CLOB,
        bind_lob_post_bind_hook_for_nclob,
    },
    &cOCI8NCLOB
};

static VALUE bind_nclob_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_nclob_data_type.bind.base);
}

static const oci8_bind_lob_data_type_t bind_blob_data_type = {
    {
        {
            {
                "OCI8::BindType::BLOB",
                {
                    (RUBY_DATA_FUNC)oci8_bind_hp_obj_mark,
                    oci8_handle_cleanup,
                    oci8_handle_size,
                },
                &oci8_bind_data_type.rb_data_type, (void*)&oci8_blob_data_type,
#ifdef RUBY_TYPED_WB_PROTECTED
                RUBY_TYPED_WB_PROTECTED,
#endif
            },
            oci8_bind_free,
            sizeof(oci8_bind_t)
        },
        bind_lob_get,
        bind_lob_set,
        bind_lob_init,
        bind_lob_init_elem,
        NULL,
        SQLT_BLOB
    },
    &cOCI8BLOB
};

static VALUE bind_blob_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_blob_data_type.bind.base);
}

static const oci8_bind_lob_data_type_t bind_bfile_data_type = {
    {
        {
            {
                "OCI8::BindType::BFILE",
                {
                    (RUBY_DATA_FUNC)oci8_bind_hp_obj_mark,
                    oci8_handle_cleanup,
                    oci8_handle_size,
                },
                &oci8_bind_data_type.rb_data_type, (void*)&oci8_bfile_data_type,
#ifdef RUBY_TYPED_WB_PROTECTED
                RUBY_TYPED_WB_PROTECTED,
#endif
            },
            oci8_bind_free,
            sizeof(oci8_bind_t)
        },
        bind_lob_get,
        bind_lob_set,
        bind_lob_init,
        bind_lob_init_elem,
        NULL,
        SQLT_BFILE
    },
    &cOCI8BFILE
};

static VALUE bind_bfile_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_bfile_data_type.bind.base);
}

/* bind_string_from_(clob|nclob|blob) */
typedef struct {
    OCILobLocator *loc;
    chunk_buf_t buf;
} string_from_lob_t;

enum bind_string_from_lob_type {
    BIND_STRING_FROM_CLOB,
    BIND_STRING_FROM_NCLOB,
    BIND_STRING_FROM_BLOB,
    NO_BIND_STRING_FROM_LOB,
};

static enum bind_string_from_lob_type bind_string_from_lob_type(oci8_bind_t *obind);
#define IS_BIND_CHAR_LOB(obind) (bind_string_from_lob_type(obind) != BIND_STRING_FROM_BLOB)

static void bind_string_from_lob_free(oci8_base_t *base)
{
    oci8_bind_t *obind = (oci8_bind_t *)base;
    string_from_lob_t *sfl = (string_from_lob_t *)obind->valuep;

    if (sfl != NULL) {
        ub4 idx = 0;
        do {
            if (sfl[idx].loc != NULL) {
                OCIDescriptorFree(sfl[idx].loc, OCI_DTYPE_LOB);
            }
            oci8_chunk_buf_free(&sfl[idx].buf);
        } while (++idx < obind->maxar_sz);
    }
    oci8_bind_free(base);
}

static VALUE bind_string_from_lob_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    string_from_lob_t *sfl = (string_from_lob_t *)data;
    VALUE str = oci8_chunk_buf_to_str(&sfl->buf, IS_BIND_CHAR_LOB(obind));

    OBJ_TAINT(str);
    return str;
}

static void bind_string_from_lob_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    const char *type_name = obind->base.data_type->rb_data_type.wrap_struct_name;
    rb_raise(rb_eRuntimeError, "Could not set any value to this bind type %s", type_name);
}

static void bind_string_from_lob_init(oci8_bind_t *obind, VALUE svc, VALUE val, VALUE length)
{
    obind->value_sz = sizeof(void *);
    obind->alloc_sz = sizeof(string_from_lob_t);
}

static void bind_string_from_lob_init_elem(oci8_bind_t *obind, VALUE svc)
{
    string_from_lob_t *sfl = (string_from_lob_t *)obind->valuep;
    ub4 idx = 0;

    do {
        sword rv = OCIDescriptorAlloc(oci8_envhp, (dvoid**)&sfl->loc, OCI_DTYPE_LOB, 0, NULL);
        if (rv != OCI_SUCCESS) {
            oci8_env_raise(oci8_envhp, rv);
        }
        sfl->buf.head = NULL;
        sfl->buf.tail = &sfl->buf.head;
        sfl++;
    } while (++idx < obind->maxar_sz);
}

static void bind_string_from_lob_post_bind_hook(oci8_bind_t *obind)
{
    if (obind->base.type != OCI_HTYPE_DEFINE) {
        const char *type_name = obind->base.data_type->rb_data_type.wrap_struct_name;
        rb_raise(rb_eRuntimeError, "Could not bind %s", type_name);
    }
    if (bind_string_from_lob_type(obind) == BIND_STRING_FROM_NCLOB) {
        ub1 csfrm = SQLCS_NCHAR;

        chker2(OCIAttrSet(obind->base.hp.ptr, obind->base.type, (void*)&csfrm, 0, OCI_ATTR_CHARSET_FORM, oci8_errhp),
               &obind->base);
    }
}

static const oci8_bind_data_type_t bind_string_from_clob_data_type = {
    {
        {
            "OCI8::BindType::StringFromCLOB",
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
        bind_string_from_lob_free,
        sizeof(oci8_bind_t)
    },
    bind_string_from_lob_get,
    bind_string_from_lob_set,
    bind_string_from_lob_init,
    bind_string_from_lob_init_elem,
    NULL,
    SQLT_CLOB,
    bind_string_from_lob_post_bind_hook,
};

static VALUE bind_string_from_clob_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_string_from_clob_data_type.base);
}
static const oci8_bind_data_type_t bind_string_from_nclob_data_type = {
    {
        {
            "OCI8::BindType::StringFromNCLOB",
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
        bind_string_from_lob_free,
        sizeof(oci8_bind_t)
    },
    bind_string_from_lob_get,
    bind_string_from_lob_set,
    bind_string_from_lob_init,
    bind_string_from_lob_init_elem,
    NULL,
    SQLT_CLOB,
    bind_string_from_lob_post_bind_hook,
};

static VALUE bind_string_from_nclob_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_string_from_nclob_data_type.base);
}
static const oci8_bind_data_type_t bind_string_from_blob_data_type = {
    {
        {
            "OCI8::BindType::StringFromBLOB",
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
        bind_string_from_lob_free,
        sizeof(oci8_bind_t)
    },
    bind_string_from_lob_get,
    bind_string_from_lob_set,
    bind_string_from_lob_init,
    bind_string_from_lob_init_elem,
    NULL,
    SQLT_BLOB,
    bind_string_from_lob_post_bind_hook,
};

static VALUE bind_string_from_blob_alloc(VALUE klass)
{
    return oci8_allocate_typeddata(klass, &bind_string_from_blob_data_type.base);
}

static enum bind_string_from_lob_type bind_string_from_lob_type(oci8_bind_t *obind)
{
    oci8_bind_data_type_t *data_type = (oci8_bind_data_type_t *)obind->base.data_type;

    if (data_type == &bind_string_from_clob_data_type) {
        return BIND_STRING_FROM_CLOB;
    }
    if (data_type == &bind_string_from_nclob_data_type) {
        return BIND_STRING_FROM_NCLOB;
    }
    if (data_type == &bind_string_from_blob_data_type) {
        return BIND_STRING_FROM_BLOB;
    }
    return NO_BIND_STRING_FROM_LOB;
}

static void read_lobs_as_string(oci8_svcctx_t *svcctx, OCIError *errhp, ub4 num, OCILobLocator **lobp_arr, chunk_buf_t **cb, ub1 csfrm);
static sb4 lob_array_read_cb(void *ctxp, ub4 array_iter, const void  *bufp, oraub8 len, ub1 piece, void  **changed_bufpp, oraub8 *changed_lenp);

void oci8_read_lobs_as_string(oci8_svcctx_t *svcctx, oci8_base_t *stmt, ub4 nrows)
{
    oci8_bind_t *obind;
    ub4 nums[3] = {0, 0, 0};
    ub4 idx;
    OCILobLocator **lobp_arr[3];
    chunk_buf_t **cb[3];
    OCIError *errhp = oci8_errhp;
    int lob_type;

    obind = (oci8_bind_t *)stmt->children;
    do {
        if (obind->base.type == OCI_HTYPE_DEFINE) {
            lob_type = bind_string_from_lob_type(obind);
            if (lob_type != NO_BIND_STRING_FROM_LOB) {
                for (idx = 0; idx < nrows; idx++) {
                    if (obind->u.inds[idx] == 0) {
                        nums[lob_type]++;
                    }
                }
            }
        }
        obind = (oci8_bind_t *)obind->base.next;
    } while (obind != (oci8_bind_t *)stmt->children);

    lobp_arr[0] = ALLOCA_N(OCILobLocator *, nums[0]);
    lobp_arr[1] = ALLOCA_N(OCILobLocator *, nums[1]);
    lobp_arr[2] = ALLOCA_N(OCILobLocator *, nums[2]);
    cb[0] = ALLOCA_N(chunk_buf_t *, nums[0]);
    cb[1] = ALLOCA_N(chunk_buf_t *, nums[1]);
    cb[2] = ALLOCA_N(chunk_buf_t *, nums[2]);

    nums[0] = nums[1] = nums[2] = 0;
    obind = (oci8_bind_t *)stmt->children;
    do {
        if (obind->base.type == OCI_HTYPE_DEFINE) {
            lob_type = bind_string_from_lob_type(obind);
            if (lob_type != NO_BIND_STRING_FROM_LOB) {
                string_from_lob_t *sfl = (string_from_lob_t *)obind->valuep;
                for (idx = 0; idx < nrows; idx++) {
                    if (obind->u.inds[idx] == 0) {
                        ub4 n = nums[lob_type];
                        lobp_arr[lob_type][n] = sfl[idx].loc;
                        cb[lob_type][n] = &sfl[idx].buf;
                        nums[lob_type]++;
                    }
                }
            }
        }
        obind = (oci8_bind_t *)obind->base.next;
    } while (obind != (oci8_bind_t *)stmt->children);

    read_lobs_as_string(svcctx, errhp, nums[0], lobp_arr[0], cb[0], SQLCS_IMPLICIT);
    read_lobs_as_string(svcctx, errhp, nums[1], lobp_arr[1], cb[1], SQLCS_NCHAR);
    read_lobs_as_string(svcctx, errhp, nums[2], lobp_arr[2], cb[2], 0);
}

static void read_lobs_as_string(oci8_svcctx_t *svcctx, OCIError *errhp, ub4 num, OCILobLocator **lobp_arr, chunk_buf_t **cb, ub1 csfrm)
{
    oraub8 *byte_amt_arr = ALLOCA_N(oraub8, num);
    oraub8 *char_amt_arr = ALLOCA_N(oraub8, num);
    oraub8 *offset_arr = ALLOCA_N(oraub8, num);
    void **bufp_arr = ALLOCA_N(void *, num);
    oraub8 *bufl_arr = ALLOCA_N(oraub8, num);
    ub4 idx, idx2;
    const ub4 max_read_size = UB4MAXVAL;

    if (num == 0) {
        return;
    }

    for (idx = 0; idx < num; idx++) {
        cb[idx]->tail = &cb[idx]->head;
        offset_arr[idx] = 1;
    }

read_more_data:
    for (idx = 0; idx < num; idx++) {
        chunk_t *chunk = oci8_chunk_next(cb[idx]);

        chunk->used_len = 0;
        byte_amt_arr[idx] = csfrm ? 0 : max_read_size;
        char_amt_arr[idx] = csfrm ? max_read_size : 0;
        bufp_arr[idx] = chunk->buf;
        bufl_arr[idx] = chunk->alloc_len;
    }
    chker2(OCILobArrayRead_nb(svcctx, svcctx->base.hp.svc, errhp, &num, lobp_arr, byte_amt_arr, char_amt_arr, offset_arr, bufp_arr, bufl_arr, OCI_FIRST_PIECE, cb, lob_array_read_cb, 0, csfrm ? csfrm : SQLCS_IMPLICIT), &svcctx->base);
    for (idx = idx2 = 0; idx < num; idx++) {
        ub4 nread = csfrm ? char_amt_arr[idx] : byte_amt_arr[idx];
        if (nread == max_read_size) {
            lobp_arr[idx2] = lobp_arr[idx];
            offset_arr[idx2] += nread;
            cb[idx2++] = cb[idx];
        }
    }
    if (idx2 != 0) {
        num = idx2;
        goto read_more_data;
    }
}

static sb4 lob_array_read_cb(void *ctxp, ub4 array_iter, const void *bufp, oraub8 len, ub1 piece, void **changed_bufpp, oraub8 *changed_lenp)
{
    {
    chunk_buf_t *cb = ((chunk_buf_t **)ctxp)[array_iter - 1];
    chunk_t *chunk = (chunk_t *)((size_t)bufp - offsetof(chunk_t, buf));
    chunk->used_len = (ub4)len;

    if (piece != OCI_LAST_PIECE) {
        chunk = oci8_chunk_next(cb);
        chunk->used_len = 0;
        *changed_bufpp = chunk->buf;
        *changed_lenp = chunk->alloc_len;
    }
    }
    return OCI_CONTINUE;
}

void Init_oci8_lob(VALUE cOCI8)
{
    id_plus = rb_intern("+");
    id_dir_alias = rb_intern("@dir_alias");
    id_filename = rb_intern("@filename");
    seek_set = rb_eval_string("::IO::SEEK_SET");
    seek_cur = rb_eval_string("::IO::SEEK_CUR");
    seek_end = rb_eval_string("::IO::SEEK_END");

#if 0
    /* for yard */
    cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    cOCI8 = rb_define_class("OCI8", cOCIHandle);
    cOCI8LOB = rb_define_class_under(cOCI8, "LOB", cOCIHandle);
    cOCI8CLOB = rb_define_class_under(cOCI8, "CLOB", cOCI8LOB);
    cOCI8NCLOB = rb_define_class_under(cOCI8, "NCLOB", cOCI8LOB);
    cOCI8BLOB = rb_define_class_under(cOCI8, "BLOB", cOCI8LOB);
    cOCI8BFILE = rb_define_class_under(cOCI8, "BFILE", cOCI8LOB);
#endif

    cOCI8LOB = oci8_define_class_under(cOCI8, "LOB", &oci8_lob_data_type, oci8_lob_alloc);
    cOCI8CLOB = oci8_define_class_under(cOCI8, "CLOB", &oci8_clob_data_type, oci8_clob_alloc);
    cOCI8NCLOB = oci8_define_class_under(cOCI8, "NCLOB", &oci8_nclob_data_type, oci8_nclob_alloc);
    cOCI8BLOB = oci8_define_class_under(cOCI8, "BLOB", &oci8_blob_data_type, oci8_blob_alloc);
    cOCI8BFILE = oci8_define_class_under(cOCI8, "BFILE", &oci8_bfile_data_type, oci8_bfile_alloc);

    rb_define_method(cOCI8CLOB, "initialize", oci8_clob_initialize, -1);
    rb_define_method(cOCI8NCLOB, "initialize", oci8_nclob_initialize, -1);
    rb_define_method(cOCI8BLOB, "initialize", oci8_blob_initialize, -1);
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

    oci8_define_bind_class("CLOB", &bind_clob_data_type.bind, bind_clob_alloc);
    oci8_define_bind_class("NCLOB", &bind_nclob_data_type.bind, bind_nclob_alloc);
    oci8_define_bind_class("BLOB", &bind_blob_data_type.bind, bind_blob_alloc);
    oci8_define_bind_class("BFILE", &bind_bfile_data_type.bind, bind_bfile_alloc);
    oci8_define_bind_class("StringFromCLOB", &bind_string_from_clob_data_type, bind_string_from_clob_alloc);
    oci8_define_bind_class("StringFromNCLOB", &bind_string_from_nclob_data_type, bind_string_from_nclob_alloc);
    oci8_define_bind_class("StringFromBLOB", &bind_string_from_blob_data_type, bind_string_from_blob_alloc);
}
