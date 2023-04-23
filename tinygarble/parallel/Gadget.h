#ifndef GADGET
#define GADGET
#include "Machine.h"
class Gadget{
    public:
    Machine *machine;
    TinyGarblePI_SH* TGPI_SH = nullptr;
    Gadget(Machine *machine, TinyGarblePI_SH* TGPI_SH): machine(machine), TGPI_SH(TGPI_SH){}
    virtual void secureCompute() = 0;
};
#endif
