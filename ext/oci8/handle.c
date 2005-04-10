/*
  handle.c - part of ruby-oci8

  Copyright (C) 2002 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIHandle
This is the abstract super class of OCI Handles.
See ((<Class Hierarchy>)).
=end
*/
#include "oci8.h"

VALUE oci8_handle_do_initialize(VALUE self, VALUE venv, ub4 type)
{
  sword rv;
  oci8_handle_t *envh;
  oci8_handle_t *h;

  Get_Handle(self, h);
  Check_Handle(venv, OCIEnv, envh); /* 1 */
  rv = OCIHandleAlloc(envh->hp, &h->hp, type, 0, NULL);
  if (rv != OCI_SUCCESS)
    oci8_env_raise(envh->hp, rv);
  h->type = type;
  h->errhp = envh->errhp;
  oci8_link(envh, h);
  return Qnil;
}

static VALUE oci8_handle_initialize(VALUE self)
{
  rb_raise(rb_eNameError, "private method `new' called for %s:Class", rb_class2name(self));
}

static void oci8_handle_do_free(oci8_handle_t *h)
{
  oci8_bind_handle_t *bh;
  int i;
  if (h->type == 0) {
    return;
  }
  /* free its children recursively.*/
  for (i = 0;i < h->size;i++) {
    if (h->children[i] != NULL)
      oci8_handle_do_free(h->children[i]);
  }
  /* unlink from parent */
  oci8_unlink(h);
  /* do free */
  switch (h->type) {
  case OCI_HTYPE_SVCCTX:
    if (h->u.svcctx.authhp)
      OCIHandleFree(h->u.svcctx.authhp, OCI_HTYPE_SESSION);
    if (h->u.svcctx.srvhp)
      OCIHandleFree(h->u.svcctx.srvhp, OCI_HTYPE_SERVER);
    break;
  case OCI_HTYPE_BIND:
  case OCI_HTYPE_DEFINE:
    bh = (void*)h;
    if (bh->bind_type->free != NULL)
      bh->bind_type->free(bh);
    break;
  }
  if (h->type >= OCI_DTYPE_FIRST)
    OCIDescriptorFree(h->hp, h->type);
  else
    OCIHandleFree(h->hp, h->type);
  h->type = 0;
  return;
}

/*
=begin
--- OCIHandle#attrSet(type, value)
     :type
       the type of attribute to be set.
       See ((<Attributes of Handles and Descriptors>)).
     :value
       depends on ((|type|)).

     correspond native OCI function: ((|OCIAttrSet|))
=end
*/


/*
=begin
--- OCIHandle#attrGet(type)
     :type
       the type of attribute.
       See also ((<Attributes of Handles and Descriptors>)).
     :return value
       depends on ((|type|)).

     correspond native OCI function: ((|OCIAttrGet|))
=end
*/


/*
=begin
--- OCIHandle#free()
     explicitly free the OCI's data structure.

     correspond native OCI function: ((|OCIHandleFree|))
=end
*/
VALUE oci8_handle_free(VALUE self)
{
  oci8_handle_t *h;

  Get_Handle(self, h);
  oci8_handle_do_free(h);
  return self;
}

void  Init_oci8_handle(void)
{
  rb_define_method(cOCIHandle, "initialize", oci8_handle_initialize, 0);
  rb_define_method(cOCIHandle, "free", oci8_handle_free, 0);
  rb_define_method(cOCIHandle, "attrSet", oci8_attr_set, 2);
  rb_define_method(cOCIHandle, "attrGet", oci8_attr_get, 1);
}

void oci8_handle_cleanup(oci8_handle_t *h)
{
  oci8_handle_do_free(h);
  free(h);
}

void oci8_handle_mark(oci8_handle_t *h)
{
  oci8_bind_handle_t *bh;
  int i;

  switch (h->type) {
  case OCI_HTYPE_STMT:
    for (i = 0;i < h->size;i++) {
      if (h->children[i] != NULL) {
	if (h->children[i]->type == OCI_HTYPE_BIND || h->children[i]->type == OCI_HTYPE_DEFINE) {
	  rb_gc_mark(h->children[i]->self);
	}
      }
    }
    break;
  case OCI_HTYPE_DEFINE:
  case OCI_HTYPE_BIND:
    bh = (oci8_bind_handle_t *)h;
    rb_gc_mark(bh->obj);
    break;
  }
  if (h->parent != NULL) {
    rb_gc_mark(h->parent->self);
  }
}

oci8_handle_t *oci8_make_handle(ub4 type, dvoid *hp, OCIError *errhp, oci8_handle_t *parenth, sb4 value_sz)
{
  VALUE obj;
  oci8_handle_t *h;

  switch (type) {
  case OCI_DTYPE_LOB:
    obj = Data_Make_Struct(cOCILobLocator, oci8_handle_t, oci8_handle_mark, oci8_handle_cleanup, h);
#ifndef OCI8_USE_CALLBACK_LOB_READ
    h->u.lob_locator.char_width = 1;
#endif
    break;
  case OCI_DTYPE_ROWID:
    obj = Data_Make_Struct(cOCIRowid, oci8_handle_t, oci8_handle_mark, oci8_handle_cleanup, h);
    break;
  case OCI_DTYPE_PARAM:
    obj = Data_Make_Struct(cOCIParam, oci8_handle_t, oci8_handle_mark, oci8_handle_cleanup, h);
    h->u.param.is_implicit = 0;
    break;
  default:
    rb_bug("unsupported type %d in oci8_make_handle()", type);
  }
  h->type = type;
  h->hp = hp;
  h->errhp = errhp;
  h->self = obj;
  h->parent = NULL;
  h->size = 0;
  h->children = NULL;
  oci8_link(parenth, h);
  return h;
}

#define CHILDREN_ARRAY_GROW_SIZE 16

void oci8_link(oci8_handle_t *parent, oci8_handle_t *child)
{
  int i;
  int new_size;

  if (parent == NULL)
    return;
  oci8_unlink(child);
  child->parent = parent;
  child->envh = parent->envh;

  for (i = 0;i < parent->size;i++) {
    if (parent->children[i] == NULL) {
      parent->children[i] = child;
      return;
    }
  }
  new_size = parent->size + CHILDREN_ARRAY_GROW_SIZE;
  parent->children = xrealloc(parent->children, sizeof(oci8_handle_t *) * new_size);
  parent->children[parent->size] = child;
  for (i = parent->size + 1;i < new_size;i++) {
    parent->children[i] = NULL;
  }
  parent->size = new_size;
  return;
}

void oci8_unlink(oci8_handle_t *self)
{
  oci8_handle_t *parent = self->parent;
  int i;

  if (self->parent == NULL)
    return;
  for (i = 0;i < parent->size;i++) {
    if (parent->children[i] == self) {
      parent->children[i] = NULL;
      self->parent = NULL;
      return;
    }
  }
}
