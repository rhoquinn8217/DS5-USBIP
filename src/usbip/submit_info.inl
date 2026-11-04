// What a submit tells the server about itself, and the audio figures the
// server logs.
//
// ⓘ Out of device.inl, unchanged, so the USB/IP server can be compiled
// against a test's own device (server.inl says why it is a template).

enum class InputReplyKind {
    None,
    Fresh,
    Stale,
    Zero
};

struct CtmSubmitInfo {
    uint8_t endpointAddress = 0;
    bool endpointIso = false;
    bool endpointInterrupt = false;
    InputReplyKind inputReply = InputReplyKind::None;
    uint32_t inputWaitUs = 0;
};

struct CtmAudioStats {
    uint64_t isoEvents = 0;
    uint64_t isoBytes = 0;
    uint64_t inputFrames = 0;
    uint64_t reservoirFrames = 0;
    uint64_t chunksBuilt = 0;
    uint64_t buildFails = 0;
    uint64_t sendFails = 0;
    uint64_t trailingBytes = 0;
    uint64_t reservoirDrops = 0;
    uint64_t reservoirWaits = 0;
    uint32_t reservoirFillMs = 0;
};
