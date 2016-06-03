/*
 *
 * Based on Translation Aggregator code (v 0.4.9)
 * (c) Setx, ScumSuckingPig and others from hongfire
 *
 * modified by kernel1024
 *
 */

#include <atlas.h>
#include <QTextCodec>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QMutex>

CAtlasServer atlasServer;
QMutex atlasMutex;

QByteArray toSJIS(const QString &str)
{
    QTextCodec *codec = QTextCodec::codecForName("SJIS");
    if (!codec)
        qFatal("SJIS not supported");
    QTextEncoder *encoderWithoutBom = codec->makeEncoder( QTextCodec::IgnoreHeader );
    QByteArray ba  = encoderWithoutBom ->fromUnicode( str );
    ba.append('\0'); ba.append('\0');
    return ba;
}

QString fromSJIS(const QByteArray &str)
{
    QTextCodec *codec = QTextCodec::codecForName("SJIS");
    if (!codec)
        qFatal("SJIS not supported");
    QTextDecoder *decoderWithoutBom = codec->makeDecoder( QTextCodec::IgnoreHeader );
    QString s  = decoderWithoutBom->toUnicode( str );
    return s;
}

// ATLAS API
// dir is 1 for jap to eng, 2 for eng to jap.
typedef int __cdecl CreateEngineType(int x, int dir, int x3, char* x4);
static CreateEngineType *CreateEngine = 0;

typedef int __cdecl DestroyEngineType();
static DestroyEngineType *DestroyEngine = 0;

typedef int __cdecl TranslatePairType(char* in, char **out, void **dunno, unsigned int *maybeSize);
static TranslatePairType *TranslatePair = 0;

typedef int __cdecl AtlInitEngineDataType(int x1, int x2, int *x3, int x4, int *x5);
static AtlInitEngineDataType *AtlInitEngineData = 0;

// No clue what this does.  Doesn't set direction.
//typedef int __cdecl SetTransStateType(int dunno);
//static SetTransStateType *SetTransState = 0;

typedef int __cdecl ChangeDirectionType(int dir);
static ChangeDirectionType *ChangeDirection = 0;

typedef int __cdecl FreeAtlasDataType(void *mem, void *noSureHowManyArgs, void *, void *);
static FreeAtlasDataType *FreeAtlasData = 0;

//typedef int __cdecl AwuWordDelType(int x1, char *type, int x3, char *word);
//static AwuWordDelType *AwuWordDel = 0;

//typedef int __cdecl AwuDlgAtlasPopupEnvDetailSetType(void *, char *type, void *x3, char *word);
//AwuDlgAtlasPopupEnvDetailSetType *AwuDlgAtlasPopupEnvDetailSet = 0;

int CAtlasServer::getVersion()
{
    return m_atlasVersion;
}

CAtlasServer::CAtlasServer()
    : QObject(NULL)
{
    m_atlasVersion = 0;
    m_atlasTransDirection = Atlas_JE;
    m_atlasHappy = false;

    h_atlecont = 0;
    h_awdict = 0;
    h_awuenv = 0;
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

int CAtlasServer::getTransDirection()
{
    return m_atlasTransDirection;
}

void CAtlasServer::uninit()
{
    m_atlasTransDirection = Atlas_JE;
    m_atlasVersion = 0;
    if (isLoaded())
		DestroyEngine();

    if (h_atlecont)
	{
        FreeLibrary(h_atlecont);
        h_atlecont = 0;
	}
    if (h_awdict)
	{
        FreeLibrary(h_awdict);
        h_awdict = 0;
	}
    if (h_awuenv)
	{
        FreeLibrary(h_awuenv);
        h_awuenv = 0;
	}
    m_atlasHappy = false;
}

QString CAtlasServer::translate(AtlasDirection transDirection, const QString &str)
{
    if (!isLoaded()) return QString("ERR");

	char *outjis = 0;
	void *unsure = 0;
	unsigned int maybeSize = 0;
    char *temp = toSJIS(str).data();

    atlasMutex.lock();
    if (m_atlasTransDirection != transDirection) {
        m_atlasTransDirection = transDirection;
        ChangeDirection(m_atlasTransDirection);
    }

    // I completely ignore return value.  Not sure if it matters.
    TranslatePair(temp, &outjis, &unsure, &maybeSize);
    atlasMutex.unlock();

	if (unsure)
		FreeAtlasData(unsure,0,0,0);

	if (outjis)
	{
        QString res = fromSJIS(QByteArray::fromRawData(outjis,strlen(outjis)));
		FreeAtlasData(outjis,0,0,0);
        return res;
	}
    return QString("ERR");
}

QStringList CAtlasServer::getEnvironments()
{
    QStringList res;
    if (!isLoaded()) return res;

    QFile f(QDir(atlasPath).absoluteFilePath("transenv.ini"));
    if (!f.open(QIODevice::ReadOnly)) return res;
    QTextStream fs(&f);
    fs.setCodec("SJIS");
    while (!fs.atEnd()) {
        QString s = fs.readLine();
        if (s.startsWith("EnvName")) {
            s.remove(0,s.indexOf('=')+1);
            s = s.trimmed();
            res.append(s);
        }
    }

    return res;
}

bool CAtlasServer::loadDLLs()
{
    if (h_atlecont && h_awdict && h_awuenv) return 1;
    wchar_t newPath[MAX_PATH*2];
    for (int v = 14; v>=13; v--)
    {
        wchar_t temp[MAX_PATH];
        wsprintfW(temp, L"Software\\Fujitsu\\ATLAS\\V%i.0\\EJ", v);
        HKEY hKey = 0;
        if (ERROR_SUCCESS != RegOpenKey(HKEY_CURRENT_USER, temp, &hKey))
            continue;

        DWORD type;
        DWORD size = sizeof(newPath)-2;
        wchar_t *name;
        int res = RegQueryValueExW(hKey, L"TRENV EJ", 0, &type, (BYTE*)newPath, &size);
        RegCloseKey(hKey);

        if (ERROR_SUCCESS != res || type != REG_SZ || !(name = wcsrchr(newPath, '\\')))
            continue;

        name[1] = 0;
        wchar_t *w = wcschr(newPath, 0);
        wcscpy(w, L"AtleCont.dll");
        h_atlecont = LoadLibraryEx(newPath, 0, LOAD_WITH_ALTERED_SEARCH_PATH);
        wcscpy(w, L"awdict.dll");
        h_awdict = LoadLibraryEx(newPath, 0, LOAD_WITH_ALTERED_SEARCH_PATH);
        wcscpy(w, L"awuenv.dll");
        h_awuenv = LoadLibraryEx(newPath, 0, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (h_atlecont && h_awdict && h_awuenv)
        {
            *w = 0;
            atlasPath = QString::fromWCharArray(newPath);
            m_atlasVersion = v;
            return true;
        }
        uninit();
    }
    return false;
}

bool CAtlasServer::isLoaded()
{
    return m_atlasHappy;
}

bool CAtlasServer::init(AtlasDirection transDirection, const QString& environment)
{
    if (isLoaded())
    {
        if (m_atlasTransDirection == transDirection)
            return true;
        uninit();
    }
    if (!loadDLLs()) return false;

    if (h_atlecont && h_awdict &&
            (CreateEngine = (CreateEngineType*) GetProcAddress(h_atlecont, "CreateEngine")) &&
            (DestroyEngine = (DestroyEngineType*) GetProcAddress(h_atlecont, "DestroyEngine")) &&
            (TranslatePair = (TranslatePairType*) GetProcAddress(h_atlecont, "TranslatePair")) &&
            (FreeAtlasData = (FreeAtlasDataType*) GetProcAddress(h_atlecont, "FreeAtlasData")) &&
            (AtlInitEngineData = (AtlInitEngineDataType*) GetProcAddress(h_atlecont, "AtlInitEngineData")) &&
            (ChangeDirection = (ChangeDirectionType*) GetProcAddress(h_atlecont, "ChangeDirection"))
//            (SetTransState = (SetTransStateType*) GetProcAddress(h_atlecont, "SetTransState")) &&
//            (AwuDlgAtlasPopupEnvDetailSet = (AwuDlgAtlasPopupEnvDetailSetType*)(h_awuenv, "AwuDlgAtlasPopupEnvDetailSet")) &&
//            (AwuWordDel = (AwuWordDelType*) GetProcAddress(h_awdict, "AwuWordDel"))
            )
    {
        m_environment = environment;
        // SetTransState(1);
        // Needed?  No clue.
        static int dunno[1000] = {0};
        static int dunno2[1000] = {0};
        if (0 == AtlInitEngineData(0, 2, dunno, 0, dunno2) &&
                1 == CreateEngine(1, (int)transDirection, 0, toSJIS(m_environment).data()))
        {
            m_atlasTransDirection = transDirection;
            m_atlasHappy = true;
            return true;
        }
    }
    uninit();
    return false;
}
