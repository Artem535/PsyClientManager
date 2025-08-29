//
// Created by a.durynin on 29.08.2025.
//

#pragma once

namespace pcm::widgets::constants {

/**
 * @brief Width ratios for each column in the client card.
 */
constexpr double kFirstColumnWidth  = 0.20;  ///< Name + Age
constexpr double kSecondColumnWidth = 0.25;  ///< Contacts
constexpr double kThirdColumnWidth  = 0.20;  ///< Last session date
constexpr double kFourthColumnWidth = 0.15;  ///< Status chip
constexpr double kFifthColumnWidth  = 0.20;  ///< Action buttons

/**
 * @brief Fixed height of the client card in pixels.
 */
constexpr int kCardHeight = 50;

/**
 * @brief General horizontal padding in pixels.
 */
constexpr int kSideMargin = 10;

/**
 * @brief Status chip geometry.
 */
constexpr int kChipWidth        = 80;
constexpr int kChipHeight       = 20;
constexpr int kChipMarginBottom = 8;

/**
 * @brief Action button geometry.
 */
constexpr int kActionBtnSize   = 24;
constexpr int kActionBtnMargin = 6;

} // namespace pcm::widgets::constants
