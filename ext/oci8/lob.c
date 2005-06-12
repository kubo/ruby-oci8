#include "oci8.h"

static VALUE oci8_lob_initialize(VALUE self)
{
  oci8_handle_t *h;

  Get_Handle(self, h);
  oci8_descriptor_do_initialize(self, OCI_DTYPE_LOB);
  h->u.lob_locator.char_width = 1;
  return Qnil;
}

static VALUE oci8_lob_set_char_width(VALUE self, VALUE vsize)
{
  oci8_handle_t *h;
  int size;

  Get_Handle(self, h); /* 0 */
  size = NUM2INT(vsize); /* 1 */

  if (size <= 0)
    rb_raise(rb_eArgError, "size must be more than one.");
  h->u.lob_locator.char_width = size;
  return vsize;
}

static VALUE oci8_lob_is_initialized_p(VALUE self)
{
  oci8_handle_t *h;
  boolean is_initialized;
  sword rv;

  Get_Handle(self, h); /* 0 */

  rv = OCILobLocatorIsInit(h->envh->hp, h->errhp, h->hp, &is_initialized);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
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
  oci8_handle_t *h;
  oci8_handle_t *svch;
  ub4 len;
  sword rv;

  Get_Handle(self, h); /* 0 */
  Check_Handle(vsvc, OCISvcCtx, svch); /* 1 */

  rv = OCILobGetLength(svch->hp, h->errhp, h->hp, &len);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return INT2FIX(len);
}

static VALUE oci8_lob_read(int argc, VALUE *argv, VALUE self)
{
  VALUE vsvc, voffset, vamt, vcsid, vcsfrm;
  oci8_handle_t *h;
  oci8_handle_t *svch;
  ub4 offset;
  ub2 csid;
  ub1 csfrm;
  ub4 amt;
  sword rv;
  char buf[4096];
  size_t buf_size_in_char;
  VALUE v = Qnil;

  rb_scan_args(argc, argv, "32", &vsvc, &voffset, &vamt, &vcsid, &vcsfrm);
  Get_Handle(self, h); /* 0 */
  Check_Handle(vsvc, OCISvcCtx, svch); /* 1 */
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
  buf_size_in_char = sizeof(buf) / h->u.lob_locator.char_width;
  do {
    rv = OCILobRead(svch->hp, h->errhp, h->hp, &amt, offset, buf, sizeof(buf), NULL, NULL, 0, SQLCS_IMPLICIT);
    if (rv != OCI_SUCCESS && rv != OCI_NEED_DATA)
      oci8_raise(h->errhp, rv, NULL);
    if (amt == 0)
      break;
    /* for fixed size charset, amt is the number of characters stored in buf. */
    if (amt > buf_size_in_char)
      rb_raise(eOCIException, "Too large buffer fetched or you set too large size of a character.");
    amt *= h->u.lob_locator.char_width;
    if (v == Qnil)
      v = rb_str_new(buf, amt);
    else
      v = rb_str_cat(v, buf, amt);
  } while (rv == OCI_NEED_DATA);
  return v;
}

static VALUE oci8_lob_write(int argc, VALUE *argv, VALUE self)
{
  VALUE vsvc, voffset, vbuf, vcsid, vcsfrm;
  oci8_handle_t *h;
  oci8_handle_t *svch;
  oci8_string_t buf;
  ub4 offset;
  ub2 csid;
  ub1 csfrm;
  ub4 amt;
  sword rv;

  rb_scan_args(argc, argv, "30", &vsvc, &voffset, &vbuf, &vcsid, &vcsfrm);
  Get_Handle(self, h); /* 0 */
  Check_Handle(vsvc, OCISvcCtx, svch); /* 1 */
  offset = NUM2INT(voffset); /* 2 */
  Get_String(vbuf, buf); /* 3 */
  Get_Int_With_Default(argc, 4, vcsid, csid, 0); /* 4 */
  Get_Int_With_Default(argc, 5, vcsfrm, csfrm, SQLCS_IMPLICIT); /* 5 */

  amt = buf.len;
  rv = OCILobWrite(svch->hp, h->errhp, h->hp, &amt, offset, buf.ptr, buf.len, OCI_ONE_PIECE, NULL, NULL, csid, csfrm);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return INT2FIX(amt);
}

static VALUE oci8_lob_trim(VALUE self, VALUE vsvc, VALUE len)
{
  oci8_handle_t *h;
  oci8_handle_t *svch;
  sword rv;

  Get_Handle(self, h); /* 0 */
  Check_Handle(vsvc, OCISvcCtx, svch); /* 1 */

  rv = OCILobTrim(svch->hp, h->errhp, h->hp, NUM2INT(len));
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return self;
}

static VALUE oci8_lob_clone(VALUE self, VALUE vsvc)
{
  oci8_handle_t *h;
  oci8_handle_t *svch;
  oci8_handle_t *newh;
  OCILobLocator *hp;
  sword rv;

  Get_Handle(self, h); /* 0 */
  Check_Handle(vsvc, OCISvcCtx, svch); /* 1 */

  rv = OCIDescriptorAlloc(h->envh->hp, (void *)&hp, OCI_DTYPE_LOB, 0, NULL);
  if (rv != OCI_SUCCESS) {
    oci8_env_raise(h->envh->hp, rv);
  }
#ifdef HAVE_OCILOBLOCATORASSIGN
  /* Oracle 8.1 or upper */
  rv = OCILobLocatorAssign(svch->hp, h->errhp, h->hp, &hp);
#else
  /* Oracle 8.0 */
  rv = OCILobAssign(h->envh->hp, h->errhp, h->hp, &hp);
#endif
  if (rv != OCI_SUCCESS) {
    oci8_raise(h->errhp, rv, NULL);
  }
  newh = oci8_make_handle(OCI_DTYPE_LOB, hp, h->errhp, h->parent, 0);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return newh->self;
}

#ifdef HAVE_OCILOBOPEN
static VALUE oci8_lob_open(VALUE self, VALUE vsvc)
{
  oci8_handle_t *h;
  oci8_handle_t *svch;
  sword rv;

  Get_Handle(self, h); /* 0 */
  Check_Handle(vsvc, OCISvcCtx, svch); /* 1 */
  
  rv = OCILobOpen(svch->hp, h->errhp, h->hp, OCI_DEFAULT);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return self;
}
#endif

#ifdef HAVE_OCILOBCLOSE
static VALUE oci8_lob_close(VALUE self, VALUE vsvc)
{
  oci8_handle_t *h;
  oci8_handle_t *svch;
  sword rv;

  Get_Handle(self, h); /* 0 */
  Check_Handle(vsvc, OCISvcCtx, svch); /* 1 */
  
  rv = OCILobClose(svch->hp, h->errhp, h->hp);
  if (rv != OCI_SUCCESS)
    oci8_raise(h->errhp, rv, NULL);
  return self;
}
#endif

/*
 * bind_clob
 */
static VALUE bind_lob_get(oci8_bind_handle_t *bh)
{
  return bh->obj;
}

static void bind_clob_set(oci8_bind_handle_t *bh, VALUE val)
{
  oci8_handle_t *h;
  if (!rb_obj_is_instance_of(val, cOCILobLocator))
    rb_raise(rb_eArgError, "Invalid argument: %s (expect OCILobLocator)", rb_class2name(CLASS_OF(val)));
  h = DATA_PTR(val);
  bh->obj = val;
  bh->value.hp = h->hp;
}

static void bind_lob_init(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
  bh->valuep = &bh->value.hp;
  bh->value_sz = sizeof(bh->value.hp);
  if (NIL_P(*val)) {
    *val = rb_funcall(cOCILobLocator, oci8_id_new, 0);
  }
}

static oci8_bind_type_t bind_clob = {
  bind_lob_get,
  bind_clob_set,
  bind_lob_init,
  NULL,
  SQLT_CLOB,
};

/*
 * bind_blob
 */
static void bind_blob_set(oci8_bind_handle_t *bh, VALUE val)
{
  oci8_handle_t *h;
  if (!rb_obj_is_instance_of(val, cOCILobLocator))
    rb_raise(rb_eArgError, "Invalid argument: %s (expect OCILobLocator)", rb_class2name(CLASS_OF(val)));
  h = DATA_PTR(val);
  bh->obj = val;
  bh->value.hp = h->hp;
}

static oci8_bind_type_t bind_blob = {
  bind_lob_get,
  bind_blob_set,
  bind_lob_init,
  NULL,
  SQLT_BLOB,
};

void Init_oci8_lob(void)
{
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

  oci8_register_bind_type("CLOB", &bind_clob);
  oci8_register_bind_type("BLOB", &bind_blob);
}
