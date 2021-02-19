/*
 *
 * Based on Translation Aggregator code (v 0.4.9)
 * (c) Setx, ScumSuckingPig and others from hongfire
 *
 * modified by kernel1024
 *
 */

#include <QTextCodec>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QMutex>
#include <QSettings>
#include <QDebug>
#include <array>
#include <algorithm>

#include "atlas.h"
#include "qsl.h"

// TODO: move from globals to singleton

CAtlasServer atlasServer;
QMutex atlasMutex;
QMutex atlasInitMutex;

QByteArray toSJIS(const QString &str)
{
    QByteArray res("ERROR");
    QTextCodec *codec = QTextCodec::codecForName("SJIS");
    if (!codec) {
        qCritical() << "SJIS codec not supported.";
        return res;
    }
    QTextEncoder *encoderWithoutBom = codec->makeEncoder( QTextCodec::IgnoreHeader );
    res  = encoderWithoutBom ->fromUnicode( str );
    delete encoderWithoutBom;
    res.append('\0'); res.append('\0');
    return res;
}

QString fromSJIS(const QByteArray &str)
{
    QString res(QSL("ERROR"));
    QTextCodec *codec = QTextCodec::codecForName("SJIS");
    if (!codec) {
        qCritical() << "SJIS codec not supported.";
        return res;
    }
    QTextDecoder *decoderWithoutBom = codec->makeDecoder( QTextCodec::IgnoreHeader );
    res  = decoderWithoutBom->toUnicode( str );
    delete decoderWithoutBom;
    return res;
}

// ATLAS API
// dir is 1 for jap to eng, 2 for eng to jap.
//typedef int __cdecl CreateEngineType(int x, int dir, int x3, char* x4);
using CreateEngineType = int __cdecl (int x, int dir, int x3, char* x4);
static CreateEngineType *CreateEngine = nullptr;

//typedef int __cdecl DestroyEngineType();
using DestroyEngineType = int __cdecl ();
static DestroyEngineType *DestroyEngine = nullptr;

//typedef int __cdecl TranslatePairType(char* in, char **out, void **dunno, unsigned int *maybeSize);
using TranslatePairType = int __cdecl (char* in, char **out, void **dunno, unsigned int *maybeSize);
static TranslatePairType *TranslatePair = nullptr;

//typedef int __cdecl AtlInitEngineDataType(int x1, int x2, int *x3, int x4, int *x5);
using AtlInitEngineDataType = int __cdecl (int x1, int x2, int *x3, int x4, int *x5);
static AtlInitEngineDataType *AtlInitEngineData = nullptr;

using FreeAtlasDataType = int __cdecl (void *mem, void *noSureHowManyArgs, void *, void *);
static FreeAtlasDataType *FreeAtlasData = nullptr;

int CAtlasServer::getVersion() const
{
    return m_atlasVersion;
}

CAtlasServer::CAtlasServer(QObject *parent)
    : QObject(parent)
{
}

CAtlasServer::~CAtlasServer()
{
    if (isLoaded())
        uninit();
}

CAtlasServer &CAtlasServer::instance()
{
    static CAtlasServer instance;
    return instance;
}

int CAtlasServer::getTransDirection() const
{
    return m_atlasTransDirection;
}

void CAtlasServer::uninit()
{
    atlasInitMutex.lock();

    m_atlasTransDirection = Atlas_JE;
    m_internalDirection = Atlas_JE;
    m_atlasVersion = 0;
    if (isLoaded())
		DestroyEngine();

    if (h_atlecont) {
        FreeLibrary(h_atlecont);
        h_atlecont = nullptr;
    }

    if (h_awdict) {
        FreeLibrary(h_awdict);
        h_awdict = nullptr;
    }

    if (h_awuenv) {
        FreeLibrary(h_awuenv);
        h_awuenv = nullptr;
    }

    m_atlasHappy = false;

    atlasInitMutex.unlock();
}

bool CAtlasServer::haveJapanese(const QString &str)
{
    return std::any_of(str.constBegin(),str.constEnd(),[](QChar c){
        constexpr ushort lowHiragana = 0x3040;
        constexpr ushort highHiragana = 0x309f;
        constexpr ushort lowKatakana = 0x30a0;
        constexpr ushort highKatakana = 0x30ff;
        constexpr ushort lowCJK = 0x4e00;
        constexpr ushort highCJK = 0x9fff;

        ushort u = c.unicode();
        if (u>=lowHiragana && u<=highHiragana) return true; // hiragana
        if (u>=lowKatakana && u<=highKatakana) return true; // katakana
        if (u>=lowCJK && u<=highCJK) return true; // CJK
        return false;
    });
}

QString CAtlasServer::translate(AtlasDirection transDirection, const QString &str)
{
    QString res = QSL("ERR");

    if (!isLoaded()) return res;

    QMutexLocker locker(&atlasMutex);

    AtlasDirection od = m_internalDirection;
    if (m_atlasTransDirection != transDirection) {
        m_atlasTransDirection = transDirection;

        if (m_atlasTransDirection==Atlas_Auto) {
            if (haveJapanese(str)) {
                m_internalDirection=Atlas_JE;
            } else {
                m_internalDirection=Atlas_EJ;
            }
        } else {
            m_internalDirection=m_atlasTransDirection;
        }

    } else if (m_atlasTransDirection==Atlas_Auto) {
        if (m_internalDirection==Atlas_JE && !haveJapanese(str)) {
            m_internalDirection=Atlas_EJ;
        } else if (m_internalDirection==Atlas_EJ && haveJapanese(str)) {
            m_internalDirection=Atlas_JE;
        }
    }

    if (od != m_internalDirection) {
        if (!init(m_internalDirection, m_environment, true)) {
            qCritical() << "Unable to reinitialize ATLAS on translation direction change.";
            return res;
        }
    }

    char *outjis = nullptr; // NOLINT
    void *unsure = nullptr;
    unsigned int maybeSize = 0U;
    QByteArray injis = toSJIS(str);
    auto *temp = injis.data();

    try {
        // I completely ignore return value.  Not sure if it matters.
        TranslatePair(temp, &outjis, &unsure, &maybeSize);

        res = fromSJIS(QByteArray::fromRawData(outjis,strlen(outjis)));
        FreeAtlasData(outjis,nullptr,nullptr,nullptr);

        if (unsure)
            FreeAtlasData(unsure,nullptr,nullptr,nullptr);
    } catch (const std::exception& ex) {
        qCritical() << "Translate exception handled: " << ex.what();
        res = QSL("ERR");
    }

    return res;
}

QStringList CAtlasServer::getEnvironments()
{
    QStringList res;
    if (!isLoaded()) return res;

    QFile f(QDir(atlasPath).absoluteFilePath(QSL("transenv.ini")));
    if (!f.open(QIODevice::ReadOnly)) return res;
    QTextStream fs(&f);
    fs.setCodec("SJIS");
    while (!fs.atEnd()) {
        QString s = fs.readLine();
        if (s.startsWith(QSL("EnvName"))) {
            s.remove(0,s.indexOf('=')+1);
            s = s.trimmed();
            res.append(s);
        }
    }

    return res;
}

bool CAtlasServer::loadDLLs()
{
    if (isDLLsLoaded())
        return true;

    constexpr int atlasVersionLow = 13;
    constexpr int atlasVersionHigh = 14;

    for (int v = atlasVersionHigh; v>=atlasVersionLow; v--) {
        QSettings registry(QSL("HKEY_CURRENT_USER\\Software\\Fujitsu\\ATLAS\\V%1.0\\EJ").arg(v),QSettings::NativeFormat);
        const QString newPath = registry.value(QSL("TRENV EJ")).toString();
        if (newPath.isEmpty()) continue;
        const QDir atlasDir = QFileInfo(newPath).dir();
        if (!atlasDir.isAbsolute() || !atlasDir.isReadable()) continue;
        std::wstring w_atlecont = QDir::toNativeSeparators(atlasDir.filePath(QSL("AtleCont.dll"))).toStdWString();
        std::wstring w_awdict = QDir::toNativeSeparators(atlasDir.filePath(QSL("awdict.dll"))).toStdWString();
        std::wstring w_awuenv = QDir::toNativeSeparators(atlasDir.filePath(QSL("awuenv.dll"))).toStdWString();

        h_atlecont = LoadLibraryExW(w_atlecont.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        h_awdict = LoadLibraryExW(w_awdict.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        h_awuenv = LoadLibraryExW(w_awuenv.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);

        if (isDLLsLoaded()) {
            atlasPath = newPath;
            m_atlasVersion = v;
            return true;
        }
        uninit();
    }
    return false;
}

bool CAtlasServer::isLoaded() const
{
    return m_atlasHappy;
}

bool CAtlasServer::isDLLsLoaded() const
{
    return (h_atlecont != nullptr) &&
            (h_awdict != nullptr) &&
            (h_awuenv != nullptr);
}

bool CAtlasServer::init(AtlasDirection transDirection, const QString& environment, bool forceDirectionChange)
{
    AtlasDirection md = m_atlasTransDirection;

    if (isLoaded())
        uninit();

    if (!loadDLLs()) return false;

    atlasInitMutex.lock();

    if (isDLLsLoaded() &&
            (CreateEngine = reinterpret_cast<CreateEngineType *>(GetProcAddress(h_atlecont,"CreateEngine"))) &&
            (DestroyEngine = reinterpret_cast<DestroyEngineType *>(GetProcAddress(h_atlecont,"DestroyEngine"))) &&
            (TranslatePair = reinterpret_cast<TranslatePairType *>(GetProcAddress(h_atlecont,"TranslatePair"))) &&
            (FreeAtlasData = reinterpret_cast<FreeAtlasDataType *>(GetProcAddress(h_atlecont,"FreeAtlasData"))) &&
            (AtlInitEngineData = reinterpret_cast<AtlInitEngineDataType *>(GetProcAddress(h_atlecont,"AtlInitEngineData")))
            )
    {
        m_environment = environment;
        try {
            const int dunnoSize = 1000;
            static std::array<int,dunnoSize> dunno1 = {0};
            static std::array<int,dunnoSize> dunno2 = {0};
            QByteArray env = toSJIS(m_environment);

            if (0 == AtlInitEngineData(0, 2, dunno1.data(), 0, dunno2.data()) &&
                    1 == CreateEngine(1, static_cast<int>(transDirection), 0, env.data()))
            {
                if (forceDirectionChange) {
                    m_atlasTransDirection = md;
                } else {
                    m_atlasTransDirection = transDirection;
                }
                m_internalDirection = transDirection;
                m_atlasHappy = true;
                atlasInitMutex.unlock();
                return true;
            }
        } catch (const std::exception& ex) {
            qCritical() << "ATLAS initialization exception handled: " << ex.what();
        }
    }
    atlasInitMutex.unlock();
    uninit();
    return false;
}
