#pragma once

#include "controller/dto.h"
#include "controller/meetings_controller.h" // for statusForError
#include "service/meeting_service.h"

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/core/macro/codegen.hpp"

namespace pcm::tokenbackend {

#include OATPP_CODEGEN_BEGIN(ApiController)

class InvitationsController : public oatpp::web::server::api::ApiController {
public:
  InvitationsController(const std::shared_ptr<ObjectMapper> &objectMapper,
                         MeetingService &service)
      : oatpp::web::server::api::ApiController(objectMapper), service_(service) {}

  ENDPOINT("POST", "/v1/invitations/{code}/client-token", clientToken, PATH(String, code),
            BODY_DTO(Object<ClientTokenRequestDto>, body)) {
    auto result =
        service_.issueClientToken(toStdString(code), toStdString(body->passcode));
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

private:
  MeetingService &service_;
};

#include OATPP_CODEGEN_END(ApiController)

} // namespace pcm::tokenbackend
