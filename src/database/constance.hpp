#pragma once

#include <array>
#include <scheme.obx.hpp>

namespace pcm::database::constance {

constexpr std::array<ObxEventStatus, 3> event_statuses = {
    ObxEventStatus{.name = "pending"}, ObxEventStatus{.name = "completed"},
    ObxEventStatus{.name = "canceled"}};

constexpr std::array<ObxPaymentStatus, 5> payment_statuses = {
    ObxPaymentStatus{.name = "pending"}, ObxPaymentStatus{.name = "paid"},
    ObxPaymentStatus{.name = "canceled"}, ObxPaymentStatus{.name = "refunded"},
    ObxPaymentStatus{.name = "skipped"}};
}; // namespace pcm::constance
