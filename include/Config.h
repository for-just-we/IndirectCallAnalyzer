//
// Created by prophe cheng on 2024/1/3.
//

#ifndef TYPEDIVE_CONFIG_H
#define TYPEDIVE_CONFIG_H

#include "llvm/Support/FileSystem.h"
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <fstream>
#include <map>
#include "Common.h"

using namespace std;
using namespace llvm;

extern bool ENABLE_MLTA;
extern bool debug_mode;
// #define SOUND_MODE 1
#define MAX_TYPE_LAYER 10

#define MAP_CALLER_TO_CALLEE 1
#define UNROLL_LOOP_ONCE 1
#define PRINT_ICALL_TARGET

#endif //TYPEDIVE_CONFIG_H
