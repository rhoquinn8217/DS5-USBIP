// Reading a device's USB descriptors: what the virtual device tells Windows
// it has, and how fast.
//
// ⭐ WHAT THESE PROTECT. parse_usb_info() turns a profile's descriptors into
// the interfaces and endpoints the virtual device is served by, and nothing
// tested it (code review, 2026-10-05). What it decides is load-bearing and
// silent when wrong:
// - which endpoints are isochronous (audio) and which interrupt (reports):
//   the USB/IP loop routes every request by these;
// - an interface listed once however many alternate settings it has;
// - a malformed descriptor ends the walk rather than reading past it;
// - the speed: one bulk endpoint smaller than 512 bytes is illegal at high
//   speed, and Windows refuses the whole configuration for it.
//
// WHAT THESE CANNOT DO. The descriptors here are written by hand in the shape
// of a DualSense's. They say the parser reads what it is given; whether a
// profile on disk describes the real device is a question for that profile.

#include "harness.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "ctm/profile.h"

using namespace ctmtest;

namespace usbdesc {

// ⓘ What descriptors.inl takes from common.inl, copied as it is there.
constexpr uint32_t kUsbSpeedHigh = 3;
constexpr uint32_t kUsbSpeedFull = 2;

static uint16_t read_le16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

#include "usb/descriptors.inl"

} // namespace usbdesc

namespace {

using Bytes = std::vector<unsigned char>;

void add(Bytes *out, std::initializer_list<unsigned char> bytes)
{
    out->insert(out->end(), bytes.begin(), bytes.end());
}

// A configuration in the shape of a DualSense's: audio control, the speaker's
// streaming interface (an empty alternate, then one with an isochronous OUT
// endpoint), the microphone's (an isochronous IN), and the HID interface with
// its interrupt IN and OUT.
Bytes dualsense_like_configuration(uint8_t numInterfaces)
{
    Bytes c;
    add(&c, {0x09, 0x02, 0x00, 0x00, numInterfaces, 0x01, 0x00, 0xC0, 0xFA});
    add(&c, {0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00});    // 0: audio control
    add(&c, {0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00});    // 1: speaker, alt 0
    add(&c, {0x09, 0x04, 0x01, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00});    // 1: speaker, alt 1
    add(&c, {0x07, 0x05, 0x01, 0x09, 0x88, 0x01, 0x04});                // iso OUT, 392 bytes
    add(&c, {0x09, 0x04, 0x02, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00});    // 2: microphone, alt 1
    add(&c, {0x07, 0x05, 0x82, 0x05, 0xC4, 0x00, 0x04});                // iso IN, 196 bytes
    add(&c, {0x09, 0x04, 0x03, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00});    // 3: HID
    add(&c, {0x07, 0x05, 0x84, 0x03, 0x40, 0x00, 0x06});                // interrupt IN, every 4 ms
    add(&c, {0x07, 0x05, 0x03, 0x03, 0x40, 0x00, 0x06});                // interrupt OUT
    const uint16_t total = static_cast<uint16_t>(c.size());
    c[2] = static_cast<unsigned char>(total & 0xFF);
    c[3] = static_cast<unsigned char>(total >> 8);
    return c;
}

Bytes dualsense_device_descriptor()
{
    return Bytes{0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40,
                 0x4C, 0x05, 0xE6, 0x0C, 0x00, 0x01, 0x01, 0x02, 0x00, 0x01};
}

} // namespace

int run_usb_descriptors_tests()
{
    section("usb descriptors: the device descriptor's ids are read little-endian");
    {
        CtmDescriptorProfile p;
        p.device_descriptor = dualsense_device_descriptor();
        p.configuration_descriptor = dualsense_like_configuration(4);
        const usbdesc::UsbDeviceInfo info = usbdesc::parse_usb_info(p);
        CTM_CHECK_EQ(static_cast<int>(info.vid), 0x054C);
        CTM_CHECK_EQ(static_cast<int>(info.pid), 0x0CE6);
        CTM_CHECK_EQ(static_cast<int>(info.bcdDevice), 0x0100);
        CTM_CHECK_EQ(static_cast<int>(info.numConfigurations), 1);
        CTM_CHECK_EQ(static_cast<int>(info.configurationValue), 1);
        CTM_CHECK_EQ(static_cast<int>(info.numInterfaces), 4);
    }

    section("usb descriptors: an interface is listed once, whatever alternates it has");
    {
        CtmDescriptorProfile p;
        p.configuration_descriptor = dualsense_like_configuration(4);
        const usbdesc::UsbDeviceInfo info = usbdesc::parse_usb_info(p);
        CTM_CHECK_EQ(info.interfaces.size(), static_cast<size_t>(4));
        CTM_CHECK(usbdesc::interface_is_class(info, 0, 0x01));     // audio
        CTM_CHECK(usbdesc::interface_is_class(info, 1, 0x01));
        CTM_CHECK(usbdesc::interface_is_class(info, 3, 0x03));     // HID
        CTM_CHECK(!usbdesc::interface_is_class(info, 3, 0x01));
        CTM_CHECK(usbdesc::find_interface(info, 9) == nullptr);
    }

    section("usb descriptors: the audio endpoints are isochronous and the report ones interrupt");
    {
        CtmDescriptorProfile p;
        p.configuration_descriptor = dualsense_like_configuration(4);
        const usbdesc::UsbDeviceInfo info = usbdesc::parse_usb_info(p);
        CTM_CHECK_EQ(info.endpoints.size(), static_cast<size_t>(4));
        CTM_CHECK(usbdesc::endpoint_is_iso(info, 0x01));
        CTM_CHECK(usbdesc::endpoint_is_iso(info, 0x82));
        CTM_CHECK(!usbdesc::endpoint_is_interrupt(info, 0x01));
        CTM_CHECK(usbdesc::endpoint_is_interrupt(info, 0x84));
        CTM_CHECK(usbdesc::endpoint_is_interrupt(info, 0x03));
        CTM_CHECK(!usbdesc::endpoint_is_iso(info, 0x84));
        // ⓘ An endpoint nobody declared is neither, rather than a guess.
        CTM_CHECK(!usbdesc::endpoint_is_iso(info, 0x85));
        CTM_CHECK(!usbdesc::endpoint_is_interrupt(info, 0x85));
        const usbdesc::EndpointInfo *in = usbdesc::find_endpoint(info, 0x84);
        CTM_CHECK(in != nullptr);
        if (in != nullptr) CTM_CHECK_EQ(static_cast<int>(in->maxPacket), 64);
        const usbdesc::EndpointInfo *spk = usbdesc::find_endpoint(info, 0x01);
        if (spk != nullptr) CTM_CHECK_EQ(static_cast<int>(spk->maxPacket), 392);
    }

    section("usb descriptors: a configuration that names no interface count is counted");
    {
        CtmDescriptorProfile p;
        p.configuration_descriptor = dualsense_like_configuration(0);
        CTM_CHECK_EQ(static_cast<int>(usbdesc::parse_usb_info(p).numInterfaces), 4);
    }

    section("usb descriptors: the interval is the high-speed one, kept between 125 us and 100 ms");
    {
        CtmDescriptorProfile p;
        p.configuration_descriptor = dualsense_like_configuration(4);
        usbdesc::UsbDeviceInfo info = usbdesc::parse_usb_info(p);
        CTM_CHECK_EQ(usbdesc::endpoint_interval_us(info, 0x84, 777), static_cast<uint32_t>(4000));   // 2^(6-1) x 125
        CTM_CHECK_EQ(usbdesc::endpoint_interval_us(info, 0x82, 777), static_cast<uint32_t>(1000));   // 2^(4-1) x 125
        CTM_CHECK_EQ(usbdesc::endpoint_interval_us(info, 0x99, 777), static_cast<uint32_t>(777));    // none: the fallback
        info.endpoints.push_back(usbdesc::EndpointInfo{0x86, 0x03, 8, 0});
        info.endpoints.push_back(usbdesc::EndpointInfo{0x87, 0x03, 8, 1});
        info.endpoints.push_back(usbdesc::EndpointInfo{0x88, 0x03, 8, 40});
        CTM_CHECK_EQ(usbdesc::endpoint_interval_us(info, 0x86, 777), static_cast<uint32_t>(777));    // 0: the fallback
        CTM_CHECK_EQ(usbdesc::endpoint_interval_us(info, 0x87, 777), static_cast<uint32_t>(125));
        CTM_CHECK_EQ(usbdesc::endpoint_interval_us(info, 0x88, 777), static_cast<uint32_t>(100000)); // capped
    }

    section("usb descriptors: a bulk endpoint under 512 bytes makes the device full speed");
    {
        CtmDescriptorProfile p;
        p.configuration_descriptor = dualsense_like_configuration(4);
        usbdesc::UsbDeviceInfo info = usbdesc::parse_usb_info(p);
        CTM_CHECK_EQ(usbdesc::usb_speed_for(info), usbdesc::kUsbSpeedHigh);   // interrupt and iso only
        info.endpoints.push_back(usbdesc::EndpointInfo{0x05, 0x02, 512, 0});
        CTM_CHECK_EQ(usbdesc::usb_speed_for(info), usbdesc::kUsbSpeedHigh);   // 512 is legal at high speed
        info.endpoints.push_back(usbdesc::EndpointInfo{0x86, 0x02, 64, 0});
        CTM_CHECK_EQ(usbdesc::usb_speed_for(info), usbdesc::kUsbSpeedFull);
    }

    section("usb descriptors: a malformed descriptor ends the walk, and nothing past it is read");
    {
        // A zero-length descriptor in the middle: the walk stops there.
        CtmDescriptorProfile zero;
        zero.configuration_descriptor = dualsense_like_configuration(4);
        zero.configuration_descriptor[9] = 0x00;          // interface 0's length
        const usbdesc::UsbDeviceInfo z = usbdesc::parse_usb_info(zero);
        CTM_CHECK(z.interfaces.empty());
        CTM_CHECK(z.endpoints.empty());

        // A length that runs past the end: what came before is kept.
        CtmDescriptorProfile over;
        over.configuration_descriptor = dualsense_like_configuration(4);
        over.configuration_descriptor.push_back(0x09);
        over.configuration_descriptor.push_back(0x04);
        over.configuration_descriptor.push_back(0x07);     // an interface 7 cut short
        const usbdesc::UsbDeviceInfo o = usbdesc::parse_usb_info(over);
        CTM_CHECK_EQ(o.interfaces.size(), static_cast<size_t>(4));
        CTM_CHECK(usbdesc::find_interface(o, 7) == nullptr);

        // Short device and configuration descriptors leave the defaults.
        CtmDescriptorProfile shortOne;
        shortOne.device_descriptor = Bytes{0x12, 0x01, 0x00};
        shortOne.configuration_descriptor = Bytes{0x09, 0x02};
        const usbdesc::UsbDeviceInfo s = usbdesc::parse_usb_info(shortOne);
        CTM_CHECK_EQ(static_cast<int>(s.vid), 0x054C);
        CTM_CHECK_EQ(static_cast<int>(s.numInterfaces), 0);
        CTM_CHECK(s.endpoints.empty());
    }

    section("usb descriptors: full speed asked for by the profile is kept");
    {
        CtmDescriptorProfile p;
        p.full_speed = true;
        CTM_CHECK(usbdesc::parse_usb_info(p).full_speed);
    }

    return 0;
}
