#include "oci8.h"

static VALUE cOCILobLocator;

typedef struct {
    oci8_base_t base;
    int char_width;
} oci8_lob_locator_t;

static oci8_base_class_t oci8_lob_locator_class = {
    NULL,
    NULL,
    sizeof(oci8_lob_locator_t),
};

static VALUE oci8_lob_initialize(VALUE self)
{
    oci8_lob_locator_t *lob = DATA_PTR(self);
    sword rv;

    rv = OCIDescriptorAlloc(oci8_envhp, &lob->base.hp, OCI_DTYPE_LOB, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    lob->base.type = OCI_DTYPE_LOB;
    lob->char_width = 1;
    return Qnil;
}

static VALUE oci8_lob_set_char_width(VALUE self, VALUE vsize)
{
    oci8_lob_locator_t *lob = DATA_PTR(self);
    int size;

    size = NUM2INT(vsize); /* 1 */

    if (size <= 0)
        rb_raise(rb_eArgError, "size must be more than one.");
    lob->char_width = size;
    return vsize;
}

static VALUE oci8_lob_is_initialized_p(VALUE self)
{
    oci8_lob_locator_t *lob = DATA_PTR(self);
    boolean is_initialized;
    sword rv;

    rv = OCILobLocatorIsInit(oci8_envhp, oci8_errhp, lob->base.hp, &is_initialized);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return is_initialized ? Qtrue : Qfalse;
}

/*
=begin
--- OCILobLocator#GetLength()
     get the length of LOB.
     counts by bytes for BLOB, by charactors for CLOB.
=end
 */
static VALUE oci8_lob_get_length(VALUE self, VALUE vsvc)
{
    oci8_lob_locator_t *lob = DATA_PTR(self);
    OCISvcCtx *svchp;
    ub4 len;
    sword rv;

    svchp = oci8_get_oci_svcctx(vsvc); /* 1 */

    rv = OCILobGetLength(svchp, oci8_errhp, lob->base.hp, &len);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return INT2FIX(len);
}

static VALUE oci8_lob_read(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_locator_t *lob = DATA_PTR(self);
    VALUE vsvc, voffset, vamt, vcsid, vcsfrm;
    OCISvcCtx *svchp;
    ub4 offset;
    ub2 csid;
    ub1 csfrm;
    ub4 amt;
    sword rv;
    char buf[4096];
    size_t buf_size_in_char;
    VALUE v = Qnil;

    rb_scan_args(argc, argv, "32", &vsvc, &voffset, &vamt, &vcsid, &vcsfrm);
    svchp = oci8_get_oci_svcctx(vsvc); /* 1 */
    offset = NUM2INT(voffset); /* 2 */
    amt = NUM2INT(vamt); /* 3 */
    Get_Int_With_Default(argc, 4, vcsid, csid, 0); /* 4 */
    Get_Int_With_Default(argc, 5, vcsfrm, csfrm, SQLCS_IMPLICIT); /* 5 */

    /* Disadvantage of polling way in contrast with callback method is
     * that it sets 'amt' the number of characters readed, when charset
     * is fixed size. For single byte charset or variable size charset,
     * it cause no problem because the unit of 'amt' is byte. But for
     * fixed size multibyte charset, how can I know the size of a
     * character from system? Therefore who want to use fixed size
     * multibyte charset must set the size explicitly.
     *
     * Umm, if I could use callback method, I have no need to care about
     * it.
     */
    buf_size_in_char = sizeof(buf) / lob->char_width;
    do {
        rv = OCILobRead(svchp, oci8_errhp, lob->base.hp, &amt, offset, buf, sizeof(buf), NULL, NULL, 0, SQLCS_IMPLICIT);
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
    return v;
}

static VALUE oci8_lob_write(int argc, VALUE *argv, VALUE self)
{
    oci8_lob_locator_t *lob = DATA_PTR(self);
    VALUE vsvc, voffset, vbuf, vcsid, vcsfrm;
    OCISvcCtx *svchp;
    ub4 offset;
    ub2 csid;
    ub1 csfrm;
    ub4 amt;
    sword rv;

    rb_scan_args(argc, argv, "30", &vsvc, &voffset, &vbuf, &vcsid, &vcsfrm);
    svchp = oci8_get_oci_svcctx(vsvc); /* 1 */
    offset = NUM2INT(voffset); /* 2 */
    StringValue(vbuf);
    Get_Int_With_Default(argc, 4, vcsid, csid, 0); /* 4 */
    Get_Int_With_Default(argc, 5, vcsfrm, csfrm, SQLCS_IMPLICIT); /* 5 */

    amt = RSTRING(vbuf)->len;
    rv = OCILobWrite(svchp, oci8_errhp, lob->base.hp, &amt, offset, RSTRING(vbuf)->ptr, amt, OCI_ONE_PIECE, NULL, NULL, csid, csfrm);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return INT2FIX(amt);
}

static VALUE oci8_lob_trim(VALUE self, VALUE vsvc, VALUE len)
{
    oci8_lob_locator_t *lob = DATA_PTR(self);
    OCISvcCtx *svchp;
    sword rv;

    svchp = oci8_get_oci_svcctx(vsvc); /* 1 */

    rv = OCILobTrim(svchp, oci8_errhp, lob->base.hp, NUM2INT(len));
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return self;
}

static VALUE oci8_lob_clone(VALUE self, VALUE vsvc)
{
    oci8_lob_locator_t *lob = DATA_PTR(self);
    oci8_lob_locator_t *newlob;
    OCISvcCtx *svchp;
    VALUE newobj;
    sword rv;

    svchp = oci8_get_oci_svcctx(vsvc); /* 1 */

    newobj = rb_funcall(CLASS_OF(self), oci8_id_new, 0);
    newlob = DATA_PTR(newobj);
#ifdef HAVE_OCILOBLOCATORASSIGN
    /* Oracle 8.1 or upper */
    rv = OCILobLocatorAssign(svchp, oci8_errhp, lob->base.hp, (OCILobLocator**)&newlob->base.hp);
#else
    /* Oracle 8.0 */
    rv = OCILobAssign(oci8_envhp, oci8_errhp, lob->base.hp, (OCILobLocator**)&newlob->base.hp);
#endif
    if (rv != OCI_SUCCESS) {
        oci8_raise(oci8_errhp, rv, NULL);
    }
    return newobj;
}

#ifdef HAVE_OCILOBOPEN
static VALUE oci8_lob_open(VALUE self, VALUE vsvc)
{
    oci8_lob_locator_t *lob = DATA_PTR(self);
    OCISvcCtx *svchp;
    sword rv;

    svchp = oci8_get_oci_svcctx(vsvc); /* 1 */
  
    rv = OCILobOpen(svchp, oci8_errhp, lob->base.hp, OCI_DEFAULT);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return self;
}
#endif

#ifdef HAVE_OCILOBCLOSE
static VALUE oci8_lob_close(VALUE self, VALUE vsvc)
{
    oci8_lob_locator_t *lob = DATA_PTR(self);
    OCISvcCtx *svchp;
    sword rv;

    svchp = oci8_get_oci_svcctx(vsvc); /* 1 */
    rv = OCILobClose(svchp, oci8_errhp, lob->base.hp);
    if (rv != OCI_SUCCESS)
        oci8_raise(oci8_errhp, rv, NULL);
    return self;
}
#endif

/*
 * bind_clob/blob
 */
static void bind_lob_set(oci8_bind_t *base, VALUE val)
{
    oci8_bind_handle_t *handle = (oci8_bind_handle_t *)base;
    oci8_base_t *h;
    if (!rb_obj_is_instance_of(val, cOCILobLocator))
        rb_raise(rb_eArgError, "Invalid argument: %s (expect OCILobLocator)", rb_class2name(CLASS_OF(val)));
    h = DATA_PTR(val);
    handle->hp = h->hp;
    handle->obj = val;
}

static void bind_lob_init(oci8_bind_t *base, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_handle_t *handle = (oci8_bind_handle_t *)base;
    base->valuep = &handle->hp;
    base->value_sz = sizeof(handle->hp);
    if (NIL_P(*val)) {
        *val = rb_funcall(cOCILobLocator, oci8_id_new, 0);
    }
}

static oci8_bind_class_t bind_clob_class = {
    {
        oci8_bind_handle_mark,
	oci8_bind_free,
	sizeof(oci8_bind_handle_t)
    },
    oci8_bind_handle_get,
    bind_lob_set,
    bind_lob_init,
    SQLT_CLOB
};

static oci8_bind_class_t bind_blob_class = {
    {
        oci8_bind_handle_mark,
	oci8_bind_free,
	sizeof(oci8_bind_handle_t)
    },
    oci8_bind_handle_get,
    bind_lob_set,
    bind_lob_init,
    SQLT_BLOB
};

void Init_oci8_lob(void)
{
  cOCILobLocator = oci8_define_class("OCILobLocator", &oci8_lob_locator_class);
  rb_define_method(cOCILobLocator, "initialize", oci8_lob_initialize, 0);
  rb_define_method(cOCILobLocator, "char_width=", oci8_lob_set_char_width, 1);
  rb_define_method(cOCILobLocator, "is_initialized?", oci8_lob_is_initialized_p, 0);
  rb_define_method(cOCILobLocator, "getLength", oci8_lob_get_length, 1);
  rb_define_method(cOCILobLocator, "read", oci8_lob_read, -1);
  rb_define_method(cOCILobLocator, "write", oci8_lob_write, -1);
  rb_define_method(cOCILobLocator, "trim", oci8_lob_trim, 2);
  rb_define_method(cOCILobLocator, "clone", oci8_lob_clone, 1);
#ifdef HAVE_OCILOBOPEN
  rb_define_method(cOCILobLocator, "open", oci8_lob_open, 1);
#endif
#ifdef HAVE_OCILOBCLOSE
  rb_define_method(cOCILobLocator, "close", oci8_lob_close, 1);
#endif

  oci8_define_bind_class("CLOB", &bind_clob_class);
  oci8_define_bind_class("BLOB", &bind_blob_class);
}
