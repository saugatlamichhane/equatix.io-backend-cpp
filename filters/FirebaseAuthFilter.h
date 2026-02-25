// equatix.io-backend-cpp/filters/FirebaseAuthFilter.h
#pragma once

#include <drogon/HttpFilter.h>

class FirebaseAuthFilter : public drogon::HttpFilter<FirebaseAuthFilter>
{
  public:
    FirebaseAuthFilter() {}
    void doFilter(const drogon::HttpRequestPtr &req,
                  drogon::FilterCallback &&fcb,
                  drogon::FilterChainCallback &&fccb) override;

    // Static method to trigger background public key refresh
    static void refreshKeys();
};

// Global function declaration for use in main.cc
void fetchFirebaseKeys();