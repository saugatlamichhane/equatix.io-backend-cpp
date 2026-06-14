#include <cstdlib>
#include <drogon/drogon.h>
#include <filters/FirebaseAuthFilter.h>
#include <stdexcept>
#include <utils/Board.h>

static std::string requireEnv(const char *name) {
  const char *val = std::getenv(name);
  if (!val)
    throw std::runtime_error(std::string("Missing required env var: ") + name);
  return val;
}

void setUpDatabase() {
  const std::string host = requireEnv("DB_HOST");
  const std::string port = requireEnv("DB_PORT");
  const std::string dbname = requireEnv("DB_NAME");
  const std::string user = requireEnv("DB_USER");
  const std::string passwd = requireEnv("DB_PASSWD");
  drogon::app().createDbClient("postgresql", host, std::stoi(port), dbname,
                               user, passwd,
                               1 // timeout in milliseconds
  );
}

void setupCors() {
  // Register sync advice to handle CORS preflight (OPTIONS) requests
  drogon::app().registerSyncAdvice(
      [](const drogon::HttpRequestPtr &req) -> drogon::HttpResponsePtr {
        if (req->method() == drogon::HttpMethod::Options) {
          auto resp = drogon::HttpResponse::newHttpResponse();

          // Set Access-Control-Allow-Origin header based on the Origin
          // request header
          const auto &origin = req->getHeader("Origin");
          if (!origin.empty()) {
            resp->addHeader("Access-Control-Allow-Origin", origin);
          }

          // Set Access-Control-Allow-Methods based on the requested method
          const auto &requestMethod =
              req->getHeader("Access-Control-Request-Method");
          if (!requestMethod.empty()) {
            resp->addHeader("Access-Control-Allow-Methods", requestMethod);
          }

          // Allow credentials to be included in cross-origin requests
          resp->addHeader("Access-Control-Allow-Credentials", "true");

          // Set allowed headers from the Access-Control-Request-Headers
          // header
          const auto &requestHeaders =
              req->getHeader("Access-Control-Request-Headers");
          if (!requestHeaders.empty()) {
            resp->addHeader("Access-Control-Allow-Headers", requestHeaders);
          }

          return std::move(resp);
        }
        return {};
      });

  // Register post-handling advice to add CORS headers to all responses
  drogon::app().registerPostHandlingAdvice(
      [](const drogon::HttpRequestPtr &req,
         const drogon::HttpResponsePtr &resp) -> void {
        // Set Access-Control-Allow-Origin based on the Origin request
        // header
        const auto &origin = req->getHeader("Origin");
        if (!origin.empty()) {
          resp->addHeader("Access-Control-Allow-Origin", origin);
        }

        // Reflect the requested Access-Control-Request-Method back in the
        // response
        const auto &requestMethod =
            req->getHeader("Access-Control-Request-Method");
        if (!requestMethod.empty()) {
          resp->addHeader("Access-Control-Allow-Methods", requestMethod);
        }

        // Allow credentials to be included in cross-origin requests
        resp->addHeader("Access-Control-Allow-Credentials", "true");

        // Reflect the requested Access-Control-Request-Headers back
        const auto &requestHeaders =
            req->getHeader("Access-Control-Request-Headers");
        if (!requestHeaders.empty()) {
          resp->addHeader("Access-Control-Allow-Headers", requestHeaders);
        }
      });
}

int main() {
  // Set HTTP listener address and port
  drogon::app().addListener("0.0.0.0", 5555);
  setupCors();
  fetchFirebaseKeys();
  drogon::app().getLoop()->runEvery(
      86400.0, []() { FirebaseAuthFilter::refreshKeys(); });
  // Load config file
  drogon::app().loadConfigFile("config.json");
  // drogon::app().loadConfigFile("../config.yaml");
  // Run HTTP framework,the method will block in the internal event loop
  setUpDatabase();
  drogon::app().run();
  return 0;
}
