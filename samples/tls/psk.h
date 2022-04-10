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

#pragma once

#include <map>
#include <string>
#include <vector>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>

class OPENSSL_PSK {
  public:
    class Ticket {
      public:
        enum Error {
            expired,
            invalid,
            not_found,
            not_matched,
            not_supported,
            not_initialized,
        };

        Ticket();
        Ticket(const unsigned char *identity, size_t identity_len, const unsigned char *key, size_t key_len,
               const unsigned char *ciphersuite);

        bool IsExpired() const;
        bool Is(const unsigned char *identity, size_t identity_len) const;

        void SetKey(const unsigned char *key, size_t key_len, const unsigned char *ciphersuite = nullptr);
        void SetSession(SSL_SESSION *session);

        int GetSession(SSL *ssl, SSL_SESSION **session);
        int UseSession(SSL *ssl, const EVP_MD *md, const unsigned char **identity, size_t *identity_len,
                       SSL_SESSION **session);

        void SetLifeTime(unsigned int seconds);

      private:
        SSL_SESSION *psk_session;
        std::basic_string<unsigned char> psk_key;
        std::basic_string<unsigned char> psk_identity;
        std::basic_string<unsigned char> psk_ciphersuite;

        int version;
        time_t expired_time;
        unsigned int life_time;
    };

    static OPENSSL_PSK &Instance();
    const std::basic_string<unsigned char> &GetIdentity(int type);

    bool HasTicket(int type = -1 /* -1 for all types */);
    Ticket *GetTicket(int type, bool crete_if_not_exists = false);
    int SetTicket(int type, const unsigned char *key, size_t key_len, const unsigned char *ciphersuite = nullptr);

    // For OpenSSL
    // server callback, return 1 if no error even if session not found
    static int FindSessionCallback(SSL *ssl, const unsigned char *identity, size_t identity_len, SSL_SESSION **session);

    // client callback for openssl, return 1 if no error
    static int UseSessionCallback(SSL *ssl, const EVP_MD *md, const unsigned char **identity, size_t *identity_len,
                                  SSL_SESSION **session);

    static int NewSessionCallback(SSL *ssl, SSL_SESSION *session);

  private:
    OPENSSL_PSK();
    ~OPENSSL_PSK();

  public:
    pthread_rwlock_t lock;
    std::map<int, Ticket *> tickets;
};


// debug code
void PrintConnection(BIO *bio, SSL *con);
long bio_dump_callback(BIO *bio, int cmd, const char *argp, int argi, long argl, long ret);
void print_stuff(BIO *bio, SSL *s, int full);
