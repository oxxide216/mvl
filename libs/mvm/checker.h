#ifndef CHECKER_H
#define CHECKER_H

#include "mvm.h"

void program_check(Program *program, TargetPlatform target);
void program_type_check(Program *program);

#endif // CHECKER_H
