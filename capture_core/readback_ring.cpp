#include "readback_ring.h"

namespace xiii {

int ReadbackRing::CopySlot() const     { return copy_; }
int ReadbackRing::LockableSlot() const { return lockable_; }

void ReadbackRing::CommitCopy() {
    lockable_ = copy_;
    copy_ ^= 1;
}

void ReadbackRing::Reset() {
    copy_ = 0;
    lockable_ = -1;
}

}  // namespace xiii
