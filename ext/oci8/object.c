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
static VALUE cOCI8Object;
static ID id_at_ruby_class;

typedef struct {
    ID id;
    VALUE val_offset;
    VALUE ind_offset;
} fini_func_t;

typedef struct {
    oci8_base_t base;
    int val_size;
    int idx_size;
    char *instance;
    char *null_struct;
    int fini_num;
    fini_func_t *fini_func;
} oci8_object_t;

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

static void oci8_object_mark(oci8_base_t *base)
{
    if (base->parent != NULL) {
        rb_gc_mark(base->parent->self);
    }
}

static void oci8_object_free(oci8_base_t *base)
{
    oci8_object_t *obj = (oci8_object_t *)base;
    int i;
    if (obj->fini_func != NULL) {
        for (i = 0; i < obj->fini_num; i++) {
            fini_func_t *fini = &obj->fini_func[i];
            rb_funcall(base->self, fini->id, 2, fini->val_offset, fini->ind_offset);
        }
        xfree(obj->fini_func);
        obj->fini_num = 0;
        obj->fini_func = NULL;
    }
    if (obj->instance != NULL) {
        xfree(obj->instance);
        obj->instance = NULL;
    }
    if (obj->null_struct != NULL) {
        xfree(obj->null_struct);
        obj->null_struct = NULL;
    }
}

static VALUE oci8_object_initialize_internal(VALUE self, VALUE val_size, VALUE ind_size, VALUE fini_funcs)
{
    oci8_object_t *obj = DATA_PTR(self);
    int num;
    int i;
    obj->val_size = NUM2INT(val_size);
    obj->idx_size = NUM2INT(ind_size);
    Check_Type(fini_funcs, T_ARRAY);
    num = RARRAY_LEN(fini_funcs);
    obj->fini_func = xmalloc(sizeof(fini_func_t) * num);
    for (i = 0; i < num; i++) {
        fini_func_t *fini = &obj->fini_func[i];
        VALUE elem = RARRAY_PTR(fini_funcs)[i];
        Check_Type(elem, T_ARRAY);
        if (RARRAY_LEN(elem) != 3) {
            rb_bug("bug in oci8/object.rb?");
        }
        Check_Type(RARRAY_PTR(elem)[0], T_SYMBOL);
        Check_Type(RARRAY_PTR(elem)[1], T_FIXNUM);
        Check_Type(RARRAY_PTR(elem)[2], T_FIXNUM);
        fini->id = SYM2ID(RARRAY_PTR(elem)[0]);
        fini->val_offset = RARRAY_PTR(elem)[1];
        fini->ind_offset = RARRAY_PTR(elem)[2];
    }
    obj->fini_num = num;
    obj->instance = xmalloc(obj->val_size);
    obj->null_struct = xmalloc(obj->idx_size);
    memset(obj->instance, 0, obj->val_size);
    memset(obj->null_struct, -1, obj->idx_size);
    *(OCIInd*)obj->null_struct = 0;
    return Qnil;
}

static void oci8_object_check_offset(VALUE self, VALUE val_offset, VALUE ind_offset, size_t val_size, void **instancep, OCIInd **indp)
{
    oci8_object_t *obj = DATA_PTR(self);

    Check_Type(val_offset, T_FIXNUM);
    Check_Type(ind_offset, T_FIXNUM);
    *instancep = (void*)(obj->instance + FIX2INT(val_offset));
    *indp = (OCIInd *)(obj->null_struct + FIX2INT(ind_offset));
}

static VALUE oci8_object_is_null_p(VALUE self)
{
    void *instance;
    OCIInd *ind;

    oci8_object_check_offset(self, INT2FIX(0), INT2FIX(0), 1, &instance, &ind);
    return *ind ? Qtrue : Qfalse;
}

/*
 * string attribute
 */
static VALUE oci8_object_fini_string(VALUE self, VALUE val_offset, VALUE ind_offset)
{
    void *instance;
    OCIInd *ind;

    oci8_object_check_offset(self, val_offset, ind_offset, sizeof(OCIString*), &instance, &ind);
    oci_lc(OCIStringResize(oci8_envhp, oci8_errhp, 0, (OCIString **)instance));
    return Qnil;
}

static VALUE oci8_object_get_string(VALUE self, VALUE val_offset, VALUE ind_offset)
{
    void *instance;
    OCIInd *ind;

    oci8_object_check_offset(self, val_offset, ind_offset, sizeof(OCIString*), &instance, &ind);
    if (*ind) {
        return Qnil;
    } else {
        OCIString **vs = (OCIString **)instance;
        return rb_str_new(TO_CHARPTR(OCIStringPtr(oci8_envhp, *vs)), OCIStringSize(oci8_envhp, *vs));
    }
}

static VALUE oci8_object_set_string(VALUE self, VALUE val_offset, VALUE ind_offset, VALUE val)
{
    void *instance;
    OCIInd *ind;

    oci8_object_check_offset(self, val_offset, ind_offset, sizeof(OCIString*), &instance, &ind);
    if (NIL_P(val)) {
        *ind = -1;
    } else {
        OCIString **vs = (OCIString **)instance;
        StringValue(val);
        oci_lc(OCIStringAssignText(oci8_envhp, oci8_errhp, RSTRING_ORATEXT(val), RSTRING_LEN(val), vs));
        *ind = 0;
    }
    return Qnil;
}

/*
 * raw attribute
 */
static VALUE oci8_object_fini_raw(VALUE self, VALUE val_offset, VALUE ind_offset)
{
    void *instance;
    OCIInd *ind;

    oci8_object_check_offset(self, val_offset, ind_offset, sizeof(OCIRaw*), &instance, &ind);
    oci_lc(OCIRawResize(oci8_envhp, oci8_errhp, 0, (OCIRaw **)instance));
    return Qnil;
}

static VALUE oci8_object_get_raw(VALUE self, VALUE val_offset, VALUE ind_offset)
{
    void *instance;
    OCIInd *ind;

    oci8_object_check_offset(self, val_offset, ind_offset, sizeof(OCIRaw*), &instance, &ind);
    if (*ind) {
        return Qnil;
    } else {
        OCIRaw **vs = (OCIRaw **)instance;
        return rb_str_new(TO_CHARPTR(OCIRawPtr(oci8_envhp, *vs)), OCIRawSize(oci8_envhp, *vs));
    }
}

static VALUE oci8_object_set_raw(VALUE self, VALUE val_offset, VALUE ind_offset, VALUE val)
{
    void *instance;
    OCIInd *ind;

    oci8_object_check_offset(self, val_offset, ind_offset, sizeof(OCIRaw*), &instance, &ind);
    if (NIL_P(val)) {
        *ind = -1;
    } else {
        OCIRaw **vs = (OCIRaw **)instance;
        StringValue(val);
        oci_lc(OCIRawAssignBytes(oci8_envhp, oci8_errhp, RSTRING_ORATEXT(val), RSTRING_LEN(val), vs));
        *ind = 0;
    }
    return Qnil;
}

/*
 * number attribute
 */
static VALUE oci8_object_get_number(VALUE self, VALUE val_offset, VALUE ind_offset)
{
    void *instance;
    OCIInd *ind;

    oci8_object_check_offset(self, val_offset, ind_offset, sizeof(OCINumber), &instance, &ind);
    if (*ind) {
        return Qnil;
    } else {
        return oci8_make_ocinumber((OCINumber *)instance);
    }
}

static VALUE oci8_object_get_integer(VALUE self, VALUE val_offset, VALUE ind_offset)
{
    void *instance;
    OCIInd *ind;

    oci8_object_check_offset(self, val_offset, ind_offset, sizeof(OCINumber), &instance, &ind);
    if (*ind) {
        return Qnil;
    } else {
        return oci8_make_integer((OCINumber *)instance);
    }
}

static VALUE oci8_object_get_float(VALUE self, VALUE val_offset, VALUE ind_offset)
{
    void *instance;
    OCIInd *ind;

    oci8_object_check_offset(self, val_offset, ind_offset, sizeof(OCINumber), &instance, &ind);
    if (*ind) {
        return Qnil;
    } else {
        return oci8_make_float((OCINumber *)instance);
    }
}

static VALUE oci8_object_set_number(VALUE self, VALUE val_offset, VALUE ind_offset, VALUE val)
{
    void *instance;
    OCIInd *ind;

    oci8_object_check_offset(self, val_offset, ind_offset, sizeof(OCINumber), &instance, &ind);
    if (NIL_P(val)) {
        *ind = -1;
    } else {
        oci8_set_ocinumber((OCINumber*)instance, val);
        *ind = 0;
    }
    return Qnil;
}

static VALUE oci8_object_set_integer(VALUE self, VALUE val_offset, VALUE ind_offset, VALUE val)
{
    void *instance;
    OCIInd *ind;

    oci8_object_check_offset(self, val_offset, ind_offset, sizeof(OCINumber), &instance, &ind);
    if (NIL_P(val)) {
        *ind = -1;
    } else {
        oci8_set_integer((OCINumber*)instance, val);
        *ind = 0;
    }
    return Qnil;
}

static oci8_base_class_t oci8_object_class = {
    oci8_object_mark,
    oci8_object_free,
    sizeof(oci8_object_t)
};

static void bind_obj_mark(oci8_base_t *base)
{
    oci8_bind_t *obind = (oci8_bind_t *)base;
    rb_gc_mark(obind->tdo);
}

static void bind_obj_free(oci8_base_t *base)
{
}

static VALUE bind_obj_get(oci8_bind_t *obind, void *data, void *null_struct)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)data;
    return oho->obj;
}

static void bind_obj_set(oci8_bind_t *obind, void *data, void **null_structp, VALUE val)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)data;
    oci8_object_t *obj;

    Check_Object(val, rb_ivar_get(obind->tdo, id_at_ruby_class));
    obj = DATA_PTR(val);
    oho->hp = obj->instance;
    oho->obj = val;
    *null_structp = obj->null_struct;
}

static void bind_obj_init(oci8_bind_t *obind, VALUE svc, VALUE *val, VALUE length)
{
    VALUE tdo_obj = length;

    obind->value_sz = sizeof(void*);
    obind->alloc_sz = sizeof(oci8_hp_obj_t);

    Check_Object(tdo_obj, cOCI8TDO);
    obind->tdo = tdo_obj;
}

static void bind_obj_init_elem(oci8_bind_t *obind, VALUE svc)
{
    oci8_hp_obj_t *oho = (oci8_hp_obj_t *)obind->valuep;
    oci8_object_t *obj;
    oci8_svcctx_t *svcctx;
    ub4 idx = 0;

    svcctx = oci8_get_svcctx(svc);
    do {
        oho[idx].obj = rb_funcall(rb_ivar_get(obind->tdo, id_at_ruby_class), oci8_id_new, 1, svc);
        obj = DATA_PTR(oho[idx].obj);
        oho[idx].hp = obj->instance;
        obind->u.null_structs[idx] = obj->null_struct;
    } while (++idx < obind->maxar_sz);
}

static oci8_bind_class_t bind_obj_class = {
    {
        bind_obj_mark,
        bind_obj_free,
        sizeof(oci8_bind_t)
    },
    bind_obj_get,
    bind_obj_set,
    bind_obj_init,
    bind_obj_init_elem,
    NULL,
    NULL,
    SQLT_NTY
};

void Init_oci_object(VALUE cOCI8)
{
    VALUE mNamedType;
    id_at_ruby_class = rb_intern("@ruby_class");

    cOCI8TDO = oci8_define_class_under(cOCI8, "TDO", &oci8_tdo_class);
    rb_define_private_method(cOCI8TDO, "setup", oci8_tdo_setup, 2);

    mNamedType = rb_define_module_under(cOCI8, "NamedType");
    cOCI8Object = oci8_define_class_under(mNamedType, "Base", &oci8_object_class);
    rb_define_private_method(cOCI8Object, "initialize_internal", oci8_object_initialize_internal, 3);
    rb_define_method(cOCI8Object, "is_null?", oci8_object_is_null_p, 0);
    /* string attribute */
    rb_define_private_method(cOCI8Object, "__fini_string", oci8_object_fini_string, 2);
    rb_define_private_method(cOCI8Object, "__get_string", oci8_object_get_string, 2);
    rb_define_private_method(cOCI8Object, "__set_string", oci8_object_set_string, 3);
    /* raw attribute */
    rb_define_private_method(cOCI8Object, "__fini_raw", oci8_object_fini_raw, 2);
    rb_define_private_method(cOCI8Object, "__get_raw", oci8_object_get_raw, 2);
    rb_define_private_method(cOCI8Object, "__set_raw", oci8_object_set_raw, 3);
    /* number attribute */
    rb_define_private_method(cOCI8Object, "__get_number", oci8_object_get_number, 2);
    rb_define_private_method(cOCI8Object, "__get_integer", oci8_object_get_integer, 2);
    rb_define_private_method(cOCI8Object, "__get_float", oci8_object_get_float, 2);
    rb_define_private_method(cOCI8Object, "__set_number", oci8_object_set_number, 3);
    rb_define_private_method(cOCI8Object, "__set_integer", oci8_object_set_integer, 3);

    oci8_define_bind_class("NamedType", &bind_obj_class);

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
}
