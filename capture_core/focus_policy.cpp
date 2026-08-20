#include "focus_policy.h"

namespace xiii {

void* ForegroundToReport(bool fgBelongsToUs, void* fg, void* gameWnd) {
    if (fgBelongsToUs || !gameWnd)
        return fg;
    return gameWnd;
}

}  // namespace xiii
