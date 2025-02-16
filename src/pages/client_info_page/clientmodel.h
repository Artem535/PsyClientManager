#pragma once
#include <QAbstractListModel>
#include <QObject>
#include <qtmetamacros.h>

class ClientModel : public QAbstractListModel {
  Q_OBJECT

public:
  explicit ClientModel(QObject *parent = nullptr);

};