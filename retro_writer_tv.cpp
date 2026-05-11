#define Uses_TApplication
#define Uses_TProgram
#define Uses_TBackground
#define Uses_TButton
#define Uses_TColorSelector
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TDrawBuffer
#define Uses_TEditWindow
#define Uses_TFileEditor
#define Uses_TIndicator
#define Uses_TObject
#define Uses_TScrollBar
#define Uses_TEvent
#define Uses_TFrame
#define Uses_TInputLine
#define Uses_TKeys
#define Uses_TCheckBoxes
#define Uses_TLabel
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TPalette
#define Uses_TRadioButtons
#define Uses_TRect
#define Uses_TStaticText
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TSubMenu
#define Uses_TText
#define Uses_TSItem
#define Uses_TWindow
#define Uses_TWindowInit
#define Uses_MsgBox
#define Uses_TFileDialog
#define Uses_TFileList
#define Uses_TMenu
#define Uses_TMenuPopup
#define Uses_THardwareInfo
#define Uses_TScreen
#include <tvision/tv.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <climits>
#include <cmath>
#include <iterator>
#include <map>
#include <functional>
#include <tuple>
#include <vector>

#include <tvision/internal/conctl.h>
#include <tvision/util.h>
#ifndef _WIN32
#include <sys/ioctl.h>
#include <tvision/hardware.h>
#endif

#ifdef HAVE_LIBSIXEL
extern "C" {
#include <sixel.h>
}
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "ia_writer.hpp"

namespace fs = std::filesystem;

extern TColorAttr shadowAttr;

namespace {

/*
 * Mapa rapido del archivo (para mantenimiento):
 * 1) Utilidades base: color, rutas, imagenes y soporte Kitty/Sixel.
 * 2) Views reutilizables: botones/labels/listas/previews personalizadas.
 * 3) Editor: RetroFileEditor + RetroEditWindow (scroll, margenes, indicador).
 * 4) Dialogos visuales: Escena visual y Agregar elemento visual.
 * 5) App principal: RetroWriterTVApp (eventos, layout, persistencia).
 * 6) Explorador de imagen: browseImageFileAbs con NavigatorListView + preview.
 * 7) main(): bootstrap de proyecto y arranque de la aplicacion.
 */

const int cmRefreshWidgets = 1009;
const int cmPreferences = 1010;
const int cmFgMatrixChanged = 1011;
const int cmBgMatrixChanged = 1012;
const int cmSymbolMatrixChanged = 1013;
const int cmNavSelect = 1014;
const int cmToggleFilePanel = 1016;
const int cmCreateFolder = 1015;
const int cmCreateTxtFile = 1017;
const int cmAsciiFontCursorChanged = 1038;
const int cmAsciiOpenPreview = 1039;
const int cmNavFilePopup = 1040;
const int cmNavMoveTrash = 1041;
/** Base de comandos para popupMenu de ids de personaje (rwPickCharacterIdFromList). */
const int cmPickCharPopupBase = 12000;
const int cmReadabilityHelp = 1022;
/** Reacomodo de panel/Mini tras cmScreenChanged: el comando se encola para handleEvent (fuera del stack de getEvent). */
const int cmDeferredTerminalLayout = 1023;
const int cmVisualScene = 1024;
const int cmVisualSaveDefaults = 1025;
const int cmVisualClearChapter = 1026;
const int cmVisualLibrary = 1027;
const int cmVisualLibAdd = 1028;
const int cmVisualGallery = 1033;
const int cmVisualGalPrevId = 1034;
const int cmVisualGalNextId = 1035;
const int cmVisualGalPrevVar = 1036;
const int cmVisualGalNextVar = 1037;
const int cmVisualSceneBgPrevId = 1038;
const int cmVisualSceneBgNextId = 1039;
const int cmVisualSceneBgPrevVar = 1040;
const int cmVisualSceneBgNextVar = 1041;
const int cmVisualSceneC2PrevId = 1042;
const int cmVisualSceneC2NextId = 1043;
const int cmVisualSceneC2PrevVar = 1044;
const int cmVisualSceneC2NextVar = 1045;
const int cmVisualSceneC3PrevId = 1046;
const int cmVisualSceneC3NextId = 1047;
const int cmVisualSceneC3PrevVar = 1048;
const int cmVisualSceneC3NextVar = 1049;
const int cmVisualSceneBgPickId = 1050;
const int cmVisualSceneBgPickVar = 1051;
const int cmVisualSceneC2PickId = 1052;
const int cmVisualSceneC2PickVar = 1053;
const int cmVisualSceneC3PickId = 1054;
const int cmVisualSceneC3PickVar = 1055;
const int cmVisualBrowseCharPath = 1056;
const int cmVisualBrowseBgPath = 1057;
/** Lista de ids de personaje sin cerrar el dialogo Biblioteca visual (evBroadcast). */
const int cmVisualPickCharInline = 1058;
const int cmVisualSceneDelBg = 1059;
const int cmVisualSceneDelC2 = 1060;
const int cmVisualSceneDelC3 = 1061;
const int cmVisualSceneRefresh = 1062;
const int cmLayoutSaveSlot = 1063;
const int cmLayoutRestoreSlot = 1064;
const int cmVisualSceneOpenLibrary = 1065;
const int cmImageNavCursorChanged = 1066;
/** Panel archivo: al cambiar ancho/alto del listado, recalcular titulo truncado y franja. */
const int cmNavPanelLayoutChanged = 1067;
const int cmIaSolicitar = 1068;
const int cmCrearConIA = 1069;
const int cmEntregarIA = 1070;
const int cmIaPedirComentario = 1071;
const int cmIaElegirUbicacion = 1072;
const int cmIaConfig = 1073;
const int cmIaResumen = 1074;

static constexpr int kMaxPreviewImagePath = 480;

/** RGB 0..255 a indice xterm 216-color cube (16..231). */
static unsigned char rgbToXterm256(unsigned r, unsigned g, unsigned b) {
    unsigned ri = (r * 6) / 256;
    unsigned gi = (g * 6) / 256;
    unsigned bi = (b * 6) / 256;
    if (ri > 5) ri = 5;
    if (gi > 5) gi = 5;
    if (bi > 5) bi = 5;
    return (unsigned char)(16 + 36 * ri + 6 * gi + bi);
}

/** Promedia RGB en [x0,x1) x [y0,y1) dentro de img WxH RGB interleaved. */
static void avgRgbRect(const std::vector<uint8_t> &rgb, int iw, int ih, int x0, int x1, int y0, int y1, unsigned &outR,
                       unsigned &outG, unsigned &outB) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > iw) x1 = iw;
    if (y1 > ih) y1 = ih;
    if (x1 <= x0 || y1 <= y0) {
        outR = outG = outB = 0;
        return;
    }
    unsigned long sr = 0, sg = 0, sb = 0;
    unsigned cnt = 0;
    for (int iy = y0; iy < y1; ++iy)
        for (int ix = x0; ix < x1; ++ix) {
            size_t off = ((size_t)iy * (size_t)iw + (size_t)ix) * 3;
            if (off + 2 < rgb.size()) {
                sr += rgb[off];
                sg += rgb[off + 1];
                sb += rgb[off + 2];
                ++cnt;
            }
        }
    if (!cnt) cnt = 1;
    outR = (unsigned)(sr / cnt);
    outG = (unsigned)(sg / cnt);
    outB = (unsigned)(sb / cnt);
}

/** Media RGB con supersampling (rejilla 3×3, 2×2 o promedio simple); el aliasing se atenúa cuando una celda cubre muchos píxeles. */
static void avgRgbRectSmooth(const std::vector<uint8_t> &rgb, int iw, int ih, int x0, int x1, int y0, int y1,
                             unsigned &outR, unsigned &outG, unsigned &outB) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > iw) x1 = iw;
    if (y1 > ih) y1 = ih;
    const int w = x1 - x0;
    const int h = y1 - y0;
    if (w <= 0 || h <= 0) {
        outR = outG = outB = 0;
        return;
    }
    const int gw = (w >= 9 && h >= 9) ? 3 : ((w >= 2 && h >= 2) ? 2 : 0);
    if (gw == 0) {
        avgRgbRect(rgb, iw, ih, x0, x1, y0, y1, outR, outG, outB);
        return;
    }
    unsigned long sr = 0, sg = 0, sb = 0;
    int n = 0;
    for (int gy = 0; gy < gw; ++gy) {
        const int ys0 = y0 + (gy * h) / gw;
        const int ys1 = std::max(ys0 + 1, y0 + ((gy + 1) * h) / gw);
        for (int gx = 0; gx < gw; ++gx) {
            const int xs0 = x0 + (gx * w) / gw;
            const int xs1 = std::max(xs0 + 1, x0 + ((gx + 1) * w) / gw);
            unsigned r, g, b;
            avgRgbRect(rgb, iw, ih, xs0, xs1, ys0, std::min(ys1, y1), r, g, b);
            sr += r;
            sg += g;
            sb += b;
            ++n;
        }
    }
    outR = (unsigned)(sr / n);
    outG = (unsigned)(sg / n);
    outB = (unsigned)(sb / n);
}

/** Muestreo bilineal RGB en coordenadas de imagen; U+2580 usa dos medias verticales por celda. */
static void sampleRgbBilinearAt(const std::vector<uint8_t> &rgb, int iw, int ih, float fx, float fy, unsigned &outR,
                                unsigned &outG, unsigned &outB) {
    if (iw <= 0 || ih <= 0 || rgb.size() < (size_t)iw * (size_t)ih * 3u) {
        outR = outG = outB = 0;
        return;
    }
    fx = std::clamp(fx, 0.0f, static_cast<float>(std::max(0, iw - 1)));
    fy = std::clamp(fy, 0.0f, static_cast<float>(std::max(0, ih - 1)));
    int x0 = static_cast<int>(std::floor(fx));
    int y0 = static_cast<int>(std::floor(fy));
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);
    x0 = std::clamp(x0, 0, iw - 1);
    y0 = std::clamp(y0, 0, ih - 1);
    const int x1 = std::min(x0 + 1, iw - 1);
    const int y1 = std::min(y0 + 1, ih - 1);
    auto sample = [&](int ix, int iy) -> const uint8_t * {
        return &rgb[((size_t)iy * (size_t)iw + (size_t)ix) * 3u];
    };
    const uint8_t *c00 = sample(x0, y0);
    const uint8_t *c10 = sample(x1, y0);
    const uint8_t *c01 = sample(x0, y1);
    const uint8_t *c11 = sample(x1, y1);
    const float w00 = (1 - tx) * (1 - ty);
    const float w10 = tx * (1 - ty);
    const float w01 = (1 - tx) * ty;
    const float w11 = tx * ty;
    outR = (unsigned)std::clamp((int)std::lround(w00 * c00[0] + w10 * c10[0] + w01 * c01[0] + w11 * c11[0]), 0, 255);
    outG = (unsigned)std::clamp((int)std::lround(w00 * c00[1] + w10 * c10[1] + w01 * c01[1] + w11 * c11[1]), 0, 255);
    outB = (unsigned)std::clamp((int)std::lround(w00 * c00[2] + w10 * c10[2] + w01 * c01[2] + w11 * c11[2]), 0, 255);
}

/** Límite del lado mayor al cargar mini (0 = sin reducir); valores muy bajos pierden detalle en pocas celdas. */
static constexpr int kDefaultMiniPreviewMaxSide = 0;

/** Reduce RGB in-place para que max(w,h) sea a lo sumo maxSide (0 = no escalar). */
static void downscaleRgbImageForMini(std::vector<uint8_t> &rgb, int &w, int &h, int maxSide) {
    if (w <= 0 || h <= 0 || (size_t)w * (size_t)h * 3u > rgb.size())
        return;
    if (maxSide <= 0)
        return;
    if (std::max(w, h) <= maxSide)
        return;
    const int maxDim = std::max(w, h);
    const int nw = std::max(1, (w * maxSide + maxDim - 1) / maxDim);
    const int nh = std::max(1, (h * maxSide + maxDim - 1) / maxDim);
    if (nw >= w && nh >= h)
        return;
    std::vector<uint8_t> out((size_t)nw * (size_t)nh * 3u);
    for (int oy = 0; oy < nh; ++oy) {
        const int y0 = oy * h / nh;
        const int y1 = std::max(y0 + 1, (oy + 1) * h / nh);
        for (int ox = 0; ox < nw; ++ox) {
            const int x0 = ox * w / nw;
            const int x1 = std::max(x0 + 1, (ox + 1) * w / nw);
            unsigned r = 0, g = 0, b = 0;
            avgRgbRect(rgb, w, h, x0, x1, y0, y1, r, g, b);
            const size_t off = ((size_t)oy * (size_t)nw + (size_t)ox) * 3u;
            out[off] = (uint8_t)r;
            out[off + 1] = (uint8_t)g;
            out[off + 2] = (uint8_t)b;
        }
    }
    rgb.swap(out);
    w = nw;
    h = nh;
}

/** U+2580 mitad superior (fg arriba, bg abajo) — UTF-8. */
static constexpr char kUpperHalfBlockUtf8[] = "\xe2\x96\x80";

/** Reescala RGB intercalado a dw×dh (bilineal, sw,sh > 0, dst no vacio). */
static void resampleRgbBilinear(const std::vector<uint8_t> &src, int sw, int sh, std::vector<uint8_t> &dst, int dw,
                                int dh) {
    dst.assign(static_cast<size_t>(dw) * static_cast<size_t>(dh) * 3u, 0);
    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0)
        return;
    const float sxFac = static_cast<float>(sw) / static_cast<float>(dw);
    const float syFac = static_cast<float>(sh) / static_cast<float>(dh);
    for (int dy = 0; dy < dh; ++dy) {
        for (int dx = 0; dx < dw; ++dx) {
            float fx = (dx + 0.5f) * sxFac - 0.5f;
            float fy = (dy + 0.5f) * syFac - 0.5f;
            int x0 = static_cast<int>(std::floor(fx));
            int y0 = static_cast<int>(std::floor(fy));
            const float tx = fx - static_cast<float>(x0);
            const float ty = fy - static_cast<float>(y0);
            x0 = std::clamp(x0, 0, sw - 1);
            y0 = std::clamp(y0, 0, sh - 1);
            const int x1 = std::min(x0 + 1, sw - 1);
            const int y1 = std::min(y0 + 1, sh - 1);
            auto sample = [&](int ix, int iy) -> const uint8_t * {
                return &src[((size_t)iy * (size_t)sw + (size_t)ix) * 3u];
            };
            const uint8_t *c00 = sample(x0, y0);
            const uint8_t *c10 = sample(x1, y0);
            const uint8_t *c01 = sample(x0, y1);
            const uint8_t *c11 = sample(x1, y1);
            const size_t o = ((size_t)dy * (size_t)dw + (size_t)dx) * 3u;
            for (int c = 0; c < 3; ++c) {
                const float v = (1 - tx) * (1 - ty) * c00[c] + tx * (1 - ty) * c10[c] + (1 - tx) * ty * c01[c] +
                                tx * ty * c11[c];
                dst[o + (size_t)c] = static_cast<uint8_t>(std::clamp((int)std::lround(v), 0, 255));
            }
        }
    }
}

/**
 * Cover centrado (object-fit: cover): escala sw×sh para cubrir dw×dh, recorte centrado.
 * Devuelve tamano reescalado nw×nh y origen (ox,oy) del recorte de tamano dw×dh dentro del escalado.
 */
static void miniPhotoCoverRect(int sw, int sh, int dw, int dh, int &nw, int &nh, int &ox, int &oy) {
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
        nw = nh = ox = oy = 0;
        return;
    }
    const double s =
        std::max(static_cast<double>(dw) / static_cast<double>(sw), static_cast<double>(dh) / static_cast<double>(sh));
    nw = std::max(1, (int)std::ceil(static_cast<double>(sw) * s - 1e-12));
    nh = std::max(1, (int)std::ceil(static_cast<double>(sh) * s - 1e-12));
    nw = std::max(nw, dw);
    nh = std::max(nh, dh);
    ox = (nw - dw) / 2;
    oy = (nh - dh) / 2;
}

/** Rellena dst dw×dh con la imagen fuente escalada en cover y centrada (sin bandas). */
static void compositeRgbCenteredCover(const std::vector<uint8_t> &src, int sw, int sh, std::vector<uint8_t> &dst, int dw,
                                      int dh) {
    dst.assign(static_cast<size_t>(dw) * static_cast<size_t>(dh) * 3u, 0);
    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0 || src.size() < (size_t)sw * (size_t)sh * 3u)
        return;
    int nw, nh, ox, oy;
    miniPhotoCoverRect(sw, sh, dw, dh, nw, nh, ox, oy);
    if (nw <= 0 || nh <= 0)
        return;
    std::vector<uint8_t> patch;
    resampleRgbBilinear(src, sw, sh, patch, nw, nh);
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            const size_t si = ((size_t)(oy + y) * (size_t)nw + (size_t)(ox + x)) * 3u;
            const size_t di = ((size_t)y * (size_t)dw + (size_t)x) * 3u;
            dst[di] = patch[si];
            dst[di + 1] = patch[si + 1];
            dst[di + 2] = patch[si + 2];
        }
    }
}

/**
 * Pixeles del rectangulo sx×sy celdas con el mismo ratio que Kitty (ws_xpixel/ws_col en float).
 * Evita bandas: Turbo Vision usaba entero (ws_xpixel/ws_col) y alteraba el aspecto vs grman_put_cell_image.
 */
static void miniPreviewGridToPixels(int sx, int sy, int maxDim, int &tw, int &th) {
    sx = std::max(1, sx);
    sy = std::max(1, sy);
#if !defined(_WIN32)
    struct winsize w {};
    const int fd = tvision::ConsoleCtl::getInstance().out();
    const int tvCols = std::max(1, (int)TScreen::screenWidth);
    const int tvRows = std::max(1, (int)TScreen::screenHeight);
    if (ioctl(fd, TIOCGWINSZ, &w) == 0 && tvCols > 0 && tvRows > 0 && w.ws_xpixel > 0 && w.ws_ypixel > 0) {
        const double cellW = static_cast<double>(w.ws_xpixel) / static_cast<double>(tvCols);
        const double cellH = static_cast<double>(w.ws_ypixel) / static_cast<double>(tvRows);
        const double boxWp = static_cast<double>(sx) * cellW;
        const double boxHp = static_cast<double>(sy) * cellH;
        tw = std::max(1, (int)std::lround(boxWp));
        th = std::max(1, (int)std::lround(static_cast<double>(tw) * boxHp / boxWp));
    } else
#endif
    {
        TPoint fs = tvision::ConsoleCtl::getInstance().getFontSize();
        int fw = std::max(1, (int)fs.x);
        int fh = std::max(1, (int)fs.y);
        if (fs.x <= 0 || fs.y <= 0) {
            fw = 8;
            fh = 16;
        }
        tw = std::max(1, sx * fw);
        th = std::max(1, sy * fh);
    }
    if (maxDim > 0 && (tw > maxDim || th > maxDim)) {
        const double dx = static_cast<double>(maxDim) / static_cast<double>(tw);
        const double dy = static_cast<double>(maxDim) / static_cast<double>(th);
        const double sc = std::min(1.0, std::min(dx, dy));
        tw = std::max(1, (int)std::floor(static_cast<double>(tw) * sc));
        th = std::max(1, (int)std::floor(static_cast<double>(th) * sc));
    }
}

static uint64_t rgbFingerprint64(const std::vector<uint8_t> &rgb) noexcept {
    const size_t n = rgb.size();
    uint64_t h = 1469598103934665603ULL; // FNV-1a
    auto mix = [&](uint8_t b) {
        h ^= uint64_t(b);
        h *= 1099511628211ULL;
    };
    mix(uint8_t(n & 0xFF));
    mix(uint8_t((n >> 8) & 0xFF));
    mix(uint8_t((n >> 16) & 0xFF));
    mix(uint8_t((n >> 24) & 0xFF));
    if (n == 0)
        return h;
    const size_t step = std::max<size_t>(1, n / 64);
    for (size_t i = 0; i < n; i += step)
        mix(rgb[i]);
    mix(rgb[n - 1]);
    return h;
}

#ifndef _WIN32
#include "kitty_placeholder_diacritics.inc"

static void utf8AppendCodepoint(std::string &s, uint32_t cp) noexcept {
    if (cp <= 0x7fu) {
        s.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7ffu) {
        s.push_back(static_cast<char>(0xc0u | ((cp >> 6) & 0x1fu)));
        s.push_back(static_cast<char>(0x80u | (cp & 0x3fu)));
    } else if (cp <= 0xffffu) {
        s.push_back(static_cast<char>(0xe0u | ((cp >> 12) & 0x0fu)));
        s.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3fu)));
        s.push_back(static_cast<char>(0x80u | (cp & 0x3fu)));
    } else {
        s.push_back(static_cast<char>(0xf0u | ((cp >> 18) & 0x07u)));
        s.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3fu)));
        s.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3fu)));
        s.push_back(static_cast<char>(0x80u | (cp & 0x3fu)));
    }
}

/** Base64 de buffer RGB crudo (Kitty f=100). */
static void base64EncodeRgb(const uint8_t *data, size_t len, std::string &out) {
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    out.clear();
    out.reserve((len + 2u) / 3u * 4u);
    size_t i = 0;
    while (i + 3u <= len) {
        const uint32_t v = (uint32_t)data[i] << 16 | (uint32_t)data[i + 1u] << 8 | (uint32_t)data[i + 2u];
        out.push_back(tbl[(v >> 18) & 63u]);
        out.push_back(tbl[(v >> 12) & 63u]);
        out.push_back(tbl[(v >> 6) & 63u]);
        out.push_back(tbl[v & 63u]);
        i += 3u;
    }
    const size_t rem = len - i;
    if (rem == 1u) {
        const uint32_t v = (uint32_t)data[i] << 16;
        out.push_back(tbl[(v >> 18) & 63u]);
        out.push_back(tbl[(v >> 12) & 63u]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2u) {
        const uint32_t v = (uint32_t)data[i] << 16 | (uint32_t)data[i + 1u] << 8;
        out.push_back(tbl[(v >> 18) & 63u]);
        out.push_back(tbl[(v >> 12) & 63u]);
        out.push_back(tbl[(v >> 6) & 63u]);
        out.push_back('=');
    }
}

static bool terminalIsKitty() noexcept {
    if (const char *v = std::getenv("KITTY_WINDOW_ID"))
        if (v && v[0] != '\0')
            return true;
    /* TERM suele ser xterm-kitty aunque falte KITTY_WINDOW_ID (tmux, wrappers). */
    if (const char *term = std::getenv("TERM"))
        if (std::strcmp(term, "xterm-kitty") == 0)
            return true;
    return false;
}

#if !defined(_WIN32)
/** Persistencia del zoom (altura de celda en px) en appearance.cfg; desactivar con RETRO_WRITER_KITTY_SAVE_ZOOM=0. */
static bool kittyZoomPersistenceEnabled() noexcept {
    const char *v = std::getenv("RETRO_WRITER_KITTY_SAVE_ZOOM");
    if (!v || v[0] == '\0')
        return true;
    return !(v[0] == '0' && v[1] == '\0');
}

static int kittyTermCellHeightPx() noexcept {
    struct winsize w {};
    for (int fd : {STDOUT_FILENO, STDERR_FILENO, STDIN_FILENO}) {
        if (ioctl(fd, TIOCGWINSZ, &w) == 0 && w.ws_row > 0 && w.ws_ypixel > 0)
            return (int)(w.ws_ypixel / (unsigned)w.ws_row);
    }
    return 0;
}

/**
 * Kitty no expone el tamaño en pt al TUI; se usa altura de celda (TIOCGWINSZ) y se aproxima con kitten @ set-font-size ±1
 * (requiere allow_remote_control en kitty.conf).
 */
static void tryAdjustKittyFontTowardCellHeight(int targetPx) noexcept {
    if (targetPx <= 0 || !terminalIsKitty())
        return;
    for (int iter = 0; iter < 56; ++iter) {
        const int cur = kittyTermCellHeightPx();
        if (cur <= 0)
            break;
        if (std::abs(cur - targetPx) <= 1)
            break;
        const char *cmd = cur < targetPx ? "kitten @ set-font-size +1 >/dev/null 2>&1" : "kitten @ set-font-size -1 >/dev/null 2>&1";
        if (std::system(cmd) != 0)
            break;
        usleep(45000);
    }
}
#endif

/** En tmux/screen las secuencias APC suelen bloquearse; se prefiere U+2580 salvo RETRO_WRITER_KITTY_FORCE=1 y passthrough configurado. */
static bool envMultiplexerLikelyBlocksKittyGraphics() noexcept {
    return std::getenv("TMUX") != nullptr || std::getenv("STY") != nullptr;
}

static bool kittyUnicodePlaceholdersEnabled(bool kittyNativePref) noexcept {
    if (!kittyNativePref || !terminalIsKitty())
        return false;
    if (envMultiplexerLikelyBlocksKittyGraphics()) {
        if (const char *f = std::getenv("RETRO_WRITER_KITTY_FORCE"))
            return f[0] == '1' && f[1] == '\0';
        return false;
    }
    return true;
}

/** Ids Kitty 0..255 para las dos ventanas mini (no solapar con la paleta del editor). */
static constexpr unsigned kKittyMiniPreviewImageId = 241u;
static constexpr unsigned kKittyMiniPreviewImageId2 = 242u;
static constexpr unsigned kKittyMiniPreviewImageId3 = 243u;
/** Vista previa del dialogo Elegir imagen (no solapar con minis ni galeria 0xE2). */
static constexpr unsigned kKittyImagePickDialogId = 0xE3u;
/** Vistas previas del dialogo Agregar elemento visual (personaje/paisaje). */
static constexpr unsigned kKittyVisualLibImageId1 = 0xE4u;
static constexpr unsigned kKittyVisualLibImageId2 = 0xE5u;

/** Borrado Kitty por id (a=d,d=i,…). */
static void deleteKittyMiniImageById(unsigned imageId) {
    if (!terminalIsKitty())
        return;
    char buf[80];
    const int n = std::snprintf(buf, sizeof(buf), "\033_Ga=d,d=i,i=%u\033\\", imageId);
    if (n > 0)
        tvision::ConsoleCtl::getInstance().write(buf, static_cast<size_t>(n));
    THardwareInfo::forgetCaretPosition();
}

static void deleteKittyMiniPlacementOnly() {
    deleteKittyMiniImageById(kKittyMiniPreviewImageId);
    deleteKittyMiniImageById(kKittyMiniPreviewImageId2);
    deleteKittyMiniImageById(kKittyMiniPreviewImageId3);
}

/** a=t: transmisión RGB (f=24) sin pintar en cursor; a=p,U=1: placement virtual c×r (no mezclar con a=T). */
static void writeKittyRgbTransmitOnly(const uint8_t *rgb, int iw, int ih, unsigned imageId) {
    if (!rgb || iw <= 0 || ih <= 0)
        return;
    std::string b64;
    base64EncodeRgb(rgb, static_cast<size_t>(iw) * static_cast<size_t>(ih) * 3u, b64);
    if (b64.empty())
        return;
    tvision::ConsoleCtl &con = tvision::ConsoleCtl::getInstance();
    constexpr size_t kMaxChunk = 4096u;
    std::string first;
    first.reserve(512);
    first = "\033_Ga=t,q=2,i=";
    first += std::to_string(imageId);
    first += ",f=24,s=";
    first += std::to_string(static_cast<unsigned>(iw));
    first += ",v=";
    first += std::to_string(static_cast<unsigned>(ih));
    size_t off = 0;
    while (off < b64.size()) {
        size_t take = std::min(kMaxChunk, b64.size() - off);
        if (off + take < b64.size())
            take -= take % 4u;
        const bool more = off + take < b64.size();
        if (off == 0) {
            first += ",m=";
            first.push_back(more ? '1' : '0');
            first.push_back(';');
            con.write(first.data(), first.size());
            con.write(b64.data() + off, take);
            con.write("\033\\", 2u);
        } else {
            char hdr[24];
            const int hn =
                std::snprintf(hdr, sizeof(hdr), "\033_Gq=2,m=%c;", more ? '1' : '0');
            if (hn > 0)
                con.write(hdr, static_cast<size_t>(hn));
            con.write(b64.data() + off, take);
            con.write("\033\\", 2u);
        }
        off += take;
    }
    THardwareInfo::forgetCaretPosition();
}

static void writeKittyVirtualPlacementUnicode(unsigned imageId, int gridCols, int gridRows) {
    if (gridCols <= 0 || gridRows <= 0)
        return;
    char buf[96];
    const int n = std::snprintf(buf, sizeof(buf), "\033_Ga=p,U=1,q=2,i=%u,c=%d,r=%d\033\\", imageId, gridCols,
                                gridRows);
    if (n > 0)
        tvision::ConsoleCtl::getInstance().write(buf, static_cast<size_t>(n));
    THardwareInfo::forgetCaretPosition();
}
#endif /* !_WIN32 */

#ifdef HAVE_LIBSIXEL
static int appendSixelChunk(char *data, int size, void *priv) {
    auto *out = static_cast<std::string *>(priv);
    out->append(data, static_cast<size_t>(size));
    return 0;
}

static bool encodeRgbToSixel(const std::vector<uint8_t> &rgb, int w, int h, int paletteColors, std::string &out) {
    out.clear();
    if (w <= 0 || h <= 0 || rgb.size() < (size_t)w * (size_t)h * 3u)
        return false;
    sixel_dither_t *dither = nullptr;
    sixel_output_t *sout = nullptr;
    if (sixel_dither_new(&dither, paletteColors, nullptr) != SIXEL_OK)
        return false;
    if (sixel_dither_initialize(dither, const_cast<unsigned char *>(rgb.data()), w, h, SIXEL_PIXELFORMAT_RGB888,
                                LARGE_AUTO, REP_AUTO, QUALITY_HIGH) != SIXEL_OK) {
        sixel_dither_unref(dither);
        return false;
    }
    sixel_dither_set_diffusion_type(dither, DIFFUSE_FS);
    if (sixel_output_new(&sout, appendSixelChunk, &out, nullptr) != SIXEL_OK) {
        sixel_dither_unref(dither);
        return false;
    }
    const SIXELSTATUS st = sixel_encode(const_cast<unsigned char *>(rgb.data()), w, h, 3, dither, sout);
    sixel_output_unref(sout);
    sixel_dither_unref(dither);
    return st == SIXEL_OK && !out.empty();
}

/** Sixel solo con RETRO_WRITER_ALLOW_SIXEL / FORCE_SIXEL=1 y miniPreviewSixel; sin emulador compatible se vería basura en pantalla. */
static bool sixelOptInFromEnv() noexcept {
    if (const char *a = std::getenv("RETRO_WRITER_ALLOW_SIXEL"))
        if (a[0] == '1' && a[1] == '\0')
            return true;
    if (const char *f = std::getenv("RETRO_WRITER_FORCE_SIXEL"))
        if (f[0] == '1' && f[1] == '\0')
            return true;
    return false;
}

static void emitSixelAtViewOrigin(TView &view, const std::string &sixel) {
    if (sixel.empty())
        return;
    const TPoint g = view.makeGlobal({0, 0});
    char cup[64];
    const int n = std::snprintf(cup, sizeof(cup), "\033[%d;%dH", (int)g.y + 1, (int)g.x + 1);
    if (n <= 0)
        return;
    tvision::ConsoleCtl &con = tvision::ConsoleCtl::getInstance();
    con.write(cup, static_cast<size_t>(n));
    con.write(sixel.data(), sixel.size());
    THardwareInfo::forgetCaretPosition();
}
#endif

/** Columna derecha del panel y mini (58); el editor empieza en kFilePanelRightX+1. */
static const short kFilePanelRightX = 58;

/** Partición vertical panel/Mini: fila Y exclusiva del borde inferior del File Manager (coords de escritorio). */
static short computeDefaultPanelSplitY(short deskBottom, short minSplitY, short maxSplitY) {
    constexpr short kMinMiniRows = 3;
    short wantMiniRows = short(deskBottom - 8);
    wantMiniRows = std::clamp(wantMiniRows, kMinMiniRows, (short)26);
    short splitY = short(deskBottom - wantMiniRows);
    return std::clamp(splitY, minSplitY, maxSplitY);
}

static TRect rwDeskWorkArea(TDeskTop *dt) {
    TRect r = dt->getExtent();
    r.a.y = 2;
    r.b.x -= 1;
    r.b.y -= 1;
    return r;
}

/** Tamaño mínimo aplicado y rectángulo recortado al área útil wa (coords de escritorio). */
static TRect rwClampWindow(TRect r, const TRect &wa, int minW, int minH) {
    int w = (int)r.b.x - (int)r.a.x;
    int h = (int)r.b.y - (int)r.a.y;
    w = std::max(w, minW);
    h = std::max(h, minH);
    int maxAx = (int)wa.b.x - w;
    int maxAy = (int)wa.b.y - h;
    maxAx = std::max(maxAx, (int)wa.a.x);
    maxAy = std::max(maxAy, (int)wa.a.y);
    int ax = std::clamp((int)r.a.x, (int)wa.a.x, maxAx);
    int ay = std::clamp((int)r.a.y, (int)wa.a.y, maxAy);
    return TRect((short)ax, (short)ay, (short)(ax + w), (short)(ay + h));
}

static bool rwParseRect4(const std::string &val, TRect &out) {
    std::istringstream iss(val);
    int ax, ay, bx, by;
    if (!(iss >> ax >> ay >> bx >> by))
        return false;
    out = TRect((short)ax, (short)ay, (short)bx, (short)by);
    return (out.b.x > out.a.x) && (out.b.y > out.a.y);
}

const int MAX_TITLE = 96;

static std::string joinPath(const std::string &a, const std::string &b) {
    if (!a.empty() && (a.back() == '/' || a.back() == '\\')) return a + b;
    return a + "/" + b;
}

/** Avance UTF-8 seguro (1..4) para recorrer rutas con nombres no ASCII. */
static size_t pathUtf8Next(const char *p, size_t rem) noexcept {
    if (rem == 0 || !p || !p[0])
        return 0;
    unsigned char c = (unsigned char)*p;
    if (c < 0x80)
        return 1;
    if ((c & 0xE0) == 0xC0)
        return rem >= 2 ? 2 : 1;
    if ((c & 0xF0) == 0xE0)
        return rem >= 3 ? 3 : 1;
    if ((c & 0xF8) == 0xF0)
        return rem >= 4 ? 4 : 1;
    return 1;
}

/** Titulo con prefijo "..." y sufijo de ruta (final visible); ancho en celdas (strwidth). */
static std::string pathTitleForWidth(const std::string &path, int maxCols) {
    if (maxCols < 1)
        maxCols = 1;
    if (strwidth(TStringView(path.c_str(), path.size())) <= maxCols)
        return path;

    static const char ell[] = "...";
    const int ellw = strwidth(TStringView(ell));
    int budget = maxCols - ellw;

    if (budget < 1) {
        /* Sin hueco para "...": solo se recorta el sufijo. */
        for (size_t i = 0; i < path.size();) {
            TStringView suf(path.c_str() + i, path.size() - i);
            if (strwidth(suf) <= maxCols)
                return std::string(suf);
            size_t step = pathUtf8Next(path.c_str() + i, path.size() - i);
            i += step ? step : 1;
        }
        return std::string(ell);
    }

    for (size_t i = 0; i < path.size();) {
        TStringView suf(path.c_str() + i, path.size() - i);
        if (strwidth(suf) <= budget)
            return std::string(ell) + std::string(suf);
        size_t step = pathUtf8Next(path.c_str() + i, path.size() - i);
        if (step == 0)
            step = 1;
        i += step;
    }
    return std::string(ell);
}

/**
 * Texto de una fila del listado: si no cabe, se recorta por la izquierda y se antepone "..."
 * para que en columnas estrechas se lea mejor el final (nombre de archivo/carpeta).
 */
static std::string labelTailForWidth(const std::string &s, int maxCols) {
    if (maxCols < 1)
        maxCols = 1;
    if (strwidth(TStringView(s.c_str(), s.size())) <= maxCols)
        return s;

    static const char ell[] = "...";
    const int ellw = strwidth(TStringView(ell));
    int budget = maxCols - ellw;

    if (budget < 1) {
        for (size_t i = 0; i < s.size();) {
            TStringView suf(s.c_str() + i, s.size() - i);
            if (strwidth(suf) <= maxCols)
                return std::string(suf);
            size_t step = pathUtf8Next(s.c_str() + i, s.size() - i);
            i += step ? step : 1;
        }
        return std::string(ell);
    }

    for (size_t i = 0; i < s.size();) {
        TStringView suf(s.c_str() + i, s.size() - i);
        if (strwidth(suf) <= budget)
            return std::string(ell) + std::string(suf);
        size_t step = pathUtf8Next(s.c_str() + i, s.size() - i);
        if (step == 0)
            step = 1;
        i += step;
    }
    return std::string(ell);
}

/** Ruta absoluta si existe; si no, devuelve p sin cambios. */
static std::string absolutePath(const std::string &p) {
    char buf[PATH_MAX];
    if (realpath(p.c_str(), buf))
        return std::string(buf);
    return p;
}

static std::string trim(const std::string &s) {
    size_t i = 0;
    while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
    size_t j = s.size();
    while (j > i && std::isspace((unsigned char)s[j - 1])) j--;
    return s.substr(i, j - i);
}

static std::string utf8ToHex(const std::string &s) {
    std::ostringstream o;
    o << std::hex << std::setfill('0');
    for (unsigned char c : s)
        o << std::setw(2) << unsigned(c);
    return o.str();
}

static int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static std::string hexToUtf8(const std::string &hexIn) {
    std::string hex = trim(hexIn);
    std::string out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        int hi = hexNibble(hex[i]), lo = hexNibble(hex[i + 1]);
        if (hi < 0 || lo < 0) continue;
        out.push_back(static_cast<char>(static_cast<unsigned char>((hi << 4) | lo)));
    }
    return out;
}

/** Longitud de cpAppColor / cpAppBlackWhite en Turbo Vision (sin '\\0'). */
static constexpr ushort kAppPaletteLen = sizeof(cpAppColor) - 1;

/** Paleta explícita desde fg/bg (contraste); cpAppColor como plantilla y 31..126 fijados literal para diálogos. */
static void fillPaletteExplicit(ushort fg, ushort bg, std::vector<char> &out) {
    out.resize(kAppPaletteLen);
    auto toBios = [](ushort c) -> uchar {
        if (c <= 0x0F)
            return uchar(c & 0x0F);
        return uchar(XTerm256toXTerm16(uchar(c & 0xFF)) & 0x0F);
    };
    uchar baseFg = toBios(fg);
    uchar baseBg = toBios(bg);
    if (baseFg == baseBg)
        baseFg = uchar((baseFg + 8) & 0x0F); /* texto y fondo no idénticos */

    uchar fgBase3 = baseFg & 0x07;
    uchar bgBase3 = baseBg & 0x07;
    for (ushort i = 0; i < kAppPaletteLen; ++i) {
        uchar src = uchar(cpAppColor[i]);
        uchar srcFg = src & 0x0F;
        uchar srcBg = (src >> 4) & 0x0F;
        uchar outFg = uchar((srcFg & 0x08) | fgBase3);
        uchar outBg = uchar((srcBg & 0x08) | bgBase3);
        if ((outFg & 0x0F) == (outBg & 0x0F))
            outFg ^= 0x08;
        uchar attr = uchar((outBg << 4) | outFg);
        if (attr == 0x00)
            attr = uchar((bgBase3 << 4) | (fgBase3 ? fgBase3 : 0x07));
        out[i] = char(attr);
    }

    for (ushort i = 31; i <= 126 && i < kAppPaletteLen; ++i)
        out[i] = cpAppColor[i];

}

static ushort parseUShortClamped(const std::string &text, ushort minV, ushort maxV, ushort fallback) {
    std::string s = trim(text);
    if (s.empty()) return fallback;
    try {
        int v = std::stoi(s);
        if (v < minV) v = minV;
        if (v > maxV) v = maxV;
        return (ushort)v;
    } catch (...) {
        return fallback;
    }
}

static int parseIntClamped(const std::string &text, int minV, int maxV, int fallback) {
    std::string s = trim(text);
    if (s.empty()) return fallback;
    try {
        long v = std::stol(s);
        if (v < minV) v = minV;
        if (v > maxV) v = maxV;
        return (int)v;
    } catch (...) {
        return fallback;
    }
}

static char parsePatternChar(const std::string &text, char fallback) {
    std::string s = trim(text);
    if (s.empty()) return fallback;
    try {
        if (s.size() > 2 && (s[0] == '0') && (s[1] == 'x' || s[1] == 'X')) {
            int v = std::stoi(s, nullptr, 16);
            return char(v & 0xFF);
        }
        if (s.size() > 2 && s[0] == '\\' && (s[1] == 'x' || s[1] == 'X')) {
            int v = std::stoi(s.substr(2), nullptr, 16);
            return char(v & 0xFF);
        }
        bool isNum = true;
        for (size_t i = 0; i < s.size(); ++i) {
            if (i == 0 && (s[i] == '+' || s[i] == '-')) continue;
            if (!std::isdigit((unsigned char)s[i])) { isNum = false; break; }
        }
        if (isNum) {
            int v = std::stoi(s);
            return char(v & 0xFF);
        }
    } catch (...) {
        return fallback;
    }
    return s[0];
}

/** Biblioteca visual (fondos / personajes con varias imagenes) + defaults de novela. Ver visuals.cfg. */
struct RwVisualLibrary {
    std::map<std::string, std::vector<std::string>> backgrounds;
    std::map<std::string, std::vector<std::string>> characters;
    /** Si no esta vacio, bgImage/charImage son relativos a (carpeta de visuals.cfg)/<assetRoot>/ — estilo vault. */
    std::string assetRootSubpath;
    std::string defBgId;
    int defBgIdx {0};
    std::string defC2Id;
    int defC2Idx {0};
    std::string defC3Id;
    int defC3Idx {0};
    bool loaded {false};
};

/** Override opcional por capitulo (misma carpeta que el .txt del editor). */
struct RwChapterScene {
    bool hasBg {false};
    std::string bgId;
    int bgIdx {0};
    bool hasC2 {false};
    std::string c2Id;
    int c2Idx {0};
    bool hasC3 {false};
    std::string c3Id;
    int c3Idx {0};
};

/** Ruta absoluta de un recurso; relativo a baseDir, o a baseDir/assetRoot si assetRoot no esta vacio. */
static std::string rwResolveMediaPath(const std::string &baseDir, const std::string &relOrAbs,
                                      const std::string &assetRootSubpath = {}) {
    std::string t = trim(relOrAbs);
    if (t.empty())
        return {};
    fs::path p(t);
    if (p.is_absolute())
        return absolutePath(t);
    const std::string ar = trim(assetRootSubpath);
    if (!ar.empty())
        return absolutePath(joinPath(joinPath(baseDir, ar), t));
    return absolutePath(joinPath(baseDir, t));
}

/** Caracteres no validos como segmento de ruta bajo el vault. */
static std::string rwSanitizeIdForFs(std::string id) {
    id = trim(id);
    for (char &c : id) {
        if (c == '/' || c == '\\')
            c = '_';
    }
    return id;
}

static bool rwTryCanonicalRegularFile(const fs::path &candidate, fs::path &outCanon) {
    std::error_code ec;
    if (!fs::is_regular_file(candidate, ec))
        return false;
    outCanon = fs::weakly_canonical(candidate, ec);
    return !ec;
}

/** Resuelve una ruta de imagen que el usuario escribio (absoluta, relativa al cwd, al proyecto o al vault). */
static bool rwResolveUserImageFile(const std::string &userPath, const std::string &visualBaseDirAbs, const std::string &assetRoot,
                                   fs::path &outCanon) {
    const std::string t = trim(userPath);
    if (t.empty())
        return false;
    std::error_code ec;
    fs::path p(t);
    if (rwTryCanonicalRegularFile(p, outCanon))
        return true;
    if (!p.is_absolute()) {
        fs::path p2 = fs::path(visualBaseDirAbs) / p;
        if (rwTryCanonicalRegularFile(p2, outCanon))
            return true;
        const std::string ar = trim(assetRoot);
        if (!ar.empty()) {
            fs::path p3 = fs::path(visualBaseDirAbs) / ar / p;
            if (rwTryCanonicalRegularFile(p3, outCanon))
                return true;
        }
        fs::path p4 = fs::absolute(p, ec);
        if (rwTryCanonicalRegularFile(p4, outCanon))
            return true;
    }
    return false;
}

static bool rwFileIsInsideDir(const fs::path &fileCanon, const fs::path &dirCanon) {
    std::error_code ec;
    fs::path rel = fs::relative(fileCanon, dirCanon, ec);
    if (ec)
        return false;
    const std::string rs = rel.generic_string();
    return !rs.empty() && rs != "." && (rs.size() < 2 || rs.substr(0, 2) != "..");
}

static std::string rwUniqueFileNameInDir(const fs::path &destDir, const std::string &baseName) {
    std::error_code ec;
    if (!fs::exists(destDir / baseName, ec))
        return baseName;
    const fs::path bp(baseName);
    const std::string stem = bp.stem().string();
    const std::string ext = bp.extension().string();
    for (int n = 2; n < 100000; ++n) {
        const std::string s = stem + "_" + std::to_string(n) + ext;
        if (!fs::exists(destDir / s, ec))
            return s;
    }
    return baseName;
}

/**
 * Si el archivo ya esta bajo el vault, solo devuelve la ruta relativa al assetRoot (o al proyecto si assetRoot vacio).
 * Si esta fuera (p. ej. Documentos), lo copia a backgrounds/<id>/ o characters/<id>/ y devuelve esa ruta relativa.
 */
static bool rwImportOrNormalizeVaultImage(bool isBg, const std::string &idRaw, const std::string &userPath,
                                          const std::string &visualBaseDirAbs, const std::string &assetRootRaw,
                                          std::string &outRelForCfg, std::string &err) {
    err.clear();
    outRelForCfg.clear();
    const std::string id = trim(idRaw);
    const std::string idFs = rwSanitizeIdForFs(id);
    if (id.empty() || idFs.empty()) {
        err = "Id invalido.";
        return false;
    }
    const std::string ar = trim(assetRootRaw);
    fs::path vaultRoot = ar.empty() ? fs::path(visualBaseDirAbs) : fs::path(visualBaseDirAbs) / ar;
    std::error_code ec;
    fs::create_directories(vaultRoot, ec);
    {
        std::error_code ec2;
        fs::path canon = fs::weakly_canonical(vaultRoot, ec2);
        if (!ec2)
            vaultRoot = canon;
    }
    if (!fs::is_directory(vaultRoot)) {
        err = "No existe la carpeta vault; revisa assetRoot.";
        return false;
    }

    fs::path srcCanon;
    if (!rwResolveUserImageFile(userPath, visualBaseDirAbs, ar, srcCanon)) {
        err = "No se encontro el archivo. Usa ruta completa o arrastra la ruta al campo.";
        return false;
    }

    if (rwFileIsInsideDir(srcCanon, vaultRoot)) {
        fs::path rel = fs::relative(srcCanon, vaultRoot, ec);
        if (ec) {
            err = "No se pudo calcular ruta dentro del vault.";
            return false;
        }
        outRelForCfg = rel.generic_string();
        return true;
    }

    const char *sub = isBg ? "backgrounds" : "characters";
    fs::path destDir = vaultRoot / sub / idFs;
    fs::create_directories(destDir, ec);
    if (ec) {
        err = "No se pudo crear la carpeta de destino en el vault.";
        return false;
    }
    const std::string baseName = srcCanon.filename().string();
    const std::string unique = rwUniqueFileNameInDir(destDir, baseName);
    const fs::path destFile = destDir / unique;
    fs::copy_file(srcCanon, destFile, fs::copy_options::none, ec);
    if (ec) {
        err = "No se pudo copiar el archivo al vault.";
        return false;
    }
    fs::path relOut = fs::relative(destFile, vaultRoot, ec);
    if (ec) {
        err = "Error al calcular ruta relativa tras copiar.";
        return false;
    }
    outRelForCfg = relOut.generic_string();
    return true;
}

static bool rwLocateVisualsCfg(const std::string &projectDir, std::string &outFileAbs, std::string &outBaseDirAbs) {
    std::error_code ec;
    const std::string a = joinPath(projectDir, "visuals.cfg");
    if (fs::is_regular_file(fs::path(a), ec)) {
        outFileAbs = absolutePath(a);
        outBaseDirAbs = absolutePath(projectDir);
        return true;
    }
    const std::string parentDir = absolutePath(joinPath(projectDir, ".."));
    const std::string b = joinPath(parentDir, "visuals.cfg");
    if (fs::is_regular_file(fs::path(b), ec)) {
        outFileAbs = absolutePath(b);
        outBaseDirAbs = parentDir;
        return true;
    }
    return false;
}

static std::string rwLowerFileName(std::string s) {
    for (char &c : s)
        c = char(std::tolower((unsigned char)c));
    return s;
}

/** Si la ruta relativa no existe bajo baseDir, prueba capN/ y la raiz de novela (assets en cap vs en raiz). */
static std::string rwResolveMediaPathWithCapFallback(const std::string &baseDir, const std::string &relOrAbs,
                                                     const std::string &assetRootSubpath, const std::string &novelRootAbs) {
    const std::string first = rwResolveMediaPath(baseDir, relOrAbs, assetRootSubpath);
    std::string t = trim(relOrAbs);
    if (t.empty())
        return {};
    fs::path p(t);
    if (p.is_absolute())
        return first;
    std::error_code ec;
    if (!first.empty() && fs::is_regular_file(fs::path(first), ec))
        return first;
    const std::string nr = trim(novelRootAbs);
    if (!nr.empty()) {
        fs::path nrp(nr);
        if (fs::is_directory(nrp, ec)) {
            for (const auto &ent : fs::directory_iterator(nrp, ec)) {
                if (!ent.is_directory(ec))
                    continue;
                const std::string nm = rwLowerFileName(ent.path().filename().string());
                if (nm.size() < 3 || nm.substr(0, 3) != "cap")
                    continue;
                const std::string capBase = absolutePath(ent.path().string());
                const std::string alt = rwResolveMediaPath(capBase, relOrAbs, assetRootSubpath);
                if (!alt.empty() && fs::is_regular_file(fs::path(alt), ec))
                    return alt;
            }
        }
        const std::string atRoot = rwResolveMediaPath(nr, relOrAbs, assetRootSubpath);
        if (!atRoot.empty() && fs::is_regular_file(fs::path(atRoot), ec))
            return atRoot;
    }
    return first;
}

/** true si pathAbs es igual a dirAbs o un archivo/carpeta dentro de dirAbs. */
static bool rwPathIsUnderDirectory(const std::string &pathAbs, const std::string &dirAbs) {
    const std::string p = absolutePath(pathAbs);
    const std::string d = absolutePath(dirAbs);
    if (p.size() < d.size())
        return false;
    if (p == d)
        return true;
    if (p.compare(0, d.size(), d) != 0)
        return false;
    const char c = p[d.size()];
    return c == '/' || c == '\\';
}

/** Raiz de novela para visuals.cfg: padre de carpeta cap... o primer ancestro con subcarpeta assets/. */
static std::string rwInferNovelRootFromEditor(const std::string &editorAbsPath, const std::string &projectDirAbs) {
    std::error_code ec;
    fs::path file(editorAbsPath);
    if (!fs::exists(file, ec))
        return absolutePath(projectDirAbs);
    fs::path dir = file.has_parent_path() ? file.parent_path() : file;
    const std::string leaf = rwLowerFileName(dir.filename().string());
    if (leaf.size() >= 3 && leaf.substr(0, 3) == "cap" && dir.has_parent_path())
        return absolutePath(dir.parent_path().string());
    fs::path walk = dir;
    for (int i = 0; i < 12 && !walk.empty() && walk != walk.root_path(); ++i) {
        if (fs::is_directory(walk / "assets", ec))
            return absolutePath(walk.string());
        walk = walk.parent_path();
    }
    return absolutePath(dir.string());
}

/** true si el fichero contiene al menos una linea bgImage o charImage (no solo assetRoot / comentarios). */
static bool rwVisualsCfgHasAnyImageLine(const std::string &pathAbs) {
    std::ifstream in(pathAbs);
    if (!in)
        return false;
    std::string line;
    while (std::getline(in, line)) {
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#')
            continue;
        if (t.rfind("bgImage ", 0) == 0 || t.rfind("charImage ", 0) == 0)
            return true;
    }
    return false;
}

/** Busca visuals.cfg en novelRoot; si no hay, en hijos capN/visuals.cfg (legacy por capitulo). */
static std::string rwResolveVisualsCfgNearNovelRoot(const std::string &novelRootAbs, std::string &outBaseDirAbs) {
    outBaseDirAbs.clear();
    std::error_code ec;
    const std::string primary = absolutePath(joinPath(novelRootAbs, "visuals.cfg"));
    const bool primaryFile = fs::is_regular_file(fs::path(primary), ec);
    const bool primaryUsable = primaryFile && rwVisualsCfgHasAnyImageLine(primary);
    fs::path root(novelRootAbs);
    std::vector<std::pair<std::string, std::string>> capDirs;
    if (fs::is_directory(root, ec)) {
        for (const auto &ent : fs::directory_iterator(root, ec)) {
            if (!ent.is_directory(ec))
                continue;
            const std::string nm = rwLowerFileName(ent.path().filename().string());
            if (nm.size() < 3 || nm.substr(0, 3) != "cap")
                continue;
            const std::string cf = joinPath(ent.path().string(), "visuals.cfg");
            if (fs::is_regular_file(fs::path(cf), ec))
                capDirs.emplace_back(ent.path().filename().string(), absolutePath(ent.path().string()));
        }
    }
    std::sort(capDirs.begin(), capDirs.end());
    if (primaryUsable) {
        outBaseDirAbs = absolutePath(novelRootAbs);
        return primary;
    }
    for (const auto &pr : capDirs) {
        const std::string cf = joinPath(pr.second, "visuals.cfg");
        if (rwVisualsCfgHasAnyImageLine(cf)) {
            outBaseDirAbs = pr.second;
            return absolutePath(cf);
        }
    }
    if (!capDirs.empty()) {
        outBaseDirAbs = capDirs[0].second;
        return absolutePath(joinPath(outBaseDirAbs, "visuals.cfg"));
    }
    if (primaryFile) {
        outBaseDirAbs = absolutePath(novelRootAbs);
        return primary;
    }
    outBaseDirAbs = absolutePath(novelRootAbs);
    return primary;
}

static void rwRestOfLineAfterFirstToken(std::istringstream &iss, std::string &out) {
    std::string rest;
    std::getline(iss, rest);
    out = trim(rest);
}

/** true si el id debe ir entre comillas en visuals.cfg / chapter_scene.cfg (espacios, comillas, etc.). */
static bool rwCfgIdNeedsQuotes(const std::string &s) {
    if (s.empty())
        return true;
    for (unsigned char c : s) {
        if (c <= ' ' || c == '"' || c == '\\')
            return true;
    }
    return false;
}

static void rwWriteCfgId(std::ostream &out, const std::string &s) {
    if (!rwCfgIdNeedsQuotes(s)) {
        out << s;
        return;
    }
    out << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\')
            out << '\\' << (char)c;
        else
            out << (char)c;
    }
    out << '"';
}

static void rwSkipStreamWs(std::istream &iss) {
    for (;;) {
        const int c = iss.peek();
        if (c != ' ' && c != '\t')
            break;
        iss.get();
    }
}

/** Un token sin espacios o una cadena entre comillas (compat: sin comillas = una palabra, como antes). */
static bool rwReadCfgToken(std::istream &iss, std::string &out) {
    out.clear();
    rwSkipStreamWs(iss);
    const int c0 = iss.peek();
    if (c0 == EOF)
        return false;
    if (c0 == '"') {
        iss.get();
        for (;;) {
            const int c = iss.get();
            if (c == EOF)
                return false;
            if (c == '"')
                break;
            if (c == '\\') {
                const int c2 = iss.get();
                if (c2 == EOF)
                    return false;
                if (c2 == '"' || c2 == '\\')
                    out += (char)c2;
                else {
                    out += '\\';
                    out += (char)c2;
                }
            } else
                out += (char)c;
        }
        return true;
    }
    return !!(iss >> out);
}

static void rwParseVisualsLine(const std::string &lineIn, RwVisualLibrary &lib) {
    std::string line = trim(lineIn);
    if (line.empty() || line[0] == '#')
        return;
    std::istringstream iss(line);
    std::string cmd;
    if (!(iss >> cmd))
        return;
    if (cmd == "assetRoot") {
        std::string rest;
        rwRestOfLineAfterFirstToken(iss, rest);
        lib.assetRootSubpath = rest;
    } else if (cmd == "bgImage") {
        std::string id;
        if (!rwReadCfgToken(iss, id))
            return;
        std::string path;
        rwRestOfLineAfterFirstToken(iss, path);
        if (!id.empty() && !path.empty())
            lib.backgrounds[id].push_back(path);
    } else if (cmd == "charImage") {
        std::string id;
        if (!rwReadCfgToken(iss, id))
            return;
        std::string path;
        rwRestOfLineAfterFirstToken(iss, path);
        if (!id.empty() && !path.empty())
            lib.characters[id].push_back(path);
    } else if (cmd == "defaultBg") {
        std::string id;
        int idx = 0;
        if (rwReadCfgToken(iss, id) && (iss >> idx)) {
            lib.defBgId = id;
            lib.defBgIdx = idx;
        }
    } else if (cmd == "defaultChar2") {
        std::string id;
        int idx = 0;
        if (rwReadCfgToken(iss, id) && (iss >> idx)) {
            lib.defC2Id = id;
            lib.defC2Idx = idx;
        }
    } else if (cmd == "defaultChar3") {
        std::string id;
        int idx = 0;
        if (rwReadCfgToken(iss, id) && (iss >> idx)) {
            lib.defC3Id = id;
            lib.defC3Idx = idx;
        }
    }
}

static bool rwLoadVisualsCfg(const std::string &path, RwVisualLibrary &out) {
    out = RwVisualLibrary{};
    std::ifstream in(path);
    if (!in.is_open())
        return false;
    std::string ver;
    if (!std::getline(in, ver) || trim(ver) != "v1")
        return false;
    std::string line;
    while (std::getline(in, line))
        rwParseVisualsLine(line, out);
    out.loaded = true;
    return true;
}

static void rwParseChapterLine(const std::string &lineIn, RwChapterScene &scene) {
    std::string line = trim(lineIn);
    if (line.empty() || line[0] == '#')
        return;
    std::istringstream iss(line);
    std::string cmd;
    if (!(iss >> cmd))
        return;
    if (cmd == "sceneBg") {
        std::string id;
        int idx = 0;
        if (rwReadCfgToken(iss, id) && (iss >> idx)) {
            scene.hasBg = true;
            scene.bgId = id;
            scene.bgIdx = idx;
        }
    } else if (cmd == "sceneChar2") {
        std::string id;
        int idx = 0;
        if (rwReadCfgToken(iss, id) && (iss >> idx)) {
            scene.hasC2 = true;
            scene.c2Id = id;
            scene.c2Idx = idx;
        }
    } else if (cmd == "sceneChar3") {
        std::string id;
        int idx = 0;
        if (rwReadCfgToken(iss, id) && (iss >> idx)) {
            scene.hasC3 = true;
            scene.c3Id = id;
            scene.c3Idx = idx;
        }
    }
}

static bool rwLoadChapterSceneCfg(const std::string &path, RwChapterScene &out) {
    out = RwChapterScene{};
    std::ifstream in(path);
    if (!in.is_open())
        return false;
    std::string ver;
    if (!std::getline(in, ver) || trim(ver) != "v1")
        return false;
    std::string line;
    while (std::getline(in, line))
        rwParseChapterLine(line, out);
    return true;
}

static void rwMergeSceneIds(const RwVisualLibrary &lib, const RwChapterScene &ch, std::string &bgId, int &bgI,
                            std::string &c2Id, int &c2I, std::string &c3Id, int &c3I) {
    bgId = ch.hasBg ? ch.bgId : lib.defBgId;
    bgI = ch.hasBg ? ch.bgIdx : lib.defBgIdx;
    c2Id = ch.hasC2 ? ch.c2Id : lib.defC2Id;
    c2I = ch.hasC2 ? ch.c2Idx : lib.defC2Idx;
    c3Id = ch.hasC3 ? ch.c3Id : lib.defC3Id;
    c3I = ch.hasC3 ? ch.c3Idx : lib.defC3Idx;
}

static bool rwPickImagePath(const std::map<std::string, std::vector<std::string>> &m, const std::string &id, int idx,
                            const std::string &baseDir, const std::string &assetRoot, const std::string &novelRootForCapFallback,
                            std::string &outPath) {
    outPath.clear();
    if (trim(id).empty())
        return false;
    auto it = m.find(id);
    if (it == m.end() || it->second.empty())
        return false;
    const int n = (int)it->second.size();
    const int i = std::clamp(idx, 0, n - 1);
    outPath = rwResolveMediaPathWithCapFallback(baseDir, it->second[(size_t)i], assetRoot, novelRootForCapFallback);
    return !outPath.empty();
}

static void rwAutoPlaceMenuPopupOnDesk(TDeskTop *host, TMenuPopup *m, TPoint p) {
    TRect r = m->getBounds();
    TPoint d = host->size - p;
    r.move(std::min((int)m->size.x, (int)d.x), std::min((int)(m->size.y + 1), (int)d.y));
    if (r.contains(p) && r.b.y - r.a.y < p.y)
        r.move(0, -(r.b.y - p.y));
    m->setBounds(r);
}

static bool rwPickCharacterIdFromList(TDeskTop *desk, const std::map<std::string, std::vector<std::string>> &ch, std::string &outId) {
    if (!desk || ch.empty())
        return false;
    std::vector<std::string> ids;
    ids.reserve(ch.size());
    for (const auto &pr : ch)
        ids.push_back(pr.first);
    std::sort(ids.begin(), ids.end());
    if (ids.size() > 4000u)
        ids.resize(4000);
    TMenuItem *chain = new TMenuItem("~C~ancelar", cmCancel, kbNoKey);
    for (int i = (int)ids.size() - 1; i >= 0; --i) {
        std::string nm = ids[(size_t)i];
        for (char &c : nm) {
            if (c == '~')
                c = '_';
        }
        const ushort cmd = (ushort)(cmPickCharPopupBase + (unsigned)i);
        chain = new TMenuItem(TStringView(nm.c_str(), (unsigned)nm.size()), cmd, kbNoKey, hcNoContext, TStringView(), chain);
    }
    TRect dr = desk->getExtent();
    TPoint lc;
    lc.x = (short)((dr.a.x + dr.b.x) / 2);
    lc.y = (short)((dr.a.y + dr.b.y) / 2);
    TRect bounds(lc, lc);
    TMenu *menu = new TMenu(*chain);
    TMenuPopup *menuPopup = new TMenuPopup(bounds, menu);
    rwAutoPlaceMenuPopupOnDesk(desk, menuPopup, lc);
    const ushort r = desk->execView(menuPopup);
    TObject::destroy(menuPopup);
    if (r >= cmPickCharPopupBase && r < cmPickCharPopupBase + (ushort)ids.size()) {
        outId = ids[(size_t)(r - cmPickCharPopupBase)];
        return true;
    }
    return false;
}

static bool rwVisualsHasActiveDefaults(const RwVisualLibrary &lib, const RwChapterScene &ch) {
    if (!lib.loaded)
        return false;
    const bool d = !trim(lib.defBgId).empty() || !trim(lib.defC2Id).empty() || !trim(lib.defC3Id).empty();
    const bool o = ch.hasBg || ch.hasC2 || ch.hasC3;
    return d || o;
}

static void rwWriteVisualsCfg(const std::string &path, const RwVisualLibrary &lib) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open())
        return;
    out << "v1\n";
    out << "# Carpeta tipo vault (opcional): rutas de bgImage/charImage relativas a <donde esta visuals.cfg>/<assetRoot>/\n";
    out << "# Ejemplo: assetRoot assets  ->  assets/backgrounds/cafe.png  y  assets/characters/ana/saludo.png\n";
    if (!trim(lib.assetRootSubpath).empty())
        out << "assetRoot " << lib.assetRootSubpath << "\n";
    out << "# bgImage <id> <ruta>  | id puede ser nombre (con espacios) entre comillas\n";
    out << "# charImage <id> <ruta>\n";
    out << "# defaultBg <id> <indice>   defaultChar2 / defaultChar3\n";
    for (const auto &pr : lib.backgrounds) {
        for (const std::string &im : pr.second) {
            out << "bgImage ";
            rwWriteCfgId(out, pr.first);
            out << " " << im << "\n";
        }
    }
    for (const auto &pr : lib.characters) {
        for (const std::string &im : pr.second) {
            out << "charImage ";
            rwWriteCfgId(out, pr.first);
            out << " " << im << "\n";
        }
    }
    if (!trim(lib.defBgId).empty()) {
        out << "defaultBg ";
        rwWriteCfgId(out, lib.defBgId);
        out << " " << lib.defBgIdx << "\n";
    }
    if (!trim(lib.defC2Id).empty()) {
        out << "defaultChar2 ";
        rwWriteCfgId(out, lib.defC2Id);
        out << " " << lib.defC2Idx << "\n";
    }
    if (!trim(lib.defC3Id).empty()) {
        out << "defaultChar3 ";
        rwWriteCfgId(out, lib.defC3Id);
        out << " " << lib.defC3Idx << "\n";
    }
}

static void rwWriteChapterSceneCfg(const std::string &path, const RwChapterScene &scene) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open())
        return;
    out << "v1\n";
    if (scene.hasBg) {
        out << "sceneBg ";
        rwWriteCfgId(out, scene.bgId);
        out << " " << scene.bgIdx << "\n";
    }
    if (scene.hasC2) {
        out << "sceneChar2 ";
        rwWriteCfgId(out, scene.c2Id);
        out << " " << scene.c2Idx << "\n";
    }
    if (scene.hasC3) {
        out << "sceneChar3 ";
        rwWriteCfgId(out, scene.c3Id);
        out << " " << scene.c3Idx << "\n";
    }
}

static std::string rwBuildVisualGroupSummary(const char *title, const std::map<std::string, std::vector<std::string>> &m,
                                             int maxItems) {
    std::string out = std::string(title) + " (" + std::to_string((int)m.size()) + "):";
    if (m.empty()) {
        out += " vacio";
        return out;
    }
    int shown = 0;
    for (const auto &pr : m) {
        if (shown >= maxItems)
            break;
        out += "\n- " + pr.first + " (" + std::to_string((int)pr.second.size()) + ")";
        ++shown;
    }
    if ((int)m.size() > shown)
        out += "\n... +" + std::to_string((int)m.size() - shown) + " mas";
    return out;
}

static std::string rwBuildVisualLibraryOverview(const RwVisualLibrary &lib) {
    return rwBuildVisualGroupSummary("Fondos", lib.backgrounds, 3) + "\n" +
           rwBuildVisualGroupSummary("Personajes", lib.characters, 3);
}

static std::string rwBuildVariantsGridForId(const std::map<std::string, std::vector<std::string>> &m, const std::string &id,
                                            int maxItems) {
    const std::string idT = trim(id);
    if (idT.empty())
        return "Variantes: selecciona un id.";
    auto it = m.find(idT);
    if (it == m.end() || it->second.empty())
        return "Variantes de '" + idT + "': vacio";
    std::string out = "Variantes de '" + idT + "' (" + std::to_string((int)it->second.size()) + "):";
    const int n = std::min(maxItems, (int)it->second.size());
    for (int i = 0; i < n; ++i) {
        std::string p = it->second[(size_t)i];
        if ((int)p.size() > 54)
            p = p.substr(0, 51) + "...";
        out += "\n[" + std::to_string(i) + "] " + p;
    }
    if ((int)it->second.size() > n)
        out += "\n... +" + std::to_string((int)it->second.size() - n) + " variantes";
    return out;
}

/** Un solo byte para TBackground; '\xb0' si el glifo UTF-8 ocupa más de un octeto. */
static char retroPatternByteFromUtf8(const char *s) noexcept {
    if (!s || !s[0])
        return '\xb0';
    if (s[1] == '\0')
        return s[0];
    return '\xb0';
}

class ColorPreviewView : public TView {
public:
    ColorPreviewView(const TRect &bounds, ushort fg, ushort bg, const std::string &symUtf8) noexcept :
        TView(bounds), fgColor(fg), bgColor(bg), patternUtf8(symUtf8.empty() ? std::string("\xb0") : symUtf8) {}

    void setColors(ushort fg, ushort bg) {
        fgColor = fg;
        bgColor = bg;
        drawView();
    }

    void setPatternUtf8(const std::string &u8) {
        patternUtf8 = u8.empty() ? std::string("\xb0") : u8;
        drawView();
    }

    virtual void draw() {
        TDrawBuffer b;
        TColorAttr attr(
            TColorDesired(TColorXTerm(uchar(fgColor & 0xFF))),
            TColorDesired(TColorXTerm(uchar(bgColor & 0xFF)))
        );
        if (size.y >= 1) {
            b.moveChar(0, ' ', attr, size.x);
            b.moveStr(0, TStringView("Colores (muestra)"), attr);
            writeLine(0, 0, size.x, 1, b);
        }
        if (size.y >= 2) {
            b.moveChar(0, ' ', attr, size.x);
            b.moveStr(0, TStringView("Texto AaBb0123"), attr);
            writeLine(0, 1, size.x, 1, b);
        }
        TStringView sym(patternUtf8.data(), patternUtf8.size());
        if (sym.size() == 0)
            sym = TStringView("\xb0", 1);
        /* Dos filas de mosaico como el escritorio; el resto del alto queda en blanco. */
        const short patternEnd = std::min<short>(size.y, (short)4);
        for (short y = 2; y < patternEnd; ++y) {
            b.moveChar(0, ' ', attr, size.x);
            short x = 0;
            while (x < size.x) {
                const int room = (int)size.x - (int)x;
                if (room <= 0)
                    break;
                const ushort w = b.moveStr((ushort)x, sym, attr, (ushort)room);
                if (w == 0)
                    break;
                x = (short)((int)x + (int)w);
            }
            writeLine(0, y, size.x, 1, b);
        }
        for (short y = patternEnd; y < size.y; ++y) {
            b.moveChar(0, ' ', attr, size.x);
            writeLine(0, y, size.x, 1, b);
        }
    }

private:
    ushort fgColor;
    ushort bgColor;
    std::string patternUtf8;
};

class CleanButton : public TView {
public:
    CleanButton(const TRect &bounds, const char *text, ushort cmd, bool isDefault = false) noexcept :
        TView(bounds), title(text ? text : ""), command(cmd), amDefault(isDefault) {
        options |= ofSelectable | ofFirstClick;
        eventMask |= evMouseDown | evKeyDown | evBroadcast;
    }

    virtual void draw() override {
        /* Mismo dibujo que TButton::drawState (!down, sin markers) para bordes y título alineados. */
        TDrawBuffer b;
        const bool isDisabled = (state & sfDisabled) != 0;
        const bool isFocused = (state & sfFocused) != 0;
        TAttrPair facePair = isDisabled ? getColor(4) : (isFocused ? getColor(2) : getColor(1));
        TColorAttr face(facePair);
        TAttrPair shadow = getColor(8);

        const int s = int(size.x) - 1;
        const int Trow = int(size.y) / 2 - 1;
        static constexpr char shTop = '\xDC', shMid = '\xDB', shBot = '\xDF';

        for (int y = 0; y <= int(size.y) - 2; ++y) {
            b.moveChar(0, ' ', face, size.x);
            b.putAttribute(0, shadow[0]);
            if (s >= 0 && s < int(size.x)) {
                b.putAttribute(ushort(s), shadow[0]);
                b.putChar(ushort(s), uchar(y == 0 ? shTop : shMid));
            }
            if (y == Trow && !title.empty()) {
                int l = (s - cstrlen(TStringView(title)) - 1) / 2;
                if (l < 1)
                    l = 1;
                b.moveStr(ushort(1 + l), TStringView(title), face);
            }
            writeLine(0, short(y), size.x, 1, b);
        }

        if (size.y >= 1) {
            b.moveChar(0, ' ', shadow[0], 2);
            if (s > 1)
                b.moveChar(2, shBot, shadow[0], ushort(s - 1));
            writeLine(0, short(size.y - 1), size.x, 1, b);
        }
    }

    virtual TPalette &getPalette() const override {
        static TPalette palette("\x0A\x0B\x0C\x0D\x0E\x0E\x0E\x0F", 8);
        return palette;
    }

    virtual void handleEvent(TEvent &event) override {
        TView::handleEvent(event);

        if (event.what == evMouseDown) {
            if ((state & sfDisabled) != 0) { clearEvent(event); return; }
            bool inside = false;
            do {
                TPoint p = makeLocal(event.mouse.where);
                inside = (p.x >= 0 && p.y >= 0 && p.x < size.x && p.y < size.y);
            } while (mouseEvent(event, evMouseMove));
            if (inside) press();
            clearEvent(event);
            return;
        }

        if (event.what == evKeyDown) {
            ushort key = event.keyDown.keyCode;
            char ch = event.keyDown.charScan.charCode;
            if ((state & sfFocused) && (ch == ' ' || key == kbEnter)) {
                press();
                clearEvent(event);
                return;
            }
        }

        if (event.what == evBroadcast && event.message.command == cmDefault &&
            amDefault && !(state & sfDisabled)) {
            press();
            clearEvent(event);
        }
    }

    virtual void setState(ushort aState, Boolean enable) override {
        TView::setState(aState, enable);
        if (aState & (sfSelected | sfFocused | sfDisabled))
            drawView();
    }

private:
    std::string title;
    ushort command;
    bool amDefault {false};

    void press() {
        TEvent e;
        e.what = evCommand;
        e.message.command = command;
        e.message.infoPtr = nullptr;
        putEvent(e);
    }
};

class FlatActionLabel : public TView {
public:
    FlatActionLabel(const TRect &bounds, const char *text, ushort cmd) noexcept :
        TView(bounds), label(text ? text : ""), command(cmd) {
        options |= ofSelectable | ofFirstClick;
        eventMask |= evMouseDown | evKeyDown;
    }

    void draw() override {
        TDrawBuffer b;
        const TColorAttr base = getColor(1);
        b.moveChar(0, ' ', base, size.x);
        /* [x] blanco con fondo rojo (sin subrayado ni caja de boton). */
        TColorAttr txt(
            TColorDesired(TColorXTerm(15)),
            TColorDesired(TColorXTerm((state & sfFocused) ? 196 : 160))
        );
        int x = std::max(0, (int(size.x) - (int)cstrlen(TStringView(label))) / 2);
        b.moveStr((ushort)x, TStringView(label), txt, size.x);
        writeLine(0, 0, size.x, 1, b);
    }

    void handleEvent(TEvent &event) override {
        TView::handleEvent(event);
        if (event.what == evMouseDown) {
            bool inside = false;
            do {
                TPoint p = makeLocal(event.mouse.where);
                inside = (p.x >= 0 && p.y >= 0 && p.x < size.x && p.y < size.y);
            } while (mouseEvent(event, evMouseMove));
            if (inside)
                press();
            clearEvent(event);
            return;
        }
        if (event.what == evKeyDown) {
            const ushort key = event.keyDown.keyCode;
            const char ch = event.keyDown.charScan.charCode;
            if ((state & sfFocused) && (key == kbEnter || ch == ' ')) {
                press();
                clearEvent(event);
            }
        }
    }

private:
    std::string label;
    ushort command;

    void press() {
        TEvent e {};
        e.what = evCommand;
        e.message.command = command;
        e.message.infoPtr = nullptr;
        putEvent(e);
    }
};

static std::string asciiLower(std::string s) {
    for (char &c : s)
        c = char(std::tolower((unsigned char)c));
    return s;
}

static std::string navLabelStem(const std::string &label) {
    if (label.size() >= 2 && label.back() == '/' && label != "../")
        return label.substr(0, label.size() - 1);
    return label;
}

class NavigatorListView;

/**
 * Activa la misma ruta que cmNavSelect en la aplicacion.
 * Antes se usaba putEvent (cola): al cerrar el dialogo modal el puntero a NavigatorListView quedaba invalido
 * o el broadcast se procesaba mal y la app podia colgarse. message() entrega en la misma pila.
 */
static void postNavSelectEvent(NavigatorListView *src) noexcept {
    if (!TProgram::application || !src)
        return;
    message(TProgram::application, evBroadcast, cmNavSelect, src);
}

/** Extensiones que stb_image suele cargar (vista previa en selector de imagen). */
static bool rwPathLooksLikeRasterImage(const std::string &path) {
    const size_t dot = path.rfind('.');
    if (dot == std::string::npos)
        return false;
    const std::string ext = asciiLower(path.substr(dot));
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".bmp" || ext == ".webp" ||
           ext == ".tga" || ext == ".psd" || ext == ".hdr" || ext == ".pic" || ext == ".pnm";
}

static bool navLabelMatchesQuick(const std::string &label, const std::string &qLower) {
    if (qLower.empty())
        return true;
    if (label == "..")
        return true;
    return asciiLower(navLabelStem(label)).find(qLower) != std::string::npos;
}

/** Franja inferior del panel: separador y botones Carpeta / .txt. */
class FolderPanelFooterStrip : public TView {
public:
    FolderPanelFooterStrip(const TRect &bounds) noexcept : TView(bounds) {
        options |= ofFirstClick;
        eventMask |= evMouseDown;
        /* gfGrowHiX|LoY|HiY: el ancho y la franja vertical siguen al redimensionar la ventana (sin HiY los botones quedaban arriba). */
        growMode = gfGrowHiX | gfGrowLoY | gfGrowHiY;
    }

    TPalette &getPalette() const override {
        static TPalette palette("\x02\x03\x04\x05\x06\x07", 6);
        return palette;
    }

    void draw() override {
        TDrawBuffer b;
        TAttrPair cNormal = getColor(0x0301);
        TAttrPair cSelect = getColor(0x0604);
        TAttrPair cDim = getColor(0x0202);

        b.moveChar(0, '\xC4', TColorAttr(cDim), size.x);
        writeLine(0, 0, size.x, 1, b);

        b.moveChar(0, ' ', cNormal, size.x);
        TAttrPair color = ((state & sfFocused) != 0) ? cSelect : cNormal;
        const BtnLayout L = layoutForWidth();
        if (L.wf > 0 && L.folderStr && L.xf >= 0 && L.xf < size.x) {
            const ushort room = ushort(std::max(0, int(size.x) - int(L.xf)));
            b.moveStr(ushort(L.xf), TStringView(L.folderStr, (unsigned)L.wf), TColorAttr(color), room);
        }
        if (L.wt > 0 && L.txtStr && L.xt >= 0 && L.xt < size.x) {
            const ushort room = ushort(std::max(0, int(size.x) - int(L.xt)));
            b.moveStr(ushort(L.xt), TStringView(L.txtStr, (unsigned)L.wt), TColorAttr(color), room);
        }
        writeLine(0, 1, size.x, 1, b);
    }

    void handleEvent(TEvent &event) override {
        TView::handleEvent(event);
        if (event.what == evMouseDown) {
            TPoint p = makeLocal(event.mouse.where);
            if (p.y == 1) {
                if (TProgram::application) {
                    if (hitFolder(p.x))
                        message(TProgram::application, evBroadcast, cmCreateFolder, this);
                    else if (hitTxt(p.x))
                        message(TProgram::application, evBroadcast, cmCreateTxtFile, this);
                }
            }
            clearEvent(event);
            return;
        }
    }

    void changeBounds(const TRect &bounds) override {
        const short pw = size.x;
        const short ph = size.y;
        TView::changeBounds(bounds);
        if ((size.x != pw || size.y != ph) && TProgram::application)
            message(TProgram::application, evBroadcast, cmNavPanelLayoutChanged, this);
    }

private:
    struct BtnLayout {
        short xf {0};
        short wf {0};
        short xt {0};
        short wt {0};
        const char *folderStr {nullptr};
        const char *txtStr {nullptr};
    };

    BtnLayout layoutForWidth() const {
        BtnLayout L;
        static constexpr const char kLongF[] = "[ Carpeta ]";
        static constexpr const char kLongT[] = "[ .txt ]";
        static constexpr const char kMidF[] = "[Carpeta]";
        static constexpr const char kMidT[] = "[.txt]";
        static constexpr const char kShortF[] = "[D]";
        static constexpr const char kShortT[] = "[T]";
        const char *f = kLongF;
        const char *t = kLongT;
        L.wf = short(sizeof(kLongF) - 1);
        L.wt = short(sizeof(kLongT) - 1);
        int gap = 2;
        auto wtotal = [&]() { return int(L.wf) + gap + int(L.wt); };

        if (wtotal() > int(size.x)) {
            f = kMidF;
            t = kMidT;
            L.wf = short(sizeof(kMidF) - 1);
            L.wt = short(sizeof(kMidT) - 1);
        }
        if (wtotal() > int(size.x)) {
            f = kShortF;
            t = kShortT;
            L.wf = short(sizeof(kShortF) - 1);
            L.wt = short(sizeof(kShortT) - 1);
            gap = 1;
        }
        const int total = int(L.wf) + gap + int(L.wt);
        int start = (int(size.x) - total) / 2;
        if (start < 0)
            start = 0;
        if (start + total > int(size.x))
            start = std::max(0, int(size.x) - total);
        L.xf = short(start);
        L.xt = short(start + int(L.wf) + gap);
        L.folderStr = f;
        L.txtStr = t;
        return L;
    }

    bool hitFolder(int px) const {
        const BtnLayout L = layoutForWidth();
        return px >= int(L.xf) && px < int(L.xf) + int(L.wf);
    }

    bool hitTxt(int px) const {
        const BtnLayout L = layoutForWidth();
        return px >= int(L.xt) && px < int(L.xt) + int(L.wt);
    }
};

/** Lista del explorador de archivos con teclado. */
class NavigatorListView : public TView {
public:
    struct NavItem {
        std::string label;
        bool isDirectory;
        std::string fullPath;
    };

    NavigatorListView(const TRect &bounds, const ushort *themeText, const ushort *themeBack) noexcept :
        TView(bounds), themeTextColor(themeText), themeBackColor(themeBack) {
        options |= ofSelectable | ofFirstClick;
        eventMask |= evMouseDown | evKeyDown | evMouseWheel;
        growMode = gfGrowHiX | gfGrowHiY;
    }

    void setItems(const std::vector<NavItem> &newItems) {
        itemsMaster = newItems;
        quickFilter.clear();
        applyQuickFilter();
    }

    bool peekCursorItem(NavItem &out) const {
        if (cursor < 0 || cursor >= (int)items.size()) return false;
        out = items[(size_t)cursor];
        return true;
    }

    void setCursorByLabel(const std::string &label) {
        for (int i = 0; i < (int)items.size(); ++i) {
            if (items[(size_t)i].label == label) {
                cursor = i;
                ensureVisible();
                return;
            }
        }
    }

    /* API externa para acoplar un TScrollBar (dialogos/pickers) sin duplicar logica de scroll. */
    int externalScrollMax() const {
        if (items.empty())
            return 0;
        const int br = bodyRows();
        if (!hasPinnedParent())
            return std::max(0, (int)items.size() - br);
        const int body = std::max(0, br - 1);
        const int nScroll = std::max(0, (int)items.size() - 1);
        return std::max(0, nScroll - body);
    }

    int externalScrollValue() const {
        return std::max(0, scrollTop);
    }

    void externalSetScrollValue(int value) {
        if (items.empty()) {
            scrollTop = 0;
            cursor = 0;
            drawView();
            return;
        }
        scrollTop = value;
        clampScrollTop();
        if (hasPinnedParent())
            cursor = std::min((int)items.size() - 1, std::max(1, 1 + scrollTop));
        else
            cursor = std::min((int)items.size() - 1, std::max(0, scrollTop));
        ensureVisible();
        drawView();
    }

    virtual void draw() {
        TDrawBuffer b;
        ushort fg = themeTextColor ? (ushort)(*themeTextColor & 0xFF) : 15;
        ushort bg = themeBackColor ? (ushort)(*themeBackColor & 0xFF) : 1;
        TColorAttr attrNormal{
            TColorDesired(TColorXTerm(uchar(fg))),
            TColorDesired(TColorXTerm(uchar(bg)))};
        TColorAttr attrSel = reverseAttribute(attrNormal);
        TColorAttr barAttr = reverseAttribute(attrSel);

        const int fr = filterRows();
        for (short y = 0; y < size.y; ++y) {
            if (fr != 0 && y == 0) {
                b.moveChar(0, ' ', barAttr, size.x);
                std::string hint = std::string("| ") + quickFilter;
                if ((int)hint.size() > (int)size.x)
                    hint = hint.substr(0, (size_t)size.x);
                b.moveStr(0, TStringView(hint), barAttr, size.x);
                writeLine(0, y, size.x, 1, b);
                continue;
            }
            int idx = rowToItemIndex(y);
            TColorAttr attrFill = attrNormal;
            TColorAttr attrText = attrNormal;
            if (idx >= 0 && idx < (int)items.size() && idx == cursor) {
                if ((state & sfFocused) != 0)
                    attrText = attrSel;
                setStyle(attrText, ushort(getStyle(attrText) | slUnderline));
            }
            b.moveChar(0, ' ', attrFill, size.x);
            if (idx >= 0 && idx < (int)items.size()) {
                const NavItem &it = items[idx];
                const std::string line = treePrefix(idx) + it.label;
                const std::string shown = labelTailForWidth(line, (int)size.x);
                b.moveStr(0, TStringView(shown.c_str(), shown.size()), attrText, size.x);
            }
            writeLine(0, y, size.x, 1, b);
        }
    }

    virtual void handleEvent(TEvent &event) {
        TView::handleEvent(event);
        if (event.what == evMouseDown) {
            if (owner)
                owner->setCurrent(this, normalSelect);
            /* meDoubleClick se lee antes de mouseEvent: el bucle reutiliza event y el flag se pierde. */
            const bool doubleClick = (event.mouse.eventFlags & meDoubleClick) != 0;
            TPoint p0 = makeLocal(event.mouse.where);
            if (filterRows() != 0 && p0.y == 0) {
                quickFilter.clear();
                applyQuickFilter();
                clearEvent(event);
                return;
            }
            do {
                TPoint p = makeLocal(event.mouse.where);
                int idx = rowToItemIndex(p.y);
                if (idx >= 0 && idx < (int)items.size())
                    cursor = idx;
                drawView();
            } while (mouseEvent(event, evMouseMove));
            if (doubleClick)
                postNavSelectEvent(this);
            clearEvent(event);
        } else if (event.what == evMouseWheel) {
            if (items.empty()) {
                clearEvent(event);
                return;
            }
            switch (event.mouse.wheel) {
                case mwUp:
                case mwLeft:
                    cursor = std::max(0, cursor - 1);
                    ensureVisible();
                    drawView();
                    clearEvent(event);
                    break;
                case mwDown:
                case mwRight:
                    cursor = std::min((int)items.size() - 1, cursor + 1);
                    ensureVisible();
                    drawView();
                    clearEvent(event);
                    break;
                default:
                    break;
            }
        } else if (event.what == evKeyDown) {
            switch (ctrlToArrow(event.keyDown.keyCode)) {
                case kbUp:
                    if (!items.empty()) {
                        int n = (int)items.size();
                        cursor = (cursor - 1 + n) % n;
                        ensureVisible();
                    }
                    drawView();
                    clearEvent(event);
                    break;
                case kbDown:
                    if (!items.empty()) {
                        int n = (int)items.size();
                        cursor = (cursor + 1) % n;
                        ensureVisible();
                    }
                    drawView();
                    clearEvent(event);
                    break;
                case kbHome:
                    cursor = 0;
                    scrollTop = 0;
                    clampScrollTop();
                    drawView();
                    clearEvent(event);
                    break;
                case kbEnd:
                    if (!items.empty()) {
                        cursor = (int)items.size() - 1;
                        ensureVisible();
                    }
                    drawView();
                    clearEvent(event);
                    break;
                case kbPgUp: {
                    if (!items.empty()) {
                        int n = (int)items.size();
                        int step = scrollPageStep();
                        cursor = (cursor - step + n) % n;
                        ensureVisible();
                    }
                    drawView();
                    clearEvent(event);
                    break;
                }
                case kbPgDn: {
                    if (!items.empty()) {
                        int n = (int)items.size();
                        int step = scrollPageStep();
                        cursor = (cursor + step) % n;
                        ensureVisible();
                    }
                    drawView();
                    clearEvent(event);
                    break;
                }
                default:
                    if (event.keyDown.keyCode == kbEnter) {
                        postNavSelectEvent(this);
                        clearEvent(event);
                    } else if (event.keyDown.keyCode == kbEsc) {
                        if (!quickFilter.empty()) {
                            quickFilter.clear();
                            applyQuickFilter();
                            clearEvent(event);
                        }
                    } else if (event.keyDown.keyCode == kbBack) {
                        if (!quickFilter.empty()) {
                            quickFilter.pop_back();
                            applyQuickFilter();
                            clearEvent(event);
                        }
                    } else {
                        const uchar ch = uchar(event.keyDown.charScan.charCode);
                        if (ch >= 32u && ch <= 126u) {
                            quickFilter.push_back(char(ch));
                            applyQuickFilter();
                            clearEvent(event);
                        }
                    }
                    break;
            }
        }
    }

private:
    std::vector<NavItem> itemsMaster;
    std::vector<NavItem> items;
    std::string quickFilter;
    int cursor {0};
    int scrollTop {0};
    const ushort *themeTextColor {nullptr};
    const ushort *themeBackColor {nullptr};

    int filterRows() const { return quickFilter.empty() ? 0 : 1; }

    int bodyRows() const { return std::max(0, (int)size.y - filterRows()); }

    void tryFocusMatchingFilter() {
        if (quickFilter.empty())
            return;
        const std::string q = asciiLower(trim(quickFilter));
        if (q.empty())
            return;

        int exactIdx = -1;
        int exactCount = 0;
        int soleIdx = -1;
        int soleCount = 0;
        for (int i = 0; i < (int)items.size(); ++i) {
            const NavItem &it = items[(size_t)i];
            if (it.label == "..")
                continue;
            soleCount++;
            soleIdx = i;
            if (asciiLower(navLabelStem(it.label)) == q) {
                exactCount++;
                exactIdx = i;
            }
        }
        if (exactCount == 1 && exactIdx >= 0) {
            cursor = exactIdx;
            return;
        }
        /* Con un único ítem distinto de ".." el cursor se fija ahí (Enter sin flechas). */
        if (soleCount == 1 && soleIdx >= 0)
            cursor = soleIdx;
    }

    void applyQuickFilter() {
        items.clear();
        const std::string qlow = asciiLower(quickFilter);
        for (const auto &it : itemsMaster) {
            if (navLabelMatchesQuick(it.label, qlow))
                items.push_back(it);
        }
        if (cursor >= (int)items.size())
            cursor = std::max(0, (int)items.size() - 1);
        if (cursor < 0)
            cursor = 0;
        tryFocusMatchingFilter();
        if (cursor >= (int)items.size())
            cursor = std::max(0, (int)items.size() - 1);
        if (cursor < 0)
            cursor = 0;
        scrollTop = 0;
        clampScrollTop();
        ensureVisible();
        drawView();
    }

    std::string treePrefix(int) const { return ""; }

    bool hasPinnedParent() const {
        return !items.empty() && items[0].label == "..";
    }

    /** Filas de scroll (sin fila fija ".." ni fila de busqueda). */
    int scrollableRows() const {
        const int br = bodyRows();
        if (!hasPinnedParent()) return br;
        return std::max(0, br - 1);
    }

    int scrollPageStep() const {
        int s = scrollableRows();
        return s > 0 ? s : 1;
    }

    void clampScrollTop() {
        if (items.empty()) {
            scrollTop = 0;
            return;
        }
        const int br = bodyRows();
        if (!hasPinnedParent()) {
            int maxSt = std::max(0, (int)items.size() - br);
            scrollTop = std::min(std::max(0, scrollTop), maxSt);
            return;
        }
        int body = scrollableRows();
        if (body <= 0) {
            scrollTop = 0;
            return;
        }
        int nScroll = std::max(0, (int)items.size() - 1);
        int maxSt = std::max(0, nScroll - body);
        scrollTop = std::min(std::max(0, scrollTop), maxSt);
    }

    /** Fila de pantalla y -> indice en items (o -1). */
    int rowToItemIndex(int row) const {
        if (row < 0 || row >= size.y) return -1;
        const int fr = filterRows();
        if (fr != 0 && row == 0) return -1;
        const int bodyRow = row - fr;
        const int br = bodyRows();
        if (bodyRow < 0 || bodyRow >= br) return -1;
        if (hasPinnedParent()) {
            if (bodyRow == 0) return 0;
            return 1 + scrollTop + (bodyRow - 1);
        }
        return scrollTop + bodyRow;
    }

    void ensureVisible() {
        if (items.empty()) return;
        const int fr = filterRows();
        const int vbr = (int)size.y - fr;
        if (!hasPinnedParent()) {
            if (cursor < scrollTop) scrollTop = cursor;
            if (cursor >= scrollTop + vbr) scrollTop = cursor - vbr + 1;
            clampScrollTop();
            return;
        }
        if (cursor == 0) {
            scrollTop = 0;
            clampScrollTop();
            return;
        }
        int body = scrollableRows();
        int pos = cursor - 1;
        if (pos < scrollTop) scrollTop = pos;
        if (body > 0 && pos >= scrollTop + body) scrollTop = pos - body + 1;
        clampScrollTop();
    }
};

/** Listado principal del panel de archivos: avisa al redimensionar para retitular/truncar la ruta. */
class MainPanelNavigatorListView : public NavigatorListView {
public:
    MainPanelNavigatorListView(const TRect &bounds, const ushort *themeText, const ushort *themeBack) noexcept :
        NavigatorListView(bounds, themeText, themeBack) {}

    void handleEvent(TEvent &event) override {
        const bool rightClick = (event.what == evMouseDown) && ((event.mouse.buttons & mbRightButton) != 0);
        NavigatorListView::handleEvent(event);
        if (!rightClick || !TProgram::application)
            return;
        NavItem it {};
        if (peekCursorItem(it) && !it.isDirectory && it.label != "..")
            message(TProgram::application, evBroadcast, cmNavFilePopup, this);
    }

    void changeBounds(const TRect &bounds) override {
        const short pw = size.x;
        const short ph = size.y;
        NavigatorListView::changeBounds(bounds);
        if ((size.x != pw || size.y != ph) && TProgram::application)
            message(TProgram::application, evBroadcast, cmNavPanelLayoutChanged, this);
    }
};

/** Lista para Escena visual: al cambiar cursor dispara refresco del preview en vivo. */
class VisualSceneListView : public NavigatorListView {
public:
    VisualSceneListView(const TRect &bounds, const ushort *themeText, const ushort *themeBack) noexcept :
        NavigatorListView(bounds, themeText, themeBack) {}

    void handleEvent(TEvent &event) override {
        NavigatorListView::NavItem before {};
        const bool hadBefore = peekCursorItem(before);
        NavigatorListView::handleEvent(event);
        NavigatorListView::NavItem after {};
        const bool hadAfter = peekCursorItem(after);
        const bool changed =
            hadBefore != hadAfter || (hadBefore && hadAfter && before.fullPath != after.fullPath);
        if (changed && owner)
            message(owner, evCommand, cmVisualSceneRefresh, this);
    }
};

class MatrixSelectorView : public TView {
public:
    enum Kind { mkForeground, mkBackground, mkSymbol };

    MatrixSelectorView(const TRect &bounds, Kind kind, const std::vector<std::string> *symbolTableUtf8, ushort count, ushort columns, ushort command) noexcept :
        TView(bounds), kind(kind), symbolsUtf8(symbolTableUtf8), count(count), columns(columns), command(command) {
        options |= ofSelectable | ofFirstClick;
        eventMask |= evMouseDown | evKeyDown;
    }

    void setSelected(ushort idx) {
        if (idx >= count) idx = 0;
        selected = idx;
        drawView();
    }

    ushort getSelected() const { return selected; }

    ushort getSelectedColor() const {
        if ((kind == mkForeground || kind == mkBackground) && selected < count)
            return selected;
        return selected;
    }

    static char symbolToPatternChar(const char *s) { return retroPatternByteFromUtf8(s); }

    char getSelectedSymbol() const {
        if (kind == mkSymbol && symbolsUtf8 && selected < count)
            return symbolToPatternChar((*symbolsUtf8)[selected].c_str());
        return '\xb0';
    }

    const char *getSelectedSymbolUtf8() const {
        if (kind == mkSymbol && symbolsUtf8 && selected < count)
            return (*symbolsUtf8)[selected].c_str();
        return "\xb0";
    }

    virtual void draw() {
        TDrawBuffer b;
        short rows = short((count + columns - 1) / columns);
        for (short y = 0; y < size.y; ++y) {
            b.moveChar(0, ' ', getColor(1), size.x);
            short dataRow = short(scrollTop + y);
            if (dataRow < rows) {
                for (ushort col = 0; col < columns; ++col) {
                    ushort i = ushort(dataRow * columns + col);
                    if (i >= count) break;
                    short cellW = (kind == mkSymbol) ? 3 : 2;
                    short x = short(col * cellW);
                    if (x + cellW - 1 >= size.x) break;
                    bool isSel = (i == selected);
                    if (kind == mkForeground) {
                        ushort xidx = i;
                        TColorAttr fgAttr(
                            TColorDesired(TColorXTerm(uchar(xidx))),
                            TColorDesired(TColorXTerm(uchar(17)))
                        );
                        b.putChar(x, '\xdb');
                        b.putAttribute(x, fgAttr);
                        b.putChar(x + 1, '\xdb');
                        b.putAttribute(x + 1, fgAttr);
                        if (isSel) {
                            b.putChar(x, '[');
                            b.putAttribute(x, reverseAttribute(fgAttr));
                            b.putChar(x + 1, ']');
                            b.putAttribute(x + 1, reverseAttribute(fgAttr));
                        }
                    } else if (kind == mkBackground) {
                        ushort xidx = i;
                        TColorAttr bgAttr(
                            TColorDesired(TColorXTerm(uchar(15))),
                            TColorDesired(TColorXTerm(uchar(xidx)))
                        );
                        b.putChar(x, ' ');
                        b.putAttribute(x, bgAttr);
                        b.putChar(x + 1, ' ');
                        b.putAttribute(x + 1, bgAttr);
                        if (isSel) {
                            b.putChar(x, '.');
                            b.putAttribute(x, bgAttr);
                        }
                    } else {
                        uchar attr = isSel ? uchar(0x70) : uchar(0x1F);
                        b.putChar(x, ' ');
                        b.putAttribute(x, attr);
                        b.putChar(x + 1, ' ');
                        b.putAttribute(x + 1, attr);
                        b.putChar(x + 2, ' ');
                        b.putAttribute(x + 2, attr);
                        if (symbolsUtf8 && i < count)
                            b.moveStr(x + 1, (*symbolsUtf8)[i].c_str(), attr, 1);
                        if (isSel) {
                            b.putChar(x, '[');
                            b.putAttribute(x, attr);
                        }
                    }
                }
            }
            if (size.x >= 2) {
                if (scrollTop > 0 && y == 0) {
                    b.putChar(size.x - 2, '^');
                    b.putAttribute(size.x - 2, getColor(2));
                }
                if (scrollTop + size.y < rows && y == size.y - 1) {
                    b.putChar(size.x - 2, 'v');
                    b.putAttribute(size.x - 2, getColor(2));
                }
            }
            writeLine(0, y, size.x, 1, b);
        }
    }

    virtual void handleEvent(TEvent &event) {
        TView::handleEvent(event);
        ushort old = selected;
        if (event.what == evMouseDown) {
            do {
                TPoint p = makeLocal(event.mouse.where);
                int cellW = (kind == mkSymbol) ? 3 : 2;
                int col = p.x / cellW;
                int row = p.y + scrollTop;
                int idx = row * columns + col;
                if (col >= 0 && col < columns && row >= 0 && idx >= 0 && idx < count)
                    selected = (ushort) idx;
            } while (mouseEvent(event, evMouseMove));
            clearEvent(event);
        } else if (event.what == evKeyDown) {
            switch (ctrlToArrow(event.keyDown.keyCode)) {
                case kbLeft: selected = (selected + count - 1) % count; break;
                case kbRight: selected = (selected + 1) % count; break;
                case kbUp: selected = (selected + count - columns) % count; break;
                case kbDown: selected = (selected + columns) % count; break;
                case kbHome: selected = 0; break;
                case kbEnd: selected = count - 1; break;
                case kbPgUp: {
                    ushort page = ushort(columns * size.y);
                    selected = (selected > page) ? ushort(selected - page) : 0;
                    break;
                }
                case kbPgDn: {
                    ushort page = ushort(columns * size.y);
                    selected = ushort(std::min<int>(count - 1, int(selected) + int(page)));
                    break;
                }
                default: return;
            }
            clearEvent(event);
        }
        if (selected != old) {
            ensureVisible();
            drawView();
            message(owner, evBroadcast, command, this);
        }
    }

private:
    Kind kind;
    const std::vector<std::string> *symbolsUtf8;
    ushort count;
    ushort columns;
    ushort command;
    ushort selected {0};
    ushort scrollTop {0};

    void ensureVisible() {
        ushort row = ushort(selected / columns);
        ushort vis = ushort(size.y);
        if (row < scrollTop)
            scrollTop = row;
        else if (vis > 0 && row >= ushort(scrollTop + vis))
            scrollTop = ushort(row - vis + 1);
    }
};

/**
 * Checkbox de "incluir assets": paleta plana como texto estatico del dialogo (evita franja de color de cluster)
 * y lectura fiable del bit antes de destruir el dialogo.
 */
class FolderAssetsCheckBoxes : public TCheckBoxes {
public:
    FolderAssetsCheckBoxes(const TRect &bounds, TSItem *strings) noexcept : TCheckBoxes(bounds, strings) {}

    bool wantsAssets() const noexcept { return (value & 1u) != 0; }
    void setWantsAssets(bool enabled) noexcept { value = enabled ? 1u : 0u; }

    TPalette &getPalette() const override {
        static TPalette palette("\x06\x06\x06\x06\x06", 5);
        return palette;
    }
};

class AppearanceDialog : public TDialog {
public:
    AppearanceDialog(TRect bounds, ushort fg, ushort bg, char pat, const std::string &patUtf8, int autoSaveSec,
                     bool autoEmDash) :
        TWindowInit(&TDialog::initFrame),
        TDialog(bounds, "Apariencia y autoguardado") {
        options |= ofCentered;
        eventMask |= evBroadcast;
        palette = dpBlueDialog;
        fgSel = new MatrixSelectorView(TRect(3, 4, 21, 16), MatrixSelectorView::mkForeground, nullptr, 256, 9, cmFgMatrixChanged);
        bgSel = new MatrixSelectorView(TRect(23, 4, 41, 16), MatrixSelectorView::mkBackground, nullptr, 256, 9, cmBgMatrixChanged);
        symbolOptions = buildSymbolOptions();
        symSel = new MatrixSelectorView(TRect(43, 4, 56, 16), MatrixSelectorView::mkSymbol, &symbolOptions, ushort(symbolOptions.size()), 4, cmSymbolMatrixChanged);
        {
            std::string initSym = patUtf8.empty() ? std::string(1, pat) : patUtf8;
            preview = new ColorPreviewView(TRect(57, 4, 82, 9), fg, bg, initSym);
        }
        fgSel->setSelected(ushort(fg & 0xFF));
        bgSel->setSelected(ushort(bg & 0xFF));
        ushort symIdx = findSymbolIndexUtf8(patUtf8);
        if (symIdx == 0 && !patUtf8.empty() && symbolOptions[0] != patUtf8)
            symIdx = findSymbolIndex(pat);
        symSel->setSelected(symIdx);
        insert(new TStaticText(TRect(3, 3, 21, 4), "Foreground 0..255"));
        insert(new TStaticText(TRect(23, 3, 41, 4), "Background 0..255"));
        insert(new TStaticText(TRect(43, 3, 56, 4), "Simbolos"));
        insert(new TStaticText(TRect(57, 3, 82, 4), u8"Patr\u00f3n escritorio"));
        insert(fgSel);
        insert(bgSel);
        insert(symSel);
        insert(preview);
        insert(new TStaticText(TRect(57, 9, 82, 10), "Autog. (s, 0=off)"));
        std::memset(autoSaveData, 0, sizeof(autoSaveData));
        std::snprintf(autoSaveData, sizeof(autoSaveData), "%d",
                      parseIntClamped(std::to_string(autoSaveSec), 0, 7200, 60));
        autoSaveInput = new TInputLine(TRect(57, 10, 82, 11), 5);
        autoSaveInput->setData(autoSaveData);
        insert(autoSaveInput);
        autoEmDashCb = new FolderAssetsCheckBoxes(TRect(57, 11, 82, 13),
            new TSItem("Auto: -- -> guion largo", nullptr));
        autoEmDashCb->setWantsAssets(autoEmDash);
        insert(autoEmDashCb);
        insert(new TButton(TRect(56, 13, 68, 15), "Aplicar", cmOK, bfDefault));
        insert(new TButton(TRect(70, 13, 82, 15), "Cancelar", cmCancel, bfNormal));
    }

    /** Tras cmOK: lee el campo y lo limita a 0..7200. */
    int takeAutoSaveIntervalSec() {
        if (autoSaveInput)
            autoSaveInput->getData(autoSaveData);
        return parseIntClamped(trim(std::string(autoSaveData)), 0, 7200, 60);
    }

    bool takeAutoEmDashEnabled() const {
        return autoEmDashCb && autoEmDashCb->wantsAssets();
    }

    ushort findSymbolIndex(char c) const {
        for (ushort i = 0; i < symbolOptions.size(); ++i)
            if (MatrixSelectorView::symbolToPatternChar(symbolOptions[i].c_str()) == c) return i;
        return 0;
    }

    ushort findSymbolIndexUtf8(const std::string &u) const {
        if (u.empty()) return 0;
        for (ushort i = 0; i < symbolOptions.size(); ++i)
            if (symbolOptions[i] == u) return i;
        return 0;
    }

    ushort currentFg() const { return fgSel->getSelectedColor(); }
    ushort currentBg() const { return bgSel->getSelectedColor(); }
    char currentSymbol() const { return symSel->getSelectedSymbol(); }
    const char *currentSymbolUtf8() const { return symSel->getSelectedSymbolUtf8(); }

    virtual void handleEvent(TEvent &event) {
        TDialog::handleEvent(event);
        if (event.what == evBroadcast) {
            if (event.message.command == cmFgMatrixChanged ||
                event.message.command == cmBgMatrixChanged ||
                event.message.command == cmSymbolMatrixChanged) {
                preview->setColors(currentFg(), currentBg());
                preview->setPatternUtf8(std::string(symSel->getSelectedSymbolUtf8()));
                clearEvent(event);
            }
        }
    }

    MatrixSelectorView *fgSel {nullptr};
    MatrixSelectorView *bgSel {nullptr};
    MatrixSelectorView *symSel {nullptr};
    ColorPreviewView *preview {nullptr};
    TInputLine *autoSaveInput {nullptr};
    FolderAssetsCheckBoxes *autoEmDashCb {nullptr};
    char autoSaveData[6] {};
    std::vector<std::string> symbolOptions;

    static std::vector<std::string> buildSymbolOptions() {
        static const char *raw = u8R"(★ ☆ ✡ ✦ ✧ ✩ ✪ ✫ ✬ ✭ ✮ ✯ ✰ ⁂ ⁎ ⁑ ✢ ✣ ✤ ✥ ✱ ✲ ✳ ✴ ✵ ✶ ✷ ✸ ✹ ✺ ✻ ✼ ✽ ✾ ✿ ❀ ❁ ❂ ❃ ❇ ❈ ❉ ❊ ❋ ❄ ❆ ❅ ⋆ ≛ ᕯ ✲ ࿏ ꙰ ۞ ⭒ ⍟
♔ ♕ ♖ ♗ ♘ ♙ ♚ ♛ ♜ ♝ ♞ ♟ ♤ ♠ ♧ ♣ ♡ ♥ ♢ ♦ ♩ ♬ ♫ ♪ ♩ · ‑ ‒ – — ― ‗ ‘ ’ ‚ ‛ “ ” „ ‟ • ‣ ․ ‥ … ‧ ′ ″ ‴ ‵ ‶ ‷ ❛ ❜ ❝ ❞ ʹ ʺ ʻ ʼ ʽ ʾ ʿ ˀ ˁ ˂ ˃ ˄ ˅ ˆ ˇ ˈ ˉ ˊ ˋ ˌ ˍ ˎ ˏ ː ˑ ˒ ˓ ˔ ˕ ˖ ˗ ˘ ˙ ˚ ˛ ˜ ˝ ˠ ⋮ ⋯ ⋰ ⋱ ⁺ ⁻ ⁼ ⁽ ⁾ ⁿ ₊ ₋ ₌ ₍ ₎ ✖ ﹢ ﹣ ＋ － ／ ＝ ÷ ± × ❏ ❐ ❑ ❒ ▀ ▁ ▂ ▃ ▄ ▅ ▆ ▇ ▉ ▊ ▋ █ ▌ ▐ ▍ ▎ ▏ ▕ ░ ▒ ▓ ▔ ▬ ▢ ▣ ▤ ▥ ▦ ▧ ▨ ▩ ▪ ▫ ▭ ▮ ▯ ☰ ☲ ☱ ☴ ☵ ☶ ☳ ☷ ▰ ▱ ◧ ◨ ◩ ◪ ◫ ∎ ■ □ ⊞ ⊟ ⊠ ⊡ ❘ ❙ ❚ 〓 ◊ ◈ ◇ ◆ ⎔ ⎚ ☖ ☗ ◄ ▲ ▼ ► ◀ ◣ ◥ ◤ ◢ ▶ ◂ ▴ ▾ ▸ ◁ △ ▽ ▷ ∆ ∇ ⊳ ⊲ ⊴ ⊵ ◅ ▻ ▵ ▿ ◃ ▹ ◭ ◮ ⫷ ⫸ ⋖ ⋗ ⋪ ⋫ ⋬ ⋭ ⊿ ◬ ≜ ⑅ │ ┃ ╽ ╿ ╏ ║ ╎ ┇ ︱ ┊ ︳ ┋ ┆ ╵ 〡 〢 ╹ ╻ ╷ 〣 ☰ ☱ ☲ ☳ ☴ ☵ ☶ ☷ ≡ ✕ ═ ━ ─ ╍ ┅ ┉ ┄ ┈ ╌ ╴ ╶ ╸ ╺ ╼ ╾ ﹉ ﹍ ﹊ ﹎ ︲ ⑆ ⑇ ⑈ ⑉ ⑊ ⑄ ⑀ ︴ ﹏ ﹌ ﹋ ╳ ╲ ╱ ︶ ︵ 〵 〴 〳 〆 ` ᐟ ‐ ⁃ ⎯ 〄 ◉ ○ ◌ ◍ ◎ ● ◐ ◑ ◒ ◓ ◔ ◕ ◖ ◗ ❂ ☢ ⊗ ⊙ ◘ ◙ ◚ ◛ ◜ ◝ ◞ ◟ ◠ ◡ ◯ 〇 〶 ⚫ ⬤ ◦ ∅ ∘ ⊕ ⊖ ⊘ ⊚ ⊛ ⊜ ⊝ ❍ ⦿ ☁ ☀ ☂ ☃ ☾ ☽ ☼ ❄ ❅ ❆ ✙ ✙ ☯ ☢ ☠ ✞ ☤ ☮ ✢ ♥ ♡ ❥ ❣ ❦ ❧ ও ⚣ ☹ ☺ ☻ ㋡ シ ッ ㋛ ⊞ ⊟ ⊠ ⊡)";

        std::vector<std::string> out;
        std::string s(raw);
        for (size_t i = 0; i < s.size();) {
            unsigned char c = (unsigned char)s[i];
            if (c <= 0x20) { ++i; continue; }
            size_t len = 1;
            if ((c & 0xE0) == 0xC0) len = 2;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xF8) == 0xF0) len = 4;
            if (i + len > s.size()) break;
            out.push_back(s.substr(i, len));
            i += len;
        }
        /* Al inicio se insertan patrones CP437 de un byte por compatibilidad con TBackground. */
        out.insert(out.begin(), {std::string("\xb0"), std::string("\xb1"), std::string("\xb2"), std::string("\xdb"), std::string(" "), std::string("."), std::string("#")});
        return out;
    }
};


/** Vista previa RGB en el dialogo Biblioteca visual (bloques U+2580). Clic = comando de archivo (mismo que Buscar). */
class LibraryImagePreview : public TView {
public:
    LibraryImagePreview(const TRect &bounds, const std::string *pathPtr, const std::string *visualBaseDirPtr,
                        const std::string *assetRootPtr, const ushort *fgPal, const ushort *bgPal, bool useSixelMini,
                        bool kittyNativeMini, int sixelPaletteColors, unsigned kittyImgId, ushort openImageCmd) noexcept :
        TView(bounds),
        pathPtr(pathPtr),
        visualBaseDirPtr(visualBaseDirPtr),
        assetRootPtr(assetRootPtr),
        fgPal(fgPal),
        bgPal(bgPal),
        useSixelMini(useSixelMini),
        kittyNativeMini(kittyNativeMini),
        sixelPaletteColors(sixelPaletteColors),
        kittyImageId(kittyImgId),
        openCmd(openImageCmd) {
        options |= ofSelectable | ofFirstClick;
        /* Sin crecer: si crece con el dialogo, puede tapar filas de botones (TButton con altura 1 ya no dibuja). */
        growMode = 0;
        eventMask |= evMouseDown;
    }

    virtual void handleEvent(TEvent &event) override {
        TView::handleEvent(event);
        if (event.what == evMouseDown) {
            TEvent cmdEv {};
            cmdEv.what = evCommand;
            cmdEv.message.command = openCmd;
            cmdEv.message.infoPtr = this;
            putEvent(cmdEv);
            clearEvent(event);
        }
    }

    virtual void draw() override {
        TDrawBuffer b;
        const uchar fgI = uchar(fgPal ? (*fgPal & 0xFF) : 15);
        const uchar bgI = uchar(bgPal ? (*bgPal & 0xFF) : 1);
        const TColorRGB rgbFg = XTerm256toRGB(fgI);
        const TColorRGB rgbBg = XTerm256toRGB(bgI);
        const TColorDesired fgD{rgbFg};
        const TColorDesired bgD{rgbBg};
        const std::string t = pathPtr ? trim(*pathPtr) : std::string{};
        if (t != cachedPath) {
            cachedPath = t;
            cachedRgb.clear();
            cachedW = cachedH = 0;
            std::string loadPath = t;
            if (!t.empty() && visualBaseDirPtr && assetRootPtr) {
                fs::path canon;
                if (rwResolveUserImageFile(t, *visualBaseDirPtr, *assetRootPtr, canon))
                    loadPath = canon.string();
            }
            std::error_code ec;
            if (!loadPath.empty() && fs::is_regular_file(fs::path(loadPath), ec)) {
                int w = 0, h = 0, comp = 0;
                unsigned char *data = stbi_load(loadPath.c_str(), &w, &h, &comp, 3);
                if (data && w > 0 && h > 0) {
                    cachedRgb.assign(data, data + (size_t)w * (size_t)h * 3u);
                    stbi_image_free(data);
                    cachedW = w;
                    cachedH = h;
                    downscaleRgbImageForMini(cachedRgb, cachedW, cachedH, 640);
                }
            }
        }
        const int imgW = cachedW;
        const int imgH = cachedH;
        const bool usePhoto = !cachedRgb.empty() && imgW > 0 && imgH > 0 &&
            cachedRgb.size() >= (size_t)imgW * (size_t)imgH * 3u;
        if (usePhoto) {
#if !defined(_WIN32)
            const bool useKittyPh = kittyUnicodePlaceholdersEnabled(kittyNativeMini);
#else
            const bool useKittyPh = false;
#endif
            const int sx = std::max(1, (int)size.x);
            const int sy = std::max(1, (int)size.y);
            if (useKittyPh) {
#ifndef _WIN32
                int tw = 0;
                int th = 0;
                miniPreviewGridToPixels(sx, sy, 4096, tw, th);
                const uint64_t rgbFp = rgbFingerprint64(cachedRgb);
                const bool needUpload = kittyCachedSx != size.x || kittyCachedSy != size.y || kittyCachedTw != tw ||
                    kittyCachedTh != th || kittyCachedImgW != imgW || kittyCachedImgH != imgH ||
                    kittyCachedRgbSize != cachedRgb.size() || kittyCachedRgbFp != rgbFp;
                if (needUpload) {
                    deleteKittyMiniImageById(kittyImageId);
                    std::vector<uint8_t> scaled;
                    compositeRgbCenteredCover(cachedRgb, imgW, imgH, scaled, tw, th);
                    writeKittyRgbTransmitOnly(scaled.data(), tw, th, kittyImageId);
                    writeKittyVirtualPlacementUnicode(kittyImageId, sx, sy);
                    kittyCachedSx = size.x;
                    kittyCachedSy = size.y;
                    kittyCachedTw = tw;
                    kittyCachedTh = th;
                    kittyCachedImgW = imgW;
                    kittyCachedImgH = imgH;
                    kittyCachedRgbSize = cachedRgb.size();
                    kittyCachedRgbFp = rgbFp;
                }
                const TColorDesired fgDKitty{TColorXTerm{static_cast<uint8_t>(kittyImageId)}};
                const ushort bgPalKitty = bgPal ? (*bgPal & 0xFF) : ushort(1);
                const TColorRGB rgbBgKitty = XTerm256toRGB((uint8_t)bgPalKitty);
                const TColorDesired bgDKitty{rgbBgKitty};
                for (short y = 0; y < size.y; ++y) {
                    const unsigned rowIdx = static_cast<unsigned>(std::min<int>(y, 255));
                    for (short x = 0; x < size.x; ++x) {
                        const unsigned colIdx = static_cast<unsigned>(std::min<int>(x, 255));
                        std::string cell;
                        utf8AppendCodepoint(cell, 0x10EEEEu);
                        utf8AppendCodepoint(cell, kKittyPlaceholderDiacritic[rowIdx]);
                        utf8AppendCodepoint(cell, kKittyPlaceholderDiacritic[colIdx]);
                        const TColorAttr attr{fgDKitty, bgDKitty};
                        b.moveStr(x, TStringView(cell.data(), cell.size()), attr, 1);
                    }
                    writeLine(0, y, size.x, 1, b);
                }
                THardwareInfo::flushScreen();
#endif
                return;
            }
        } else {
            for (short y = 0; y < size.y; ++y) {
                for (short x = 0; x < size.x; ++x) {
                    const bool d = ((x + y) & 1) != 0;
                    b.moveChar(x, char(d ? '\xb7' : ' '), TColorAttr{d ? fgD : bgD, bgD}, 1);
                }
                writeLine(0, y, size.x, 1, b);
            }
            return;
        }
        const int sx = std::max(1, (int)size.x);
        const int sy = std::max(1, (int)size.y);
        const int bands = 2 * sy;
        const float imgWf = static_cast<float>(imgW);
        const float imgHf = static_cast<float>(imgH);
        int twHb = 0;
        int thHb = 0;
        miniPreviewGridToPixels(sx, sy, 4096, twHb, thHb);
        int nwHb = 0, nhHb = 0, oxHb = 0, oyHb = 0;
        miniPhotoCoverRect(imgW, imgH, twHb, thHb, nwHb, nhHb, oxHb, oyHb);
        const auto sampleHb = [&](float pxf, float pyf, unsigned &r, unsigned &g, unsigned &bb) {
            if (nwHb <= 0 || nhHb <= 0) {
                r = g = bb = 0;
                return;
            }
            const float ix =
                (static_cast<float>(oxHb) + pxf) / static_cast<float>(nwHb) * imgWf;
            const float iy =
                (static_cast<float>(oyHb) + pyf) / static_cast<float>(nhHb) * imgHf;
            sampleRgbBilinearAt(cachedRgb, imgW, imgH, ix, iy, r, g, bb);
        };
        for (short y = 0; y < size.y; ++y) {
            for (short x = 0; x < size.x; ++x) {
                const float px =
                    ((static_cast<float>(x) + 0.5f) / static_cast<float>(sx)) * static_cast<float>(twHb);
                const int kTop = 2 * int(y);
                const int kBot = 2 * int(y) + 1;
                const float pyTop =
                    ((static_cast<float>(kTop) + 0.5f) / static_cast<float>(bands)) * static_cast<float>(thHb);
                const float pyBot =
                    ((static_cast<float>(kBot) + 0.5f) / static_cast<float>(bands)) * static_cast<float>(thHb);
                unsigned rt = 0, gt = 0, bt = 0, rb = 0, gb = 0, bb = 0;
                sampleHb(px, pyTop, rt, gt, bt);
                sampleHb(px, pyBot, rb, gb, bb);
                const auto clip8 = [](unsigned v) -> uint8_t {
                    return (uint8_t)std::min(255u, v);
                };
                const TColorAttr attr{TColorDesired{TColorRGB(clip8(rt), clip8(gt), clip8(bt))},
                                      TColorDesired{TColorRGB(clip8(rb), clip8(gb), clip8(bb))}};
                b.moveStr(x, TStringView(kUpperHalfBlockUtf8), attr, 1);
            }
            writeLine(0, y, size.x, 1, b);
        }
#ifdef HAVE_LIBSIXEL
        if (useSixelMini && sixelOptInFromEnv()) {
            int tw = 0;
            int th = 0;
            miniPreviewGridToPixels((int)size.x, (int)size.y, 2048, tw, th);
            std::vector<uint8_t> scaled;
            compositeRgbCenteredCover(cachedRgb, imgW, imgH, scaled, tw, th);
            std::string sixel;
            const int pc = std::clamp(sixelPaletteColors, 16, 256);
            if (encodeRgbToSixel(scaled, tw, th, pc, sixel)) {
                THardwareInfo::flushScreen();
                emitSixelAtViewOrigin(*this, sixel);
            }
        }
#endif
    }

    ~LibraryImagePreview() override {
#if !defined(_WIN32)
        deleteKittyMiniImageById(kittyImageId);
#endif
    }

private:
    const std::string *pathPtr;
    const std::string *visualBaseDirPtr;
    const std::string *assetRootPtr;
    const ushort *fgPal;
    const ushort *bgPal;
    bool useSixelMini {false};
    bool kittyNativeMini {false};
    int sixelPaletteColors {256};
    unsigned kittyImageId {0xE4u};
    ushort openCmd {0};
    mutable std::string cachedPath;
    mutable std::vector<uint8_t> cachedRgb;
    mutable int cachedW {0};
    mutable int cachedH {0};
#ifndef _WIN32
    short kittyCachedSx {-1};
    short kittyCachedSy {-1};
    int kittyCachedTw {0};
    int kittyCachedTh {0};
    int kittyCachedImgW {0};
    int kittyCachedImgH {0};
    size_t kittyCachedRgbSize {0};
    uint64_t kittyCachedRgbFp {0};
#endif
};

/** Indicador minimo (sin marco/sombra) junto al id de personaje: abre la lista por broadcast. */
class TinyCharIdListGlyph : public TView {
public:
    explicit TinyCharIdListGlyph(const TRect &bounds) noexcept : TView(bounds) {
        options |= ofSelectable | ofFirstClick;
        growMode = 0;
        eventMask |= evMouseDown;
    }

    void handleEvent(TEvent &event) override {
        TView::handleEvent(event);
        if (event.what == evMouseDown) {
            message(owner, evBroadcast, cmVisualPickCharInline, this);
            clearEvent(event);
        }
    }

    void draw() override {
        TDrawBuffer b;
        const TColorAttr cell = getColor(1);
        for (short x = 0; x < size.x; ++x)
            b.moveChar(x, ' ', cell, 1);
        if (size.x > 0)
            b.moveStr(0, TStringView(u8"\u25be"), cell, 1);
        writeLine(0, 0, size.x, 1, b);
    }
};

/**
 * TDialog estandar solo llama endModal() para cmOK/cmCancel/cmYes/cmNo.
 * Los botones con comando propio (p. ej. cmVisualLibAdd) deben cerrar el modal aqui;
 * si no, putEvent(evCommand) llega a execute() pero execView nunca termina con ese codigo.
 */
class VisualLibraryDialog : public TDialog {
public:
    VisualLibraryDialog(const TRect &bounds, TStringView aTitle) noexcept :
        TWindowInit(&TDialog::initFrame),
        TDialog(bounds, aTitle) {
        palette = dpBlueDialog;
    }

    void setPickTarget(TDeskTop *dt, const std::map<std::string, std::vector<std::string>> *ch, char *buf, TInputLine *ln) noexcept {
        pickDesk = dt;
        pickChars = ch;
        pickIdBuf = buf;
        pickIdLine = ln;
    }

    void handleEvent(TEvent &event) override {
        if (event.what == evBroadcast && event.message.command == cmVisualPickCharInline) {
            if (pickDesk && pickChars && pickIdBuf) {
                if (pickChars->empty()) {
                    messageBox(mfInformation | mfOKButton, "No hay personajes en la biblioteca.");
                } else {
                    std::string picked;
                    if (rwPickCharacterIdFromList(pickDesk, *pickChars, picked)) {
                        std::strncpy(pickIdBuf, picked.c_str(), 71);
                        pickIdBuf[71] = '\0';
                        if (pickIdLine) {
                            pickIdLine->setData(static_cast<void *>(pickIdBuf));
                            pickIdLine->drawView();
                        }
                    }
                }
            }
            clearEvent(event);
            return;
        }
        TDialog::handleEvent(event);
        if (event.what == evCommand && (state & sfModal) != 0) {
            switch (event.message.command) {
            case cmVisualLibAdd:
            case cmVisualBrowseCharPath:
            case cmVisualBrowseBgPath:
                endModal(event.message.command);
                clearEvent(event);
                break;
            default:
                break;
            }
        }
    }

    TColorAttr mapColor(uchar index) override {
        if (index == 0x2E || index == 0x4E || index == 0x6E)
            return TColorAttr(0x00);
        return TDialog::mapColor(index);
    }

private:
    TDeskTop *pickDesk {nullptr};
    const std::map<std::string, std::vector<std::string>> *pickChars {nullptr};
    char *pickIdBuf {nullptr};
    TInputLine *pickIdLine {nullptr};
};

class ImageNavPickerListView : public NavigatorListView {
public:
    ImageNavPickerListView(const TRect &bounds, const ushort *themeText, const ushort *themeBack) noexcept :
        NavigatorListView(bounds, themeText, themeBack) {
    }

    void handleEvent(TEvent &event) override {
        NavItem before {};
        const bool hadBefore = peekCursorItem(before);
        NavigatorListView::handleEvent(event);
        NavItem after {};
        const bool hadAfter = peekCursorItem(after);
        const bool changed = hadBefore != hadAfter ||
            (hadBefore && hadAfter &&
             (before.fullPath != after.fullPath || before.label != after.label || before.isDirectory != after.isDirectory));
        if (changed && owner)
            message(owner, evBroadcast, cmImageNavCursorChanged, this);
    }
};

class PathStripView : public TView {
public:
    PathStripView(const TRect &bounds, const std::string *pathPtr, const int *offsetPtr) noexcept :
        TView(bounds), pathPtr(pathPtr), offsetPtr(offsetPtr) {
        growMode = gfGrowHiX;
    }

    void setOffsetPtr(const int *ptr) noexcept {
        offsetPtr = ptr;
    }

    void draw() override {
        TDrawBuffer b;
        const TColorAttr cell = getColor(1);
        b.moveChar(0, ' ', cell, size.x);
        const std::string src = pathPtr ? *pathPtr : std::string{};
        const int off = std::max(0, offsetPtr ? *offsetPtr : 0);
        if (!src.empty() && off < (int)src.size()) {
            std::string s = src.substr((size_t)off);
            if ((int)s.size() > size.x)
                s.resize((size_t)size.x);
            b.moveStr(0, TStringView(s), cell, size.x);
        }
        writeLine(0, 0, size.x, 1, b);
    }

private:
    const std::string *pathPtr;
    const int *offsetPtr;
};

class ImageNavPickerDialog : public TDialog {
public:
    ImageNavPickerDialog(const TRect &bounds, TStringView aTitle) noexcept :
        TWindowInit(&TDialog::initFrame),
        TDialog(bounds, aTitle) {
        palette = dpBlueDialog;
    }

    void bind(ImageNavPickerListView *v, TInputLine *line, char *lineBuf, std::string *selectedPath, PathStripView *stripView,
              TScrollBar *hScroll, std::string *previewPath, LibraryImagePreview *previewView) noexcept {
        list = v;
        pathLine = line;
        pathLineBuf = lineBuf;
        selectedPathPtr = selectedPath;
        selectedPathStrip = stripView;
        if (selectedPathStrip)
            selectedPathStrip->setOffsetPtr(&selectedOffset);
        selectedPathScroll = hScroll;
        previewPathPtr = previewPath;
        preview = previewView;
    }

    void refreshFromSelection() {
        if (!list || !pathLine || !pathLineBuf || !selectedPathPtr || !previewPathPtr)
            return;
        NavigatorListView::NavItem it {};
        const bool hasItem = list->peekCursorItem(it);
        std::string shownPath = hasItem ? trim(it.fullPath) : std::string{};
        *selectedPathPtr = shownPath;
        selectedOffset = 0;
        /* La barra horizontal controla el scroll del listado; al cambiar cursor se resincroniza. */
        if (selectedPathScroll)
            syncListScrollBar();
        if (selectedPathStrip)
            selectedPathStrip->drawView();
        if (hasItem && !it.isDirectory && rwPathLooksLikeRasterImage(it.fullPath))
            *previewPathPtr = it.fullPath;
        else
            previewPathPtr->clear();
        if (preview)
            preview->drawView();
    }

    void handleEvent(TEvent &event) override {
        if (event.what == evBroadcast && event.message.command == cmImageNavCursorChanged) {
            refreshFromSelection();
            clearEvent(event);
            return;
        }
        if (event.what == evBroadcast && event.message.command == cmScrollBarChanged &&
            event.message.infoPtr == selectedPathScroll) {
            /* La misma barra "inferior" desplaza el listado de archivos (estilo navegador principal). */
            if (list && selectedPathScroll)
                list->externalSetScrollValue(selectedPathScroll->value);
            refreshFromSelection();
            if (selectedPathStrip)
                selectedPathStrip->drawView();
            clearEvent(event);
            return;
        }
        TDialog::handleEvent(event);
    }

private:
    void syncListScrollBar() {
        if (!list || !selectedPathScroll)
            return;
        /* Rango/pagina derivados del viewport real de NavigatorListView. */
        const int maxScroll = list->externalScrollMax();
        const int value = std::min(maxScroll, std::max(0, list->externalScrollValue()));
        const int page = std::max(1, list->getBounds().b.y - list->getBounds().a.y - 2);
        selectedPathScroll->setParams(value, 0, maxScroll, page, 1);
    }

    ImageNavPickerListView *list {nullptr};
    TInputLine *pathLine {nullptr};
    char *pathLineBuf {nullptr};
    std::string *selectedPathPtr {nullptr};
    PathStripView *selectedPathStrip {nullptr};
    TScrollBar *selectedPathScroll {nullptr};
    std::string *previewPathPtr {nullptr};
    LibraryImagePreview *preview {nullptr};
    int selectedOffset {0};
};

class VisualSceneDialog : public TDialog {
public:
    std::function<void()> onRefresh;
    VisualSceneDialog(const TRect &bounds, TStringView aTitle) noexcept :
        TWindowInit(&TDialog::initFrame),
        TDialog(bounds, aTitle) {
        palette = dpBlueDialog;
    }

    void handleEvent(TEvent &event) override {
        TDialog::handleEvent(event);
        if (event.what == evCommand && (state & sfModal) != 0) {
            switch (event.message.command) {
            case cmVisualSceneRefresh:
                if (onRefresh)
                    onRefresh();
                clearEvent(event);
                break;
            case cmVisualSaveDefaults:
            case cmVisualSceneDelBg:
            case cmVisualSceneDelC2:
            case cmVisualSceneDelC3:
            case cmVisualSceneOpenLibrary:
                endModal(event.message.command);
                clearEvent(event);
                break;
            default:
                break;
            }
        }
    }

    TColorAttr mapColor(uchar index) override {
        /* Sombra de dialogo solida para no "ensuciar" sombras de vistas de abajo. */
        if (index == 0x2E || index == 0x4E || index == 0x6E)
            return TColorAttr(0x00);
        return TDialog::mapColor(index);
    }
};

class VisualGalleryDialog : public TDialog {
public:
    VisualGalleryDialog(const TRect &bounds, TStringView aTitle) noexcept :
        TWindowInit(&TDialog::initFrame),
        TDialog(bounds, aTitle) {
        palette = dpBlueDialog;
    }

    void handleEvent(TEvent &event) override {
        TDialog::handleEvent(event);
        if (event.what == evCommand && (state & sfModal) != 0) {
            switch (event.message.command) {
            case cmVisualGalPrevId:
            case cmVisualGalNextId:
            case cmVisualGalPrevVar:
            case cmVisualGalNextVar:
                endModal(event.message.command);
                clearEvent(event);
                break;
            default:
                break;
            }
        }
    }
};

class UnicodeBackground : public TBackground {
public:
    UnicodeBackground(const TRect &bounds, const std::string &patternUtf8,
                      const ushort *fg, const ushort *bg) noexcept :
        TBackground(bounds, ' '), utf8Pattern(patternUtf8), fgColor(fg), bgColor(bg) {}

    void setPatternUtf8(const std::string &s) {
        utf8Pattern = s.empty() ? "\xb0" : s;
        drawView();
    }

    virtual void draw() override {
        TDrawBuffer b;
        TColorAttr attr(
            TColorDesired(TColorXTerm(uchar((*fgColor) & 0xFF))),
            TColorDesired(TColorXTerm(uchar((*bgColor) & 0xFF)))
        );
        for (short y = 0; y < size.y; ++y) {
            b.moveChar(0, ' ', attr, size.x);
            for (short x = 0; x < size.x; ++x) {
                b.moveStr(x, utf8Pattern.c_str(), attr, 1);
            }
            writeLine(0, y, size.x, 1, b);
        }
    }

private:
    std::string utf8Pattern;
    const ushort *fgColor;
    const ushort *bgColor;
};

/** Lado del lienzo logico (rejilla = pixeles simulados). */
static constexpr int kPixelGrid = 1000;

static bool rectInkMajority(const std::vector<uint8_t> *pixels, int x0, int x1, int y0, int y1) {
    if (!pixels || (int)pixels->size() < kPixelGrid * kPixelGrid)
        return false;
    int ink = 0, tot = 0;
    const int xa = std::max(0, x0);
    const int xb = std::min(x1, kPixelGrid);
    const int ya = std::max(0, y0);
    const int yb = std::min(y1, kPixelGrid);
    for (int yy = ya; yy < yb; ++yy)
        for (int xx = xa; xx < xb; ++xx) {
            ++tot;
            if ((*pixels)[(size_t)yy * (size_t)kPixelGrid + (size_t)xx])
                ++ink;
        }
    if (tot <= 0)
        return false;
    return ink * 2 > tot;
}

/** Mini bajo el panel: PNG/JPG si hay ruta; si no, rejilla 1000×1000. Foto vía Kitty nativo, U+2580+truecolor o Sixel según preferencias y entorno. */
class PixelPreviewView : public TView {
public:
    PixelPreviewView(const TRect &bounds, const std::vector<uint8_t> *pix, const std::vector<uint8_t> *imgRgb,
                     const int *imgRgbWPtr, const int *imgRgbHPtr, const ushort *fg, const ushort *bg, bool useSixelMini,
                     bool kittyNativeMini, int sixelPaletteColors, unsigned kittyImgId) noexcept :
        TView(bounds),
        pixels(pix),
        imageRgb(imgRgb),
        imgWPtr(imgRgbWPtr),
        imgHPtr(imgRgbHPtr),
        fgColor(fg),
        bgColor(bg),
        useSixelMini(useSixelMini),
        kittyNativeMini(kittyNativeMini),
        sixelPaletteColors(sixelPaletteColors),
        kittyImageId(kittyImgId) {
        /* growMode no nulo: el lienzo sigue al redimensionar la ventana Mini (con 0 quedaba imagen en un rincón). */
        growMode = gfGrowHiX | gfGrowHiY;
    }

    virtual void draw() override {
        TDrawBuffer b;
        const int imgW = (imgWPtr && *imgWPtr > 0) ? *imgWPtr : 0;
        const int imgH = (imgHPtr && *imgHPtr > 0) ? *imgHPtr : 0;
        const bool usePhoto = imageRgb && imgW > 0 && imgH > 0 &&
            imageRgb->size() >= (size_t)imgW * (size_t)imgH * 3u;
        if (usePhoto) {
#if !defined(_WIN32)
            const bool useKittyPh = kittyUnicodePlaceholdersEnabled(kittyNativeMini);
#else
            const bool useKittyPh = false;
#endif
            const int sx = std::max(1, (int)size.x);
            const int sy = std::max(1, (int)size.y);

            if (useKittyPh) {
#ifndef _WIN32
                int tw = 0;
                int th = 0;
                miniPreviewGridToPixels(sx, sy, 4096, tw, th);
                const uint64_t rgbFp = rgbFingerprint64(*imageRgb);
                const bool needUpload = kittyCachedSx != size.x || kittyCachedSy != size.y || kittyCachedTw != tw ||
                    kittyCachedTh != th || kittyCachedImgW != imgW || kittyCachedImgH != imgH ||
                    kittyCachedRgbSize != imageRgb->size() || kittyCachedRgbFp != rgbFp;
                if (needUpload) {
                    deleteKittyMiniImageById(kittyImageId);
                    std::vector<uint8_t> scaled;
                    compositeRgbCenteredCover(*imageRgb, imgW, imgH, scaled, tw, th);
                    writeKittyRgbTransmitOnly(scaled.data(), tw, th, kittyImageId);
                    writeKittyVirtualPlacementUnicode(kittyImageId, sx, sy);
                    kittyCachedSx = size.x;
                    kittyCachedSy = size.y;
                    kittyCachedTw = tw;
                    kittyCachedTh = th;
                    kittyCachedImgW = imgW;
                    kittyCachedImgH = imgH;
                    kittyCachedRgbSize = imageRgb->size();
                    kittyCachedRgbFp = rgbFp;
                }
                const TColorDesired fgD{TColorXTerm{static_cast<uint8_t>(kittyImageId)}};
                const ushort bgPalKitty = bgColor ? (*bgColor & 0xFF) : ushort(1);
                const TColorRGB rgbBg = XTerm256toRGB((uint8_t)bgPalKitty);
                const TColorDesired bgD{rgbBg};
                for (short y = 0; y < size.y; ++y) {
                    const unsigned rowIdx = static_cast<unsigned>(std::min<int>(y, 255));
                    for (short x = 0; x < size.x; ++x) {
                        const unsigned colIdx = static_cast<unsigned>(std::min<int>(x, 255));
                        std::string cell;
                        utf8AppendCodepoint(cell, 0x10EEEEu);
                        utf8AppendCodepoint(cell, kKittyPlaceholderDiacritic[rowIdx]);
                        utf8AppendCodepoint(cell, kKittyPlaceholderDiacritic[colIdx]);
                        const TColorAttr attr{fgD, bgD};
                        b.moveStr(x, TStringView(cell.data(), cell.size()), attr, 1);
                    }
                    writeLine(0, y, size.x, 1, b);
                }
                THardwareInfo::flushScreen();
#endif
                return;
            }

            const int bands = 2 * sy;
            const float imgWf = static_cast<float>(imgW);
            const float imgHf = static_cast<float>(imgH);
            int twHb = 0;
            int thHb = 0;
            miniPreviewGridToPixels(sx, sy, 4096, twHb, thHb);
            int nwHb, nhHb, oxHb, oyHb;
            miniPhotoCoverRect(imgW, imgH, twHb, thHb, nwHb, nhHb, oxHb, oyHb);
            const auto sampleHb = [&](float pxf, float pyf, unsigned &r, unsigned &g, unsigned &b) {
                if (nwHb <= 0 || nhHb <= 0) {
                    r = g = b = 0;
                    return;
                }
                const float ix =
                    (static_cast<float>(oxHb) + pxf) / static_cast<float>(nwHb) * imgWf;
                const float iy =
                    (static_cast<float>(oyHb) + pyf) / static_cast<float>(nhHb) * imgHf;
                sampleRgbBilinearAt(*imageRgb, imgW, imgH, ix, iy, r, g, b);
            };
            for (short y = 0; y < size.y; ++y) {
                for (short x = 0; x < size.x; ++x) {
                    const float px =
                        ((static_cast<float>(x) + 0.5f) / static_cast<float>(sx)) * static_cast<float>(twHb);
                    const int kTop = 2 * int(y);
                    const int kBot = 2 * int(y) + 1;
                    const float pyTop =
                        ((static_cast<float>(kTop) + 0.5f) / static_cast<float>(bands)) * static_cast<float>(thHb);
                    const float pyBot =
                        ((static_cast<float>(kBot) + 0.5f) / static_cast<float>(bands)) * static_cast<float>(thHb);
                    unsigned rt = 0, gt = 0, bt = 0, rb = 0, gb = 0, bb = 0;
                    sampleHb(px, pyTop, rt, gt, bt);
                    sampleHb(px, pyBot, rb, gb, bb);
                    const auto clip8 = [](unsigned v) -> uint8_t {
                        return (uint8_t)std::min(255u, v);
                    };
                    const TColorAttr attr{
                        TColorDesired{TColorRGB(clip8(rt), clip8(gt), clip8(bt))},
                        TColorDesired{TColorRGB(clip8(rb), clip8(gb), clip8(bb))}};
                    b.moveStr(x, TStringView(kUpperHalfBlockUtf8), attr, 1);
                }
                writeLine(0, y, size.x, 1, b);
            }
#ifdef HAVE_LIBSIXEL
            /* Sin opt-in de Sixel no se reescala ni codifica (ruido en TTY y trabajo innecesario). */
            if (useSixelMini && sixelOptInFromEnv() && !useKittyPh) {
                int tw = 0;
                int th = 0;
                miniPreviewGridToPixels((int)size.x, (int)size.y, 2048, tw, th);
                std::vector<uint8_t> scaled;
                compositeRgbCenteredCover(*imageRgb, imgW, imgH, scaled, tw, th);
                std::string sixel;
                const int pc = std::clamp(sixelPaletteColors, 16, 256);
                if (encodeRgbToSixel(scaled, tw, th, pc, sixel)) {
                    THardwareInfo::flushScreen();
                    emitSixelAtViewOrigin(*this, sixel);
                }
            }
#endif
            return;
        }

        const ushort fg = fgColor ? (*fgColor & 0xFF) : ushort(15);
        const ushort bg = bgColor ? (*bgColor & 0xFF) : ushort(1);
        const TColorRGB rgbFg = XTerm256toRGB((uint8_t)fg);
        const TColorRGB rgbBg = XTerm256toRGB((uint8_t)bg);
        const TColorDesired fgD{rgbFg};
        const TColorDesired bgD{rgbBg};
        const int sx = std::max(1, (int)size.x);
        const int sy = std::max(1, (int)size.y);
        const int bands = 2 * sy;
        for (short y = 0; y < size.y; ++y) {
            for (short x = 0; x < size.x; ++x) {
                const int x0 = int(x) * kPixelGrid / sx;
                const int x1 = std::max(x0 + 1, int(x + 1) * kPixelGrid / sx);
                const int kTop = 2 * int(y);
                const int kBot = 2 * int(y) + 1;
                const int yaT = kTop * kPixelGrid / bands;
                const int ybT = std::max(yaT + 1, (kTop + 1) * kPixelGrid / bands);
                const int yaB = kBot * kPixelGrid / bands;
                const int ybB = std::max(yaB + 1, (kBot + 1) * kPixelGrid / bands);
                const bool topInk = rectInkMajority(pixels, x0, x1, yaT, std::min(ybT, kPixelGrid));
                const bool botInk = rectInkMajority(pixels, x0, x1, yaB, std::min(ybB, kPixelGrid));
                if (topInk && botInk)
                    b.moveChar(x, '\xdb', TColorAttr{fgD, fgD}, 1);
                else if (topInk || botInk) {
                    const TColorAttr attr{
                        TColorDesired(topInk ? rgbFg : rgbBg),
                        TColorDesired(botInk ? rgbFg : rgbBg)};
                    b.moveStr(x, TStringView(kUpperHalfBlockUtf8), attr, 1);
                } else
                    b.moveChar(x, ' ', TColorAttr{bgD, bgD}, 1);
            }
            writeLine(0, y, size.x, 1, b);
        }
    }

private:
    const std::vector<uint8_t> *pixels;
    const std::vector<uint8_t> *imageRgb;
    const int *imgWPtr {nullptr};
    const int *imgHPtr {nullptr};
    const ushort *fgColor;
    const ushort *bgColor;
    bool useSixelMini {false};
    bool kittyNativeMini {false};
    int sixelPaletteColors {256};
    unsigned kittyImageId {241};
#ifndef _WIN32
    short kittyCachedSx {-1};
    short kittyCachedSy {-1};
    int kittyCachedTw {0};
    int kittyCachedTh {0};
    int kittyCachedImgW {0};
    int kittyCachedImgH {0};
    size_t kittyCachedRgbSize {0};
    uint64_t kittyCachedRgbFp {0};
#endif
};

/**
 * TFileDialog "Elegir imagen" con la misma vista previa que las ventanas Mini (Kitty / Sixel / truecolor),
 * no solo bloques Unicode.
 */
class RwImagePickDialog : public TFileDialog {
    std::vector<uint8_t> pixelStub;
    std::vector<uint8_t> previewRgb;
    int previewW {0};
    int previewH {0};
    PixelPreviewView *imgPreview {nullptr};
    bool miniSixel;
    bool miniKitty;
    int sixelColors;
    ushort *txCol;
    ushort *bkCol;
    unsigned kittyImgId;

    void refreshImagePreview() {
        char pathBuf[MAXPATH];
        getFileName(pathBuf);
        std::string p = trim(std::string(pathBuf));
        previewRgb.clear();
        previewW = previewH = 0;
        std::error_code ec;
        if (!p.empty() && p.find('*') == std::string::npos && p.find('?') == std::string::npos) {
            const fs::path fp(p);
            if (!fs::is_directory(fp, ec) && rwPathLooksLikeRasterImage(p) && fs::is_regular_file(fp, ec) && !ec) {
                int w = 0, h = 0, comp = 0;
                unsigned char *data = stbi_load(p.c_str(), &w, &h, &comp, 3);
                if (data && w > 0 && h > 0) {
                    previewRgb.assign(data, data + (size_t)w * (size_t)h * 3u);
                    stbi_image_free(data);
                    previewW = w;
                    previewH = h;
                    downscaleRgbImageForMini(previewRgb, previewW, previewH, 2048);
                }
            }
        }
        if (imgPreview)
            imgPreview->drawView();
    }

public:
    RwImagePickDialog(TStringView wild, TStringView title, ushort *tc, ushort *bk, bool useSixelMini, bool kittyNativeMini,
                      int sixelPaletteColors, unsigned kittyId) noexcept :
        TWindowInit(&TDialog::initFrame),
        TFileDialog(wild, title, "~A~rchivo", fdOpenButton, 0),
        miniSixel(useSixelMini),
        miniKitty(kittyNativeMini),
        sixelColors(sixelPaletteColors),
        txCol(tc),
        bkCol(bk),
        kittyImgId(kittyId) {
        pixelStub.assign(4, 0);
        TRect box = getBounds();
        /* Ensancha y recentra: lista principal a la izquierda + preview a la derecha sin tapar botones. */
        const short growW = 20;
        box.grow(growW / 2, 0);
        if (TProgram::application) {
            TRect app = TProgram::application->getBounds();
            if (box.a.x < app.a.x)
                box.move(app.a.x - box.a.x, 0);
            if (box.b.x > app.b.x)
                box.move(app.b.x - box.b.x, 0);
        }
        locate(box);
        short previewLeft = short(size.x - 24);
        short previewTop = 3;
        if (fileList) {
            fileList->numCols = 1;
            TRect lb = fileList->getBounds();
            /* Mantener columna de archivos compacta y ceder el ancho extra a la preview. */
            const short targetRight = short(std::min<int>(lb.a.x + 23, size.x - 30));
            lb.b.x = targetRight;
            fileList->changeBounds(lb);
            if (fileList->hScrollBar) {
                TRect sb = fileList->hScrollBar->getBounds();
                sb.b.x = targetRight;
                fileList->hScrollBar->changeBounds(sb);
            }
            previewLeft = short(targetRight + 2);
            previewTop = lb.a.y;
        }
        const short prevL = previewLeft;
        const short prevR = short(size.x - 14);
        const short prevT = previewTop;
        const short prevB = short(size.y - 8);
        imgPreview = new PixelPreviewView(TRect(prevL, prevT, prevR, prevB), &pixelStub, &previewRgb, &previewW, &previewH, txCol,
                                          bkCol,
#ifdef HAVE_LIBSIXEL
                                          miniSixel,
#else
                                          false,
#endif
#if !defined(_WIN32)
                                          miniKitty,
#else
                                          false,
#endif
#ifdef HAVE_LIBSIXEL
                                          sixelColors,
#else
                                          256,
#endif
                                          kittyImgId);
        imgPreview->growMode = gfGrowLoX | gfGrowHiX | gfGrowHiY;
        insertBefore(imgPreview, static_cast<TView *>(fileList));
        refreshImagePreview();
    }

    void handleEvent(TEvent &event) override {
        if (event.what == evMouseWheel && fileList) {
            if (owner)
                owner->setCurrent(fileList, normalSelect);
            TEvent k {};
            k.what = evKeyDown;
            switch (event.mouse.wheel) {
                case mwUp:
                case mwLeft:
                    k.keyDown.keyCode = kbUp;
                    break;
                case mwDown:
                case mwRight:
                    k.keyDown.keyCode = kbDown;
                    break;
                default:
                    k.keyDown.keyCode = kbNoKey;
                    break;
            }
            if (k.keyDown.keyCode != kbNoKey) {
                fileList->handleEvent(k);
                clearEvent(event);
                refreshImagePreview();
                return;
            }
        }
        TFileDialog::handleEvent(event);
        if (event.what == evBroadcast && event.message.command == cmFileFocused)
            refreshImagePreview();
    }

    ~RwImagePickDialog() {
#if !defined(_WIN32)
        deleteKittyMiniImageById(kKittyImagePickDialogId);
#endif
    }
};

/** Cuenta "palabras" como bloques separados por espacio (isspace); \\r se ignora. */
static size_t countWordsInEditorBuffer(TEditor *ed) noexcept {
    if (!ed || ed->bufLen == 0)
        return 0;
    size_t n = 0;
    bool inWord = false;
    for (uint i = 0; i < ed->bufLen; ++i) {
        unsigned char c = (unsigned char)ed->bufChar(i);
        if (c == '\r')
            continue;
        if (std::isspace(c))
            inWord = false;
        else if (!inWord) {
            ++n;
            inWord = true;
        }
    }
    return n;
}

/** Pie del editor: palabras + * si hay cambios sin guardar. */
class WordCountIndicator : public TIndicator {
    TEditor *linked {nullptr};
    bool primed {false};
    bool lastSeenModified {false};
    bool saveAckActive {false};
    std::chrono::steady_clock::time_point saveAckUntil {};
    bool titlePrimed {false};
    bool lastDirtyForTitle {false};

public:
    WordCountIndicator(const TRect &bounds, TEditor *ed) noexcept : TIndicator(bounds), linked(ed) {
        growMode = gfGrowHiX | gfGrowLoY | gfGrowHiY;
    }

    /** Tras idle(): si expiró el acuse de guardado, debe repintarse la franja. */
    bool expireSaveAckIfNeeded() noexcept {
        if (!saveAckActive)
            return false;
        if (std::chrono::steady_clock::now() >= saveAckUntil) {
            saveAckActive = false;
            return true;
        }
        return false;
    }

    virtual void draw() override {
        /* Franja horizontal como TIndicator (CP437 0xC4 / 0xCD). */
        TDrawBuffer b;
        TAttrPair color;
        char frame;
        if ((state & sfDragging) == 0) {
            color = getColor(1);
            frame = '\xC4';
        } else {
            color = getColor(2);
            frame = '\xCD';
        }
        b.moveChar(0, frame, color, size.x);
        const auto now = std::chrono::steady_clock::now();
        if (linked) {
            const bool nowMod = linked->modified;
            if (primed) {
                if (lastSeenModified && !nowMod) {
                    saveAckActive = true;
                    saveAckUntil = now + std::chrono::milliseconds(2200);
                }
            } else
                primed = true;
            lastSeenModified = nowMod;
        }
        const bool showAck = saveAckActive && now < saveAckUntil;
        const size_t nw = linked ? countWordsInEditorBuffer(linked) : 0;
        const bool dirty = linked && linked->modified;
        if (owner) {
            if (TWindow *win = dynamic_cast<TWindow *>(owner)) {
                if (!titlePrimed) {
                    titlePrimed = true;
                    lastDirtyForTitle = dirty;
                } else if (dirty != lastDirtyForTitle) {
                    lastDirtyForTitle = dirty;
                    if (win->frame)
                        win->frame->drawView();
                }
            }
        }
        /* El asterisco de modificado se coloca junto a "Palabras:" (columna 2). */
        char txt[96];
        std::snprintf(txt, sizeof(txt), "%sPalabras: %zu%s", dirty ? "* " : "  ", nw,
                      showAck ? " | OK guardado" : "");
        b.moveStr(2, TStringView(txt), color);
        writeBuf(0, 0, size.x, 1, b);
    }
};

/** Rueda del ratón; barras de desplazamiento ocultas al activarse. */
class RetroFileEditor : public TFileEditor {
public:
    RetroFileEditor(const TRect &bounds, TScrollBar *hs, TScrollBar *vs, TIndicator *ind, TStringView fn) noexcept :
        TFileEditor(bounds, hs, vs, ind, fn) {
        eventMask |= evMouseWheel;
    }

    void requestInitialViewportReset() {
        pendingInitialViewportReset = true;
    }

    void setAutoEmDashFromDoubleHyphen(bool enabled) {
        autoEmDashFromDoubleHyphen = enabled;
    }

    struct UndoSnapshot {
        std::string text;
        uint curPtr {0};
        int dx {0};
        int dy {0};
    };

    std::string makeSnapshotText() {
        std::string s;
        if (bufLen == 0)
            return s;
        s.resize((size_t)bufLen);
        getText(0, TSpan<char>(&s[0], bufLen));
        return s;
    }

    UndoSnapshot makeUndoSnapshot() {
        UndoSnapshot snap;
        snap.text = makeSnapshotText();
        snap.curPtr = curPtr;
        snap.dx = delta.x;
        snap.dy = delta.y;
        return snap;
    }

    void restoreSnapshot(const UndoSnapshot &snap) {
        restoringSnapshot = true;
        setBufLen(0);
        if (!snap.text.empty())
            insertText(snap.text.data(), (uint)snap.text.size(), False);
        const uint targetCur = std::min<uint>(snap.curPtr, bufLen);
        setCurPtr(targetCur, 0);
        scrollTo(snap.dx, snap.dy);
        restoringSnapshot = false;
        update(ufView);
    }

    void handleEvent(TEvent &event) override {
        auto moveByVisualRow = [&](int dir, uchar selMode) -> bool {
            if (dir != -1 && dir != 1)
                return false;
            // Ancho visible del editor en celdas. Si la línea cabe, dejamos el comportamiento normal.
            const int wrapW = std::max<int>(1, (int)size.x);
            const uint ls = lineStart(curPtr);
            const uint le = lineEnd(ls);
            const int lineCols = (int)charPos(ls, le);
            if (lineCols < wrapW)
                return false;

            const int curCol = (int)charPos(ls, curPtr);
            const int colInRow = curCol % wrapW;
            const int lastRow = (lineCols > 0) ? (lineCols - 1) / wrapW : 0;
            const int curRow = curCol / wrapW;

            auto clampToLine = [&](uint start, uint end, int targetCol) -> uint {
                const int maxCol = (int)charPos(start, end);
                const int c = std::max(0, std::min(targetCol, maxCol));
                return charPtr(start, c);
            };

            if (dir < 0) {
                if (curRow > 0) {
                    const int targetCol = curCol - wrapW;
                    setCurPtr(clampToLine(ls, le, targetCol), selMode);
                    return true;
                }
                if (ls == 0)
                    return false;
                const uint pls = prevLine(ls);
                const uint ple = lineEnd(pls);
                const int pCols = (int)charPos(pls, ple);
                const int pLastRow = (pCols > 0) ? (pCols - 1) / wrapW : 0;
                const int targetCol = pLastRow * wrapW + colInRow;
                setCurPtr(clampToLine(pls, ple, targetCol), selMode);
                return true;
            } else {
                if (curRow < lastRow) {
                    const int targetCol = curCol + wrapW;
                    setCurPtr(clampToLine(ls, le, targetCol), selMode);
                    return true;
                }
                const uint nls = nextLine(ls);
                if (nls == ls)
                    return false;
                const uint nle = lineEnd(nls);
                const int targetCol = colInRow;
                setCurPtr(clampToLine(nls, nle, targetCol), selMode);
                return true;
            }
        };

        if (event.what == evKeyDown) {
            const auto &kd = event.keyDown;
            const ushort k = ctrlToArrow(kd.keyCode);
            if (k == kbUp || k == kbDown) {
                const uchar selMode = ((kd.controlKeyState & kbShift) != 0) ? smExtend : 0;
                if (moveByVisualRow(k == kbUp ? -1 : 1, selMode)) {
                    clearEvent(event);
                    return;
                }
            }
        }

        if (event.what == evCommand) {
            if (event.message.command == cmLineUp) {
                if (moveByVisualRow(-1, 0)) {
                    clearEvent(event);
                    return;
                }
            } else if (event.message.command == cmLineDown) {
                if (moveByVisualRow(1, 0)) {
                    clearEvent(event);
                    return;
                }
            }
        }

        if (event.what == evKeyDown) {
            /* Fallback para terminales donde Ctrl+Shift+Flecha llega igual que Ctrl+Flecha. */
            const auto &kd = event.keyDown;
            if (kd.keyCode == kbCtrlZ || kd.charScan.charCode == 26) {
                if (!undoSnapshots.empty()) {
                    const UndoSnapshot snap = undoSnapshots.back();
                    undoSnapshots.pop_back();
                    restoreSnapshot(snap);
                }
                clearEvent(event);
                return;
            }
            /* Fallback ANSI CSI: cubre variantes de Ctrl+Shift+Flechas/Home/End entre terminales. */
            auto parseCsiMoveCmd = [&](const TEvent &ev) -> ushort {
                const auto &k = ev.keyDown;
                if (k.textLength < 4 || k.text[0] != 27 || k.text[1] != '[')
                    return 0;
                std::string seq(k.text, k.text + k.textLength);
                const char tail = seq.back();
                if (tail != 'D' && tail != 'C' && tail != 'H' && tail != 'F' && tail != '~')
                    return 0;
                std::vector<int> params;
                int current = 0;
                bool haveNum = false;
                for (size_t i = 2; i + 1 < seq.size(); ++i) {
                    const char ch = seq[i];
                    if (ch >= '0' && ch <= '9') {
                        current = current * 10 + (ch - '0');
                        haveNum = true;
                    } else if (ch == ';') {
                        params.push_back(haveNum ? current : 0);
                        current = 0;
                        haveNum = false;
                    } else {
                        return 0;
                    }
                }
                if (haveNum)
                    params.push_back(current);
                int mod = 0;
                if (params.size() >= 2)
                    mod = params[1];
                else if (params.size() == 1)
                    mod = params[0];
                /* 5=Ctrl, 6=Ctrl+Shift, 7/8=Alt variants con Ctrl (segun emulador). */
                if (mod != 5 && mod != 6 && mod != 7 && mod != 8)
                    return 0;
                if (tail == '~') {
                    const int lead = params.empty() ? 0 : params[0];
                    if (lead == 1 || lead == 7)
                        return cmTextStart;
                    if (lead == 4 || lead == 8)
                        return cmTextEnd;
                    return 0;
                }
                switch (tail) {
                    case 'D': return cmWordLeft;
                    case 'C': return cmWordRight;
                    case 'H': return cmTextStart;
                    case 'F': return cmTextEnd;
                    default: return 0;
                }
            };
            const ushort csiMoveCmd = parseCsiMoveCmd(event);
            if (csiMoveCmd != 0) {
                message(this, evCommand, cmStartSelect, nullptr);
                message(this, evCommand, csiMoveCmd, nullptr);
                clearEvent(event);
                return;
            }
            const bool ctrlHeld = (kd.controlKeyState & kbCtrlShift) != 0;
            ushort moveCmd = 0;
            switch (kd.keyCode) {
                case kbCtrlLeft: moveCmd = cmWordLeft; break;
                case kbCtrlRight: moveCmd = cmWordRight; break;
                case kbCtrlHome: moveCmd = cmTextStart; break;
                case kbCtrlEnd: moveCmd = cmTextEnd; break;
                /* Algunos emuladores reportan variantes Alt para Ctrl+Shift+Flecha. */
                case kbAltLeft: moveCmd = cmWordLeft; break;
                case kbAltRight: moveCmd = cmWordRight; break;
                case kbAltHome: moveCmd = cmTextStart; break;
                case kbAltEnd: moveCmd = cmTextEnd; break;
                case kbLeft:
                    /* Algunos terminales pierden Shift cuando va junto con Ctrl+Flecha. */
                    if (ctrlHeld) moveCmd = cmWordLeft;
                    break;
                case kbRight:
                    if (ctrlHeld) moveCmd = cmWordRight;
                    break;
                case kbHome:
                    if (ctrlHeld) moveCmd = cmTextStart;
                    break;
                case kbEnd:
                    if (ctrlHeld) moveCmd = cmTextEnd;
                    break;
                default: break;
            }
            if (moveCmd != 0) {
                message(this, evCommand, cmStartSelect, nullptr);
                message(this, evCommand, moveCmd, nullptr);
                clearEvent(event);
                return;
            }
        }
        auto runEditorContextMenu = [&](TPoint whereGlobal) -> bool {
            TGroup *g = owner;
            while (g && dynamic_cast<TDeskTop *>(g) == nullptr)
                g = g->owner;
            TDeskTop *desk = dynamic_cast<TDeskTop *>(g);
            if (!desk)
                return false;

            TMenuItem *chain = new TMenuItem("~U~ndo", cmUndo, kbCtrlZ, hcNoContext, "Ctrl-Z");
            chain = new TMenuItem("~P~egar", cmPaste, kbCtrlV, hcNoContext, "Ctrl-V", chain);
            chain = new TMenuItem("~C~opiar", cmCopy, kbCtrlC, hcNoContext, "Ctrl-C", chain);
            chain = new TMenuItem("Cor~t~ar", cmCut, kbCtrlX, hcNoContext, "Ctrl-X", chain);

            TRect bounds(whereGlobal, whereGlobal);
            TMenu *menu = new TMenu(*chain);
            TMenuPopup *menuPopup = new TMenuPopup(bounds, menu);
            rwAutoPlaceMenuPopupOnDesk(desk, menuPopup, whereGlobal);
            const ushort cmd = desk->execView(menuPopup);
            TObject::destroy(menuPopup);
            if (cmd == cmCut || cmd == cmCopy || cmd == cmPaste || cmd == cmUndo)
                message(this, evCommand, cmd, nullptr);
            return true;
        };

        if (event.what == evMouseDown && (event.mouse.buttons & mbRightButton) != 0) {
            runEditorContextMenu(event.mouse.where);
            clearEvent(event);
            return;
        }
        if (event.what == evKeyDown && event.keyDown.keyCode == kbShiftF10) {
            TPoint p = makeGlobal(TPoint{1, 1});
            runEditorContextMenu(p);
            clearEvent(event);
            return;
        }

        auto isTypingLikeKey = [](const TEvent &ev) -> bool {
            if (ev.what != evKeyDown)
                return false;
            const auto &kd = ev.keyDown;
            const bool ctrlHeld = (kd.controlKeyState & kbCtrlShift) != 0;
            const uchar ch = uchar(kd.charScan.charCode);
            if (kd.keyCode == kbCtrlZ || kd.keyCode == kbCtrlC || kd.keyCode == kbCtrlX || kd.keyCode == kbCtrlV ||
                ch == 26 || ch == 3 || ch == 24 || ch == 22)
                return false;
            /* Atajos Ctrl no deben activar la heurística de autoscroll de tecleo. */
            if (ctrlHeld && (kd.controlKeyState & kbPaste) == 0)
                return false;
            if (kd.textLength > 0)
                return true;
            if (ch == 9 || ch == 13 || ch == 8)
                return true;
            if (ch >= 32u && ch < 255u)
                return true;
            if ((kd.controlKeyState & kbPaste) != 0)
                return true;
            switch (kd.keyCode) {
                case kbDel:
                case kbBack:
                case kbEnter:
                    return true;
                default:
                    return false;
            }
        };
        if (event.what == evMouseWheel) {
            int dx = delta.x;
            int dy = delta.y;
            switch (event.mouse.wheel) {
                case mwUp:
                    dy -= 3;
                    break;
                case mwDown:
                    dy += 3;
                    break;
                case mwLeft:
                    dx -= 3;
                    break;
                case mwRight:
                    dx += 3;
                    break;
                default:
                    break;
            }
            scrollTo(dx, dy);
            clearEvent(event);
            return;
        }
        const bool mayModify = (event.what == evKeyDown || event.what == evCommand);
        UndoSnapshot before;
        if (!restoringSnapshot && mayModify)
            before = makeUndoSnapshot();
        const bool typingLike = isTypingLikeKey(event);
        const bool typedPlainHyphen = (event.what == evKeyDown && event.keyDown.textLength == 1 &&
                                       event.keyDown.text[0] == '-' &&
                                       (event.keyDown.controlKeyState & kbCtrlShift) == 0);
        const int oldDy = delta.y;
        const int oldDx = delta.x;
        TFileEditor::handleEvent(event);
        if (delta.x != 0)
            scrollTo(0, delta.y);
        if (autoEmDashFromDoubleHyphen && typedPlainHyphen && curPtr >= 2) {
            const uint p0 = prevChar(prevChar(curPtr));
            const uint p1 = prevChar(curPtr);
            const char c0 = (char)bufChar(p0);
            const char c1 = (char)bufChar(p1);
            if (c0 == '-' && c1 == '-') {
                // Reemplaza solo el par recien tipeado para mantener la edicion predecible.
                deleteRange(p0, curPtr, True);
                static const char emDash[] = "\xE2\x80\x94"; // UTF-8: —
                insertText(emDash, (uint)sizeof(emDash) - 1u, False);
            }
        }
        if (!restoringSnapshot && mayModify) {
            std::string after = makeSnapshotText();
            if (after != before.text) {
                undoSnapshots.push_back(std::move(before));
                if (undoSnapshots.size() > 512)
                    undoSnapshots.erase(undoSnapshots.begin());
            }
        }
        if (typingLike) {
            const int newDx = delta.x;
            const int newDy = delta.y;
            if (newDy != oldDy) {
                /* Soft-wrap usa fila visual; validar visibilidad con delta anterior evita saltos falsos. */
                delta.y = oldDy;
                const bool stillVisibleAtOldScroll = cursorVisible() == True;
                delta.y = newDy;
                /* Evita "ruido" de autoscroll: solo seguir el cursor si quedo fuera de vista. */
                if (stillVisibleAtOldScroll)
                    scrollTo(oldDx, oldDy);
                else
                    scrollTo(newDx, newDy);
            }
        }
    }

    /** Las barras que TV muestra al activar el editor se ocultan de nuevo para un marco limpio. */
    void setState(ushort aState, Boolean enable) override {
        TFileEditor::setState(aState, enable);
        if (aState == sfActive) {
            if (hScrollBar)
                hScrollBar->hide();
            if (vScrollBar)
                vScrollBar->hide();
            if (delta.x != 0)
                scrollTo(0, delta.y);
            if (enable && pendingInitialViewportReset) {
                scrollTo(0, 0);
                pendingInitialViewportReset = false;
                drawView();
            }
        }
    }

private:
    std::vector<UndoSnapshot> undoSnapshots;
    bool restoringSnapshot {false};
    bool pendingInitialViewportReset {true};
    bool autoEmDashFromDoubleHyphen {false};
};

/** Marco del editor con borde simple siempre (activo/inactivo), manteniendo eventos normales de TFrame. */
class RetroSingleLineFrame : public TFrame {
public:
    RetroSingleLineFrame(const TRect &bounds) noexcept : TFrame(bounds) {}

    void draw() override {
        TDrawBuffer b;
        TColorAttr cFrame = getColor((state & sfActive) ? 0x0503 : 0x0101);
        TColorAttr cTitle = getColor((state & sfActive) ? 0x0004 : 0x0002);

        const short w = size.x;
        const short h = size.y;
        if (w <= 1 || h <= 1)
            return;

        b.moveChar(0, '\xC4', cFrame, w);
        b.putChar(0, '\xDA');
        b.putChar(w - 1, '\xBF');
        writeLine(0, 0, w, 1, b);

        for (short y = 1; y < h - 1; ++y) {
            b.moveChar(0, ' ', cFrame, w);
            b.putChar(0, '\xB3');
            b.putChar(w - 1, '\xB3');
            writeLine(0, y, w, 1, b);
        }

        b.moveChar(0, '\xC4', cFrame, w);
        b.putChar(0, '\xC0');
        b.putChar(w - 1, '\xD9');
        writeLine(0, h - 1, w, 1, b);

        if (owner != nullptr) {
            const short maxTitle = std::max<short>(0, w - 6);
            const char *title = ((TWindow *)owner)->getTitle(maxTitle);
            if (title != nullptr && maxTitle > 0) {
                short l = std::max<short>(0, std::min<short>((short)strwidth(title), maxTitle));
                short x = (short)((w - l) / 2);
                if (x > 1 && x + l < w - 1) {
                    b.moveChar(0, ' ', cFrame, w);
                    b.putChar(0, '\xDA');
                    b.putChar(w - 1, '\xBF');
                    b.moveStr(x, TStringView(title), cTitle, l);
                    writeLine(0, 0, w, 1, b);
                }
            }
        }
    }
};

/** Ventana de edición con indicador de palabras y barras ocultas (RetroFileEditor::setState). */
class RetroEditWindow : public TEditWindow {
    std::string titleDirtyScratch;

public:
    static TFrame *initFrame(TRect r) { return new RetroSingleLineFrame(r); }

    RetroEditWindow(const TRect &bounds, TStringView fileName, int aNumber) noexcept :
        TWindowInit(&RetroEditWindow::initFrame),
        TEditWindow(bounds, fileName, aNumber) {
        if (!editor)
            return;
        TScrollBar *hs = editor->hScrollBar;
        TScrollBar *vs = editor->vScrollBar;
        TRect eb = editor->getBounds();
        remove(editor);
        destroy(editor);
        /* Margen interno editable: aqui ajustar separacion de lineas respecto al marco. */
        if ((eb.b.x - eb.a.x) > 8) {
            eb.a.x += 2;
            eb.b.x -= 2;
        }
        if ((eb.b.y - eb.a.y) > 6) {
            eb.a.y += 1;
            eb.b.y -= 1;
        }
        editor = new RetroFileEditor(eb, hs, vs, nullptr, fileName);
        insert(editor);

        const short sh = short(size.y - 1);
        if (hs)
            hs->hide();
        if (vs)
            vs->hide();
        if (editor->indicator) {
            remove(editor->indicator);
            destroy(editor->indicator);
            editor->indicator = nullptr;
        }
        TRect ir(1, sh, short(size.x - 1), size.y);
        auto *wi = new WordCountIndicator(ir, editor);
        insertBefore(wi, editor);
        editor->indicator = wi;
    }

    const char *getTitle(short maxSize) override {
        if (!editor)
            return TEditWindow::getTitle(maxSize);
        /* El nombre va en la ventana Mini; en el marco, línea horizontal CP437 como el resto del diseño (\xC4). */
        int n = (int)maxSize;
        if (n < 4)
            n = 32;
        n = std::min(n, 200);
        titleDirtyScratch.assign((size_t)n, '\xC4');
        return titleDirtyScratch.c_str();
    }

    void handleEvent(TEvent &event) override {
        if (event.what == evKeyDown && editor) {
            const TKey k(event.keyDown);
            if (k == TKey('S', kbCtrlShift | kbShift)) {
                message(editor, evCommand, cmSaveAs, nullptr);
                clearEvent(event);
            } else if (k == TKey('S', kbCtrlShift)) {
                message(editor, evCommand, cmSave, nullptr);
                clearEvent(event);
            }
        }
        TEditWindow::handleEvent(event);
    }
};

class RetroWriterTVApp;

class ReadOnlyLineView : public TView {
public:
    explicit ReadOnlyLineView(const TRect &bounds, const std::string *textPtr) noexcept :
        TView(bounds), textPtr(textPtr) {}

    void draw() override {
        TDrawBuffer b;
        const TColorAttr cell = getColor(1);
        b.moveChar(0, ' ', cell, size.x);
        const std::string src = textPtr ? *textPtr : std::string{};
        if (!src.empty()) {
            std::string s = src;
            if ((int)s.size() > size.x)
                s.resize((size_t)size.x);
            b.moveStr(0, TStringView(s), cell, size.x);
        }
        writeLine(0, 0, size.x, 1, b);
    }

private:
    const std::string *textPtr {nullptr};
};

class ClockMenuBar : public TMenuBar {
public:
    using TMenuBar::TMenuBar;

    void draw() override {
        TMenuBar::draw();
        std::time_t tt = std::time(nullptr);
        struct tm tm {};
#if defined(_WIN32)
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        char buf[6] = {};
        if (std::strftime(buf, sizeof buf, "%H:%M", &tm) > 0) {
            TDrawBuffer b;
            const TColorAttr cell = getColor(1);
            const int len = (int)std::strlen(buf);
            b.moveStr(0, TStringView(buf), cell, len);
            const int x = std::max(0, size.x - len - 1);
            writeLine(x, 0, len, 1, b);
        }
    }
};

/** Asistente Crear con IA: modo de prompt, solicitud a endpoint/OpenAI/plantilla, carpeta bajo el panel actual. */
class CrearConIADialog : public TDialog {
public:
    RetroWriterTVApp *app {nullptr};
    TRadioButtons *modes {nullptr};
    ReadOnlyLineView *ideaLine {nullptr};
    ReadOnlyLineView *baseDirLine {nullptr};
    TInputLine *folderLine {nullptr};
    ReadOnlyLineView *daysLine {nullptr};
    ReadOnlyLineView *statusLine {nullptr};
    char ideaData[500] {};
    char daysData[16] {};
    /** Ultimo plazo (1..7) devuelto por la IA en Solicitar; 0 = sin dato (al crear se usa azar). */
    int iaDeadlineDays {0};
    /** Modo IA elegido automaticamente en la ultima solicitud; -1 = no solicitado aun. */
    int selectedMode { -1 };
    char baseDirData[MAXPATH] {};
    std::string ideaText;
    std::string baseDirText;
    std::string daysText;
    std::string statusText;

    CrearConIADialog(const TRect &bounds, RetroWriterTVApp *ap, const std::string &baseDirHint,
                     const std::string &folderHint) :
        TWindowInit(&TDialog::initFrame),
        TDialog(bounds, "Crear con IA"),
        app(ap) {
        options |= ofCentered;
        palette = dpBlueDialog;

        modes = new TRadioButtons(
            TRect(3, 3, 72, 10),
            new TSItem("~F~rases cortas",
                new TSItem("~D~iez palabras",
                    new TSItem("~C~inco palabras",
                        new TSItem("Palabra ~j~aponesa",
                            new TSItem("3 palabras ~n~o comunes",
                                new TSItem("3 palabras ~r~andom", nullptr)))))));
        insert(modes);
        ushort zm = 0;
        modes->setData(&zm);
        modes->setState(sfDisabled, True);

        ideaLine = new ReadOnlyLineView(TRect(3, 12, 75, 13), &ideaText);
        insert(ideaLine);
        insert(new TLabel(TRect(3, 11, 44, 12), "Idea", ideaLine));

        baseDirLine = new ReadOnlyLineView(TRect(3, 15, 75, 16), &baseDirText);
        insert(baseDirLine);
        insert(new TLabel(TRect(3, 14, 48, 15), "Ubicación", baseDirLine));

        folderLine = new TInputLine(TRect(3, 18, 53, 19), MAX_TITLE - 1);
        insert(folderLine);
        insert(new TLabel(TRect(3, 17, 25, 18), "Nombre carpeta:", folderLine));

        daysLine = new ReadOnlyLineView(TRect(56, 18, 75, 19), &daysText);
        insert(daysLine);
        insert(new TLabel(TRect(56, 17, 75, 18), "Dias sugeridos", daysLine));

        statusLine = new ReadOnlyLineView(TRect(3, 21, 75, 22), &statusText);
        insert(statusLine);
        insert(new TLabel(TRect(3, 20, 22, 21), "Estado IA", statusLine));

        insert(new TButton(TRect(3, 23, 22, 25), "Solicitar", cmIaSolicitar, bfNormal));
        insert(new TButton(TRect(24, 23, 40, 25), "Crear", cmOK, bfDefault));
        insert(new TButton(TRect(44, 23, 62, 25), "Cancelar", cmCancel, bfNormal));

        std::memset(baseDirData, 0, sizeof baseDirData);
        if (!trim(baseDirHint).empty())
            std::snprintf(baseDirData, sizeof baseDirData, "%s", baseDirHint.c_str());
        baseDirText = baseDirData;
        baseDirLine->drawView();

        if (!folderHint.empty()) {
            char fd[MAX_TITLE - 1] = {};
            std::snprintf(fd, sizeof fd, "%s", folderHint.c_str());
            folderLine->setData(fd);
        }
        std::memset(ideaData, 0, sizeof ideaData);
        ideaText = ideaData;
        std::memset(daysData, 0, sizeof daysData);
        std::snprintf(daysData, sizeof daysData, "-");
        if (daysLine)
            daysText = daysData;
        statusText = "Listo";
    }

    void handleEvent(TEvent &event) override {
        if (event.what == evCommand && event.message.command == cmIaSolicitar) {
            if (TProgram::application)
                message(TProgram::application, evBroadcast, cmIaSolicitar, this);
            clearEvent(event);
            return;
        }
        TDialog::handleEvent(event);
    }
};

class RetroWriterTVApp : public TApplication {
public:
    RetroWriterTVApp(const std::string &projectDir) :
        TProgInit(&RetroWriterTVApp::initStatusLine,
                  &RetroWriterTVApp::initMenuBar,
                  &RetroWriterTVApp::initDeskTop),
        projectDir(projectDir),
        preferencesPath(joinPath(projectDir, "appearance.cfg")),
        workspacePath(joinPath(projectDir, "workspace.cfg")),
        iaWriterCfgPath(joinPath(projectDir, "ia_writer.cfg")),
        iaRegistryPath(joinPath(projectDir, "ia_escrituras.jsonl")),
        iaRequestsPath(joinPath(projectDir, "ia_solicitudes.jsonl")) {
        loadAppearancePreferences();
        fillPaletteExplicit(textColor, backColor, paletteBytes);
        /* cwd del panel: "/" por defecto; workspace.cfg lo restaura si existe. */
        browserDir = absolutePath("/");
        lastEditorPath.clear();
        loadWorkspaceSession();
        reloadVisualLibraryFromDisk();
        reloadChapterSceneForEditorFile(lastEditorPath);
        if (pixelCanvas.size() != (size_t)kPixelGrid * (size_t)kPixelGrid)
            pixelCanvas.assign((size_t)kPixelGrid * (size_t)kPixelGrid, 0);

        if (!lastEditorPath.empty()) {
            std::ifstream chk(lastEditorPath);
            if (chk.good())
                openFileInEditor(lastEditorPath);
            else {
                lastEditorPath.clear();
                openFileInEditor("");
            }
        } else {
            openFileInEditor("");
        }
        createDesktopWidgets();
        applyDesktopPatternChar();
        startAutoSaveTimer();
    }

    static TMenuBar *initMenuBar(TRect r) {
        r.b.y = r.a.y + 1;
        return new ClockMenuBar(
            r,
            *new TSubMenu("~F~ile", kbAltF) +
                *new TMenuItem("~G~uardar", cmSave, TKey('S', kbCtrlShift), hcNoContext, "Ctrl-S") +
                *new TMenuItem("G~u~ardar como...", cmSaveAs, TKey('S', kbCtrlShift | kbShift), hcNoContext,
                               "Ctrl-Mayus-S") +
                newLine() +
                *new TMenuItem("E~x~it", cmQuit, kbCtrlQ, hcNoContext, "Ctrl-Q") +
            *new TSubMenu("~W~indows", kbAltW) +
                *new TMenuItem("~P~anel archivos (Ctrl+E)", cmToggleFilePanel, kbCtrlE, hcNoContext, "Ctrl-E") +
                *new TMenuItem("Guardar layout", cmLayoutSaveSlot, TKey(), hcNoContext, 0) +
                *new TMenuItem("Restaurar layout", cmLayoutRestoreSlot, TKey(), hcNoContext, 0) +
                newLine() +
                *new TMenuItem("~L~eer texto (tamano)...", cmReadabilityHelp, TKey(), hcNoContext, 0) +
                newLine() +
                *new TMenuItem("Refrescar paneles", cmRefreshWidgets, kbF6, hcNoContext, "F6") +
            *new TSubMenu("~P~referencias", kbAltP) +
                *new TMenuItem("~C~olores, fondo y autoguardado...", cmPreferences, kbF5, hcNoContext, "F5") +
                *new TMenuItem("~E~scena visual (capitulo)...", cmVisualScene, TKey(), hcNoContext, 0) +
                *new TMenuItem("Agregar elemento ~v~isual...", cmVisualLibrary, TKey(), hcNoContext, 0) +
            *new TSubMenu("Crear con ~I~A", kbAltI) +
                *new TMenuItem("Configurar IA...", cmIaConfig, TKey(), hcNoContext, 0) +
                *new TMenuItem("Crear con IA...", cmCrearConIA, TKey(), hcNoContext, 0) +
                *new TMenuItem("Entregar escritura IA...", cmEntregarIA, TKey(), hcNoContext, 0) +
                *new TMenuItem("Resumen IA...", cmIaResumen, TKey(), hcNoContext, 0)
        );
    }

    static TStatusLine *initStatusLine(TRect r) {
        r.a.y = r.b.y - 1;
        return new TStatusLine(
            r,
            *new TStatusDef(0, 0xFFFF) +
                *new TStatusItem("~Ctrl-S~ Guardar", TKey('S', kbCtrlShift), cmSave) +
                *new TStatusItem("~Ctrl-E~ Panel archivos", kbCtrlE, cmToggleFilePanel) +
                *new TStatusItem("~F5~ Preferencias", kbF5, cmPreferences) +
                *new TStatusItem("~Ctrl-Q~ Salir", kbCtrlQ, cmQuit) +
                *new TStatusItem(0, kbF10, cmMenu)
        );
    }

    virtual TPalette &getPalette() const {
        RetroWriterTVApp *self = const_cast<RetroWriterTVApp *>(this);
        if (appPaletteDirty || !cachedAppPalette) {
            if (self->paletteBytes.empty()) {
                /* Sane defaults for the very first call from TApplication constructor */
                fillPaletteExplicit(textColor, backColor, self->paletteBytes);
            }
            self->rebuildCachedPalette();
        }
        return *cachedAppPalette;
    }

    virtual TColorAttr mapColor(uchar index) override {
        TColorAttr base = TApplication::mapColor(index);

        /* Los índices 0x20..0x7F se dejan en BIOS para diálogos clásicos (botones, sombras). */
        if (index >= 0x20 && index <= 0x7F)
            return base;

        auto toBios16 = [](ushort c) -> uchar {
            if (c <= 0x0F)
                return uchar(c & 0x0F);
            return uchar(XTerm256toXTerm16(uchar(c & 0xFF)) & 0x0F);
        };

        TColorAttr out;
        /* cpEditor:
           - índice 6: texto normal
           - índice 7: texto seleccionado (forzamos contraste invirtiendo fg/bg) */
        if (index == 7) {
            /* Seleccion visible SIEMPRE en rutas de color BIOS de TEditor (TAttrPair). */
            out = TColorAttr(0x70);
            return out;
        }

        uchar fg16 = toBios16(textColor);
        uchar bg16 = toBios16(backColor);
        uchar bios = base.toBIOS();
        uchar srcFg = uchar(bios & 0x0F);
        uchar srcBg = uchar((bios >> 4) & 0x0F);

        /* La inversión fg/bg del atributo base se conserva al mapear a xterm-256. */
        bool swapped = (srcFg == bg16 && srcBg == fg16);
        if (swapped) {
            out = TColorAttr(
                TColorDesired(TColorXTerm(uchar(backColor & 0xFF))),
                TColorDesired(TColorXTerm(uchar(textColor & 0xFF)))
            );
        } else {
            out = TColorAttr(
                TColorDesired(TColorXTerm(uchar(textColor & 0xFF))),
                TColorDesired(TColorXTerm(uchar(backColor & 0xFF)))
            );
        }
        /* Índices 6/7 (cpEditor): negrita ANSI opcional (editorBold). */
        if (editorBold && (index == 6 || index == 7))
            setStyle(out, ushort(getStyle(out) | slBold));
        return out;
    }

    virtual void idle() override {
        TProgram::idle();
#if !defined(_WIN32)
        if (kittyPendingZoomRestore) {
            kittyPendingZoomRestore = false;
            if (terminalIsKitty() && kittyZoomPersistenceEnabled() && kittySavedCellHeightPx > 0) {
                tryAdjustKittyFontTowardCellHeight(kittySavedCellHeightPx);
                TEvent e {};
                e.what = evCommand;
                e.message.command = cmDeferredTerminalLayout;
                e.message.infoPtr = nullptr;
                putEvent(e);
            }
        }
#endif
        if (editorWindow && editorWindow->editor && editorWindow->editor->indicator) {
            if (auto *wi = dynamic_cast<WordCountIndicator *>(editorWindow->editor->indicator)) {
                if (wi->expireSaveAckIfNeeded())
                    wi->drawView();
            }
        }
        /* Autoguardado ligero del layout (~cada unos segundos en reposo) para no perder movimientos al cerrar el terminal. */
        if (++layoutIdleSaves >= 500) {
            layoutIdleSaves = 0;
            saveWorkspaceSession();
        }
    }

    virtual void shutDown() override {
        if (autoSaveTimer != nullptr) {
            killTimer(autoSaveTimer);
            autoSaveTimer = nullptr;
        }
        saveWorkspaceSession();
        saveAppearancePreferences();
        TProgram::shutDown();
    }

    virtual void getEvent(TEvent &event) override {
        const ushort pw = TScreen::screenWidth;
        const ushort ph = TScreen::screenHeight;
        TProgram::getEvent(event);
        /* Tras cmScreenChanged el redibujado ocurre dentro de getEvent; el reacomodo se difiere a handleEvent para evitar ncurses/cursor incoherentes. */
        if (TScreen::screenWidth != pw || TScreen::screenHeight != ph) {
            TEvent e {};
            e.what = evCommand;
            e.message.command = cmDeferredTerminalLayout;
            e.message.infoPtr = nullptr;
            putEvent(e);
        }
        /* Teclas de salida global se convierten en cmQuit aquí para que no las consuman las vistas. */
        if (event.what == evKeyDown &&
            (event.keyDown.keyCode == kbCtrlQ ||
             event.keyDown.keyCode == kbF12)) {
            event.what = evCommand;
            event.message.command = cmQuit;
            event.message.infoPtr = 0;
        }
        if (event.what == evKeyDown && event.keyDown.keyCode == kbCtrlE) {
            event.what = evCommand;
            event.message.command = cmToggleFilePanel;
            event.message.infoPtr = 0;
        }
    }

    virtual Boolean valid(ushort command) override {
        /* cmQuit se acepta siempre (salida inmediata aunque una vista devolviera falso). */
        if (command == cmQuit)
            return True;
        return TApplication::valid(command);
    }

    virtual void handleEvent(TEvent &event) {

        if (event.what == evMouseWheel && navListView && (navListView->state & sfFocused) != 0) {
            navListView->handleEvent(event);
            if (event.what == evNothing)
                return;
        }

        if (event.what == evCommand && event.message.command == cmQuit) {
            requestQuit();
            clearEvent(event);
            return;
        }

        if (event.what == evCommand && event.message.command == cmDeferredTerminalLayout) {
            syncWidgetsToTerminalSize();
            clearEvent(event);
            return;
        }
        if (event.what == evBroadcast && event.message.command == cmTimerExpired) {
            if (autoSaveTimer != nullptr && event.message.infoPtr == autoSaveTimer)
                tryAutoSaveEditor();
        }

        if (event.what == evBroadcast && event.message.command == cmIaSolicitar) {
            auto *d = static_cast<CrearConIADialog *>(event.message.infoPtr);
            if (d && d->modes && d->ideaLine) {
                auto setStatus = [&](const std::string &s) {
                    if (d->statusLine) {
                        d->statusText = s;
                        d->statusLine->drawView();
                        TScreen::flushScreen();
                    }
                };
                static std::mt19937 rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
                std::uniform_int_distribution<int> distMode(0, 5);
                const ushort mode = (ushort)distMode(rng);
                d->selectedMode = (int)mode;
                if (d->modes)
                    d->modes->setData((void *)&mode);
                ensureIaConfig();
                std::string idea, err;
                int dlim = 0;
                setStatus("Enviando solicitud (modo random)...");
                const bool ok = rw_ia::fetchIdea(iaConfig, (int)mode, idea, dlim, err, setStatus);
                /* Loggea SIEMPRE (éxito o error), aunque el usuario no presione Crear. */
                {
                    std::time_t tnow = std::time(nullptr);
                    struct tm tmb {};
                    localtime_r(&tnow, &tmb);
                    char ts[64];
                    std::strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%S", &tmb);
                    std::ostringstream jl;
                    jl << "{\"ts\":\"" << rw_ia::jsonEscapeString(ts) << "\",\"accion\":\"solicitar\""
                       << ",\"modo\":" << (int)mode
                       << ",\"ok\":" << (ok ? "true" : "false")
                       << ",\"ubicacion\":\"" << rw_ia::jsonEscapeString(d->baseDirText) << "\""
                       << ",\"idea\":\"" << rw_ia::jsonEscapeString(idea) << "\""
                       << ",\"dias\":" << dlim
                       << ",\"error\":\"" << rw_ia::jsonEscapeString(err) << "\"}";
                    appendIaRequestLogJsonl(jl.str());
                }

                if (ok) {
                    d->iaDeadlineDays = (dlim >= 1 && dlim <= 7) ? dlim : 0;
                    std::memset(d->ideaData, 0, sizeof d->ideaData);
                    std::snprintf(d->ideaData, sizeof d->ideaData, "%s", idea.c_str());
                    d->ideaText = d->ideaData;
                    d->ideaLine->drawView();
                    if (d->daysLine) {
                        std::memset(d->daysData, 0, sizeof d->daysData);
                        if (d->iaDeadlineDays > 0)
                            std::snprintf(d->daysData, sizeof d->daysData, "%d", d->iaDeadlineDays);
                        else
                            std::snprintf(d->daysData, sizeof d->daysData, "(azar)");
                        d->daysText = d->daysData;
                        d->daysLine->drawView();
                    }
                    setStatus("Recibido");
                } else {
                    setStatus("Error");
                    messageBox(mfError | mfOKButton, "IA: %s", err.c_str());
                }
            }
            clearEvent(event);
            return;
        }
        if (event.what == evBroadcast && event.message.command == cmNavSelect) {
            auto *src = static_cast<NavigatorListView *>(event.message.infoPtr);
            fileManagerActivate(src);
            clearEvent(event);
            return;
        }
        if (event.what == evBroadcast && event.message.command == cmNavFilePopup) {
            auto *src = static_cast<NavigatorListView *>(event.message.infoPtr);
            if (src && src == navListView) {
                NavigatorListView::NavItem it {};
                if (src->peekCursorItem(it) && !it.isDirectory && it.label != "..") {
                    TDeskTop *desk = deskTop;
                    if (desk) {
                        TPoint p = navWindow ? navWindow->makeGlobal(TPoint{3, 3}) : TPoint{3, 3};
                        TMenuItem *chain = new TMenuItem("Mover a ~t~rash", cmNavMoveTrash, TKey(), hcNoContext, 0);
                        TRect bounds(p, p);
                        TMenu *menu = new TMenu(*chain);
                        TMenuPopup *popup = new TMenuPopup(bounds, menu);
                        rwAutoPlaceMenuPopupOnDesk(desk, popup, p);
                        const ushort cmd = desk->execView(popup);
                        TObject::destroy(popup);
                        if (cmd == cmNavMoveTrash) {
                            const ushort ans = messageBox(mfConfirmation | mfYesButton | mfNoButton,
                                                          "Mover a trash?\n%s", it.label.c_str());
                            if (ans == cmYes) {
                                std::string err;
                                if (!movePathToTrash(it.fullPath, err))
                                    messageBox(mfError | mfOKButton, "%s", err.c_str());
                                else
                                    syncFilePanelListing();
                            }
                        }
                    }
                }
            }
            clearEvent(event);
            return;
        }

        if (event.what == evBroadcast && event.message.command == cmCreateFolder) {
            onCreateFolderInBrowser();
            clearEvent(event);
            return;
        }

        if (event.what == evBroadcast && event.message.command == cmNavPanelLayoutChanged) {
            updateNavWindowTitle();
            if (navListView)
                navListView->drawView();
            if (navWindow)
                navWindow->redraw();
            clearEvent(event);
            return;
        }

        if (event.what == evBroadcast && event.message.command == cmCreateTxtFile) {
            onCreateTxtFileInBrowser();
            clearEvent(event);
            return;
        }

        TApplication::handleEvent(event);
        if (event.what != evCommand) return;

        switch (event.message.command) {
            case cmReadabilityHelp:
                messageBox(
                    "Tamano del texto en el terminal\n\n"
                    "Turbo Vision usa la fuente del terminal (una celda = un caracter). "
                    "Para letra mas grande o pequena:\n"
                    "  Kitty: Ctrl+Shift+Plus / Ctrl+Shift+Minus\n"
                    "  Otros: tamano de fuente del emulador.\n\n"
                    "  Ajuste visual por ancho (soft-wrap): no inserta saltos en el archivo.\n\n"
                    "Negrita del editor: appearance.cfg -> editorBold 1 (por defecto).\n"
                    "Autoguardado: autoSaveIntervalSec segundos (0 = off; por defecto 60).\n"
                    "Guardar: Ctrl-S | Guardar como: Ctrl-Mayus-S | Archivo en el menu.\n"
                    "Cambios sin guardar: * en el titulo y antes de \"Palabras:\" en la franja inferior.",
                    mfInformation | mfOKButton);
                clearEvent(event);
                break;
            case cmRefreshWidgets:
                reloadMiniPreviewImage();
                syncFilePanelListing();
                redrawMiniPreviewOnly();
                relayoutEditorWindow();
                ensureEditorAboveNavigator();
                updateMiniPreviewWindowTitles();
                if (deskTop)
                    deskTop->drawView();
                redraw();
                clearEvent(event);
                break;
            case cmPreferences:
                showPreferencesDialog();
                clearEvent(event);
                break;
            case cmIaConfig:
                showIaConfigDialog();
                clearEvent(event);
                break;
            case cmCrearConIA:
                showCrearConIADialog();
                clearEvent(event);
                break;
            case cmEntregarIA:
                showEntregarIADialog();
                clearEvent(event);
                break;
            case cmIaResumen:
                showIaResumenDialog();
                clearEvent(event);
                break;
            case cmVisualScene:
                showVisualSceneDialog();
                clearEvent(event);
                break;
            case cmVisualLibrary:
                showVisualLibraryDialog();
                clearEvent(event);
                break;
            case cmVisualGallery:
                showVisualSceneDialog();
                clearEvent(event);
                break;
            case cmToggleFilePanel:
                onToggleFilePanel();
                clearEvent(event);
                break;
            case cmLayoutSaveSlot:
                saveLayoutSnapshotSlot();
                clearEvent(event);
                break;
            case cmLayoutRestoreSlot:
                restoreLayoutSnapshotSlot();
                clearEvent(event);
                break;
            case cmQuit:
                requestQuit();
                clearEvent(event);
                break;
            default:
                break;
        }
    }

private:
    std::string projectDir;
    std::string preferencesPath;
    std::string workspacePath;
    std::string iaWriterCfgPath;
    std::string iaRegistryPath;
    std::string iaRequestsPath;
    rw_ia::Config iaConfig {};
    bool iaConfigLoaded {false};
    /** Directorio mostrado en el File Manager (explorador). */
    std::string browserDir;
    /** Ultimo archivo abierto en el editor (sesion y reapertura). */
    std::string lastEditorPath;
    /** Color explicito de texto VGA (0..15). */
    ushort textColor {7};
    /** Color explicito de fondo VGA (0..7 recomendado). */
    ushort backColor {0};
    /** Negrita ANSI en el texto del editor (appearance.cfg: editorBold 0|1). */
    bool editorBold {true};
    /** Segundos entre autoguardados del editor (0 = desactivado). appearance.cfg: autoSaveIntervalSec */
    int autoSaveIntervalSec {60};
    /** Si esta activo, al escribir "--" se convierte a guion largo Unicode (—). */
    bool autoEmDashFromDoubleHyphen {false};
    TTimerId autoSaveTimer {nullptr};
    /** Caracter literal del fondo del escritorio. */
    char desktopPatternChar {'\xb0'};
    /** Simbolo UTF-8 elegido para modo unicode del fondo. */
    std::string desktopPatternUtf8 {"\xb0"};

    mutable bool appPaletteDirty {true};
    mutable std::unique_ptr<TPalette> cachedAppPalette;
    std::vector<char> paletteBytes;

    TEditWindow *editorWindow {nullptr};
    TWindow *navWindow {nullptr};
    NavigatorListView *navListView {nullptr};
    /**
     * Partición vertical File Manager / Mini (fila Y exclusiva del límite). Respaldo si no hay navGeom.
     * 0 = aún no fijado (usar computeDefaultPanelSplitY la primera vez).
     */
    int sessionPanelSplitY {0};
    /** Geometrías guardadas en workspace.cfg (coords del escritorio). */
    bool loadedNavGeom {false};
    TRect storedNavGeom {};
    /** Posiciones guardadas de cada ventana mini (workspace.cfg: miniGeom1..3). */
    bool loadedMiniGeom[3] {false, false, false};
    TRect storedMiniGeom[3] {};
    bool loadedEditorGeom {false};
    TRect storedEditorGeom {};
    bool loadedSavedLayoutNavGeom {false};
    TRect savedLayoutNavGeom {};
    bool loadedSavedLayoutMiniGeom[3] {false, false, false};
    TRect savedLayoutMiniGeom[3] {};
    bool loadedSavedLayoutEditorGeom {false};
    TRect savedLayoutEditorGeom {};
    int savedLayoutSplitY {0};
#if !defined(_WIN32)
    int savedLayoutKittyCellHeightPx {0};
#endif
    ushort layoutIdleSaves {0};
#if !defined(_WIN32)
    /** Altura de celda en px guardada (Kitty); 0 = aun no hay valor en appearance.cfg. */
    int kittySavedCellHeightPx {0};
    bool kittyPendingZoomRestore {true};
#endif
    /** Ventana miniatura con vista previa pixel debajo del panel de archivos. */
    TWindow *previewWindow {nullptr};
    TWindow *previewWindow2 {nullptr};
    TWindow *previewWindow3 {nullptr};
    PixelPreviewView *previewPixelView {nullptr};
    PixelPreviewView *previewPixelView2 {nullptr};
    PixelPreviewView *previewPixelView3 {nullptr};
    /** Lienzo 1000x1000 (1 = tinta, 0 = vacio); colores vienen de textColor/backColor. */
    std::vector<uint8_t> pixelCanvas;
    /** Biblioteca visuals.cfg + override chapter_scene.cfg (carpeta del .txt abierto). */
    RwVisualLibrary visualLibrary {};
    RwChapterScene chapterScene {};
    std::string visualConfigPathAbs;
    /** Carpeta global de la biblioteca visual (compartida por toda la novela). */
    std::string visualGlobalBaseDirAbs;
    std::string visualBaseDirAbs;
    /** Raiz de novela para fallback de rutas (meep vs meep/cap1/assets). */
    std::string visualNovelRootAbs;
    std::string chapterScenePathAbs;
    /** Lado mayor al cargar la imagen mini (0 = no reducir). appearance.cfg: miniPreviewMaxSide */
    int miniPreviewMaxSide {kDefaultMiniPreviewMaxSide};
    /** Pixeles RGB 8-bit cargados desde disco (ancho x alto x 3). */
    std::vector<uint8_t> miniPreviewRgb;
    int miniPreviewImgW {0};
    int miniPreviewImgH {0};
    std::vector<uint8_t> miniPreviewRgb2;
    int miniPreviewImgW2 {0};
    int miniPreviewImgH2 {0};
    std::vector<uint8_t> miniPreviewRgb3;
    int miniPreviewImgW3 {0};
    int miniPreviewImgH3 {0};
    /** Salida Sixel solo con RETRO_WRITER_ALLOW_SIXEL + esta opcion; 0 = bloques U+2580 o Kitty nativo. */
    bool miniPreviewSixel {false};
    /** Kitty: imagen RGB en calidad de celda + placeholders Unicode (no capa flotante sobre el TUI). */
#if !defined(_WIN32)
    bool miniPreviewKittyNative {true};
#else
    bool miniPreviewKittyNative {false};
#endif
    /** Colores de paleta Sixel (16..256). */
    int miniPreviewSixelColors {256};
    /** Solo File Manager embebido (Ctrl+E). La ventana Mini no se oculta con esto. */
    bool filePanelVisible {true};
    UnicodeBackground *unicodeBackground {nullptr};

    /** Tras insertar o sustituir el fondo: mini, panel y editor se colocan delante del TBackground. */
    void restackDesktopWindowsAboveBackground() {
        if (!deskTop)
            return;
        TDeskTop *dt = static_cast<TDeskTop *>(deskTop);
        TView *bg = dt->background;
        if (!bg)
            return;
        TView *top = bg;
        if (previewWindow && previewWindow->owner == deskTop) {
            previewWindow->putInFrontOf(bg);
            top = previewWindow;
        }
        if (previewWindow2 && previewWindow2->owner == deskTop) {
            previewWindow2->putInFrontOf(top);
            top = previewWindow2;
        }
        if (previewWindow3 && previewWindow3->owner == deskTop) {
            previewWindow3->putInFrontOf(top);
            top = previewWindow3;
        }
        if (navWindow && navWindow->owner == deskTop) {
            navWindow->putInFrontOf(top);
            top = navWindow;
        }
        if (editorWindow && editorWindow->owner == deskTop)
            editorWindow->putInFrontOf(top);
    }

    void startAutoSaveTimer() {
        if (autoSaveTimer != nullptr) {
            killTimer(autoSaveTimer);
            autoSaveTimer = nullptr;
        }
        if (autoSaveIntervalSec <= 0)
            return;
        const unsigned ms = (unsigned)autoSaveIntervalSec * 1000u;
        autoSaveTimer = setTimer(ms, (int)ms);
    }

    void tryAutoSaveEditor() {
        if (!editorWindow || !editorWindow->editor)
            return;
        TFileEditor *ed = editorWindow->editor;
        if (!ed->modified || !ed->fileName[0])
            return;
        ed->save();
    }

    void syncWidgetsToTerminalSize() {
        const ushort tw = TScreen::screenWidth;
        const ushort th = TScreen::screenHeight;
        if (tw == 0 || th == 0)
            return;
        refreshNavigatorWidget();
        relayoutEditorWindow();
        applyDesktopPatternChar();
        restackDesktopWindowsAboveBackground();
        ensureEditorAboveNavigator();
        updateMiniPreviewWindowTitles();
        if (deskTop)
            deskTop->redraw();
        redraw();
        TScreen::flushScreen();
    }

    void requestQuit() {
        if (TProgram::application)
            TProgram::application->endModal(cmQuit);
        if (deskTop)
            deskTop->endModal(cmQuit);
        endState = cmQuit;
        endModal(cmQuit);

        /* Se encola cmQuit para cerrar bucles modales intermedios. */
        TEvent e;
        e.what = evCommand;
        e.message.command = cmQuit;
        e.message.infoPtr = 0;
        putEvent(e);
    }

    void rebuildCachedPalette() {
        cachedAppPalette.reset(new TPalette(paletteBytes.data(), kAppPaletteLen));
        appPaletteDirty = false;
    }

    /** Relleno entre ventanas: caracter pattern del fondo Turbo Vision. */
    void loadAppearancePreferences() {
        std::ifstream in(preferencesPath);
        if (!in.is_open()) return;
        std::string ver;
        if (!std::getline(in, ver)) return;
        if (trim(ver) != "v1") return;
        std::string line;
        while (std::getline(in, line)) {
            line = trim(line);
            if (line.empty()) continue;
            size_t sp = line.find(' ');
            if (sp == std::string::npos) continue;
            std::string key = trim(line.substr(0, sp));
            std::string val = trim(line.substr(sp + 1));
            if (key == "textColor")
                textColor = parseUShortClamped(val, 0, 255, textColor);
            else if (key == "backColor")
                backColor = parseUShortClamped(val, 0, 255, backColor);
            else if (key == "patternUtf8") {
                std::string dec = hexToUtf8(val);
                if (!dec.empty()) {
                    desktopPatternUtf8 = dec;
                    desktopPatternChar = MatrixSelectorView::symbolToPatternChar(desktopPatternUtf8.c_str());
                }
            } else if (key == "miniPreviewMaxSide")
                miniPreviewMaxSide = parseIntClamped(val, 0, 8192, miniPreviewMaxSide);
            else if (key == "miniPreviewSixel")
                miniPreviewSixel = parseIntClamped(val, 0, 1, miniPreviewSixel ? 1 : 0) != 0;
            else if (key == "miniPreviewSixelColors")
                miniPreviewSixelColors = parseIntClamped(val, 16, 256, miniPreviewSixelColors);
#if !defined(_WIN32)
            else if (key == "miniPreviewKittyNative")
                miniPreviewKittyNative = parseIntClamped(val, 0, 1, miniPreviewKittyNative ? 1 : 0) != 0;
#endif
            else if (key == "editorBold")
                editorBold = parseIntClamped(val, 0, 1, editorBold ? 1 : 0) != 0;
            else if (key == "autoSaveIntervalSec")
                autoSaveIntervalSec = parseIntClamped(val, 0, 7200, autoSaveIntervalSec);
            else if (key == "autoEmDashFromDoubleHyphen")
                autoEmDashFromDoubleHyphen = parseIntClamped(val, 0, 1, autoEmDashFromDoubleHyphen ? 1 : 0) != 0;
#if !defined(_WIN32)
            else if (key == "kittyCellHeightPx")
                kittySavedCellHeightPx = parseIntClamped(val, 0, 256, kittySavedCellHeightPx);
#endif
        }
    }

    void saveAppearancePreferences() {
        std::ofstream out(preferencesPath, std::ios::trunc);
        if (!out.is_open()) return;
        out << "v1\n";
        out << "textColor " << textColor << "\n";
        out << "backColor " << backColor << "\n";
        out << "patternUtf8 " << utf8ToHex(desktopPatternUtf8) << "\n";
        out << "miniPreviewMaxSide " << miniPreviewMaxSide << "\n";
        out << "miniPreviewSixel " << (miniPreviewSixel ? 1 : 0) << "\n";
        out << "miniPreviewSixelColors " << miniPreviewSixelColors << "\n";
#if !defined(_WIN32)
        out << "miniPreviewKittyNative " << (miniPreviewKittyNative ? 1 : 0) << "\n";
#endif
        out << "editorBold " << (editorBold ? 1 : 0) << "\n";
        out << "autoSaveIntervalSec " << autoSaveIntervalSec << "\n";
        out << "autoEmDashFromDoubleHyphen " << (autoEmDashFromDoubleHyphen ? 1 : 0) << "\n";
#if !defined(_WIN32)
        if (kittyZoomPersistenceEnabled()) {
            if (terminalIsKitty()) {
                const int h = kittyTermCellHeightPx();
                if (h > 0) {
                    kittySavedCellHeightPx = h;
                    out << "kittyCellHeightPx " << h << "\n";
                }
            } else if (kittySavedCellHeightPx > 0)
                out << "kittyCellHeightPx " << kittySavedCellHeightPx << "\n";
        } else if (kittySavedCellHeightPx > 0)
            out << "kittyCellHeightPx " << kittySavedCellHeightPx << "\n";
#endif
    }

    void reloadVisualLibraryFromDisk() {
        visualLibrary = RwVisualLibrary{};
        visualConfigPathAbs.clear();
        visualBaseDirAbs.clear();
        visualNovelRootAbs.clear();
        std::string novelRoot = trim(visualGlobalBaseDirAbs);
        if (novelRoot.empty())
            novelRoot = absolutePath(projectDir);
        visualNovelRootAbs = novelRoot;
        std::string baseForCfg;
        visualConfigPathAbs = rwResolveVisualsCfgNearNovelRoot(novelRoot, baseForCfg);
        visualBaseDirAbs = baseForCfg;
        std::error_code ec;
        if (!fs::is_regular_file(fs::path(visualConfigPathAbs), ec))
            return;
        rwLoadVisualsCfg(visualConfigPathAbs, visualLibrary);
    }

    void reloadChapterSceneForEditorFile(const std::string &editorAbsPath) {
        chapterScene = RwChapterScene{};
        chapterScenePathAbs.clear();
        if (trim(editorAbsPath).empty())
            return;
        fs::path p(editorAbsPath);
        if (!p.has_parent_path())
            return;
        std::error_code ec;
        const std::string cf = joinPath(p.parent_path().string(), "chapter_scene.cfg");
        if (!fs::is_regular_file(fs::path(cf), ec))
            return;
        chapterScenePathAbs = absolutePath(cf);
        rwLoadChapterSceneCfg(chapterScenePathAbs, chapterScene);
    }

    bool computeVisualMiniPaths(std::string &p1, std::string &p2, std::string &p3) const {
        p1.clear();
        p2.clear();
        p3.clear();
        if (!rwVisualsHasActiveDefaults(visualLibrary, chapterScene))
            return false;
        std::string bgId, c2Id, c3Id;
        int bgI, c2I, c3I;
        rwMergeSceneIds(visualLibrary, chapterScene, bgId, bgI, c2Id, c2I, c3Id, c3I);
        rwPickImagePath(visualLibrary.backgrounds, bgId, bgI, visualBaseDirAbs, visualLibrary.assetRootSubpath, visualNovelRootAbs,
                        p1);
        rwPickImagePath(visualLibrary.characters, c2Id, c2I, visualBaseDirAbs, visualLibrary.assetRootSubpath, visualNovelRootAbs,
                        p2);
        rwPickImagePath(visualLibrary.characters, c3Id, c3I, visualBaseDirAbs, visualLibrary.assetRootSubpath, visualNovelRootAbs,
                        p3);
        return true;
    }

    /* Escena visual = dialogo unificado para fondo/personaje1/personaje2 con preview en vivo. */
    void showVisualSceneDialog() {
        if (!deskTop)
            return;
        if (editorWindow && editorWindow->editor) {
            const char *fn = editorWindow->editor->fileName;
            if (fn && fn[0] != '\0') {
                lastEditorPath = absolutePath(fn);
                syncVisualGlobalRootFromEditorPath(lastEditorPath);
                reloadVisualLibraryFromDisk();
            }
        }
        std::string statusLine;
        auto clampVar = [](const std::map<std::string, std::vector<std::string>> &m, std::string &id, int &idx) {
            auto it = m.find(id);
            if (id.empty() || it == m.end() || it->second.empty()) {
                id.clear();
                idx = 0;
                return;
            }
            idx = std::clamp(idx, 0, (int)it->second.size() - 1);
        };
        auto fillIdList = [&](NavigatorListView *lv, const std::map<std::string, std::vector<std::string>> &m, const std::string &cur) {
            std::vector<NavigatorListView::NavItem> items;
            items.push_back(NavigatorListView::NavItem{"(ninguno)", false, ""});
            for (const auto &pr : m)
                items.push_back(NavigatorListView::NavItem{pr.first, false, pr.first});
            lv->setItems(items);
            if (cur.empty())
                lv->setCursorByLabel("(ninguno)");
            else
                lv->setCursorByLabel(cur);
        };
        auto fillVarList = [&](NavigatorListView *lv, const std::map<std::string, std::vector<std::string>> &m, const std::string &id,
                               int cur) {
            int maxN = 1;
            auto it = m.find(id);
            if (!id.empty() && it != m.end() && !it->second.empty())
                maxN = std::max(1, (int)it->second.size());
            std::vector<NavigatorListView::NavItem> items;
            for (int i = 0; i < maxN; ++i)
                items.push_back(NavigatorListView::NavItem{std::to_string(i), false, std::to_string(i)});
            lv->setItems(items);
            lv->setCursorByLabel(std::to_string(std::clamp(cur, 0, std::max(0, maxN - 1))));
        };
        auto readId = [](NavigatorListView *lv) {
            NavigatorListView::NavItem it {};
            if (lv && lv->peekCursorItem(it))
                return it.fullPath;
            return std::string{};
        };
        auto readVar = [](NavigatorListView *lv) {
            NavigatorListView::NavItem it {};
            if (lv && lv->peekCursorItem(it))
                return parseIntClamped(it.fullPath, 0, 4095, 0);
            return 0;
        };
        auto pickAbs = [&](const std::map<std::string, std::vector<std::string>> &m, const std::string &id, int idx) {
            std::string out;
            rwPickImagePath(m, id, idx, visualBaseDirAbs, visualLibrary.assetRootSubpath, visualNovelRootAbs, out);
            return out;
        };
        auto loadPreview = [&](const std::string &absPath, std::vector<uint8_t> &rgb, int &iw, int &ih) {
            rgb.clear();
            iw = ih = 0;
            if (absPath.empty())
                return;
            int w = 0, h = 0, comp = 0;
            unsigned char *data = stbi_load(absPath.c_str(), &w, &h, &comp, 3);
            if (data && w > 0 && h > 0) {
                rgb.assign(data, data + (size_t)w * (size_t)h * 3u);
                stbi_image_free(data);
                iw = w;
                ih = h;
                downscaleRgbImageForMini(rgb, iw, ih, std::max(320, miniPreviewMaxSide * 2));
            }
        };
        auto eraseVariantFromMap = [&](std::map<std::string, std::vector<std::string>> &m, const std::string &id, int idx,
                                       const char *kindName) {
            if (id.empty()) {
                statusLine = std::string("No hay id seleccionado en ") + kindName + ".";
                return;
            }
            auto it = m.find(id);
            if (it == m.end() || it->second.empty()) {
                statusLine = std::string("El id ya no existe en ") + kindName + ".";
                return;
            }
            const int maxIdx = (int)it->second.size() - 1;
            const int useIdx = std::clamp(idx, 0, maxIdx);
            const ushort ans = messageBox(mfConfirmation | mfYesButton | mfNoButton,
                                          "Eliminar variante %d de '%s' (%s)?",
                                          useIdx, id.c_str(), kindName);
            if (ans != cmYes) {
                statusLine = "Eliminacion cancelada.";
                return;
            }
            it->second.erase(it->second.begin() + useIdx);
            if (it->second.empty())
                m.erase(it);
            ensureVisualConfigForWrite();
            persistVisualLibraryToDisk();
            statusLine = std::string("Eliminada variante ") + std::to_string(useIdx) + " de '" + id + "'.";
        };

        std::string bgId, c2Id, c3Id;
        int bgIdx = 0, c2Idx = 0, c3Idx = 0;
        rwMergeSceneIds(visualLibrary, chapterScene, bgId, bgIdx, c2Id, c2Idx, c3Id, c3Idx);
        clampVar(visualLibrary.backgrounds, bgId, bgIdx);
        clampVar(visualLibrary.characters, c2Id, c2Idx);
        clampVar(visualLibrary.characters, c3Id, c3Idx);
        for (;;) {

            std::vector<uint8_t> rgbBg, rgbC2, rgbC3;
            int iwBg = 0, ihBg = 0, iwC2 = 0, ihC2 = 0, iwC3 = 0, ihC3 = 0;
            loadPreview(pickAbs(visualLibrary.backgrounds, bgId, bgIdx), rgbBg, iwBg, ihBg);
            loadPreview(pickAbs(visualLibrary.characters, c2Id, c2Idx), rgbC2, iwC2, ihC2);
            loadPreview(pickAbs(visualLibrary.characters, c3Id, c3Idx), rgbC3, iwC3, ihC3);

            VisualSceneDialog *d = new VisualSceneDialog(TRect(3, 4, 100, 32), "Escena visual");
            d->options |= ofCentered;
            d->setState(sfShadow, False);
            d->palette = dpBlueDialog;

            d->insert(new TStaticText(TRect(2, 2, 28, 3), "Fondo"));
            d->insert(new TStaticText(TRect(30, 2, 38, 3), "Var"));
            d->insert(new TStaticText(TRect(40, 2, 66, 3), "Vista"));
            d->insert(new TStaticText(TRect(68, 2, 84, 3), "Eliminar"));

            d->insert(new TStaticText(TRect(2, 10, 28, 11), "Personaje 1"));
            d->insert(new TStaticText(TRect(30, 10, 38, 11), "Var"));
            d->insert(new TStaticText(TRect(40, 10, 66, 11), "Vista"));
            d->insert(new TStaticText(TRect(68, 10, 84, 11), "Eliminar"));

            d->insert(new TStaticText(TRect(2, 18, 28, 19), "Personaje 2"));
            d->insert(new TStaticText(TRect(30, 18, 38, 19), "Var"));
            d->insert(new TStaticText(TRect(40, 18, 66, 19), "Vista"));
            d->insert(new TStaticText(TRect(68, 18, 84, 19), "Eliminar"));

            auto *lvBgId = new VisualSceneListView(TRect(2, 3, 28, 9), &textColor, &backColor);
            auto *lvBgVar = new VisualSceneListView(TRect(30, 3, 38, 9), &textColor, &backColor);
            auto *lvC2Id = new VisualSceneListView(TRect(2, 11, 28, 17), &textColor, &backColor);
            auto *lvC2Var = new VisualSceneListView(TRect(30, 11, 38, 17), &textColor, &backColor);
            auto *lvC3Id = new VisualSceneListView(TRect(2, 19, 28, 25), &textColor, &backColor);
            auto *lvC3Var = new VisualSceneListView(TRect(30, 19, 38, 25), &textColor, &backColor);
            d->insert(lvBgId);
            d->insert(lvBgVar);
            d->insert(lvC2Id);
            d->insert(lvC2Var);
            d->insert(lvC3Id);
            d->insert(lvC3Var);

            fillIdList(lvBgId, visualLibrary.backgrounds, bgId);
            fillIdList(lvC2Id, visualLibrary.characters, c2Id);
            fillIdList(lvC3Id, visualLibrary.characters, c3Id);
            fillVarList(lvBgVar, visualLibrary.backgrounds, bgId, bgIdx);
            fillVarList(lvC2Var, visualLibrary.characters, c2Id, c2Idx);
            fillVarList(lvC3Var, visualLibrary.characters, c3Id, c3Idx);

            auto *pvBg = new PixelPreviewView(TRect(40, 3, 66, 9), &pixelCanvas, &rgbBg, &iwBg, &ihBg, &textColor, &backColor,
#ifdef HAVE_LIBSIXEL
                                           miniPreviewSixel,
#else
                                           false,
#endif
#if !defined(_WIN32)
                                           miniPreviewKittyNative,
#else
                                           false,
#endif
#ifdef HAVE_LIBSIXEL
                                           miniPreviewSixelColors,
#else
                                           256,
#endif
                                           0xE4u);
            d->insert(pvBg);
            auto *pvC2 = new PixelPreviewView(TRect(40, 11, 66, 17), &pixelCanvas, &rgbC2, &iwC2, &ihC2, &textColor, &backColor,
#ifdef HAVE_LIBSIXEL
                                           miniPreviewSixel,
#else
                                           false,
#endif
#if !defined(_WIN32)
                                           miniPreviewKittyNative,
#else
                                           false,
#endif
#ifdef HAVE_LIBSIXEL
                                           miniPreviewSixelColors,
#else
                                           256,
#endif
                                           0xE5u);
            d->insert(pvC2);
            auto *pvC3 = new PixelPreviewView(TRect(40, 19, 66, 25), &pixelCanvas, &rgbC3, &iwC3, &ihC3, &textColor, &backColor,
#ifdef HAVE_LIBSIXEL
                                           miniPreviewSixel,
#else
                                           false,
#endif
#if !defined(_WIN32)
                                           miniPreviewKittyNative,
#else
                                           false,
#endif
#ifdef HAVE_LIBSIXEL
                                           miniPreviewSixelColors,
#else
                                           256,
#endif
                                           0xE6u);
            d->insert(pvC3);

            d->insert(new FlatActionLabel(TRect(71, 6, 76, 7), "[x]", cmVisualSceneDelBg));
            d->insert(new FlatActionLabel(TRect(71, 14, 76, 15), "[x]", cmVisualSceneDelC2));
            d->insert(new FlatActionLabel(TRect(71, 22, 76, 23), "[x]", cmVisualSceneDelC3));

            std::string st = trim(statusLine);
            if (st.size() > 98)
                st = st.substr(0, 95) + "...";
            if (!st.empty())
                d->insert(new TStaticText(TRect(2, 1, 102, 2), TStringView(st.c_str(), (unsigned)st.size())));

            d->insert(new CleanButton(TRect(14, 26, 30, 28), "Guardar", cmOK, true));
            d->insert(new CleanButton(TRect(32, 26, 48, 28), "Default", cmVisualSaveDefaults, false));
            d->insert(new CleanButton(TRect(50, 26, 68, 28), "Agregar visual", cmVisualSceneOpenLibrary, false));
            d->insert(new CleanButton(TRect(70, 26, 86, 28), "Cancelar", cmCancel, false));

            d->onRefresh = [&]() {
                bgId = readId(lvBgId);
                c2Id = readId(lvC2Id);
                c3Id = readId(lvC3Id);
                bgIdx = readVar(lvBgVar);
                c2Idx = readVar(lvC2Var);
                c3Idx = readVar(lvC3Var);
                clampVar(visualLibrary.backgrounds, bgId, bgIdx);
                clampVar(visualLibrary.characters, c2Id, c2Idx);
                clampVar(visualLibrary.characters, c3Id, c3Idx);
                fillVarList(lvBgVar, visualLibrary.backgrounds, bgId, bgIdx);
                fillVarList(lvC2Var, visualLibrary.characters, c2Id, c2Idx);
                fillVarList(lvC3Var, visualLibrary.characters, c3Id, c3Idx);
                bgIdx = readVar(lvBgVar);
                c2Idx = readVar(lvC2Var);
                c3Idx = readVar(lvC3Var);
                loadPreview(pickAbs(visualLibrary.backgrounds, bgId, bgIdx), rgbBg, iwBg, ihBg);
                loadPreview(pickAbs(visualLibrary.characters, c2Id, c2Idx), rgbC2, iwC2, ihC2);
                loadPreview(pickAbs(visualLibrary.characters, c3Id, c3Idx), rgbC3, iwC3, ihC3);
                if (pvBg) pvBg->drawView();
                if (pvC2) pvC2->drawView();
                if (pvC3) pvC3->drawView();
            };

            const ushort res = deskTop->execView(d);

            bgId = readId(lvBgId);
            c2Id = readId(lvC2Id);
            c3Id = readId(lvC3Id);
            bgIdx = readVar(lvBgVar);
            c2Idx = readVar(lvC2Var);
            c3Idx = readVar(lvC3Var);
            destroy(d);

            clampVar(visualLibrary.backgrounds, bgId, bgIdx);
            clampVar(visualLibrary.characters, c2Id, c2Idx);
            clampVar(visualLibrary.characters, c3Id, c3Idx);

            if (res == cmCancel)
                return;

            if (res == cmVisualSceneOpenLibrary) {
                showVisualLibraryDialog();
                return;
            }

            if (res == cmVisualSceneRefresh)
                continue;

            if (res == cmVisualSceneDelBg) {
                eraseVariantFromMap(visualLibrary.backgrounds, bgId, bgIdx, "fondos");
                continue;
            }
            if (res == cmVisualSceneDelC2 || res == cmVisualSceneDelC3) {
                const bool delC2 = (res == cmVisualSceneDelC2);
                eraseVariantFromMap(visualLibrary.characters, delC2 ? c2Id : c3Id, delC2 ? c2Idx : c3Idx, "personajes");
                continue;
            }

            RwChapterScene ch {};
            if (!bgId.empty()) {
                ch.hasBg = true;
                ch.bgId = bgId;
                ch.bgIdx = bgIdx;
            }
            if (!c2Id.empty()) {
                ch.hasC2 = true;
                ch.c2Id = c2Id;
                ch.c2Idx = c2Idx;
            }
            if (!c3Id.empty()) {
                ch.hasC3 = true;
                ch.c3Id = c3Id;
                ch.c3Idx = c3Idx;
            }

            if (res == cmOK) {
                if (lastEditorPath.empty()) {
                    statusLine = "Guarda el texto en un .txt del capitulo para escribir chapter_scene.cfg.";
                    continue;
                }
                const std::string outScene = joinPath(fs::path(lastEditorPath).parent_path().string(), "chapter_scene.cfg");
                rwWriteChapterSceneCfg(outScene, ch);
                chapterScenePathAbs = absolutePath(outScene);
                chapterScene = ch;
                statusLine = "Escena guardada para este capitulo.";
            } else if (res == cmVisualSaveDefaults) {
                ensureVisualConfigForWrite();
                if (!visualLibrary.loaded) {
                    visualLibrary.loaded = true;
                    visualLibrary.backgrounds.clear();
                    visualLibrary.characters.clear();
                    visualLibrary.assetRootSubpath = "assets";
                }
                visualLibrary.defBgId = bgId;
                visualLibrary.defBgIdx = bgIdx;
                visualLibrary.defC2Id = c2Id;
                visualLibrary.defC2Idx = c2Idx;
                visualLibrary.defC3Id = c3Id;
                visualLibrary.defC3Idx = c3Idx;
                persistVisualLibraryToDisk();
                statusLine = "Defaults guardados para nuevos capitulos/carpetas.";
            }

            reloadVisualLibraryFromDisk();
            reloadChapterSceneForEditorFile(lastEditorPath);
            reloadMiniPreviewImage();
            updateMiniPreviewWindowTitles();
            redrawMiniPreviewOnly();
            if (deskTop)
                deskTop->drawView();
            redraw();
        }
    }

    /** Garantiza ruta de visuals.cfg y biblioteca en memoria antes de escribir. */
    void ensureVisualConfigForWrite() {
        if (visualConfigPathAbs.empty()) {
            std::string base = trim(visualGlobalBaseDirAbs);
            if (base.empty())
                base = absolutePath(projectDir);
            visualConfigPathAbs = absolutePath(joinPath(base, "visuals.cfg"));
            visualBaseDirAbs = base;
        }
        if (!visualLibrary.loaded) {
            visualLibrary.loaded = true;
            visualLibrary.backgrounds.clear();
            visualLibrary.characters.clear();
            if (trim(visualLibrary.assetRootSubpath).empty())
                visualLibrary.assetRootSubpath = "assets";
        }
    }

    void persistVisualLibraryToDisk() {
        rwWriteVisualsCfg(visualConfigPathAbs, visualLibrary);
        rwLoadVisualsCfg(visualConfigPathAbs, visualLibrary);
    }

    /** Abre el dialogo Turbo Vision de archivo con vista previa (misma pipeline que Mini: Kitty / Sixel / truecolor). */
    std::string browseImageFileAbs(const std::string &startDirHint);

    /* Alta de recursos visuales: captura id+ruta y persiste en visuals.cfg + assets/. */
    void showVisualLibraryDialog() {
        if (!deskTop)
            return;
        if (editorWindow && editorWindow->editor) {
            const char *fn = editorWindow->editor->fileName;
            if (fn && fn[0] != '\0') {
                lastEditorPath = absolutePath(fn);
                syncVisualGlobalRootFromEditorPath(lastEditorPath);
            }
        }
        reloadVisualLibraryFromDisk();
        visualLibrary.assetRootSubpath = "assets";
        ensureVisualConfigForWrite();
        if (!visualLibrary.loaded)
            reloadVisualLibraryFromDisk();

        std::string draftPathChar;
        std::string draftPathBg;
        std::string draftIdChar;
        std::string draftIdBg;
        std::string lastAction;

        auto ensureNovelFromEditor = [&]() -> bool {
            if (!editorWindow || !editorWindow->editor) {
                lastAction = "Abre un .txt del capitulo para ubicar la novela.";
                return false;
            }
            const char *fn = editorWindow->editor->fileName;
            if (!fn || fn[0] == '\0') {
                lastAction = "Guarda el buffer en un .txt dentro de la novela.";
                return false;
            }
            lastEditorPath = absolutePath(fn);
            syncVisualGlobalRootFromEditorPath(lastEditorPath);
            reloadVisualLibraryFromDisk();
            visualLibrary.assetRootSubpath = "assets";
            ensureVisualConfigForWrite();
            if (!visualLibrary.loaded)
                reloadVisualLibraryFromDisk();
            return true;
        };

        for (;;) {
            char pathCharBuf[kMaxPreviewImagePath + 1] = {};
            char pathBgBuf[kMaxPreviewImagePath + 1] = {};
            char idCharBuf[72] = {};
            char idBgBuf[72] = {};
            if (!draftPathChar.empty())
                std::strncpy(pathCharBuf, draftPathChar.c_str(), kMaxPreviewImagePath);
            if (!draftPathBg.empty())
                std::strncpy(pathBgBuf, draftPathBg.c_str(), kMaxPreviewImagePath);
            if (!draftIdChar.empty())
                std::strncpy(idCharBuf, draftIdChar.c_str(), sizeof(idCharBuf) - 1);
            if (!draftIdBg.empty())
                std::strncpy(idBgBuf, draftIdBg.c_str(), sizeof(idBgBuf) - 1);

            std::string displayPathChar = trim(std::string(pathCharBuf));
            std::string displayPathBg = trim(std::string(pathBgBuf));

            static const char kVisLibDlgTitle[] = "Agregar elemento visual";

            const short xL = 2, xR = 82, xMid = 42;
            /* Sin Buscar/Ubicar: el clic en la vista previa abre el selector (cmVisualBrowse*). */
            VisualLibraryDialog *d = new VisualLibraryDialog(
                TRect(2, 0, 85, 23), TStringView(kVisLibDlgTitle, (unsigned)std::strlen(kVisLibDlgTitle)));
            d->options |= ofCentered;
            d->setState(sfShadow, False);
            d->palette = dpBlueDialog;
            d->insert(new TStaticText(TRect(xL, 2, xMid, 3), "PERSONAJE"));
            d->insert(new TStaticText(TRect(xMid + 1, 2, xR, 3), "PAISAJE"));

            TInputLine *inIdC = new TInputLine(TRect(9, 13, 38, 14), 24);
            d->insert(new TLabel(TRect(xL, 13, 8, 14), "~I~d", inIdC));
            d->insert(inIdC);
            d->insert(new TinyCharIdListGlyph(TRect(38, 13, 40, 14)));
            TInputLine *inIdB = new TInputLine(TRect(54, 13, xR, 14), 26);
            d->insert(new TLabel(TRect(xMid + 1, 13, 53, 14), "~P~aisaje", inIdB));
            d->insert(inIdB);

            TInputLine *inPathC = new TInputLine(TRect(9, 15, 40, 16), kMaxPreviewImagePath);
            d->insert(new TLabel(TRect(xL, 15, 8, 16), "~R~uta", inPathC));
            d->insert(inPathC);
            TInputLine *inPathB = new TInputLine(TRect(53, 15, xR, 16), kMaxPreviewImagePath);
            d->insert(new TLabel(TRect(xMid + 1, 15, 52, 16), "~M~edia", inPathB));
            d->insert(inPathB);

            d->insert(new CleanButton(TRect(28, 18, 41, 20), "Agregar", cmVisualLibAdd, true));
            d->insert(new CleanButton(TRect(43, 18, 56, 20), "Cancelar", cmCancel, false));

            {
                std::string st = trim(lastAction);
                if (st.size() > 76)
                    st = st.substr(0, 73) + "...";
                d->insert(new TStaticText(TRect(xL, 20, xR, 21), TStringView(st.c_str(), (unsigned)st.size())));
            }

            d->insert(new LibraryImagePreview(TRect(xL, 4, xMid, 12), &displayPathChar, &visualBaseDirAbs,
                                              &visualLibrary.assetRootSubpath, &textColor, &backColor,
#ifdef HAVE_LIBSIXEL
                                              miniPreviewSixel,
#else
                                              false,
#endif
#if !defined(_WIN32)
                                              miniPreviewKittyNative,
#else
                                              false,
#endif
#ifdef HAVE_LIBSIXEL
                                              miniPreviewSixelColors,
#else
                                              256,
#endif
                                              kKittyVisualLibImageId1, cmVisualBrowseCharPath));
            d->insert(new LibraryImagePreview(TRect(xMid + 1, 4, xR, 12), &displayPathBg, &visualBaseDirAbs,
                                              &visualLibrary.assetRootSubpath, &textColor, &backColor,
#ifdef HAVE_LIBSIXEL
                                              miniPreviewSixel,
#else
                                              false,
#endif
#if !defined(_WIN32)
                                              miniPreviewKittyNative,
#else
                                              false,
#endif
#ifdef HAVE_LIBSIXEL
                                              miniPreviewSixelColors,
#else
                                              256,
#endif
                                              kKittyVisualLibImageId2, cmVisualBrowseBgPath));

            inPathC->setData(static_cast<void *>(pathCharBuf));
            inPathB->setData(static_cast<void *>(pathBgBuf));
            inIdC->setData(static_cast<void *>(idCharBuf));
            inIdB->setData(static_cast<void *>(idBgBuf));
            d->setPickTarget(deskTop, &visualLibrary.characters, idCharBuf, inIdC);

            d->setCurrent(inPathC, normalSelect);
            const ushort res = deskTop->execView(d);

            inPathC->getData(static_cast<void *>(pathCharBuf));
            inPathB->getData(static_cast<void *>(pathBgBuf));
            inIdC->getData(static_cast<void *>(idCharBuf));
            inIdB->getData(static_cast<void *>(idBgBuf));
            destroy(d);

            draftPathChar = trim(std::string(pathCharBuf));
            draftPathBg = trim(std::string(pathBgBuf));
            draftIdChar = trim(std::string(idCharBuf));
            draftIdBg = trim(std::string(idBgBuf));

            if (res == cmCancel)
                break;

            if (res == cmVisualBrowseCharPath) {
                {
                    std::string start = trim(browserDir);
                    if (start.empty())
                        start = !trim(visualBaseDirAbs).empty() ? visualBaseDirAbs : absolutePath(projectDir);
                    const std::string got = browseImageFileAbs(start);
                    if (!got.empty())
                        draftPathChar = got;
                }
                continue;
            }
            if (res == cmVisualBrowseBgPath) {
                {
                    std::string start = trim(browserDir);
                    if (start.empty())
                        start = !trim(visualBaseDirAbs).empty() ? visualBaseDirAbs : absolutePath(projectDir);
                    const std::string got = browseImageFileAbs(start);
                    if (!got.empty())
                        draftPathBg = got;
                }
                continue;
            }
            if (res != cmVisualLibAdd)
                break;

            if (!ensureNovelFromEditor()) {
                continue;
            }
            visualLibrary.assetRootSubpath = "assets";

            const bool chAny = !draftIdChar.empty() || !draftPathChar.empty();
            const bool bgAny = !draftIdBg.empty() || !draftPathBg.empty();
            const bool chOk = !draftIdChar.empty() && !draftPathChar.empty();
            const bool bgOk = !draftIdBg.empty() && !draftPathBg.empty();

            if (chAny && !chOk) {
                lastAction = "Personaje: indica id y ruta, o deja ambos vacios.";
                continue;
            }
            if (bgAny && !bgOk) {
                lastAction = "Paisaje: indica id y ruta, o deja ambos vacios.";
                continue;
            }
            if (!chOk && !bgOk) {
                lastAction = "Nada que guardar (ambas columnas vacias).";
                continue;
            }

            std::vector<std::string> okParts;
            if (chOk) {
                std::string relStored, impErr;
                if (!rwImportOrNormalizeVaultImage(false, draftIdChar, draftPathChar, visualBaseDirAbs, visualLibrary.assetRootSubpath,
                                                   relStored, impErr)) {
                    lastAction = "Personaje: " + impErr;
                    continue;
                }
                visualLibrary.characters[draftIdChar].push_back(relStored);
                persistVisualLibraryToDisk();
                okParts.push_back("personaje '" + draftIdChar + "'");
            }
            if (bgOk) {
                std::string relStored, impErr;
                if (!rwImportOrNormalizeVaultImage(true, draftIdBg, draftPathBg, visualBaseDirAbs, visualLibrary.assetRootSubpath,
                                                   relStored, impErr)) {
                    lastAction = "Paisaje: " + impErr;
                    continue;
                }
                visualLibrary.backgrounds[draftIdBg].push_back(relStored);
                persistVisualLibraryToDisk();
                okParts.push_back("paisaje '" + draftIdBg + "'");
            }

            reloadMiniPreviewImage();
            redrawMiniPreviewOnly();
            if (deskTop)
                deskTop->drawView();
            redraw();

            std::string msg = "OK Guardado";
            for (size_t i = 0; i < okParts.size(); ++i) {
                msg += (i == 0 ? ": " : ", ");
                msg += okParts[i];
            }
            lastAction = msg;
            draftPathChar.clear();
            draftPathBg.clear();
            draftIdChar.clear();
            draftIdBg.clear();
            continue;
        }
    }

    void showVisualGalleryDialog() {
        if (!deskTop)
            return;
        if (editorWindow && editorWindow->editor) {
            const char *fn = editorWindow->editor->fileName;
            if (fn && fn[0] != '\0') {
                lastEditorPath = absolutePath(fn);
                syncVisualGlobalRootFromEditorPath(lastEditorPath);
            }
        }
        reloadVisualLibraryFromDisk();
        ensureVisualConfigForWrite();

        ushort kindSel = 1; // 0=fondos, 1=personajes
        int selBg = 0, selChar = 0;
        int varBg = 0, varChar = 0;

        for (;;) {
            const bool isBg = (kindSel == 0);
            auto &m = isBg ? visualLibrary.backgrounds : visualLibrary.characters;
            std::vector<std::string> ids;
            ids.reserve(m.size());
            for (const auto &pr : m)
                ids.push_back(pr.first);

            int &selRef = isBg ? selBg : selChar;
            int &varRef = isBg ? varBg : varChar;
            if (ids.empty()) {
                selRef = 0;
                varRef = 0;
            } else {
                selRef = std::clamp(selRef, 0, (int)ids.size() - 1);
                const auto &vv = m[ids[(size_t)selRef]];
                const int maxV = std::max(0, (int)vv.size() - 1);
                varRef = std::clamp(varRef, 0, maxV);
            }

            const std::string selId = ids.empty() ? std::string{} : ids[(size_t)selRef];
            std::string relPath;
            int variants = 0;
            if (!selId.empty()) {
                auto it = m.find(selId);
                if (it != m.end()) {
                    variants = (int)it->second.size();
                    if (variants > 0)
                        relPath = it->second[(size_t)std::clamp(varRef, 0, variants - 1)];
                }
            }
            const std::string absPath = relPath.empty()
                                            ? std::string{}
                                            : rwResolveMediaPathWithCapFallback(visualBaseDirAbs, relPath, visualLibrary.assetRootSubpath,
                                                                                visualNovelRootAbs);

            std::vector<uint8_t> previewRgb;
            int previewW = 0, previewH = 0;
            if (!absPath.empty()) {
                int w = 0, h = 0, comp = 0;
                unsigned char *data = stbi_load(absPath.c_str(), &w, &h, &comp, 3);
                if (data && w > 0 && h > 0) {
                    previewRgb.assign(data, data + (size_t)w * (size_t)h * 3u);
                    stbi_image_free(data);
                    previewW = w;
                    previewH = h;
                    downscaleRgbImageForMini(previewRgb, previewW, previewH, std::max(320, miniPreviewMaxSide * 2));
                }
            }

            std::string head = pathTitleForWidth(visualConfigPathAbs.empty() ? joinPath(visualBaseDirAbs, "visuals.cfg")
                                                                             : visualConfigPathAbs,
                                                 88);
            std::string info1 = std::string(isBg ? "FONDOS" : "PERSONAJES") + "  ids: " + std::to_string((int)ids.size());
            std::string info2 =
                std::string("Id: ") + (selId.empty() ? std::string("(sin seleccion)") : selId) + "    variante: " +
                std::to_string(varRef) + "/" + std::to_string(std::max(0, variants - 1));
            std::string info3 = std::string("Ruta: ") + (relPath.empty() ? std::string("(sin imagen)") : relPath);

            VisualGalleryDialog *d = new VisualGalleryDialog(TRect(2, 1, 98, 30), "Galeria visual");
            d->options |= ofCentered;
            d->palette = dpBlueDialog;
            const short xL = 2, xR = 95;
            d->insert(new TStaticText(TRect(xL, 1, xR, 2), head.c_str()));
            d->insert(new TStaticText(TRect(xL, 2, 45, 3), info1.c_str()));
            d->insert(new TStaticText(TRect(xL, 3, 45, 4), info2.c_str()));
            d->insert(new TStaticText(TRect(xL, 4, 45, 5), pathTitleForWidth(info3, 42).c_str()));

            TRadioButtons *rb = new TRadioButtons(TRect(xL, 6, 26, 9), new TSItem("~F~ondo", new TSItem("~P~ersonaje", nullptr)));
            d->insert(rb);
            rb->setData(&kindSel);

            d->insert(new TButton(TRect(xL, 10, xL + 8, 12), "< id", cmVisualGalPrevId, bfNormal));
            d->insert(new TButton(TRect(xL + 10, 10, xL + 18, 12), "id >", cmVisualGalNextId, bfNormal));
            d->insert(new TButton(TRect(xL, 13, xL + 8, 15), "< var", cmVisualGalPrevVar, bfNormal));
            d->insert(new TButton(TRect(xL + 10, 13, xL + 18, 15), "var >", cmVisualGalNextVar, bfNormal));
            d->insert(new TButton(TRect(xL, 24, xL + 18, 26), "Cerrar", cmCancel, bfDefault));

            auto *pv = new PixelPreviewView(TRect(30, 6, 94, 26), &pixelCanvas, &previewRgb, &previewW, &previewH, &textColor,
                                            &backColor,
#ifdef HAVE_LIBSIXEL
                                            miniPreviewSixel,
#else
                                            false,
#endif
#if !defined(_WIN32)
                                            miniPreviewKittyNative,
#else
                                            false,
#endif
#ifdef HAVE_LIBSIXEL
                                            miniPreviewSixelColors,
#else
                                            256,
#endif
                                            0xE2u);
            d->insert(pv);

            TColorAttr prevShadowAttr = shadowAttr;
            shadowAttr = TColorAttr(0x00);
            const ushort res = deskTop->execView(d);
            shadowAttr = prevShadowAttr;
            rb->getData(&kindSel);
            destroy(d);

            if (res == cmCancel || res == cmOK)
                break;

            if (res == cmVisualGalPrevId || res == cmVisualGalNextId) {
                if (!ids.empty()) {
                    if (res == cmVisualGalPrevId)
                        selRef = (selRef - 1 + (int)ids.size()) % (int)ids.size();
                    else
                        selRef = (selRef + 1) % (int)ids.size();
                    varRef = 0;
                }
                continue;
            }
            if (res == cmVisualGalPrevVar || res == cmVisualGalNextVar) {
                if (!selId.empty()) {
                    auto it = m.find(selId);
                    if (it != m.end() && !it->second.empty()) {
                        const int n = (int)it->second.size();
                        if (res == cmVisualGalPrevVar)
                            varRef = (varRef - 1 + n) % n;
                        else
                            varRef = (varRef + 1) % n;
                    }
                }
                continue;
            }
        }
    }

    /** Recarga PNG/JPG para las tres ventanas Mini. */
    void reloadMiniPreviewImage() {
        /* loadAppearancePreferences() no se llama aquí: la ruta nueva aún no está en disco y sobrescribiría la mini. */
        auto loadOne = [&](const std::string &path, std::vector<uint8_t> &rgb, int &iw, int &ih) {
            rgb.clear();
            iw = ih = 0;
            if (path.empty())
                return;
            int w = 0, h = 0, comp = 0;
            unsigned char *data = stbi_load(path.c_str(), &w, &h, &comp, 3);
            if (!data || w <= 0 || h <= 0)
                return;
            rgb.assign(data, data + (size_t)w * (size_t)h * 3u);
            stbi_image_free(data);
            iw = w;
            ih = h;
            downscaleRgbImageForMini(rgb, iw, ih, miniPreviewMaxSide);
        };
        std::string vp1, vp2, vp3;
        if (computeVisualMiniPaths(vp1, vp2, vp3)) {
            loadOne(vp1, miniPreviewRgb, miniPreviewImgW, miniPreviewImgH);
            loadOne(vp2, miniPreviewRgb2, miniPreviewImgW2, miniPreviewImgH2);
            loadOne(vp3, miniPreviewRgb3, miniPreviewImgW3, miniPreviewImgH3);
        } else {
            miniPreviewRgb.clear();
            miniPreviewRgb2.clear();
            miniPreviewRgb3.clear();
            miniPreviewImgW = miniPreviewImgH = 0;
            miniPreviewImgW2 = miniPreviewImgH2 = 0;
            miniPreviewImgW3 = miniPreviewImgH3 = 0;
        }
    }

    /** Lee workspace.cfg (v1): cwd del panel, visibilidad del panel, ultimo archivo del editor. */
    void loadWorkspaceSession() {
        std::ifstream in(workspacePath);
        if (!in.is_open()) return;
        std::string ver;
        if (!std::getline(in, ver) || trim(ver) != "v1") return;
        std::string cwdVal, editVal, visualRootVal;
        std::string line;
        while (std::getline(in, line)) {
            std::string t = trim(line);
            if (t.empty()) continue;
            size_t sp = t.find(' ');
            if (sp == std::string::npos) continue;
            std::string key = trim(t.substr(0, sp));
            std::string val = t.substr(sp + 1);
            while (!val.empty() && val[0] == ' ') val.erase(0, 1);
            if (key == "cwd")
                cwdVal = val;
            else if (key == "edit")
                editVal = val;
            else if (key == "visualRoot")
                visualRootVal = val;
            else if (key == "panel")
                filePanelVisible = (trim(val) != "0");
            else if (key == "splitY")
                sessionPanelSplitY = parseIntClamped(val, 0, 4095, sessionPanelSplitY);
            else if (key == "savedSplitY")
                savedLayoutSplitY = parseIntClamped(val, 0, 4095, savedLayoutSplitY);
            else if (key == "navGeom") {
                TRect gr;
                if (rwParseRect4(val, gr)) {
                    storedNavGeom = gr;
                    loadedNavGeom = true;
                }
            } else if (key == "savedNavGeom") {
                TRect gr;
                if (rwParseRect4(val, gr)) {
                    savedLayoutNavGeom = gr;
                    loadedSavedLayoutNavGeom = true;
                }
            } else if (key == "miniGeom1") {
                TRect gr;
                if (rwParseRect4(val, gr)) {
                    storedMiniGeom[0] = gr;
                    loadedMiniGeom[0] = true;
                }
            } else if (key == "savedMiniGeom1") {
                TRect gr;
                if (rwParseRect4(val, gr)) {
                    savedLayoutMiniGeom[0] = gr;
                    loadedSavedLayoutMiniGeom[0] = true;
                }
            } else if (key == "miniGeom2") {
                TRect gr;
                if (rwParseRect4(val, gr)) {
                    storedMiniGeom[1] = gr;
                    loadedMiniGeom[1] = true;
                }
            } else if (key == "savedMiniGeom2") {
                TRect gr;
                if (rwParseRect4(val, gr)) {
                    savedLayoutMiniGeom[1] = gr;
                    loadedSavedLayoutMiniGeom[1] = true;
                }
            } else if (key == "miniGeom3") {
                TRect gr;
                if (rwParseRect4(val, gr)) {
                    storedMiniGeom[2] = gr;
                    loadedMiniGeom[2] = true;
                }
            } else if (key == "savedMiniGeom3") {
                TRect gr;
                if (rwParseRect4(val, gr)) {
                    savedLayoutMiniGeom[2] = gr;
                    loadedSavedLayoutMiniGeom[2] = true;
                }
            } else if (key == "miniGeom") {
                /* Compatibilidad: antigua clave unica -> solo ventana 1 si aun no hay miniGeom1. */
                TRect gr;
                if (rwParseRect4(val, gr) && !loadedMiniGeom[0]) {
                    storedMiniGeom[0] = gr;
                    loadedMiniGeom[0] = true;
                }
            } else if (key == "editorGeom") {
                TRect gr;
                if (rwParseRect4(val, gr)) {
                    storedEditorGeom = gr;
                    loadedEditorGeom = true;
                }
            } else if (key == "savedEditorGeom") {
                TRect gr;
                if (rwParseRect4(val, gr)) {
                    savedLayoutEditorGeom = gr;
                    loadedSavedLayoutEditorGeom = true;
                }
#if !defined(_WIN32)
            } else if (key == "savedKittyCellHeightPx") {
                savedLayoutKittyCellHeightPx = parseIntClamped(val, 0, 256, savedLayoutKittyCellHeightPx);
#endif
            }
        }

        std::error_code ec;
        if (!cwdVal.empty() && fs::is_directory(fs::path(cwdVal), ec))
            browserDir = absolutePath(cwdVal);

        lastEditorPath.clear();
        if (!editVal.empty())
            lastEditorPath = editVal;
        visualGlobalBaseDirAbs.clear();
        if (!visualRootVal.empty() && fs::is_directory(fs::path(visualRootVal), ec))
            visualGlobalBaseDirAbs = absolutePath(visualRootVal);
    }

    void saveWorkspaceSession() {
        captureLayoutToStored();
        std::ofstream out(workspacePath, std::ios::trunc);
        if (!out.is_open()) return;
        out << "v1\n";
        out << "cwd " << browserDir << "\n";
        out << "panel " << (filePanelVisible ? 1 : 0) << "\n";
        out << "splitY " << sessionPanelSplitY << "\n";
        out << "savedSplitY " << savedLayoutSplitY << "\n";
        if (loadedNavGeom)
            out << "navGeom " << storedNavGeom.a.x << " " << storedNavGeom.a.y << " " << storedNavGeom.b.x << " "
                << storedNavGeom.b.y << "\n";
        if (loadedSavedLayoutNavGeom)
            out << "savedNavGeom " << savedLayoutNavGeom.a.x << " " << savedLayoutNavGeom.a.y << " "
                << savedLayoutNavGeom.b.x << " " << savedLayoutNavGeom.b.y << "\n";
        for (int i = 0; i < 3; ++i) {
            if (loadedMiniGeom[i]) {
                const TRect &g = storedMiniGeom[i];
                out << "miniGeom" << (i + 1) << " " << g.a.x << " " << g.a.y << " " << g.b.x << " " << g.b.y << "\n";
            }
            if (loadedSavedLayoutMiniGeom[i]) {
                const TRect &g = savedLayoutMiniGeom[i];
                out << "savedMiniGeom" << (i + 1) << " " << g.a.x << " " << g.a.y << " " << g.b.x << " " << g.b.y
                    << "\n";
            }
        }
        if (loadedEditorGeom)
            out << "editorGeom " << storedEditorGeom.a.x << " " << storedEditorGeom.a.y << " " << storedEditorGeom.b.x
                << " " << storedEditorGeom.b.y << "\n";
        if (loadedSavedLayoutEditorGeom)
            out << "savedEditorGeom " << savedLayoutEditorGeom.a.x << " " << savedLayoutEditorGeom.a.y << " "
                << savedLayoutEditorGeom.b.x << " " << savedLayoutEditorGeom.b.y << "\n";
#if !defined(_WIN32)
        if (savedLayoutKittyCellHeightPx > 0)
            out << "savedKittyCellHeightPx " << savedLayoutKittyCellHeightPx << "\n";
#endif
        if (!lastEditorPath.empty())
            out << "edit " << lastEditorPath << "\n";
        if (!trim(visualGlobalBaseDirAbs).empty())
            out << "visualRoot " << visualGlobalBaseDirAbs << "\n";
    }

    void saveLayoutSnapshotSlot() {
        captureLayoutToStored();
        loadedSavedLayoutNavGeom = loadedNavGeom;
        savedLayoutNavGeom = storedNavGeom;
        for (int i = 0; i < 3; ++i) {
            loadedSavedLayoutMiniGeom[i] = loadedMiniGeom[i];
            savedLayoutMiniGeom[i] = storedMiniGeom[i];
        }
        loadedSavedLayoutEditorGeom = loadedEditorGeom;
        savedLayoutEditorGeom = storedEditorGeom;
        savedLayoutSplitY = sessionPanelSplitY;
#if !defined(_WIN32)
        if (terminalIsKitty()) {
            const int h = kittyTermCellHeightPx();
            if (h > 0)
                savedLayoutKittyCellHeightPx = h;
        } else if (kittySavedCellHeightPx > 0)
            savedLayoutKittyCellHeightPx = kittySavedCellHeightPx;
#endif
        saveWorkspaceSession();
        messageBox(mfInformation | mfOKButton, "Layout guardado.");
    }

    void restoreLayoutSnapshotSlot() {
        if (!loadedSavedLayoutNavGeom && !loadedSavedLayoutEditorGeom &&
            !loadedSavedLayoutMiniGeom[0] && !loadedSavedLayoutMiniGeom[1] && !loadedSavedLayoutMiniGeom[2]) {
            messageBox(mfInformation | mfOKButton, "Aun no hay un layout guardado.");
            return;
        }
        loadedNavGeom = loadedSavedLayoutNavGeom;
        storedNavGeom = savedLayoutNavGeom;
        for (int i = 0; i < 3; ++i) {
            loadedMiniGeom[i] = loadedSavedLayoutMiniGeom[i];
            storedMiniGeom[i] = savedLayoutMiniGeom[i];
        }
        loadedEditorGeom = loadedSavedLayoutEditorGeom;
        storedEditorGeom = savedLayoutEditorGeom;
        sessionPanelSplitY = savedLayoutSplitY;
#if !defined(_WIN32)
        if (terminalIsKitty() && kittyZoomPersistenceEnabled() && savedLayoutKittyCellHeightPx > 0) {
            kittySavedCellHeightPx = savedLayoutKittyCellHeightPx;
            tryAdjustKittyFontTowardCellHeight(savedLayoutKittyCellHeightPx);
        }
#endif
        refreshNavigatorWidget();
        relayoutEditorWindow();
        applyDesktopPatternChar();
        updateMiniPreviewWindowTitles();
        if (deskTop)
            deskTop->drawView();
        redraw();
        saveWorkspaceSession();
    }

    void updateNavWindowTitle() {
        if (!navWindow)
            return;
        const int navW = std::max(1, (int)navWindow->size.x);
        /* TFrame dibuja el titulo con moveStr(..., ancho <= size.x - 10); si pasamos de mas, recorta por la derecha y se pierde el sufijo. */
        std::string t = pathTitleForWidth(browserDir, std::max(1, navW - 10));
        char *nt = newStr(TStringView(t.c_str(), t.size()));
        if (!nt)
            return;
        delete[] (char *)navWindow->title;
        navWindow->title = nt;
        if (navWindow->frame)
            navWindow->frame->drawView();
    }

    /** Mini 1: solo nombre de archivo del buffer (sin ruta) o cadena vacia si no hay archivo guardado. */
    std::string mini1EditorFileTitle() const {
        if (editorWindow && editorWindow->editor) {
            const char *fn = editorWindow->editor->fileName;
            if (fn && fn[0] != '\0') {
                fs::path p(fn);
                std::string base = p.filename().string();
                if (!base.empty())
                    return base;
            }
        }
        return {};
    }

    void updateMiniPreviewWindowTitles() {
        auto setTitleAbs = [&](TWindow *w, std::string t) {
            if (!w)
                return;
            t = trim(t);
            const int maxCols = std::max(4, (int)w->size.x - 2);
            while ((int)strwidth(TStringView(t.c_str(), t.size())) > maxCols && t.size() > 1)
                t.pop_back();
            char *nt = newStr(TStringView(t.c_str(), t.size()));
            if (!nt)
                return;
            delete[] (char *)w->title;
            w->title = nt;
            if (w->frame)
                w->frame->drawView();
        };
        auto setTitle = [&](TWindow *w, const std::string &tUser, const char *fallback) {
            if (!w)
                return;
            std::string t = trim(tUser);
            if (t.empty())
                t = fallback;
            setTitleAbs(w, std::move(t));
        };
        setTitleAbs(previewWindow, mini1EditorFileTitle());
        std::string mergeBg, mergeC2, mergeC3;
        int mergeBgI = 0, mergeC2I = 0, mergeC3I = 0;
        rwMergeSceneIds(visualLibrary, chapterScene, mergeBg, mergeBgI, mergeC2, mergeC2I, mergeC3, mergeC3I);
        setTitle(previewWindow2, mergeC2, "Mini 2");
        setTitle(previewWindow3, mergeC3, "Mini 3");
    }

    void syncFilePanelListing() {
        if (!navListView)
            return;
        navListView->setItems(buildFileManagerItems());
        updateNavWindowTitle();
    }

    void redrawMiniPreviewOnly() {
        if (previewPixelView)
            previewPixelView->drawView();
        if (previewPixelView2)
            previewPixelView2->drawView();
        if (previewPixelView3)
            previewPixelView3->drawView();
        if (previewWindow && previewWindow->frame)
            previewWindow->frame->drawView();
        if (previewWindow2 && previewWindow2->frame)
            previewWindow2->frame->drawView();
        if (previewWindow3 && previewWindow3->frame)
            previewWindow3->frame->drawView();
    }

    void captureLayoutToStored() {
        if (!deskTop)
            return;
        if (navWindow && navWindow->owner == deskTop) {
            storedNavGeom = navWindow->getBounds();
            loadedNavGeom = true;
            sessionPanelSplitY = (int)storedNavGeom.b.y;
        }
        if (previewWindow && previewWindow->owner == deskTop) {
            storedMiniGeom[0] = previewWindow->getBounds();
            loadedMiniGeom[0] = true;
        }
        if (previewWindow2 && previewWindow2->owner == deskTop) {
            storedMiniGeom[1] = previewWindow2->getBounds();
            loadedMiniGeom[1] = true;
        }
        if (previewWindow3 && previewWindow3->owner == deskTop) {
            storedMiniGeom[2] = previewWindow3->getBounds();
            loadedMiniGeom[2] = true;
        }
        if (editorWindow && editorWindow->owner == deskTop) {
            storedEditorGeom = editorWindow->getBounds();
            loadedEditorGeom = true;
        }
    }

    /** Geometría explícita del usuario (workspace.cfg): solo encajar en el área útil, sin reanclar a split/columna. */
    TRect clampRectToDeskArea(TRect r, int minW, int minH) const {
        if (!deskTop)
            return r;
        TRect wa = rwDeskWorkArea(static_cast<TDeskTop *>(deskTop));
        r = rwClampWindow(r, wa, minW, minH);
        if (r.b.x <= r.a.x)
            r.b.x = (short)(r.a.x + minW);
        if (r.b.y <= r.a.y)
            r.b.y = (short)(r.a.y + minH);
        return r;
    }

    void restoreMainWorkspaceAfterModal() {
        if (!deskTop)
            return;
        reloadMiniPreviewImage();
        if (!previewWindow || !previewWindow2 || !previewWindow3 ||
            (filePanelVisible && (!navWindow || !navListView)))
            createDesktopWidgets();
        else {
            syncFilePanelListing();
            redrawMiniPreviewOnly();
        }
        relayoutEditorWindow();
        applyDesktopPatternChar();
        updateMiniPreviewWindowTitles();
        deskTop->drawView();
        redraw();
    }

    void applyDesktopPatternChar() {
        if (!deskTop) return;
        TDeskTop *dt = static_cast<TDeskTop *>(deskTop);
        if (!dt->background) return;
        bool wantsUnicode = desktopPatternUtf8.size() > 1;

        if (wantsUnicode) {
            if (!unicodeBackground || dt->background != unicodeBackground) {
                if (dt->background && dt->background->owner) {
                    TBackground *old = dt->background;
                    dt->remove(old);
                    destroy(old);
                }
                UnicodeBackground *bg = new UnicodeBackground(dt->getExtent(), desktopPatternUtf8, &textColor, &backColor);
                dt->background = bg;
                dt->insert(bg);
                unicodeBackground = bg;
            } else {
                unicodeBackground->setPatternUtf8(desktopPatternUtf8);
            }
            dt->background->drawView();
            restackDesktopWindowsAboveBackground();
            ensureEditorAboveNavigator();
            return;
        }

        if (unicodeBackground && dt->background == unicodeBackground) {
            TBackground *old = dt->background;
            dt->remove(old);
            destroy(old);
            unicodeBackground = nullptr;
            TBackground *bg = new TBackground(dt->getExtent(), desktopPatternChar);
            dt->background = bg;
            dt->insert(bg);
        }
        dt->background->pattern = desktopPatternChar;
        dt->background->drawView();
        restackDesktopWindowsAboveBackground();
        ensureEditorAboveNavigator();
    }

    static ushort execDialog(TDialog *d, void *data) {
        TView *v = TProgram::application->validView(d);
        if (!v) return cmCancel;
        if (data) v->setData(data);
        ushort result = TProgram::deskTop->execView(v);
        if (result != cmCancel && data) v->getData(data);
        destroy(v);
        return result;
    }

    bool promptTitle(const char *dlgTitle, const char *lbl, std::string &outTitle) {
        struct Data { char title[MAX_TITLE]; } data = {{0}};
        TDialog *d = new TDialog(TRect(18, 6, 72, 16), dlgTitle);
        d->options |= ofCentered;
        d->palette = dpBlueDialog;
        TInputLine *input = new TInputLine(TRect(4, 5, 44, 6), MAX_TITLE - 1);
        d->insert(input);
        d->insert(new TLabel(TRect(4, 4, 30, 5), lbl, input));
        d->insert(new TButton(TRect(16, 9, 26, 11), "O~K~", cmOK, bfDefault));
        d->insert(new TButton(TRect(29, 9, 43, 11), "Cancelar", cmCancel, bfNormal));
        d->setCurrent(input, normalSelect);
        ushort res = execDialog(d, &data);
        if (res != cmOK) return false;
        outTitle = trim(data.title);
        return !outTitle.empty();
    }

    bool promptFolderCreateOptions(std::string &outName, bool &outCreateAssets) {
#pragma pack(push, 1)
        struct Data {
            char title[MAX_TITLE - 1];
            ushort options;
        } data = {};
#pragma pack(pop)

        TDialog *d = new TDialog(TRect(18, 6, 74, 17), "Crear carpeta");
        d->options |= ofCentered;
        d->palette = dpBlueDialog;

        TInputLine *input = new TInputLine(TRect(4, 5, 52, 6), MAX_TITLE - 1);
        d->insert(input);
        d->insert(new TLabel(TRect(4, 4, 30, 5), "Nombre", input));
        auto *assetsCb = new FolderAssetsCheckBoxes(TRect(4, 8, 58, 10),
            new TSItem("~I~ncluir subcarpeta assets/", nullptr));
        d->insert(assetsCb);
        d->insert(new TButton(TRect(17, 12, 27, 14), "Crear", cmOK, bfDefault));
        d->insert(new TButton(TRect(30, 12, 44, 14), "Cancelar", cmCancel, bfNormal));
        d->setCurrent(input, normalSelect);

        TView *v = TProgram::application->validView(d);
        if (!v) {
            destroy(d);
            return false;
        }
        v->setData(&data);
        ushort res = TProgram::deskTop->execView(v);
        const bool assetsWanted = assetsCb->wantsAssets();
        if (res != cmCancel)
            v->getData(&data);
        destroy(v);

        if (res != cmOK)
            return false;
        outName = trim(data.title);
        outCreateAssets = assetsWanted;
        return !outName.empty();
    }

    /**
     * Cierra el editor actual. Si hay cambios sin guardar y el usuario cancela el dialogo,
     * no se cierra (evita dejar un editor huerfano + otro nuevo = dos ventanas, scroll roto).
     */
    bool tryCloseCurrentEditor() {
        if (!editorWindow || !editorWindow->owner)
            return true;
        if (!editorWindow->valid(cmClose))
            return false;
        editorWindow->close();
        editorWindow = nullptr;
        updateMiniPreviewWindowTitles();
        return true;
    }

    TRect editorDeskRect() const {
        if (!deskTop) return TRect(short(kFilePanelRightX + 1), 2, 80, 25);
        TRect r = deskTop->getExtent();
        /* Menos margen superior y un poco mas de aire en los lados. */
        r.a.y = 1;
        r.b.x -= 2;
        r.b.y -= 1;
        /* Columna izquierda si hay Mini o File Manager (Ctrl+E ya no quita la Mini). */
        const bool leftStrip = (previewWindow != nullptr) || (previewWindow2 != nullptr) ||
            (previewWindow3 != nullptr) || (filePanelVisible && navWindow != nullptr);
        r.a.x = leftStrip ? (short)(kFilePanelRightX + 2) : 2;
        return r;
    }

    /* Reaplica geometria activa: snapshot guardado o layout automatico segun paneles visibles. */
    void relayoutEditorWindow() {
        if (!editorWindow || !deskTop || editorWindow->owner != deskTop)
            return;
        TRect nr = loadedEditorGeom ? clampRectToDeskArea(storedEditorGeom, 24, 6) : editorDeskRect();
        editorWindow->locate(nr);
    }

    void onToggleFilePanel() {
        filePanelVisible = !filePanelVisible;
        refreshNavigatorWidget();
        relayoutEditorWindow();
        saveWorkspaceSession();
        ensureEditorAboveNavigator();
        applyDesktopPatternChar();
        if (deskTop) {
            if (deskTop->background)
                deskTop->background->drawView();
            deskTop->redraw();
        }
        redraw();
    }

    /** Si el .txt no esta bajo el proyecto del IDE, la biblioteca visual es la novela (ej. meep/), no retro_writer/. */
    void syncVisualGlobalRootFromEditorPath(const std::string &absEditorPath) {
        const std::string pd = absolutePath(projectDir);
        if (trim(absEditorPath).empty()) {
            if (trim(visualGlobalBaseDirAbs).empty())
                visualGlobalBaseDirAbs = pd;
            return;
        }
        const std::string ed = absolutePath(absEditorPath);
        if (rwPathIsUnderDirectory(ed, pd))
            visualGlobalBaseDirAbs = pd;
        else
            visualGlobalBaseDirAbs = rwInferNovelRootFromEditor(ed, pd);
    }

    void openFileInEditor(const std::string &path) {
        if (!deskTop) return;
        if (!tryCloseCurrentEditor())
            return;
        std::string absPath;
        if (!path.empty())
            absPath = absolutePath(path);
        syncVisualGlobalRootFromEditorPath(absPath);
        TRect r = loadedEditorGeom ? clampRectToDeskArea(storedEditorGeom, 24, 6) : editorDeskRect();
        const char *titleArg = absPath.empty() ? "" : absPath.c_str();
        RetroEditWindow *w = new RetroEditWindow(r, titleArg, wnNoNumber);
        /* ofTopSelect desactivado: con el valor por defecto el panel suele quedar como ventana actual y el editor pierde foco. */
        w->options &= ~ofTopSelect;
        TView *v = validView(w);
        if (!v) return;
        editorWindow = w;
        deskTop->insert(v);
        ensureEditorAboveNavigator();
        /* Tras ensureEditorAboveNavigator el foco puede quedar en el panel; aquí se fuerza el editor. */
        if (deskTop && editorWindow)
            deskTop->setCurrent(editorWindow, normalSelect);
        /* Al abrir archivo nuevo/largo, asegurar vista desde columna 0 (evita "recorte" visual inicial). */
        if (editorWindow && editorWindow->editor) {
            if (auto *re = dynamic_cast<RetroFileEditor *>(editorWindow->editor)) {
                re->requestInitialViewportReset();
                re->setAutoEmDashFromDoubleHyphen(autoEmDashFromDoubleHyphen);
            }
            editorWindow->editor->scrollTo(0, 0);
            editorWindow->editor->drawView();
        }
        lastEditorPath = std::move(absPath);
        saveWorkspaceSession();
        updateMiniPreviewWindowTitles();
        reloadVisualLibraryFromDisk();
        reloadChapterSceneForEditorFile(lastEditorPath);
        reloadMiniPreviewImage();
        redrawMiniPreviewOnly();
        if (deskTop)
            deskTop->drawView();
        redraw();
    }

    void reopenEditorFromSession() {
        if (!lastEditorPath.empty()) {
            std::ifstream chk(lastEditorPath);
            if (chk.good()) {
                openFileInEditor(lastEditorPath);
                return;
            }
        }
        openFileInEditor("");
    }

    std::vector<NavigatorListView::NavItem> buildFileManagerItems() const {
        std::vector<NavigatorListView::NavItem> out;
        std::error_code ec;
        fs::path cur(browserDir);
        if (!fs::is_directory(cur, ec))
            return out;

        fs::path par = cur.parent_path();
        if (par != cur) {
            NavigatorListView::NavItem up;
            up.label = "..";
            up.isDirectory = true;
            up.fullPath = absolutePath(par.string());
            out.push_back(up);
        }

        std::vector<std::pair<bool, std::string>> names;
        fs::directory_iterator it(cur, ec), end;
        if (ec)
            return out;
        for (; it != end; it.increment(ec)) {
            if (ec) break;
            const auto &ent = *it;
            std::string name = ent.path().filename().string();
            if (name.empty() || name == "." || name == "..") continue;
            bool isDir = ent.is_directory(ec);
            names.push_back({isDir, name});
        }
        std::sort(names.begin(), names.end(), [](const auto &a, const auto &b) {
            if (a.first != b.first) return a.first > b.first;
            return a.second < b.second;
        });
        for (const auto &p : names) {
            NavigatorListView::NavItem it;
            it.isDirectory = p.first;
            it.fullPath = joinPath(browserDir, p.second);
            it.fullPath = absolutePath(it.fullPath);
            it.label = p.first ? (p.second + "/") : p.second;
            out.push_back(it);
        }
        return out;
    }

    void onCreateFolderInBrowser() {
        std::string name;
        bool createAssets = false;
        if (!promptFolderCreateOptions(name, createAssets)) return;
        name = trim(name);
        if (name.empty() || name == "." || name == "..") return;
        if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return;
        std::string path = joinPath(browserDir, name);
        if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
            messageBox(mfError | mfOKButton, "No se pudo crear la carpeta: %s", strerror(errno));
            return;
        }
        if (createAssets) {
            const std::string assetsPath = joinPath(path, "assets");
            if (mkdir(assetsPath.c_str(), 0755) != 0 && errno != EEXIST) {
                messageBox(mfError | mfOKButton, "No se pudo crear assets/: %s", strerror(errno));
                return;
            }
        }
        browserDir = absolutePath(path);
        syncFilePanelListing();
        saveWorkspaceSession();
    }

    static bool nameEndsWithTxt(const std::string &s) {
        if (s.size() < 4)
            return false;
        return asciiLower(s.substr(s.size() - 4)) == ".txt";
    }

    static std::string shellQuote(const std::string &s) {
        std::string out;
        out.push_back('\'');
        for (char c : s) {
            if (c == '\'')
                out += "'\\''";
            else
                out.push_back(c);
        }
        out.push_back('\'');
        return out;
    }

    std::string renderAsciiWithFontFile(const std::string &text, const std::string &fontFileAbs) const {
        const std::string t = trim(text);
        if (t.empty() || fontFileAbs.empty())
            return {};
        const std::string script = joinPath(projectDir, "tools/render_ascii.py");
        const std::string cmd = "python3 " + shellQuote(script) + " --font-file " + shellQuote(fontFileAbs) +
                                " --text " + shellQuote(t) + " 2>/dev/null";
        FILE *fp = popen(cmd.c_str(), "r");
        if (!fp)
            return t;
        std::string out;
        char buf[4096];
        while (std::fgets(buf, sizeof buf, fp))
            out += buf;
        const int st = pclose(fp);
        if (st != 0 || out.empty())
            return t;
        std::string normalized;
        normalized.reserve(out.size() + 64);
        size_t p = 0;
        while (p <= out.size()) {
            size_t q = out.find('\n', p);
            const bool hasLf = (q != std::string::npos);
            if (!hasLf)
                q = out.size();
            std::string line = out.substr(p, q - p);
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
            normalized += line;
            if (hasLf)
                normalized.push_back('\n');
            if (!hasLf)
                break;
            p = q + 1;
        }
        return normalized;
    }

    std::vector<NavigatorListView::NavItem> asciiFontItems() const {
        std::vector<NavigatorListView::NavItem> out;
        std::error_code ec;
        const fs::path dir = fs::path(projectDir) / "ascii_fonts";
        if (fs::is_directory(dir, ec) && !ec) {
            for (const auto &ent : fs::directory_iterator(dir, ec)) {
                if (ec) break;
                if (!ent.is_regular_file(ec) || ec) continue;
                const std::string ext = asciiLower(ent.path().extension().string());
                if (ext != ".flf" && ext != ".tlf")
                    continue;
                NavigatorListView::NavItem it;
                it.label = ent.path().stem().string();
                it.isDirectory = false;
                it.fullPath = absolutePath(ent.path().string());
                out.push_back(std::move(it));
            }
        }
        std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.label < b.label; });
        return out;
    }

    bool promptTxtTemplate(std::string &outFileName, std::string &outAsciiText, std::string &outFontFile) {
        class AsciiPreviewView : public TView {
        public:
            const std::string *text {nullptr};
            AsciiPreviewView(const TRect &bounds, const std::string *txt) noexcept : TView(bounds), text(txt) {}
            void draw() override {
                TDrawBuffer b;
                const TColorAttr cell = getColor(1);
                const std::string src = text ? *text : std::string{};
                std::vector<std::string> lines;
                size_t p = 0;
                while (p <= src.size()) {
                    size_t q = src.find('\n', p);
                    if (q == std::string::npos) q = src.size();
                    lines.push_back(src.substr(p, q - p));
                    if (q == src.size()) break;
                    p = q + 1;
                }
                for (short y = 0; y < size.y; ++y) {
                    b.moveChar(0, ' ', cell, size.x);
                    if ((size_t)y < lines.size()) {
                        std::string row = lines[(size_t)y];
                        if ((int)row.size() > size.x)
                            row.resize((size_t)size.x);
                        b.moveStr(0, TStringView(row), cell, size.x);
                    }
                    writeLine(0, y, size.x, 1, b);
                }
            }
        };
        class AsciiFontListView : public NavigatorListView {
        public:
            AsciiFontListView(const TRect &bounds, const ushort *themeText, const ushort *themeBack) noexcept :
                NavigatorListView(bounds, themeText, themeBack) {}
            void handleEvent(TEvent &event) override {
                NavItem before {};
                const bool hadBefore = peekCursorItem(before);
                NavigatorListView::handleEvent(event);
                NavItem after {};
                const bool hadAfter = peekCursorItem(after);
                const bool changed = hadBefore != hadAfter ||
                    (hadBefore && hadAfter &&
                     (before.fullPath != after.fullPath || before.label != after.label || before.isDirectory != after.isDirectory));
                if (changed && owner)
                    message(owner, evBroadcast, cmAsciiFontCursorChanged, this);
            }
        };
        class TxtTemplateDialog : public TDialog {
        public:
            TInputLine *fileLine {nullptr};
            TInputLine *asciiLine {nullptr};
            std::string fontFile;
            std::vector<NavigatorListView::NavItem> fonts;
            std::function<std::string(const std::string &, const std::string &)> renderCb;

            TxtTemplateDialog(const TRect &r, const std::vector<NavigatorListView::NavItem> &fontsIn,
                              std::function<std::string(const std::string &, const std::string &)> cb) :
                TWindowInit(&TDialog::initFrame), TDialog(r, "Nuevo archivo TXT") {
                options |= ofCentered;
                palette = dpBlueDialog;
                fonts = fontsIn;
                renderCb = std::move(cb);
                if (!fonts.empty())
                    fontFile = fonts.front().fullPath;
            }

            std::string buildPreviewText() {
                if (!asciiLine)
                    return "(vacio)";
                char buf[MAX_TITLE - 1] = {};
                asciiLine->getData(buf);
                const std::string ascii = trim(std::string(buf));
                if (ascii.empty())
                    return "(vacio)";
                return renderCb ? renderCb(ascii, fontFile) : ascii;
            }

            void openPreviewDialog(const ushort *themeText, const ushort *themeBack) {
                class AsciiPreviewDialog : public TDialog {
                public:
                    AsciiFontListView *list {nullptr};
                    AsciiPreviewView *preview {nullptr};
                    std::string previewText;
                    std::string selectedFont;
                    TInputLine *asciiLine {nullptr};
                    std::function<std::string(const std::string &, const std::string &)> renderCb;

                    AsciiPreviewDialog(const TRect &r) : TWindowInit(&TDialog::initFrame), TDialog(r, "Vista previa ASCII") {
                        options |= ofCentered;
                        palette = dpBlueDialog;
                    }

                    void refreshPreview() {
                        if (!list || !asciiLine)
                            return;
                        NavigatorListView::NavItem it {};
                        if (list->peekCursorItem(it))
                            selectedFont = it.fullPath;
                        char buf[MAX_TITLE - 1] = {};
                        asciiLine->getData(buf);
                        const std::string ascii = trim(std::string(buf));
                        if (ascii.empty())
                            previewText = "(vacio)";
                        else
                            previewText = renderCb ? renderCb(ascii, selectedFont) : ascii;
                        if (preview)
                            preview->drawView();
                        TScreen::flushScreen();
                    }

                    void handleEvent(TEvent &event) override {
                        if (event.what == evBroadcast && event.message.command == cmAsciiFontCursorChanged) {
                            refreshPreview();
                            clearEvent(event);
                            return;
                        }
                        TDialog::handleEvent(event);
                    }
                };

                AsciiPreviewDialog *pv = new AsciiPreviewDialog(TRect(4, 2, 108, 27));
                pv->options |= ofCentered;
                pv->palette = dpBlueDialog;
                pv->asciiLine = asciiLine;
                pv->renderCb = renderCb;
                pv->selectedFont = fontFile;

                /* Arriba: lista (4 visibles). Abajo: preview a todo el ancho. */
                pv->list = new AsciiFontListView(TRect(3, 3, 101, 7), themeText, themeBack);
                pv->list->setItems(fonts);
                pv->insert(pv->list);
                pv->insert(new TLabel(TRect(3, 2, 18, 3), "Fuentes ASCII", pv->list));
                pv->preview = new AsciiPreviewView(TRect(3, 9, 101, 22), &pv->previewText);
                pv->insert(pv->preview);
                pv->insert(new TLabel(TRect(3, 8, 19, 9), "Vista previa", pv->preview));
                pv->insert(new TButton(TRect(62, 23, 80, 25), "Usar fuente", cmOK, bfDefault));
                pv->insert(new TButton(TRect(82, 23, 98, 25), "Cancelar", cmCancel, bfNormal));

                for (size_t i = 0; i < fonts.size(); ++i) {
                    if (fonts[i].fullPath == fontFile) {
                        pv->list->setCursorByLabel(fonts[i].label);
                        break;
                    }
                }
                pv->refreshPreview();
                pv->setCurrent(pv->list, normalSelect);
                const ushort res = TProgram::deskTop->execView(pv);
                if (res == cmOK)
                    fontFile = pv->selectedFont;
                destroy(pv);
            }

            void handleEvent(TEvent &event) override {
                TDialog::handleEvent(event);
                if (event.what == evCommand && event.message.command == cmAsciiOpenPreview) {
                    openPreviewDialog(themeTextRef, themeBackRef);
                    clearEvent(event);
                }
            }

            const ushort *themeTextRef {nullptr};
            const ushort *themeBackRef {nullptr};
        };
#pragma pack(push, 1)
        struct Data {
            char fileName[MAX_TITLE - 1];
            char asciiText[MAX_TITLE - 1];
        } data = {};
#pragma pack(pop)
        outFileName.clear();
        outAsciiText.clear();
        outFontFile.clear();
        const auto fonts = asciiFontItems();
        TxtTemplateDialog *d = new TxtTemplateDialog(
            TRect(14, 5, 94, 12), fonts, [this](const std::string &t, const std::string &f) { return renderAsciiWithFontFile(t, f); });
        d->fileLine = new TInputLine(TRect(3, 3, 34, 4), MAX_TITLE - 2);
        d->insert(d->fileLine);
        d->insert(new TLabel(TRect(3, 2, 30, 3), "Nombre del archivo (.txt)", d->fileLine));
        d->asciiLine = new TInputLine(TRect(36, 3, 76, 4), MAX_TITLE - 2);
        d->insert(d->asciiLine);
        d->insert(new TLabel(TRect(36, 2, 76, 3), "Texto ASCII (opcional)", d->asciiLine));
        if (fonts.empty()) {
            d->insert(new TLabel(TRect(3, 5, 76, 6), "No hay fuentes en ascii_fonts/. El .txt se creara normal.", d->asciiLine));
        } else {
            d->insert(new TButton(TRect(24, 6, 36, 8), "preview", cmAsciiOpenPreview, bfNormal));
        }
        d->insert(new TButton(TRect(38, 6, 50, 8), "Aceptar", cmOK, bfDefault));
        d->insert(new TButton(TRect(52, 6, 64, 8), "Cancelar", cmCancel, bfNormal));
        d->themeTextRef = &textColor;
        d->themeBackRef = &backColor;
        d->setCurrent(d->asciiLine, normalSelect);
        data.fileName[0] = '\0';
        data.asciiText[0] = '\0';
        d->setData(&data);
        ushort res = TProgram::deskTop->execView(d);
        char fileBuf[MAX_TITLE - 1] = {};
        char asciiBuf[MAX_TITLE - 1] = {};
        if (res != cmCancel) {
            if (d->fileLine)
                d->fileLine->getData(fileBuf);
            if (d->asciiLine)
                d->asciiLine->getData(asciiBuf);
        }
        const std::string pickedFont = d->fontFile;
        destroy(d);
        if (res != cmOK)
            return false;
        outFileName = trim(std::string(fileBuf));
        outAsciiText = trim(std::string(asciiBuf));
        outFontFile = pickedFont;
        return !outFileName.empty();
    }

    void onCreateTxtFileInBrowser() {
        std::string name, asciiSeed;
        std::string asciiFontFile;
        if (!promptTxtTemplate(name, asciiSeed, asciiFontFile)) return;
        name = trim(name);
        if (name.empty() || name == "." || name == "..") return;
        if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return;
        if (!nameEndsWithTxt(name))
            name += ".txt";
        const std::string path = joinPath(browserDir, name);
        std::ofstream f(path, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!f) {
            messageBox(mfError | mfOKButton, "No se pudo crear el archivo: %s", strerror(errno));
            return;
        }
        if (!asciiSeed.empty()) {
            const std::string asciiTitle = asciiFontFile.empty() ? asciiSeed : renderAsciiWithFontFile(asciiSeed, asciiFontFile);
            f << asciiTitle << "\n\n";
        }
        f.close();
        const std::string absPath = absolutePath(path);
        openFileInEditor(absPath);
        if (editorWindow && editorWindow->editor) {
            if (auto *re = dynamic_cast<RetroFileEditor *>(editorWindow->editor))
                re->requestInitialViewportReset();
            editorWindow->editor->scrollTo(0, 0);
            editorWindow->editor->drawView();
            TScreen::flushScreen();
        }
        syncFilePanelListing();
        saveWorkspaceSession();
    }

    bool movePathToTrash(const std::string &absPath, std::string &err) {
        err.clear();
        if (absPath.empty()) {
            err = "Ruta vacia.";
            return false;
        }
        const std::string q = shellQuote(absPath);
        auto run = [](const std::string &cmd) -> bool { return std::system(cmd.c_str()) == 0; };
        if (run("gio trash -- " + q + " >/dev/null 2>&1"))
            return true;
        if (run("kioclient5 move " + q + " trash:/ >/dev/null 2>&1"))
            return true;

        const char *home = std::getenv("HOME");
        if (!home || !home[0]) {
            err = "HOME no disponible.";
            return false;
        }
        const fs::path trashDir = fs::path(home) / ".local/share/Trash/files";
        std::error_code ec;
        fs::create_directories(trashDir, ec);
        if (ec) {
            err = "No se pudo preparar Trash/files.";
            return false;
        }
        fs::path src(absPath);
        fs::path dst = trashDir / src.filename();
        if (fs::exists(dst, ec))
            dst = trashDir / (src.stem().string() + "-" + std::to_string((long long)std::time(nullptr)) + src.extension().string());
        ec.clear();
        fs::rename(src, dst, ec);
        if (!ec)
            return true;
        if (run("mv " + q + " " + shellQuote(dst.string()) + " >/dev/null 2>&1"))
            return true;
        err = "No se pudo mover a trash.";
        return false;
    }

    void fileManagerActivate(NavigatorListView *src) {
        if (!src) return;
        NavigatorListView::NavItem it;
        if (!src->peekCursorItem(it)) return;

        if (!it.isDirectory && it.label != "..") {
            std::error_code ec;
            const fs::path fp(it.fullPath);
            if (!fs::is_regular_file(fp, ec) || ec) {
                messageBox(mfError | mfOKButton, "No es un archivo normal o no esta disponible:\n%s", it.fullPath.c_str());
                return;
            }
            constexpr std::uintmax_t kMaxEditorOpenBytes = 64u * 1024u * 1024u;
            if (std::uintmax_t sz = fs::file_size(fp, ec); !ec && sz > kMaxEditorOpenBytes) {
                messageBox(mfError | mfOKButton, "El archivo supera 64 MiB; el editor lo cargaria entero en memoria.");
                return;
            }
            openFileInEditor(it.fullPath);
            if (src->owner) {
                if (TDialog *dlg = dynamic_cast<TDialog *>(src->owner))
                    dlg->endModal(cmOK);
            }
            return;
        }

        browserDir = absolutePath(it.fullPath);

        if (src == navListView) {
            syncFilePanelListing();
        } else {
            src->setItems(buildFileManagerItems());
            if (navListView)
                navListView->setItems(buildFileManagerItems());
        }
        saveWorkspaceSession();
    }


    void showPreferencesDialog() {
        loadAppearancePreferences();
        AppearanceDialog *d = new AppearanceDialog(TRect(2, 2, 88, 20), textColor, backColor,
            desktopPatternChar, desktopPatternUtf8, autoSaveIntervalSec, autoEmDashFromDoubleHyphen);

        ushort res = deskTop->execView(d);
        if (res == cmOK) {
            textColor = d->currentFg();
            backColor = d->currentBg();
            desktopPatternChar = d->currentSymbol();
            desktopPatternUtf8 = d->currentSymbolUtf8();
            autoSaveIntervalSec = d->takeAutoSaveIntervalSec();
            autoEmDashFromDoubleHyphen = d->takeAutoEmDashEnabled();
            saveAppearancePreferences();
            fillPaletteExplicit(textColor, backColor, paletteBytes);
            appPaletteDirty = true;
            applyDesktopPatternChar();
            syncFilePanelListing();
            redrawMiniPreviewOnly();
            relayoutEditorWindow();
            restackDesktopWindowsAboveBackground();
            ensureEditorAboveNavigator();
            if (editorWindow)
                editorWindow->redraw();
            if (editorWindow && editorWindow->editor) {
                if (auto *re = dynamic_cast<RetroFileEditor *>(editorWindow->editor))
                    re->setAutoEmDashFromDoubleHyphen(autoEmDashFromDoubleHyphen);
            }
            if (navWindow)
                navWindow->redraw();
            if (previewWindow)
                previewWindow->redraw();
            if (previewWindow2)
                previewWindow2->redraw();
            if (previewWindow3)
                previewWindow3->redraw();
            startAutoSaveTimer();
            updateMiniPreviewWindowTitles();
            if (deskTop) {
                deskTop->redraw();
                deskTop->drawView();
            }
            redraw();
            TScreen::flushScreen();
        }
        destroy(d);
    }

    bool saveIaConfigToDisk(const rw_ia::Config &cfg, std::string &err) {
        err.clear();
        std::ofstream o(iaWriterCfgPath, std::ios::trunc | std::ios::binary);
        if (!o) {
            err = "No se pudo escribir ia_writer.cfg";
            return false;
        }
        o << "v1\n";
        if (!trim(cfg.endpoint).empty())
            o << "endpoint " << trim(cfg.endpoint) << "\n";
        if (!trim(cfg.endpoint2).empty())
            o << "endpoint2 " << trim(cfg.endpoint2) << "\n";
        if (!trim(cfg.endpoint3).empty())
            o << "endpoint3 " << trim(cfg.endpoint3) << "\n";
        if (!trim(cfg.feedbackEndpoint).empty())
            o << "feedback_endpoint " << trim(cfg.feedbackEndpoint) << "\n";
        if (!trim(cfg.feedbackEndpoint2).empty())
            o << "feedback_endpoint2 " << trim(cfg.feedbackEndpoint2) << "\n";
        if (!trim(cfg.openaiModel).empty())
            o << "openai_model " << trim(cfg.openaiModel) << "\n";
        if (!trim(cfg.openaiApiKey).empty())
            o << "openai_key " << trim(cfg.openaiApiKey) << "\n";
        o << "timeout_sec " << std::max(5, cfg.timeoutSec) << "\n";
        return true;
    }

    void showIaConfigDialog() {
        ensureIaConfig();
        TDialog *d = new TDialog(TRect(4, 2, 98, 26), "Configurar IA");
        d->options |= ofCentered;
        d->palette = dpBlueDialog;

        char endpointBuf[380] = {};
        char endpoint2Buf[380] = {};
        char endpoint3Buf[380] = {};
        char feedbackBuf[380] = {};
        char feedback2Buf[380] = {};
        char modelBuf[64] = {};
        char keyBuf[220] = {};
        char timeoutBuf[16] = {};

        std::snprintf(endpointBuf, sizeof endpointBuf, "%s", iaConfig.endpoint.c_str());
        std::snprintf(endpoint2Buf, sizeof endpoint2Buf, "%s", iaConfig.endpoint2.c_str());
        std::snprintf(endpoint3Buf, sizeof endpoint3Buf, "%s", iaConfig.endpoint3.c_str());
        std::snprintf(feedbackBuf, sizeof feedbackBuf, "%s", iaConfig.feedbackEndpoint.c_str());
        std::snprintf(feedback2Buf, sizeof feedback2Buf, "%s", iaConfig.feedbackEndpoint2.c_str());
        std::snprintf(modelBuf, sizeof modelBuf, "%s", iaConfig.openaiModel.c_str());
        std::snprintf(keyBuf, sizeof keyBuf, "%s", iaConfig.openaiApiKey.c_str());
        std::snprintf(timeoutBuf, sizeof timeoutBuf, "%d", std::max(5, iaConfig.timeoutSec));

        d->insert(new TStaticText(TRect(3, 2, 56, 3), "Endpoint idea #1 (fallback ordenado)"));
        TInputLine *endpointIn = new TInputLine(TRect(3, 3, 92, 4), sizeof endpointBuf - 1);
        endpointIn->setData(endpointBuf);
        d->insert(endpointIn);

        d->insert(new TStaticText(TRect(3, 5, 56, 6), "Endpoint idea #2 (opcional)"));
        TInputLine *endpoint2In = new TInputLine(TRect(3, 6, 92, 7), sizeof endpoint2Buf - 1);
        endpoint2In->setData(endpoint2Buf);
        d->insert(endpoint2In);

        d->insert(new TStaticText(TRect(3, 8, 56, 9), "Endpoint idea #3 (opcional)"));
        TInputLine *endpoint3In = new TInputLine(TRect(3, 9, 92, 10), sizeof endpoint3Buf - 1);
        endpoint3In->setData(endpoint3Buf);
        d->insert(endpoint3In);

        d->insert(new TStaticText(TRect(3, 11, 56, 12), "Endpoint feedback #1"));
        TInputLine *feedbackIn = new TInputLine(TRect(3, 12, 92, 13), sizeof feedbackBuf - 1);
        feedbackIn->setData(feedbackBuf);
        d->insert(feedbackIn);

        d->insert(new TStaticText(TRect(3, 14, 56, 15), "Endpoint feedback #2 (opcional)"));
        TInputLine *feedback2In = new TInputLine(TRect(3, 15, 92, 16), sizeof feedback2Buf - 1);
        feedback2In->setData(feedback2Buf);
        d->insert(feedback2In);

        d->insert(new TStaticText(TRect(3, 17, 30, 18), "Modelo OpenAI"));
        TInputLine *modelIn = new TInputLine(TRect(3, 18, 34, 19), sizeof modelBuf - 1);
        modelIn->setData(modelBuf);
        d->insert(modelIn);

        d->insert(new TStaticText(TRect(36, 17, 58, 18), "Timeout (seg)"));
        TInputLine *timeoutIn = new TInputLine(TRect(36, 18, 52, 19), sizeof timeoutBuf - 1);
        timeoutIn->setData(timeoutBuf);
        d->insert(timeoutIn);

        d->insert(new TStaticText(TRect(3, 20, 30, 21), "OpenAI API key"));
        TInputLine *keyIn = new TInputLine(TRect(3, 21, 92, 22), sizeof keyBuf - 1);
        keyIn->setData(keyBuf);
        d->insert(keyIn);

        d->insert(new TStaticText(TRect(3, 23, 92, 24),
                                  "Orden: endpoint #1 -> #2 -> #3 -> OpenAI. Guardar crea/actualiza ia_writer.cfg."));

        d->insert(new TButton(TRect(58, 17, 74, 19), "Guardar", cmOK, bfDefault));
        d->insert(new TButton(TRect(76, 17, 92, 19), "Cancelar", cmCancel, bfNormal));

        ushort res = deskTop->execView(d);
        if (res == cmOK) {
            endpointIn->getData(endpointBuf);
            endpoint2In->getData(endpoint2Buf);
            endpoint3In->getData(endpoint3Buf);
            feedbackIn->getData(feedbackBuf);
            feedback2In->getData(feedback2Buf);
            modelIn->getData(modelBuf);
            keyIn->getData(keyBuf);
            timeoutIn->getData(timeoutBuf);

            rw_ia::Config next = iaConfig;
            next.endpoint = trim(std::string(endpointBuf));
            next.endpoint2 = trim(std::string(endpoint2Buf));
            next.endpoint3 = trim(std::string(endpoint3Buf));
            next.feedbackEndpoint = trim(std::string(feedbackBuf));
            next.feedbackEndpoint2 = trim(std::string(feedback2Buf));
            next.openaiModel = trim(std::string(modelBuf));
            next.openaiApiKey = trim(std::string(keyBuf));
            next.timeoutSec = std::max(5, std::atoi(trim(std::string(timeoutBuf)).c_str()));

            std::string err;
            if (!saveIaConfigToDisk(next, err))
                messageBox(mfError | mfOKButton, "%s", err.c_str());
            else {
                iaConfig = next;
                iaConfigLoaded = true;
                messageBox(mfInformation | mfOKButton, "Configuracion IA guardada en:\n%s", iaWriterCfgPath.c_str());
            }
        }
        destroy(d);
    }

    void ensureIaConfig() {
        if (iaConfigLoaded)
            return;
        rw_ia::loadConfig(iaWriterCfgPath, iaConfig);
        iaConfigLoaded = true;
    }

    bool appendIaRequestLogJsonl(const std::string &line) {
        std::ofstream o(iaRequestsPath, std::ios::app | std::ios::binary);
        if (!o) return false;
        o << line << "\n";
        return true;
    }

    bool commitIaProyecto(const std::string &baseDirAbs, const std::string &name, const std::string &ideaText, int modeIndex,
                          int deadlineDays,
                         std::string &err) {
        err.clear();
        std::string base = trim(baseDirAbs);
        if (base.empty())
            base = browserDir;
        base = absolutePath(base);
        std::error_code ec;
        if (!fs::is_directory(fs::path(base), ec)) {
            err = "Ubicación no existe o no es carpeta";
            return false;
        }
        std::string n = trim(name);
        if (n.empty() || n == "." || n == "..") {
            err = "Nombre de carpeta invalido";
            return false;
        }
        if (n.find('/') != std::string::npos || n.find('\\') != std::string::npos) {
            err = "Sin separadores en el nombre";
            return false;
        }
        std::string path = joinPath(base, n);
        if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
            err = strerror(errno);
            return false;
        }
        const std::string ap = joinPath(path, "assets");
        if (mkdir(ap.c_str(), 0755) != 0 && errno != EEXIST) {
            err = strerror(errno);
            return false;
        }
        const std::string chapterDir = joinPath(path, "1");
        if (mkdir(chapterDir.c_str(), 0755) != 0 && errno != EEXIST) {
            err = strerror(errno);
            return false;
        }
        const std::string chapterFile = joinPath(chapterDir, "1.txt");
        std::ofstream ch(chapterFile, std::ios::binary | std::ios::app);
        if (!ch) {
            err = "No se pudo crear 1/1.txt";
            return false;
        }
        const std::string premisaPath = joinPath(path, "premisa.txt");
        std::ofstream f(premisaPath, std::ios::binary | std::ios::trunc);
        if (!f) {
            err = "No se pudo crear premisa.txt";
            return false;
        }
        static const char *kModo[] = {"frases", "diez_palabras", "cinco_palabras"};
        const int mi = std::clamp(modeIndex, 0, 2);
        const char *mt = kModo[mi];
        const std::string due = rw_ia::deadlineIsoFromDays(deadlineDays);
        std::time_t tnow = std::time(nullptr);
        struct tm tmb {};
        localtime_r(&tnow, &tmb);
        char cbuf[64];
        std::strftime(cbuf, sizeof cbuf, "%Y-%m-%dT%H:%M:%S", &tmb);

        f << "# RetroWriter - Crear con IA\n# modo " << mt << "\n# creado " << cbuf << "\n# entrega sugerida " << due
          << "\n\n"
          << ideaText << "\n";
        f.close();

        rw_ia::Assignment a;
        a.id = std::to_string((long)tnow) + "_" + std::to_string(std::rand() & 0xFFFFFF);
        a.novelRootAbs = absolutePath(path);
        a.premisaAbs = absolutePath(premisaPath);
        a.createdIso = cbuf;
        a.deadlineIso = due;
        a.estado = "pendiente";
        a.modo = mt;
        if (!rw_ia::appendJsonl(iaRegistryPath, a)) {
            err = "No se pudo escribir ia_escrituras.jsonl";
            return false;
        }
        browserDir = absolutePath(path);
        syncFilePanelListing();
        saveWorkspaceSession();
        return true;
    }

    void showCrearConIADialog() {
        ensureIaConfig();
        CrearConIADialog *d = new CrearConIADialog(TRect(2, 1, 76, 26), this, browserDir, "");
        TView *v = validView(d);
        if (!v) {
            destroy(d);
            return;
        }
        ushort res = deskTop->execView(v);
        ushort modeIdx = 999;
        char baseDirRaw[MAXPATH] = {};
        char folderRaw[MAX_TITLE - 1] = {};
        char ideaCopy[500] = {};
        int iaDaysFromModel = 0;
        if (res == cmOK) {
            auto *cd = static_cast<CrearConIADialog *>(v);
            if (cd->selectedMode >= 0)
                modeIdx = (ushort)cd->selectedMode;
            std::snprintf(baseDirRaw, sizeof baseDirRaw, "%s", cd->baseDirData);
            cd->folderLine->getData(folderRaw);
            std::memcpy(ideaCopy, cd->ideaData, sizeof ideaCopy);
            iaDaysFromModel = cd->iaDeadlineDays;
        }
        destroy(v);
        if (res != cmOK)
            return;
        const std::string baseDir = trim(std::string(baseDirRaw));
        const std::string folderName = trim(std::string(folderRaw));
        const std::string ideaText = trim(std::string(ideaCopy));
        if (modeIdx > 5) {
            messageBox(mfError | mfOKButton, "Primero pulsa Solicitar para obtener una idea IA.");
            return;
        }
        if (baseDir.empty() || folderName.empty() || ideaText.empty()) {
            messageBox(mfError | mfOKButton, "Completa ubicación, carpeta e idea (usa Solicitar si hace falta).");
            return;
        }
        const int ddays =
            (iaDaysFromModel >= 1 && iaDaysFromModel <= 7) ? iaDaysFromModel : rw_ia::randomDeadlineDays();
        std::string err;
        if (!commitIaProyecto(baseDir, folderName, ideaText, (int)modeIdx, ddays, err)) {
            messageBox(mfError | mfOKButton, "%s", err.c_str());
            return;
        }
        char msg[320];
        if (ddays == 1) {
            std::snprintf(msg, sizeof msg,
                          "Listo: carpeta, premisa.txt y registro. Entrega sugerida en 1 día (ver ia_escrituras.jsonl).");
        } else {
            std::snprintf(msg, sizeof msg,
                          "Listo: carpeta, premisa.txt y registro. Entrega sugerida en %d días (ver ia_escrituras.jsonl).",
                          ddays);
        }
        messageBox(mfInformation | mfOKButton, "%s", msg);
    }

    void showEntregarIADialog() {
        ensureIaConfig();
        auto pend = rw_ia::loadPending(iaRegistryPath);
        if (pend.empty()) {
            messageBox(mfInformation | mfOKButton, "No hay escrituras IA pendientes.");
            return;
        }
        const int n = (int)std::min(pend.size(), size_t(12));
        TSItem *chain = nullptr;
        for (int i = n - 1; i >= 0; --i) {
            const std::string lab = std::to_string(i + 1) + ") " + pathTitleForWidth(pend[(size_t)i].novelRootAbs, 42);
            chain = new TSItem(lab.c_str(), chain);
        }
        const short rbBot = (short)(3 + std::max(3, n));
        TDialog *dlg = new TDialog(TRect(2, 2, 76, (short)(rbBot + 6)), "Entregar escritura IA");
        dlg->options |= ofCentered;
        dlg->palette = dpBlueDialog;
        auto *rb = new TRadioButtons(TRect(3, 3, 72, rbBot), chain);
        dlg->insert(rb);
        ushort z = 0;
        rb->setData(&z);
        dlg->insert(new TButton(TRect(3, (short)(rbBot + 1), 26, (short)(rbBot + 3)), "Entregar", cmOK, bfDefault));
        dlg->insert(
            new TButton(TRect(28, (short)(rbBot + 1), 50, (short)(rbBot + 3)), "Comentario IA", cmIaPedirComentario, bfNormal));
        dlg->insert(new TButton(TRect(52, (short)(rbBot + 1), 70, (short)(rbBot + 3)), "Cerrar", cmCancel, bfNormal));
        TView *v2 = validView(dlg);
        if (!v2) {
            destroy(dlg);
            return;
        }
        ushort cmd = deskTop->execView(v2);
        ushort sel = 0;
        if (cmd == cmOK || cmd == cmIaPedirComentario)
            rb->getData(&sel);
        destroy(v2);
        if (cmd == cmCancel)
            return;
        if (sel >= (ushort)n)
            return;
        const rw_ia::Assignment &pick = pend[sel];

        if (cmd == cmIaPedirComentario) {
            ensureIaConfig();
            std::ifstream pf(pick.premisaAbs);
            std::string txt((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>());
            if (txt.empty()) {
                messageBox(mfError | mfOKButton, "premisa.txt vacio o no legible.");
                return;
            }
            const std::string body = std::string("{\"accion\":\"comentario\",\"archivo\":\"") +
                                     rw_ia::jsonEscapeString(pick.premisaAbs) + std::string("\",\"texto\":\"") +
                                     rw_ia::jsonEscapeString(txt) + "\"}";
            std::string reply, err;
            if (!rw_ia::fetchFeedback(iaConfig, body, reply, err)) {
                messageBox(mfError | mfOKButton, "Comentario IA: %s", err.c_str());
                return;
            }
            messageBox(mfInformation | mfOKButton, "%s", reply.c_str());
            return;
        }

        std::time_t tnow = std::time(nullptr);
        struct tm tmb {};
        localtime_r(&tnow, &tmb);
        char nowBuf[64];
        std::strftime(nowBuf, sizeof nowBuf, "%Y-%m-%dT%H:%M:%S", &tmb);
        const bool onTime = (std::string(nowBuf) <= pick.deadlineIso);
        if (!rw_ia::markDelivered(iaRegistryPath, pick.id, onTime)) {
            messageBox(mfError | mfOKButton, "No se pudo actualizar el registro.");
            return;
        }
        messageBox(mfInformation | mfOKButton, onTime ? "Entregada a tiempo." : "Entregada fuera de plazo.");
    }

    void showIaResumenDialog() {
        auto all = rw_ia::loadAll(iaRegistryPath);
        if (all.empty()) {
            messageBox(mfInformation | mfOKButton, "No hay registros en ia_escrituras.jsonl.");
            return;
        }

        auto parseIso = [](const std::string &iso, std::tm &out) -> bool {
            std::memset(&out, 0, sizeof out);
            int Y = 0, M = 0, D = 0, h = 0, m = 0, s = 0;
            if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) != 6)
                return false;
            out.tm_year = Y - 1900;
            out.tm_mon = M - 1;
            out.tm_mday = D;
            out.tm_hour = h;
            out.tm_min = m;
            out.tm_sec = s;
            out.tm_isdst = -1;
            return true;
        };

        auto dayDiffFromNow = [&](const std::string &deadlineIso) -> int {
            std::tm dl {};
            if (!parseIso(deadlineIso, dl))
                return 0;
            const std::time_t tdl = std::mktime(&dl);
            const std::time_t tnow = std::time(nullptr);
            const double diff = std::difftime(tdl, tnow);
            const double absDays = std::fabs(diff) / 86400.0;
            const int days = (int)std::ceil(absDays);
            return diff >= 0 ? days : -days;
        };

        std::vector<std::string> pendientes;
        std::vector<std::string> vencidasPend;
        std::vector<std::string> completasTiempo;
        std::vector<std::string> completasTarde;

        for (const auto &a : all) {
            std::string title = pathTitleForWidth(a.novelRootAbs, 40);
            if (a.estado == "pendiente") {
                const int dd = dayDiffFromNow(a.deadlineIso);
                if (dd >= 0) {
                    if (dd == 0)
                        pendientes.push_back(title + " (vence hoy)");
                    else if (dd == 1)
                        pendientes.push_back(title + " (falta 1 dia)");
                    else
                        pendientes.push_back(title + " (faltan " + std::to_string(dd) + " dias)");
                } else {
                    const int ov = -dd;
                    if (ov == 1)
                        vencidasPend.push_back(title + " (vencida hace 1 dia)");
                    else
                        vencidasPend.push_back(title + " (vencida hace " + std::to_string(ov) + " dias)");
                }
            } else if (a.estado == "entregada") {
                completasTiempo.push_back(title);
            } else if (a.estado == "entregada_tarde") {
                completasTarde.push_back(title);
            }
        }

        auto appendSection = [](std::string &txt, const char *name, const std::vector<std::string> &items) {
            txt += std::string(name) + " (" + std::to_string(items.size()) + ")\n";
            const int showN = std::min((int)items.size(), 8);
            for (int i = 0; i < showN; ++i)
                txt += " - " + items[(size_t)i] + "\n";
            if ((int)items.size() > showN)
                txt += " - ... +" + std::to_string((int)items.size() - showN) + " mas\n";
            txt += "\n";
        };

        std::string report;
        appendSection(report, "Pendientes", pendientes);
        appendSection(report, "Vencidas (aun entregables)", vencidasPend);
        appendSection(report, "Completadas a tiempo", completasTiempo);
        appendSection(report, "Completadas tarde", completasTarde);

        TDialog *d = new TDialog(TRect(2, 1, 98, 29), "Resumen de escrituras IA");
        d->options |= ofCentered;
        d->palette = dpBlueDialog;
        d->insert(new TStaticText(TRect(3, 2, 94, 25), report.c_str()));
        d->insert(new TButton(TRect(40, 25, 58, 27), "Cerrar", cmCancel, bfDefault));
        TView *v = validView(d);
        if (!v) {
            destroy(d);
            return;
        }
        deskTop->execView(v);
        destroy(v);
    }

    void refreshNavigatorWidget() {
#if !defined(_WIN32)
        if (terminalIsKitty())
            deleteKittyMiniPlacementOnly();
#endif
        previewPixelView = nullptr;
        previewPixelView2 = nullptr;
        previewPixelView3 = nullptr;
        if (previewWindow && previewWindow->owner)
            previewWindow->close();
        previewWindow = nullptr;
        if (previewWindow2 && previewWindow2->owner)
            previewWindow2->close();
        previewWindow2 = nullptr;
        if (previewWindow3 && previewWindow3->owner)
            previewWindow3->close();
        previewWindow3 = nullptr;
        if (navWindow && navWindow->owner) navWindow->close();
        navWindow = nullptr;
        navListView = nullptr;

        if (!deskTop)
            return;

        /**
         * Ctrl+E solo oculta el File Manager; la Mini sigue en la columna izquierda.
         * Geometrías en workspace.cfg (navGeom, miniGeom1..3, editorGeom) restauran posiciones.
         */
        const short deskBottom = short(deskTop->getExtent().b.y - 1);
        constexpr short kNavTop = 2;
        constexpr short kMinNavWinBy = 4;
        constexpr short kMinPreviewOuterRows = 3;
        const short minSplitY = short(kNavTop + 1 + kMinNavWinBy);
        const short maxSplitY = short(deskBottom - kMinPreviewOuterRows);
        if (minSplitY > maxSplitY)
            return;

        TRect navR {};
        TRect preR {};
        short splitY = 0;

        if (filePanelVisible) {
            if (loadedNavGeom)
                navR = clampRectToDeskArea(storedNavGeom, 16, 6);
            else {
                splitY = computeDefaultPanelSplitY(deskBottom, minSplitY, maxSplitY);
                if (sessionPanelSplitY > 0)
                    splitY = (short)std::clamp(sessionPanelSplitY, (int)minSplitY, (int)maxSplitY);
                navR = TRect(1, kNavTop, kFilePanelRightX, splitY);
            }
            splitY = navR.b.y;
            preR = TRect(1, splitY, kFilePanelRightX, deskBottom);
        } else {
            preR = TRect(1, kNavTop, kFilePanelRightX, deskBottom);
        }

        if (filePanelVisible) {
            const int navW = navR.b.x - navR.a.x;
            std::string navWinTitle = pathTitleForWidth(browserDir, std::max(1, navW - 10));
            TWindow *nw = new TWindow(navR, navWinTitle.c_str(), wnNoNumber);
            nw->flags &= ~wfZoom;
            nw->flags &= ~wfClose;
            nw->options &= ~ofTopSelect;
            nw->eventMask |= evBroadcast;
            const short winRx = short(navR.b.x - navR.a.x - 1);
            const short winBy = short(navR.b.y - navR.a.y - 1);
            navListView = new MainPanelNavigatorListView(TRect(1, 1, winRx, short(winBy - 2)), &textColor, &backColor);
            navListView->setItems(buildFileManagerItems());
            nw->insert(navListView);
            nw->insert(new FolderPanelFooterStrip(TRect(1, winBy - 2, winRx, winBy)));
            TView *nv = validView(nw);
            if (!nv) {
                navListView = nullptr;
                return;
            }
            deskTop->insert(nv);
            navWindow = nw;
            sessionPanelSplitY = (int)navR.b.y;
        }

        const int preH = int(preR.b.y) - int(preR.a.y);
        const int thirdH = std::max(3, preH / 3);
        auto defaultMiniRect = [&](int slot) -> TRect {
            const short y0 = short(int(preR.a.y) + slot * thirdH);
            const short y1 = short(slot == 2 ? int(preR.b.y) : int(preR.a.y) + (slot + 1) * thirdH);
            return TRect(preR.a.x, y0, preR.b.x, y1);
        };
        auto pickMiniRect = [&](int slot) -> TRect {
            if (loadedMiniGeom[slot])
                return clampRectToDeskArea(storedMiniGeom[slot], 8, 3);
            return clampRectToDeskArea(defaultMiniRect(slot), 8, 3);
        };

        auto makeMiniWindow = [&](int slotIdx, const TRect &outer, std::vector<uint8_t> *rgbBuf, int *iwP, int *ihP,
                                  unsigned kittyId) -> bool {
            const short pRx = short(outer.b.x - outer.a.x - 1);
            const short pBy = short(outer.b.y - outer.a.y - 1);
            TWindow *pw = new TWindow(outer, "Mini", wnNoNumber);
            pw->flags &= ~wfZoom;
            pw->flags &= ~wfClose;
            pw->options &= ~ofTopSelect;
            auto *pv = new PixelPreviewView(TRect(1, 1, pRx, pBy), &pixelCanvas, rgbBuf, iwP, ihP, &textColor, &backColor,
#ifdef HAVE_LIBSIXEL
                                            miniPreviewSixel,
#else
                                            false,
#endif
#if !defined(_WIN32)
                                            miniPreviewKittyNative,
#else
                                            false,
#endif
#ifdef HAVE_LIBSIXEL
                                            miniPreviewSixelColors,
#else
                                            256,
#endif
                                            kittyId);
            pw->insert(pv);
            TView *pvw = validView(pw);
            if (!pvw) {
                return false;
            }
            deskTop->insert(pvw);
            if (slotIdx == 0) {
                previewWindow = pw;
                previewPixelView = pv;
            } else if (slotIdx == 1) {
                previewWindow2 = pw;
                previewPixelView2 = pv;
            } else {
                previewWindow3 = pw;
                previewPixelView3 = pv;
            }
            return true;
        };

        if (!makeMiniWindow(0, pickMiniRect(0), &miniPreviewRgb, &miniPreviewImgW, &miniPreviewImgH,
                            kKittyMiniPreviewImageId)) {
            previewPixelView = nullptr;
            previewWindow = nullptr;
        }
        if (!makeMiniWindow(1, pickMiniRect(1), &miniPreviewRgb2, &miniPreviewImgW2, &miniPreviewImgH2,
                            kKittyMiniPreviewImageId2)) {
            previewPixelView2 = nullptr;
            previewWindow2 = nullptr;
        }
        if (!makeMiniWindow(2, pickMiniRect(2), &miniPreviewRgb3, &miniPreviewImgW3, &miniPreviewImgH3,
                            kKittyMiniPreviewImageId3)) {
            previewPixelView3 = nullptr;
            previewWindow3 = nullptr;
        }

        if (navListView && navWindow)
            navWindow->setCurrent(navListView, normalSelect);
        restackDesktopWindowsAboveBackground();
        ensureEditorAboveNavigator();
        updateMiniPreviewWindowTitles();
    }

    /** El editor debe dibujarse encima del panel/Mini para que no se vea sombra/recorte raro. */
    void ensureEditorAboveNavigator() {
        if (!deskTop || !editorWindow || editorWindow->owner != deskTop)
            return;
        TView *under = nullptr;
        if (filePanelVisible && navWindow && navWindow->owner == deskTop) {
            if (previewWindow && previewWindow->owner == deskTop)
                navWindow->putInFrontOf(previewWindow);
            if (previewWindow2 && previewWindow2->owner == deskTop)
                navWindow->putInFrontOf(previewWindow2);
            if (previewWindow3 && previewWindow3->owner == deskTop)
                navWindow->putInFrontOf(previewWindow3);
            under = navWindow;
        } else if (previewWindow && previewWindow->owner == deskTop) {
            under = previewWindow;
            if (previewWindow2 && previewWindow2->owner == deskTop) {
                previewWindow2->putInFrontOf(under);
                under = previewWindow2;
            }
            if (previewWindow3 && previewWindow3->owner == deskTop) {
                previewWindow3->putInFrontOf(under);
                under = previewWindow3;
            }
        } else if (previewWindow2 && previewWindow2->owner == deskTop) {
            under = previewWindow2;
            if (previewWindow3 && previewWindow3->owner == deskTop) {
                previewWindow3->putInFrontOf(under);
                under = previewWindow3;
            }
        } else if (previewWindow3 && previewWindow3->owner == deskTop) {
            under = previewWindow3;
        }
        TView *prev = deskTop->current;
        if (under)
            editorWindow->putInFrontOf(under);
        if (prev == navWindow || prev == previewWindow || prev == previewWindow2 || prev == previewWindow3)
            deskTop->setCurrent(prev, normalSelect);
        else
            deskTop->setCurrent(editorWindow, normalSelect);
    }

    void createDesktopWidgets() {
        reloadMiniPreviewImage();
        refreshNavigatorWidget();
    }

    void destroyWidgetWindows() {
#if !defined(_WIN32)
        if (terminalIsKitty())
            deleteKittyMiniPlacementOnly();
#endif
        previewPixelView = nullptr;
        previewPixelView2 = nullptr;
        previewPixelView3 = nullptr;
        if (previewWindow && previewWindow->owner)
            previewWindow->close();
        previewWindow = nullptr;
        if (previewWindow2 && previewWindow2->owner)
            previewWindow2->close();
        previewWindow2 = nullptr;
        if (previewWindow3 && previewWindow3->owner)
            previewWindow3->close();
        previewWindow3 = nullptr;
        if (navWindow && navWindow->owner) navWindow->close();
        navWindow = nullptr;
        navListView = nullptr;
    }
};

std::string RetroWriterTVApp::browseImageFileAbs(const std::string &startDirHint) {
    if (!deskTop)
        return {};
    /* Pickers de imagen reutilizan el mismo orden del navegador principal: "..", carpetas, archivos. */
    auto resolveStartDir = [&]() -> std::string {
        std::error_code ec;
        if (!trim(startDirHint).empty()) {
            fs::path p(startDirHint);
            if (fs::is_directory(p, ec) && !ec)
                return absolutePath(p.string());
        }
        if (const char *h = std::getenv("HOME")) {
            fs::path p(h);
            ec.clear();
            if (fs::is_directory(p, ec) && !ec)
                return absolutePath(p.string());
        }
        return absolutePath(projectDir);
    };

    auto buildItemsForDir = [&](const std::string &dirAbs) -> std::vector<NavigatorListView::NavItem> {
        std::vector<NavigatorListView::NavItem> out;
        std::error_code ec;
        fs::path cur(dirAbs);
        if (!fs::is_directory(cur, ec) || ec)
            return out;
        fs::path par = cur.parent_path();
        if (par != cur) {
            NavigatorListView::NavItem up;
            up.label = "..";
            up.isDirectory = true;
            up.fullPath = absolutePath(par.string());
            out.push_back(up);
        }
        std::vector<std::pair<bool, std::string>> names;
        fs::directory_iterator it(cur, ec), end;
        if (ec)
            return out;
        for (; it != end; it.increment(ec)) {
            if (ec)
                break;
            const auto &ent = *it;
            std::string name = ent.path().filename().string();
            if (name.empty() || name == "." || name == "..")
                continue;
            bool isDir = ent.is_directory(ec);
            names.push_back({isDir, name});
        }
        std::sort(names.begin(), names.end(), [](const auto &a, const auto &b) {
            if (a.first != b.first)
                return a.first > b.first;
            return a.second < b.second;
        });
        for (const auto &p : names) {
            NavigatorListView::NavItem it;
            it.isDirectory = p.first;
            it.fullPath = absolutePath(joinPath(dirAbs, p.second));
            it.label = p.first ? (p.second + "/") : p.second;
            out.push_back(it);
        }
        return out;
    };

    std::string dirAbs = resolveStartDir();
    std::string keepLabel;
    for (;;) {
        std::string selectedPreviewPath;
        std::string selectedPath;
        char pathBuf[kMaxPreviewImagePath + 1] {};
        std::strncpy(pathBuf, dirAbs.c_str(), kMaxPreviewImagePath);

        ImageNavPickerDialog *d = new ImageNavPickerDialog(TRect(4, 2, 94, 27), "Explorador de imagen");
        d->options |= ofCentered;
        d->setState(sfShadow, False);
        d->insert(new TStaticText(TRect(2, 2, 16, 3), "Ruta:"));

        TInputLine *pathLine = new TInputLine(TRect(8, 2, 96, 3), kMaxPreviewImagePath);
        pathLine->setData(static_cast<void *>(pathBuf));
        d->insert(pathLine);

        ImageNavPickerListView *lv = new ImageNavPickerListView(TRect(2, 4, 33, 21), &textColor, &backColor);
        lv->setItems(buildItemsForDir(dirAbs));
        if (!keepLabel.empty())
            lv->setCursorByLabel(keepLabel);
        d->insert(lv);

        LibraryImagePreview *pv = new LibraryImagePreview(TRect(35, 4, 72, 20), &selectedPreviewPath, &dirAbs,
                                                          &visualLibrary.assetRootSubpath, &textColor, &backColor,
#ifdef HAVE_LIBSIXEL
                                                          miniPreviewSixel,
#else
                                                          false,
#endif
#if !defined(_WIN32)
                                                          miniPreviewKittyNative,
#else
                                                          false,
#endif
#ifdef HAVE_LIBSIXEL
                                                          miniPreviewSixelColors,
#else
                                                          256,
#endif
                                                          kKittyImagePickDialogId, 0);
        d->insert(pv);
        PathStripView *strip = new PathStripView(TRect(2, 21, 72, 22), &selectedPath, nullptr);
        d->insert(strip);
        TScrollBar *hs = new TScrollBar(TRect(2, 22, 72, 23));
        d->insert(hs);

        d->bind(lv, pathLine, pathBuf, &selectedPath, strip, hs, &selectedPreviewPath, pv);
        d->refreshFromSelection();

        d->insert(new CleanButton(TRect(74, 18, 88, 20), "Agregar", cmOK, true));
        d->insert(new CleanButton(TRect(74, 21, 88, 23), "Cancelar", cmCancel, false));
        d->setCurrent(lv, normalSelect);

        const ushort res = deskTop->execView(d);
        NavigatorListView::NavItem picked {};
        const bool hasPick = lv->peekCursorItem(picked);
        destroy(d);

        if (res == cmCancel)
            return {};
        if (res != cmOK || !hasPick)
            continue;
        if (picked.isDirectory || picked.label == "..") {
            dirAbs = absolutePath(picked.fullPath);
            keepLabel.clear();
            continue;
        }
        if (!rwPathLooksLikeRasterImage(picked.fullPath)) {
            messageBox(mfError | mfOKButton, "Selecciona una imagen valida (png/jpg/webp/bmp/gif/tga).");
            keepLabel = picked.label;
            continue;
        }
        std::error_code ec;
        if (!fs::is_regular_file(fs::path(picked.fullPath), ec) || ec) {
            messageBox(mfError | mfOKButton, "No se pudo abrir la ruta seleccionada.");
            keepLabel = picked.label;
            continue;
        }
        return picked.fullPath;
    }
}

}

/** Directorio del proyecto: argv[1], o "." / ".." según dónde se encuentre appearance.cfg. */
static std::string resolveProjectDirectory(int argc, char **argv) {
    if (argc >= 2)
        return std::string(argv[1]);
    std::error_code ec;
    if (fs::is_regular_file(fs::path("appearance.cfg"), ec))
        return ".";
    if (fs::is_regular_file(fs::path("..") / "appearance.cfg", ec))
        return "..";
    return ".";
}

int main(int argc, char **argv) {
    std::string projectDir = resolveProjectDirectory(argc, argv);
    std::error_code ec;
    if (!fs::is_directory(fs::path(projectDir), ec)) {
        std::fprintf(stderr, "retro_writer_tv: la carpeta de proyecto no existe: %s\n", projectDir.c_str());
        return 1;
    }
    RetroWriterTVApp app(projectDir);
    app.run();
    app.shutDown();
    return 0;
}
