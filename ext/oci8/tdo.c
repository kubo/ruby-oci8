/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
/*
 * Document-class: OCI8::TDO
 *
 * OCI8::TDO is the class for Type Descriptor Object, which describe
 * Oracle's object type.
 *
 * An instance of OCI8::TDO is specific to the connection. This means
 * One TDO instance for a connection is not available to another connection.
 */
#include "oci8.h"
#include <orid.h>

static VALUE cOCITDO;
static VALUE mapping;
static ID id_at_type_name;
static ID id_at_schema_name;
static ID id_at_attrs;
static ID id_at_attr_syms;
static ID id_at_types;
static ID id_at_class;
static ID id_at_con;

typedef struct {
    oci8_base_t base;
    oci8_svcctx_t *svcctx;
    OCIDescribe *deschp;
} oci8_tdo_t;

static void oci8_tdo_free(oci8_base_t *base)
{
    oci8_tdo_t *tdo = (oci8_tdo_t*)base;
    if (base->hp != NULL) {
        OCIObjectUnpin(oci8_envhp, oci8_errhp, base->hp);
        base->hp = NULL;
    }
    if (tdo->deschp != NULL) {
        OCIHandleFree(tdo->deschp, OCI_HTYPE_DESCRIBE);
        tdo->deschp = NULL;
    }
}

static oci8_base_class_t oci8_tdo_class = {
    NULL,
    oci8_tdo_free,
    sizeof(oci8_tdo_t),
};

static VALUE oci8_tdo_init(VALUE self, oci8_svcctx_t *svcctx, OCIParam *param)
{
    oci8_tdo_t *tdo = DATA_PTR(self);
    OCIRef *tdo_ref;
    ub2 num_type_attrs;
    OCIParam *lparam;
    VALUE schema_name;
    VALUE type_name;
    VALUE klass;
    VALUE attrs;
    VALUE attr_syms;
    VALUE types;
    ub2 idx;
    ub4 name_max = 0;
    char *name;
    OraText *str;
    ub4 strlen;

    tdo->svcctx = svcctx;
    /* get tdo */
    oci_lc(OCIAttrGet(param, OCI_DTYPE_PARAM, (dvoid *)&tdo_ref, 0, OCI_ATTR_REF_TDO, oci8_errhp));
    oci_lc(OCIObjectPin(oci8_envhp, oci8_errhp, tdo_ref, 0, OCI_PIN_ANY, OCI_DURATION_SESSION, OCI_LOCK_NONE, (dvoid**)&tdo->base.hp));
    /* get schema name */
    oci_lc(OCIAttrGet(param, OCI_DTYPE_PARAM, &str, &strlen, OCI_ATTR_SCHEMA_NAME, oci8_errhp));
    schema_name = rb_str_new(str, strlen);

    /* get type name */
    oci_lc(OCIAttrGet(param, OCI_DTYPE_PARAM, &str, &strlen, OCI_ATTR_NAME, oci8_errhp));
    type_name = rb_str_new(str, strlen);

    /* determine klass */
    klass = rb_hash_aref(mapping, type_name);
    if (NIL_P(klass)) {
        klass = rb_cObject;
    }

    /* get attribute names */
    oci_lc(OCIAttrGet(param, OCI_DTYPE_PARAM, &num_type_attrs, 0, OCI_ATTR_NUM_TYPE_ATTRS, oci8_errhp));
    oci_lc(OCIAttrGet(param, OCI_DTYPE_PARAM, &lparam, 0, OCI_ATTR_LIST_TYPE_ATTRS, oci8_errhp));
    attrs = rb_ary_new2(num_type_attrs);
    attr_syms = rb_ary_new2(num_type_attrs);
    types = rb_ary_new2(num_type_attrs);

    for (idx = 1; idx <= num_type_attrs; idx++) {
        OCIParam *param;
        text *text;
        ub4 size;
        ub2 typecode;
        VALUE name_obj;
        VALUE type_obj;

        oci_lc(OCIParamGet(lparam, OCI_DTYPE_PARAM, oci8_errhp, (dvoid *)&param, idx));

        /* attribute name */
        oci_lc(OCIAttrGet(param, OCI_DTYPE_PARAM, &text, &size, OCI_ATTR_NAME, oci8_errhp));
        name_obj = rb_str_new(text, size);
        if (name_max < size)
            name_max = size;

        /* attribyte type */
        oci_lc(OCIAttrGet(param, OCI_DTYPE_PARAM, &typecode, 0, OCI_ATTR_TYPECODE, oci8_errhp));
        switch (typecode) {
            oci8_tdo_t *tdo;
            sword rv;
            OCIRef *ref;
        case OCI_TYPECODE_OBJECT:
        case OCI_TYPECODE_OPAQUE:
            type_obj = rb_obj_alloc(CLASS_OF(self));
            tdo = DATA_PTR(type_obj);
            rv = OCIHandleAlloc(oci8_envhp, (void*)&tdo->deschp, OCI_HTYPE_DESCRIBE, 0, 0);
            if (rv != OCI_SUCCESS)
                oci8_env_raise(oci8_envhp, rv);
            ref = NULL;
            oci_lc(OCIAttrGet(param, OCI_DTYPE_PARAM, &ref, NULL, OCI_ATTR_REF_TDO, oci8_errhp));
            oci_rc(svcctx, OCIDescribeAny(svcctx->base.hp, oci8_errhp, ref, 0,
                                          OCI_OTYPE_REF, OCI_DEFAULT, OCI_PTYPE_TYPE, tdo->deschp));
            oci_lc(OCIAttrGet(tdo->deschp, OCI_HTYPE_DESCRIBE, &param, 0, OCI_ATTR_PARAM, oci8_errhp));
            oci8_tdo_init(type_obj, svcctx, param);
            break;
        default:
            type_obj = INT2FIX(typecode);
        }
        rb_ary_store(attrs, idx - 1, name_obj);
        rb_ary_store(types, idx - 1, type_obj);
    }
    name = ALLOCA_N(char, name_max + 2);
    name[0] = '@';
    for (idx = 0; idx < num_type_attrs; idx++) {
        VALUE name_obj = RARRAY_PTR(attrs)[idx];
        OCIMultiByteStrCaseConversion(oci8_envhp, name + 1, RSTRING_PTR(name_obj), OCI_NLS_LOWERCASE);
        rb_ary_store(attr_syms, idx, ID2SYM(rb_intern(name)));
    }
    rb_ivar_set(self, id_at_schema_name, schema_name);
    rb_ivar_set(self, id_at_type_name, type_name);
    rb_ivar_set(self, id_at_class, klass);
    rb_ivar_set(self, id_at_attrs, attrs);
    rb_ivar_set(self, id_at_attr_syms, attr_syms);
    rb_ivar_set(self, id_at_types, types);

    return Qnil;
}

static VALUE oci8_tdo_initialize(VALUE self, VALUE metadata)
{
    oci8_base_t *md;
    oci8_svcctx_t *svcctx;

    Check_Handle(metadata, cOCI8MetadataBase, md);
    /* TODO link to svcctx to free on closing the connection. */
    svcctx = oci8_get_svcctx(rb_ivar_get(metadata, id_at_con));
    return oci8_tdo_init(self, svcctx, md->hp);
}

static VALUE oci8_tdo_hash(VALUE self)
{
    oci8_base_t *tdo = DATA_PTR(self);
    return LONG2FIX(tdo->hp);
}

static VALUE oci8_tdo_eq(VALUE lhs, VALUE rhs)
{
    oci8_base_t *l = DATA_PTR(lhs);
    oci8_base_t *r;

    if (!rb_obj_is_kind_of(rhs, cOCITDO))
        return Qfalse;
    r = DATA_PTR(rhs);
    return l->hp == r->hp ? Qtrue : Qfalse;
}

typedef struct {
    oci8_bind_t bind;
} bind_tdo_t;

static void bind_tdo_mark(oci8_base_t *base)
{
}

static void bind_tdo_free(oci8_base_t *base)
{
    oci8_bind_t *bind = (oci8_bind_t*)base;
    if (bind->valuep != NULL) {
        OCIObjectFree(oci8_envhp, oci8_errhp, bind->valuep, OCI_DEFAULT);
        bind->valuep = NULL;
    }
    oci8_bind_free(base);
}

static VALUE oraobject_to_rubyobj(VALUE tdo_obj, dvoid *instance, dvoid *null_struct);
static VALUE oraopaque_to_rubyobj(VALUE tdo_obj, dvoid *instance, dvoid *null_struct);
static VALUE orascalar_to_rubyobj(oci8_svcctx_t *svcctx, OCITypeCode typecode, dvoid *instance);

static VALUE bind_tdo_get(oci8_bind_t *bind)
{
    bind_tdo_t *bind_tdo = (bind_tdo_t*)bind;
    return oraobject_to_rubyobj(bind_tdo->bind.tdo, bind_tdo->bind.valuep, bind_tdo->bind.null_struct);
}

static VALUE oraobject_to_rubyobj(VALUE tdo_obj, dvoid *instance, dvoid *null_struct)
{
    VALUE attrs = rb_ivar_get(tdo_obj, id_at_attrs);
    VALUE attr_syms = rb_ivar_get(tdo_obj, id_at_attr_syms);
    VALUE types = rb_ivar_get(tdo_obj, id_at_types);
    VALUE klass = rb_ivar_get(tdo_obj, id_at_class);
    VALUE obj;
    oci8_tdo_t *tdo;
    int num_attrs;
    int i;

    if (!rb_obj_is_kind_of(tdo_obj, cOCITDO)) {
        rb_raise(rb_eRuntimeError, "unexpected type structure");
    }
    Data_Get_Struct(tdo_obj, oci8_tdo_t, tdo);
    Check_Type(attrs, T_ARRAY);
    Check_Type(attr_syms, T_ARRAY);
    Check_Type(types, T_ARRAY);
    Check_Type(klass, T_CLASS);

    obj = rb_obj_alloc(klass);
    num_attrs = RARRAY_LEN(attrs);
    for (i = 0; i < num_attrs; i++) {
        VALUE attr = RARRAY_PTR(attrs)[i];
        VALUE type = RARRAY_PTR(types)[i];
        CONST OraText *name = (OraText *)RSTRING_PTR(attr);
        ub4 namelen = RSTRING_LEN(attr);
        OCIInd attr_null_status;
        dvoid *attr_null_struct;
        dvoid *attr_value;
        OCIType *attr_tdo;
        VALUE attr_obj;

        oci_lc(OCIObjectGetAttr(oci8_envhp, oci8_errhp, instance,
                                null_struct, tdo->base.hp,
                                &name, &namelen, 1, NULL, 0,
                                &attr_null_status, &attr_null_struct,
                                &attr_value, &attr_tdo));
        if (attr_null_status) {
            attr_obj = Qnil;
        } else {
            OCITypeCode typecode = OCITypeTypeCode(oci8_envhp, oci8_errhp, attr_tdo);
            switch (typecode) {
            case OCI_TYPECODE_OBJECT:
                attr_obj = oraobject_to_rubyobj(type, attr_value, attr_null_struct);
                break;
            case OCI_TYPECODE_OPAQUE:
                attr_obj = oraopaque_to_rubyobj(type, attr_value, attr_null_struct);
                break;
            default:
                if (!FIXNUM_P(type) || FIX2INT(type) != typecode)
                    rb_raise(rb_eRuntimeError, "unexpected type structure");
                attr_obj = orascalar_to_rubyobj(tdo->svcctx, typecode, attr_value);
                break;
            }
        }
        rb_ivar_set(obj, SYM2ID(RARRAY_PTR(attr_syms)[i]), attr_obj);
    }
    rb_obj_call_init(obj, 0, NULL);
    return obj;
}

static VALUE oraopaque_to_rubyobj(VALUE tdo_obj, dvoid *instance, dvoid *null_struct)
{
    return Qnil;
}

static VALUE orascalar_to_rubyobj(oci8_svcctx_t *svcctx, OCITypeCode typecode, dvoid *instance)
{
    OCIString *vs;

    switch (typecode) {
    case OCI_TYPECODE_CHAR:
    case OCI_TYPECODE_VARCHAR:
    case OCI_TYPECODE_VARCHAR2:
    case OCI_TYPECODE_RAW:
        vs = *(OCIString **)instance;
        return rb_str_new(OCIStringPtr(oci8_envhp, vs), OCIStringSize(oci8_envhp, vs));
    case OCI_TYPECODE_NUMBER:
    case OCI_TYPECODE_DECIMAL:
        return oci8_make_ocinumber((OCINumber *)instance);
    case OCI_TYPECODE_INTEGER:
    case OCI_TYPECODE_SMALLINT:
        return oci8_make_integer((OCINumber *)instance);
    case OCI_TYPECODE_REAL:
    case OCI_TYPECODE_DOUBLE:
    case OCI_TYPECODE_FLOAT:
        return oci8_make_float((OCINumber *)instance);
    case OCI_TYPECODE_CLOB:
        return oci8_make_clob(svcctx, *(OCILobLocator **)instance);
    case OCI_TYPECODE_BLOB:
        return oci8_make_blob(svcctx, *(OCILobLocator **)instance);
    case OCI_TYPECODE_DATE:
        return oci8_make_datetime_from_ocidate((OCIDate*)instance);
    case OCI_TYPECODE_TIMESTAMP:
    case OCI_TYPECODE_TIMESTAMP_TZ:
    case OCI_TYPECODE_TIMESTAMP_LTZ:
        return oci8_make_datetime_from_ocidatetime(*(OCIDateTime**)instance);
    case OCI_TYPECODE_INTERVAL_YM:
        return oci8_make_interval_ym(*(OCIInterval**)instance);
    case OCI_TYPECODE_INTERVAL_DS:
        return oci8_make_interval_ds(*(OCIInterval**)instance);
    case OCI_TYPECODE_BFLOAT:
      return rb_float_new((double)*(float*)instance);
    case OCI_TYPECODE_BDOUBLE:
      return rb_float_new(*(double*)instance);
    }
    rb_raise(rb_eRuntimeError, "unsupported typecode %d", typecode);
}

static void bind_tdo_set(oci8_bind_t *base, VALUE val)
{
    rb_notimplement();
}

static void bind_tdo_init(oci8_bind_t *bind, VALUE svc, VALUE *val, VALUE length, VALUE prec, VALUE scale)
{
    VALUE tdo_obj = length;
    oci8_svcctx_t *svcctx;
    oci8_tdo_t *tdo;

    svcctx = oci8_get_svcctx(svc);
    if (!rb_obj_is_kind_of(tdo_obj, cOCITDO)) {
        rb_raise(rb_eTypeError, "invalid argument %s (expect %s)", rb_class2name(CLASS_OF(length)), rb_class2name(cOCITDO));
    }
    Data_Get_Struct(length, oci8_tdo_t, tdo);
    oci_lc(OCIObjectNew(oci8_envhp, oci8_errhp, svcctx->base.hp, OCI_TYPECODE_OBJECT, tdo->base.hp, NULL, OCI_DURATION_SESSION, 0, (dvoid*)&bind->valuep));
    oci_lc(OCIObjectGetInd(oci8_envhp, oci8_errhp, bind->valuep, &bind->null_struct));
    bind->tdo = tdo_obj;
}

static oci8_bind_class_t bind_tdo_class = {
    {
        bind_tdo_mark,
        bind_tdo_free,
        sizeof(bind_tdo_t)
    },
    bind_tdo_get,
    bind_tdo_set,
    bind_tdo_init,
    NULL,
    NULL,
    SQLT_NTY
};

void Init_oci_tdo(VALUE cOCI8)
{
    cOCITDO = oci8_define_class_under(cOCI8, "TDO", &oci8_tdo_class);

    mapping = rb_hash_new();
    rb_define_const(cOCITDO, "Mapping", mapping);
    id_at_attrs = rb_intern("@attrs");
    id_at_attr_syms = rb_intern("@attr_syms");
    id_at_types = rb_intern("@types");
    id_at_class = rb_intern("@klass");
    id_at_con = rb_intern("@con");
    id_at_type_name = rb_intern("@type_name");
    id_at_schema_name = rb_intern("@schema_name");
    rb_define_method(cOCITDO, "initialize", oci8_tdo_initialize, 1);
    rb_define_method(cOCITDO, "hash", oci8_tdo_hash, 0);
    rb_define_method(cOCITDO, "==", oci8_tdo_eq, 1);

    oci8_define_bind_class("TDO", &bind_tdo_class);
}
