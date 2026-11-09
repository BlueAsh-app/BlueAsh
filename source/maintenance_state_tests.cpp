#include "maintenance_state.h"
#include <cassert>
int main(){MaintenanceState s;assert(!s.accept(0,true));assert(s.accept(1,true)==1);assert(!s.accept(1,true));assert(!s.accept(0,false));assert(!s.accept(2,true));assert(s.awaitingEnd);assert(s.accept(0,true)==2);assert(!s.accept(0,true));assert(!s.accept(-1,true));}
