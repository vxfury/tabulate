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

#include "cxxopt.h"

#include <string>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>

int cli_main(int argc, char *argv[]);
int svr_main(int argc, char *argv[]);

int main(int argc, char **argv)
{
    cxxopt::Dispatcher dispatcher(argv[0]);
    dispatcher.add("client", cli_main);
    dispatcher.add("server", svr_main);

    return dispatcher(argc, argv);
}

static int parse_addr(struct sockaddr_in6 &sockaddr, const char *addr, const char *port)
{
    bzero(&sockaddr, sizeof(sockaddr));
    {
        sockaddr_in *ipv4 = (sockaddr_in *)&sockaddr;
        if (inet_pton(AF_INET, addr, &ipv4->sin_addr) != 1) return -EINVAL;

        ipv4->sin_family = AF_INET;
        ipv4->sin_port = htons(atoi(port));

        return 0;
    }

    {
        if (inet_pton(AF_INET6, addr, &sockaddr.sin6_addr) != 1) return -EINVAL;

        sockaddr.sin6_family = AF_INET6;
        sockaddr.sin6_port = htons(atoi(port));

        return 0;
    }
}

/* TLS - Client */
#include "psk.h"

int cli_main(int argc, char *argv[])
{
    cxxopt::Options options("tls-server", "tls server");
    // clang-format off
    options.add_group()
    ("h,help", "Display this and exit")
    ("psk", "Pre shared key", cxxopt::Value<std::string>(), cxxopt::REQUIRED)
    ("psk-session", "Path to PSK session file", cxxopt::Value<std::string>(), cxxopt::REQUIRED)
    ("ciphersuites", "ciphersuites for TLS 1.3", cxxopt::Value<std::string>()->set_default("TLS_AES_128_GCM_SHA256"), cxxopt::REQUIRED);
    // clang-format on
    auto results = options.parse(argc, argv);
    const char *addr_str = argc >= 1 ? argv[0] : "127.0.0.1";
    const char *port_str = argc >= 2 ? argv[1] : "9999";

    if (results.has('h')) {
        std::cout << options.usage() << std::endl;
        exit(0);
    }

    // tls
    SSL_CTX *ctx = nullptr;
    {
        SSL_library_init();
        SSL_load_error_strings();

        if ((ctx = SSL_CTX_new(TLS_client_method())) == nullptr) {
            ERR_print_errors_fp(stderr);
            exit(1);
        }
    }
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL_STORE);
    SSL_CTX_sess_set_new_cb(ctx, OPENSSL_PSK::NewSessionCallback);

    if (results.has("psk")) {
        long key_len = 0;
        unsigned char *key = OPENSSL_hexstr2buf(results("psk").c_str(), &key_len);
        OPENSSL_PSK::Instance().SetTicket(1, key, key_len, nullptr);
        OPENSSL_free(key);
    }

    if (results.has("psk-session")) {
        std::string sessf = results("psk-session");
        if (access(sessf.c_str(), F_OK) == 0) {
            BIO *stmp = BIO_new_file(sessf.c_str(), "r");

            if (stmp == NULL) {
                printf("Can't open PSK session file %s\n", sessf.c_str());
                ERR_print_errors_fp(stderr);
                exit(1);
            }
            SSL_SESSION *psksess = PEM_read_bio_SSL_SESSION(stmp, NULL, 0, NULL);
            BIO_free(stmp);
            if (psksess == NULL) {
                printf("Can't read PSK session file %s\n", sessf.c_str());
                ERR_print_errors_fp(stderr);
                exit(1);
            } else {
                OPENSSL_PSK::Instance().GetTicket(0, true)->SetSession(psksess);
            }
        }
    }

    if (results.has("ciphersuites")) {
        SSL_CTX_set_ciphersuites(ctx, results("ciphersuites").c_str());
    }

    if (OPENSSL_PSK::Instance().HasTicket()) {
        SSL_CTX_set_psk_use_session_callback(ctx, OPENSSL_PSK::Instance().UseSessionCallback);
    }

    // SSL_CTX_clear_mode(ctx, SSL_MODE_AUTO_RETRY);
    SSL_CTX_sess_set_cache_size(ctx, 128);

    // tcp handshake
    int err, fd = -1;
    {
        if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            perror("socket");
            SSL_CTX_free(ctx);
            exit(1);
        }
        struct sockaddr_in6 svraddr;
        if (parse_addr(svraddr, addr_str, port_str) != 0) {
            perror("parse_addr");
            SSL_CTX_free(ctx);
            close(fd);
            exit(1);
        }

        size_t addrlen = (svraddr.sin6_family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
        if ((err = connect(fd, (struct sockaddr *)&svraddr, addrlen)) != 0) {
            perror("connect");
            SSL_CTX_free(ctx);
            close(fd);
            exit(1);
        }
    }

    // tls handshake
    SSL *ssl = nullptr;
    {
        if ((ssl = SSL_new(ctx)) == nullptr) {
            close(fd);
            SSL_CTX_free(ctx);
            exit(1);
        }

        if (!SSL_set_fd(ssl, fd)) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(fd);
            SSL_CTX_free(ctx);
            exit(1);
        }

        if ((err = SSL_connect(ssl)) != 1) {
            ERR_print_errors_fp(stderr);
            if (err < 0) {
                // not successful, because a fatal error occurred either at the protocol
                // level or a connection failure occurred
                printf("SSL_connect fatal error fail: %d\n", err);
            } else if (0 == err) {
                // not successful but was shut down controlled and by the specifications
                // of the TLS/SSL protocol
                printf("SSL_connect shut down controlled fail: %d\n", err);
            } else {
                printf("SSL_connect unknown error fail: %d\n", err);
            }
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(fd);
            SSL_CTX_free(ctx);
            exit(1);
        }
    }

    {
        BIO *bio = BIO_new_fd(STDOUT_FILENO, BIO_NOCLOSE);
        print_stuff(bio, ssl, 1);
        BIO_free(bio);
    }

    // application
    {
        char buffer[1024 + 1];
        memset(buffer, 0, sizeof(buffer));
        if ((err = SSL_read(ssl, buffer, 1024)) <= 0) {
            printf("SSL_read fail: %d\n", err);
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(fd);
            SSL_CTX_free(ctx);
            exit(1);
        }
        printf("SSL_read success: %s\n", buffer);

        snprintf(buffer, sizeof(buffer), "Hello, I am Alice.");
        if ((err = SSL_write(ssl, buffer, strlen(buffer))) <= 0) {
            printf("SSL_write fail: %d\n", err);
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(fd);
            SSL_CTX_free(ctx);
            exit(1);
        }
        printf("SSL_write success: %s\n", buffer);

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(fd);
    }

    SSL_CTX_free(ctx);

    return 0;
}

/* TLS - Server */
/*
 * By default s_server uses an in-memory cache which caches SSL_SESSION
 * structures without any serialisation. This hides some bugs which only
 * become apparent in deployed servers. By implementing a basic external
 * session cache some issues can be debugged using s_server.
 */

typedef struct simple_ssl_session_st {
    unsigned char *id;
    unsigned int idlen;
    unsigned char *der;
    int derlen;
    struct simple_ssl_session_st *next;
} simple_ssl_session;

static simple_ssl_session *first = NULL;

static int add_session(SSL *ssl, SSL_SESSION *session)
{
    simple_ssl_session *sess = (simple_ssl_session *)malloc(sizeof(*sess));
    unsigned char *p;

    SSL_SESSION_get_id(session, &sess->idlen);
    sess->derlen = i2d_SSL_SESSION(session, NULL);
    if (sess->derlen < 0) {
        TRACE("Error encoding session");
        OPENSSL_free(sess);
        return 0;
    }

    sess->id = (unsigned char *)OPENSSL_memdup(SSL_SESSION_get_id(session, NULL), sess->idlen);
    sess->der = (unsigned char *)malloc(sess->derlen);
    if (!sess->id) {
        TRACE("Out of memory adding to external cache");
        OPENSSL_free(sess->id);
        OPENSSL_free(sess->der);
        OPENSSL_free(sess);
        return 0;
    }
    p = sess->der;

    /* Assume it still works. */
    if (i2d_SSL_SESSION(session, &p) != sess->derlen) {
        TRACE("Unexpected session encoding length");
        OPENSSL_free(sess->id);
        OPENSSL_free(sess->der);
        OPENSSL_free(sess);
        return 0;
    }

    sess->next = first;
    first = sess;
    TRACE("New session added to external cache");
    return 0;
}

static SSL_SESSION *get_session(SSL *ssl, const unsigned char *id, int idlen, int *do_copy)
{
    simple_ssl_session *sess;
    *do_copy = 0;
    for (sess = first; sess; sess = sess->next) {
        if (idlen == (int)sess->idlen && !memcmp(sess->id, id, idlen)) {
            const unsigned char *p = sess->der;
            TRACE("Lookup session: cache hit");
            return d2i_SSL_SESSION(NULL, &p, sess->derlen);
        }
    }
    TRACE("Lookup session: cache miss");
    return NULL;
}

static void del_session(SSL_CTX *sctx, SSL_SESSION *session)
{
    simple_ssl_session *sess, *prev = NULL;
    const unsigned char *id;
    unsigned int idlen;
    id = SSL_SESSION_get_id(session, &idlen);
    for (sess = first; sess; sess = sess->next) {
        if (idlen == sess->idlen && !memcmp(sess->id, id, idlen)) {
            if (prev)
                prev->next = sess->next;
            else
                first = sess->next;
            OPENSSL_free(sess->id);
            OPENSSL_free(sess->der);
            OPENSSL_free(sess);
            return;
        }
        prev = sess;
    }
}

static void init_session_cache_ctx(SSL_CTX *sctx)
{
    SSL_CTX_set_session_cache_mode(sctx, SSL_SESS_CACHE_NO_INTERNAL | SSL_SESS_CACHE_SERVER);
    SSL_CTX_sess_set_new_cb(sctx, add_session);
    SSL_CTX_sess_set_get_cb(sctx, get_session);
    SSL_CTX_sess_set_remove_cb(sctx, del_session);
}

static void free_sessions(void)
{
    simple_ssl_session *sess, *tsess;
    for (sess = first; sess;) {
        OPENSSL_free(sess->id);
        OPENSSL_free(sess->der);
        tsess = sess;
        sess = sess->next;
        OPENSSL_free(tsess);
    }
    first = NULL;
}

#ifndef OPENSSL_NO_PSK
static unsigned int psk_server_cb(SSL *ssl, const char *identity, unsigned char *psk, unsigned int max_psk_len)
{
    if (!SSL_is_dtls(ssl) && SSL_version(ssl) >= TLS1_3_VERSION) {
        /*
         * This callback is designed for use in (D)TLSv1.2 (or below). It is
         * possible to use a single callback for all protocol versions - but it
         * is preferred to use a dedicated callback for TLSv1.3. For TLSv1.3 we
         * have psk_find_session_cb.
         */
        return 0;
    }

    return 0;
}
#endif

#ifndef TLS_CERT_KEY_DIR
#define TLS_CERT_KEY_DIR ""
#endif

int svr_main(int argc, char *argv[])
{
    cxxopt::Options options("tls-server", "tls server");
    // clang-format off
    options.add_group()
    ("h,help", "Display this and exit")
    ("cert", "Path to cert", cxxopt::Value<std::string>()->set_default(std::string(TLS_CERT_KEY_DIR) + "server.crt"), cxxopt::REQUIRED)
    ("key", "Path to private key", cxxopt::Value<std::string>()->set_default(std::string(TLS_CERT_KEY_DIR) + "server.key"), cxxopt::REQUIRED)
    ("psk", "Pre shared key", cxxopt::Value<std::string>(), cxxopt::REQUIRED)
    ("psk-session", "Path to PSK session file", cxxopt::Value<std::string>(), cxxopt::REQUIRED)
    ("ciphersuites", "ciphersuites for TLS 1.3", cxxopt::Value<std::string>(), cxxopt::REQUIRED);
    // clang-format on
    auto results = options.parse(argc, argv);
    const char *addr_str = argc >= 1 ? argv[0] : "0.0.0.0";
    const char *port_str = argc >= 2 ? argv[1] : "9999";

    if (results.has('h')) {
        std::cout << options.usage() << std::endl;
        exit(0);
    }

    // BIO_set_callback(sbio, bio_dump_callback);
    // BIO_set_callback_arg(sbio, (char *)bio_c_out);

    int err;
    int listen_fd = -1;
    // tcp listen
    {
        struct sockaddr_in6 svraddr;
        parse_addr(svraddr, addr_str, port_str);

        if ((listen_fd = socket(svraddr.sin6_family, SOCK_STREAM, IPPROTO_TCP)) < 0) {
            perror("socket");
            exit(1);
        }
        size_t addrlen = (svraddr.sin6_family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
        if ((err = bind(listen_fd, (struct sockaddr *)&svraddr, addrlen)) < 0) {
            perror("bind");
            close(listen_fd);
            exit(1);
        }
        if ((err = listen(listen_fd, 1024)) < 0) {
            perror("listen");
            close(listen_fd);
            exit(1);
        }
    }

    // tls
    SSL_CTX *ctx = nullptr;
    {
        SSL_library_init();
        SSL_load_error_strings();

        if ((ctx = SSL_CTX_new(TLS_server_method())) == nullptr) {
            ERR_print_errors_fp(stderr);
            close(listen_fd);
            exit(1);
        }

        auto certfile = results("cert");
        auto prikeyfile = results("key");
        if (!SSL_CTX_use_certificate_file(ctx, certfile.c_str(), SSL_FILETYPE_PEM)
            || !SSL_CTX_use_PrivateKey_file(ctx, prikeyfile.c_str(), SSL_FILETYPE_PEM)
            || !SSL_CTX_check_private_key(ctx)) {
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(ctx);
            close(listen_fd);
            exit(1);
        }

        if (results.has("psk")) {
            long key_len = 0;
            unsigned char *key = OPENSSL_hexstr2buf(results("psk").c_str(), &key_len);
            OPENSSL_PSK::Instance().SetTicket(1, key, key_len, nullptr);
            OPENSSL_free(key);
        }

        if (results.has("psk-session")) {
            auto file = results("psk-session");
            if (access(file.c_str(), F_OK) == 0) {
                BIO *stmp = BIO_new_file(file.c_str(), "r");
                if (stmp == NULL) {
                    ERR_print_errors_fp(stderr);
                    SSL_CTX_free(ctx);
                    close(listen_fd);
                    exit(1);
                }
                SSL_SESSION *psksess = PEM_read_bio_SSL_SESSION(stmp, NULL, 0, NULL);
                BIO_free(stmp);
                if (psksess == NULL) {
                    printf("Can't read PSK session file %s\n", file.c_str());
                    ERR_print_errors_fp(stderr);
                } else {
                    OPENSSL_PSK::Instance().GetTicket(0, true)->SetSession(psksess);
                }
            }
        }

        if (results.has("ciphersuites")) {
            SSL_CTX_set_ciphersuites(ctx, results("ciphersuites").c_str());
        }

        // psk-session resumepation not supported now
        if (OPENSSL_PSK::Instance().HasTicket() /* || psksess != NULL */) {
            SSL_CTX_set_psk_find_session_callback(ctx, OPENSSL_PSK::Instance().FindSessionCallback);
        }

        // init_session_cache_ctx(ctx);

        // SSL_CTX_clear_mode(ctx, SSL_MODE_AUTO_RETRY);
        SSL_CTX_sess_set_cache_size(ctx, 128);
    }

    while (true) {
        // tcp handshake
        int fd = -1;
        {
            struct sockaddr_in6 cliaddr;
            socklen_t cliaddrlen = sizeof(cliaddr);
            if ((fd = accept(listen_fd, (struct sockaddr *)&cliaddr, &cliaddrlen)) < 0) {
                perror("accept");
                continue;
            }
            {
                char str[INET6_ADDRSTRLEN] = {};
                inet_ntop(cliaddr.sin6_family,
                          cliaddr.sin6_family == AF_INET ? (void *)&((sockaddr_in *)&cliaddr)->sin_addr
                                                         : (void *)&cliaddr.sin6_addr,
                          str, INET6_ADDRSTRLEN);
                printf("connection from %s:%d\n", str, ntohs(cliaddr.sin6_port));
            }
        }

        // tls handshake
        SSL *ssl = nullptr;
        {
            if ((ssl = SSL_new(ctx)) == nullptr) {
                ERR_print_errors_fp(stderr);
                close(fd);
                continue;
            }

            if (!SSL_set_fd(ssl, fd)) {
                SSL_shutdown(ssl);
                SSL_free(ssl);
                close(fd);
                continue;
            }

            if ((err = SSL_accept(ssl)) != 1) {
                ERR_print_errors_fp(stdout);
                if (err < 0) {
                    // not successful because a fatal error occurred either at the protocol
                    // level or a connection failure occurred
                    printf("SSL_accept fatal error: %d\n", err);
                } else if (err == 0) {
                    // not successful but was shut down controlled and by the specifications
                    // of the TLS/SSL protocol
                    ERR_print_errors_fp(stdout);
                    printf("SSL_accept shut down controlled fail: %d, %d\n", err, SSL_get_error(ssl, err));
                } else {
                    printf("SSL_accept unknown error fail: %d\n", err);
                }

                SSL_shutdown(ssl);
                SSL_free(ssl);
                close(fd);
                continue;
            }
        }

        {
            BIO *bio = BIO_new_fd(STDOUT_FILENO, BIO_NOCLOSE);
            PrintConnection(bio, ssl);
            BIO_free(bio);
        }

        // application data
        {
            char buffer[1024 + 1];
            snprintf(buffer, sizeof(buffer), "Hello, I am Bob.");
            if ((err = SSL_write(ssl, buffer, strlen(buffer))) <= 0) {
                printf("SSL_write fail: %d\n", err);
                SSL_shutdown(ssl);
                SSL_free(ssl);
                close(fd);
                continue;
            }
            printf("SSL_write success: %s\n", buffer);

            memset(buffer, 0, sizeof(buffer));
            if ((err = SSL_read(ssl, buffer, 1024)) <= 0) {
                printf("SSL_read fail: %d\n", err);
                SSL_shutdown(ssl);
                SSL_free(ssl);
                close(fd);
                break;
            }
            printf("SSL_read success: %s\n", buffer);
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(fd);
    }

    SSL_CTX_free(ctx);
    close(listen_fd);

    return 0;
}
