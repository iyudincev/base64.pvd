#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <gdiplus.h>

extern "C" {
#include "PictureViewPlugin.h"
}

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")

struct PvdContext {
    HGLOBAL           hGlobal;
    IStream*          pStream;
    Gdiplus::Bitmap*  pBitmap;

    PvdContext() : hGlobal(NULL), pStream(NULL), pBitmap(NULL) {}

    ~PvdContext() {
        if (pBitmap) delete pBitmap;
        if (pStream) pStream->Release();
    }
};

std::wstring Utf8ToWstring(const char* utf8Str) {
    if (!utf8Str) return L"";
    int wchars_num = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
    if (wchars_num <= 0) return L"";
    std::wstring wstr(wchars_num - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, &wstr[0], wchars_num);
    return wstr;
}

std::vector<BYTE> DecodeBase64(const char *buffer, size_t size) {
    static const signed char lookup[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };
    std::vector<BYTE> out;
    out.reserve(size * 3 / 4);
    int val = 0, valb = -8;
    for (size_t i = 0; i < size; i++) {
        BYTE c = static_cast<BYTE>(buffer[i]);
        if (lookup[c] < 0) continue;
        val = (val << 6) + lookup[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<BYTE>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::vector<BYTE> DecodeBase64(const std::string& base64_str) {
  return DecodeBase64(base64_str.data(), base64_str.length());
}

bool QuickCheckFormat(const BYTE *pBuf, size_t lBuf, const char **format) {
    const size_t N_CHARS_DECODE = 20;
    if (lBuf < N_CHARS_DECODE) return false;
    std::vector<BYTE> raw = DecodeBase64(reinterpret_cast<const char *>(pBuf), N_CHARS_DECODE);

    if (raw.size() < 4) return false;

    if (raw[0] == 0xFF && raw[1] == 0xD8 && raw[2] == 0xFF) {*format = "JPEG"; return true;}
    if (raw[0] == 0x89 && raw[1] == 0x50 && raw[2] == 0x4E && raw[3] == 0x47) {*format = "PNG"; return true;}
    if (raw[0] == 0x47 && raw[1] == 0x49 && raw[2] == 0x46 && raw[3] == 0x38) {*format = "GIF"; return true;}
    if (raw[0] == 0x42 && raw[1] == 0x4D) {*format = "BMP"; return true;}

    return false;
}

bool ExtractRawData(const std::wstring& filePath, std::vector<BYTE>& outData) {
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD size = GetFileSize(hFile, NULL);
    if (size == 0 || size == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return false;
    }

    std::string buffer(size, '\0');
    DWORD read = 0;
    if (!ReadFile(hFile, &buffer[0], size, &read, NULL) || read != size) {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);
    outData = DecodeBase64(buffer);
    return true;
}

ULONG_PTR g_GdiplusToken = 0;

UINT32 __stdcall pvdInit(void) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    if (Gdiplus::GdiplusStartup(&g_GdiplusToken, &gdiplusStartupInput, NULL) == Gdiplus::Ok) {
        return PVD_CURRENT_INTERFACE_VERSION;
    }
    return 0;
}

void __stdcall pvdExit(void) {
    if (g_GdiplusToken) {
        Gdiplus::GdiplusShutdown(g_GdiplusToken);
        g_GdiplusToken = 0;
    }
}

void __stdcall pvdPluginInfo(pvdInfoPlugin *pPluginInfo) {
    if (!pPluginInfo) return;
    pPluginInfo->Priority = 10;
    pPluginInfo->pName = "Base64-GDI+";
    pPluginInfo->pVersion = "1.0";
    pPluginInfo->pComments = "Decodes Base64 encoded images via GDI+";
}

BOOL __stdcall pvdFileOpen(const char *pFileName, INT64 lFileSize, const BYTE *pBuf, UINT32 lBuf, pvdInfoImage *pImageInfo, void **ppContext) {
    if (!pFileName || !ppContext || !pImageInfo) return FALSE;

    const char *format;
    if (!QuickCheckFormat(pBuf, lBuf, &format)) return FALSE;

    std::wstring wPath = Utf8ToWstring(pFileName);

    std::vector<BYTE> rawData;
    if (!ExtractRawData(wPath, rawData)) return FALSE;

    auto context = new (std::nothrow) PvdContext();
    if (!context) return FALSE;

    context->hGlobal = GlobalAlloc(GMEM_MOVEABLE, rawData.size());
    if (!context->hGlobal) { delete context; return FALSE; }

    void* pData = GlobalLock(context->hGlobal);
    if (!pData) { delete context; return FALSE; }
    memcpy(pData, rawData.data(), rawData.size());
    GlobalUnlock(context->hGlobal);

    if (CreateStreamOnHGlobal(context->hGlobal, TRUE, &context->pStream) == S_OK) {
        context->pBitmap = Gdiplus::Bitmap::FromStream(context->pStream);
        if (context->pBitmap && context->pBitmap->GetLastStatus() == Gdiplus::Ok) {
            UINT frameCount = 1;
            UINT dimensionsCount = context->pBitmap->GetFrameDimensionsCount();
            if (dimensionsCount > 0) {
                std::vector<GUID> dimensionIDs(dimensionsCount);
                if (context->pBitmap->GetFrameDimensionsList(dimensionIDs.data(), dimensionsCount) == Gdiplus::Ok) {
                    frameCount = context->pBitmap->GetFrameCount(&Gdiplus::FrameDimensionTime);
                    if (frameCount == 0) {
                        frameCount = context->pBitmap->GetFrameCount(&Gdiplus::FrameDimensionPage);
                    }
                    if (frameCount == 0) {
                        frameCount = 1;
                    }
                }
            }

            pImageInfo->nPages = frameCount;
            pImageInfo->Flags = frameCount > 1 ? PVD_IIF_ANIMATED : 0;
            pImageInfo->pFormatName = format;
            pImageInfo->pCompression = nullptr;
            pImageInfo->pComments = nullptr;

            *ppContext = reinterpret_cast<void*>(context);
            return TRUE;
        }
    }

    delete context;
    return FALSE;
}

void __stdcall pvdFileClose(void *pContext) {
    if (pContext) {
        auto context = reinterpret_cast<PvdContext*>(pContext);
        delete context;
    }
}

BOOL __stdcall pvdPageInfo(void *pContext, UINT32 iPage, pvdInfoPage *pPageInfo) {
    if (!pContext || !pPageInfo) return FALSE;
    auto context = reinterpret_cast<PvdContext*>(pContext);
    if (!context->pBitmap) return FALSE;

    if (context->pBitmap->SelectActiveFrame(&Gdiplus::FrameDimensionTime, iPage) != Gdiplus::Ok) {
        if (context->pBitmap->SelectActiveFrame(&Gdiplus::FrameDimensionPage, iPage) != Gdiplus::Ok) {
            return FALSE;
        }
    }

    pPageInfo->lWidth = context->pBitmap->GetWidth();
    pPageInfo->lHeight = context->pBitmap->GetHeight();
    pPageInfo->nBPP = 24;
    pPageInfo->lFrameTime = 0; // Значение по умолчанию (если статичное изображение)

    // Читаем задержку кадра из GIF метаданных
    UINT size = context->pBitmap->GetPropertyItemSize(PropertyTagFrameDelay);
    if (size > 0) {
        std::vector<BYTE> buffer(size);
        Gdiplus::PropertyItem* pItem = reinterpret_cast<Gdiplus::PropertyItem*>(buffer.data());

        if (context->pBitmap->GetPropertyItem(PropertyTagFrameDelay, size, pItem) == Gdiplus::Ok) {
            // PropertyTagFrameDelay содержит массив задержек для каждого кадра (тип PropertyTagTypeLong)
            // Каждое значение — это длительность в 1/100 секунды
            long* pDelays = reinterpret_cast<long*>(pItem->value);

            // Безопасно проверяем, чтобы индекс не вышел за границы выделенного массива свойств
            UINT maxFramesInProp = pItem->length / sizeof(long);
            if (iPage < maxFramesInProp) {
                // Переводим из 1/100 секунды в миллисекунды
                pPageInfo->lFrameTime = pDelays[iPage] * 10;
            }
        }
    }

    // Если задержка нулевая или слишком маленькая (бывает в битых GIF), задаем стандартные 100 мс
    if (pPageInfo->lFrameTime <= 0) {
        pPageInfo->lFrameTime = 100;
    }

    return TRUE;
}

BOOL __stdcall pvdPageDecode(void *pContext, UINT32 iPage, pvdInfoDecode *pDecodeInfo, pvdDecodeCallback DecodeCallback, void *pDecodeCallbackContext) {
    if (!pContext || !pDecodeInfo) return FALSE;
    auto context = reinterpret_cast<PvdContext*>(pContext);
    if (!context->pBitmap) return FALSE;

    if (context->pBitmap->SelectActiveFrame(&Gdiplus::FrameDimensionTime, iPage) != Gdiplus::Ok) {
        if (context->pBitmap->SelectActiveFrame(&Gdiplus::FrameDimensionPage, iPage) != Gdiplus::Ok) {
            return FALSE;
        }
    }

    UINT width = context->pBitmap->GetWidth();
    UINT height = context->pBitmap->GetHeight();
    INT32 pitch24 = (width * 3 + 3) & ~3;

    BYTE* pPixelBuffer = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, pitch24 * height));
    if (!pPixelBuffer) return FALSE;

    Gdiplus::Rect rect(0, 0, width, height);
    Gdiplus::BitmapData bmpData;

    Gdiplus::Status status = context->pBitmap->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat24bppRGB, &bmpData);
    if (status == Gdiplus::Ok) {
        for (UINT y = 0; y < height; ++y) {
            BYTE* pSrcRow = reinterpret_cast<BYTE*>(bmpData.Scan0) + (y * bmpData.Stride);
            BYTE* pDstRow = pPixelBuffer + ((height - 1 - y) * pitch24);
            memcpy(pDstRow, pSrcRow, width * 3);
        }
        context->pBitmap->UnlockBits(&bmpData);

        pDecodeInfo->pImage = pPixelBuffer;
        pDecodeInfo->pPalette = NULL;
        pDecodeInfo->Flags = 0;
        pDecodeInfo->nBPP = 24;
        pDecodeInfo->nColorsUsed = 0;
        pDecodeInfo->lImagePitch = -pitch24;
        return TRUE;
    }

    HeapFree(GetProcessHeap(), 0, pPixelBuffer);
    return FALSE;
}

void __stdcall pvdPageFree(void *pContext, pvdInfoDecode *pDecodeInfo) {
    if (pDecodeInfo && pDecodeInfo->pImage) {
        HeapFree(GetProcessHeap(), 0, pDecodeInfo->pImage);
        pDecodeInfo->pImage = NULL;
    }
}
