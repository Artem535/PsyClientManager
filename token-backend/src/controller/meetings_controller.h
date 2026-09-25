#pragma once

#include "controller/dto.h"
#include "service/meeting_service.h"

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"

namespace pcm::tokenbackend {

inline std::string toStdString(const oatpp::String &value) {
  return value ? *value : std::string();
}

inline oatpp::web::protocol::http::Status statusForError(ServiceError err) {
  switch (err) {
  case ServiceError::Unauthorized:
    return oatpp::web::protocol::http::Status::CODE_401;
  case ServiceError::NotFound:
    return oatpp::web::protocol::http::Status::CODE_404;
  case ServiceError::WrongPasscode:
    return oatpp::web::protocol::http::Status::CODE_401;
  case ServiceError::TooManyAttempts:
    return oatpp::web::protocol::http::Status::CODE_429;
  case ServiceError::MeetingWindowClosed:
    return oatpp::web::protocol::http::Status::CODE_410;
  }
  return oatpp::web::protocol::http::Status::CODE_500;
}

#include OATPP_CODEGEN_BEGIN(ApiController)

class MeetingsController : public oatpp::web::server::api::ApiController {
public:
  MeetingsController(const std::shared_ptr<ObjectMapper> &objectMapper,
                      MeetingService &service, const std::string &invitationBaseUrl)
      : oatpp::web::server::api::ApiController(objectMapper), service_(service),
        invitationBaseUrl_(invitationBaseUrl) {}

  ENDPOINT("POST", "/v1/meetings", createMeeting, HEADER(String, authHeader, "Authorization"),
            BODY_DTO(Object<CreateMeetingRequestDto>, body)) {
    auto result = service_.createMeeting(toStdString(authHeader), toStdString(body->scheduledStart),
                                          toStdString(body->scheduledEnd));
    if (!result.ok()) {
      auto err = ErrorResponseDto::createShared();
      err->error = "unauthorized";
      return createDtoResponse(statusForError(*result.error), err);
    }
    auto dto = CreateMeetingResponseDto::createShared();
    dto->meetingRef = result.value->meetingRef;
    dto->invitationUrl = invitationBaseUrl_ + result.value->invitationCode;
    dto->passcode = result.value->passcode;
    dto->scheduledStart = result.value->scheduledStart;
    dto->scheduledEnd = result.value->scheduledEnd;
    return createDtoResponse(Status::CODE_200, dto);
  }

  ENDPOINT("POST", "/v1/meetings/{meetingRef}/specialist-token", specialistToken,
            PATH(String, meetingRef), HEADER(String, authHeader, "Authorization")) {
    auto result =
        service_.issueSpecialistToken(toStdString(authHeader), toStdString(meetingRef));
    if (!result.ok()) {
      auto err = ErrorResponseDto::createShared();
      err->error = "request_failed";
      return createDtoResponse(statusForError(*result.error), err);
    }
    auto dto = TokenResponseDto::createShared();
    dto->endpointUrl = result.value->endpointUrl;
    dto->roomName = result.value->roomName;
    dto->token = result.value->jwt;
    dto->expiresAt = result.value->expiresAtUnix;
    return createDtoResponse(Status::CODE_200, dto);
  }

  ENDPOINT("POST", "/v1/meetings/{meetingRef}/invalidate", invalidateMeeting,
            PATH(String, meetingRef), HEADER(String, authHeader, "Authorization")) {
    auto result =
        service_.invalidateMeeting(toStdString(authHeader), toStdString(meetingRef));
    if (!result.ok()) {
      auto err = ErrorResponseDto::createShared();
      err->error = "request_failed";
      return createDtoResponse(statusForError(*result.error), err);
    }
    return createResponse(Status::CODE_204, "");
  }

private:
  MeetingService &service_;
  std::string invitationBaseUrl_;
};

#include OATPP_CODEGEN_END(ApiController)

} // namespace pcm::tokenbackend
