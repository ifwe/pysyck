
#include <Python.h>
#include <syck.h>

/****************************************************************************
 * Python 2.2 compatibility.
 ****************************************************************************/

#ifndef PyDoc_STR
#define PyDoc_VAR(name)         static char name[]
#define PyDoc_STR(str)          (str)
#define PyDoc_STRVAR(name, str) PyDoc_VAR(name) = PyDoc_STR(str)
#endif

#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC  void
#endif

/****************************************************************************
 * Global objects: _syck.error, 'scalar', 'seq', 'map',
 * '1quote', '2quote', 'fold', 'literal', 'plain', '+', '-'.
 ****************************************************************************/

static PyObject *PySyck_Error;

static PyObject *PySyck_ScalarKind;
static PyObject *PySyck_SeqKind;
static PyObject *PySyck_MapKind;

static PyObject *PySyck_1QuoteStyle;
static PyObject *PySyck_2QuoteStyle;
static PyObject *PySyck_FoldStyle;
static PyObject *PySyck_LiteralStyle;
static PyObject *PySyck_PlainStyle;

static PyObject *PySyck_StripChomp;
static PyObject *PySyck_KeepChomp;

/****************************************************************************
 * The type _syck.Node.
 ****************************************************************************/

PyDoc_STRVAR(PySyckNode_doc,
    "_syck.Node() -> TypeError\n\n"
    "_syck.Node is an abstract type. It is a base type for _syck.Scalar,\n"
    "_syck.Seq, and _syck.Map. You cannot create an instance of _syck.Node\n"
    "directly. You may use _syck.Node for type checking or subclassing.\n");

typedef struct {
    PyObject_HEAD
    /* Common fields for all Node types: */
    PyObject *value;    /* always an object */
    PyObject *tag;      /* a string object or NULL */
    PyObject *anchor;   /* a string object or NULL */
} PySyckNodeObject;


static int
PySyckNode_clear(PySyckNodeObject *self)
{
    PyObject *tmp;

    tmp = self->value;
    self->value = NULL;
    Py_XDECREF(tmp);

    tmp = self->tag;
    self->tag = NULL;
    Py_XDECREF(tmp);

    tmp = self->anchor;
    self->value = NULL;
    Py_XDECREF(tmp);

    return 0;
}

static int
PySyckNode_traverse(PySyckNodeObject *self, visitproc visit, void *arg)
{
    int ret;

    if (self->value)
        if ((ret = visit(self->value, arg)) != 0)
            return ret;

    if (self->tag)
        if ((ret = visit(self->tag, arg)) != 0)
            return ret;

    if (self->anchor)
        if ((ret = visit(self->anchor, arg)) != 0)
            return ret;

    return 0;
}

static void
PySyckNode_dealloc(PySyckNodeObject *self)
{
    PySyckNode_clear(self);
    self->ob_type->tp_free((PyObject *)self);
}

static PyObject *
PySyckNode_getkind(PySyckNodeObject *self, PyObject **closure)
{
    Py_INCREF(*closure);
    return *closure;
}

static PyObject *
PySyckNode_getvalue(PySyckNodeObject *self, void *closure)
{
    Py_INCREF(self->value);
    return self->value;
}

static PyObject *
PySyckNode_gettag(PySyckNodeObject *self, void *closure)
{
    PyObject *value = self->tag ? self->tag : Py_None;
    Py_INCREF(value);
    return value;
}

static int
PySyckNode_settag(PySyckNodeObject *self, PyObject *value, void *closure)
{
    if (!value) {
        PyErr_SetString(PyExc_TypeError, "cannot delete 'tag'");
        return -1;
    }

    if (value == Py_None) {
        Py_XDECREF(self->tag);
        self->tag = NULL;
        return 0;
    }

    if (!PyString_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "'tag' must be a string");
        return -1;
    }

    Py_XDECREF(self->tag);
    Py_INCREF(value);
    self->tag = value;

    return 0;
}

static PyObject *
PySyckNode_getanchor(PySyckNodeObject *self, void *closure)
{
    PyObject *value = self->anchor ? self->anchor : Py_None;
    Py_INCREF(value);
    return value;
}

static int
PySyckNode_setanchor(PySyckNodeObject *self, PyObject *value, void *closure)
{
    if (!value) {
        PyErr_SetString(PyExc_TypeError, "cannot delete 'anchor'");
        return -1;
    }

    if (value == Py_None) {
        Py_XDECREF(self->anchor);
        self->anchor = NULL;
        return 0;
    }

    if (!PyString_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "'anchor' must be a string");
        return -1;
    }

    Py_XDECREF(self->anchor);
    Py_INCREF(value);
    self->anchor = value;

    return 0;
}

static PyTypeObject PySyckNode_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "_syck.Node",                               /* tp_name */
    sizeof(PySyckNodeObject),                   /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)PySyckNode_dealloc,             /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_GC,  /* tp_flags */
    PySyckNode_doc,                             /* tp_doc */
    (traverseproc)PySyckNode_traverse,          /* tp_traverse */
    (inquiry)PySyckNode_clear,                  /* tp_clear */
};

/****************************************************************************
 * The type _syck.Scalar.
 ****************************************************************************/

PyDoc_STRVAR(PySyckScalar_doc,
    "Scalar(value='', tag=None, style=None, indent=0, width=0, chomp=None)\n"
    "      -> a Scalar node\n\n"
    "_syck.Scalar represents a scalar node in Syck parser and emitter\n"
    "graphs. A scalar node points to a single string value.\n");

typedef struct {
    PyObject_HEAD
    /* Common fields for all Node types: */
    PyObject *value;    /* always a string object */
    PyObject *tag;      /* a string object or NULL */
    PyObject *anchor;   /* a string object or NULL */
    /* Scalar-specific fields: */
    enum scalar_style style;
    int indent;
    int width;
    char chomp;
} PySyckScalarObject;

static PyObject *
PySyckScalar_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PySyckScalarObject *self;

    self = (PySyckScalarObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;

    self->value = PyString_FromString("");
    if (!self->value) {
        Py_DECREF(self);
        return NULL;
    }

    self->tag = NULL;
    self->anchor = NULL;
    self->style = scalar_none;
    self->indent = 0;
    self->width = 0;
    self->chomp = 0;

    return (PyObject *)self;
}

static int
PySyckScalar_setvalue(PySyckScalarObject *self, PyObject *value, void *closure)
{
    if (!value) {
        PyErr_SetString(PyExc_TypeError, "cannot delete 'value'");
        return -1;
    }
    if (!PyString_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "'value' must be a string");
        return -1;
    }

    Py_DECREF(self->value);
    Py_INCREF(value);
    self->value = value;

    return 0;
}

static PyObject *
PySyckScalar_getstyle(PySyckScalarObject *self, void *closure)
{
    PyObject *value;

    switch (self->style) {
        case scalar_1quote: value = PySyck_1QuoteStyle; break;
        case scalar_2quote: value = PySyck_2QuoteStyle; break;
        case scalar_fold: value = PySyck_FoldStyle; break;
        case scalar_literal: value = PySyck_LiteralStyle; break;
        case scalar_plain: value = PySyck_PlainStyle; break;
        default: value = Py_None;
    }

    Py_INCREF(value);
    return value;
}

static int
PySyckScalar_setstyle(PySyckScalarObject *self, PyObject *value, void *closure)
{
    char *str;

    if (!value) {
        PyErr_SetString(PyExc_TypeError, "cannot delete 'style'");
        return -1;
    }

    if (value == Py_None) {
        self->style = scalar_none;
        return 0;
    }

    if (!PyString_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "'style' must be a string or None");
        return -1;
    }

    str = PyString_AsString(value);
    if (!str) return -1;

    if (strcmp(str, "1quote") == 0)
        self->style = scalar_1quote;
    else if (strcmp(str, "2quote") == 0)
        self->style = scalar_2quote;
    else if (strcmp(str, "fold") == 0)
        self->style = scalar_fold;
    else if (strcmp(str, "literal") == 0)
        self->style = scalar_literal;
    else if (strcmp(str, "plain") == 0)
        self->style = scalar_plain;
    else {
        PyErr_SetString(PyExc_TypeError, "unknown 'style'");
        return -1;
    }

    return 0;
}

static PyObject *
PySyckScalar_getindent(PySyckScalarObject *self, void *closure)
{
    return PyInt_FromLong(self->indent);
}

static int
PySyckScalar_setindent(PySyckScalarObject *self, PyObject *value, void *closure)
{
    if (!value) {
        PyErr_SetString(PyExc_TypeError, "cannot delete 'indent'");
        return -1;
    }

    if (!PyInt_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "'indent' must be an integer");
        return -1;
    }

    self->indent = PyInt_AS_LONG(value);

    return 0;
}

static PyObject *
PySyckScalar_getwidth(PySyckScalarObject *self, void *closure)
{
    return PyInt_FromLong(self->width);
}

static int
PySyckScalar_setwidth(PySyckScalarObject *self, PyObject *value, void *closure)
{
    if (!value) {
        PyErr_SetString(PyExc_TypeError, "cannot delete 'width'");
        return -1;
    }

    if (!PyInt_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "'width' must be an integer");
        return -1;
    }

    self->width = PyInt_AS_LONG(value);

    return 0;
}

static PyObject *
PySyckScalar_getchomp(PySyckScalarObject *self, void *closure)
{
    PyObject *value;

    switch (self->chomp) {
        case NL_CHOMP: value = PySyck_StripChomp; break;
        case NL_KEEP: value = PySyck_KeepChomp; break;
        default: value = Py_None;
    }

    Py_INCREF(value);
    return value;
}

static int
PySyckScalar_setchomp(PySyckScalarObject *self, PyObject *value, void *closure)
{
    char *str;

    if (!value) {
        PyErr_SetString(PyExc_TypeError, "cannot delete 'chomp'");
        return -1;
    }

    if (value == Py_None) {
        self->chomp = 0;
        return 0;
    }

    if (!PyString_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "'chomp' must be '+', '-', or None");
        return -1;
    }

    str = PyString_AsString(value);
    if (!str) return -1;

    if (strcmp(str, "-") == 0)
        self->chomp = NL_CHOMP;
    else if (strcmp(str, "+") == 0)
        self->chomp = NL_KEEP;
    else {
        PyErr_SetString(PyExc_TypeError, "'chomp' must be '+', '-', or None");
        return -1;
    }

    return 0;
}

static int
PySyckScalar_init(PySyckScalarObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *value = NULL;
    PyObject *tag = NULL;
    PyObject *anchor = NULL;
    PyObject *style = NULL;
    PyObject *indent = NULL;
    PyObject *width = NULL;
    PyObject *chomp = NULL;

    static char *kwdlist[] = {"value", "tag", "anchor",
        "style", "indent", "width", "chomp", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OOOOOOO", kwdlist,
                &value, &tag, &anchor, &style, &indent, &width, &chomp))
        return -1;

    if (value && PySyckScalar_setvalue(self, value, NULL) < 0)
        return -1;

    if (tag && PySyckNode_settag((PySyckNodeObject *)self, tag, NULL) < 0)
        return -1;

    if (anchor && PySyckNode_setanchor((PySyckNodeObject *)self, anchor, NULL) < 0)
        return -1;

    if (style && PySyckScalar_setstyle(self, style, NULL) < 0)
        return -1;

    if (indent && PySyckScalar_setindent(self, indent, NULL) < 0)
        return -1;

    if (width && PySyckScalar_setwidth(self, width, NULL) < 0)
        return -1;

    if (chomp && PySyckScalar_setchomp(self, chomp, NULL) < 0)
        return -1;

    return 0;
}

static PyGetSetDef PySyckScalar_getsetters[] = {
    {"kind", (getter)PySyckNode_getkind, NULL,
        PyDoc_STR("the node kind, always 'scalar', read-only"),
        &PySyck_ScalarKind},
    {"value", (getter)PySyckNode_getvalue, (setter)PySyckScalar_setvalue,
        PyDoc_STR("the node value, a string"), NULL},
    {"tag", (getter)PySyckNode_gettag, (setter)PySyckNode_settag,
        PyDoc_STR("the node tag, a string or None"), NULL},
    {"anchor", (getter)PySyckNode_getanchor, (setter)PySyckNode_setanchor,
        PyDoc_STR("the node anchor, a string or None"), NULL},
    {"style", (getter)PySyckScalar_getstyle, (setter)PySyckScalar_setstyle,
        PyDoc_STR("the node style, values: None (means literal or plain),\n"
            "'1quote', '2quote', 'fold', 'literal', 'plain'"), NULL},
    {"indent", (getter)PySyckScalar_getindent, (setter)PySyckScalar_setindent,
        PyDoc_STR("the node indentation, an integer"), NULL},
    {"width", (getter)PySyckScalar_getwidth, (setter)PySyckScalar_setwidth,
        PyDoc_STR("the node width, an integer"), NULL},
    {"chomp", (getter)PySyckScalar_getchomp, (setter)PySyckScalar_setchomp,
        PyDoc_STR("the chomping method,\n"
            "values: None (clip), '-' (strip), or '+' (keep)"), NULL},
    {NULL}  /* Sentinel */
};

static PyTypeObject PySyckScalar_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "_syck.Scalar",                             /* tp_name */
    sizeof(PySyckScalarObject),                 /* tp_basicsize */
    0,                                          /* tp_itemsize */
    0,                                          /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,     /* tp_flags */
    PySyckScalar_doc,                           /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    0,                                          /* tp_methods */
    0,                                          /* tp_members */
    PySyckScalar_getsetters,                    /* tp_getset */
    &PySyckNode_Type,                           /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)PySyckScalar_init,                /* tp_init */
    0,                                          /* tp_alloc */
    PySyckScalar_new,                           /* tp_new */
};

/****************************************************************************
 * The type _syck.Seq.
 ****************************************************************************/

PyDoc_STRVAR(PySyckSeq_doc,
    "Seq(value=[], tag=None, inline=False) -> a Seq node\n\n"
    "_syck.Seq represents a sequence node in Syck parser and emitter\n"
    "graphs. A sequence node points to an ordered set of subnodes.\n");

typedef struct {
    PyObject_HEAD
    /* Common fields for all Node types: */
    PyObject *value;    /* always an object */
    PyObject *tag;      /* a string object or NULL */
    PyObject *anchor;   /* a string object or NULL */
    /* Seq-specific fields: */
    enum seq_style style;
} PySyckSeqObject;

static PyObject *
PySyckSeq_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PySyckSeqObject *self;

    self = (PySyckSeqObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;

    self->value = PyList_New(0);
    if (!self->value) {
        Py_DECREF(self);
        return NULL;
    }

    self->tag = NULL;
    self->anchor = NULL;
    self->style = seq_none;

    return (PyObject *)self;
}

static int
PySyckSeq_setvalue(PySyckSeqObject *self, PyObject *value, void *closure)
{
    if (!value) {
        PyErr_SetString(PyExc_TypeError, "cannot delete 'value'");
        return -1;
    }
    if (!PySequence_Check(value)) { /* PySequence_Check always succeeds */
        PyErr_SetString(PyExc_TypeError, "'value' must be a sequence");
        return -1;
    }

    Py_DECREF(self->value);
    Py_INCREF(value);
    self->value = value;

    return 0;
}

static PyObject *
PySyckSeq_getinline(PySyckSeqObject *self, void *closure)
{
    PyObject *value = (self->style == seq_inline) ? Py_True : Py_False;

    Py_INCREF(value);
    return value;
}

static int
PySyckSeq_setinline(PySyckSeqObject *self, PyObject *value, void *closure)
{
    if (!value) {
        PyErr_SetString(PyExc_TypeError, "cannot delete 'inline'");
        return -1;
    }

    if (!PyInt_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "'inline' must be a Boolean object");
        return -1;
    }

    self->style = PyInt_AS_LONG(value) ? seq_inline : seq_none;

    return 0;
}

static int
PySyckSeq_init(PySyckSeqObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *value = NULL;
    PyObject *tag = NULL;
    PyObject *anchor = NULL;
    PyObject *inline_ = NULL;

    static char *kwdlist[] = {"value", "tag", "anchor", "inline", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OOOO", kwdlist,
                &value, &tag, &anchor, &inline_))
        return -1;

    if (value && PySyckSeq_setvalue(self, value, NULL) < 0)
        return -1;

    if (tag && PySyckNode_settag((PySyckNodeObject *)self, tag, NULL) < 0)
        return -1;

    if (anchor && PySyckNode_setanchor((PySyckNodeObject *)self, anchor, NULL) < 0)
        return -1;

    if (inline_ && PySyckSeq_setinline(self, inline_, NULL) < 0)
        return -1;

    return 0;
}

static PyGetSetDef PySyckSeq_getsetters[] = {
    {"kind", (getter)PySyckNode_getkind, NULL,
        PyDoc_STR("the node kind, always 'seq', read-only"), &PySyck_SeqKind},
    {"value", (getter)PySyckNode_getvalue, (setter)PySyckSeq_setvalue,
        PyDoc_STR("the node value, a sequence"), NULL},
    {"tag", (getter)PySyckNode_gettag, (setter)PySyckNode_settag,
        PyDoc_STR("the node tag, a string or None"), NULL},
    {"anchor", (getter)PySyckNode_getanchor, (setter)PySyckNode_setanchor,
        PyDoc_STR("the node anchor, a string or None"), NULL},
    {"inline", (getter)PySyckSeq_getinline, (setter)PySyckSeq_setinline,
        PyDoc_STR("the block/flow flag"), NULL},
    {NULL}  /* Sentinel */
};

static PyTypeObject PySyckSeq_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "_syck.Seq",                                /* tp_name */
    sizeof(PySyckSeqObject),                    /* tp_basicsize */
    0,                                          /* tp_itemsize */
    0,                                          /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_GC,  /* tp_flags */
    PySyckSeq_doc,                              /* tp_doc */
    (traverseproc)PySyckNode_traverse,          /* tp_traverse */
    (inquiry)PySyckNode_clear,                  /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    0,                                          /* tp_methods */
    0,                                          /* tp_members */
    PySyckSeq_getsetters,                       /* tp_getset */
    &PySyckNode_Type,                           /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)PySyckSeq_init,                   /* tp_init */
    0,                                          /* tp_alloc */
    PySyckSeq_new,                              /* tp_new */
};

/****************************************************************************
 * The type _syck.Map.
 ****************************************************************************/

PyDoc_STRVAR(PySyckMap_doc,
    "Map(value='', tag=None, inline=False) -> a Map node\n\n"
    "_syck.Map represents a mapping node in Syck parser and emitter\n"
    "graphs. A mapping node points to an unordered collections of pairs.\n");

typedef struct {
    PyObject_HEAD
    /* Common fields for all Node types: */
    PyObject *value;    /* always an object */
    PyObject *tag;      /* a string object or NULL */
    PyObject *anchor;   /* a string object or NULL */
    /* Map-specific fields: */
    enum map_style style;
} PySyckMapObject;

static PyObject *
PySyckMap_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PySyckMapObject *self;

    self = (PySyckMapObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;

    self->value = PyDict_New();
    if (!self->value) {
        Py_DECREF(self);
        return NULL;
    }

    self->tag = NULL;
    self->anchor = NULL;
    self->style = seq_none;

    return (PyObject *)self;
}

static int
PySyckMap_setvalue(PySyckMapObject *self, PyObject *value, void *closure)
{
    if (!value) {
        PyErr_SetString(PyExc_TypeError, "cannot delete 'value'");
        return -1;
    }
    if (!PyMapping_Check(value)) { /* PyMapping_Check always succeeds */
        PyErr_SetString(PyExc_TypeError, "'value' must be a mapping");
        return -1;
    }

    Py_DECREF(self->value);
    Py_INCREF(value);
    self->value = value;

    return 0;
}

static PyObject *
PySyckMap_getinline(PySyckMapObject *self, void *closure)
{
    PyObject *value = (self->style == map_inline) ? Py_True : Py_False;

    Py_INCREF(value);
    return value;
}

static int
PySyckMap_setinline(PySyckMapObject *self, PyObject *value, void *closure)
{
    if (!value) {
        PyErr_SetString(PyExc_TypeError, "cannot delete 'inline'");
        return -1;
    }

    if (!PyInt_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "'inline' must be a Boolean object");
        return -1;
    }

    self->style = PyInt_AS_LONG(value) ? map_inline : map_none;

    return 0;
}

static int
PySyckMap_init(PySyckMapObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *value = NULL;
    PyObject *tag = NULL;
    PyObject *anchor = NULL;
    PyObject *inline_ = NULL;

    static char *kwdlist[] = {"value", "tag", "anchor", "inline", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OOOO", kwdlist,
                &value, &tag, &anchor, &inline_))
        return -1;

    if (value && PySyckMap_setvalue(self, value, NULL) < 0)
        return -1;

    if (tag && PySyckNode_settag((PySyckNodeObject *)self, tag, NULL) < 0)
        return -1;

    if (anchor && PySyckNode_setanchor((PySyckNodeObject *)self, anchor, NULL) < 0)
        return -1;

    if (inline_ && PySyckMap_setinline(self, inline_, NULL) < 0)
        return -1;

    return 0;
}

static PyGetSetDef PySyckMap_getsetters[] = {
    {"kind", (getter)PySyckNode_getkind, NULL,
        PyDoc_STR("the node kind, always 'map', read-only"), &PySyck_MapKind},
    {"value", (getter)PySyckNode_getvalue, (setter)PySyckMap_setvalue,
        PyDoc_STR("the node value, a mapping"), NULL},
    {"tag", (getter)PySyckNode_gettag, (setter)PySyckNode_settag,
        PyDoc_STR("the node tag, a string or None"), NULL},
    {"anchor", (getter)PySyckNode_getanchor, (setter)PySyckNode_setanchor,
        PyDoc_STR("the node anchor, a string or None"), NULL},
    {"inline", (getter)PySyckMap_getinline, (setter)PySyckMap_setinline,
        PyDoc_STR("the block/flow flag"), NULL},
    {NULL}  /* Sentinel */
};

static PyTypeObject PySyckMap_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "_syck.Map",                                /* tp_name */
    sizeof(PySyckMapObject),                    /* tp_basicsize */
    0,                                          /* tp_itemsize */
    0,                                          /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_GC,  /* tp_flags */
    PySyckMap_doc,                              /* tp_doc */
    (traverseproc)PySyckNode_traverse,          /* tp_traverse */
    (inquiry)PySyckNode_clear,                  /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    0,                                          /* tp_methods */
    0,                                          /* tp_members */
    PySyckMap_getsetters,                       /* tp_getset */
    &PySyckNode_Type,                           /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)PySyckMap_init,                   /* tp_init */
    0,                                          /* tp_alloc */
    PySyckMap_new,                              /* tp_new */
};

/****************************************************************************
 * The type _syck.Parser.
 ****************************************************************************/

PyDoc_STRVAR(PySyckParser_doc,
    "Parser(source, implicit_typing=True, taguri_expansion=True)\n"
    "      -> a Parser object\n\n"
    "_syck.Parser is a low-lever wrapper of the Syck parser. It parses\n"
    "a YAML stream and produces a graph of Nodes.\n");

typedef struct {
    PyObject_HEAD
    /* Attributes: */
    PyObject *source;       /* a string or file-like object */
    int implicit_typing;
    int taguri_expansion;
    /* Internal fields: */
    PyObject *symbols;      /* symbol table, a list, NULL outside parse() */
    SyckParser *parser;
    int parsing;
    int halt;
} PySyckParserObject;

static PyObject *
PySyckParser_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PySyckParserObject *self;

    self = (PySyckParserObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;

    self->source = NULL;
    self->implicit_typing = 0;
    self->taguri_expansion = 0;
    self->symbols = NULL;
    self->parser = NULL;
    self->parsing = 0;
    self->halt = 1;

    /*
    self->symbols = PyList_New(0);
    if (!self->symbols) {
        Py_DECREF(self);
        return NULL;
    }
    */

    return (PyObject *)self;
}

static int
PySyckParser_clear(PySyckParserObject *self)
{
    PyObject *tmp;

    if (self->parser) {
        syck_free_parser(self->parser);
        self->parser = NULL;
    }

    tmp = self->source;
    self->source = NULL;
    Py_XDECREF(tmp);

    tmp = self->symbols;
    self->symbols = NULL;
    Py_XDECREF(tmp);

    return 0;
}

static int
PySyckParser_traverse(PySyckParserObject *self, visitproc visit, void *arg)
{
    int ret;

    if (self->source)
        if ((ret = visit(self->source, arg)) != 0)
            return ret;

    if (self->symbols)
        if ((ret = visit(self->symbols, arg)) != 0)
            return ret;

    return 0;
}

static void
PySyckParser_dealloc(PySyckParserObject *self)
{
    PySyckParser_clear(self);
    self->ob_type->tp_free((PyObject *)self);
}

static PyObject *
PySyckParser_getsource(PySyckParserObject *self, void *closure)
{
    PyObject *value = self->source ? self->source : Py_None;

    Py_INCREF(value);
    return value;
}

static PyObject *
PySyckParser_getimplicit_typing(PySyckParserObject *self, void *closure)
{
    PyObject *value = self->implicit_typing ? Py_True : Py_False;

    Py_INCREF(value);
    return value;
}

static PyObject *
PySyckParser_gettaguri_expansion(PySyckParserObject *self, void *closure)
{
    PyObject *value = self->taguri_expansion ? Py_True : Py_False;

    Py_INCREF(value);
    return value;
}

static PyObject *
PySyckParser_geteof(PySyckParserObject *self, void *closure)
{
    PyObject *value = self->halt ? Py_True : Py_False;

    Py_INCREF(value);
    return value;
}

static PyGetSetDef PySyckParser_getsetters[] = {
    {"source", (getter)PySyckParser_getsource, NULL,
        PyDoc_STR("IO source, a string or file-like object"), NULL},
    {"implicit_typing", (getter)PySyckParser_getimplicit_typing, NULL,
        PyDoc_STR("implicit typing of builtin YAML types"), NULL},
    {"taguri_expansion", (getter)PySyckParser_gettaguri_expansion, NULL,
        PyDoc_STR("expansion of types in full taguri"), NULL},
    {"eof", (getter)PySyckParser_geteof, NULL,
        PyDoc_STR("EOF flag"), NULL},
    {NULL}  /* Sentinel */
};

static SYMID
PySyckParser_node_handler(SyckParser *parser, SyckNode *node)
{
    PySyckParserObject *self = (PySyckParserObject *)parser->bonus;

    SYMID index;
    PySyckNodeObject *object = NULL;

    PyObject *key, *value;
    int k;

    if (self->halt)
        return -1;

    switch (node->kind) {

        case syck_str_kind:
            object = (PySyckNodeObject *)
                PySyckScalar_new(&PySyckScalar_Type, NULL, NULL);
            if (!object) goto error;
            value = PyString_FromStringAndSize(node->data.str->ptr,
                    node->data.str->len);
            if (!value) goto error;
            Py_DECREF(object->value);
            object->value = value;
            break;

        case syck_seq_kind:
            object = (PySyckNodeObject *)
                PySyckSeq_new(&PySyckSeq_Type, NULL, NULL);
            if (!object) goto error;
            for (k = 0; k < node->data.list->idx; k++) {
                index = syck_seq_read(node, k);
                value = PyList_GetItem(self->symbols, index);
                if (!value) goto error;
                if (PyList_Append(object->value, value) < 0)
                    goto error;
            }
            break;

        case syck_map_kind:
            object = (PySyckNodeObject *)
                PySyckMap_new(&PySyckMap_Type, NULL, NULL);
            if (!object) goto error;
            for (k = 0; k < node->data.pairs->idx; k++)
            {
                index = syck_map_read(node, map_key, k);
                key = PyList_GetItem(self->symbols, index);
                if (!key) goto error;
                index = syck_map_read(node, map_value, k);
                value = PyList_GetItem(self->symbols, index);
                if (!value) goto error;
                if (PyDict_SetItem(object->value, key, value) < 0)
                    goto error;
            }
            break;
    }

    if (node->type_id) {
        object->tag = PyString_FromString(node->type_id);
        if (!object->tag) goto error;
    }

    if (node->anchor) {
        object->anchor = PyString_FromString(node->anchor);
        if (!object->anchor) goto error;
    }

    if (PyList_Append(self->symbols, (PyObject *)object) < 0)
        goto error;

    index = PyList_GET_SIZE(self->symbols)-1;
    return index;

error:
    Py_XDECREF(object);
    self->halt = 1;
    return -1;
}

static void
PySyckParser_error_handler(SyckParser *parser, char *str)
{
    PySyckParserObject *self = (PySyckParserObject *)parser->bonus;
    PyObject *value;

    if (self->halt) return;

    self->halt = 1;

    value = Py_BuildValue("(sii)", str,
            parser->linect, parser->cursor - parser->lineptr);
    if (value) {
        PyErr_SetObject(PySyck_Error, value);
    }
}

static long
PySyckParser_read_handler(char *buf, SyckIoFile *file, long max_size, long skip)
{
    PySyckParserObject *self = (PySyckParserObject *)file->ptr;

    PyObject *value;

    char *str;
    int length;

    buf[skip] = '\0';

    if (self->halt) {
        return skip;
    }
    
    max_size -= skip;

    value = PyObject_CallMethod(self->source, "read", "(i)", max_size);
    if (!value) {
        self->halt = 1;
        return skip;
    }

    if (!PyString_CheckExact(value)) {
        Py_DECREF(value);
        PyErr_SetString(PyExc_TypeError, "file-like object should return a string");
        self->halt = 1;
        
        return skip;
    }

    str = PyString_AS_STRING(value);
    length = PyString_GET_SIZE(value);
    if (!length) {
        Py_DECREF(value);
        return skip;
    }

    if (length > max_size) {
        Py_DECREF(value);
        PyErr_SetString(PyExc_ValueError, "read returns an overly long string");
        self->halt = 1;
        return skip;
    }

    memcpy(buf+skip, str, length);
    length += skip;
    buf[length] = '\0';

    Py_DECREF(value);

    return length;
}

static int
PySyckParser_init(PySyckParserObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *source = NULL;
    int implicit_typing = 1;
    int taguri_expansion = 1;

    static char *kwdlist[] = {"source", "implicit_typing", "taguri_expansion",
        NULL};

    PySyckParser_clear(self);

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|ii", kwdlist,
                &source, &implicit_typing, &taguri_expansion))
        return -1;

    Py_INCREF(source);
    self->source = source;

    self->implicit_typing = implicit_typing;
    self->taguri_expansion = taguri_expansion;

    self->parser = syck_new_parser();
    self->parser->bonus = self;

    if (PyString_CheckExact(self->source)) {
        syck_parser_str(self->parser,
                PyString_AS_STRING(self->source),
                PyString_GET_SIZE(self->source), NULL);
    }
    /*
    else if (PyUnicode_CheckExact(self->source)) {
        syck_parser_str(self->parser,
                PyUnicode_AS_DATA(self->source),
                PyString_GET_DATA_SIZE(self->source), NULL);
    }
    */
    else {
        syck_parser_file(self->parser, (FILE *)self, PySyckParser_read_handler);
    }

    syck_parser_implicit_typing(self->parser, self->implicit_typing);
    syck_parser_taguri_expansion(self->parser, self->taguri_expansion);

    syck_parser_handler(self->parser, PySyckParser_node_handler);
    syck_parser_error_handler(self->parser, PySyckParser_error_handler);
    /*
    syck_parser_bad_anchor_handler(parser, PySyckParser_bad_anchor_handler);
    */

    self->parsing = 0;
    self->halt = 0;

    return 0;
}

static PyObject *
PySyckParser_parse(PySyckParserObject *self)
{
    SYMID index;
    PyObject *value;

    if (self->parsing) {
        PyErr_SetString(PyExc_RuntimeError, "do not call Parser.parse while it is already parsing");
        return NULL;
    }

    if (self->halt) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    self->symbols = PyList_New(0);
    if (!self->symbols) {
        return NULL;
    }

    self->parsing = 1;
    index = syck_parse(self->parser);
    self->parsing = 0;

    if (self->halt || self->parser->eof) {
        Py_DECREF(self->symbols);
        self->symbols = NULL;

        if (self->halt) return NULL;

        self->halt = 1;
        Py_INCREF(Py_None);
        return Py_None;
    }

    value = PyList_GetItem(self->symbols, index);

    Py_DECREF(self->symbols);
    self->symbols = NULL;

    return value;
}

PyDoc_STRVAR(PySyckParser_parse_doc,
    "parse() -> the root Node object\n\n"
    "Parses the source and returns the next document. On EOF, returns None\n"
    "and sets the 'eof' attribute on.\n");

static PyMethodDef PySyckParser_methods[] = {
    {"parse",  (PyCFunction)PySyckParser_parse,
        METH_NOARGS, PySyckParser_parse_doc},
    {NULL}  /* Sentinel */
};

static PyTypeObject PySyckParser_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /* ob_size */
    "_syck.Parser",                             /* tp_name */
    sizeof(PySyckParserObject),                 /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor)PySyckParser_dealloc,           /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_compare */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    0,                                          /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_GC,  /* tp_flags */
    PySyckParser_doc,                           /* tp_doc */
    (traverseproc)PySyckParser_traverse,        /* tp_traverse */
    (inquiry)PySyckParser_clear,                /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    PySyckParser_methods,                       /* tp_methods */
    0,                                          /* tp_members */
    PySyckParser_getsetters,                    /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    (initproc)PySyckParser_init,                /* tp_init */
    0,                                          /* tp_alloc */
    PySyckParser_new,                           /* tp_new */
};



#if 0
    

static PyObject *
PySyckNode_New(char *type_id, PyObject *value) /* Note: steals the reference to the value. */
{
    PySyckNodeObject *self;
    PyObject *kind;

    self = PyObject_NEW(PySyckNodeObject, &PySyckNode_Type);
    if (!self) {
        Py_XDECREF(value);
        return NULL;
    }

    self->value = value;

    if (PyList_Check(value))
        kind = PySyck_SeqKind;
    else if (PyDict_Check(value))
        kind = PySyck_MapKind;
    else
        kind = PySyck_ScalarKind;
    Py_INCREF(kind);
    self->kind = kind;

    if (type_id) {
        self->type_id = PyString_FromString(type_id);
        if (!self->type_id) {
            Py_DECREF(self);
            return NULL;
        }
    }
    else {
        Py_INCREF(Py_None);
        self->type_id = Py_None;
    }

    return (PyObject *)self;
}

/* Parser type. */

typedef struct {
    PyObject_HEAD
    PyObject *source;
    PyObject *resolver;
    PyObject *symbols;
    SyckParser *syck;
    int error;
} PySyckParserObject;

static void
PySyckParser_free(PySyckParserObject *parser)
{
    Py_XDECREF(parser->source);
    parser->source = NULL;
    Py_XDECREF(parser->resolver);
    parser->resolver = NULL;
    Py_XDECREF(parser->symbols);
    parser->symbols = NULL;
    if (parser->syck) {
        syck_free_parser(parser->syck);
        parser->syck = NULL;
    }
}

static PyObject *
PySyckParser_parse(PySyckParserObject *parser, PyObject *args)
{
    SYMID index;
    PyObject *value;

    if (!PyArg_ParseTuple(args, ":parse"))
        return NULL;

    if (!parser->syck) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    if (parser->symbols) {
        PyErr_SetString(PyExc_RuntimeError, "do not call Parser.parse while it is running");
        return NULL;
    }

    parser->symbols = PyList_New(0);
    if (!parser->symbols) {
        return NULL;
    }

    index = syck_parse(parser->syck);

    if (parser->error) {
        PySyckParser_free(parser);
        return NULL;
    }

    if (parser->syck->eof) {
        PySyckParser_free(parser);
        Py_INCREF(Py_None);
        return Py_None;
    }

    value = PyList_GetItem(parser->symbols, index);

    Py_DECREF(parser->symbols);
    parser->symbols = NULL;

    return value;
}

static char PySyckParser_parse_doc[] =
    "Parses the next document in the YAML stream, return the root Node object or None on EOF.";

static PyObject *
PySyckParser_eof(PySyckParserObject *parser, PyObject *args)
{
    PyObject *value;

    if (!PyArg_ParseTuple(args, ":eof"))
        return NULL;

    value = parser->syck ? Py_False : Py_True;

    Py_INCREF(value);
    return value;
}

static char PySyckParser_eof_doc[] =
    "Checks if the parser is stopped.";

static PyMethodDef PySyckParser_methods[] = {
    {"parse",  (PyCFunction)PySyckParser_parse, METH_VARARGS, PySyckParser_parse_doc},
    {"eof",  (PyCFunction)PySyckParser_eof, METH_VARARGS, PySyckParser_eof_doc},
    {NULL}  /* Sentinel */
};

static void
PySyckParser_dealloc(PySyckParserObject *parser)
{
    PySyckParser_free(parser);
    PyObject_Del(parser);
}

static PyObject *
PySyckParser_getattr(PySyckParserObject *parser, char *name)
{
    return Py_FindMethod(PySyckParser_methods, (PyObject *)parser, name);
}

static char PySyckParser_doc[] =
    "_syck.Parser(yaml_string_or_file, resolver=None, implicit_typing=True, taguri_expansion=True) -> Parser object\n"
    "\n"
    "Methods of the Parser object:\n\n"
    "parse() -- Parses the next document in the YAML stream, return the root Node object or None on EOF.\n"
    "eof() -- Checks if the parser is stopped.\n";

static PyTypeObject PySyckParser_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                  /* ob_size */
    "_syck.Parser",                     /* tp_name */
    sizeof(PySyckParserObject),         /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)PySyckParser_dealloc,   /* tp_dealloc */
    0,                                  /* tp_print */
    (getattrfunc)PySyckParser_getattr,  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_compare */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    PySyckParser_doc,                   /* tp_doc */
};


static SYMID
PySyckParser_node_handler(SyckParser *syck, SyckNode *node)
{
    PySyckParserObject *parser = (PySyckParserObject *)syck->bonus;

    SYMID index;
    PyObject *object = NULL;

    PyObject *key, *value, *item;
    int k;

    if (parser->error)
        return -1;

    switch (node->kind) {

        case syck_str_kind:
            object = PyString_FromStringAndSize(node->data.str->ptr,
                    node->data.str->len);
            if (!object) goto error;
            break;

        case syck_seq_kind:
            object = PyList_New(node->data.list->idx);
            if (!object) goto error;
            for (k = 0; k < node->data.list->idx; k++) {
                index = syck_seq_read(node, k);
                item = PyList_GetItem(parser->symbols, index);
                if (!item) goto error;
                Py_INCREF(item);
                PyList_SET_ITEM(object, k, item);
            }
            break;

        case syck_map_kind:
            object = PyDict_New();
            if (!object) goto error;
            for (k = 0; k < node->data.pairs->idx; k++)
            {
                index = syck_map_read(node, map_key, k);
                key = PyList_GetItem(parser->symbols, index);
                if (!key) goto error;
                index = syck_map_read(node, map_value, k);
                value = PyList_GetItem(parser->symbols, index);
                if (!value) goto error;
                if (PyDict_SetItem(object, key, value) < 0)
                    goto error;
            }
            break;
    }

    object = PySyckNode_New(node->type_id, object);
    if (!object) goto error;

    if (parser->resolver) {
        value = PyObject_CallFunction(parser->resolver, "(O)", object);
        if (!value) goto error;
        Py_DECREF(object);
        object = value;
    }

    if (PyList_Append(parser->symbols, object) < 0)
        goto error;

    index = PyList_Size(parser->symbols)-1;
    return index;

error:
    Py_XDECREF(object);
    parser->error = 1;
    return -1;
}


#endif

/* The module _syck. */

static PyMethodDef PySyck_methods[] = {
    {NULL}  /* Sentinel */
};

PyDoc_STRVAR(PySyck_doc,
    "low-level wrapper for the Syck YAML parser and emitter");

PyMODINIT_FUNC
init_syck(void)
{
    PyObject *m;

    if (PyType_Ready(&PySyckNode_Type) < 0)
        return;
    if (PyType_Ready(&PySyckScalar_Type) < 0)
        return;
    if (PyType_Ready(&PySyckSeq_Type) < 0)
        return;
    if (PyType_Ready(&PySyckMap_Type) < 0)
        return;
    if (PyType_Ready(&PySyckParser_Type) < 0)
        return;
    
    PySyck_Error = PyErr_NewException("_syck.error", NULL, NULL);
    if (!PySyck_Error) return;

    PySyck_ScalarKind = PyString_FromString("scalar");
    if (!PySyck_ScalarKind) return;
    PySyck_SeqKind = PyString_FromString("seq");
    if (!PySyck_SeqKind) return;
    PySyck_MapKind = PyString_FromString("map");
    if (!PySyck_MapKind) return;

    PySyck_1QuoteStyle = PyString_FromString("1quote");
    if (!PySyck_1QuoteStyle) return;
    PySyck_2QuoteStyle = PyString_FromString("2quote");
    if (!PySyck_2QuoteStyle) return;
    PySyck_FoldStyle = PyString_FromString("fold");
    if (!PySyck_FoldStyle) return;
    PySyck_LiteralStyle = PyString_FromString("literal");
    if (!PySyck_LiteralStyle) return;
    PySyck_PlainStyle = PyString_FromString("plain");
    if (!PySyck_PlainStyle) return;

    PySyck_StripChomp = PyString_FromString("-");
    if (!PySyck_StripChomp) return;
    PySyck_KeepChomp = PyString_FromString("+");
    if (!PySyck_KeepChomp) return;

    m = Py_InitModule3("_syck", PySyck_methods, PySyck_doc);

    Py_INCREF(PySyck_Error);
    if (PyModule_AddObject(m, "error", (PyObject *)PySyck_Error) < 0)
        return;

    Py_INCREF(&PySyckNode_Type);
    if (PyModule_AddObject(m, "Node", (PyObject *)&PySyckNode_Type) < 0)
        return;

    Py_INCREF(&PySyckScalar_Type);
    if (PyModule_AddObject(m, "Scalar", (PyObject *)&PySyckScalar_Type) < 0)
        return;

    Py_INCREF(&PySyckSeq_Type);
    if (PyModule_AddObject(m, "Seq", (PyObject *)&PySyckSeq_Type) < 0)
        return;

    Py_INCREF(&PySyckMap_Type);
    if (PyModule_AddObject(m, "Map", (PyObject *)&PySyckMap_Type) < 0)
        return;

    Py_INCREF(&PySyckParser_Type);
    if (PyModule_AddObject(m, "Parser", (PyObject *)&PySyckParser_Type) < 0)
        return;
}

