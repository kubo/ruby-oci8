/* -*- c-file-style: "ruby"; indent-tabs-mode: nil -*- */
#include "oci8.h"

#if !BUILD_FOR_ORACLE_10_1
#error available on Oracle 10.1 or later.
#endif

#ifndef HAVE_XMLOTN_H
/* declarations in xmlproc.h of Oracle XML Development Kit */
typedef xmlnode xmlelemnode;
typedef xmlnode xmlpinode;
typedef xmlnode xmldocnode;
typedef xmlnode xmldtdnode;
typedef xmlnode xmlnotenode;
typedef xmlnode xmlnamedmap;
typedef xmlnode xmlattrnode;

typedef enum {
    XMLERR_OK = 0,
} xmlerr;

typedef enum {
    XMLDOM_NONE     = 0,  /* bogus node */
    XMLDOM_ELEM     = 1,  /* element */
    XMLDOM_ATTR     = 2,  /* attribute */
    XMLDOM_TEXT     = 3,  /* char data not escaped by CDATA */
    XMLDOM_CDATA    = 4,  /* char data escaped by CDATA */
    XMLDOM_ENTREF   = 5,  /* entity reference */
    XMLDOM_ENTITY   = 6,  /* entity */
    XMLDOM_PI       = 7,  /* <?processing instructions?> */
    XMLDOM_COMMENT  = 8,  /* <!-- Comments --> */
    XMLDOM_DOC      = 9,  /* Document */
    XMLDOM_DTD      = 10, /* DTD */
    XMLDOM_FRAG     = 11, /* Document fragment */
    XMLDOM_NOTATION = 12, /* notation */
    XMLDOM_ELEMDECL = 13, /* DTD element declaration */
    XMLDOM_ATTRDECL = 14, /* DTD attribute declaration */
    XMLDOM_CPELEM   = 15, /* element */
    XMLDOM_CPCHOICE = 16, /* choice (a|b) */
    XMLDOM_CPSEQ    = 17, /* sequence (a,b) */
    XMLDOM_CPPCDATA = 18, /* #PCDATA */
    XMLDOM_CPSTAR   = 19, /* '*' (zero or more) */
    XMLDOM_CPPLUS   = 20, /* '+' (one or more) */
    XMLDOM_CPOPT    = 21, /* '?' (optional) */
    XMLDOM_CPEND    = 22  /* end marker */
} xmlnodetype;

typedef struct xmldomcb {
    void *cb[1];
} xmldomcb;

typedef struct xmlctxhead
{
    ub4           cw_xmlctxhead;        /* checkword */
    oratext      *name_xmlctxhead;      /* name for context */
    void         *cb_xmlctxhead;        /* top-level function callbacks */
    xmldomcb     *domcb_xmlctxhead;     /* DOM function callbacks */
} xmlctxhead;
#define XML_DOMCB(xctx) ((xmlctxhead *) xctx)->domcb_xmlctxhead

typedef xmlerr (*XmlDomGetDecl_t)(xmlctx *xctx, xmldocnode *doc, oratext **ver, oratext **enc, sb4 *std);
#define XmlDomGetDecl(xctx, doc, ver, enc, std) \
      ((XmlDomGetDecl_t)XML_DOMCB(xctx)->cb[0])((xctx), (doc), (ver), (enc), (std))

typedef xmldtdnode* (*XmlDomGetDTD_t)(xmlctx *xctx, xmldocnode *doc);
#define XmlDomGetDTD(xctx, doc) \
      ((XmlDomGetDTD_t)XML_DOMCB(xctx)->cb[3])((xctx), (doc))

typedef xmlnodetype (*XmlDomGetNodeType_t)(xmlctx *xctx, xmlnode *node);
#define XmlDomGetNodeType(xctx, node) \
      ((XmlDomGetNodeType_t)XML_DOMCB(xctx)->cb[34])((xctx), (node))

typedef xmlnode* (*XmlDomGetFirstChild_t)(xmlctx *xctx, xmlnode *node);
#define XmlDomGetFirstChild(xctx, node) \
      ((XmlDomGetFirstChild_t)XML_DOMCB(xctx)->cb[46])((xctx), (node))

typedef xmlnode* (*XmlDomGetNextSibling_t)(xmlctx *xctx, xmlnode *node);
#define XmlDomGetNextSibling(xctx, node) \
      ((XmlDomGetNextSibling_t)XML_DOMCB(xctx)->cb[53])((xctx), (node))

typedef xmlnamedmap* (*XmlDomGetAttrs_t)(xmlctx *xctx, xmlelemnode *elem);
#define XmlDomGetAttrs(xctx, node) \
    ((XmlDomGetAttrs_t)XML_DOMCB(xctx)->cb[55])((xctx), (node))

typedef xmlnode* (*XmlDomGetNodeMapItem_t)(xmlctx *xctx, xmlnamedmap *map, ub4 index);
#define XmlDomGetNodeMapItem(xctx, map, index) \
      ((XmlDomGetNodeMapItem_t)XML_DOMCB(xctx)->cb[82])((xctx), (map), (index))

typedef ub4 (*XmlDomGetNodeMapLength_t)(xmlctx *xctx, xmlnamedmap *map);
#define XmlDomGetNodeMapLength(xctx, map) \
      ((XmlDomGetNodeMapLength_t)XML_DOMCB(xctx)->cb[83])((xctx), (map))

typedef oratext* (*XmlDomGetCharData_t)(xmlctx *xctx, xmlnode *node);
#define XmlDomGetCharData(xctx, node) \
      ((XmlDomGetCharData_t)XML_DOMCB(xctx)->cb[90])((xctx), (node))

typedef oratext* (*XmlDomGetAttrName_t)(xmlctx *xctx, xmlattrnode *attr);
#define XmlDomGetAttrName(xctx, attr) \
      ((XmlDomGetAttrName_t)XML_DOMCB(xctx)->cb[98])((xctx), (attr))

typedef oratext* (*XmlDomGetAttrValue_t)(xmlctx *xctx, xmlattrnode *attr);
#define XmlDomGetAttrValue(xctx, attr) \
      ((XmlDomGetAttrValue_t)XML_DOMCB(xctx)->cb[106])((xctx), (attr))

typedef oratext* (*XmlDomGetTag_t)(xmlctx *xctx, xmlelemnode *elem);
#define XmlDomGetTag(xctx, elem) \
      ((XmlDomGetTag_t)XML_DOMCB(xctx)->cb[112])((xctx), (elem))

typedef oratext* (*XmlDomGetDTDName_t)(xmlctx *xctx, xmldtdnode *dtd);
#define XmlDomGetDTDName(xctx, dtd) \
      ((XmlDomGetDTDName_t)XML_DOMCB(xctx)->cb[131])((xctx), (dtd))

typedef xmlnamedmap* (*XmlDomGetDTDEntities_t)(xmlctx *xctx, xmldtdnode *dtd);
#define XmlDomGetDTDEntities(xctx, dtd) \
      ((XmlDomGetDTDEntities_t)XML_DOMCB(xctx)->cb[132])((xctx), (dtd))

typedef xmlnamedmap* (*XmlDomGetDTDNotations_t)(xmlctx *xctx, xmldtdnode *dtd);
#define XmlDomGetDTDNotations(xctx, dtd) \
      ((XmlDomGetDTDNotations_t)XML_DOMCB(xctx)->cb[133])((xctx), (dtd))

typedef oratext* (*XmlDomGetDTDPubID_t)(xmlctx *xctx, xmldtdnode *dtd);
#define XmlDomGetDTDPubID(xctx, dtd) \
      ((XmlDomGetDTDPubID_t)XML_DOMCB(xctx)->cb[134])((xctx), (dtd))

typedef oratext* (*XmlDomGetDTDSysID_t)(xmlctx *xctx, xmldtdnode *dtd);
#define XmlDomGetDTDSysID(xctx, dtd) \
      ((XmlDomGetDTDSysID_t)XML_DOMCB(xctx)->cb[135])((xctx), (dtd))

typedef oratext* (*XmlDomGetPITarget_t)(xmlctx *xctx, xmlpinode *pi);
#define XmlDomGetPITarget(xctx, pi) \
      ((XmlDomGetPITarget_t)XML_DOMCB(xctx)->cb[143])((xctx), (pi))

typedef oratext* (*XmlDomGetPIData_t)(xmlctx *xctx, xmlpinode *pi);
#define XmlDomGetPIData(xctx, pi) \
      ((XmlDomGetPIData_t)XML_DOMCB(xctx)->cb[144])((xctx), (pi))

/* end of declarations in xmlproc.h of Oracle XML Development Kit */
#endif /* HAVE_XMLOTN_H */

static ID id_add;
static ID id_add_attribute;
static VALUE REXML_Element;
static VALUE REXML_Text;
static VALUE REXML_CData;
static VALUE REXML_Instruction;
static VALUE REXML_Comment;
static VALUE REXML_Document;
static VALUE REXML_XMLDecl;
static VALUE REXML_DocType;
static VALUE REXML_NotationDecl;

static VALUE oci8_make_element(struct xmlctx *xctx, xmlelemnode *elem);
static VALUE oci8_make_cdata(struct xmlctx *xctx, xmlnode *node);
static VALUE oci8_make_text(struct xmlctx *xctx, xmlnode *node);
static VALUE oci8_make_pi(struct xmlctx *xctx, xmlpinode *pi);
static VALUE oci8_make_comment(struct xmlctx *xctx, xmlnode *node);
static VALUE oci8_make_document(struct xmlctx *xctx, xmldocnode *doc);
static VALUE oci8_make_dtd(struct xmlctx *xctx, xmldtdnode *dtd);

static VALUE add_child_nodes(VALUE obj, struct xmlctx *xctx, xmlnode *node);
static VALUE add_attributes(VALUE obj, struct xmlctx *xctx, xmlnode *node);
static VALUE add_nodemap(VALUE obj, struct xmlctx *xctx, xmlnamedmap *map);

void Init_oci_xmldb(void)
{
    id_add = rb_intern("add");
    id_add_attribute = rb_intern("add_attribute");
    rb_require("rexml/document");
    REXML_Document = rb_eval_string("REXML::Document");
    REXML_Element = rb_eval_string("REXML::Element");
    REXML_Text = rb_eval_string("REXML::Text");
    REXML_CData = rb_eval_string("REXML::CData");
    REXML_Instruction = rb_eval_string("REXML::Instruction");
    REXML_Comment = rb_eval_string("REXML::Comment");
    REXML_XMLDecl = rb_eval_string("REXML::XMLDecl");
    REXML_DocType = rb_eval_string("REXML::DocType");
    REXML_NotationDecl = rb_eval_string("REXML::NotationDecl");
}

VALUE oci8_make_rexml(struct xmlctx *xctx, xmlnode *node)
{
    xmlnodetype nodetype = XmlDomGetNodeType(xctx, node);

    switch (nodetype) {
    case XMLDOM_NONE:     /*  0 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_NONE");
    case XMLDOM_ELEM:     /*  1 */
        return oci8_make_element(xctx, (xmlelemnode*)node);
    case XMLDOM_ATTR:     /*  2 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_ATTR");
    case XMLDOM_TEXT:     /*  3 */
        return oci8_make_text(xctx, node);
    case XMLDOM_CDATA:    /*  4 */
        return oci8_make_cdata(xctx, node);
    case XMLDOM_ENTREF:   /*  5 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_ENTREF");
    case XMLDOM_ENTITY:   /*  6 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_ENTITY");
    case XMLDOM_PI:       /*  7 */
        return oci8_make_pi(xctx, (xmlpinode*)node);
    case XMLDOM_COMMENT:  /*  8 */
        return oci8_make_comment(xctx, node);
    case XMLDOM_DOC:      /*  9 */
        return oci8_make_document(xctx, (xmldocnode*)node);
    case XMLDOM_DTD:      /* 10 */
        return oci8_make_dtd(xctx, (xmldtdnode*)node);
    case XMLDOM_FRAG:     /* 11 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_FRAG");
    case XMLDOM_NOTATION: /* 12 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_NOTATION");
    case XMLDOM_ELEMDECL: /* 13 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_ELEMDECL");
    case XMLDOM_ATTRDECL: /* 14 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_ATTRDECL");
    case XMLDOM_CPELEM:   /* 15 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_CPELEM");
    case XMLDOM_CPCHOICE: /* 16 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_CPCHOICE");
    case XMLDOM_CPSEQ:    /* 17 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_CPSEQ");
    case XMLDOM_CPPCDATA: /* 18 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_CPPCDATA");
    case XMLDOM_CPSTAR:   /* 19 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_CPSTAR");
    case XMLDOM_CPPLUS:   /* 20 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_CPPLUS");
    case XMLDOM_CPOPT:    /* 21 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_CPOPT");
    case XMLDOM_CPEND:    /* 22 */
        rb_raise(rb_eRuntimeError, "unsupported XML node type: XMLDOM_CPEND");
    }
    rb_raise(rb_eRuntimeError, "unsupported XML node type: %d", nodetype);
}

static VALUE oci8_make_element(struct xmlctx *xctx, xmlelemnode *elem)
{
    oratext *name;
    VALUE obj;

    name = XmlDomGetTag(xctx, elem);
    obj = rb_funcall(REXML_Element, oci8_id_new, 1, rb_str_new2_ora(name));
    add_attributes(obj, xctx, (xmlnode*)elem);
    return add_child_nodes(obj, xctx, (xmlnode*)elem);
}

static VALUE oci8_make_text(struct xmlctx *xctx, xmlnode *node)
{
    oratext *data = XmlDomGetCharData(xctx, node);
    return rb_funcall(REXML_Text, oci8_id_new, 1, rb_str_new2_ora(data));
}

static VALUE oci8_make_cdata(struct xmlctx *xctx, xmlnode *node)
{
    oratext *data = XmlDomGetCharData(xctx, node);
    return rb_funcall(REXML_CData, oci8_id_new, 1, rb_str_new2_ora(data));
}

static VALUE oci8_make_pi(struct xmlctx *xctx, xmlpinode *pi)
{
    oratext *target;
    oratext *data;

    target= XmlDomGetPITarget(xctx, pi);
    data = XmlDomGetPIData(xctx, pi);
    return rb_funcall(REXML_Instruction, oci8_id_new, 2, rb_str_new2_ora(target), data ? rb_str_new2_ora(data) : Qnil);
}

static VALUE oci8_make_comment(struct xmlctx *xctx, xmlnode *node)
{
    oratext *data = XmlDomGetCharData(xctx, node);
    return rb_funcall(REXML_Comment, oci8_id_new, 1, rb_str_new2_ora(data));
}

static VALUE oci8_make_document(struct xmlctx *xctx, xmldocnode *doc)
{
    xmlerr err;
    oratext *ver;
    oratext *enc;
    sb4 std;
#ifdef ENABLE_DTD
    xmldtdnode* dtd;
#endif
    VALUE obj;
    VALUE decl;

    obj = rb_funcall(REXML_Document, oci8_id_new, 0);
    err = XmlDomGetDecl(xctx, doc, &ver, &enc, &std);
    if (err == XMLERR_OK) {
        decl = rb_funcall(REXML_XMLDecl, oci8_id_new, 3,
                          ver ? rb_str_new2_ora(ver) : Qnil,
                          enc ? rb_str_new2_ora(enc) : Qnil,
                          (std < 0) ? Qnil : ((std > 0) ? rb_str_new2("yes") : rb_str_new2("no")));
        rb_funcall(obj, id_add, 1, decl);
    }
#ifdef ENABLE_DTD
    dtd = XmlDomGetDTD(xctx, doc);
    if (dtd != NULL) {
        rb_funcall(obj, id_add, 1, oci8_make_dtd(xctx, dtd));
    }
#endif
    return add_child_nodes(obj, xctx, (xmlnode*)doc);
}

static VALUE oci8_make_dtd(struct xmlctx *xctx, xmldtdnode *dtd)
{
    /*
     * DTD support is not finished.
     * I don't know how to get full dtd data from xmldtdnode.
     */
    oratext *name;
    oratext *pubid;
    oratext *sysid;
    xmlnamedmap *entities;
    xmlnamedmap *notations;
    VALUE obj;

    name = XmlDomGetDTDName(xctx, dtd);
    pubid = XmlDomGetDTDPubID(xctx, dtd);
    sysid = XmlDomGetDTDSysID(xctx, dtd);
    entities = XmlDomGetDTDEntities(xctx, dtd);
    notations = XmlDomGetDTDNotations(xctx, dtd);

    obj = rb_funcall(REXML_DocType, oci8_id_new, 1, rb_str_new2_ora(name));
    if (entities != NULL)
        add_nodemap(obj, xctx, entities);
    if (notations != NULL)
        add_nodemap(obj, xctx, notations);
    return obj;
}

static VALUE add_child_nodes(VALUE obj, struct xmlctx *xctx, xmlnode *node)
{
    node = XmlDomGetFirstChild(xctx, node);
    while (node != NULL) {
        rb_funcall(obj, id_add, 1, oci8_make_rexml(xctx, node));
        node = XmlDomGetNextSibling(xctx, node);
    }
    return obj;
}

static VALUE add_attributes(VALUE obj, struct xmlctx *xctx, xmlnode *node)
{
    xmlnamedmap *attrs;
    xmlnode *attr;
    oratext *name;
    oratext *value;
    ub4 num;
    ub4 idx;

    attrs = XmlDomGetAttrs(xctx, node);
    num = XmlDomGetNodeMapLength(xctx, attrs);
    for (idx = 0; idx < num; idx++) {
        attr = XmlDomGetNodeMapItem(xctx, attrs, idx);
        name = XmlDomGetAttrName(xctx, attr);
        value = XmlDomGetAttrValue(xctx, attr);
        rb_funcall(obj, id_add_attribute, 2, 
                   rb_str_new2_ora(name),
                   value ? rb_str_new2_ora(value) : Qnil);
    }
    return obj;
}

static VALUE add_nodemap(VALUE obj, struct xmlctx *xctx, xmlnamedmap *map)
{
    xmlnode *node;
    ub4 num;
    ub4 idx;

    num = XmlDomGetNodeMapLength(xctx, map);
    for (idx = 0; idx < num; idx++) {
        node = XmlDomGetNodeMapItem(xctx, map, idx);
        rb_funcall(obj, id_add, 1, oci8_make_rexml(xctx, node));
    }
    return obj;
}
