/**
 *
 *  FirebaseAuthFilter.h
 *
 */

#pragma once

#include <drogon/HttpFilter.h>
using namespace drogon;


class FirebaseAuthFilter : public HttpFilter<FirebaseAuthFilter>
{
  public:
    FirebaseAuthFilter() {}
    void doFilter(const HttpRequestPtr &req,
                  FilterCallback &&fcb,
                  FilterChainCallback &&fccb) override;
};

