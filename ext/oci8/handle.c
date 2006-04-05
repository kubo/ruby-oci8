/*
  handle.c - part of ruby-oci8

  Copyright (C) 2002-2006 KUBO Takehiro <kubo@jiubao.org>

=begin
== OCIHandle
This is the abstract super class of OCI Handles.
See ((<Class Hierarchy>)).
=end
*/
#include "oci8.h"

static void oci8_handle_do_free(oci8_handle_t *h)
{
  int i;
  if (h->type == 0) {
    return;
  }
  /* free its children recursively.*/
  for (i = 0;i < h->size;i++) {
    if (h->children[i] != NULL)
      oci8_handle_do_free(h->children[i]);
  }
  xfree(h->children);
  /* unlink from parent */
  oci8_unlink(h);
  /* do free */
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
  rb_define_method(cOCIHandle, "free", oci8_handle_free, 0);
  rb_define_method(cOCIHandle, "attrSet", oci8_attr_set, 2);
  rb_define_method(cOCIHandle, "attrGet", oci8_attr_get, 1);
  rb_define_singleton_method(cOCIHandle, "new", oci8_s_new, 0);
}

void oci8_handle_cleanup(oci8_handle_t *h)
{
  oci8_handle_do_free(h);
  xfree(h);
}

VALUE oci8_s_new(VALUE self)
{
  rb_raise(rb_eNameError, "private method `new' called for %s:Class", rb_class2name(self));
}


static void oci8_handle_mark(oci8_handle_t *h)
{
  oci8_bind_handle_t *bh;
  int i;

  switch (h->type) {
  case OCI_HTYPE_SVCCTX:
    for (i = 0;i < h->size;i++) {
      if (h->children[i] != NULL) {
	if (h->children[i]->type == OCI_HTYPE_SERVER || h->children[i]->type == OCI_HTYPE_SESSION) {
	  rb_gc_mark(h->children[i]->self);
	}
      }
    }
    break;
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
    if (bh->bind_type == BIND_HANDLE)
      rb_gc_mark(bh->value.v);
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
  oci8_bind_handle_t *bh;

  switch (type) {
  case OCI_HTYPE_ENV:
    obj = Data_Make_Struct(cOCIEnv, oci8_handle_t, oci8_handle_mark, oci8_handle_cleanup, h);
    break;
  case OCI_HTYPE_SVCCTX:
    obj = Data_Make_Struct(cOCISvcCtx, oci8_handle_t, oci8_handle_mark, oci8_handle_cleanup, h);
    break;
  case OCI_HTYPE_STMT:
    obj = Data_Make_Struct(cOCIStmt, oci8_handle_t, oci8_handle_mark, oci8_handle_cleanup, h);
    rb_ivar_set(obj, oci8_id_define_array, Qnil);
    rb_ivar_set(obj, oci8_id_bind_hash, Qnil);
    break;
  case OCI_HTYPE_SERVER:
    obj = Data_Make_Struct(cOCIServer, oci8_handle_t, oci8_handle_mark, oci8_handle_cleanup, h);
    break;
  case OCI_HTYPE_SESSION:
    obj = Data_Make_Struct(cOCISession, oci8_handle_t, oci8_handle_mark, oci8_handle_cleanup, h);
    break;
  case OCI_HTYPE_DESCRIBE:
    obj = Data_Make_Struct(cOCIDescribe, oci8_handle_t, oci8_handle_mark, oci8_handle_cleanup, h);
    break;
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
  case OCI_HTYPE_BIND:
    bh = xmalloc(sizeof(oci8_bind_handle_t) - sizeof(bh->value) + value_sz);
    obj = Data_Wrap_Struct(cOCIBind, oci8_handle_mark, oci8_handle_cleanup, bh);
    bh->bind_type = 0;
    bh->ind = -1;
    bh->rlen = value_sz;
    bh->value_sz = value_sz;
    h = (oci8_handle_t *)bh;
    break;
  case OCI_HTYPE_DEFINE:
    bh = xmalloc(sizeof(oci8_bind_handle_t) - sizeof(bh->value) + value_sz);
    obj = Data_Wrap_Struct(cOCIDefine, oci8_handle_mark, oci8_handle_cleanup, bh);
    bh->bind_type = 0;
    bh->ind = -1;
    bh->rlen = value_sz;
    bh->value_sz = value_sz;
    h = (oci8_handle_t *)bh;
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
