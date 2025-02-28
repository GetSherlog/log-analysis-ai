#pragma once

#include <drogon/HttpController.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace logai {
namespace web {

/**
 * @class ApiController
 * @brief Base class for all LogAI API controllers
 */
class ApiController : public drogon::HttpController<ApiController> {
public:
    ApiController() = default;
    virtual ~ApiController() = default;

protected:
    /**
     * @brief Create a success JSON response
     * @param data The data to include in the response
     * @return HttpResponsePtr with JSON response
     */
    drogon::HttpResponsePtr createJsonResponse(const json& data) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(data.dump());
        resp->setStatusCode(drogon::k200OK);
        return resp;
    }

    /**
     * @brief Create an error JSON response
     * @param message Error message
     * @param status HTTP status code
     * @return HttpResponsePtr with JSON error response
     */
    drogon::HttpResponsePtr createErrorResponse(const std::string& message, 
                                                drogon::HttpStatusCode status = drogon::k400BadRequest) {
        json error = {
            {"error", true},
            {"message", message}
        };
        auto resp = drogon::HttpResponse::newHttpJsonResponse(error.dump());
        resp->setStatusCode(status);
        return resp;
    }

    /**
     * @brief Parse JSON from request body
     * @param req HTTP request
     * @param result JSON object to store the parsed result
     * @return true if parsing successful, false otherwise
     */
    bool parseJsonBody(const drogon::HttpRequestPtr& req, json& result) {
        try {
            result = json::parse(req->getBody());
            return true;
        } catch (const json::parse_error& e) {
            return false;
        }
    }
};

} // namespace web
} // namespace logai 