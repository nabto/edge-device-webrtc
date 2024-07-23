#pragma once

/**
 * Some toolchains do not come with a definition for std::round in <cmath> This
 * function is required in some of the dependencies. This header creates a fix
 * which can be employed in these cases without having to rely on patching of
 * the source code. This is a HACK to be able to use some odd toolchains.
 *
 * According to
 * https://github.com/fmtlib/fmt/issues/3342#issuecomment-1465172404 it is a
 * common issue.
 */

#include <math.h>
namespace std {
    using ::round;
}
