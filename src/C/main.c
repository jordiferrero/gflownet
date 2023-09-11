#include "main.h"
#include "structmember.h"

static void Graph_dealloc(Graph *self) {
    Py_XDECREF(self->graph_def);
    if (self->num_edges > 0) {
        free(self->edges);
        free(self->edge_attrs);
    }
    if (self->num_nodes > 0) {
        free(self->nodes);
        free(self->node_attrs);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Graph_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    Graph *self;
    self = (Graph *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->graph_def = Py_None;
        Py_INCREF(Py_None);
        self->num_nodes = 0;
        self->num_edges = 0;
        self->nodes = self->edges = self->node_attrs = self->edge_attrs = NULL;
    }
    return (PyObject *)self;
}

static int Graph_init(Graph *self, PyObject *args, PyObject *kwds) {
    static char *kwlist[] = {"graph_def", NULL};
    PyObject *graph_def = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &graph_def))
        return -1;

    if (graph_def) {
        SELF_SET(graph_def);
    }
    return 0;
}

static PyMemberDef Graph_members[] = {
    {"graph_def", T_OBJECT_EX, offsetof(Graph, graph_def), 0, "node values"}, {NULL} /* Sentinel */
};

static PyObject *Graph_add_node(Graph *self, PyObject *args, PyObject *kwds) {
    PyObject *node = NULL;
    if (!PyArg_ParseTuple(args, "O", &node))
        return NULL;
    if (node) {
        if (!PyLong_Check(node)) {
            PyErr_SetString(PyExc_TypeError, "node must be an int");
            return NULL;
        }
        int node_id = PyLong_AsLong(node);
        for (int i = 0; i < self->num_nodes; i++) {
            if (self->nodes[i] == node_id) {
                PyErr_SetString(PyExc_KeyError, "node already exists");
                return NULL;
            }
        }
        int node_pos = self->num_nodes;
        self->num_nodes++;
        self->nodes = realloc(self->nodes, self->num_nodes * sizeof(int));
        self->nodes[node_pos] = node_id;
        self->degrees = realloc(self->degrees, self->num_nodes * sizeof(int));
        self->degrees[node_pos] = 0;
        Py_ssize_t num_attrs;
        if (kwds == NULL || (num_attrs = PyDict_Size(kwds)) == 0) {
            Py_RETURN_NONE;
        }
        // Now we need to add the node attributes
        // First check if the attributes are found in the GraphDef
        GraphDef *gt = (GraphDef *)self->graph_def;
        // for k in kwds:
        //   assert k in gt.node_values and kwds[k] in gt.node_values[k]
        PyObject *key, *value;
        Py_ssize_t pos = 0;
        int node_attr_pos = self->num_node_attrs;
        self->num_node_attrs += num_attrs;
        self->node_attrs = realloc(self->node_attrs, self->num_node_attrs * 3 * sizeof(int));

        while (PyDict_Next(kwds, &pos, &key, &value)) {
            PyObject *node_values = PyDict_GetItem(gt->node_values, key);
            if (node_values == NULL) {
                PyErr_SetString(PyExc_KeyError, "key not found in GraphDef");
                return NULL;
            }
            Py_ssize_t value_idx = PySequence_Index(node_values, value);
            if (value_idx == -1) {
                PyErr_SetString(PyExc_KeyError, "value not found in GraphDef");
                return NULL;
            }
            int attr_index = PyLong_AsLong(PyDict_GetItem(gt->node_keypos, key));
            self->node_attrs[node_attr_pos * 3] = node_pos;
            self->node_attrs[node_attr_pos * 3 + 1] = attr_index;
            self->node_attrs[node_attr_pos * 3 + 2] = value_idx;
            node_attr_pos++;
        }
    }
    Py_RETURN_NONE;
}

static PyObject *Graph_add_edge(Graph *self, PyObject *args, PyObject *kwds) {
    PyObject *u = NULL, *v = NULL;
    if (!PyArg_ParseTuple(args, "OO", &u, &v))
        return NULL;
    if (u && v) {
        if (!PyLong_Check(u) || !PyLong_Check(v)) {
            PyErr_SetString(PyExc_TypeError, "u, v must be ints");
            return NULL;
        }
        int u_id = -PyLong_AsLong(u);
        int v_id = -PyLong_AsLong(v);
        int u_pos = -1;
        int v_pos = -1;
        if (u_id < v_id) {
            int tmp = u_id;
            u_id = v_id;
            v_id = tmp;
        }
        for (int i = 0; i < self->num_nodes; i++) {
            if (self->nodes[i] == -u_id) {
                u_id = -u_id;
                u_pos = i;
            } else if (self->nodes[i] == -v_id) {
                v_id = -v_id;
                v_pos = i;
            }
            if (u_pos >= 0 && v_pos >= 0) {
                break;
            }
        }
        if (u_id < 0 || v_id < 0) {
            PyErr_SetString(PyExc_KeyError, "u, v must refer to existing nodes");
            return NULL;
        }
        for (int i = 0; i < self->num_edges; i++) {
            if (self->edges[i * 2] == u_pos && self->edges[i * 2 + 1] == v_pos) {
                PyErr_SetString(PyExc_KeyError, "edge already exists");
                return NULL;
            }
        }
        self->num_edges++;
        self->edges = realloc(self->edges, self->num_edges * sizeof(int) * 2);
        self->edges[self->num_edges * 2 - 2] = u_pos;
        self->edges[self->num_edges * 2 - 1] = v_pos;
        self->degrees[u_pos]++;
        self->degrees[v_pos]++;

        Py_ssize_t num_attrs;
        if (kwds == NULL || (num_attrs = PyDict_Size(kwds)) == 0) {
            Py_RETURN_NONE;
        }
        int edge_id = self->num_edges - 1;
        // First check if the attributes are found in the GraphDef
        GraphDef *gt = (GraphDef *)self->graph_def;
        PyObject *key, *value;
        Py_ssize_t pos = 0;
        int edge_attr_pos = self->num_edge_attrs;
        self->num_edge_attrs += num_attrs;
        self->edge_attrs = realloc(self->edge_attrs, self->num_edge_attrs * 3 * sizeof(int));

        while (PyDict_Next(kwds, &pos, &key, &value)) {
            PyObject *edge_values = PyDict_GetItem(gt->edge_values, key);
            if (edge_values == NULL) {
                PyErr_SetString(PyExc_KeyError, "key not found in GraphDef");
                return NULL;
            }
            Py_ssize_t value_idx = PySequence_Index(edge_values, value);
            if (value_idx == -1) {
                PyErr_SetString(PyExc_KeyError, "value not found in GraphDef");
                return NULL;
            }
            int attr_index = PyLong_AsLong(PyDict_GetItem(gt->edge_keypos, key));
            self->edge_attrs[edge_attr_pos * 3] = edge_id;
            self->edge_attrs[edge_attr_pos * 3 + 1] = attr_index;
            self->edge_attrs[edge_attr_pos * 3 + 2] = value_idx;
            edge_attr_pos++;
        }
    }
    Py_RETURN_NONE;
}

PyObject *Graph_isdirected(Graph *self, PyObject *Py_UNUSED(ignored)) { Py_RETURN_FALSE; }

void bridge_dfs(int v, int parent, int *visited, int *tin, int *low, int *timer, int **adj, int *degrees, int *result,
                Graph *g) {
    visited[v] = 1;
    tin[v] = low[v] = (*timer)++;
    for (int i = 0; i < degrees[v]; i++) {
        int to = adj[v][i];
        if (to == parent)
            continue;
        if (visited[to]) {
            low[v] = mini(low[v], tin[to]);
        } else {
            bridge_dfs(to, v, visited, tin, low, timer, adj, degrees, result, g);
            low[v] = mini(low[v], low[to]);
            if (low[to] > tin[v]) {
                result[get_edge_index_from_pos(g, v, to)] = 1;
            }
        }
    }
}
PyObject *Graph_bridges(PyObject *_self, PyObject *args) {
    Graph *self = (Graph *)_self;
    // from:
    // https://cp-algorithms.com/graph/bridge-searching.html
    int n = self->num_nodes; // number of nodes
    int **adj = malloc(n * sizeof(int *));
    for (int i = 0; i < n; i++) {
        adj[i] = malloc(self->degrees[i] * sizeof(int));
    }
    int is_bridge[self->num_edges];
    for (int i = 0; i < self->num_edges; i++) {
        is_bridge[i] = 0;
        *(adj[self->edges[i * 2]]) = self->edges[i * 2 + 1];
        *(adj[self->edges[i * 2 + 1]]) = self->edges[i * 2];
        // increase the pointer
        adj[self->edges[i * 2]]++;
        adj[self->edges[i * 2 + 1]]++;
    }
    // reset the pointers using the degrees
    for (int i = 0; i < n; i++) {
        adj[i] -= self->degrees[i];
    }
    int visited[n];
    int tin[n];
    int low[n];
    for (int i = 0; i < n; i++) {
        visited[i] = 0;
        tin[i] = -1;
        low[i] = -1;
    }

    int timer = 0;
    for (int i = 0; i < n; ++i) {
        if (!visited[i])
            bridge_dfs(i, -1, visited, tin, low, &timer, adj, self->degrees, is_bridge, self);
    }
    if (args == NULL) {
        // This function is being called from python, so we must return a list
        PyObject *result = PyList_New(0);
        for (int i = 0; i < self->num_edges; i++) {
            if (is_bridge[i]) {
                PyList_Append(result, PyTuple_Pack(2, PyLong_FromLong(self->nodes[self->edges[i * 2]]),
                                                   PyLong_FromLong(self->nodes[self->edges[i * 2 + 1]])));
            }
        }
        return result;
    }
    // This function is being called from C, so args is actually a int* to store the result
    int *result = (int *)args;
    for (int i = 0; i < self->num_edges; i++) {
        result[i] = is_bridge[i];
    }
    for (int i = 0; i < n; i++) {
        free(adj[i]);
    }
    free(adj);
    return 0; // success
}

static PyMethodDef Graph_methods[] = {
    {"add_node", (PyCFunction)Graph_add_node, METH_VARARGS | METH_KEYWORDS, "Add a node"},
    {"add_edge", (PyCFunction)Graph_add_edge, METH_VARARGS | METH_KEYWORDS, "Add an edge"},
    {"is_directed", (PyCFunction)Graph_isdirected, METH_NOARGS, "Is the graph directed?"},
    {"is_multigraph", (PyCFunction)Graph_isdirected, METH_NOARGS, "Is the graph a multigraph?"},
    {"bridges", (PyCFunction)Graph_bridges, METH_NOARGS, "Find the bridges of the graph"},
    {NULL} /* Sentinel */
};

static PyObject *Graph_getnodes(Graph *self, void *closure) {
    // Return a new NodeView
    PyObject *args = PyTuple_Pack(1, self);
    PyObject *obj = PyObject_CallObject((PyObject *)&NodeViewType, args);
    Py_DECREF(args);
    return obj;
}

static PyObject *Graph_getedges(Graph *self, void *closure) {
    // Return a new EdgeView
    PyObject *args = PyTuple_Pack(1, self);
    PyObject *obj = PyObject_CallObject((PyObject *)&EdgeViewType, args);
    Py_DECREF(args);
    return obj;
}

static PyGetSetDef Graph_getsetters[] = {
    {"nodes", (getter)Graph_getnodes, NULL, "nodes", NULL},
    {"edges", (getter)Graph_getedges, NULL, "edges", NULL},
    {NULL} /* Sentinel */
};

static PyObject *Graph_contains(Graph *self, PyObject *v) {
    if (!PyLong_Check(v)) {
        PyErr_SetString(PyExc_TypeError, "Graph.__contains__ only accepts integers");
        return NULL;
    }
    int index = PyLong_AsLong(v);
    for (int i = 0; i < self->num_nodes; i++) {
        if (self->nodes[i] == index) {
            Py_RETURN_TRUE;
        }
    }
    Py_RETURN_FALSE;
}

static Py_ssize_t Graph_len(Graph *self) { return self->num_nodes; }

static PySequenceMethods Graph_seqmeth = {
    .sq_contains = (objobjproc)Graph_contains,
    .sq_length = (lenfunc)Graph_len,
};

static PyObject *Graph_iter(PyObject *self) {
    PyObject *args = PyTuple_Pack(1, self);
    PyObject *obj = PyObject_CallObject((PyObject *)&NodeViewType, args);
    Py_DECREF(args);
    printf("iter created %p -> %p\n", self, obj);
    return obj;
}

static PyObject *Graph_getitem(PyObject *self, PyObject *key) {
    PyObject *args = PyTuple_Pack(2, self, key);
    PyObject *obj = PyObject_CallObject((PyObject *)&NodeViewType, args);
    Py_DECREF(args);
    return obj;
}
static PyMappingMethods Graph_mapmeth = {
    .mp_subscript = Graph_getitem,
};

PyTypeObject GraphType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "_C.Graph",
    .tp_doc = PyDoc_STR("Constrained Graph object"),
    .tp_basicsize = sizeof(Graph),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Graph_new,
    .tp_init = (initproc)Graph_init,
    .tp_dealloc = (destructor)Graph_dealloc,
    .tp_members = Graph_members,
    .tp_methods = Graph_methods,
    .tp_iter = Graph_iter,
    .tp_getset = Graph_getsetters,
    .tp_as_sequence = &Graph_seqmeth,
    .tp_as_mapping = &Graph_mapmeth,
};

PyObject *Graph_getnodeattr(Graph *self, int index, PyObject *k) {
    GraphDef *gt = (GraphDef *)self->graph_def;
    PyObject *value_list = PyDict_GetItem(gt->node_values, k);
    if (value_list == NULL) {
        PyErr_SetString(PyExc_KeyError, "key not found");
        return NULL;
    }
    long attr_index = PyLong_AsLong(PyDict_GetItem(gt->node_keypos, k));
    int true_node_index = -1;
    for (int i = 0; i < self->num_nodes; i++) {
        if (self->nodes[i] == index) {
            true_node_index = i;
            break;
        }
    }
    if (true_node_index == -1) {
        PyErr_SetString(PyExc_KeyError, "node not found");
        return NULL;
    }

    for (int i = 0; i < self->num_node_attrs; i++) {
        if (self->node_attrs[i * 3] == true_node_index && self->node_attrs[i * 3 + 1] == attr_index) {
            return PyList_GetItem(value_list, self->node_attrs[i * 3 + 2]);
        }
    }
    PyErr_SetString(PyExc_KeyError, "attribute not set for this node");
    return NULL;
}

PyObject *Graph_setnodeattr(Graph *self, int index, PyObject *k, PyObject *v) {
    GraphDef *gt = (GraphDef *)self->graph_def;
    int true_node_index = -1;
    for (int i = 0; i < self->num_nodes; i++) {
        if (self->nodes[i] == index) {
            true_node_index = i;
            break;
        }
    }
    if (true_node_index == -1) {
        PyErr_SetString(PyExc_KeyError, "node not found");
        return NULL;
    }
    PyObject *node_values = PyDict_GetItem(gt->node_values, k);
    if (node_values == NULL) {
        PyErr_SetString(PyExc_KeyError, "key not found in GraphDef");
        return NULL;
    }
    Py_ssize_t value_idx = PySequence_Index(node_values, v);
    if (value_idx == -1) {
        PyErr_SetString(PyExc_KeyError, "value not found in GraphDef");
        return NULL;
    }
    int attr_index = PyLong_AsLong(PyDict_GetItem(gt->node_keypos, k));
    for (int i = 0; i < self->num_node_attrs; i++) {
        if (self->node_attrs[i * 3] == true_node_index && self->node_attrs[i * 3 + 1] == attr_index) {
            self->node_attrs[i * 3 + 2] = value_idx;
            Py_RETURN_NONE;
        }
    }
    // Could not find the attribute, add it
    int new_idx = self->num_node_attrs;
    self->num_node_attrs++;
    self->node_attrs = realloc(self->node_attrs, self->num_node_attrs * 3 * sizeof(int));
    self->node_attrs[new_idx * 3] = true_node_index;
    self->node_attrs[new_idx * 3 + 1] = attr_index;
    self->node_attrs[new_idx * 3 + 2] = value_idx;
    Py_RETURN_NONE;
}

PyObject *Graph_getedgeattr(Graph *self, int index, PyObject *k) {
    GraphDef *gt = (GraphDef *)self->graph_def;
    PyObject *value_list = PyDict_GetItem(gt->edge_values, k);
    if (value_list == NULL) {
        PyErr_SetString(PyExc_KeyError, "key not found");
        return NULL;
    }
    long attr_index = PyLong_AsLong(PyDict_GetItem(gt->edge_keypos, k));
    if (index > self->num_edges) {
        // Should never happen, index is computed by us in EdgeView_getitem, not
        // by the user!
        PyErr_SetString(PyExc_KeyError, "edge not found [but this should never happen!]");
        return NULL;
    }

    for (int i = 0; i < self->num_edge_attrs; i++) {
        if (self->edge_attrs[i * 3] == index && self->edge_attrs[i * 3 + 1] == attr_index) {
            return PyList_GetItem(value_list, self->edge_attrs[i * 3 + 2]);
        }
    }
    PyErr_SetString(PyExc_KeyError, "attribute not set for this node");
    return NULL;
}
PyObject *Graph_setedgeattr(Graph *self, int index, PyObject *k, PyObject *v) {
    GraphDef *gt = (GraphDef *)self->graph_def;
    PyObject *edge_values = PyDict_GetItem(gt->edge_values, k);
    if (edge_values == NULL) {
        PyErr_SetString(PyExc_KeyError, "key not found in GraphDef");
        return NULL;
    }
    Py_ssize_t value_idx = PySequence_Index(edge_values, v);
    if (value_idx == -1) {
        PyErr_SetString(PyExc_KeyError, "value not found in GraphDef");
        return NULL;
    }
    int attr_index = PyLong_AsLong(PyDict_GetItem(gt->edge_keypos, k));
    for (int i = 0; i < self->num_edge_attrs; i++) {
        if (self->edge_attrs[i * 3] == index && self->edge_attrs[i * 3 + 1] == attr_index) {
            self->edge_attrs[i * 3 + 2] = value_idx;
            Py_RETURN_NONE;
        }
    }
    // Could not find the attribute, add it
    int new_idx = self->num_edge_attrs;
    self->num_edge_attrs++;
    self->edge_attrs = realloc(self->edge_attrs, self->num_edge_attrs * 3 * sizeof(int));
    self->edge_attrs[new_idx * 3] = index;
    self->edge_attrs[new_idx * 3 + 1] = attr_index;
    self->edge_attrs[new_idx * 3 + 2] = value_idx;
    Py_RETURN_NONE;
}

static PyObject *spam_system(PyObject *self, PyObject *args) {
    const char *command;
    int sts;

    if (!PyArg_ParseTuple(args, "s", &command))
        return NULL;
    sts = system(command);
    if (sts < 0) {
        PyErr_SetString(SpamError, "System command failed");
        return NULL;
    }
    return PyLong_FromLong(sts);
}

static PyMethodDef SpamMethods[] = {
    {"system", spam_system, METH_VARARGS, "Execute a shell command."},
    {"mol_graph_to_Data", mol_graph_to_Data, METH_VARARGS, "Convert a mol_graph to a Data object."},
    {NULL, NULL, 0, NULL} /* Sentinel */
};

static struct PyModuleDef spammodule = {PyModuleDef_HEAD_INIT, "_C", /* name of module */
                                        "doc",                       /* module documentation, may be NULL */
                                        -1,                          /* size of per-interpreter state of the module,
                                                                        or -1 if the module keeps state in global variables. */
                                        SpamMethods};

PyMODINIT_FUNC PyInit__C(void) {
    PyObject *m;

    m = PyModule_Create(&spammodule);
    if (m == NULL)
        return NULL;

    SpamError = PyErr_NewException("_C.error", NULL, NULL);
    Py_XINCREF(SpamError);
    if (PyModule_AddObject(m, "error", SpamError) < 0) {
        Py_XDECREF(SpamError);
        Py_CLEAR(SpamError);
        Py_DECREF(m);
        return NULL;
    }
    PyTypeObject *types[] = {&GraphType, &GraphDefType, &NodeViewType, &EdgeViewType};
    char *names[] = {"Graph", "GraphDef", "NodeView", "EdgeView"};
    for (int i = 0; i < (int)(sizeof(types) / sizeof(PyTypeObject *)); i++) {
        if (PyType_Ready(types[i]) < 0) {
            Py_DECREF(m);
            return NULL;
        }
        Py_XINCREF(types[i]);
        if (PyModule_AddObject(m, names[i], (PyObject *)types[i]) < 0) {
            Py_XDECREF(types[i]);
            Py_DECREF(m);
            return NULL;
        }
    }

    return m;
}