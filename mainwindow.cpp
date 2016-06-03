#include <QMessageBox>
#include <QSslKey>
#include <QSslCertificate>
#include <QFileDialog>
#include <QInputDialog>
#include "mainwindow.h"
#include "atlas.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->btnAddToken,&QPushButton::clicked,this,&MainWindow::addToken);
    connect(ui->btnDeleteToken,&QPushButton::clicked,this,&MainWindow::delToken);
    connect(ui->btnClearCert,&QPushButton::clicked,this,&MainWindow::clearCert);
    connect(ui->btnClearKey,&QPushButton::clicked,this,&MainWindow::clearKey);
    connect(ui->btnLoadCert,&QPushButton::clicked,this,&MainWindow::loadCert);
    connect(ui->btnLoadKey,&QPushButton::clicked,this,&MainWindow::loadKey);
    connect(ui->spinPort,SIGNAL(valueChanged(int)),this,SLOT(updatePort(int)));
    connect(ui->listEnvironment,SIGNAL(currentIndexChanged(QString)),
            this,SLOT(changeEnvironment(QString)));
    connect(ui->btnSave,&QPushButton::clicked,this,&MainWindow::saveSettings);

    daemon = new CServer(true, this);

    if (!daemon->isListening())
        QMessageBox::critical(0,tr("AtlasTCPSvc-NG"),
                              tr("Failed to start ATLAS server. "
                                 "Maybe port is in use, or ATLAS engine initialization error"));

    initializeUiParams();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initializeUiParams()
{
    if (!atlasServer.isLoaded()) {
        QMessageBox::critical(0,tr("AtlasTCPSvc-NG"),
                              tr("ATLAS engine failed to load. "
                                 "Unable to initialize parameters."));
        ui->btnSave->setEnabled(false);
        return;
    }

    ui->spinPort->setValue(daemon->atlasPort());
    QStringList env = atlasServer.getEnvironments();
    ui->listEnvironment->addItems(env);
    if (env.contains(daemon->atlasEnv()))
        ui->listEnvironment->setCurrentIndex(env.indexOf(daemon->atlasEnv()));
    ui->listTokens->addItems(daemon->clientTokens());

    showSSLParams();
}

void MainWindow::showSSLParams()
{
    QSslKey key = daemon->privateKey();
    if (key.isNull())
        ui->lblKey->setText(tr("<empty>"));
    else {
        int bits = key.length();
        switch (key.algorithm()) {
            case QSsl::Opaque: ui->lblKey->setText(tr("OPAQUE, %1 bits").arg(bits)); break;
            case QSsl::Rsa: ui->lblKey->setText(tr("RSA, %1 bits").arg(bits)); break;
            case QSsl::Dsa: ui->lblKey->setText(tr("DSA, %1 bits").arg(bits)); break;
            case QSsl::Ec: ui->lblKey->setText(tr("EC-key, %1 bits").arg(bits)); break;
        }
    }

    QSslCertificate cert = daemon->serverCert();
    if (cert.isNull())
        ui->lblCert->setText(tr("<empty>"));
    else {
        QStringList sl = cert.subjectInfo(QSslCertificate::CommonName);
        if (sl.isEmpty())
            ui->lblCert->setText(tr("<some certificate>"));
        else
            ui->lblCert->setText(sl.first());
    }
}

void MainWindow::addToken()
{
    bool ok;
    QString s = QInputDialog::getText(this,tr("AtlasTCPSvc-NG"),
                                      tr("Add new authentication token"),
                                      QLineEdit::Normal,QString(),&ok);
    if (ok && !s.isEmpty()) {
        daemon->addToken(s);
        ui->listTokens->clear();
        ui->listTokens->addItems(daemon->clientTokens());
    }
}

void MainWindow::delToken()
{
    QListWidgetItem *itm = ui->listTokens->currentItem();
    if (itm==NULL || itm->text().isEmpty()) return;

    daemon->deleteToken(itm->text());
    ui->listTokens->clear();
    ui->listTokens->addItems(daemon->clientTokens());
}

void MainWindow::loadKey()
{
    QString fname = QFileDialog::getOpenFileName(this,tr("OpenSSL server private key"),QString(),
                                                 tr("Private RSA key file, PEM format (*.key)"));
    if (fname.isEmpty()) return;

    QFile f(fname);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this,tr("AtlasTCPSvc-NG"),
                              tr("Unable to open key file."));
        return;
    }

    QSslKey key(&f,QSsl::Rsa);
    f.close();
    if (key.isNull()) {
        QMessageBox::critical(this,tr("AtlasTCPSvc-NG"),
                              tr("Unable to parse key file. Private RSA key in PEM format needed."));
        return;
    }

    daemon->setPrivateKey(key);
    showSSLParams();
}

void MainWindow::loadCert()
{
    QString fname = QFileDialog::getOpenFileName(this,tr("OpenSSL server certificate"),QString(),
                                                 tr("Server certificate, PEM format (*.crt)"));
    if (fname.isEmpty()) return;

    QFile f(fname);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this,tr("AtlasTCPSvc-NG"),
                              tr("Unable to open certificate file."));
        return;
    }

    QSslCertificate cert(&f);
    f.close();
    if (cert.isNull()) {
        QMessageBox::critical(this,tr("AtlasTCPSvc-NG"),
                              tr("Unable to parse certificate file. "
                                 "Certificate in PEM format needed."));
        return;
    }

    daemon->setServerCert(cert);
    showSSLParams();
}

void MainWindow::clearKey()
{
    daemon->setPrivateKey(QSslKey());
    showSSLParams();
}

void MainWindow::clearCert()
{
    daemon->setServerCert(QSslCertificate());
    showSSLParams();
}

void MainWindow::updatePort(int port)
{
    daemon->setAtlasPort(port);
}

void MainWindow::changeEnvironment(const QString &env)
{
    if (!env.isEmpty())
        daemon->setAtlasEnv(env);
}

void MainWindow::saveSettings()
{
    if (!daemon->saveSettings())
        QMessageBox::critical(this,tr("AtlasTCPSvc-NG"),
                              tr("Unable to save service configuration. "
                                 "Make sure you have administrator or local system privileges."));
    else
        QMessageBox::information(this,tr("AtlasTCPSvc-NG"),
                                 tr("Service settings saved."
                                    "You can now restart server."));
}