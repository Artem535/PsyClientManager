#include "application.h"
#include "../widgets/app_settings.h"

#include <QLocale>
#include <QStringList>
#include <QTranslator>
#include <QIcon>
#include <oclero/qlementine.hpp>

Q_LOGGING_CATEGORY(logApplication, "pcm.Application")

namespace pcm {

Application::Application() {
  mDb = std::make_shared<database::Database>(mConf);
}

int Application::run(int argc, char *argv[]) {
  QApplication app(argc, argv);
  app.setOrganizationName("PsyClientManager");
  app.setApplicationName("PsyClientManager");
  app.setApplicationDisplayName("PsyClientManager");
  app.setApplicationVersion("0.1.0");
  app.setWindowIcon(QIcon(":/icons/brain-solid-full.svg"));
  auto *style = new oclero::qlementine::QlementineStyle(&app);
  app.setStyle(style);

  auto *themeManager = new oclero::qlementine::ThemeManager(style, &app);
  themeManager->loadDirectory(":/themes");
  themeManager->setCurrentTheme("Dark");
  qCInfo(logApplication) << "Qlementine themes loaded:" << themeManager->themeCount()
                         << "current:" << themeManager->currentTheme();

  QTranslator translator;
  const QString localeName = QLocale::system().name().toLower();
  const QString preferredLanguage = pcm::app_settings::languageCode();
  const bool preferRu = preferredLanguage == "ru" ||
                        (preferredLanguage == "system" && localeName.startsWith("ru"));
  const QStringList translationOrder =
      preferRu ? QStringList{"app_ru", "app_en"}
               : QStringList{"app_en", "app_ru"};

  auto tryLoadTranslation = [&](const QString &baseName) -> bool {
    const QStringList resourceCandidates = {
        QString(":/i18n/%1.qm").arg(baseName),
        QString(":/i18n/i18n/%1.qm").arg(baseName),
    };
    for (const auto &resourcePath : resourceCandidates) {
      if (translator.load(resourcePath)) {
        app.installTranslator(&translator);
        qCInfo(logApplication) << "Loaded translation:" << resourcePath;
        return true;
      }
    }
    return false;
  };

  bool translationLoaded = false;
  for (const auto &baseName : translationOrder) {
    if (tryLoadTranslation(baseName)) {
      translationLoaded = true;
      break;
    }
  }

  if (!translationLoaded) {
    qCWarning(logApplication) << "Failed to load translations. Locale:"
                              << localeName;
  }

  mMainWindow = std::make_unique<MainWindow>();
  mClientModel = std::make_shared<QClientModel>(mDb);

  mMainWindow->addEventInfoPage(new QTimelineModel(mDb, this));
  mMainWindow->addClientInfoPage(mClientModel);
  mMainWindow->addClientCardPage();
  mMainWindow->connectSignals();
  connectSignals();

  mMainWindow->show();

  return app.exec();
}
void Application::saveClient(const DuckClient &client) {
  qCDebug(logApplication) << "Application::saveClient| Client id:" << client.id;
  mDb->add_client(client);
  if (mClientModel) {
    mClientModel->reload();
  }
}

void Application::fillClientComboBox(QComboBox *box) {
  box->clear();
  const auto clients = mDb->get_clients();
  for (const auto &client : clients) {
    QString title{"%1 %2"};
    const auto name = client->name != std::nullopt
                          ? QString::fromStdString(client->name.value())
                          : tr(": VALUE_UNDEFINED");
    const auto lastname = client->last_name != std::nullopt
                              ? QString::fromStdString(client->last_name.value())
                              : tr(": VALUE_UNDEFINED");
    title = title.arg(name, lastname);
    const auto var = QVariant::fromValue(client->id);
    box->addItem(title, var);
  }
}

void Application::saveClientEventPair(const int64_t clientId,
                                      const int64_t eventId) {
  qCDebug(logApplication) << "Application::saveClientEventPair| Client id:"
                          << clientId << " Event id:" << eventId;

  mDb->add_event_client(eventId, clientId);
}

void Application::connectSignals() {
  connect(mMainWindow.get(), &MainWindow::provideSaveClient, this,
          &Application::saveClient);
  connect(mMainWindow.get(), &MainWindow::provideClientEventPairSave, this,
          &Application::saveClientEventPair);

  {
    const auto widget = mMainWindow->getPage(MainWindow::Pages::eventInfo);
    const auto page = dynamic_cast<QEventInfoPage *>(widget);
    connect(page, &QEventInfoPage::provideFillClientComboBox, this,
            &Application::fillClientComboBox);
    connect(page, &QEventInfoPage::provideClientByEventId, [page, this](int64_t eventId) {
      const auto client = mDb->get_client_by_event(eventId);
      emit page->clientResolved(client.id);
    });

  }
}

} // namespace pcm
