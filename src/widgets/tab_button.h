//
// Created by a.durynin on 10.09.2025.
//
#pragma once

#include <QPushButton>
#include <QIcon>

#include "constants.hpp"

class TabButton final: public QPushButton {
  Q_OBJECT
public:
  explicit TabButton(const QString &text, QWidget *parent = nullptr);
  explicit TabButton(const QIcon &icon, const QString &text, QWidget *parent = nullptr);

protected:
  void paintEvent(QPaintEvent *event) override;
  QSize sizeHint() const override;

private:
  void init();

  QString mText;
  QIcon mIcon;
};
