/*
  descriptor.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>
*/
#include "oci8.h"

static VALUE cOCIRowid;

typedef struct {
    oci8_base_t base;
} oci8_rowid_t;

static oci8_base_class_t oci8_rowid_class = {
    NULL,
    NULL,
    sizeof(oci8_rowid_t),
};

static VALUE oci8_rowid_initialize(VALUE self)
{
    oci8_rowid_t *rowid = DATA_PTR(self);
    sword rv;

    rv = OCIDescriptorAlloc(oci8_envhp, &rowid->base.hp, OCI_DTYPE_ROWID, 0, NULL);
    if (rv != OCI_SUCCESS)
        oci8_env_raise(oci8_envhp, rv);
    rowid->base.type = OCI_DTYPE_ROWID;
    return Qnil;
}

/*
 * bind_rowid
 */
static void bind_rowid_set(oci8_bind_t *base, VALUE val)
{
    oci8_bind_handle_t *handle = (oci8_bind_handle_t *)base;
    oci8_base_t *h;
    if (!rb_obj_is_instance_of(val, cOCIRowid))
      rb_raise(rb_eArgError, "Invalid argument: %s (expect OCIRowid)", rb_class2name(CLASS_OF(val)));
    h = DATA_PTR(val);
    handle->hp = h->hp;
    handle->obj = val;
}

static void bind_rowid_init(oci8_bind_t *base, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    oci8_bind_handle_t *handle = (oci8_bind_handle_t *)base;
    base->valuep = &handle->hp;
    base->value_sz = sizeof(handle->hp);
    if (NIL_P(*val)) {
        *val = rb_funcall(cOCIRowid, oci8_id_new, 0);
    }
}

static oci8_bind_class_t bind_rowid_class = {
    {
        oci8_bind_handle_mark,
	oci8_bind_free,
	sizeof(oci8_bind_handle_t)
    },
    oci8_bind_handle_get,
    bind_rowid_set,
    bind_rowid_init,
    SQLT_RDD
};

void Init_oci8_rowid(void)
{
    cOCIRowid = oci8_define_class("OCIRowid", &oci8_rowid_class);
    rb_define_method(cOCIRowid, "initialize", oci8_rowid_initialize, 0);
    oci8_define_bind_class("OCIRowid", &bind_rowid_class);
}

VALUE oci8_get_rowid_attr(oci8_base_t *base, ub4 attrtype)
{
    oci8_base_t *rowid;
    VALUE obj;
    sword rv;

    obj = rb_funcall(cOCIRowid, oci8_id_new, 0);
    rowid = DATA_PTR(obj);
    rv = OCIAttrGet(base->hp, base->type, rowid->hp, 0, attrtype, oci8_errhp);
    if (rv != OCI_SUCCESS) {
        oci8_base_free(rowid);
        oci8_raise(oci8_errhp, rv, NULL);
    }
    return obj;
}
