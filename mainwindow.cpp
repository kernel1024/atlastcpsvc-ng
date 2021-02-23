#include <QMessageBox>
#include <QSslKey>
#include <QSslCertificate>
#include <QFileDialog>
#include <QInputDialog>
#include "mainwindow.h"
#include "atlas.h"
#include "service.h"
#include "ui_mainwindow.h"
#include "ui_logindlg.h"

CMainWindow::CMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/trans.png"));

    connect(ui->btnAddToken,&QPushButton::clicked,this,&CMainWindow::addToken);
    connect(ui->btnDeleteToken,&QPushButton::clicked,this,&CMainWindow::delToken);
    connect(ui->btnClearCert,&QPushButton::clicked,this,&CMainWindow::clearCert);
    connect(ui->btnClearKey,&QPushButton::clicked,this,&CMainWindow::clearKey);
    connect(ui->btnLoadCert,&QPushButton::clicked,this,&CMainWindow::loadCert);
    connect(ui->btnLoadKey,&QPushButton::clicked,this,&CMainWindow::loadKey);
#ifdef qOverload
    connect(ui->spinPort,qOverload<int>(&QSpinBox::valueChanged),this,&MainWindow::updatePort);
    connect(ui->listEnvironment,qOverload<int>(&QComboBox::currentIndexChanged),
            this,&MainWindow::changeEnvironment);
#endif
    connect(ui->btnSave,&QPushButton::clicked,this,&CMainWindow::saveSettings);
    connect(ui->btnSvcInstall,&QPushButton::clicked,this,&CMainWindow::installSerivce);
    connect(ui->btnSvcUninstall,&QPushButton::clicked,this,&CMainWindow::uninstallSerivce);

    ui->btnSvcInstall->setEnabled(false);
    ui->btnSvcUninstall->setEnabled(false);
}

CMainWindow::~CMainWindow()
{
    delete ui;
}

void CMainWindow::updateService(CService *service)
{
    m_service = service;
    if (m_service == nullptr) return;

    if (!m_service->daemon()->isAtlasLoaded()) {
        QMessageBox::critical(nullptr,QGuiApplication::applicationDisplayName(),
                              tr("ATLAS engine failed to load. "
                                 "Unable to initialize parameters."));
        ui->btnSave->setEnabled(false);
        return;
    }

    updateWidgets();
}

void CMainWindow::updateWidgets()
{
    ui->btnSvcInstall->setEnabled(false);
    ui->btnSvcUninstall->setEnabled(false);

    if (m_service == nullptr) return;

    ui->btnSvcInstall->setEnabled(!m_service->isInstalled());
    ui->btnSvcUninstall->setEnabled(m_service->isInstalled());

    const auto daemon = m_service->daemon();

    ui->spinPort->setValue(daemon->atlasPort());

    const QStringList env = daemon->atlasEnvironments();
    ui->listEnvironment->addItems(env);

    if (env.contains(daemon->atlasEnv()))
        ui->listEnvironment->setCurrentIndex(env.indexOf(daemon->atlasEnv()));

    ui->listTokens->clear();
    ui->listTokens->addItems(daemon->clientTokens());

    ui->lblUserName->setText(CService::getCurrentUserName());

    updateSSLWidgets();
}

void CMainWindow::updateSSLWidgets()
{
    ui->lblKey->setText(tr("<empty>"));
    ui->lblCert->setText(tr("<empty>"));

    if (m_service == nullptr) return;

    QSslKey key = m_service->daemon()->privateKey();
    if (!key.isNull()) {
        const int bits = key.length();
        switch (key.algorithm()) {
            case QSsl::Opaque: ui->lblKey->setText(tr("Black box, %1 bits").arg(bits)); break;
            case QSsl::Rsa: ui->lblKey->setText(tr("RSA, %1 bits").arg(bits)); break;
            case QSsl::Dsa: ui->lblKey->setText(tr("DSA, %1 bits").arg(bits)); break;
            case QSsl::Ec: ui->lblKey->setText(tr("Elliptic Curve, %1 bits").arg(bits)); break;
            case QSsl::Dh: ui->lblKey->setText(tr("Diffie-Hellman, %1 bits").arg(bits)); break;
        }
    }

    QSslCertificate cert = m_service->daemon()->serverCert();
    if (!cert.isNull()) {
        const QStringList sl = cert.subjectInfo(QSslCertificate::CommonName);
        if (sl.isEmpty()) {
            ui->lblCert->setText(tr("<some certificate>"));
        } else {
            ui->lblCert->setText(sl.first());
        }
    }
}

void CMainWindow::addToken()
{
    if (m_service == nullptr) return;

    bool ok = false;
    const QString token = QInputDialog::getText(this,QGuiApplication::applicationDisplayName(),
                                                tr("Add new authentication token"),
                                                QLineEdit::Normal,QString(),&ok);
    if (ok && !token.isEmpty()) {
        m_service->daemon()->addToken(token);
        ui->listTokens->clear();
        ui->listTokens->addItems(m_service->daemon()->clientTokens());
    }
}

void CMainWindow::delToken()
{
    if (m_service == nullptr) return;

    QListWidgetItem *itm = ui->listTokens->currentItem();
    if (itm == nullptr || itm->text().isEmpty()) return;

    m_service->daemon()->deleteToken(itm->text());
    ui->listTokens->clear();
    ui->listTokens->addItems(m_service->daemon()->clientTokens());
}

void CMainWindow::loadKey()
{
    if (m_service == nullptr) return;

    const QString fname = QFileDialog::getOpenFileName(this,tr("OpenSSL server private key"),QString(),
                                                       tr("Private RSA key file, PEM format (*.key)"));
    if (fname.isEmpty()) return;

    QFile f(fname);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                              tr("Unable to open key file."));
        return;
    }

    QSslKey key(&f,QSsl::Rsa);
    f.close();
    if (key.isNull()) {
        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                              tr("Unable to parse key file. Private RSA key in PEM format needed."));
        return;
    }

    m_service->daemon()->setPrivateKey(key);
    updateSSLWidgets();
}

void CMainWindow::loadCert()
{
    if (m_service == nullptr) return;

    const QString fname = QFileDialog::getOpenFileName(this,tr("OpenSSL server certificate"),QString(),
                                                       tr("Server certificate, PEM format (*.crt)"));
    if (fname.isEmpty()) return;

    QFile f(fname);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                              tr("Unable to open certificate file."));
        return;
    }

    QSslCertificate cert(&f);
    f.close();
    if (cert.isNull()) {
        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                              tr("Unable to parse certificate file. "
                                 "Certificate in PEM format needed."));
        return;
    }

    m_service->daemon()->setServerCert(cert);
    updateSSLWidgets();
}

void CMainWindow::clearKey()
{
    if (m_service == nullptr) return;

    m_service->daemon()->setPrivateKey(QSslKey());
    updateSSLWidgets();
}

void CMainWindow::clearCert()
{
    if (m_service == nullptr) return;

    m_service->daemon()->setServerCert(QSslCertificate());
    updateSSLWidgets();
}

void CMainWindow::updatePort(int port)
{
    if (m_service == nullptr) return;

    m_service->daemon()->setAtlasPort(port);
}

void CMainWindow::changeEnvironment(const QString &env)
{
    if (m_service == nullptr) return;

    if (!env.isEmpty())
        m_service->daemon()->setAtlasEnv(env);
}

void CMainWindow::saveSettings()
{
    if (m_service == nullptr) return;

    if (!m_service->daemon()->saveSettings()) {
        QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                              tr("Unable to save service configuration. "
                                 "Make sure you have administrator or local system privileges."));
    } else {
        QMessageBox::information(this,QGuiApplication::applicationDisplayName(),
                                 tr("Service settings saved."
                                    "You can now restart server."));
    }
}

void CMainWindow::installSerivce()
{
    if (m_service == nullptr) return;

    QDialog dlg(this);
    Ui::loginDialog ldui;
    ldui.setupUi(&dlg);

    ldui.editUser->setText(CService::getCurrentUserName());
    ldui.labelMsg->setText(tr("Login credentials for ATLAS TCP service.\n"
                              "Service and ATLAS itself will save settings within selected user profile.\n"
                              "You can leave both field empty to use default LocalSystem account."));
    dlg.setWindowTitle(tr("%1 service login").arg(QGuiApplication::applicationDisplayName()));

    if (dlg.exec() == QDialog::Accepted) {
        if (!m_service->isInstalled()) {
            QString params = QString("-i");
            if (!ldui.editUser->text().isEmpty()) {
                params.append(QString(" %1").arg(ldui.editUser->text()));
                if (!ldui.editPassword->text().isEmpty())
                    params.append(QString(" %1").arg(ldui.editPassword->text()));
            }
            if (CService::runAs(QString(),params,true)!=0) {
                QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                                      tr("Unable to install service."));
            }
        }
        updateWidgets();
    }
}

void CMainWindow::uninstallSerivce()
{
    if (m_service == nullptr) return;

    if (m_service->isInstalled()) {
        if (CService::runAs(QString(),"-u",true) != 0) {
            QMessageBox::critical(this,QGuiApplication::applicationDisplayName(),
                                  tr("Unable to uninstall service."));
        }
    }
    updateWidgets();
}
