// What the TV is told about a UAC prompt.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-11-09): the TV takes every
// controller while a prompt is up, and offers its keyboard only on a prompt
// that can ask for a password or PIN, so the kind has to follow Windows' own
// settings, and nothing may be said while Windows is locked.
//
// WHAT THESE CANNOT DO. No prompt opens here: the rule is checked with the
// settings Windows would give. That consent.exe is up for exactly as long as a
// prompt is, and that this listener sees it, was measured with a real prompt
// (uac_prompt_rule.inl).

#include "harness.h"

#include <fstream>
#include <sstream>
#include <string>

#include "app/uac_prompt_rule.inl"

using namespace ctmtest;

namespace {

std::string read_source(const char *relative)
{
    for (const char *prefix : {"", "../", "../../", "../../../", "../../../../"}) {
        std::ifstream in(std::string(prefix) + relative, std::ios::binary);
        if (in) {
            std::ostringstream text;
            text << in.rdbuf();
            return text.str();
        }
    }
    return std::string();
}

}  // namespace

int run_uac_prompt_rule_tests()
{
    using namespace uac_prompt_rule;

    section("UAC: the kind of a prompt, from Windows' settings");
    {
        Policy policy;   // an administrator, every setting at its default
        CTM_CHECK_EQ(kind_of_prompt(policy), kYesNo);
        // Yes or No: consent on the secure desktop, consent, and consent for others' programs
        for (int consent : {2, 4, 5}) {
            policy.consentPromptBehaviorAdmin = consent;
            CTM_CHECK_EQ(kind_of_prompt(policy), kYesNo);
        }
        // a password or PIN: prompt for credentials, on the secure desktop or not
        for (int credentials : {1, 3}) {
            policy.consentPromptBehaviorAdmin = credentials;
            CTM_CHECK_EQ(kind_of_prompt(policy), kTyping);
        }
        // Administrator protection asks for Windows Hello
        policy = Policy{};
        policy.typeOfAdminApprovalMode = 2;
        CTM_CHECK_EQ(kind_of_prompt(policy), kTyping);
        // an account that is not an administrator is asked for one's password or PIN
        policy = Policy{};
        policy.administrator = false;
        CTM_CHECK_EQ(kind_of_prompt(policy), kTyping);
    }

    section("UAC: what the TV is told");
    {
        const Policy admin;
        CTM_CHECK_EQ(status_for(false, false, admin), kNone);   // no prompt
        CTM_CHECK_EQ(status_for(true, false, admin), kYesNo);
        CTM_CHECK_EQ(status_for(true, true, admin), kNone);     // locked: the lock screen's rule
        Policy standard;
        standard.administrator = false;
        CTM_CHECK_EQ(status_for(true, false, standard), kTyping);
        // the words STATUS may carry, and nothing else
        CTM_CHECK_EQ(status_word_value(kNone), 0);
        CTM_CHECK_EQ(status_word_value(kYesNo), 1);
        CTM_CHECK_EQ(status_word_value(kTyping), 2);
        CTM_CHECK_EQ(status_word_value(3), 0);
        CTM_CHECK_EQ(status_word_value(-1), 0);
    }

    section("UAC: where the prompt is looked for");
    {
        const std::string watch = read_source("src/app/uac_watch.inl");
        CTM_CHECK(!watch.empty());
        // the program Windows runs for a prompt, matched in this listener's own session
        CTM_CHECK(watch.find("L\"consent.exe\"") != std::string::npos);
        CTM_CHECK(watch.find("WTSEnumerateProcessesW(") != std::string::npos);
        CTM_CHECK(watch.find("info[i].SessionId == session") != std::string::npos);
        // the settings the kind comes from
        CTM_CHECK(watch.find("L\"ConsentPromptBehaviorAdmin\", 5") != std::string::npos);
        CTM_CHECK(watch.find("L\"TypeOfAdminApprovalMode\", 1") != std::string::npos);
        const std::string agent = read_source("src/app/agent.inl");
        CTM_CHECK(!agent.empty());
        // watched from the start, beside the lock
        CTM_CHECK(agent.find("    watch_windows_lock();\n    watch_uac_prompts();") != std::string::npos ||
                  agent.find("    watch_windows_lock();\r\n    watch_uac_prompts();") != std::string::npos);
        // and not looked for while Windows is locked
        CTM_CHECK(agent.find("const bool up = !locked && uac_watch::prompt_up(session);") != std::string::npos);
    }

    return 0;
}
