// The rumble dials on a DS4's motors (2026-11-04): master_rumble_gain,
// rumble_gain_heavy, rumble_gain_soft and rumble_floor, as a DualSense has them.
//
// ⓘ WHERE THE MOTORS ARE. Windows sends the virtual DS4 a USB output report 0x05
// whichever way the pad itself is connected: byte 1 claims the motors with 0x01,
// byte 4 is the weak motor (right, the soft one) and byte 5 the strong one (left,
// the heavy one) -- the TV core's ds4_report.inl has the layout from the Linux
// driver. The TV turns the report into the pad's own afterwards, and signs it
// over Bluetooth, so a byte changed here reaches a cabled DS4 and a Bluetooth
// one alike.
//
// The arithmetic is the DualSense's (ds5_output_overrides.inl): master times the
// motor's own gain, a straight multiply capped at 255, then the floor
// (rumble_floor.inl). ⓘ Pure, beside rumble_floor.inl, so a test can include
// both.

#include <cstddef>
#include <cstdint>

static const uint8_t kDs4OutReportId   = 0x05;
static const size_t  kDs4IdxOutFlags   = 1;
static const uint8_t kDs4ClaimMotors   = 0x01;
static const size_t  kDs4IdxMotorSoft  = 4;
static const size_t  kDs4IdxMotorHeavy = 5;
static const int     kDs4RumbleGainMax = 500;   // as a DualSense's

// A gain set or not (-1), clamped to the range the dials have.
static int ds4_rumble_gain_or_full(int gain)
{
    if (gain < 0) return 100;
    return gain > kDs4RumbleGainMax ? kDs4RumbleGainMax : gain;
}

static uint8_t ds4_scale_motor(uint8_t value, int gainPercent)
{
    const int scaled = (static_cast<int>(value) * gainPercent) / 100;
    return scaled > 255 ? 255 : static_cast<uint8_t>(scaled);
}

// Scales a DS4 output report's two motors in place. Each of the four is -1 when
// its config does not set it, and with none set the game's rumble stands.
// True when a byte changed.
static bool ds4_scale_motors(uint8_t *data, size_t length,
                             int master, int heavy, int soft, int floorPct)
{
    if (data == nullptr || length <= kDs4IdxMotorHeavy || data[0] != kDs4OutReportId) {
        return false;
    }
    // A report that does not claim the motors is not driving them.
    if ((data[kDs4IdxOutFlags] & kDs4ClaimMotors) == 0) {
        return false;
    }
    if (master < 0 && heavy < 0 && soft < 0 && floorPct < 0) {
        return false;
    }
    const int rumbleFloor = floorPct < 0 ? kRumbleFloorDefaultPercent
        : (floorPct > 100 ? 100 : floorPct);
    const int masterGain = ds4_rumble_gain_or_full(master);
    const int softEffective = (masterGain * ds4_rumble_gain_or_full(soft)) / 100;
    const int heavyEffective = (masterGain * ds4_rumble_gain_or_full(heavy)) / 100;
    const uint8_t softWas = data[kDs4IdxMotorSoft];
    const uint8_t heavyWas = data[kDs4IdxMotorHeavy];
    data[kDs4IdxMotorSoft] = rumble_apply_floor(
        ds4_scale_motor(softWas, softEffective), softWas, rumbleFloor, softEffective);
    data[kDs4IdxMotorHeavy] = rumble_apply_floor(
        ds4_scale_motor(heavyWas, heavyEffective), heavyWas, rumbleFloor, heavyEffective);
    return data[kDs4IdxMotorSoft] != softWas || data[kDs4IdxMotorHeavy] != heavyWas;
}
