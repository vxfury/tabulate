/**
 * Copyright 2022 Kiran Nowak(kiran.nowak@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "psk.h"
#include <mutex>
#include <stdio.h>
#include <unistd.h>

#define TRACE(fmt, ...) printf("%s:%d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

static constexpr size_t psk_type_size = 3;
static constexpr size_t psk_ciphersuite_len = 2;
static constexpr unsigned char tls13_aes128gcmsha256_id[] = {0x13, 0x01};
static constexpr unsigned char tls13_aes256gcmsha384_id[] = {0x13, 0x02};

OPENSSL_PSK &OPENSSL_PSK::Instance()
{
    static OPENSSL_PSK instance;
    return instance;
}

/* map<type, indenity> */
static const unsigned char __PSK_IDENTITY_ACCESS[] = "OpenSSL Access PSK";
static const unsigned char __PSK_IDENTITY_REFRESH[] = "OpenSSL Refresh PSK";
static const std::map<int, std::basic_string<unsigned char>> identities = {
    {1, {__PSK_IDENTITY_ACCESS, sizeof(__PSK_IDENTITY_ACCESS)}},
    {2, {__PSK_IDENTITY_REFRESH, sizeof(__PSK_IDENTITY_REFRESH)}},
    {3, {nullptr, 0}},
};

const std::basic_string<unsigned char> &OPENSSL_PSK::GetIdentity(int type)
{
    if (identities.count(type)) {
        return identities.at(type);
    } else {
        return identities.at(3);
    }
}

int GetTypeFromIdentity(const unsigned char *identity, size_t identity_len)
{
    for (auto pair : identities) {
        if (identity_len == pair.second.size() && memcmp(identity, pair.second.data(), identity_len) == 0) {
            return pair.first;
        }
    }
    return -1;
}

OPENSSL_PSK::Ticket *OPENSSL_PSK::GetTicket(int type, bool crete_if_not_exists)
{
    if (tickets.count(type)) {
        return tickets.at(type);
    }
    if (crete_if_not_exists) {
        tickets[type] = new Ticket();
        return tickets[type];
    }

    return nullptr;
}

bool OPENSSL_PSK::HasTicket(int type)
{
    if (type >= 0) {
        return GetTicket(type) != nullptr;
    } else {
        for (int type = 0; type < psk_type_size; type++) {
            if (GetTicket(type) != nullptr) {
                return true;
            }
        }
        return false;
    }
}

// server callback for openssl, return 1 if no error even if session not found
int OPENSSL_PSK::FindSessionCallback(SSL *ssl, const unsigned char *identity, size_t identity_len,
                                     SSL_SESSION **session)
{
    int type = GetTypeFromIdentity(identity, identity_len);
    if (type == -1) {
        TRACE("Unkown PSK identity(%s, %zu)", (const char *)identity, identity_len);
        *session = nullptr;
        return 1;
    }

    int ret = 1;
    pthread_rwlock_rdlock(&OPENSSL_PSK::Instance().lock);
    OPENSSL_PSK::Ticket *ticket = OPENSSL_PSK::Instance().GetTicket(type);
    if (ticket != nullptr) {
        if (ticket->GetSession(ssl, session) != 0 && !ticket->IsExpired()) {
            TRACE("Expired PSK");
            ret = 0;
        } else {
            TRACE("PSK found identity(%s, %zu)", (const char *)identity, identity_len);
        }
    } else {
        *session = nullptr;
    }
    pthread_rwlock_unlock(&OPENSSL_PSK::Instance().lock);

    return ret;
}

// client callback for openssl, return 1 if no error
int OPENSSL_PSK::UseSessionCallback(SSL *ssl, const EVP_MD *md, const unsigned char **identity, size_t *identity_len,
                                    SSL_SESSION **session)
{
    pthread_rwlock_rdlock(&OPENSSL_PSK::Instance().lock);
    for (int type = 0; type < psk_type_size; type++) {
        OPENSSL_PSK::Ticket *ticket = OPENSSL_PSK::Instance().GetTicket(type);
        if (ticket != nullptr) {
            int err = ticket->UseSession(ssl, md, identity, identity_len, session);
            if (err == 0 && *session != nullptr) {
                TRACE("Session resumed, identity(%s, %zu, %p)", (char *)*identity, *identity_len, *session);
                pthread_rwlock_unlock(&OPENSSL_PSK::Instance().lock);
                return 1;
            }
        }
    }
    pthread_rwlock_unlock(&OPENSSL_PSK::Instance().lock);
    TRACE("No suiteable session");

    return 1;
}

int OPENSSL_PSK::NewSessionCallback(SSL *ssl, SSL_SESSION *session)
{
    const char *session_file = nullptr;
    if (session_file != nullptr) {
        BIO *stmp = BIO_new_file(session_file, "w");

        if (stmp == nullptr) {
            TRACE("Error writing session file %s", session_file);
        } else {
            PEM_write_bio_SSL_SESSION(stmp, session);
            BIO_free(stmp);
        }
    }

    /**
     * Session data gets dumped on connection for TLSv1.2 and below, and on
     * arrival of the NewSessionTicket for TLSv1.3.
     */
    if (SSL_version(ssl) == TLS1_3_VERSION) {
        unsigned char out[SSL_MAX_MASTER_KEY_LENGTH];
        size_t outlen = SSL_SESSION_get_master_key(session, out, sizeof(out));

        unsigned char ciphersuite[4], *ciphersuite_ = nullptr;
        const SSL_CIPHER *cipher = SSL_SESSION_get0_cipher(session);
        if (cipher != nullptr) {
            uint32_t id = SSL_CIPHER_get_id(cipher);
            ciphersuite[0] = (unsigned char)(id >> 24);
            ciphersuite[1] = (unsigned char)((id >> 16) & 0xffL);
            ciphersuite[2] = (unsigned char)((id >> 8) & 0xffL);
            ciphersuite[3] = (unsigned char)(id & 0xffL);
            if ((id & 0xff000000L) == 0x03000000L) {
                ciphersuite_ = ciphersuite + 2; // SSL3 cipher
            }
        }
        int type = OPENSSL_PSK::Instance().HasTicket(1) ? 2 : 1;
        OPENSSL_PSK::Instance().SetTicket(type, out, outlen, ciphersuite_);
        TRACE("SaveTicket(%d): [0x%X, 0x%X, 0x%X, 0x%X]", type, ciphersuite[0], ciphersuite[1], ciphersuite[2],
              ciphersuite[3]);

        BIO *bio_out = BIO_new_fd(STDOUT_FILENO, BIO_NOCLOSE);
        if (bio_out != nullptr) {
            BIO_printf(bio_out, "---\nPost-Handshake New Session Ticket arrived:\n");
            SSL_SESSION_print(bio_out, session);
            BIO_printf(bio_out, "---\n");
            BIO_free(bio_out);
        }
    }

    /**
     * We always return a "fail" response so that the session gets freed again
     * because we haven't used the reference.
     */
    return 0;
}

int OPENSSL_PSK::SetTicket(int type, const unsigned char *key, size_t key_len, const unsigned char *ciphersuite)
{
    pthread_rwlock_wrlock(&lock);
    auto ticket = GetTicket(type);
    auto identity = GetIdentity(type);
    if (ticket != nullptr) {
        ticket->SetKey(key, key_len, ciphersuite);
    } else {
        tickets[type] = new Ticket(identity.data(), identity.size(), key, key_len, ciphersuite);
    }
    pthread_rwlock_unlock(&lock);

    return 0;
}

OPENSSL_PSK::OPENSSL_PSK() : lock(PTHREAD_RWLOCK_INITIALIZER) {}
OPENSSL_PSK::~OPENSSL_PSK()
{
    pthread_rwlock_destroy(&lock);
}


/* PSK Ticket and Session */
OPENSSL_PSK::Ticket::Ticket() : version(0), psk_session(nullptr), life_time(600) {}

OPENSSL_PSK::Ticket::Ticket(const unsigned char *identity, size_t identity_len, const unsigned char *key,
                            size_t key_len, const unsigned char *ciphersuite)
    : version(0), psk_session(nullptr), life_time(600)
{
    psk_key = std::basic_string<unsigned char>(key, key_len);
    psk_identity = std::basic_string<unsigned char>(identity, identity_len);
    if (ciphersuite == nullptr) {
        ciphersuite = tls13_aes128gcmsha256_id;
    }
    psk_ciphersuite = std::basic_string<unsigned char>(ciphersuite, psk_ciphersuite_len);
}

bool OPENSSL_PSK::Ticket::Is(const unsigned char *identity, size_t identity_len) const
{
    if (identity != nullptr && identity_len != 0) {
        if (psk_identity.size() == identity_len && memcmp(psk_identity.data(), identity, identity_len) == 0) {
            return true;
        }
    }

    return false;
}

void OPENSSL_PSK::Ticket::SetLifeTime(unsigned int seconds)
{
    life_time = seconds;
}

bool OPENSSL_PSK::Ticket::IsExpired() const
{
    return false; // FIXME
    auto now = time(nullptr);
    return (expired_time < now) || ((expired_time - life_time) > now);
}

void OPENSSL_PSK::Ticket::SetKey(const unsigned char *key, size_t key_len, const unsigned char *ciphersuite)
{
    if (psk_session != nullptr) {
        SSL_SESSION_free(psk_session);
        psk_session = nullptr;
    }
    psk_key = std::basic_string<unsigned char>(key, key_len);
    if (ciphersuite != nullptr) {
        psk_ciphersuite = std::basic_string<unsigned char>(ciphersuite, psk_ciphersuite_len);
    }
    expired_time = time(nullptr) + life_time;
}

void OPENSSL_PSK::Ticket::SetSession(SSL_SESSION *session)
{
    if (psk_session != nullptr) {
        SSL_SESSION_free(psk_session);
        psk_session = nullptr;
    }
    if (session != nullptr) {
        SSL_SESSION_up_ref(session);
        psk_session = session;
    }
}

int OPENSSL_PSK::Ticket::GetSession(SSL *ssl, SSL_SESSION **session)
{
    if (IsExpired()) {
        TRACE("expired");
        return Error::expired;
    }

    if (psk_session != nullptr) {
        *session = psk_session;
        SSL_SESSION_up_ref(psk_session);
        return 0;
    }

    const SSL_CIPHER *cipher = SSL_CIPHER_find(ssl, psk_ciphersuite.data());
    if (cipher == nullptr) {
        TRACE("Error finding suitable ciphersuite");
        return Error::not_supported;
    }
    TRACE("[0x%X, 0x%X]", psk_ciphersuite[0], psk_ciphersuite[1]);

    SSL_SESSION *tmpsess = SSL_SESSION_new();
    if (tmpsess == nullptr || !SSL_SESSION_set1_master_key(tmpsess, psk_key.c_str(), psk_key.size())
        || !SSL_SESSION_set_cipher(tmpsess, cipher) || !SSL_SESSION_set_protocol_version(tmpsess, SSL_version(ssl))) {
        TRACE("Invalid");
        return Error::invalid;
    }
    *session = tmpsess;

    return 0;
}

int OPENSSL_PSK::Ticket::UseSession(SSL *ssl, const EVP_MD *md, const unsigned char **identity, size_t *identity_len,
                                    SSL_SESSION **session)
{
    SSL_SESSION *usesess = nullptr;
    const SSL_CIPHER *cipher = nullptr;

    if (psk_session != nullptr) {
        SSL_SESSION_up_ref(psk_session);
        usesess = psk_session;
    } else {
        cipher = SSL_CIPHER_find(ssl, psk_ciphersuite.data());
        if (cipher == nullptr) {
            TRACE("Error finding suitable ciphersuite");
            return 0;
        }

        usesess = SSL_SESSION_new();
        if (usesess == nullptr || !SSL_SESSION_set1_master_key(usesess, psk_key.data(), psk_key.size())
            || !SSL_SESSION_set_cipher(usesess, cipher) || !SSL_SESSION_set_protocol_version(usesess, TLS1_3_VERSION)) {
            SSL_SESSION_free(usesess);
            TRACE("Error intializing session");
            return Error::invalid;
        }
    }

    cipher = SSL_SESSION_get0_cipher(usesess);
    if (cipher == nullptr) {
        SSL_SESSION_free(usesess);
        TRACE("Invalid session");
        return Error::invalid;
    }

    if (md != nullptr && SSL_CIPHER_get_handshake_digest(cipher) != md) {
        /* PSK not usable, ignore it */
        *identity = nullptr;
        *identity_len = 0;
        *session = nullptr;
        SSL_SESSION_free(usesess);
        TRACE("PSK not useable, ignore it");
    } else {
        *session = usesess;
        *identity = psk_identity.data();
        *identity_len = psk_identity.size();
    }

    return 0;
}


// debug code
long bio_dump_callback(BIO *bio, int cmd, const char *argp, int argi, long argl, long ret)
{
    BIO *out;

    out = (BIO *)BIO_get_callback_arg(bio);
    if (out == NULL) return ret;

    if (cmd == (BIO_CB_READ | BIO_CB_RETURN)) {
        BIO_printf(out, "read from %p [%p] (%lu bytes => %ld (0x%lX))\n", (void *)bio, (void *)argp,
                   (unsigned long)argi, ret, ret);
        BIO_dump(out, argp, (int)ret);
        return ret;
    } else if (cmd == (BIO_CB_WRITE | BIO_CB_RETURN)) {
        BIO_printf(out, "write to %p [%p] (%lu bytes => %ld (0x%lX))\n", (void *)bio, (void *)argp, (unsigned long)argi,
                   ret, ret);
        BIO_dump(out, argp, (int)ret);
    }
    return ret;
}

#include <openssl/x509.h>

static void print_raw_cipherlist(BIO *bio, SSL *s)
{
    const unsigned char *rlist;
    static const unsigned char scsv_id[] = {0, 0xFF};
    size_t i, rlistlen, num;
    if (!SSL_is_server(s)) return;
    num = SSL_get0_raw_cipherlist(s, NULL);
    OPENSSL_assert(num == 2);
    rlistlen = SSL_get0_raw_cipherlist(s, &rlist);
    BIO_puts(bio, "Client cipher list: ");
    for (i = 0; i < rlistlen; i += num, rlist += num) {
        const SSL_CIPHER *c = SSL_CIPHER_find(s, rlist);
        if (i) BIO_puts(bio, ":");
        if (c != NULL) {
            BIO_puts(bio, SSL_CIPHER_get_name(c));
        } else if (memcmp(rlist, scsv_id, num) == 0) {
            BIO_puts(bio, "SCSV");
        } else {
            size_t j;
            BIO_puts(bio, "0x");
            for (j = 0; j < num; j++) BIO_printf(bio, "%02X", rlist[j]);
        }
    }
    BIO_puts(bio, "\n");
}

static const char *get_sigtype(int nid)
{
    switch (nid) {
        case EVP_PKEY_RSA:
            return "RSA";

        case EVP_PKEY_RSA_PSS:
            return "RSA-PSS";

        case EVP_PKEY_DSA:
            return "DSA";

        case EVP_PKEY_EC:
            return "ECDSA";

        case NID_ED25519:
            return "Ed25519";

        case NID_ED448:
            return "Ed448";

        case NID_id_GostR3410_2001:
            return "gost2001";

        case NID_id_GostR3410_2012_256:
            return "gost2012_256";

        case NID_id_GostR3410_2012_512:
            return "gost2012_512";

        default:
            return NULL;
    }
}

static int do_print_sigalgs(BIO *bio, SSL *s, int shared)
{
    int i, nsig, client;
    client = SSL_is_server(s) ? 0 : 1;
    if (shared)
        nsig = SSL_get_shared_sigalgs(s, 0, NULL, NULL, NULL, NULL, NULL);
    else
        nsig = SSL_get_sigalgs(s, -1, NULL, NULL, NULL, NULL, NULL);
    if (nsig == 0) return 1;

    if (shared) BIO_puts(bio, "Shared ");

    if (client) BIO_puts(bio, "Requested ");
    BIO_puts(bio, "Signature Algorithms: ");
    for (i = 0; i < nsig; i++) {
        int hash_nid, sign_nid;
        unsigned char rhash, rsign;
        const char *sstr = NULL;
        if (shared)
            SSL_get_shared_sigalgs(s, i, &sign_nid, &hash_nid, NULL, &rsign, &rhash);
        else
            SSL_get_sigalgs(s, i, &sign_nid, &hash_nid, NULL, &rsign, &rhash);
        if (i) BIO_puts(bio, ":");
        sstr = get_sigtype(sign_nid);
        if (sstr)
            BIO_printf(bio, "%s", sstr);
        else
            BIO_printf(bio, "0x%02X", (int)rsign);
        if (hash_nid != NID_undef)
            BIO_printf(bio, "+%s", OBJ_nid2sn(hash_nid));
        else if (sstr == NULL)
            BIO_printf(bio, "+0x%02X", (int)rhash);
    }
    BIO_puts(bio, "\n");
    return 1;
}

typedef struct string_int_pair_st {
    const char *name;
    int retval;
} OPT_PAIR, STRINT_PAIR;

static const char *lookup(int val, const STRINT_PAIR *list, const char *def)
{
    for (; list->name; ++list)
        if (list->retval == val) return list->name;
    return def;
}

void *app_malloc(int sz, const char *what)
{
    void *vp = OPENSSL_malloc(sz);

    if (vp == NULL) {
        // BIO_printf(bio_err, "%s: Could not allocate %d bytes for %s\n", opt_getprog(), sz, what);
        // ERR_print_errors(bio_err);
        exit(1);
    }
    return vp;
}


static void ssl_print_client_cert_types(BIO *bio, SSL *s)
{
    static STRINT_PAIR cert_type_list[] = {{"RSA sign", TLS_CT_RSA_SIGN},
                                           {"DSA sign", TLS_CT_DSS_SIGN},
                                           {"RSA fixed DH", TLS_CT_RSA_FIXED_DH},
                                           {"DSS fixed DH", TLS_CT_DSS_FIXED_DH},
                                           {"ECDSA sign", TLS_CT_ECDSA_SIGN},
                                           {"RSA fixed ECDH", TLS_CT_RSA_FIXED_ECDH},
                                           {"ECDSA fixed ECDH", TLS_CT_ECDSA_FIXED_ECDH},
                                           {"GOST01 Sign", TLS_CT_GOST01_SIGN},
                                           {"GOST12 Sign", TLS_CT_GOST12_SIGN},
                                           {NULL}};

    const unsigned char *p;
    int i;
    int cert_type_num = SSL_get0_certificate_types(s, &p);
    if (!cert_type_num) return;
    BIO_puts(bio, "Client Certificate Types: ");
    for (i = 0; i < cert_type_num; i++) {
        unsigned char cert_type = p[i];
        const char *cname = lookup((int)cert_type, cert_type_list, NULL);

        if (i) BIO_puts(bio, ", ");
        if (cname != NULL)
            BIO_puts(bio, cname);
        else
            BIO_printf(bio, "UNKNOWN (%d),", cert_type);
    }
    BIO_puts(bio, "\n");
}

int ssl_print_sigalgs(BIO *bio, SSL *s)
{
    int nid;
    if (!SSL_is_server(s)) ssl_print_client_cert_types(bio, s);
    do_print_sigalgs(bio, s, 0);
    do_print_sigalgs(bio, s, 1);
    if (SSL_get_peer_signature_nid(s, &nid) && nid != NID_undef)
        BIO_printf(bio, "Peer signing digest: %s\n", OBJ_nid2sn(nid));
    if (SSL_get_peer_signature_type_nid(s, &nid)) BIO_printf(bio, "Peer signature type: %s\n", get_sigtype(nid));
    return 1;
}

/*
 * Hex encoder for TLSA RRdata, not ':' delimited.
 */
static char *hexencode(const unsigned char *data, size_t len)
{
    static const char *hex = "0123456789abcdef";
    char *out;
    char *cp;
    size_t outlen = 2 * len + 1;
    int ilen = (int)outlen;

    if (outlen < len || ilen < 0 || outlen != (size_t)ilen) {
        // BIO_printf(bio_err, "%s: %zu-byte buffer too large to hexencode\n", opt_getprog(), len);
        exit(1);
    }
    cp = out = (char *)app_malloc(ilen, "TLSA hex data buffer");

    while (len-- > 0) {
        *cp++ = hex[(*data >> 4) & 0x0f];
        *cp++ = hex[*data++ & 0x0f];
    }
    *cp = '\0';
    return out;
}

void print_verify_detail(SSL *s, BIO *bio)
{
    int mdpth;
    EVP_PKEY *mspki;
    long verify_err = SSL_get_verify_result(s);

    if (verify_err == X509_V_OK) {
        const char *peername = SSL_get0_peername(s);

        BIO_printf(bio, "Verification: OK\n");
        if (peername != NULL) BIO_printf(bio, "Verified peername: %s\n", peername);
    } else {
        const char *reason = X509_verify_cert_error_string(verify_err);

        BIO_printf(bio, "Verification error: %s\n", reason);
    }

    if ((mdpth = SSL_get0_dane_authority(s, NULL, &mspki)) >= 0) {
        uint8_t usage, selector, mtype;
        const unsigned char *data = NULL;
        size_t dlen = 0;
        char *hexdata;

        mdpth = SSL_get0_dane_tlsa(s, &usage, &selector, &mtype, &data, &dlen);

        /*
         * The TLSA data field can be quite long when it is a certificate,
         * public key or even a SHA2-512 digest.  Because the initial octets of
         * ASN.1 certificates and public keys contain mostly boilerplate OIDs
         * and lengths, we show the last 12 bytes of the data instead, as these
         * are more likely to distinguish distinct TLSA records.
         */
#define TLSA_TAIL_SIZE 12
        if (dlen > TLSA_TAIL_SIZE)
            hexdata = hexencode(data + dlen - TLSA_TAIL_SIZE, TLSA_TAIL_SIZE);
        else
            hexdata = hexencode(data, dlen);
        BIO_printf(bio, "DANE TLSA %d %d %d %s%s %s at depth %d\n", usage, selector, mtype,
                   (dlen > TLSA_TAIL_SIZE) ? "..." : "", hexdata,
                   (mspki != NULL) ? "signed the certificate"
                   : mdpth         ? "matched TA certificate"
                                   : "matched EE certificate",
                   mdpth);
        OPENSSL_free(hexdata);
    }
}

#ifndef OPENSSL_NO_EC
int ssl_print_point_formats(BIO *out, SSL *s)
{
    int i, nformats;
    const char *pformats;
    nformats = SSL_get0_ec_point_formats(s, &pformats);
    if (nformats <= 0) return 1;
    BIO_puts(out, "Supported Elliptic Curve Point Formats: ");
    for (i = 0; i < nformats; i++, pformats++) {
        if (i) BIO_puts(out, ":");
        switch (*pformats) {
            case TLSEXT_ECPOINTFORMAT_uncompressed:
                BIO_puts(out, "uncompressed");
                break;

            case TLSEXT_ECPOINTFORMAT_ansiX962_compressed_prime:
                BIO_puts(out, "ansiX962_compressed_prime");
                break;

            case TLSEXT_ECPOINTFORMAT_ansiX962_compressed_char2:
                BIO_puts(out, "ansiX962_compressed_char2");
                break;

            default:
                BIO_printf(out, "unknown(%d)", (int)*pformats);
                break;
        }
    }
    BIO_puts(out, "\n");
    return 1;
}

int ssl_print_groups(BIO *bio, SSL *s, int noshared)
{
    int i, ngroups, *groups, nid;
    const char *gname;

    ngroups = SSL_get1_groups(s, NULL);
    if (ngroups <= 0) return 1;
    groups = (int *)app_malloc(ngroups * sizeof(int), "groups to print");
    SSL_get1_groups(s, groups);

    BIO_puts(bio, "Supported Elliptic Groups: ");
    for (i = 0; i < ngroups; i++) {
        if (i) BIO_puts(bio, ":");
        nid = groups[i];
        /* If unrecognised print bio hex version */
        if (nid & TLSEXT_nid_unknown) {
            BIO_printf(bio, "0x%04X", nid & 0xFFFF);
        } else {
            /* TODO(TLS1.3): Get group name here */
            /* Use NIST name for curve if it exists */
            gname = EC_curve_nid2nist(nid);
            if (gname == NULL) gname = OBJ_nid2sn(nid);
            BIO_printf(bio, "%s", gname);
        }
    }
    OPENSSL_free(groups);
    if (noshared) {
        BIO_puts(bio, "\n");
        return 1;
    }
    BIO_puts(bio, "\nShared Elliptic groups: ");
    ngroups = SSL_get_shared_group(s, -1);
    for (i = 0; i < ngroups; i++) {
        if (i) BIO_puts(bio, ":");
        nid = SSL_get_shared_group(s, i);
        /* TODO(TLS1.3): Convert for DH groups */
        gname = EC_curve_nid2nist(nid);
        if (gname == NULL) gname = OBJ_nid2sn(nid);
        BIO_printf(bio, "%s", gname);
    }
    if (ngroups == 0) BIO_puts(bio, "NONE");
    BIO_puts(bio, "\n");
    return 1;
}
#endif

int ssl_print_tmp_key(BIO *out, SSL *s)
{
    EVP_PKEY *key;

    if (!SSL_get_peer_tmp_key(s, &key)) return 1;
    BIO_puts(out, "Server Temp Key: ");
    switch (EVP_PKEY_id(key)) {
        case EVP_PKEY_RSA:
            BIO_printf(out, "RSA, %d bits\n", EVP_PKEY_bits(key));
            break;

        case EVP_PKEY_DH:
            BIO_printf(out, "DH, %d bits\n", EVP_PKEY_bits(key));
            break;
#ifndef OPENSSL_NO_EC
        case EVP_PKEY_EC: {
            EC_KEY *ec = EVP_PKEY_get1_EC_KEY(key);
            int nid;
            const char *cname;
            nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec));
            EC_KEY_free(ec);
            cname = EC_curve_nid2nist(nid);
            if (cname == NULL) cname = OBJ_nid2sn(nid);
            BIO_printf(out, "ECDH, %s, %d bits\n", cname, EVP_PKEY_bits(key));
        } break;
#endif
        default:
            BIO_printf(out, "%s, %d bits\n", OBJ_nid2sn(EVP_PKEY_id(key)), EVP_PKEY_bits(key));
    }
    EVP_PKEY_free(key);
    return 1;
}


void PrintSummary(BIO *bio, SSL *s)
{
    BIO_printf(bio, "Protocol version: %s\n", SSL_get_version(s));
    print_raw_cipherlist(bio, s);
    const SSL_CIPHER *cipher = SSL_get_current_cipher(s);
    BIO_printf(bio, "Ciphersuite: %s\n", SSL_CIPHER_get_name(cipher));
    do_print_sigalgs(bio, s, 0);
    X509 *peer = SSL_get_peer_certificate(s);
    if (peer != NULL) {
        int nid;

        BIO_puts(bio, "Peer certificate: ");
        X509_NAME_print_ex(bio, X509_get_subject_name(peer), 0, XN_FLAG_ONELINE);
        BIO_puts(bio, "\n");
        if (SSL_get_peer_signature_nid(s, &nid)) BIO_printf(bio, "Hash used: %s\n", OBJ_nid2sn(nid));
        if (SSL_get_peer_signature_type_nid(s, &nid)) BIO_printf(bio, "Signature type: %s\n", get_sigtype(nid));
        print_verify_detail(s, bio);
    } else {
        BIO_puts(bio, "No peer certificate\n");
    }
    X509_free(peer);
#ifndef OPENSSL_NO_EC
    ssl_print_point_formats(bio, s);
    if (SSL_is_server(s))
        ssl_print_groups(bio, s, 1);
    else
        ssl_print_tmp_key(bio, s);
#else
    if (!SSL_is_server(s)) ssl_print_tmp_key(bio, s);
#endif
}

bool s_brief = true;

void print_name(BIO *out, const char *title, X509_NAME *nm, unsigned long lflags)
{
    char *buf;
    char mline = 0;
    int indent = 0;

    if (title) BIO_puts(out, title);
    if ((lflags & XN_FLAG_SEP_MASK) == XN_FLAG_SEP_MULTILINE) {
        mline = 1;
        indent = 4;
    }
    if (lflags == XN_FLAG_COMPAT) {
        buf = X509_NAME_oneline(nm, 0, 0);
        BIO_puts(out, buf);
        BIO_puts(out, "\n");
        OPENSSL_free(buf);
    } else {
        if (mline) BIO_puts(out, "\n");
        X509_NAME_print_ex(out, nm, indent, lflags);
        BIO_puts(out, "\n");
    }
}

int dump_cert_text(BIO *out, X509 *x)
{
    print_name(out, "subject=", X509_get_subject_name(x), XN_FLAG_ONELINE);
    BIO_puts(out, "\n");
    print_name(out, "issuer=", X509_get_issuer_name(x), XN_FLAG_ONELINE);
    BIO_puts(out, "\n");

    return 0;
}

void print_ca_names(BIO *bio, SSL *s)
{
    const char *cs = SSL_is_server(s) ? "server" : "client";
    const STACK_OF(X509_NAME) *sk = SSL_get0_peer_CA_list(s);
    int i;

    if (sk == NULL || sk_X509_NAME_num(sk) == 0) {
        if (!SSL_is_server(s)) BIO_printf(bio, "---\nNo %s certificate CA names sent\n", cs);
        return;
    }

    BIO_printf(bio, "---\nAcceptable %s certificate CA names\n", cs);
    for (i = 0; i < sk_X509_NAME_num(sk); i++) {
        X509_NAME_print_ex(bio, sk_X509_NAME_value(sk, i), 0, XN_FLAG_ONELINE);
        BIO_write(bio, "\n", 1);
    }
}

void PrintConnection(BIO *bio, SSL *con)
{
    const char *str;
    X509 *peer;
    char buf[BUFSIZ];
#if !defined(OPENSSL_NO_NEXTPROTONEG)
    const unsigned char *next_proto_neg;
    unsigned next_proto_neg_len;
#endif
    unsigned char *exportedkeymat;
    int i;

    if (s_brief) PrintSummary(bio, con);

    PEM_write_bio_SSL_SESSION(bio, SSL_get_session(con));

    peer = SSL_get_peer_certificate(con);
    if (peer != NULL) {
        BIO_printf(bio, "Client certificate\n");
        PEM_write_bio_X509(bio, peer);
        dump_cert_text(bio, peer);
        X509_free(peer);
        peer = NULL;
    }

    if (SSL_get_shared_ciphers(con, buf, sizeof(buf)) != NULL) BIO_printf(bio, "Shared ciphers:%s\n", buf);
    str = SSL_CIPHER_get_name(SSL_get_current_cipher(con));
    ssl_print_sigalgs(bio, con);
#ifndef OPENSSL_NO_EC
    ssl_print_point_formats(bio, con);
    ssl_print_groups(bio, con, 0);
#endif
    print_ca_names(bio, con);
    BIO_printf(bio, "CIPHER is %s\n", (str != NULL) ? str : "(NONE)");

#if !defined(OPENSSL_NO_NEXTPROTONEG)
    SSL_get0_next_proto_negotiated(con, &next_proto_neg, &next_proto_neg_len);
    if (next_proto_neg) {
        BIO_printf(bio, "NEXTPROTO is ");
        BIO_write(bio, next_proto_neg, next_proto_neg_len);
        BIO_printf(bio, "\n");
    }
#endif
#ifndef OPENSSL_NO_SRTP
    {
        SRTP_PROTECTION_PROFILE *srtp_profile = SSL_get_selected_srtp_profile(con);

        if (srtp_profile) BIO_printf(bio, "SRTP Extension negotiated, profile=%s\n", srtp_profile->name);
    }
#endif
    if (SSL_session_reused(con)) BIO_printf(bio, "Reused session-id\n");
    BIO_printf(bio, "Secure Renegotiation IS%s supported\n", SSL_get_secure_renegotiation_support(con) ? "" : " NOT");
    if ((SSL_get_options(con) & SSL_OP_NO_RENEGOTIATION)) BIO_printf(bio, "Renegotiation is DISABLED\n");

#if 0
    if (keymatexportlabel != NULL) {
        BIO_printf(bio, "Keying material exporter:\n");
        BIO_printf(bio, "    Label: '%s'\n", keymatexportlabel);
        BIO_printf(bio, "    Length: %i bytes\n", keymatexportlen);
        exportedkeymat = (unsigned char *)app_malloc(keymatexportlen, "export key");
        if (!SSL_export_keying_material(con, exportedkeymat, keymatexportlen, keymatexportlabel,
                                        strlen(keymatexportlabel), NULL, 0, 0)) {
            BIO_printf(bio, "    Error\n");
        } else {
            BIO_printf(bio, "    Keying material: ");
            for (i = 0; i < keymatexportlen; i++) BIO_printf(bio, "%02X", exportedkeymat[i]);
            BIO_printf(bio, "\n");
        }
        OPENSSL_free(exportedkeymat);
    }
#endif

    (void)BIO_flush(bio);
}

void print_stuff(BIO *bio, SSL *s, int full)
{
    X509 *peer = NULL;
    STACK_OF(X509) *sk;
    const SSL_CIPHER *c;
    int i, istls13 = (SSL_version(s) == TLS1_3_VERSION);
    long verify_result;
#ifndef OPENSSL_NO_COMP
    const COMP_METHOD *comp, *expansion;
#endif
    unsigned char *exportedkeymat;
#ifndef OPENSSL_NO_CT
    const SSL_CTX *ctx = SSL_get_SSL_CTX(s);
#endif

    if (full) {
        int got_a_chain = 0;

        sk = SSL_get_peer_cert_chain(s);
        if (sk != NULL) {
            got_a_chain = 1;

            BIO_printf(bio, "---\nCertificate chain\n");
            for (i = 0; i < sk_X509_num(sk); i++) {
                BIO_printf(bio, "%2d s:", i);
                X509_NAME_print_ex(bio, X509_get_subject_name(sk_X509_value(sk, i)), 0, XN_FLAG_ONELINE);
                BIO_puts(bio, "\n");
                BIO_printf(bio, "   i:");
                X509_NAME_print_ex(bio, X509_get_issuer_name(sk_X509_value(sk, i)), 0, XN_FLAG_ONELINE);
                BIO_puts(bio, "\n");
                // showcerts PEM_write_bio_X509(bio, sk_X509_value(sk, i));
            }
        }

        BIO_printf(bio, "---\n");
        peer = SSL_get_peer_certificate(s);
        if (peer != NULL) {
            BIO_printf(bio, "Server certificate\n");

            /* Redundant if we showed the whole chain */
            // XXX if (!(c_showcerts && got_a_chain)) PEM_write_bio_X509(bio, peer);
            dump_cert_text(bio, peer);
        } else {
            BIO_printf(bio, "no peer certificate available\n");
        }
        print_ca_names(bio, s);

        ssl_print_sigalgs(bio, s);
        ssl_print_tmp_key(bio, s);

#ifndef OPENSSL_NO_CT
        /*
         * When the SSL session is anonymous, or resumed via an abbreviated
         * handshake, no SCTs are provided as part of the handshake.  While in
         * a resumed session SCTs may be present in the session's certificate,
         * no callbacks are invoked to revalidate these, and in any case that
         * set of SCTs may be incomplete.  Thus it makes little sense to
         * attempt to display SCTs from a resumed session's certificate, and of
         * course none are associated with an anonymous peer.
         */
        if (peer != NULL && !SSL_session_reused(s) && SSL_ct_is_enabled(s)) {
            const STACK_OF(SCT) *scts = SSL_get0_peer_scts(s);
            int sct_count = scts != NULL ? sk_SCT_num(scts) : 0;

            BIO_printf(bio, "---\nSCTs present (%i)\n", sct_count);
            if (sct_count > 0) {
                const CTLOG_STORE *log_store = SSL_CTX_get0_ctlog_store(ctx);

                BIO_printf(bio, "---\n");
                for (i = 0; i < sct_count; ++i) {
                    SCT *sct = sk_SCT_value(scts, i);

                    BIO_printf(bio, "SCT validation status: %s\n", SCT_validation_status_string(sct));
                    SCT_print(sct, bio, 0, log_store);
                    if (i < sct_count - 1) BIO_printf(bio, "\n---\n");
                }
                BIO_printf(bio, "\n");
            }
        }
#endif

        BIO_printf(bio,
                   "---\nSSL handshake has read %ju bytes "
                   "and written %ju bytes\n",
                   BIO_number_read(SSL_get_rbio(s)), BIO_number_written(SSL_get_wbio(s)));
    }
    print_verify_detail(s, bio);
    BIO_printf(bio, (SSL_session_reused(s) ? "---\nReused, " : "---\nNew, "));
    c = SSL_get_current_cipher(s);
    BIO_printf(bio, "%s, Cipher is %s\n", SSL_CIPHER_get_version(c), SSL_CIPHER_get_name(c));
    if (peer != NULL) {
        EVP_PKEY *pktmp;

        pktmp = X509_get0_pubkey(peer);
        BIO_printf(bio, "Server public key is %d bit\n", EVP_PKEY_bits(pktmp));
    }
    BIO_printf(bio, "Secure Renegotiation IS%s supported\n", SSL_get_secure_renegotiation_support(s) ? "" : " NOT");
#ifndef OPENSSL_NO_COMP
    comp = SSL_get_current_compression(s);
    expansion = SSL_get_current_expansion(s);
    BIO_printf(bio, "Compression: %s\n", comp ? SSL_COMP_get_name(comp) : "NONE");
    BIO_printf(bio, "Expansion: %s\n", expansion ? SSL_COMP_get_name(expansion) : "NONE");
#endif

#ifdef SSL_DEBUG
    {
        /* Print out local port of connection: useful for debugging */
        int sock;
        union BIO_sock_info_u info;

        sock = SSL_get_fd(s);
        if ((info.addr = BIO_ADDR_new()) != NULL && BIO_sock_info(sock, BIO_SOCK_INFO_ADDRESS, &info)) {
            BIO_printf(bio_c_out, "LOCAL PORT is %u\n", ntohs(BIO_ADDR_rawport(info.addr)));
        }
        BIO_ADDR_free(info.addr);
    }
#endif

#if 0
#if !defined(OPENSSL_NO_NEXTPROTONEG)
    if (next_proto.status != -1) {
        const unsigned char *proto;
        unsigned int proto_len;
        SSL_get0_next_proto_negotiated(s, &proto, &proto_len);
        BIO_printf(bio, "Next protocol: (%d) ", next_proto.status);
        BIO_write(bio, proto, proto_len);
        BIO_write(bio, "\n", 1);
    }
#endif
#endif

    {
        const unsigned char *proto;
        unsigned int proto_len;
        SSL_get0_alpn_selected(s, &proto, &proto_len);
        if (proto_len > 0) {
            BIO_printf(bio, "ALPN protocol: ");
            BIO_write(bio, proto, proto_len);
            BIO_write(bio, "\n", 1);
        } else
            BIO_printf(bio, "No ALPN negotiated\n");
    }

#ifndef OPENSSL_NO_SRTP
    {
        SRTP_PROTECTION_PROFILE *srtp_profile = SSL_get_selected_srtp_profile(s);

        if (srtp_profile) BIO_printf(bio, "SRTP Extension negotiated, profile=%s\n", srtp_profile->name);
    }
#endif

    if (istls13) {
        switch (SSL_get_early_data_status(s)) {
            case SSL_EARLY_DATA_NOT_SENT:
                BIO_printf(bio, "Early data was not sent\n");
                break;

            case SSL_EARLY_DATA_REJECTED:
                BIO_printf(bio, "Early data was rejected\n");
                break;

            case SSL_EARLY_DATA_ACCEPTED:
                BIO_printf(bio, "Early data was accepted\n");
                break;
        }

        /*
         * We also print the verify results when we dump session information,
         * but in TLSv1.3 we may not get that right away (or at all) depending
         * on when we get a NewSessionTicket. Therefore we print it now as well.
         */
        verify_result = SSL_get_verify_result(s);
        BIO_printf(bio, "Verify return code: %ld (%s)\n", verify_result, X509_verify_cert_error_string(verify_result));
    } else {
        /* In TLSv1.3 we do this on arrival of a NewSessionTicket */
        SSL_SESSION_print(bio, SSL_get_session(s));
    }

#if 0
    if (SSL_get_session(s) != NULL && keymatexportlabel != NULL) {
        BIO_printf(bio, "Keying material exporter:\n");
        BIO_printf(bio, "    Label: '%s'\n", keymatexportlabel);
        BIO_printf(bio, "    Length: %i bytes\n", keymatexportlen);
        exportedkeymat = app_malloc(keymatexportlen, "export key");
        if (!SSL_export_keying_material(s, exportedkeymat, keymatexportlen, keymatexportlabel,
                                        strlen(keymatexportlabel), NULL, 0, 0)) {
            BIO_printf(bio, "    Error\n");
        } else {
            BIO_printf(bio, "    Keying material: ");
            for (i = 0; i < keymatexportlen; i++) BIO_printf(bio, "%02X", exportedkeymat[i]);
            BIO_printf(bio, "\n");
        }
        OPENSSL_free(exportedkeymat);
    }
    BIO_printf(bio, "---\n");
#endif

    X509_free(peer);
    /* flush, or debugging output gets mixed with http response */
    (void)BIO_flush(bio);
}
