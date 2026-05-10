#include "hal.h"
#include <board.h>
#include <cJSON.h>
#include <esp_log.h>
#include <mooncake_log.h>

static const std::string_view _tag = "LocalLLM";

static std::string json_to_string(cJSON* root)
{
    char* raw = cJSON_PrintUnformatted(root);
    if (raw == nullptr) {
        return {};
    }

    std::string value(raw);
    cJSON_free(raw);
    return value;
}

static std::string make_chat_payload(std::string_view prompt)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", CONFIG_NUKOEVI_LLM_MODEL);
    cJSON_AddBoolToObject(root, "stream", false);
    cJSON_AddNumberToObject(root, "temperature", 0.7);

    cJSON* messages = cJSON_AddArrayToObject(root, "messages");

    cJSON* system_message = cJSON_CreateObject();
    cJSON_AddStringToObject(system_message, "role", "system");
    cJSON_AddStringToObject(system_message, "content", CONFIG_NUKOEVI_LLM_SYSTEM_PROMPT);
    cJSON_AddItemToArray(messages, system_message);

    cJSON* user_message = cJSON_CreateObject();
    cJSON_AddStringToObject(user_message, "role", "user");
    cJSON_AddStringToObject(user_message, "content", std::string(prompt).c_str());
    cJSON_AddItemToArray(messages, user_message);

    std::string payload = json_to_string(root);
    cJSON_Delete(root);
    return payload;
}

static bool parse_chat_response(std::string_view body, std::string& response)
{
    cJSON* root = cJSON_ParseWithLength(body.data(), body.size());
    if (root == nullptr) {
        return false;
    }

    cJSON* choices = cJSON_GetObjectItem(root, "choices");
    cJSON* choice = cJSON_IsArray(choices) ? cJSON_GetArrayItem(choices, 0) : nullptr;
    cJSON* message = choice ? cJSON_GetObjectItem(choice, "message") : nullptr;
    cJSON* content = message ? cJSON_GetObjectItem(message, "content") : nullptr;

    bool success = false;
    if (cJSON_IsString(content) && content->valuestring != nullptr) {
        response = content->valuestring;
        success = true;
    }

    cJSON_Delete(root);
    return success;
}

bool Hal::requestLocalLlmChat(std::string_view prompt, std::string& response, std::function<void(std::string_view)> onLog)
{
    startNetwork(onLog);

    auto& board = Board::GetInstance();
    auto network = board.GetNetwork();
    auto http = network->CreateHttp(0);

    if (!http) {
        mclog::tagError(_tag, "failed to create http client");
        if (onLog) {
            onLog("LLM HTTP client failed");
        }
        return false;
    }

    std::string payload = make_chat_payload(prompt);
    if (payload.empty()) {
        mclog::tagError(_tag, "failed to build chat payload");
        if (onLog) {
            onLog("LLM payload failed");
        }
        return false;
    }

    http->SetHeader("Content-Type", "application/json");
    http->SetContent(std::move(payload));

    if (onLog) {
        onLog("Calling iPhone LLM...");
    }

    if (!http->Open("POST", CONFIG_NUKOEVI_LLM_ENDPOINT)) {
        mclog::tagError(_tag, "failed to open local llm request: {}", CONFIG_NUKOEVI_LLM_ENDPOINT);
        if (onLog) {
            onLog("LLM connect failed");
        }
        return false;
    }

    int status_code = http->GetStatusCode();
    std::string body = http->ReadAll();
    if (status_code < 200 || status_code >= 300) {
        mclog::tagError(_tag, "local llm http failed: status={}, body={}", status_code, body);
        if (onLog) {
            onLog("LLM HTTP failed");
        }
        return false;
    }

    if (!parse_chat_response(body, response)) {
        mclog::tagError(_tag, "failed to parse local llm response: {}", body);
        if (onLog) {
            onLog("LLM response failed");
        }
        return false;
    }

    return true;
}
