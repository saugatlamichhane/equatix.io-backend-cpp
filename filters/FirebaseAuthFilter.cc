/**
 *
 *  FirebaseAuthFilter.cc
 *
 */

#include "FirebaseAuthFilter.h"
#include <drogon/HttpClient.h>
#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>
#include <jwt-cpp/jwt.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Result.h>
#include <trantor/utils/Logger.h>

static std::unordered_map<std::string, std::string> firebaseKeys;
static std::mutex keyMutex;

using namespace drogon;

std::string certToPubKey(const std::string &certPem) {
  BIO *bio = BIO_new_mem_buf(certPem.data(), certPem.size());
  if (!bio) {
    throw std::runtime_error("Failed to create BIO");
  }

  X509 *cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  if (!cert) {
    throw std::runtime_error("Failed to parse certificate");
  }

  EVP_PKEY *pkey = X509_get_pubkey(cert);
  X509_free(cert);
  if (!pkey) {
    throw std::runtime_error("Failed to extract public key");
  }

  BIO *pubBio = BIO_new(BIO_s_mem());
  if (!pubBio) {
    EVP_PKEY_free(pkey);
    throw std::runtime_error("Failed to create BIO for public key");
  }

  if (PEM_write_bio_PUBKEY(pubBio, pkey) != 1) {
    EVP_PKEY_free(pkey);
    BIO_free(pubBio);
    throw std::runtime_error("Failed to write public key to BIO");
  }
  EVP_PKEY_free(pkey);

  char *pubKeyData = nullptr;
  long pubKeyLen = BIO_get_mem_data(pubBio, &pubKeyData);
  std::string pubKey(pubKeyData, pubKeyLen);
  BIO_free(pubBio);

  return pubKey;
}

void fetchFirebaseKeys() {
  auto client = drogon::HttpClient::newHttpClient("https://www.googleapis.com");
  auto req = drogon::HttpRequest::newHttpRequest();
  req->setPath("/robot/v1/metadata/x509/securetoken@system.gserviceaccount.com");
  client->sendRequest(
      req, [](drogon::ReqResult result, const drogon::HttpResponsePtr &resp) {
        if (result != drogon::ReqResult::Ok || !resp) {
          LOG_ERROR << "Failed to fetch Firebase keys";
          return;
        }
        try {
          Json::Value jsonResponse;
          Json::CharReaderBuilder builder;
          std::string errs;
          std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
          if (!reader->parse(resp->getBody().data(),
                             resp->getBody().data() + resp->getBody().size(),
                             &jsonResponse, &errs)) {
            LOG_ERROR << "Failed to parse Firebase keys JSON: " << errs;
            return;
          }

          std::lock_guard<std::mutex> lock(keyMutex);
          firebaseKeys.clear();
          for (const auto &key : jsonResponse.getMemberNames()) {
            std::string certPem = jsonResponse[key].asString();
            try {
              std::string pubKey = certToPubKey(certPem);
              firebaseKeys[key] = pubKey;
            } catch (const std::exception &e) {
              LOG_ERROR << "Error converting cert to public key for key " << key
                        << ": " << e.what();
            }
          }
          LOG_INFO << "Successfully fetched and converted Firebase keys";
        } catch (const std::exception &e) {
          LOG_ERROR << "Exception while processing Firebase keys: " << e.what();
        }
      });
}

bool verifyFirebaseToken(const std::string &token) {
  try {
    auto decoded = jwt::decode(token);
    auto kid = decoded.get_header_claim("kid").as_string();
    std::string pubKey;
    {
      std::lock_guard<std::mutex> lock(keyMutex);
      if (firebaseKeys.find(kid) == firebaseKeys.end()) {
        LOG_ERROR << "No public key found for kid: " << kid;
        return false;
      }
      pubKey = firebaseKeys[kid];
    }
    if (pubKey.empty()) {
      fetchFirebaseKeys();
      std::lock_guard<std::mutex> lock(keyMutex);
      if (firebaseKeys.find(kid) == firebaseKeys.end()) {
        LOG_ERROR << "No public key found for kid after refresh: " << kid;
        return false;
      }
    }
    pubKey = firebaseKeys[kid];
    auto verifier =
        jwt::verify()
            .allow_algorithm(jwt::algorithm::rs256(pubKey, "", "", ""))
            .with_issuer("https://securetoken.google.com/equatix-io")
            .with_audience("equatix-io");
    verifier.verify(decoded);
    LOG_INFO << "Token verified successfully for kid: " << kid;
    return true;
  } catch (const std::exception &e) {
    LOG_ERROR << "Failed to decode token: " << e.what();
    return false;
  }
}

void FirebaseAuthFilter::doFilter(const HttpRequestPtr &req,
                                  FilterCallback &&fcb,
                                  FilterChainCallback &&fccb) {
  try {
      fetchFirebaseKeys();
      auto headers = req->getHeaders();
      if(headers.find("authorization") == headers.end()) {
          throw std::runtime_error("Authorization header missing");
      }
      std::string authHeader = headers.at("authorization");
      if(authHeader.substr(0, 7) != "Bearer ") {
          throw std::runtime_error("Invalid authorization header format");
      }
      std::string token = authHeader.substr(7);
      if(!verifyFirebaseToken(token)) {
          throw std::runtime_error("Invalid token");
      }
      auto decoded = jwt::decode(token);
      std::string uid = decoded.get_payload_claim("sub").as_string();
      req->attributes()->insert("uid", uid);
      auto client = drogon::app().getDbClient();
      std::string name = decoded.get_payload_claim("name").as_string();
      std::string email = decoded.get_payload_claim("email").as_string();
      std::string photo = decoded.get_payload_claim("picture").as_string();
      client->execSqlAsync("INSERT INTO users(uid, name, email, photo) VALUES($1, $2, $3, $4) ON CONFLICT (uid) DO NOTHING", [=] (const drogon::orm::Result& r) {
              LOG_INFO << "User upserted.";
      }, 
      [](const drogon::orm::DrogonDbException& e) {
      LOG_DEBUG << "DB Error " << e.base().what();
      },
      uid, name, email, photo);

      
  } catch (const std::exception &e) {
    LOG_ERROR << "Authentication Failed: " << e.what();
    auto res = drogon::HttpResponse::newHttpResponse();
    res->setStatusCode(k500InternalServerError);
    fcb(res);
  }

  fccb();
}
