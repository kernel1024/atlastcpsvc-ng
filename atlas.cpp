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
#include <QDebug>
#include <windows.h>
#include <array>
#include <algorithm>

#include "atlas.h"
#include "qsl.h"

CAtlasServer atlasServer;
QMutex atlasMutex;

std::wstring GetStringValueFromHKLM(HKEY hKey, const std::wstring& regSubKey, const std::wstring& regValue)
{
    size_t bufferSize = 0xFFF; // If too small, will be resized down below.
    std::wstring valueBuf; // Contiguous buffer since C++11.
    valueBuf.resize(bufferSize);
    auto cbData = static_cast<DWORD>(bufferSize * sizeof(wchar_t));
    auto rc = RegGetValueW(
        hKey,
        regSubKey.c_str(),
        regValue.c_str(),
        RRF_RT_REG_SZ,
        nullptr,
        static_cast<void*>(valueBuf.data()),
        &cbData
    );
    while (rc == ERROR_MORE_DATA)
    {
        // Get a buffer that is big enough.
        cbData /= sizeof(wchar_t);
        if (cbData > static_cast<DWORD>(bufferSize))
        {
            bufferSize = static_cast<size_t>(cbData);
        }
        else
        {
            bufferSize *= 2;
            cbData = static_cast<DWORD>(bufferSize * sizeof(wchar_t));
        }
        valueBuf.resize(bufferSize);
        rc = RegGetValueW(
            HKEY_LOCAL_MACHINE,
            regSubKey.c_str(),
            regValue.c_str(),
            RRF_RT_REG_SZ,
            nullptr,
            static_cast<void*>(valueBuf.data()),
            &cbData
        );
    }
    if (rc == ERROR_SUCCESS)
    {
        cbData /= sizeof(wchar_t);
        valueBuf.resize(static_cast<size_t>(cbData - 1)); // remove end null character
        return valueBuf;
    }
    else
    {
        throw std::runtime_error("Windows system error code: " + std::to_string(rc));
    }
}

QByteArray toSJIS(const QString &str)
{
    QByteArray res("ERROR");
    QTextCodec *codec = QTextCodec::codecForName("SJIS");
    if (!codec)
        return res;
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
    if (!codec)
        return res;
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
    m_atlasTransDirection = Atlas_JE;
    m_internalDirection = Atlas_JE;
    m_atlasVersion = 0;
    if (isLoaded())
		DestroyEngine();

    if (h_atlecont.isLoaded())
        h_atlecont.unload();

    if (h_awdict.isLoaded())
        h_awdict.unload();

    if (h_awuenv.isLoaded())
        h_awuenv.unload();

    m_atlasHappy = false;
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
    if (!isLoaded()) return QSL("ERR");

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

    if (od!=m_internalDirection) {
        qDebug() << "Change direction to " << m_internalDirection;
        if (!init(m_internalDirection, m_environment, true))
            return QSL("ERR");
    }

    char *outjis = nullptr;
    void *unsure = nullptr;
    unsigned int maybeSize = 0U;
    QByteArray injis = toSJIS(str);
    auto *temp = injis.data();

    // I completely ignore return value.  Not sure if it matters.
    TranslatePair(temp, &outjis, &unsure, &maybeSize);

	if (unsure)
		FreeAtlasData(unsure,nullptr,nullptr,nullptr);

    if (outjis) {
        QString res = fromSJIS(QByteArray::fromRawData(outjis,strlen(outjis)));
		FreeAtlasData(outjis,nullptr,nullptr,nullptr);
        return res;
	}
    return QSL("ERR");
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
    if (h_atlecont.isLoaded() && h_awdict.isLoaded() && h_awuenv.isLoaded())
        return true;

    constexpr int atlasVersionLow = 13;
    constexpr int atlasVersionHigh = 14;

    for (int v = atlasVersionHigh; v>=atlasVersionLow; v--) {
        std::wstring buf = QSL("Software\\Fujitsu\\ATLAS\\V%1.0\\EJ").arg(v).toStdWString();
        HKEY hKey = nullptr;
        if (ERROR_SUCCESS != RegOpenKeyW(HKEY_CURRENT_USER, buf.data(), &hKey))
            continue;

        buf.assign(MAX_PATH*2, 0);
        DWORD type = REG_NONE;
        DWORD size = (buf.length()-1) * sizeof(wchar_t);
        RegGetValueW(hKey,()
        int res = RegQueryValueExW(hKey, L"TRENV EJ", nullptr, &type, reinterpret_cast<BYTE*>(buf.data()), &size);
        RegCloseKey(hKey);

        if (ERROR_SUCCESS != res || type != REG_SZ)
            continue;

        QString newPath = QString::fromWCharArray(buf);
        if (!newPath.contains('\\'))
            continue;
        h_atlecont.setFileName(QSL("%1\\AtleCont.dll"));
        h_atlecont.load();
        h_awdict.setFileName(QSL("%1\\awdict.dll"));
        h_awdict.load();
        h_awuenv.setFileName(QSL("%1\\awuenv.dll"));
        h_awuenv.load();
        if (h_atlecont.isLoaded() && h_awdict.isLoaded() && h_awuenv.isLoaded())
        {
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

bool CAtlasServer::init(AtlasDirection transDirection, const QString& environment, bool forceDirectionChange)
{
    AtlasDirection md = m_atlasTransDirection;

    if (isLoaded())
        uninit();

    if (!loadDLLs()) return false;

    if (h_atlecont.isLoaded() &&
            (CreateEngine = reinterpret_cast<CreateEngineType *>(h_atlecont.resolve("CreateEngine"))) &&
            (DestroyEngine = reinterpret_cast<DestroyEngineType *>(h_atlecont.resolve("DestroyEngine"))) &&
            (TranslatePair = reinterpret_cast<TranslatePairType *>(h_atlecont.resolve("TranslatePair"))) &&
            (FreeAtlasData = reinterpret_cast<FreeAtlasDataType *>(h_atlecont.resolve("FreeAtlasData"))) &&
            (AtlInitEngineData = reinterpret_cast<AtlInitEngineDataType *>(h_atlecont.resolve("AtlInitEngineData")))
            )
    {
        m_environment = environment;
        const int dunnoSize = 1000;
        static std::array<int,dunnoSize> dunno1 = {0};
        static std::array<int,dunnoSize> dunno2 = {0};
        if (0 == AtlInitEngineData(0, 2, dunno1.data(), 0, dunno2.data()) &&
                1 == CreateEngine(1, static_cast<int>(transDirection), 0, toSJIS(m_environment).data()))
        {
            if (forceDirectionChange) {
                m_atlasTransDirection = md;
            } else {
                m_atlasTransDirection = transDirection;
            }
            m_internalDirection = transDirection;
            m_atlasHappy = true;
            return true;
        }
    }
    uninit();
    return false;
}
