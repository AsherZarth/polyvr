#ifndef VRPYGRAPH_H_INCLUDED
#define VRPYGRAPH_H_INCLUDED

#include "VRPyBase.h"
#include "core/math/graph.h"

struct VRPyGraph : VRPyBaseT<OSG::Graph> {
    static PyMethodDef methods[];
    static PyObject* getEdges(VRPyGraph* self);
    static PyObject* getInEdges(VRPyGraph* self, PyObject* args);
    static PyObject* getOutEdges(VRPyGraph* self, PyObject* args);
};

#endif // VRPYGRAPH_H_INCLUDED
