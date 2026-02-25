// equatix.io-backend-cpp/filters/FirebaseAuthFilter.cc
#include "FirebaseAuthFilter.h"
#include <drogon/HttpClient.h>
#include <jwt-cpp/jwt.h>
#include <mutex>
#include <chrono>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

using namespace drogon;

// Internal cache and mutex with internal linkage
static std::unordered_map<std::string, std::string> firebaseKeys;
static std::mutex keyMutex;

// Utility to extract Public Key from X.509 Certificate
std::string certToPubKey(const std::string &certPem) {
    BIO *bio = BIO_new_mem_buf(certPem.data(), certPem.size());
    if (!bio) throw std::runtime_error("Failed to create BIO");

    X509 *cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!cert) throw std::runtime_error("Failed to parse certificate");

    EVP_PKEY *pkey = X509_get_pubkey(cert);
    X509_free(cert);
    if (!pkey) throw std::runtime_error("Failed to extract public key");

    BIO *pubBio = BIO_new(BIO_s_mem());
    if (PEM_write_bio_PUBKEY(pubBio, pkey) != 1) {
        EVP_PKEY_free(pkey);
        BIO_free(pubBio);
        throw std::runtime_error("Failed to write public key");
    }
    EVP_PKEY_free(pkey);

    char *pubKeyData = nullptr;
    long pubKeyLen = BIO_get_mem_data(pubBio, &pubKeyData);
    std::string pubKey(pubKeyData, pubKeyLen);
    BIO_free(pubBio);
    return pubKey;
}

// Global fetch implementation used by main.cc and internal refresh
void fetchFirebaseKeys() {
    auto client = HttpClient::newHttpClient("https://www.googleapis.com");
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/robot/v1/metadata/x509/securetoken@system.gserviceaccount.com");

    client->sendRequest(req, [](ReqResult result, const HttpResponsePtr &resp) {
        if (result != ReqResult::Ok || !resp) return;
        try {
            auto json = resp->getJsonObject();
            std::lock_guard<std::mutex> lock(keyMutex);
            firebaseKeys.clear();
            for (const auto &key : json->getMemberNames()) {
                firebaseKeys[key] = certToPubKey((*json)[key].asString());
            }
            LOG_INFO << "Firebase public keys updated and cached.";
        } catch (...) {
            LOG_ERROR << "Error processing Firebase keys.";
        }
    });
}

void FirebaseAuthFilter::refreshKeys() {
    fetchFirebaseKeys();
}

void FirebaseAuthFilter::doFilter(const HttpRequestPtr &req,
                                  FilterCallback &&fcb,
                                  FilterChainCallback &&fccb) {
    try {
        auto authHeader = req->getHeader("authorization");
        if (authHeader.size() < 7 || authHeader.substr(0, 7) != "Bearer ") {
            throw std::runtime_error("Invalid format");
        }
        
        std::string token = authHeader.substr(7);
        auto decoded = jwt::decode(token);
        auto kid = decoded.get_header_claim("kid").as_string();

        std::string pubKey;
        {
            std::lock_guard<std::mutex> lock(keyMutex);
            if (firebaseKeys.count(kid)) pubKey = firebaseKeys[kid];
        }

        // Return 401 if key is missing (triggers frontend to retry/login)
        if (pubKey.empty()) {
            fetchFirebaseKeys(); // Async refresh for next time
            auto res = HttpResponse::newHttpResponse();
            res->setStatusCode(k401Unauthorized);
            fcb(res);
            return;
        }

        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::rs256(pubKey, "", "", ""))
            .with_issuer("https://securetoken.google.com/equatix-io")
            .with_audience("equatix-io");

        verifier.verify(decoded);

        // Attach claims to attributes for controllers to use
        req->attributes()->insert("uid", decoded.get_payload_claim("sub").as_string());
        req->attributes()->insert("name", decoded.get_payload_claim("name").as_string());
        req->attributes()->insert("email", decoded.get_payload_claim("email").as_string());
        req->attributes()->insert("photo", decoded.get_payload_claim("picture").as_string());

        fccb();
    } catch (...) {
        auto res = HttpResponse::newHttpResponse();
        res->setStatusCode(k401Unauthorized);
        fcb(res);
    }
}