#ifndef NODE_COMP
#define NODE_COMP
#include <emp-tool/emp-tool.h>
#include "tinygarble/program_interface_sh.h"
class GraphNode;
class NodeComparator{
    public:
    virtual void lt(block* dst, GraphNode* n1, GraphNode* n2, TinyGarblePI_SH* TGPI_SH) = 0;
};
#endif
