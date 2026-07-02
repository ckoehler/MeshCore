#include <gtest/gtest.h>
#include "Mesh.h"
#include "helpers/SimpleMeshTables.h"

using namespace mesh;

// ── minimal stubs ────────────────────────────────────────────────────────────

struct NullRadio : Radio {
    int recvRaw(uint8_t*, int) override { return 0; }
    uint32_t getEstAirtimeFor(int) override { return 0; }
    float packetScore(float, int) override { return 0; }
    bool startSendRaw(const uint8_t*, int) override { return false; }
    bool isSendComplete() override { return true; }
    void onSendFinished() override {}
    bool isInRecvMode() const override { return false; }
};

struct NullMillis : MillisecondClock {
    unsigned long getMillis() override { return 0; }
};

struct ZeroRNG : RNG {
    void random(uint8_t* dest, size_t sz) override { memset(dest, 0, sz); }
};

struct NullRTC : RTCClock {
    uint32_t getCurrentTime() override { return 0; }
    void setCurrentTime(uint32_t) override {}
};

struct NullPacketManager : PacketManager {
    Packet* allocNew() override { return nullptr; }
    void free(Packet*) override {}
    void queueOutbound(Packet*, uint8_t, uint32_t) override {}
    Packet* getNextOutbound(uint32_t) override { return nullptr; }
    int getOutboundCount(uint32_t) const override { return 0; }
    int getOutboundTotal() const override { return 0; }
    int getFreeCount() const override { return 0; }
    Packet* getOutboundByIdx(int) override { return nullptr; }
    Packet* removeOutboundByIdx(int) override { return nullptr; }
    void queueInbound(Packet*, uint32_t) override {}
    Packet* getNextInbound(uint32_t) override { return nullptr; }
};

// Forwarding repeater node: always allows forwarding, exposes onRecvPacket.
class TestMesh : public Mesh {
public:
    TestMesh(Radio& r, MillisecondClock& ms, RNG& rng, RTCClock& rtc,
             PacketManager& mgr, MeshTables& tables)
        : Mesh(r, ms, rng, rtc, mgr, tables) {}

    bool allowPacketForward(const Packet*) override { return true; }
    DispatcherAction recv(Packet* pkt) { return onRecvPacket(pkt); }
};

static bool isRetransmit(DispatcherAction a) { return (a >> 24) != 0; }

// ── bug reproduction ──────────────────────────────────────────────────────────
//
// A flood ACK arrives via two paths.  Copy 1 has a full path (too many hops
// to append self and re-flood); Copy 2 carries the same payload but a short
// path that would be forwardable.
//
// Current behaviour (bug): markSeen() fires for Copy 1 before routeRecvPacket
// checks path length.  Copy 2 then hits wasSeen() == true and is released
// without ever reaching the forward check.
//
// Expected behaviour (after fix): Copy 1 is released (path full); Copy 2 is
// retransmitted because markForwarded() / a separate seen-forward bit is only
// set when a copy is actually committed to forward.

TEST(OnRecvPacket, FloodAck_SecondCopyWithShortPath_ShouldBeForwarded) {
    NullRadio radio;
    NullMillis ms;
    ZeroRNG rng;
    NullRTC rtc;
    NullPacketManager mgr;
    SimpleMeshTables tables;
    TestMesh node(radio, ms, rng, rtc, mgr, tables);

    const uint32_t ack_crc = 0xDEADBEEF;

    // Copy 1: hash_size=2, path count=32 → path byte length=64=MAX_PATH_SIZE
    // routeRecvPacket condition: (32+1)*2 = 66 > 64 → cannot forward
    Packet copy1;
    copy1.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_ACK << PH_TYPE_SHIFT);
    copy1.setPathHashSizeAndCount(2, 32);
    memset(copy1.path, 0xAA, 64);
    memcpy(copy1.payload, &ack_crc, 4);
    copy1.payload_len = 4;

    DispatcherAction a1 = node.recv(&copy1);
    EXPECT_EQ(ACTION_RELEASE, a1) << "copy1: path full, should be released";

    // Copy 2: same payload (same hash), hash_size=1, path count=1
    // routeRecvPacket condition: (1+1)*1 = 2 ≤ 64 → should forward
    Packet copy2;
    copy2.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_ACK << PH_TYPE_SHIFT);
    copy2.setPathHashSizeAndCount(1, 1);
    copy2.path[0] = 0xBB;
    memcpy(copy2.payload, &ack_crc, 4);
    copy2.payload_len = 4;

    DispatcherAction a2 = node.recv(&copy2);
    // This FAILS with the current code: markSeen fired for copy1, so
    // wasSeen returns true for copy2 and it is released without forwarding.
    EXPECT_TRUE(isRetransmit(a2)) << "copy2 has a forwardable path but was dropped "
                                     "(markSeen poisoned the dedup table before "
                                     "routeRecvPacket checked path length)";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
