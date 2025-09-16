//
// Created by a.durynin on 10.09.2025.
//
#pragma once

#include <QPushButton>

#include "constants.hpp"

class TabButton final: public QPushButton {
  Q_OBJECT
public:
  explicit TabButton(const QString &text, QWidget *parent = nullptr);

protected:
  void paintEvent(QPaintEvent *event) override;
  QSize sizeHint() const override;

private:
  QString mText;
};
