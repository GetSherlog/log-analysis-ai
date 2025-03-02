#pragma once

#include "ApiController.h"
#include "drain_parser.h"
#include "file_data_loader.h"
#include "preprocessor.h"
#include <drogon/HttpController.h>
#include <memory>
#include <vector>
#include <string>

namespace logai {
namespace web {

/**
 * @class LogParserController
 * @brief Controller for log parsing related endpoints
 */
class LogParserController : public drogon::HttpController<LogParserController, false>,
                         public ApiController {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(LogParserController::parseDrain, "/api/parser/drain", drogon::Post);
    ADD_METHOD_TO(LogParserController::parseFile, "/api/parser/file", drogon::Post);
    METHOD_LIST_END

    LogParserController();
    ~LogParserController() = default;

    /**
     * @brief Parse log lines using DRAIN algorithm
     * @param req HTTP request with log lines in JSON
     * @param callback Response callback
     */
    void parseDrain(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Parse logs from a file using the appropriate parser based on file type
     * @param req HTTP request with file path and parser config
     * @param callback Response callback
     */
    void parseFile(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    std::shared_ptr<ApiController> apiController_;
    std::unique_ptr<DrainParser> drainParser_;
};

} // namespace web
} // namespace logai 