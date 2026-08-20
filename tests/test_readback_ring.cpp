#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "readback_ring.h"

using namespace xiii;

TEST_CASE("fresh ring copies into slot 0 and has nothing lockable") {
    ReadbackRing r;
    CHECK(r.CopySlot() == 0);
    CHECK(r.LockableSlot() == -1);
}

TEST_CASE("after the first committed copy that slot becomes lockable") {
    ReadbackRing r;
    r.CommitCopy();
    CHECK(r.CopySlot() == 1);
    CHECK(r.LockableSlot() == 0);
}

TEST_CASE("slots alternate: second commit locks slot 1, copies into 0") {
    ReadbackRing r;
    r.CommitCopy();
    r.CommitCopy();
    CHECK(r.CopySlot() == 0);
    CHECK(r.LockableSlot() == 1);
}

TEST_CASE("copy and lockable slots never coincide across many commits") {
    ReadbackRing r;
    for (int i = 0; i < 100; ++i) {
        r.CommitCopy();
        CHECK(r.CopySlot() != r.LockableSlot());
        CHECK((r.CopySlot() == 0 || r.CopySlot() == 1));
        CHECK((r.LockableSlot() == 0 || r.LockableSlot() == 1));
    }
}

TEST_CASE("a failed (uncommitted) copy changes nothing") {
    ReadbackRing r;
    r.CommitCopy();  // slot 0 now lockable
    // Caller issues a copy into slot 1 but it fails, so no CommitCopy():
    // repeated queries must keep returning the same schedule.
    CHECK(r.CopySlot() == 1);
    CHECK(r.LockableSlot() == 0);
    CHECK(r.CopySlot() == 1);
    CHECK(r.LockableSlot() == 0);
}

TEST_CASE("reset forgets committed data") {
    ReadbackRing r;
    r.CommitCopy();
    r.CommitCopy();
    r.Reset();
    CHECK(r.CopySlot() == 0);
    CHECK(r.LockableSlot() == -1);
}
