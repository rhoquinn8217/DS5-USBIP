// Which TVs may bridge from outside this PC's network: the pairing proof.
//
// ⭐ WHAT THESE PROTECT. A TV calling from the internet is let in only with a
// proof it is paired with this PC's streaming host (code review, 2026-10-05):
// a signature, by the key it paired with, over a challenge this listener gave
// it and the command it is sending. Each way that could let a stranger in, or
// shut a paired TV out, has a section: the signature checked against the
// right device and nothing else, a challenge used once and only by the caller
// it was given to, the host's list of devices read from its own state file,
// and who needs a proof at all.
//
// ⭐ THE VECTORS ARE INDEPENDENT. The two certificates below were made with
// openssl for these tests (their private keys were never kept), and both
// signatures were made by openssl over signed_text(), then checked by it:
// so a pass here means Windows' crypto agrees with another implementation's,
// not just with itself. Certificate A signed the good signature; B, a
// stranger's, the bad one.
//
// WHAT THESE CANNOT DO. They do not show that the TV signs what this checks:
// that is the TV's half, run against a listener on a set.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

#include "harness.h"

#include <chrono>
#include <string>
#include <vector>

#include "app/paired_tv.inl"

using namespace ctmtest;

namespace {

const char *const kCertA = R"(-----BEGIN CERTIFICATE-----
MIIDJzCCAg+gAwIBAgIUD2JMjhjgDpxtY0w5JeUVDBrrkX8wDQYJKoZIhvcNAQEL
BQAwIzEhMB8GA1UEAwwYTlZJRElBIEdhbWVTdHJlYW0gQ2xpZW50MB4XDTI2MTAw
ODA1MjkxNVoXDTQ2MTAwMzA1MjkxNVowIzEhMB8GA1UEAwwYTlZJRElBIEdhbWVT
dHJlYW0gQ2xpZW50MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxEhx
8t+9juUgO7BtgxDjL04RRZHqpL3C8XmfAYUfkLndZwBPLpHZPU8elm2GMSVAiyxg
pPuSNP8wiGfY80LqgpAVsRo93AOaIawchVIvUyHY/ekJ/uUGRaYqQ3wuPIHineFB
lnILWHV9HBjuA4AkHsy5Q5lhsQ8wsitPzlVz836xFP/hRgA8DcOd1BYXJtB4KhAx
iqluBoMdxgTDGB4y/Wa9RvjLHY34iCAHNahzkxtDXTH9zZCje7wqmJCxR9fixcbM
P2TRv5FbFZNO9+XLK2UgE1NtqNEnmhPhFNiNcq01YObSQecZDgMayb3MdKWZN1o1
dly1GAd/OOM1JmJZYQIDAQABo1MwUTAdBgNVHQ4EFgQU/i+fKInZq7E5rghna1bA
T9M4PfwwHwYDVR0jBBgwFoAU/i+fKInZq7E5rghna1bAT9M4PfwwDwYDVR0TAQH/
BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAF9fXNKv47eTuOFwbIihHJOzvAHoT
zd1F1xdn5RcAYsoyg2TfP0ft635ByoTNLn75lGW4PQhZ73hi2msIenQGPm/Kmj8z
t8pgol90PF4RnJAiC0Oiji+iN+35SC1LIDEoVGkkXDvmR96sjjGGpP4TYpLPPhTc
5ig8+x9i2gqHyBx1r0IjYv2E3UYDUt603bd/a0jwUp1A5wGjUW7PPsGx8PAkeeAU
65+aDqeuoDz85Hzs+5Jzq2AWoH92Fxnw8liR9Hx2WzT78AN2yFVZZMOiMEZG+Jzf
eCmsT4RQBHcezi9kdW0Hqx90qfulP8YiaCzcNXBJK8WpFce/plxfnakHUw==
-----END CERTIFICATE-----
)";

const char *const kCertB = R"(-----BEGIN CERTIFICATE-----
MIIDJzCCAg+gAwIBAgIUUrZkV7lfWMLrDB1Zgme8X8kQpJMwDQYJKoZIhvcNAQEL
BQAwIzEhMB8GA1UEAwwYTlZJRElBIEdhbWVTdHJlYW0gQ2xpZW50MB4XDTI2MTAw
ODA1MjkxNVoXDTQ2MTAwMzA1MjkxNVowIzEhMB8GA1UEAwwYTlZJRElBIEdhbWVT
dHJlYW0gQ2xpZW50MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAvdcM
nrmisiRNxKLMzYdyPtHOp2O4nRqHsh4pGBBvAIV1zylwUjMH2Dq+IwxowbDYXM6B
qERYD/LtW5RIGmEwQQUOYjUR6HuSduQGMqIKzgHOGv6gyeQAXRcdiZlOGs+5R/HX
eIJ8VyzlWzkpb6WIt+Rfs4Ln7RV5z2P16/9qTXKFCVnsW3nCR2iKRocrwD0sL14h
rXBYiiegpkX6dZSwkXBi0msMnnazC+cCGnVIsEL0MPZZKYOb3BWDDBF+mK5JJdjl
GIYxH538HEGmo/62hogk7CowHSyPpZVXzf9/ErgwhK1UoswY1aJHkkDWtXUxiUsS
cIRhgQEJjbY200Mq5wIDAQABo1MwUTAdBgNVHQ4EFgQUB+wIDERhXoDdpQsNqUId
v3iBtnowHwYDVR0jBBgwFoAUB+wIDERhXoDdpQsNqUIdv3iBtnowDwYDVR0TAQH/
BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEALo+anqL92GT8koJYc+phGN/lBzFa
G7Wo8mPt/GBve4wvYQ/EGDfyZCfVC+fpibLAF6cJimrcyI4qHyUgEfeq2tTEUQP+
TbIjvewcKVu5uwrvyNx36Ublvs0Nma1/tIFA00yLj4t+8mqxNLkG/EjsrSb/JjLz
kZL8oWlJ+LSn+p4Xa9mHor7xGQXVN3el+IewzEi1ECG60JfcomiombARpWTq6q0b
nbXuz8faRSr4/rE03T3Cn4FekRoxMl0FYms1lVxMm0ZIJqzC8kr0RQLM7jRE8v+T
Se1tWjOKbeTRi1typJD2a8Img/y2RnbjRz2YfxkB2Rya++Ba9bqGxWCM4Q==
-----END CERTIFICATE-----
)";

// By openssl (dgst -sha256 -sign), over signed_text(kNonce, kCommand).
const char *const kSigByA =
    "QLhki4ehlXRlllbDrfq6lALD9eSHetGHRvhvqFRAY8nGcf5xNFqqaN+3stT6j5auH8gRYiRkJ1IIi5Vg1B+FW/UDsfyM10XRrTc2B4VH50LD"
    "gbe8kkV6a3J2PnE/8Gj6yx0bXF6gJvPEd55LmYCPFGffwdPJQ4kXoh1pG8wdR/lAQix+Y6cW9/ZZdQnqXTRUGV6U/9Fg0hq5puMVqketQy6h"
    "FgUinOT9mAIsFuDuJ7wwNCJy9E32wnd/2H5PzQ2C71Z8NRegFaKP+WNnghrsgRkxS/bX0rWH/x0GnzUNt3OpiDjWDLLl57eGewnv0fwALNKe"
    "fmwgeraiJVcQ9yyE2Q==";
const char *const kSigByB =
    "LStXjR6Rii6eiUvgE7iRFldfqGgmEsfv090Q8vwRdyeszQCDn6kLOfUsoV+PAOY1V5IZ503YGgdbhBxy6On6qqrbJdSoteYiT5ESxMEJHsDa"
    "lfMlbkNP/CSTLzaBj/RJoketC0ti3uV5W78T7uxJ73RieJKx/Oh34NZXLPVLqqbYnYQHExoyZCyYes3woJpVlZZJYn2VQMMMye8ioshSh/Gw"
    "FI5SJvq/k5199PMNugZW/GmcWpCGH1kGwGkevOysmNDapm5AkZ8UWF8dj4j8sZOhmy8kvpb9ImqJYqEv39J+UocWqZnkwu3dzmhLSyp1sKGt"
    "KfcB3SYeK1JsQ1NTuA==";

const char *const kNonce = "00112233445566778899aabbccddeeff";
const char *const kCommand = "BRIDGE_START ds5 48056 ctm-ds5-3";
// openssl x509 -outform DER | sha256sum, the first 32 hex.
const char *const kFpA = "0842278520b2043b5d3a153a71b4d87f";
const char *const kFpB = "a2397daff58d8e6c79378573a7f7d360";

uint32_t ip(unsigned a, unsigned b, unsigned c, unsigned d)
{
    return (a << 24) | (b << 16) | (c << 8) | d;
}

std::vector<uint8_t> b64(const char *text)
{
    std::vector<uint8_t> out;
    paired_tv::base64_decode(text, &out);
    return out;
}

// A string as JSON writes it.
std::string json_string(const std::string &s)
{
    std::string out = "\"";
    for (const char c : s) {
        if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out.push_back(c);
    }
    return out + "\"";
}

// A host state file in the shape Apollo writes, with A and B paired.
std::string state_file()
{
    return std::string("{\n  \"password\": \"0123456789abcdef\",\n  \"root\": {\n    \"named_devices\": [\n") +
           "      {\"allow_client_commands\": true, \"cert\": " + json_string(kCertA) +
           ", \"do\": [{\"cmd\": \"x\", \"elevated\": false}], \"last_seen\": 1759800000, " +
           "\"name\": \"C1\", \"perm\": 2147418111, \"uuid\": \"11111111-2222-3333-4444-555555555555\"},\n" +
           "      {\"name\": \"C3 \\u00e9\", \"uuid\": \"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\", \"cert\": " +
           json_string(kCertB) + "}\n    ],\n    \"uniqueid\": \"99999999-8888-7777-6666-555555555555\"\n  },\n" +
           "  \"salt\": \"0011223344556677\",\n  \"username\": \"someone\"\n}\n";
}

} // namespace

int run_paired_tv_tests()
{
    section("paired tv: a certificate's fingerprint is what openssl makes of it");
    {
        CTM_CHECK_EQ(paired_tv::fingerprint_of(kCertA), std::string(kFpA));
        CTM_CHECK_EQ(paired_tv::fingerprint_of(kCertB), std::string(kFpB));
        CTM_CHECK(paired_tv::fingerprint_of("not a certificate").empty());
    }

    section("paired tv: a signature is accepted from its own key and nothing else");
    {
        const std::string text = paired_tv::signed_text(kNonce, kCommand);
        CTM_CHECK(paired_tv::verify(kCertA, text, b64(kSigByA)));
        CTM_CHECK(!paired_tv::verify(kCertA, text, b64(kSigByB)));        // a stranger's key
        CTM_CHECK(!paired_tv::verify(kCertB, text, b64(kSigByA)));        // the wrong device
        CTM_CHECK(!paired_tv::verify(kCertA, paired_tv::signed_text(kNonce, "BRIDGE_START hid 48056 ctm-ds5-3"),
                                     b64(kSigByA)));                         // another command
        CTM_CHECK(!paired_tv::verify(kCertA, paired_tv::signed_text("ffeeddccbbaa99887766554433221100", kCommand),
                                     b64(kSigByA)));                         // another challenge
        CTM_CHECK(!paired_tv::verify(kCertA, text, std::vector<uint8_t>()));
    }

    section("paired tv: the proof comes off the end of the line, and the command is what was signed");
    {
        std::string command;
        paired_tv::Proof proof;
        const std::string line = std::string(kCommand) + " PROOF=" + kNonce + ":" + kFpA + ":" + kSigByA;
        CTM_CHECK(paired_tv::split(line, &command, &proof));
        CTM_CHECK_EQ(command, std::string(kCommand));
        CTM_CHECK_EQ(proof.nonce, std::string(kNonce));
        CTM_CHECK_EQ(proof.fingerprint, std::string(kFpA));
        CTM_CHECK_EQ(proof.signature64, std::string(kSigByA));
        CTM_CHECK(!paired_tv::split(kCommand, &command, &proof));
        CTM_CHECK_EQ(command, std::string(kCommand));
        CTM_CHECK(paired_tv::split(std::string(kCommand) + " PROOF=nonsense", &command, &proof));
        CTM_CHECK(proof.nonce.empty());
    }

    section("paired tv: a challenge is used once, by the caller it was given to, within 30 seconds");
    {
        using Clock = paired_tv::Nonces::Clock;
        const Clock::time_point t0 = Clock::now();
        paired_tv::Nonces nonces;
        nonces.issue("n1", ip(203, 0, 113, 7), t0);
        CTM_CHECK(!nonces.redeem("n1", ip(198, 51, 100, 9), t0));          // not this caller's
        CTM_CHECK(nonces.redeem("n1", ip(203, 0, 113, 7), t0));
        CTM_CHECK(!nonces.redeem("n1", ip(203, 0, 113, 7), t0));           // once
        nonces.issue("n2", ip(203, 0, 113, 7), t0);
        CTM_CHECK(!nonces.redeem("n2", ip(203, 0, 113, 7), t0 + std::chrono::seconds(31)));
        CTM_CHECK(!nonces.redeem("never", ip(203, 0, 113, 7), t0));
        // ⓘ Anyone may ask for one, so asking cannot grow the list without end.
        for (int i = 0; i < 1000; ++i) nonces.issue("x" + std::to_string(i), ip(203, 0, 113, 7), t0);
        CTM_CHECK(nonces.size() <= paired_tv::Nonces::kMax);
        CTM_CHECK(nonces.redeem("x999", ip(203, 0, 113, 7), t0));          // the newest survive
    }

    section("paired tv: a proof is needed from the internet, not from this PC or the home network");
    {
        using paired_tv::Need;
        CTM_CHECK(paired_tv::need_for(ip(127, 0, 0, 1), true, true) == Need::NotNeeded);
        CTM_CHECK(paired_tv::need_for(ip(192, 168, 1, 129), true, false) == Need::NotNeeded);
        CTM_CHECK(paired_tv::need_for(ip(10, 1, 2, 3), true, false) == Need::NotNeeded);
        CTM_CHECK(paired_tv::need_for(ip(172, 31, 255, 1), true, false) == Need::NotNeeded);
        CTM_CHECK(paired_tv::need_for(ip(100, 101, 102, 103), true, false) == Need::NotNeeded);   // Tailscale
        CTM_CHECK(paired_tv::need_for(ip(169, 254, 3, 4), true, false) == Need::NotNeeded);
        CTM_CHECK(paired_tv::need_for(ip(203, 0, 113, 7), true, false) == Need::Required);
        CTM_CHECK(paired_tv::need_for(ip(172, 32, 0, 1), true, false) == Need::Required);        // past 172.16/12
        CTM_CHECK(paired_tv::need_for(ip(172, 15, 0, 1), true, false) == Need::Required);
        CTM_CHECK(paired_tv::need_for(ip(100, 128, 0, 1), true, false) == Need::Required);       // past 100.64/10
        CTM_CHECK(paired_tv::need_for(ip(100, 63, 255, 255), true, false) == Need::Required);
        // The home network too, when asked for.
        CTM_CHECK(paired_tv::need_for(ip(192, 168, 1, 129), true, true) == Need::Required);
        // And nobody, with the check switched off.
        CTM_CHECK(paired_tv::need_for(ip(203, 0, 113, 7), false, false) == Need::NotNeeded);
    }

    section("paired tv: the host's paired devices are read from its state file");
    {
        const std::vector<paired_tv::Device> devices = paired_tv::devices_from_state(state_file());
        CTM_CHECK_EQ(devices.size(), static_cast<size_t>(2));
        if (devices.size() == 2) {
            CTM_CHECK_EQ(devices[0].name, std::string("C1"));
            CTM_CHECK_EQ(devices[0].uuid, std::string("11111111-2222-3333-4444-555555555555"));
            CTM_CHECK_EQ(paired_tv::fingerprint_of(devices[0].certPem), std::string(kFpA));
            CTM_CHECK_EQ(devices[1].name, std::string("C3 ?"));       // not ASCII: shown, never matched
            CTM_CHECK_EQ(paired_tv::fingerprint_of(devices[1].certPem), std::string(kFpB));
        }
        // ⓘ What it cannot read gives nothing, or what it had: never a crash.
        CTM_CHECK(paired_tv::devices_from_state("").empty());
        CTM_CHECK(paired_tv::devices_from_state("{\"root\": {}}").empty());
        CTM_CHECK(paired_tv::devices_from_state("not json").empty());
        const std::string cut = state_file().substr(0, state_file().find("\"name\": \"C3"));
        CTM_CHECK(paired_tv::devices_from_state(cut).size() <= 1);
    }

    section("paired tv: the whole check, for a caller from the internet");
    {
        using paired_tv::Verdict;
        using Clock = paired_tv::Nonces::Clock;
        const Clock::time_point t0 = Clock::now();
        const uint32_t tv = ip(203, 0, 113, 7);
        const std::vector<paired_tv::Device> devices = paired_tv::devices_from_state(state_file());
        const std::string good = std::string(kCommand) + " PROOF=" + kNonce + ":" + kFpA + ":" + kSigByA;
        paired_tv::Nonces nonces;
        std::string who;

        nonces.issue(kNonce, tv, t0);
        CTM_CHECK(paired_tv::check(good, tv, paired_tv::Need::Required, &nonces, &devices, &who, t0) == Verdict::Proven);
        CTM_CHECK_EQ(who, std::string("C1"));
        // The same line again: its challenge is spent.
        CTM_CHECK(paired_tv::check(good, tv, paired_tv::Need::Required, &nonces, &devices, &who, t0) == Verdict::StaleChallenge);
        // From another address, with the challenge given to this one.
        nonces.issue(kNonce, tv, t0);
        CTM_CHECK(paired_tv::check(good, ip(198, 51, 100, 9), paired_tv::Need::Required, &nonces, &devices, &who, t0) == Verdict::StaleChallenge);
        // A stranger's signature under C1's name.
        nonces.issue(kNonce, tv, t0);
        const std::string forged = std::string(kCommand) + " PROOF=" + kNonce + ":" + kFpA + ":" + kSigByB;
        CTM_CHECK(paired_tv::check(forged, tv, paired_tv::Need::Required, &nonces, &devices, &who, t0) == Verdict::BadSignature);
        // A device the host never paired.
        nonces.issue(kNonce, tv, t0);
        const std::string unknown = std::string(kCommand) + " PROOF=" + kNonce + ":ffffffffffffffffffffffffffffffff:" + kSigByA;
        CTM_CHECK(paired_tv::check(unknown, tv, paired_tv::Need::Required, &nonces, &devices, &who, t0) == Verdict::UnknownDevice);
        // A command changed after it was signed.
        nonces.issue(kNonce, tv, t0);
        const std::string changed = std::string("BRIDGE_START hid 48056 ctm-ds5-3 PROOF=") + kNonce + ":" + kFpA + ":" + kSigByA;
        CTM_CHECK(paired_tv::check(changed, tv, paired_tv::Need::Required, &nonces, &devices, &who, t0) == Verdict::BadSignature);
        // No proof at all, a malformed one, and no list from the host.
        CTM_CHECK(paired_tv::check(kCommand, tv, paired_tv::Need::Required, &nonces, &devices, &who, t0) == Verdict::Missing);
        CTM_CHECK(paired_tv::check(std::string(kCommand) + " PROOF=x:y:z", tv, paired_tv::Need::Required, &nonces, &devices, &who, t0) == Verdict::Malformed);
        nonces.issue(kNonce, tv, t0);
        CTM_CHECK(paired_tv::check(good, tv, paired_tv::Need::Required, &nonces, nullptr, &who, t0) == Verdict::NoHostState);
        CTM_CHECK(!paired_tv::lets_in(Verdict::NoHostState));
    }

    section("paired tv: from the home network a proof is checked, and only logged");
    {
        using paired_tv::Verdict;
        using Clock = paired_tv::Nonces::Clock;
        const Clock::time_point t0 = Clock::now();
        const uint32_t home = ip(192, 168, 1, 129);
        const std::vector<paired_tv::Device> devices = paired_tv::devices_from_state(state_file());
        paired_tv::Nonces nonces;
        std::string who;
        // No proof: in, as before.
        CTM_CHECK(paired_tv::check(kCommand, home, paired_tv::Need::NotNeeded, &nonces, &devices, &who, t0) == Verdict::NotNeeded);
        // A bad one: still in, since none was needed.
        const std::string forged = std::string(kCommand) + " PROOF=" + kNonce + ":" + kFpA + ":" + kSigByB;
        nonces.issue(kNonce, home, t0);
        CTM_CHECK(paired_tv::check(forged, home, paired_tv::Need::NotNeeded, &nonces, &devices, &who, t0) == Verdict::NotNeeded);
        // A good one: proven, so the log can say who.
        const std::string good = std::string(kCommand) + " PROOF=" + kNonce + ":" + kFpA + ":" + kSigByA;
        nonces.issue(kNonce, home, t0);
        CTM_CHECK(paired_tv::check(good, home, paired_tv::Need::NotNeeded, &nonces, &devices, &who, t0) == Verdict::Proven);
    }

    section("paired tv: challenges are random, and the right length");
    {
        const std::string a = paired_tv::new_nonce();
        const std::string b = paired_tv::new_nonce();
        CTM_CHECK_EQ(a.size(), static_cast<size_t>(32));
        CTM_CHECK(a != b);
    }

    return 0;
}
