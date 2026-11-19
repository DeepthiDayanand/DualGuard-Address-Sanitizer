#ifndef OUT_H
#define OUT_H

#include <iostream>
#include <string>

#include "util.h"
#include "LightHardening.h"
#include "AbstractDomain.h"

std::ostream &operator<<(std::ostream &os, const RegConstraintMap &obj);
std::ostream &operator<<(std::ostream &os, const BlockConstraintMap &obj);
std::ostream &operator<<(std::ostream &os, const std::map<Register *, LightHardeningPass::RegMessage> &obj);
std::ostream &operator<<(std::ostream &os, const llvm::Value *obj);
std::ostream &operator<<(std::ostream &os, const LeakPoint &obj);
std::ostream &operator<<(std::ostream &os, const std::set<LeakPoint> &obj);
std::ostream &operator<<(std::ostream &os, const MemoryRegion &obj);
std::ostream &operator<<(std::ostream &os, const MemStateMap &obj);

#endif