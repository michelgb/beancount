/* A Python extension module that invokes the parser defined in lexer.l and
 * grammar.y. */

#include <Python.h>
#include <moduleobject.h>
#include <ctype.h>

#include "parser.h"
#include "lexer.h"
#include "parser.h"


/* The bison header file does not contain this... silly. */
extern int yyparse(void);

extern const char* getTokenName(int token);

extern int yy_firstline;


/* The current builder during parsing (as a global variable for now). */
PyObject* builder = 0;


/* Check if the object is null; if so, report an error. This is used internally
   only, to debug the parser rules, and should never trigger in production. */
PyObject* checkNull(PyObject* o)
{
    if ( o == NULL ) {
        PyErr_Print();
        abort();
    }
    return o;
}



PyDoc_STRVAR(parse_doc,
"Parse the filename, calling back methods on the builder.\
Your builder is responsible to accumulating results.");

PyObject* parse(PyObject *self, PyObject *args, PyObject* kwds)
{
    FILE* fp = NULL;
    int result;

    /* Unpack and validate arguments */
    const char* filename = 0;
    const char* report_filename = 0;
    int report_firstline = 0;
    extern int yydebug;
    /* FIXME: You could support flex debugging too: yyset_debug(int bdebug) */
    static char *kwlist[] = {"filename", "builder",
                             "report_filename", "report_firstline",
                             "yydebug", NULL};
    if ( !PyArg_ParseTupleAndKeywords(args, kwds, "sO|sip", kwlist,
                                      &filename, &builder,
                                      &report_filename, &report_firstline,
                                      &yydebug) ) {
        return NULL;
    }
    Py_XINCREF(builder);

    /* Open the file. */
    fp = fopen(filename, "r");
    if ( fp == NULL ) {
        return PyErr_Format(PyExc_IOError, "Cannot open file '%s'.", filename);
    }

    /* Initialize the parser. */
    yyin = fp;
    if ( report_filename != 0 ) {
        yy_filename = report_filename;
    }
    else {
        yy_filename = filename;
    }
    yy_firstline = report_firstline;

    /* Parse! This will call back methods on the builder instance. */
    result = yyparse();

    /* Finalize the parser. */
    if ( fp != NULL ) {
        fclose(fp);
    }
    yylex_destroy();
    Py_XDECREF(builder);
    builder = 0;
    yy_filename = 0;

    /* Check for parsing errors. */
    if ( result != 0 ) {
        return PyErr_Format(PyExc_RuntimeError, "Parsing error.");
    }

    Py_RETURN_NONE;
}

/* FIXME: Make a version of this which takes a string as input! For
 * unit-testing... */


PyObject* get_yyfilename(PyObject *self, PyObject *args)
{
  return PyUnicode_FromString(yy_filename);
}

PyObject* get_yylineno(PyObject *self, PyObject *args)
{
  return PyLong_FromLong(yylineno);
}



/* Inititalize the lexer to start running in debug mode. */
PyObject* lexer_init(PyObject *self, PyObject *args)
{
    FILE* fp = NULL;

    /* Unpack and validate arguments */
    const char* filename = 0;
    if ( !PyArg_ParseTuple(args, "sO", &filename, &builder) ) {
        return NULL;
    }
    Py_XINCREF(builder);

    /* Open the file. */
    fp = fopen(filename, "r");
    if ( fp == NULL ) {
        return PyErr_Format(PyExc_IOError, "Cannot open file '%s'.", filename);
    }

    /* Initialize the parser. */
    yyin = fp;
    yy_filename = filename;

    Py_RETURN_NONE;
}

/* Get the next token; return None if complete. */
PyObject* lexer_next(PyObject *self, PyObject *args)
{
    const char* tokenName = NULL;
    YYSTYPE yylval;
    YYLTYPE yylloc;
    int token;
    PyObject* obj;

    /* Run the lexer. */
    token = yylex(&yylval, &yylloc);
    if ( token == 0 ) {
        yylex_destroy();
        Py_RETURN_NONE;
    }

    obj = Py_None;
    if (token == DATE ||
        token == ACCOUNT ||
        token == CURRENCY ||
        token == STRING ||
        token == NUMBER ||
        token == TAG ||
        token == LINK) {

        obj = yylval.pyobj;
    }

    tokenName = getTokenName(token);
    return Py_BuildValue("(sis#O)", tokenName, yylloc.first_line, yytext, yyleng, obj);
}


static PyMethodDef module_functions[] = {
    {"parse", (PyCFunction)parse, METH_VARARGS|METH_KEYWORDS, parse_doc},
    {"get_yyfilename", (PyCFunction)get_yyfilename, METH_VARARGS, NULL},
    {"get_yylineno", (PyCFunction)get_yylineno, METH_VARARGS, NULL},
    {"lexer_init", lexer_init, METH_VARARGS, NULL},
    {"lexer_next", lexer_next, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL} /* Sentinel */
};

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "_beancount",                         /* m_name */
    "Beancount parser extension module.", /* m_doc */
    -1,                                   /* m_size */
    module_functions,                     /* m_methods */
    NULL,                                 /* m_reload */
    NULL,                                 /* m_traverse */
    NULL,                                 /* m_clear */
    NULL,                                 /* m_free */
};

PyMODINIT_FUNC PyInit__parser(void)
{
    PyObject* m = PyModule_Create(&moduledef);
    if ( m == NULL )
        Py_RETURN_NONE;

    return m;
}
