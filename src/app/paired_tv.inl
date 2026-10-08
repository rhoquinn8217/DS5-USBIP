// Which TVs may bridge from outside this PC's own network: the ones paired
// with this PC's streaming host, proving it with the key they paired with.
//
// ⛔⛔ WHY (code review, 2026-10-05). The control port is forwarded to the
// internet for remote play, and BRIDGE_START from anyone made Windows see a
// USB keyboard the caller could type on. rhoquinn8217 chose, 2026-10-07, to
// trust exactly the devices the stream already trusts: a TV that can stream
// from this PC has a client certificate paired with Apollo (or Sunshine),
// whose state file lists every paired device's certificate.
//
// ⭐ THE PROOF. The TV asks for a challenge (PROOF_CHALLENGE, answered "OK
// NONCE <32 hex>"), then sends its command with one more word on the end:
//
//     BRIDGE_START ds5 48056 ctm-ds5-3 PROOF=<nonce>:<fingerprint>:<signature>
//
// - the nonce is the one it was given: single use, 30 seconds, and only from
//   the address it was given to;
// - the fingerprint is the first 16 bytes, in hex, of the SHA-256 of its
//   certificate (DER), naming which paired device it claims to be;
// - the signature is RSA PKCS#1 v1.5 over SHA-256, in base64, by the key it
//   paired with, of signed_text(): a fixed first line, the nonce and the
//   command, so it can be used for nothing else.
// ⓘ A listener that does not know PROOF_CHALLENGE answers "ERR unknown
// command", and the TV then sends the command plainly; one that does not know
// PROOF= reads the command's own words and never looks at the last one. So a
// new TV works with an old listener and an old TV with a new one -- where no
// proof is needed.
//
// ⭐ WHO NEEDS ONE. This PC never does. A caller on the home network does not,
// unless [global] require_paired_tv_on_home_network = true: the forwarded port
// is what reaches the world, and a TV at home on an older build keeps
// bridging. A caller from the internet always does, unless [global]
// require_paired_tv = false turns the check off (it is then logged, not
// enforced). ⓘ The home network means the private ranges, Tailscale's
// included: a forwarded port's caller arrives from its own public address.

#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace paired_tv {

// ---------------------------------------------------------------- the text --

inline const char *const kProofWord = " PROOF=";

// What the TV signs: a first line no other message of anyone's begins with,
// so a signature over this cannot be passed off as one over anything else.
inline std::string signed_text(const std::string &nonce, const std::string &command)
{
    return "DS5-USBIP proof 1\n" + nonce + "\n" + command;
}

struct Proof {
    std::string nonce;          // 32 lowercase hex
    std::string fingerprint;    // 32 lowercase hex
    std::string signature64;    // base64, as sent
};

// The command as it was before the proof was added, and the proof if there
// was one. ⓘ The command is everything before the LAST " PROOF=", which is the
// exact text the TV signed; nothing after the proof is read.
inline bool split(const std::string &line, std::string *command, Proof *proof)
{
    const size_t at = line.rfind(kProofWord);
    if (at == std::string::npos) {
        if (command) *command = line;
        return false;
    }
    if (command) *command = line.substr(0, at);
    const std::string token = line.substr(at + std::char_traits<char>::length(kProofWord));
    const size_t c1 = token.find(':');
    const size_t c2 = c1 == std::string::npos ? std::string::npos : token.find(':', c1 + 1);
    if (c1 == std::string::npos || c2 == std::string::npos) {
        if (proof) *proof = Proof{};
        return true;            // there WAS a proof, and it is malformed
    }
    if (proof) {
        proof->nonce = token.substr(0, c1);
        proof->fingerprint = token.substr(c1 + 1, c2 - c1 - 1);
        proof->signature64 = token.substr(c2 + 1);
        // A trailing space or a stray word after the signature is not base64.
        const size_t space = proof->signature64.find(' ');
        if (space != std::string::npos) proof->signature64.resize(space);
    }
    return true;
}

// -------------------------------------------------------------- addresses --

// Host byte order (ntohl of sin_addr).
inline bool is_this_pc(uint32_t a) { return (a >> 24) == 127u; }

inline bool is_home_network(uint32_t a)
{
    return (a >> 24) == 10u ||                     // 10.0.0.0/8
           (a >> 20) == 0xAC1u ||                  // 172.16.0.0/12
           (a >> 16) == 0xC0A8u ||                 // 192.168.0.0/16
           (a >> 16) == 0xA9FEu ||                 // 169.254.0.0/16, link-local
           (a >> 22) == (0x64400000u >> 22);       // 100.64.0.0/10: Tailscale, carrier NAT
}

enum class Need { NotNeeded, Required };

inline Need need_for(uint32_t peer, bool requireAtAll, bool requireAtHome)
{
    if (is_this_pc(peer) || !requireAtAll) return Need::NotNeeded;
    if (is_home_network(peer)) return requireAtHome ? Need::Required : Need::NotNeeded;
    return Need::Required;
}

// ----------------------------------------------------------------- nonces --

// Challenges given out and not yet used. ⓘ Bounded: anyone may ask for one,
// so a flood of asks must not grow this without end; the oldest goes first.
class Nonces {
public:
    static constexpr std::chrono::seconds kLife{30};
    static constexpr size_t kMax = 256;

    using Clock = std::chrono::steady_clock;

    void issue(const std::string &nonce, uint32_t peer, Clock::time_point now)
    {
        prune(now);
        if (byNonce_.size() >= kMax) {
            auto oldest = byNonce_.begin();
            for (auto it = byNonce_.begin(); it != byNonce_.end(); ++it) {
                if (it->second.issued < oldest->second.issued) oldest = it;
            }
            byNonce_.erase(oldest);
        }
        byNonce_[nonce] = Entry{peer, now};
    }

    // True once: for a nonce given to this peer, within its life.
    bool redeem(const std::string &nonce, uint32_t peer, Clock::time_point now)
    {
        prune(now);
        const auto it = byNonce_.find(nonce);
        if (it == byNonce_.end() || it->second.peer != peer) return false;
        byNonce_.erase(it);
        return true;
    }

    size_t size() const { return byNonce_.size(); }

private:
    struct Entry { uint32_t peer = 0; Clock::time_point issued; };

    void prune(Clock::time_point now)
    {
        for (auto it = byNonce_.begin(); it != byNonce_.end(); ) {
            if (now - it->second.issued > kLife) it = byNonce_.erase(it);
            else ++it;
        }
    }

    std::map<std::string, Entry> byNonce_;
};

// ------------------------------------------------- the host's paired devices --

struct Device {
    std::string name;
    std::string uuid;
    std::string certPem;
};

// A small JSON reader: just enough to walk the host's state file to
// root.named_devices and read three strings from each. ⓘ Anything it does not
// understand stops the walk with what it has, rather than guessing.
class Json {
public:
    explicit Json(const std::string &text) : s_(text) {}

    std::vector<Device> named_devices()
    {
        std::vector<Device> out;
        ws();
        if (!peek('{')) return out;
        walk_object([&](const std::string &key) {
            if (key == "root") {
                ws();
                if (!peek('{')) return skip_value();
                return walk_object([&](const std::string &inner) {
                    if (inner != "named_devices") return skip_value();
                    return read_devices(&out);
                });
            }
            return skip_value();
        });
        return out;
    }

private:
    bool peek(char c) { ws(); return i_ < s_.size() && s_[i_] == c; }

    void ws()
    {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\r' || s_[i_] == '\n')) ++i_;
    }

    template <typename F>
    bool walk_object(F onKey)
    {
        if (!peek('{')) return false;
        ++i_;
        if (peek('}')) { ++i_; return true; }
        for (;;) {
            std::string key;
            if (!read_string(&key)) return false;
            if (!peek(':')) return false;
            ++i_;
            if (!onKey(key)) return false;
            if (peek(',')) { ++i_; continue; }
            if (peek('}')) { ++i_; return true; }
            return false;
        }
    }

    bool read_devices(std::vector<Device> *out)
    {
        if (!peek('[')) return skip_value();
        ++i_;
        if (peek(']')) { ++i_; return true; }
        for (;;) {
            Device d;
            if (peek('{')) {
                const bool ok = walk_object([&](const std::string &key) {
                    std::string *into = key == "name" ? &d.name
                                      : key == "uuid" ? &d.uuid
                                      : key == "cert" ? &d.certPem : nullptr;
                    if (into != nullptr && peek('"')) return read_string(into);
                    return skip_value();
                });
                if (!ok) return false;
                if (!d.certPem.empty()) out->push_back(d);
            } else if (!skip_value()) {
                return false;
            }
            if (peek(',')) { ++i_; continue; }
            if (peek(']')) { ++i_; return true; }
            return false;
        }
    }

    bool read_string(std::string *out)
    {
        if (!peek('"')) return false;
        ++i_;
        std::string v;
        while (i_ < s_.size()) {
            const char c = s_[i_++];
            if (c == '"') { if (out) *out = v; return true; }
            if (c != '\\') { v.push_back(c); continue; }
            if (i_ >= s_.size()) return false;
            const char e = s_[i_++];
            switch (e) {
            case 'n': v.push_back('\n'); break;
            case 'r': v.push_back('\r'); break;
            case 't': v.push_back('\t'); break;
            case 'b': v.push_back('\b'); break;
            case 'f': v.push_back('\f'); break;
            case 'u': {
                // ⓘ Kept only for plain ASCII; anything else becomes '?'.
                // Names are shown, never matched, and certificates are ASCII.
                if (i_ + 4 > s_.size()) return false;
                unsigned code = 0;
                for (int k = 0; k < 4; ++k) {
                    const char h = s_[i_++];
                    code <<= 4;
                    if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
                    else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
                    else return false;
                }
                v.push_back(code < 0x80 ? static_cast<char>(code) : '?');
                break;
            }
            default: v.push_back(e); break;     // \" \\ \/
            }
        }
        return false;
    }

    bool skip_value()
    {
        ws();
        if (i_ >= s_.size()) return false;
        const char c = s_[i_];
        if (c == '"') return read_string(nullptr);
        if (c == '{') return walk_object([&](const std::string &) { return skip_value(); });
        if (c == '[') {
            ++i_;
            if (peek(']')) { ++i_; return true; }
            for (;;) {
                if (!skip_value()) return false;
                if (peek(',')) { ++i_; continue; }
                if (peek(']')) { ++i_; return true; }
                return false;
            }
        }
        // A number, true, false or null: up to the next delimiter.
        const size_t start = i_;
        while (i_ < s_.size() && s_[i_] != ',' && s_[i_] != '}' && s_[i_] != ']' &&
               s_[i_] != ' ' && s_[i_] != '\r' && s_[i_] != '\n' && s_[i_] != '\t') ++i_;
        return i_ > start;
    }

    const std::string &s_;
    size_t i_ = 0;
};

inline std::vector<Device> devices_from_state(const std::string &json)
{
    return Json(json).named_devices();
}

// ----------------------------------------------------------- the crypto (CNG) --

inline bool base64_decode(const std::string &text, std::vector<uint8_t> *out)
{
    DWORD size = 0;
    if (!CryptStringToBinaryA(text.c_str(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64,
                              nullptr, &size, nullptr, nullptr) || size == 0) {
        return false;
    }
    out->assign(size, 0);
    if (!CryptStringToBinaryA(text.c_str(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64,
                              out->data(), &size, nullptr, nullptr)) {
        return false;
    }
    out->resize(size);
    return true;
}

inline bool pem_to_der(const std::string &pem, std::vector<uint8_t> *der)
{
    DWORD size = 0;
    if (!CryptStringToBinaryA(pem.c_str(), static_cast<DWORD>(pem.size()), CRYPT_STRING_BASE64HEADER,
                              nullptr, &size, nullptr, nullptr) || size == 0) {
        return false;
    }
    der->assign(size, 0);
    if (!CryptStringToBinaryA(pem.c_str(), static_cast<DWORD>(pem.size()), CRYPT_STRING_BASE64HEADER,
                              der->data(), &size, nullptr, nullptr)) {
        return false;
    }
    der->resize(size);
    return true;
}

inline bool sha256(const uint8_t *data, size_t len, std::array<uint8_t, 32> *out)
{
    return BCRYPT_SUCCESS(BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
                                     const_cast<PUCHAR>(data), static_cast<ULONG>(len),
                                     out->data(), static_cast<ULONG>(out->size())));
}

inline std::string to_hex(const uint8_t *data, size_t len)
{
    static const char *const kHex = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(kHex[data[i] >> 4]);
        out.push_back(kHex[data[i] & 0x0F]);
    }
    return out;
}

// The first 16 bytes of the certificate's SHA-256, in lowercase hex: what a
// TV names itself by. Empty when the certificate cannot be read.
inline std::string fingerprint_of(const std::string &certPem)
{
    std::vector<uint8_t> der;
    std::array<uint8_t, 32> digest{};
    if (!pem_to_der(certPem, &der) || !sha256(der.data(), der.size(), &digest)) return std::string();
    return to_hex(digest.data(), 16);
}

// RSA PKCS#1 v1.5 over SHA-256 of `message`, by the key in `certPem`.
inline bool verify(const std::string &certPem, const std::string &message, const std::vector<uint8_t> &signature)
{
    std::vector<uint8_t> der;
    if (!pem_to_der(certPem, &der) || signature.empty()) return false;
    PCCERT_CONTEXT cert = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                                       der.data(), static_cast<DWORD>(der.size()));
    if (cert == nullptr) return false;
    BCRYPT_KEY_HANDLE key = nullptr;
    const bool imported = CryptImportPublicKeyInfoEx2(X509_ASN_ENCODING, &cert->pCertInfo->SubjectPublicKeyInfo,
                                                      0, nullptr, &key) != FALSE;
    CertFreeCertificateContext(cert);
    if (!imported) return false;
    std::array<uint8_t, 32> digest{};
    bool ok = sha256(reinterpret_cast<const uint8_t *>(message.data()), message.size(), &digest);
    if (ok) {
        BCRYPT_PKCS1_PADDING_INFO padding = { BCRYPT_SHA256_ALGORITHM };
        ok = BCRYPT_SUCCESS(BCryptVerifySignature(key, &padding, digest.data(), static_cast<ULONG>(digest.size()),
                                                  const_cast<PUCHAR>(signature.data()),
                                                  static_cast<ULONG>(signature.size()), BCRYPT_PAD_PKCS1));
    }
    BCryptDestroyKey(key);
    return ok;
}

// 16 random bytes, in hex.
inline std::string new_nonce()
{
    std::array<uint8_t, 16> bytes{};
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                                        BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
        return std::string();
    }
    return to_hex(bytes.data(), bytes.size());
}

// ---------------------------------------------------------------- the check --

enum class Verdict {
    Missing,        // no proof was sent
    Proven,         // a paired device, proven
    Malformed,      // a proof that is not one
    StaleChallenge, // a nonce never given to this caller, used, or too old
    UnknownDevice,  // no paired device has that fingerprint
    BadSignature,   // the fingerprint is known, the signature is not its key's
    NoHostState,    // the host's list of paired devices could not be read
};

// ⓘ Whether this verdict lets the caller in, where a proof is or is not
// needed. Where one is not, nothing about a proof turns anyone away.
inline bool lets_in(Verdict v, Need need)
{
    return need == Need::NotNeeded || v == Verdict::Proven;
}

inline const char *describe(Verdict v)
{
    switch (v) {
    case Verdict::Missing:        return "no pairing proof was sent";
    case Verdict::Proven:         return "proven by a TV paired with this PC";
    case Verdict::Malformed:      return "the pairing proof was malformed";
    case Verdict::StaleChallenge: return "the pairing proof's challenge was not this caller's, was used, or expired";
    case Verdict::UnknownDevice:  return "no device paired with this PC has that certificate";
    case Verdict::BadSignature:   return "the pairing proof's signature is not that device's";
    case Verdict::NoHostState:    return "this PC's streaming host's list of paired devices could not be read";
    }
    return "?";
}

// What the proof on one line amounts to, given the caller, the challenges
// given out and the host's devices. ⓘ The real verdict, always: a TV at home
// that sends a wrong proof is reported as wrong, so the log shows it, and
// lets_in() decides separately whether that matters for this caller.
inline Verdict check(const std::string &line, uint32_t peer, Nonces *nonces,
                     const std::vector<Device> *devices, std::string *who,
                     Nonces::Clock::time_point now)
{
    std::string command;
    Proof proof;
    const bool hadProof = split(line, &command, &proof);
    if (!hadProof) return Verdict::Missing;

    Verdict verdict = Verdict::Proven;
    std::vector<uint8_t> signature;
    if (proof.nonce.size() != 32 || proof.fingerprint.size() != 32 ||
        !base64_decode(proof.signature64, &signature)) {
        verdict = Verdict::Malformed;
    } else if (!nonces->redeem(proof.nonce, peer, now)) {
        verdict = Verdict::StaleChallenge;
    } else if (devices == nullptr) {
        verdict = Verdict::NoHostState;
    } else {
        const Device *match = nullptr;
        for (const Device &d : *devices) {
            if (fingerprint_of(d.certPem) == proof.fingerprint) { match = &d; break; }
        }
        if (match == nullptr) {
            verdict = Verdict::UnknownDevice;
        } else if (!verify(match->certPem, signed_text(proof.nonce, command), signature)) {
            verdict = Verdict::BadSignature;
        } else if (who) {
            *who = match->name;
        }
    }
    return verdict;
}

} // namespace paired_tv
