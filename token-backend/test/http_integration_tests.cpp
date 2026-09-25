// HTTP-layer integration tests.
//
// Everything else in this suite tests below the wire: MeetingService without
// HTTP, the repositories without a service. That left the actual contract —
// routing, header extraction, DTO field names, status-code mapping —
// unverified by anything automated, which is precisely where this project has
// shipped bugs (Task 1's unrouted endpoint, and findings I4 and I5 in the
// final review).
//
// These tests run the real controllers on a real oat++ server bound to a
// loopback test port and drive it with a real HTTP client, so a request here
// travels the same path a desktop client's would.

#include "auth/static_token_authorizer.h"
#include "config.h"
#include "controller/health_controller.h"
#include "controller/invitations_controller.h"
#include "controller/meetings_controller.h"
#include "db/accounts_repository.h"
#include "db/invitations_repository.h"
#include "db/meetings_repository.h"
#include "db/migrations.h"
#include "db/sqlite_connection.h"
#include "service/meeting_service.h"

#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/client/ConnectionProvider.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/client/HttpRequestExecutor.hpp"
#include "oatpp/web/protocol/http/outgoing/BufferBody.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/HttpRouter.hpp"

#include <gtest/gtest.h>
#include <sodium.h>

#include <chrono>
#include <ctime>
#include <memory>
#include <string>
#include <thread>

namespace {

constexpr v_uint16 kTestPort = 18432;
const char *kInvitationBase = "https://join.example.test/j/";

std::string isoFromNow(int64_t offsetSeconds) {
  auto when = std::chrono::system_clock::now() + std::chrono::seconds(offsetSeconds);
  std::time_t t = std::chrono::system_clock::to_time_t(when);
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

// The responses are small and flat, so a substring probe is enough to assert
// on field names and values without pulling in a JSON parser. Asserting on the
// raw wire bytes is arguably the point: it catches a DTO field being renamed.
bool bodyHas(const std::string &body, const std::string &needle) {
  return body.find(needle) != std::string::npos;
}

std::string quotedField(const std::string &name, const std::string &value) {
  return "\"" + name + "\":\"" + value + "\"";
}

// Pulls a top-level string field out of a flat JSON object. oat++ escapes the
// slashes in URLs ("https:\/\/..."), which the callers below account for.
std::string jsonString(const std::string &body, const std::string &field) {
  const std::string key = "\"" + field + "\":\"";
  auto start = body.find(key);
  if (start == std::string::npos) {
    return {};
  }
  start += key.size();
  auto end = body.find('"', start);
  if (end == std::string::npos) {
    return {};
  }
  std::string raw = body.substr(start, end - start);
  std::string out;
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '\\' && i + 1 < raw.size()) {
      ++i;
    }
    out += raw[i];
  }
  return out;
}

struct HttpResponse {
  int status = 0;
  std::string body;
};

} // namespace

// One server for the whole file: starting and stopping an oat++ server per
// test would dominate the runtime, and each test creates its own meeting so
// they do not collide over shared state.
class HttpIntegrationTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    ASSERT_TRUE(sodium_init() >= 0);

    conn_ = new pcm::tokenbackend::SqliteConnection(":memory:");
    pcm::tokenbackend::runMigrations(*conn_);

    accounts_ = new pcm::tokenbackend::AccountsRepository(*conn_);
    credential_ = new std::string(accounts_->seedAccount());
    authorizer_ = new pcm::tokenbackend::StaticTokenAuthorizer(*accounts_);
    meetings_ = new pcm::tokenbackend::MeetingsRepository(*conn_);
    invitations_ = new pcm::tokenbackend::InvitationsRepository(*conn_);

    config_ = new pcm::tokenbackend::Config{};
    config_->liveKitApiKey = "test-key";
    config_->liveKitApiSecret = "test-secret";
    config_->tokenTtlSeconds = 600;

    service_ = new pcm::tokenbackend::MeetingService(*authorizer_, *meetings_, *invitations_,
                                                      *config_, "ws://livekit.test:7880");

    auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();

    auto router = oatpp::web::server::HttpRouter::createShared();
    healthController_ = std::make_shared<pcm::tokenbackend::HealthController>();
    meetingsController_ = std::make_shared<pcm::tokenbackend::MeetingsController>(
        objectMapper, *service_, kInvitationBase);
    invitationsController_ =
        std::make_shared<pcm::tokenbackend::InvitationsController>(objectMapper, *service_);

    // Exactly the wiring main.cpp performs. If an endpoint is left unrouted
    // there — the Task 1 bug — these tests see the same 404 a client would.
    router->route(healthController_->getEndpoints());
    router->route(meetingsController_->getEndpoints());
    router->route(invitationsController_->getEndpoints());

    connectionHandler_ = oatpp::web::server::HttpConnectionHandler::createShared(router);
    serverProvider_ = oatpp::network::tcp::server::ConnectionProvider::createShared(
        {"127.0.0.1", kTestPort});
    server_ = new oatpp::network::Server(serverProvider_, connectionHandler_);

    serverThread_ = new std::thread([] { server_->run(); });

    clientProvider_ = oatpp::network::tcp::client::ConnectionProvider::createShared(
        {"127.0.0.1", kTestPort});
    executor_ = oatpp::web::client::HttpRequestExecutor::createShared(clientProvider_);

    // Wait for the listener rather than sleeping a fixed amount.
    for (int i = 0; i < 100; ++i) {
      try {
        auto response = executor_->execute("GET", "/healthz", {}, nullptr, nullptr);
        if (response->getStatusCode() == 200) {
          return;
        }
      } catch (...) {
        // not listening yet
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    FAIL() << "test server did not come up on port " << kTestPort;
  }

  static void TearDownTestSuite() {
    server_->stop();
    connectionHandler_->stop();
    serverProvider_->stop();
    // The accept loop wakes on the next connection attempt; nudge it so the
    // thread is joinable promptly instead of blocking the whole suite.
    try {
      executor_->execute("GET", "/healthz", {}, nullptr, nullptr);
    } catch (...) {
    }
    serverThread_->join();

    delete serverThread_;
    delete server_;
    delete service_;
    delete config_;
    delete invitations_;
    delete meetings_;
    delete authorizer_;
    delete credential_;
    delete accounts_;
    delete conn_;
  }

  static HttpResponse request(const char *method, const std::string &path,
                               const oatpp::web::client::RequestExecutor::Headers &headers = {},
                               const std::string &body = "") {
    std::shared_ptr<oatpp::web::protocol::http::outgoing::Body> outgoing;
    if (!body.empty()) {
      outgoing = oatpp::web::protocol::http::outgoing::BufferBody::createShared(
          oatpp::String(body.c_str()), "application/json");
    }
    auto response = executor_->execute(method, path.c_str(), headers, outgoing, nullptr);
    HttpResponse result;
    result.status = response->getStatusCode();
    auto text = response->readBodyToString();
    result.body = text ? *text : std::string();
    return result;
  }

  static oatpp::web::client::RequestExecutor::Headers authHeaders(const std::string &value) {
    oatpp::web::client::RequestExecutor::Headers headers;
    headers.put("Authorization", oatpp::String(value.c_str()));
    return headers;
  }

  static oatpp::web::client::RequestExecutor::Headers bearer() {
    return authHeaders("Bearer " + *credential_);
  }

  // Creates a meeting over HTTP and returns the parsed response.
  struct CreatedMeeting {
    std::string meetingRef;
    std::string invitationCode;
    std::string passcode;
    std::string body;
  };

  static CreatedMeeting createMeeting(int64_t startOffset = -60, int64_t endOffset = 60 * 60) {
    auto response = request("POST", "/v1/meetings", bearer(),
                             "{\"scheduledStart\":\"" + isoFromNow(startOffset) +
                                 "\",\"scheduledEnd\":\"" + isoFromNow(endOffset) + "\"}");
    EXPECT_EQ(response.status, 200) << response.body;
    CreatedMeeting created;
    created.body = response.body;
    created.meetingRef = jsonString(response.body, "meetingRef");
    created.passcode = jsonString(response.body, "passcode");
    auto url = jsonString(response.body, "invitationUrl");
    created.invitationCode = url.substr(url.rfind('/') + 1);
    return created;
  }

  static std::string clientTokenPath(const std::string &code) {
    return "/v1/invitations/" + code + "/client-token";
  }

  static pcm::tokenbackend::SqliteConnection *conn_;
  static pcm::tokenbackend::AccountsRepository *accounts_;
  static pcm::tokenbackend::Authorizer *authorizer_;
  static pcm::tokenbackend::MeetingsRepository *meetings_;
  static pcm::tokenbackend::InvitationsRepository *invitations_;
  static pcm::tokenbackend::Config *config_;
  static pcm::tokenbackend::MeetingService *service_;
  static std::string *credential_;

  static std::shared_ptr<pcm::tokenbackend::HealthController> healthController_;
  static std::shared_ptr<pcm::tokenbackend::MeetingsController> meetingsController_;
  static std::shared_ptr<pcm::tokenbackend::InvitationsController> invitationsController_;
  static std::shared_ptr<oatpp::web::server::HttpConnectionHandler> connectionHandler_;
  static std::shared_ptr<oatpp::network::tcp::server::ConnectionProvider> serverProvider_;
  static std::shared_ptr<oatpp::network::tcp::client::ConnectionProvider> clientProvider_;
  static std::shared_ptr<oatpp::web::client::HttpRequestExecutor> executor_;
  static oatpp::network::Server *server_;
  static std::thread *serverThread_;
};

pcm::tokenbackend::SqliteConnection *HttpIntegrationTest::conn_ = nullptr;
pcm::tokenbackend::AccountsRepository *HttpIntegrationTest::accounts_ = nullptr;
pcm::tokenbackend::Authorizer *HttpIntegrationTest::authorizer_ = nullptr;
pcm::tokenbackend::MeetingsRepository *HttpIntegrationTest::meetings_ = nullptr;
pcm::tokenbackend::InvitationsRepository *HttpIntegrationTest::invitations_ = nullptr;
pcm::tokenbackend::Config *HttpIntegrationTest::config_ = nullptr;
pcm::tokenbackend::MeetingService *HttpIntegrationTest::service_ = nullptr;
std::string *HttpIntegrationTest::credential_ = nullptr;
std::shared_ptr<pcm::tokenbackend::HealthController> HttpIntegrationTest::healthController_;
std::shared_ptr<pcm::tokenbackend::MeetingsController> HttpIntegrationTest::meetingsController_;
std::shared_ptr<pcm::tokenbackend::InvitationsController>
    HttpIntegrationTest::invitationsController_;
std::shared_ptr<oatpp::web::server::HttpConnectionHandler> HttpIntegrationTest::connectionHandler_;
std::shared_ptr<oatpp::network::tcp::server::ConnectionProvider>
    HttpIntegrationTest::serverProvider_;
std::shared_ptr<oatpp::network::tcp::client::ConnectionProvider>
    HttpIntegrationTest::clientProvider_;
std::shared_ptr<oatpp::web::client::HttpRequestExecutor> HttpIntegrationTest::executor_;
oatpp::network::Server *HttpIntegrationTest::server_ = nullptr;
std::thread *HttpIntegrationTest::serverThread_ = nullptr;

// --- routing ----------------------------------------------------------------

TEST_F(HttpIntegrationTest, HealthEndpointIsRouted) {
  auto response = request("GET", "/healthz");
  EXPECT_EQ(response.status, 200);
  EXPECT_EQ(response.body, "ok");
}

TEST_F(HttpIntegrationTest, UnknownPathIs404) {
  EXPECT_EQ(request("GET", "/v1/nope").status, 404);
}

// --- POST /v1/meetings ------------------------------------------------------

TEST_F(HttpIntegrationTest, CreateMeetingReturnsEveryDocumentedField) {
  auto start = isoFromNow(-60);
  auto end = isoFromNow(60 * 60);
  auto response = request("POST", "/v1/meetings", bearer(),
                           "{\"scheduledStart\":\"" + start + "\",\"scheduledEnd\":\"" + end +
                               "\"}");

  ASSERT_EQ(response.status, 200) << response.body;
  EXPECT_TRUE(bodyHas(response.body, "\"meetingRef\":\"mtg_")) << response.body;
  EXPECT_TRUE(bodyHas(response.body, "\"invitationUrl\":\"")) << response.body;
  EXPECT_TRUE(bodyHas(response.body, "\"passcode\":\"")) << response.body;
  EXPECT_TRUE(bodyHas(response.body, quotedField("scheduledStart", start))) << response.body;
  EXPECT_TRUE(bodyHas(response.body, quotedField("scheduledEnd", end))) << response.body;

  EXPECT_EQ(jsonString(response.body, "passcode").size(), 6u);
  EXPECT_EQ(jsonString(response.body, "invitationUrl").rfind(kInvitationBase, 0), 0u)
      << "invitationUrl must be built from INVITATION_BASE_URL";
}

TEST_F(HttpIntegrationTest, CreateMeetingWithNoAuthorizationHeaderIs401) {
  auto response = request("POST", "/v1/meetings", {},
                           "{\"scheduledStart\":\"" + isoFromNow(-60) + "\",\"scheduledEnd\":\"" +
                               isoFromNow(60 * 60) + "\"}");
  EXPECT_EQ(response.status, 401) << response.body;
  EXPECT_TRUE(bodyHas(response.body, "\"error\"")) << response.body;
}

TEST_F(HttpIntegrationTest, CreateMeetingWithWrongCredentialIs401) {
  auto response = request("POST", "/v1/meetings", authHeaders("Bearer not-the-credential"),
                           "{\"scheduledStart\":\"" + isoFromNow(-60) + "\",\"scheduledEnd\":\"" +
                               isoFromNow(60 * 60) + "\"}");
  EXPECT_EQ(response.status, 401) << response.body;
}

TEST_F(HttpIntegrationTest, CreateMeetingWithoutASchedduleIs400) {
  auto response = request("POST", "/v1/meetings", bearer(), "{}");
  EXPECT_EQ(response.status, 400) << response.body;
  EXPECT_TRUE(bodyHas(response.body, "schedule_required")) << response.body;
}

// I5: the RFC 7235 form used to hash the literal "Bearer <cred>" and 401.
TEST_F(HttpIntegrationTest, BothBearerAndBareCredentialFormsAreAccepted) {
  const std::string payload = "{\"scheduledStart\":\"" + isoFromNow(-60) +
                               "\",\"scheduledEnd\":\"" + isoFromNow(60 * 60) + "\"}";

  EXPECT_EQ(request("POST", "/v1/meetings", authHeaders("Bearer " + *credential_), payload).status,
            200);
  EXPECT_EQ(request("POST", "/v1/meetings", authHeaders("bearer " + *credential_), payload).status,
            200)
      << "the scheme token is case-insensitive per RFC 7235";
  EXPECT_EQ(request("POST", "/v1/meetings", authHeaders(*credential_), payload).status, 200)
      << "the bare form must keep working";
}

// --- POST /v1/meetings/{ref}/specialist-token -------------------------------

TEST_F(HttpIntegrationTest, SpecialistTokenReturnsTheDocumentedFields) {
  auto meeting = createMeeting();
  auto response =
      request("POST", "/v1/meetings/" + meeting.meetingRef + "/specialist-token", bearer());

  ASSERT_EQ(response.status, 200) << response.body;
  // oat++ escapes the slashes in a JSON string, so compare the decoded value.
  EXPECT_EQ(jsonString(response.body, "endpointUrl"), "ws://livekit.test:7880") << response.body;
  EXPECT_TRUE(bodyHas(response.body, "\"roomName\":\"rm_")) << response.body;
  EXPECT_TRUE(bodyHas(response.body, "\"token\":\"")) << response.body;
  EXPECT_TRUE(bodyHas(response.body, "\"expiresAt\":")) << response.body;
  EXPECT_TRUE(bodyHas(response.body, "\"token\":\"eyJ"))
      << "the token must be a JWT: " << response.body;
}

TEST_F(HttpIntegrationTest, SpecialistTokenForAnUnknownMeetingIs404) {
  EXPECT_EQ(request("POST", "/v1/meetings/mtg_does_not_exist/specialist-token", bearer()).status,
            404);
}

TEST_F(HttpIntegrationTest, SpecialistTokenWithoutCredentialIs401) {
  auto meeting = createMeeting();
  EXPECT_EQ(request("POST", "/v1/meetings/" + meeting.meetingRef + "/specialist-token").status,
            401);
}

// --- POST /v1/invitations/{code}/client-token -------------------------------

TEST_F(HttpIntegrationTest, ClientTokenSucceedsWithNoCredentialAtAll) {
  auto meeting = createMeeting();
  auto response = request("POST", clientTokenPath(meeting.invitationCode), {},
                           "{\"passcode\":\"" + meeting.passcode + "\"}");

  ASSERT_EQ(response.status, 200) << response.body;
  EXPECT_TRUE(bodyHas(response.body, "\"token\":\"eyJ")) << response.body;
  EXPECT_TRUE(bodyHas(response.body, "\"roomName\":\"rm_")) << response.body;
  EXPECT_EQ(jsonString(response.body, "endpointUrl"), "ws://livekit.test:7880") << response.body;
}

TEST_F(HttpIntegrationTest, ClientTokenWithWrongPasscodeIs401) {
  auto meeting = createMeeting();
  auto response =
      request("POST", clientTokenPath(meeting.invitationCode), {}, "{\"passcode\":\"000000\"}");
  EXPECT_EQ(response.status, 401) << response.body;
}

TEST_F(HttpIntegrationTest, ClientTokenWithUnknownCodeIs404) {
  auto response = request("POST", clientTokenPath("no-such-code"), {}, "{\"passcode\":\"123456\"}");
  EXPECT_EQ(response.status, 404) << response.body;
}

// I4.1: `{}` used to reach the service as an empty passcode and be scored as a
// wrong guess, so six of them permanently killed the invitation.
TEST_F(HttpIntegrationTest, ClientTokenWithMissingPasscodeIs400AndCostsNoAttempt) {
  auto meeting = createMeeting();

  for (int i = 0; i < 7; ++i) {
    auto response = request("POST", clientTokenPath(meeting.invitationCode), {}, "{}");
    EXPECT_EQ(response.status, 400) << "request " << i << ": " << response.body;
    EXPECT_TRUE(bodyHas(response.body, "passcode_required")) << response.body;
  }

  auto stillWorks = request("POST", clientTokenPath(meeting.invitationCode), {},
                             "{\"passcode\":\"" + meeting.passcode + "\"}");
  EXPECT_EQ(stillWorks.status, 200)
      << "malformed requests must not burn the 5-attempt budget: " << stillWorks.body;
}

TEST_F(HttpIntegrationTest, ClientTokenWithEmptyPasscodeStringIs400) {
  auto meeting = createMeeting();
  auto response =
      request("POST", clientTokenPath(meeting.invitationCode), {}, "{\"passcode\":\"\"}");
  EXPECT_EQ(response.status, 400) << response.body;
}

TEST_F(HttpIntegrationTest, FiveWrongPasscodesLockTheInvitationOutWith429) {
  auto meeting = createMeeting();

  for (int i = 0; i < 5; ++i) {
    request("POST", clientTokenPath(meeting.invitationCode), {}, "{\"passcode\":\"000000\"}");
  }

  auto response = request("POST", clientTokenPath(meeting.invitationCode), {},
                           "{\"passcode\":\"" + meeting.passcode + "\"}");
  EXPECT_EQ(response.status, 429) << response.body;
}

TEST_F(HttpIntegrationTest, InvitationCodeStaysRedeemableForReconnects) {
  auto meeting = createMeeting();
  auto path = clientTokenPath(meeting.invitationCode);
  const std::string body = "{\"passcode\":\"" + meeting.passcode + "\"}";

  EXPECT_EQ(request("POST", path, {}, body).status, 200);
  EXPECT_EQ(request("POST", path, {}, body).status, 200)
      << "the invitation is not consumed by first use";
}

// --- I3: scheduled-window enforcement over the wire -------------------------

TEST_F(HttpIntegrationTest, TokensOutsideTheScheduledWindowAre410) {
  auto future = createMeeting(3 * 60 * 60, 4 * 60 * 60);

  EXPECT_EQ(request("POST", clientTokenPath(future.invitationCode), {},
                     "{\"passcode\":\"" + future.passcode + "\"}")
                .status,
            410);
  EXPECT_EQ(
      request("POST", "/v1/meetings/" + future.meetingRef + "/specialist-token", bearer()).status,
      410);

  auto past = createMeeting(-4 * 60 * 60, -3 * 60 * 60);
  EXPECT_EQ(request("POST", clientTokenPath(past.invitationCode), {},
                     "{\"passcode\":\"" + past.passcode + "\"}")
                .status,
            410);
}

// --- POST /v1/meetings/{ref}/invitation (I4.2) ------------------------------

TEST_F(HttpIntegrationTest, ReissueInvitationRetiresTheOldCodeAndKeepsTheMeeting) {
  auto meeting = createMeeting();

  auto response = request("POST", "/v1/meetings/" + meeting.meetingRef + "/invitation", bearer());
  ASSERT_EQ(response.status, 200) << response.body;
  EXPECT_TRUE(bodyHas(response.body, quotedField("meetingRef", meeting.meetingRef)))
      << response.body;

  auto newUrl = jsonString(response.body, "invitationUrl");
  auto newCode = newUrl.substr(newUrl.rfind('/') + 1);
  auto newPasscode = jsonString(response.body, "passcode");
  EXPECT_NE(newCode, meeting.invitationCode);

  EXPECT_EQ(request("POST", clientTokenPath(meeting.invitationCode), {},
                     "{\"passcode\":\"" + meeting.passcode + "\"}")
                .status,
            410)
      << "the superseded code must stop working";
  EXPECT_EQ(
      request("POST", clientTokenPath(newCode), {}, "{\"passcode\":\"" + newPasscode + "\"}")
          .status,
      200);
}

TEST_F(HttpIntegrationTest, ReissueRecoversAnInvitationFromTheLockout) {
  auto meeting = createMeeting();
  for (int i = 0; i < 5; ++i) {
    request("POST", clientTokenPath(meeting.invitationCode), {}, "{\"passcode\":\"000000\"}");
  }
  ASSERT_EQ(request("POST", clientTokenPath(meeting.invitationCode), {},
                     "{\"passcode\":\"" + meeting.passcode + "\"}")
                .status,
            429);

  auto response = request("POST", "/v1/meetings/" + meeting.meetingRef + "/invitation", bearer());
  ASSERT_EQ(response.status, 200) << response.body;
  auto newUrl = jsonString(response.body, "invitationUrl");
  auto newCode = newUrl.substr(newUrl.rfind('/') + 1);

  EXPECT_EQ(request("POST", clientTokenPath(newCode), {},
                     "{\"passcode\":\"" + jsonString(response.body, "passcode") + "\"}")
                .status,
            200);
}

TEST_F(HttpIntegrationTest, ReissueWithoutCredentialIs401AndUnknownMeetingIs404) {
  auto meeting = createMeeting();
  EXPECT_EQ(request("POST", "/v1/meetings/" + meeting.meetingRef + "/invitation").status, 401);
  EXPECT_EQ(request("POST", "/v1/meetings/mtg_does_not_exist/invitation", bearer()).status, 404);
}

// --- POST /v1/meetings/{ref}/invalidate -------------------------------------

TEST_F(HttpIntegrationTest, InvalidateReturns204AndStopsBothTokenEndpoints) {
  auto meeting = createMeeting();

  auto response = request("POST", "/v1/meetings/" + meeting.meetingRef + "/invalidate", bearer());
  EXPECT_EQ(response.status, 204) << response.body;
  EXPECT_TRUE(response.body.empty()) << "204 must carry no body, got: " << response.body;

  EXPECT_EQ(
      request("POST", "/v1/meetings/" + meeting.meetingRef + "/specialist-token", bearer()).status,
      410);
  EXPECT_EQ(request("POST", clientTokenPath(meeting.invitationCode), {},
                     "{\"passcode\":\"" + meeting.passcode + "\"}")
                .status,
            410);
}

TEST_F(HttpIntegrationTest, InvalidateWithoutCredentialIs401AndUnknownMeetingIs404) {
  auto meeting = createMeeting();
  EXPECT_EQ(request("POST", "/v1/meetings/" + meeting.meetingRef + "/invalidate").status, 401);
  EXPECT_EQ(request("POST", "/v1/meetings/mtg_does_not_exist/invalidate", bearer()).status, 404);
}
