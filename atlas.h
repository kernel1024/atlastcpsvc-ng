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
#include <QDir>
#include <QMutex>
#include <windows.h>

// ATLAS API
// dir is 1 for jap to eng, 2 for eng to jap.
using CreateEngineType = int __cdecl (int x, int dir, int x3, char* x4);
using DestroyEngineType = int __cdecl ();
using TranslatePairType = int __cdecl (char* in, char **out, void **dunno, unsigned int *maybeSize);
using AtlInitEngineDataType = int __cdecl (int x1, int x2, int *x3, int x4, int *x5);
using FreeAtlasDataType = int __cdecl (void *mem, void *noSureHowManyArgs, void *, void *);

class CAtlas : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(CAtlas)
public:
    enum AtlasDirection {
        Atlas_Auto = 0,
        Atlas_JE = 1,
        Atlas_EJ = 2
    };
    explicit CAtlas(QObject *parent = nullptr);
    ~CAtlas() override;

    // returns 0 if not initialized.
    int getTransDirection() const;

    bool init(AtlasDirection transDirection, const QString &environment, bool forceDirectionChange = false);
    void uninit();
    bool loadDLLs();

    bool isLoaded() const;
    bool isDLLsLoaded() const;
    int getVersion() const;

    QString translate(AtlasDirection transDirection, const QString &str);
    QStringList getEnvironments();

    bool haveJapanese(const QString &str);
private:
    bool m_atlasHappy { false };
    int m_atlasVersion { 0 };
    AtlasDirection m_atlasTransDirection { Atlas_JE };
    AtlasDirection m_internalDirection { Atlas_JE };

    HMODULE h_atlecont { nullptr };
    HMODULE h_awdict { nullptr };
    HMODULE h_awuenv { nullptr };

    CreateEngineType *CreateEngine { nullptr };
    DestroyEngineType *DestroyEngine { nullptr };
    TranslatePairType *TranslatePair { nullptr };
    AtlInitEngineDataType *AtlInitEngineData { nullptr };
    FreeAtlasDataType *FreeAtlasData { nullptr };

    QString m_environment;
    QDir m_atlasDir;
    QMutex m_atlasMutex;
    QMutex m_atlasInitMutex;
};

#endif // ATLAS_H
