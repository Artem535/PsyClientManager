#include "clientinfo.h"
#include "ui/pages/ui_clientinfo.h"

ClientInfo::ClientInfo(std::shared_ptr<pcm::database::Database> db,
                       QWidget *parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::ClientInfo>()), m_db(db) {
  m_ui->setupUi(this);
}

ClientInfo::~ClientInfo() = default;
