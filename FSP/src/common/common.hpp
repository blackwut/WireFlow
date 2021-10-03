#pragma once

#define COUT_STRING_W   24
#define COUT_DOUBLE_W   8
#define COUT_INTEGER_W  8
#define COUT_PRECISION  3

#define COUT_HEADER     std::setw(COUT_STRING_W) << std::right
#define COUT_FLOAT      std::setw(COUT_DOUBLE_W) << std::right << std::fixed << std::setprecision(COUT_PRECISION)
#define COUT_INTEGER    std::setw(COUT_INTEGER_W) << std::right << std::fixed
