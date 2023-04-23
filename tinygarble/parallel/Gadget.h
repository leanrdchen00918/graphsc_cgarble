#ifndef GADGET
#define GADGET
#include <vector>
#include "Machine.h"
#include "GraphNode.h"
using namespace std;
class Gadget{
    public:
    Machine *machine;
    TinyGarblePI_SH* TGPI_SH = nullptr;
    Gadget(Machine *machine, TinyGarblePI_SH* TGPI_SH): machine(machine), TGPI_SH(TGPI_SH){}
    virtual void secureCompute() = 0;
    virtual void secureCompute(vector<GraphNode*>& nodes) = 0;
};
#endif
