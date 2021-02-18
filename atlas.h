/*
 *
 * Based on Translation Aggregator code (v 0.4.9)
 * (c) Setx, ScumSuckingPig and others from hongfire
 *
 * modified by kernel1024
 *
 */

#ifndef ATLAS_H
#define ATLAS_H

#include <QObject>
#include <QLibrary>

class CAtlasServer : public QObject
{
    Q_OBJECT

public:
    enum AtlasDirection {
        Atlas_Auto = 0,
        Atlas_JE = 1,
        Atlas_EJ = 2
    };
    explicit CAtlasServer(QObject *parent = nullptr);
    ~CAtlasServer() override;
    static CAtlasServer& instance();

    // returns 0 if not initialized.
    int getTransDirection() const;

    bool init(AtlasDirection transDirection, const QString &environment, bool forceDirectionChange = false);
    void uninit();
    bool loadDLLs();

    bool isLoaded() const;
    int getVersion() const;

    QString translate(AtlasDirection transDirection, const QString &str);
    QStringList getEnvironments();

    bool haveJapanese(const QString &str);
private:
    QString m_environment;
    QString atlasPath;
    int m_atlasVersion { 0 };
    AtlasDirection m_atlasTransDirection { Atlas_JE };
    AtlasDirection m_internalDirection { Atlas_JE };
    bool m_atlasHappy { false };

    QLibrary h_atlecont;
    QLibrary h_awdict;
    QLibrary h_awuenv;
};

extern CAtlasServer atlasServer;

#endif // ATLAS_H
