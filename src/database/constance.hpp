#pragma once

#include <array>
#include <scheme.obx.hpp>

namespace pcm::database::constance {

constexpr std::array<EventStatus, 3> event_statuses = {
    EventStatus{.name = "pending"}, EventStatus{.name = "completed"},
    EventStatus{.name = "canceled"}};

constexpr std::array<PaymentStatus, 5> payment_statuses = {
    PaymentStatus{.name = "pending"}, PaymentStatus{.name = "paid"},
    PaymentStatus{.name = "canceled"}, PaymentStatus{.name = "refunded"},
    PaymentStatus{.name = "skiped"}};
}; // namespace pcm::constance
