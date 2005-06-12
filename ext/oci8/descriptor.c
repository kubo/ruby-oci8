/*
  descriptor.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIDescriptor
The abstract class of OCI descriptors.
=end
*/
#include "oci8.h"

VALUE oci8_descriptor_do_initialize(VALUE self, ub4 type)
{
  sword rv;
  oci8_handle_t *envh;
  oci8_handle_t *h;

  Get_Handle(self, h);
  Check_Handle(oci8_env, OCIEnv, envh);
  rv = OCIDescriptorAlloc(envh->hp, &h->hp, type, 0, NULL);
  if (rv != OCI_SUCCESS)
    oci8_env_raise(envh->hp, rv);
  h->type = type;
  h->errhp = envh->errhp;
  oci8_link(envh, h);
  return Qnil;
}

static VALUE oci8_rowid_initialize(VALUE self)
{
  oci8_handle_t *h;

  Get_Handle(self, h);
  oci8_descriptor_do_initialize(self, OCI_DTYPE_ROWID);
  return Qnil;
}

/*
 * bind_rowid
 */
static VALUE bind_rowid_get(oci8_bind_handle_t *bh)
{
  return bh->obj;
}

static void bind_rowid_set(oci8_bind_handle_t *bh, VALUE val)
{
  oci8_handle_t *h;
  if (!rb_obj_is_instance_of(val, cOCIRowid))
    rb_raise(rb_eArgError, "Invalid argument: %s (expect OCIRowid)", rb_class2name(CLASS_OF(val)));
  h = DATA_PTR(val);
  bh->obj = val;
  bh->value.hp = h->hp;
}

static void bind_rowid_init(oci8_bind_handle_t *bh, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
  bh->valuep = &bh->value.hp;
  bh->value_sz = sizeof(bh->value.hp);
  if (NIL_P(*val)) {
    *val = rb_funcall(cOCIRowid, oci8_id_new, 0);
  }
}

static oci8_bind_type_t bind_rowid = {
  bind_rowid_get,
  bind_rowid_set,
  bind_rowid_init,
  NULL,
  SQLT_RDD,
};

void Init_oci8_descriptor(void)
{
  rb_define_method(cOCIDescriptor, "free", oci8_handle_free, 0);
  rb_define_method(cOCIRowid, "initialize", oci8_rowid_initialize, 0);

  oci8_register_bind_type("OCIRowid", &bind_rowid);
}
