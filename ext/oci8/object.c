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

static VALUE cOCI8TDO;
static VALUE cOCI8NamedType;
static VALUE cOCI8BindNamedType;
static ID id_to_value;

typedef struct {
    oci8_base_t base;
    VALUE tdo;
    char **instancep;
    char **null_structp;
} oci8_named_type_t;

enum {
    ATTR_INVALID = 0,
    ATTR_STRING,
    ATTR_RAW,
    ATTR_OCINUMBER,
    ATTR_FLOAT,
    ATTR_INTEGER,
    ATTR_NAMED_TYPE,
    ATTR_NAMED_COLLECTION,
    NAMED_COLLECTION,
};

static VALUE get_attribute(VALUE self, VALUE datatype, VALUE typeinfo, void *data, OCIInd *ind);
static VALUE get_coll_elements(VALUE self, VALUE datatype, VALUE typeinfo, OCIColl *coll);
static void set_attribute(VALUE datatype, VALUE typeinfo, void *data, OCIInd *ind, VALUE val);

static void oci8_tdo_mark(oci8_base_t *base)
{
    if (base->parent != NULL) {
        rb_gc_mark(base->parent->self);
    }
}

static void oci8_tdo_free(oci8_base_t *base)
{
    if (base->hp.tdo != NULL) {
        OCIObjectUnpin(oci8_envhp, oci8_errhp, base->hp.tdo);
        base->hp.tdo = NULL;
    }
}

static VALUE oci8_tdo_setup(VALUE self, VALUE svc, VALUE md_obj)
{
    oci8_base_t *tdo = DATA_PTR(self);
    oci8_svcctx_t *svcctx = oci8_get_svcctx(svc);
    oci8_base_t *md;
    OCIRef *tdo_ref = NULL;

    Check_Object(md_obj, cOCI8MetadataBase);
    md = DATA_PTR(md_obj);

    if (tdo->hp.tdo != NULL) {
        OCIObjectUnpin(oci8_envhp, oci8_errhp, tdo->hp.tdo);
        tdo->hp.tdo = NULL;
    }
    oci_lc(OCIAttrGet(md->hp.ptr, OCI_DTYPE_PARAM, &tdo_ref, NULL, OCI_ATTR_REF_TDO, oci8_errhp));
    if (tdo_ref == NULL)
        return Qnil;
    oci_lc(OCIObjectPin(oci8_envhp, oci8_errhp, tdo_ref, 0, OCI_PIN_ANY, OCI_DURATION_SESSION, OCI_LOCK_NONE, &tdo->hp.ptr));
    oci8_link_to_parent(tdo, &svcctx->base);
    return self;
}

static oci8_base_class_t oci8_tdo_class = {
    oci8_tdo_mark,
    oci8_tdo_free,
    sizeof(oci8_base_t)
};

static void oci8_named_type_mark(oci8_base_t *base)
{
    if (base->parent != NULL) {
        rb_gc_mark(base->parent->self);
    }
}

static void oci8_named_type_free(oci8_base_t *base)
{
    oci8_named_type_t *obj = (oci8_named_type_t *)base;
    obj->instancep = NULL;
    obj->null_structp = NULL;
}

static VALUE oci8_named_type_initialize(VALUE self)
{
    oci8_named_type_t *obj = DATA_PTR(self);

    obj->tdo = Qnil;
    obj->instancep = NULL;
    obj->null_structp = NULL;
    return Qnil;
}

static VALUE oci8_named_type_tdo(VALUE self)
{
    oci8_named_type_t *obj = DATA_PTR(self);
    return obj->tdo;
}

static void oci8_named_type_check_offset(VALUE self, VALUE val_offset, VALUE ind_offset, size_t val_size, void **instancep, OCIInd **indp)
{
    oci8_named_type_t *obj = DATA_PTR(self);
    if (obj->instancep == NULL || obj->null_structp == NULL) {
        rb_raise(rb_eRuntimeError, "%s is not initialized or freed", rb_obj_classname(self));
    }
    Check_Type(val_offset, T_FIXNUM);
    Check_Type(ind_offset, T_FIXNUM);
    *instancep = (void*)(*obj->instancep + FIX2INT(val_offset));
    *indp = (OCIInd *)(*obj->null_structp + FIX2INT(ind_offset));
}

/*
 * attribute
 */
static VALUE oci8_named_type_get_attribute(VALUE self, VALUE datatype, VALUE typeinfo, VALUE val_offset, VALUE ind_offset)
{
    void *data;
    OCIInd *ind;

    oci8_named_type_check_offset(self, val_offset, ind_offset, sizeof(OCIString*), &data, &ind);
    return get_attribute(self, datatype, typeinfo, data, ind);
}

static VALUE get_attribute(VALUE self, VALUE datatype, VALUE typeinfo, void *data, OCIInd *ind)
{
    VALUE rv;
    VALUE tmp_obj;
    oci8_named_type_t *obj;

    if (*ind) {
        return Qnil;
    }
    Check_Type(datatype, T_FIXNUM);
    switch (FIX2INT(datatype)) {
    case ATTR_STRING:
        return rb_str_new(TO_CHARPTR(OCIStringPtr(oci8_envhp, *(OCIString **)data)),
                          OCIStringSize(oci8_envhp, *(OCIString **)data));
    case ATTR_RAW:
        return rb_str_new(TO_CHARPTR(OCIRawPtr(oci8_envhp, *(OCIRaw **)data)),
                          OCIRawSize(oci8_envhp, *(OCIRaw **)data));
    case ATTR_OCINUMBER:
        return oci8_make_ocinumber((OCINumber *)data);
    case ATTR_FLOAT:
        return oci8_make_float((OCINumber *)data);
    case ATTR_INTEGER:
        return oci8_make_integer((OCINumber *)data);
    case ATTR_NAMED_TYPE:
        Check_Object(typeinfo, cOCI8TDO);
        /* Be carefull. Don't use _tmp_obj_ out of this function. */
        tmp_obj = rb_funcall(cOCI8NamedType, oci8_id_new, 0);
        obj = DATA_PTR(tmp_obj);
        obj->tdo = typeinfo;
        obj->instancep = (char**)&data;
        obj->null_structp = (char**)&ind;
        oci8_link_to_parent(&obj->base, DATA_PTR(self));
        rv = rb_funcall(tmp_obj, id_to_value, 0);
        oci8_unlink_from_parent(&obj->base);
        return rv;
    case ATTR_NAMED_COLLECTION:
        data = *(OCIColl**)data;
    case NAMED_COLLECTION:
        Check_Type(typeinfo, T_ARRAY);
        if (RARRAY_LEN(typeinfo) != 2) {
            rb_raise(rb_eRuntimeError, "bug?");
        }
        datatype = RARRAY_PTR(typeinfo)[0];
        typeinfo = RARRAY_PTR(typeinfo)[1];
        return get_coll_elements(self, datatype, typeinfo, (OCIColl*)data);

    default:
        rb_raise(rb_eRuntimeError, "not supported datatype");
    }
}

static VALUE oci8_named_type_get_coll_elements(VALUE self, VALUE datatype, VALUE typeinfo)
{
    oci8_named_type_t *obj = DATA_PTR(self);
    OCIInd *ind;

    if (obj->instancep == NULL || obj->null_structp == NULL) {
        rb_raise(rb_eRuntimeError, "%s is not initialized or freed", rb_obj_classname(self));
    }
    ind = (OCIInd*)*obj->null_structp;
    if (*ind) {
        return Qnil;
    }
    return get_coll_elements(self, datatype, typeinfo, (OCIColl*)*obj->instancep);
}

static VALUE get_coll_elements(VALUE self, VALUE datatype, VALUE typeinfo, OCIColl *coll)
{
    void *data;
    OCIInd *ind;
    VALUE ary;
    sb4 size;
    sb4 idx;

    oci_lc(OCICollSize(oci8_envhp, oci8_errhp, coll, &size));
    ary = rb_ary_new2(size);
    for (idx = 0; idx < size; idx++) {
        boolean exists;
        oci_lc(OCICollGetElem(oci8_envhp, oci8_errhp, coll, idx, &exists, &data, (dvoid**)&ind));
        if (exists) {
            rb_ary_store(ary, idx, get_attribute(self, datatype, typeinfo, data, ind));
        }
    }
    return ary;
}


static VALUE oci8_named_type_set_attribute(VALUE self, VALUE datatype, VALUE typeinfo, VALUE val_offset, VALUE ind_offset, VALUE val)
{
    void *data;
    OCIInd *ind;

    oci8_named_type_check_offset(self, val_offset, ind_offset, sizeof(OCIString*), &data, &ind);
    set_attribute(datatype, typeinfo, data, ind, val);
    return Qnil;
}

static void set_attribute(VALUE datatype, VALUE typeinfo, void *data, OCIInd *ind, VALUE val)
{
    /* named collection */
    OCIColl *coll;
    sb4 idx;
    void **elem_ptr;
    OCIInd *elem_ind_ptr;
    union {
        OCIString *os;
    } elem;
    OCIInd elem_ind;

    if (NIL_P(val)) {
        *ind = -1;
        return;
    }
    Check_Type(datatype, T_FIXNUM);
    switch (FIX2INT(datatype)) {
    case ATTR_STRING:
        StringValue(val);
        oci_lc(OCIStringAssignText(oci8_envhp, oci8_errhp,
                                   RSTRING_ORATEXT(val), RSTRING_LEN(val),
                                   (OCIString **)data));
        break;
    case ATTR_RAW:
        StringValue(val);
        oci_lc(OCIRawAssignBytes(oci8_envhp, oci8_errhp,
                                 RSTRING_ORATEXT(val), RSTRING_LEN(val),
                                 (OCIRaw **)data));
        break;
    case ATTR_OCINUMBER:
    case ATTR_FLOAT:
        oci8_set_ocinumber((OCINumber*)data, val);
        break;
    case ATTR_INTEGER:
        oci8_set_integer((OCINumber*)data, val);
        break;
    case ATTR_NAMED_TYPE:
        Check_Object(typeinfo, cOCI8TDO);
        rb_notimplement();
    case ATTR_NAMED_COLLECTION:
        Check_Type(typeinfo, T_ARRAY);
        if (RARRAY_LEN(typeinfo) != 2) {
            rb_raise(rb_eRuntimeError, "bug?");
        }
        datatype = RARRAY_PTR(typeinfo)[0];
        typeinfo = RARRAY_PTR(typeinfo)[1];
        Check_Type(datatype, T_FIXNUM);
        Check_Type(val, T_ARRAY);
        switch (FIX2INT(datatype)) {
        case ATTR_STRING:
            elem_ptr = (void**)&elem;
            elem_ind_ptr = &elem_ind;
            elem.os = NULL;
            OCIStringAssignText(oci8_envhp, oci8_errhp, (text *)"dummy", 5, &elem.os);
            break;
        default:
            rb_raise(rb_eRuntimeError, "not supported datatype");
        }
#if 0
        oci_lc(OCIObjectNew(oci8_envhp, oci8_errhp, svcctx->base.hp.svc, OCI_TYPECODE_OBJECT, tdo->hp.tdo, NULL, OCI_DURATION_SESSION, TRUE, (dvoid**)obj->instancep));
#endif
        coll = *(OCIColl**)data;
        if (coll == NULL) {
            rb_bug("coll is null");
        }
        for (idx = 0; idx < RARRAY_LEN(val); idx++) {
            set_attribute(datatype, typeinfo, elem_ptr, elem_ind_ptr, RARRAY_PTR(val)[idx]);
            oci_lc(OCICollAppend(oci8_envhp, oci8_errhp, *elem_ptr, elem_ind_ptr, coll));
        }
        switch (FIX2INT(datatype)) {
        case ATTR_STRING:
            if (elem.os != NULL) {
                oci_lc(OCIStringResize(oci8_envhp, oci8_errhp, 0, &elem.os));
            }
            break;
        default:
            rb_raise(rb_eRuntimeError, "not supported datatype");
        }
        break;
    default:
        rb_raise(rb_eRuntimeError, "not supported datatype");
    }
    *ind = 0;
}

static oci8_base_class_t oci8_named_type_class = {
    oci8_named_type_mark,
    oci8_named_type_free,
    sizeof(oci8_named_type_t)
};

static void bind_named_type_mark(oci8_base_t *base)
{
    oci8_bind_t *obind = (oci8_bind_t *)base;
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)obind->valuep;
    ub4 idx = 0;

    do {
        rb_gc_mark(oho[idx].obj);
    } while (++idx < obind->maxar_sz);
    rb_gc_mark(obind->tdo);
}

static void bind_named_type_free(oci8_base_t *base)
{
#if 0
    oci8_bind_t *obind = (oci8_bind_t *)base;
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)obind->valuep;
    ub4 idx = 0;

    do {
        if (objs[idx] != NULL) {
            OCIObjectFree(oci8_envhp, oci8_errhp, objs[idx], OCI_DEFAULT);
            objs[idx] = NULL;
        }
    } while (++idx < obind->maxar_sz);
#endif
}

static VALUE bind_named_type_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)data;
    return oho->obj;
}

static void bind_named_type_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    rb_raise(rb_eRuntimeError, "not supported");
}

static void bind_named_type_init(oci8_bind_t *obind, VALUE svc, VALUE *val, VALUE length)
{
    VALUE tdo_obj = length;

    obind->value_sz = sizeof(void*);
    obind->alloc_sz = sizeof(oci8_hp_obj_t);

    Check_Object(tdo_obj, cOCI8TDO);
    obind->tdo = tdo_obj;
}

static void bind_named_type_init_elem(oci8_bind_t *obind, VALUE svc)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)obind->valuep;
    oci8_base_t *tdo = DATA_PTR(obind->tdo);
    OCITypeCode tc = OCITypeTypeCode(oci8_envhp, oci8_errhp, tdo->hp.tdo);
    oci8_named_type_t *obj;
    oci8_svcctx_t *svcctx;
    ub4 idx = 0;

    svcctx = oci8_get_svcctx(svc);
    do {
        oho[idx].obj = rb_funcall(cOCI8NamedType, oci8_id_new, 0);
        obj = DATA_PTR(oho[idx].obj);
        obj->tdo = obind->tdo;
        obj->instancep = (char**)&oho[idx].hp;
        obj->null_structp = (char**)&obind->u.null_structs[idx];
        oci8_link_to_parent(&obj->base, &obind->base);

        oci_lc(OCIObjectNew(oci8_envhp, oci8_errhp, svcctx->base.hp.svc, tc, tdo->hp.tdo, NULL, OCI_DURATION_SESSION, TRUE, (dvoid**)obj->instancep));
        oci_lc(OCIObjectGetInd(oci8_envhp, oci8_errhp, (dvoid*)*obj->instancep, (dvoid**)obj->null_structp));
    } while (++idx < obind->maxar_sz);
}

static oci8_bind_class_t bind_named_type_class = {
    {
        bind_named_type_mark,
        bind_named_type_free,
        sizeof(oci8_bind_t)
    },
    bind_named_type_get,
    bind_named_type_set,
    bind_named_type_init,
    bind_named_type_init_elem,
    NULL,
    NULL,
    SQLT_NTY
};

void Init_oci_object(VALUE cOCI8)
{
    id_to_value = rb_intern("to_value");

    /* OCI8::TDO */
    cOCI8TDO = oci8_define_class_under(cOCI8, "TDO", &oci8_tdo_class);
    rb_define_private_method(cOCI8TDO, "setup", oci8_tdo_setup, 2);
    rb_define_const(cOCI8TDO, "ATTR_STRING", INT2FIX(ATTR_STRING));
    rb_define_const(cOCI8TDO, "ATTR_RAW", INT2FIX(ATTR_RAW));
    rb_define_const(cOCI8TDO, "ATTR_OCINUMBER", INT2FIX(ATTR_OCINUMBER));
    rb_define_const(cOCI8TDO, "ATTR_FLOAT", INT2FIX(ATTR_FLOAT));
    rb_define_const(cOCI8TDO, "ATTR_INTEGER", INT2FIX(ATTR_INTEGER));
    rb_define_const(cOCI8TDO, "ATTR_NAMED_TYPE", INT2FIX(ATTR_NAMED_TYPE));
    rb_define_const(cOCI8TDO, "ATTR_NAMED_COLLECTION", INT2FIX(ATTR_NAMED_COLLECTION));
    rb_define_const(cOCI8TDO, "NAMED_COLLECTION", INT2FIX(NAMED_COLLECTION));
#define ALIGNMENT_OF(type) (size_t)&(((struct {char c; type t;}*)0)->t)
    rb_define_const(cOCI8TDO, "SIZE_OF_POINTER", INT2FIX(sizeof(void *)));
    rb_define_const(cOCI8TDO, "ALIGNMENT_OF_POINTER", INT2FIX(ALIGNMENT_OF(void *)));
    rb_define_const(cOCI8TDO, "SIZE_OF_OCINUMBER", INT2FIX(sizeof(OCINumber)));
    rb_define_const(cOCI8TDO, "ALIGNMENT_OF_OCINUMBER", INT2FIX(ALIGNMENT_OF(OCINumber)));
    rb_define_const(cOCI8TDO, "SIZE_OF_OCIDATE", INT2FIX(sizeof(OCIDate)));
    rb_define_const(cOCI8TDO, "ALIGNMENT_OF_OCIDATE", INT2FIX(ALIGNMENT_OF(OCIDate)));
    rb_define_const(cOCI8TDO, "SIZE_OF_FLOAT", INT2FIX(sizeof(float)));
    rb_define_const(cOCI8TDO, "ALIGNMENT_OF_FLOAT", INT2FIX(ALIGNMENT_OF(float)));
    rb_define_const(cOCI8TDO, "SIZE_OF_DOUBLE", INT2FIX(sizeof(double)));
    rb_define_const(cOCI8TDO, "ALIGNMENT_OF_DOUBLE", INT2FIX(ALIGNMENT_OF(double)));

    /* OCI8::NamedType */
    cOCI8NamedType = oci8_define_class_under(cOCI8, "NamedType", &oci8_named_type_class);
    rb_define_method(cOCI8NamedType, "initialize", oci8_named_type_initialize, 0);
    rb_define_method(cOCI8NamedType, "tdo", oci8_named_type_tdo, 0);
    rb_define_private_method(cOCI8NamedType, "get_attribute", oci8_named_type_get_attribute, 4);
    rb_define_private_method(cOCI8NamedType, "get_coll_elements", oci8_named_type_get_coll_elements, 2);
    rb_define_private_method(cOCI8NamedType, "set_attribute", oci8_named_type_set_attribute, 5);

    /* OCI8::BindType::NamedType */
    cOCI8BindNamedType = oci8_define_bind_class("NamedType", &bind_named_type_class);
}
