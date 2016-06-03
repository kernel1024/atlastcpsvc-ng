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
#include <windows.h>

class CAtlasServer : public QObject
{
    Q_OBJECT

public:
    enum AtlasDirection {
        Atlas_Auto = 0,
        Atlas_JE = 1,
        Atlas_EJ = 2
    };
    CAtlasServer();
    virtual ~CAtlasServer();
    static CAtlasServer& instance();

    // returns 0 if not initialized.
    int getTransDirection();

    bool init(AtlasDirection transDirection, const QString &environment, bool forceDirectionChange = false);
    void uninit();
    bool loadDLLs();

    bool isLoaded();
    int getVersion();

    QString translate(AtlasDirection transDirection, const QString &str);
    QStringList getEnvironments();

    bool haveJapanese(const QString &str);
private:
    QString m_environment;
    QString atlasPath;
    int m_atlasVersion;
    AtlasDirection m_atlasTransDirection;
    AtlasDirection m_internalDirection;
    bool m_atlasHappy;

    HMODULE h_atlecont;
    HMODULE h_awdict;
    HMODULE h_awuenv;
};

extern CAtlasServer atlasServer;

#endif // ATLAS_H
