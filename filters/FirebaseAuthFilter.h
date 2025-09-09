/**
 *
 *  FirebaseAuthFilter.h
 *
 */

#pragma once

#include <drogon/HttpFilter.h>
using namespace drogon;


static std::unordered_map<std::string, std::string> firebaseKeys;
static std::mutex keyMutex;

std::string certToPubKey(const std::string &certPem);
void fetchFirebaseKeys();

class FirebaseAuthFilter : public HttpFilter<FirebaseAuthFilter>
{
  public:
    FirebaseAuthFilter() {}
    void doFilter(const HttpRequestPtr &req,
                  FilterCallback &&fcb,
                  FilterChainCallback &&fccb) override;
};

