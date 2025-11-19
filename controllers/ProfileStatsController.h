#pragma once
#include <drogon/HttpController.h>

using namespace drogon;

class ProfileStatsController : public drogon::HttpController<ProfileStatsController>
{
public:
    METHOD_LIST_BEGIN
    // GET /profile/stats/{uid}
    ADD_METHOD_TO(ProfileStatsController::getStats, "/profile/stats/{1}", Get);
    METHOD_LIST_END

    void getStats(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback,
                  const std::string &uid) const;
};
