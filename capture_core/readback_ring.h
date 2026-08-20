// capture_core/readback_ring.h
//
// Slot scheduler for the pipelined (double-buffered) GPU->CPU readback: each
// capture tick CopyRects into one system-memory surface while LockRect-ing the
// OTHER surface, whose copy was issued a tick earlier and has had a full
// capture interval to complete -- so the lock (and, if the driver defers the
// blit, the copy) no longer waits on in-flight GPU work. Costs one capture
// interval of extra latency on the VR overlay.
//
// Pure bookkeeping, no D3D/Windows dependencies; the caller owns the surfaces.

#pragma once

namespace xiii {

class ReadbackRing {
public:
    // Slot index (0 or 1) the caller should CopyRects into this tick.
    int CopySlot() const;
    // Slot index holding the newest COMMITTED copy (safe to lock/decode),
    // or -1 until the first commit (priming tick) and after Reset().
    int LockableSlot() const;
    // Record that the copy into CopySlot() was issued successfully; that slot
    // becomes the lockable one and the other becomes the next copy target.
    // On a failed copy simply don't call this -- the schedule is unchanged.
    void CommitCopy();
    // Device reset / surface recreation: all slot contents are gone.
    void Reset();

private:
    int copy_     = 0;
    int lockable_ = -1;
};

}  // namespace xiii
