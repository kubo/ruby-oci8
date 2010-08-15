/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * ocihandle.c
 *
 * Copyright (C) 2009 KUBO Takehiro <kubo@jiubao.org>
 *
 * implement OCIHandle
 *
 */
#include "oci8.h"

#ifdef _MSC_VER
#define MAGIC_NUMBER 0xDEAFBEAFDEAFBEAFui64;
#else
#define MAGIC_NUMBER 0xDEAFBEAFDEAFBEAFull;
#endif

static long check_data_range(VALUE val, long min, long max, const char *type)
{
    long lval = NUM2LONG(val);
    if (lval < min || max < lval) {
        rb_raise(rb_eRangeError, "integer %ld too %s to convert to `%s'",
                 lval, lval < 0 ? "small" : "big", type);
    }
    return lval;
}


static oci8_base_class_t oci8_base_class = {
    NULL,
    NULL,
    sizeof(oci8_base_t),
};

static VALUE oci8_handle_initialize(VALUE self)
{
    rb_raise(rb_eNameError, "private method `new' called for %s:Class", rb_class2name(CLASS_OF(self)));
}

/*
 * call-seq:
 *   free()
 *
 * <b>(new in 2.0.0)</b>
 *
 * Clears the object internal structure and its dependents.
 */
static VALUE oci8_handle_free(VALUE self)
{
    oci8_base_t *base = DATA_PTR(self);

    oci8_base_free(base);
    return self;
}

static void oci8_handle_mark(oci8_base_t *base)
{
    if (base->klass->mark != NULL)
        base->klass->mark(base);
}

static void oci8_handle_cleanup(oci8_base_t *base)
{
    if (oci8_in_finalizer) {
        /* Do nothing when the program exits.
         * The first two words of memory addressed by VALUE datatype is
         * changed in finalizer. If a ruby function which access it such
         * as rb_obj_is_kind_of is called, it may cause SEGV.
         */
        return;
    }
    oci8_base_free(base);
    xfree(base);
}

static VALUE oci8_s_allocate(VALUE klass)
{
    oci8_base_t *base;
    const oci8_base_class_t *base_class;
    VALUE superklass;
    VALUE obj;

    superklass = klass;
    while (!RTEST(rb_ivar_defined(superklass, oci8_id_oci8_class))) {
        superklass = RCLASS_SUPER(superklass);
        if (superklass == rb_cObject)
            rb_raise(rb_eRuntimeError, "private method `new' called for %s:Class", rb_class2name(klass));
    }
    obj = rb_ivar_get(superklass, oci8_id_oci8_class);
    base_class = DATA_PTR(obj);

    base = xmalloc(base_class->size);
    memset(base, 0, base_class->size);

    obj = Data_Wrap_Struct(klass, oci8_handle_mark, oci8_handle_cleanup, base);
    base->self = obj;
    base->klass = base_class;
    base->parent = NULL;
    base->next = base;
    base->prev = base;
    base->children = NULL;
    return obj;
}

/*
 * call-seq:
 *   attr_get_ub1(attr_type) -> fixnum
 *
 * <b>(new in 2.0.4)</b>
 *
 * Gets the value of an attribute as `ub1' datatype.
 */
static VALUE attr_get_ub1(VALUE self, VALUE attr_type)
{
    oci8_base_t *base = DATA_PTR(self);
    union {
        ub1 value;
        ub8 dummy; /* padding for incorrect attrtype to protect the stack */
    } v;

    v.dummy = MAGIC_NUMBER;
    Check_Type(attr_type, T_FIXNUM);
    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &v.value, NULL, FIX2INT(attr_type), oci8_errhp));
    return INT2FIX(v.value);
}

/*
 * call-seq:
 *   attr_get_ub2(attr_type) -> fixnum
 *
 * <b>(new in 2.0.4)</b>
 *
 * Gets the value of an attribute as `ub2' datatype.
 */
static VALUE attr_get_ub2(VALUE self, VALUE attr_type)
{
    oci8_base_t *base = DATA_PTR(self);
    union {
        ub2 value;
        ub8 dummy; /* padding for incorrect attrtype to protect the stack */
    } v;

    v.dummy = MAGIC_NUMBER;
    Check_Type(attr_type, T_FIXNUM);
    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &v.value, NULL, FIX2INT(attr_type), oci8_errhp));
    return INT2FIX(v.value);
}

/*
 * call-seq:
 *   attr_get_ub4(attr_type) -> integer
 *
 * <b>(new in 2.0.4)</b>
 *
 * Gets the value of an attribute as `ub4' datatype.
 */
static VALUE attr_get_ub4(VALUE self, VALUE attr_type)
{
    oci8_base_t *base = DATA_PTR(self);
    union {
        ub4 value;
        ub8 dummy; /* padding for incorrect attrtype to protect the stack */
    } v;

    v.dummy = MAGIC_NUMBER;
    Check_Type(attr_type, T_FIXNUM);
    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &v.value, NULL, FIX2INT(attr_type), oci8_errhp));
    return UINT2NUM(v.value);
}

/*
 * call-seq:
 *   attr_get_ub8(attr_type) -> integer
 *
 * <b>(new in 2.0.4)</b>
 *
 * Gets the value of an attribute as `ub8' datatype.
 */
static VALUE attr_get_ub8(VALUE self, VALUE attr_type)
{
    oci8_base_t *base = DATA_PTR(self);
    union {
        ub8 value;
        ub8 dummy;
    } v;

    v.dummy = MAGIC_NUMBER;
    Check_Type(attr_type, T_FIXNUM);
    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &v.value, NULL, FIX2INT(attr_type), oci8_errhp));
    return ULL2NUM(v.value);
}

/*
 * call-seq:
 *   attr_get_sb1(attr_type) -> fixnum
 *
 * <b>(new in 2.0.4)</b>
 *
 * Gets the value of an attribute as `sb1' datatype.
 */
static VALUE attr_get_sb1(VALUE self, VALUE attr_type)
{
    oci8_base_t *base = DATA_PTR(self);
    union {
        sb1 value;
        ub8 dummy; /* padding for incorrect attrtype to protect the stack */
    } v;

    v.dummy = MAGIC_NUMBER;
    Check_Type(attr_type, T_FIXNUM);
    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &v.value, NULL, FIX2INT(attr_type), oci8_errhp));
    return INT2FIX(v.value);
}

/*
 * call-seq:
 *   attr_get_sb2(attr_type) -> fixnum
 *
 * <b>(new in 2.0.4)</b>
 *
 * Gets the value of an attribute as `sb2' datatype.
 */
static VALUE attr_get_sb2(VALUE self, VALUE attr_type)
{
    oci8_base_t *base = DATA_PTR(self);
    union {
        sb2 value;
        ub8 dummy; /* padding for incorrect attrtype to protect the stack */
    } v;

    v.dummy = MAGIC_NUMBER;
    Check_Type(attr_type, T_FIXNUM);
    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &v.value, NULL, FIX2INT(attr_type), oci8_errhp));
    return INT2FIX(v.value);
}

/*
 * call-seq:
 *   attr_get_sb4(attr_type) -> integer
 *
 * <b>(new in 2.0.4)</b>
 *
 * Gets the value of an attribute as `sb4' datatype.
 */
static VALUE attr_get_sb4(VALUE self, VALUE attr_type)
{
    oci8_base_t *base = DATA_PTR(self);
    union {
        sb4 value;
        ub8 dummy;
    } v;

    v.dummy = MAGIC_NUMBER;
    Check_Type(attr_type, T_FIXNUM);
    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &v.value, NULL, FIX2INT(attr_type), oci8_errhp));
    return INT2NUM(v.value);
}

/*
 * call-seq:
 *   attr_get_sb8(attr_type) -> integer
 *
 * <b>(new in 2.0.4)</b>
 *
 * Gets the value of an attribute as `sb8' datatype.
 */
static VALUE attr_get_sb8(VALUE self, VALUE attr_type)
{
    oci8_base_t *base = DATA_PTR(self);
    union {
        sb8 value;
        ub8 dummy; /* padding for incorrect attrtype to protect the stack */
    } v;

    v.dummy = MAGIC_NUMBER;
    Check_Type(attr_type, T_FIXNUM);
    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &v.value, NULL, FIX2INT(attr_type), oci8_errhp));
    return LL2NUM(v.value);
}

/*
 * call-seq:
 *   attr_get_boolean(attr_type) -> true or false
 *
 * <b>(new in 2.0.4)</b>
 *
 * Gets the value of an attribute as `boolean' datatype.
 */
static VALUE attr_get_boolean(VALUE self, VALUE attr_type)
{
    oci8_base_t *base = DATA_PTR(self);
    union {
        boolean value;
        ub8 dummy; /* padding for incorrect attrtype to protect the stack */
    } v;

    v.dummy = MAGIC_NUMBER;
    Check_Type(attr_type, T_FIXNUM);
    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &v.value, NULL, FIX2INT(attr_type), oci8_errhp));
    return v.value ? Qtrue : Qfalse;
}

/*
 * call-seq:
 *   attr_get_string(attr_type) -> string
 *
 * <b>(new in 2.0.4)</b>
 *
 * Gets the value of an attribute as `oratext *' datatype.
 * The return value is converted to Encoding.default_internal or
 * tagged with OCI8.encoding when the ruby version is 1.9.
 *
 * <b>Caution:</b> If the specified attr_type's datatype is not a
 * pointer type, it causes a segmentation fault.
 */
static VALUE attr_get_string(VALUE self, VALUE attr_type)
{
    oci8_base_t *base = DATA_PTR(self);
    union {
        char *value;
        ub8 dummy; /* padding for incorrect attrtype to protect the stack */
    } v;
    ub4 size = 0;

    v.dummy = MAGIC_NUMBER;
    Check_Type(attr_type, T_FIXNUM);
    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &v.value, &size, FIX2INT(attr_type), oci8_errhp));
    return rb_external_str_new_with_enc(v.value, size, oci8_encoding);
}

/*
 * call-seq:
 *   attr_get_binary(attr_type) -> string
 *
 * <b>(new in 2.0.4)</b>
 *
 * Gets the value of an attribute as `ub1 *' datatype.
 * The return value is tagged with ASCII-8BIT when the ruby version is 1.9.
 *
 * <b>Caution:</b> If the specified attr_type's datatype is not a
 * pointer type, it causes a segmentation fault.
 */
static VALUE attr_get_binary(VALUE self, VALUE attr_type)
{
    oci8_base_t *base = DATA_PTR(self);
    union {
        char *value;
        ub8 dummy; /* padding for incorrect attrtype to protect the stack */
    } v;
    ub4 size = 0;

    v.dummy = 0;
    Check_Type(attr_type, T_FIXNUM);
    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &v.value, &size, FIX2INT(attr_type), oci8_errhp));
    return rb_tainted_str_new(v.value, size);
}

/*
 * call-seq:
 *   attr_get_integer(attr_type) -> integer
 *
 * <b>(new in 2.0.4)</b>
 *
 * Gets the value of an attribute as `ub1 *' datatype.
 * The return value is converted to Integer from internal Oracle NUMBER format.
 *
 * <b>Caution:</b> If the specified attr_type's datatype is not a
 * pointer type, it causes a segmentation fault.
 */
static VALUE attr_get_integer(VALUE self, VALUE attr_type)
{
    oci8_base_t *base = DATA_PTR(self);
    union {
        OCINumber *value;
        ub8 dummy; /* padding for incorrect attrtype to protect the stack */
    } v;
    ub4 size = 0;

    v.dummy = 0;
    Check_Type(attr_type, T_FIXNUM);
    oci_lc(OCIAttrGet(base->hp.ptr, base->type, &v.value, &size, FIX2INT(attr_type), oci8_errhp));
    return oci8_make_integer(v.value, oci8_errhp);
}

/*
 * call-seq:
 *   attr_set_ub1(attr_type, attr_value)
 *
 * <b>(new in 2.0.4)</b>
 *
 * Sets the value of an attribute as `ub1' datatype.
 *
 * <b>Caution:</b> If the specified attr_type's datatype is a
 * pointer type, it causes a segmentation fault.
 */
static VALUE attr_set_ub1(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    ub1 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = (ub1)check_data_range(val, 0, UCHAR_MAX, "ub1");
    /* set attribute */
    oci_lc(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp));
    return self;
}

/*
 * call-seq:
 *   attr_set_ub2(attr_type, attr_value)
 *
 * <b>(new in 2.0.4)</b>
 *
 * Sets the value of an attribute as `ub2' datatype.
 *
 * <b>Caution:</b> If the specified attr_type's datatype is a
 * pointer type, it causes a segmentation fault.
 */
static VALUE attr_set_ub2(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    ub2 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = (ub2)check_data_range(val, 0, USHRT_MAX, "ub2");
    /* set attribute */
    oci_lc(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp));
    return self;
}

/*
 * call-seq:
 *   attr_set_ub4(attr_type, attr_value)
 *
 * <b>(new in 2.0.4)</b>
 *
 * Sets the value of an attribute as `ub4' datatype.
 *
 * <b>Caution:</b> If the specified attr_type's datatype is a
 * pointer type, it causes a segmentation fault.
 */
static VALUE attr_set_ub4(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    ub4 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = NUM2UINT(val);
    /* set attribute */
    oci_lc(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp));
    return self;
}

/*
 * call-seq:
 *   attr_set_ub8(attr_type, attr_value)
 *
 * <b>(new in 2.0.4)</b>
 *
 * Sets the value of an attribute as `ub8' datatype.
 *
 * <b>Caution:</b> If the specified attr_type's datatype is a
 * pointer type, it causes a segmentation fault.
 */
static VALUE attr_set_ub8(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    ub8 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = NUM2ULL(val);
    /* set attribute */
    oci_lc(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp));
    return self;
}

/*
 * call-seq:
 *   attr_set_sb1(attr_type, attr_value)
 *
 * <b>(new in 2.0.4)</b>
 *
 * Sets the value of an attribute as `sb1' datatype.
 *
 * <b>Caution:</b> If the specified attr_type's datatype is a
 * pointer type, it causes a segmentation fault.
 */
static VALUE attr_set_sb1(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    sb1 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = (sb1)check_data_range(val, CHAR_MIN, CHAR_MAX, "sb1");
    /* set attribute */
    oci_lc(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp));
    return self;
}

/*
 * call-seq:
 *   attr_set_sb2(attr_type, attr_value)
 *
 * <b>(new in 2.0.4)</b>
 *
 * Sets the value of an attribute as `sb2' datatype.
 *
 * <b>Caution:</b> If the specified attr_type's datatype is a
 * pointer type, it causes a segmentation fault.
 */
static VALUE attr_set_sb2(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    sb2 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = (sb2)check_data_range(val, SHRT_MIN, SHRT_MAX, "sb2");
    /* set attribute */
    oci_lc(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp));
    return self;
}

/*
 * call-seq:
 *   attr_set_sb4(attr_type, attr_value)
 *
 * <b>(new in 2.0.4)</b>
 *
 * Sets the value of an attribute as `sb4' datatype.
 *
 * <b>Caution:</b> If the specified attr_type's datatype is a
 * pointer type, it causes a segmentation fault.
 */
static VALUE attr_set_sb4(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    sb4 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = NUM2INT(val);
    /* set attribute */
    oci_lc(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp));
    return self;
}

/*
 * call-seq:
 *   attr_set_sb8(attr_type, attr_value)
 *
 * <b>(new in 2.0.4)</b>
 *
 * Sets the value of an attribute as `sb8' datatype.
 *
 * <b>Caution:</b> If the specified attr_type's datatype is a
 * pointer type, it causes a segmentation fault.
 */
static VALUE attr_set_sb8(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    sb8 value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = NUM2LL(val);
    /* set attribute */
    oci_lc(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp));
    return self;
}

/*
 * call-seq:
 *   attr_set_boolean(attr_type, attr_value)
 *
 * <b>(new in 2.0.4)</b>
 *
 * Sets the value of an attribute as `boolean' datatype.
 *
 * <b>Caution:</b> If the specified attr_type's datatype is a
 * pointer type, it causes a segmentation fault.
 */
static VALUE attr_set_boolean(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    boolean value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    value = RTEST(val) ? TRUE : FALSE;
    /* set attribute */
    oci_lc(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp));
    return self;
}

/*
 * call-seq:
 *   attr_set_string(attr_type, string_value)
 *
 * <b>(new in 2.0.4)</b>
 *
 * Sets the value of an attribute as `oratext *' datatype.
 * +string_value+ is converted to OCI8.encoding before it is set
 * when the ruby version is 1.9.
 */
static VALUE attr_set_string(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    OCI8SafeStringValue(val);
    /* set attribute */
    oci_lc(OCIAttrSet(base->hp.ptr, base->type, RSTRING_PTR(val), RSTRING_LEN(val), FIX2INT(attr_type), oci8_errhp));
    return self;
}

/*
 * call-seq:
 *   attr_set_binary(attr_type, string_value)
 *
 * <b>(new in 2.0.4)</b>
 *
 * Sets the value of an attribute as `ub1 *' datatype.
 */
static VALUE attr_set_binary(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    SafeStringValue(val);
    /* set attribute */
    oci_lc(OCIAttrSet(base->hp.ptr, base->type, RSTRING_PTR(val), RSTRING_LEN(val), FIX2INT(attr_type), oci8_errhp));
    return self;
}

/*
 * call-seq:
 *   attr_set_integer(attr_type, number)
 *
 * <b>(new in 2.0.4)</b>
 *
 * Sets the value of an attribute as `ub1 *' datatype.
 * +number+ is converted to internal Oracle NUMBER format before
 * it is set.
 */
static VALUE attr_set_integer(VALUE self, VALUE attr_type, VALUE val)
{
    oci8_base_t *base = DATA_PTR(self);
    OCINumber value;

    /* validate arguments */
    Check_Type(attr_type, T_FIXNUM);
    oci8_set_integer(&value, val, oci8_errhp);
    /* set attribute */
    oci_lc(OCIAttrSet(base->hp.ptr, base->type, &value, sizeof(value), FIX2INT(attr_type), oci8_errhp));
    return self;
}

void Init_oci8_handle(void)
{
    VALUE obj;

    /*
     * <b>(new in 2.0.0)</b>
     *
     * OCIHandle is the abstract base class of OCI handles and
     * OCI descriptors; opaque data types of Oracle Call Interface.
     */
    oci8_cOCIHandle = rb_define_class("OCIHandle", rb_cObject);
    rb_define_alloc_func(oci8_cOCIHandle, oci8_s_allocate);
    rb_define_method_nodoc(oci8_cOCIHandle, "initialize", oci8_handle_initialize, 0);
    rb_define_private_method(oci8_cOCIHandle, "free", oci8_handle_free, 0);
    obj = Data_Wrap_Struct(rb_cObject, 0, 0, &oci8_base_class);
    rb_ivar_set(oci8_cOCIHandle, oci8_id_oci8_class, obj);

    /* methods to get attributes */
    rb_define_private_method(oci8_cOCIHandle, "attr_get_ub1", attr_get_ub1, 1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_ub2", attr_get_ub2, 1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_ub4", attr_get_ub4, 1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_ub8", attr_get_ub8, 1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_sb1", attr_get_sb1, 1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_sb2", attr_get_sb2, 1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_sb4", attr_get_sb4, 1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_sb8", attr_get_sb8, 1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_boolean", attr_get_boolean, 1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_string", attr_get_string, 1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_binary", attr_get_binary, 1);
    rb_define_private_method(oci8_cOCIHandle, "attr_get_integer", attr_get_integer, 1);

    /* methods to set attributes */
    rb_define_private_method(oci8_cOCIHandle, "attr_set_ub1", attr_set_ub1, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_ub2", attr_set_ub2, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_ub4", attr_set_ub4, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_ub8", attr_set_ub8, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_sb1", attr_set_sb1, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_sb2", attr_set_sb2, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_sb4", attr_set_sb4, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_sb8", attr_set_sb8, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_boolean", attr_set_boolean, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_string", attr_set_string, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_binary", attr_set_binary, 2);
    rb_define_private_method(oci8_cOCIHandle, "attr_set_integer", attr_set_integer, 2);
}
