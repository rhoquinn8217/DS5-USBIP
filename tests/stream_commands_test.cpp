// Vibepollo's stream commands: our one entry put in its list beside the
// person's own, and taken out again, with nothing else in the file touched.
//
// ⭐ WHAT THESE PROTECT. The file is Vibepollo's settings, the person's own,
// and their own commands sit in the same list: on rhoquinn8217's PC an
// "undo" that runs a script as every stream ends. A wrong edit stops
// Vibepollo starting, or quietly drops their command.

#include "harness.h"

#include <string>
#include <vector>

#include "app/stream_commands.inl"

using namespace ctmtest;
namespace sc = stream_commands;

namespace {

const char *const kExe = "C:\\Program Files\\DS5-USBIP\\DS5-USBIP.exe";

// Their file, as it was on 2026-10-12, with the other settings round it.
const std::string kTheirs =
    "sunshine_name = PC\n"
    "global_prep_cmd = [{\"do\":\"\",\"elevated\":false,\"undo\":\"C:\\\\vm_tools\\\\resume.ahk\"}]\n"
    "min_log_level = 2\n";

}  // namespace

int run_stream_commands_tests()
{
    section("stream commands: our entry, as Vibepollo is to read it");
    {
        CTM_CHECK(sc::our_entry(kExe) ==
                  "{\"do\":\"\\\"C:\\\\Program Files\\\\DS5-USBIP\\\\DS5-USBIP.exe\\\" start\","
                  "\"elevated\":false,"
                  "\"undo\":\"\\\"C:\\\\Program Files\\\\DS5-USBIP\\\\DS5-USBIP.exe\\\" stop\"}");
    }

    section("stream commands: added after the person's own, and nothing else changes");
    {
        std::string out;
        CTM_CHECK(sc::with_ours(kTheirs, kExe, &out) == sc::Result::Changed);
        const std::string want =
            "sunshine_name = PC\n"
            "global_prep_cmd = [{\"do\":\"\",\"elevated\":false,\"undo\":\"C:\\\\vm_tools\\\\resume.ahk\"}," +
            sc::our_entry(kExe) + "]\n"
            "min_log_level = 2\n";
        CTM_CHECK(out == want);
        std::string again;
        CTM_CHECK(sc::with_ours(out, kExe, &again) == sc::Result::Unchanged);
        CTM_CHECK(again == out);
    }

    section("stream commands: taken out again, their file as it was, byte for byte");
    {
        std::string added;
        std::string removed;
        sc::with_ours(kTheirs, kExe, &added);
        CTM_CHECK(sc::without_ours(added, &removed) == sc::Result::Changed);
        CTM_CHECK(removed == kTheirs);
        std::string again;
        CTM_CHECK(sc::without_ours(removed, &again) == sc::Result::Unchanged);
        CTM_CHECK(again == kTheirs);
    }

    section("stream commands: an entry of ours from another folder is replaced, not doubled");
    {
        const std::string old =
            "global_prep_cmd = [" + sc::our_entry("D:\\Old\\DS5-USBIP.exe") + ",{\"do\":\"x\",\"undo\":\"y\"}]\n";
        std::string out;
        CTM_CHECK(sc::with_ours(old, kExe, &out) == sc::Result::Changed);
        CTM_CHECK(out == "global_prep_cmd = [{\"do\":\"x\",\"undo\":\"y\"}," + sc::our_entry(kExe) + "]\n");
    }

    section("stream commands: no list yet gets one, in the file's own line endings");
    {
        std::string out;
        CTM_CHECK(sc::with_ours("a = 1\r\nb = 2", kExe, &out) == sc::Result::Changed);
        CTM_CHECK(out == "a = 1\r\nb = 2\r\nglobal_prep_cmd = [" + sc::our_entry(kExe) + "]\r\n");
        CTM_CHECK(sc::with_ours("global_prep_cmd = []\n", kExe, &out) == sc::Result::Changed);
        CTM_CHECK(out == "global_prep_cmd = [" + sc::our_entry(kExe) + "]\n");
        CTM_CHECK(sc::without_ours("a = 1\n", &out) == sc::Result::Unchanged);
    }

    section("stream commands: braces and brackets inside a command are only text");
    {
        const std::string tricky = "global_prep_cmd = [{\"do\":\"echo {a} [b] \\\"c\\\"\",\"undo\":\"\"}]\n";
        std::string added;
        std::string removed;
        CTM_CHECK(sc::with_ours(tricky, kExe, &added) == sc::Result::Changed);
        CTM_CHECK(sc::without_ours(added, &removed) == sc::Result::Changed);
        CTM_CHECK(removed == tricky);
    }

    section("stream commands: what is not understood is left alone");
    {
        std::string out;
        const std::string odd = "global_prep_cmd = not a list\n";
        CTM_CHECK(sc::with_ours(odd, kExe, &out) == sc::Result::NotUnderstood);
        CTM_CHECK(out == odd);
        CTM_CHECK(sc::without_ours(odd, &out) == sc::Result::NotUnderstood);
        CTM_CHECK(out == odd);
    }

    section("stream commands: comments, and keys that only begin with the name, are other lines");
    {
        const std::string conf =
            "# global_prep_cmd = [{\"do\":\"DS5-USBIP.exe\"}]\n"
            "global_prep_cmd_note = 1\n";
        std::string out;
        CTM_CHECK(sc::without_ours(conf, &out) == sc::Result::Unchanged);
        CTM_CHECK(sc::with_ours(conf, kExe, &out) == sc::Result::Changed);
        CTM_CHECK(out == conf + "global_prep_cmd = [" + sc::our_entry(kExe) + "]\n");
    }
    return 0;
}
