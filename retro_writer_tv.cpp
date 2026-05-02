#define Uses_TApplication
#define Uses_TProgram
#define Uses_TBackground
#define Uses_TButton
#define Uses_TColorSelector
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TDrawBuffer
#define Uses_TEditWindow
#define Uses_TEvent
#define Uses_TInputLine
#define Uses_TKeys
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
#define Uses_MsgBox
#include <tvision/tv.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <climits>
#include <vector>

namespace fs = std::filesystem;

extern TColorAttr shadowAttr;

namespace {

const int cmNewNovel = 1001;
const int cmNewChapter = 1002;
const int cmWelcome = 1003;
const int cmPrevNovel = 1004;
const int cmNextNovel = 1005;
const int cmPrevChapter = 1006;
const int cmNextChapter = 1007;
const int cmNavigator = 1008;
const int cmRefreshWidgets = 1009;
const int cmPreferences = 1010;
const int cmFgMatrixChanged = 1011;
const int cmBgMatrixChanged = 1012;
const int cmSymbolMatrixChanged = 1013;
const int cmNavSelect = 1014;
const int cmToggleFilePanel = 1016;
const int cmCreateFolder = 1015;
const int cmCreateTxtFile = 1017;

/** Borde derecho exclusivo del panel de archivos expandido (editor desde 34). */
static const short kFilePanelRightX = 33;

const int MAX_TITLE = 96;

struct EntryMeta {
    std::string key;
    std::string title;
};

static bool ensureDir(const std::string &path) {
    if (mkdir(path.c_str(), 0755) == 0) return true;
    return errno == EEXIST;
}

static std::string joinPath(const std::string &a, const std::string &b) {
    if (!a.empty() && (a.back() == '/' || a.back() == '\\')) return a + b;
    return a + "/" + b;
}

/** Título corto: ruta completa o ".../" + cola (columnas monoespacio, p. ej. ancho del marco). */
static std::string pathTitleForWidth(const std::string &path, int maxCols) {
    if (maxCols < 1) maxCols = 1;
    if ((int)path.size() <= maxCols) return path;
    static const char ell[] = ".../";
    const int ellLen = 4;
    int tail = maxCols - ellLen;
    if (tail < 1)
        return path.substr(path.size() - (size_t)std::min((int)path.size(), maxCols));
    return std::string(ell) + path.substr(path.size() - (size_t)tail);
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

static std::string slugify(const std::string &s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c)) out.push_back((char)std::tolower(c));
        else if (c == ' ' || c == '-' || c == '_') out.push_back('_');
    }
    if (out.empty()) out = "novela";
    return out;
}

/** Longitud de cpAppColor / cpAppBlackWhite en Turbo Vision (sin '\\0'). */
static constexpr ushort kAppPaletteLen = sizeof(cpAppColor) - 1;

/** Construye paleta explicita manteniendo contraste entre controles. */
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
        baseFg = uchar((baseFg + 8) & 0x0F); // evita texto/fondo idéntico.

    // Recolor limpio: mantenemos la estructura original de cpAppColor
    // (inversiones, brillos y jerarquia), solo la "teñimos" con fg/bg elegidos.
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

    // IMPORTANTE:
    // Conservamos intactas las 3 paletas de dialogo (32..127, 1-indexed)
    // para que TDialog + dpBlueDialog mantenga el estilo clasico de botones.
    // Si estas entradas se "tiñen" con fg/bg global, los botones salen rojos/raros.
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

class ColorPreviewView : public TView {
public:
    ColorPreviewView(const TRect &bounds, ushort fg, ushort bg, char pat) noexcept :
        TView(bounds), fgColor(fg), bgColor(bg), pattern(pat) {}

    void setColors(ushort fg, ushort bg) {
        fgColor = fg;
        bgColor = bg;
        drawView();
    }

    void setPattern(char pat) {
        pattern = pat;
        drawView();
    }

    virtual void draw() {
        TDrawBuffer b;
        TColorAttr attr(
            TColorDesired(TColorXTerm(uchar(fgColor & 0xFF))),
            TColorDesired(TColorXTerm(uchar(bgColor & 0xFF)))
        );
        b.moveChar(0, ' ', attr, size.x);
        b.moveStr(1, "Vista previa de color", attr);
        writeLine(0, 0, size.x, 1, b);
        b.moveChar(0, ' ', attr, size.x);
        b.moveStr(1, "Texto AaBb0123", attr);
        writeLine(0, 1, size.x, 1, b);
        b.moveChar(0, pattern, attr, size.x);
        b.moveStr(1, "Simbolo fondo", attr);
        writeLine(0, 2, size.x, 1, b);
    }

private:
    ushort fgColor;
    ushort bgColor;
    char pattern;
};

class CleanButton : public TView {
public:
    CleanButton(const TRect &bounds, const char *text, ushort cmd, bool isDefault = false) noexcept :
        TView(bounds), title(text ? text : ""), command(cmd), amDefault(isDefault) {
        options |= ofSelectable | ofFirstClick;
        eventMask |= evMouseDown | evKeyDown | evBroadcast;
    }

    virtual void draw() override {
        // Replica TButton::drawState (!down, showMarkers false) para que bordes,
        // glifos (\xDC/\xDB/\xDF) y centrado del titulo coincidan pixel a pixel.
        TDrawBuffer b;
        const bool isDisabled = (state & sfDisabled) != 0;
        const bool isFocused = (state & sfFocused) != 0;
        TAttrPair facePair = isDisabled ? getColor(4) : (isFocused ? getColor(2) : getColor(1));
        TColorAttr face(facePair);
        setStyle(face, ushort(getStyle(face) | slBold));
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
        // Igual que TButton: mapea a entradas de boton del owner/dialogo.
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

using WelcomeActionButton = CleanButton;

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

static bool navLabelMatchesQuick(const std::string &label, const std::string &qLower) {
    if (qLower.empty())
        return true;
    if (label == "..")
        return true;
    return asciiLower(navLabelStem(label)).find(qLower) != std::string::npos;
}

/**
 * Pie del panel: linea separadora + fila con "[ Crear carpeta ]" y "[ .txt ]".
 */
class FolderPanelFooterStrip : public TView {
public:
    FolderPanelFooterStrip(const TRect &bounds) noexcept : TView(bounds) {
        options |= ofFirstClick;
        eventMask |= evMouseDown;
        growMode = gfGrowHiX;
    }

    virtual TPalette &getPalette() const override {
        static TPalette palette("\x02\x03\x04\x05\x06\x07", 6);
        return palette;
    }

    virtual void draw() override {
        TDrawBuffer b;
        TAttrPair cNormal = getColor(0x0301);
        TAttrPair cSelect = getColor(0x0604);
        TAttrPair cDim = getColor(0x0202);

        b.moveChar(0, '\xC4', TColorAttr(cDim), size.x);
        writeLine(0, 0, size.x, 1, b);

        b.moveChar(0, ' ', cNormal, size.x);
        TAttrPair color = ((state & sfFocused) != 0) ? cSelect : cNormal;
        short xf = 0, wf = 0, xt = 0, wt = 0;
        twoButtonLayout(xf, wf, xt, wt);
        if (wf > 0 && xf >= 0 && xf < size.x)
            b.moveStr(ushort(xf), TStringView(kFolderLabel), TColorAttr(color), ushort(size.x - xf));
        if (wt > 0 && xt >= 0 && xt < size.x)
            b.moveStr(ushort(xt), TStringView(kTxtLabel), TColorAttr(color), ushort(size.x - xt));
        writeLine(0, 1, size.x, 1, b);
    }

    virtual void handleEvent(TEvent &event) override {
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

private:
    static constexpr const char kFolderLabel[] = "[ Crear carpeta ]";
    static constexpr const char kTxtLabel[] = "[ .txt ]";

    void twoButtonLayout(short &outXF, short &outWF, short &outXT, short &outWT) const {
        outWF = short(sizeof(kFolderLabel) - 1);
        outWT = short(sizeof(kTxtLabel) - 1);
        const int gap = 2;
        const int total = int(outWF) + gap + int(outWT);
        int start = (int(size.x) - total) / 2;
        if (start < 0)
            start = 0;
        outXF = short(start);
        outXT = short(start + int(outWF) + gap);
    }

    bool hitFolder(int px) const {
        short xf = 0, wf = 0, xt = 0, wt = 0;
        twoButtonLayout(xf, wf, xt, wt);
        return px >= int(xf) && px < int(xf) + int(wf);
    }

    bool hitTxt(int px) const {
        short xf = 0, wf = 0, xt = 0, wt = 0;
        twoButtonLayout(xf, wf, xt, wt);
        return px >= int(xt) && px < int(xt) + int(wt);
    }
};

/** Interactive list-based navigator with keyboard navigation. */
class NavigatorListView : public TView {
public:
    struct NavItem {
        std::string label;     // "..", "carpeta/", "archivo.txt"
        bool isDirectory;
        std::string fullPath;  // ruta absoluta o canonica para abrir/navegar
    };

    NavigatorListView(const TRect &bounds, const ushort *themeText, const ushort *themeBack) noexcept :
        TView(bounds), themeTextColor(themeText), themeBackColor(themeBack) {
        options |= ofSelectable | ofFirstClick;
        eventMask |= evMouseDown | evKeyDown;
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
            TColorAttr attr = attrNormal;
            if (idx >= 0 && idx < (int)items.size() && idx == cursor) {
                if ((state & sfFocused) != 0)
                    attr = attrSel;
                setStyle(attr, ushort(getStyle(attr) | slUnderline));
            }
            b.moveChar(0, ' ', attr, size.x);
            if (idx >= 0 && idx < (int)items.size()) {
                const NavItem &it = items[idx];
                std::string line = treePrefix(idx) + it.label;
                b.moveStr(0, line.c_str(), attr, size.x);
            }
            writeLine(0, y, size.x, 1, b);
        }
    }

    virtual void handleEvent(TEvent &event) {
        TView::handleEvent(event);
        if (event.what == evMouseDown) {
            if (owner)
                owner->setCurrent(this, normalSelect);
            // meDoubleClick debe leerse ANTES de mouseEvent: el bucle reutiliza `event` y pierde el flag.
            const bool doubleClick = (event.mouse.eventFlags & meDoubleClick) != 0;
            TPoint p0 = makeLocal(event.mouse.where);
            if (filterRows() != 0 && p0.y == 0) {
                quickFilter.clear();
                applyQuickFilter();
                drawView();
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
            if (doubleClick && TProgram::application)
                message(TProgram::application, evBroadcast, cmNavSelect, this);
            clearEvent(event);
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
                        if (TProgram::application)
                            message(TProgram::application, evBroadcast, cmNavSelect, this);
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
        // Un solo candidato distinto de "..": seleccionarlo para Enter sin mover flechas.
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

    /** Lista plana de explorador: sin prefijos de arbol. */
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
            return selected; // indice xterm directo (0..255)
        return selected;
    }

    static char symbolToPatternChar(const char *s) {
        if (!s || !s[0])
            return '\xb0';
        // El patron de TBackground es 1 byte: si es UTF-8 multibyte,
        // usamos un fallback estable para no corromper el fondo.
        if (s[1] == '\0')
            return s[0];
        return '\xb0';
    }

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
                    // Celda de 2 columnas (como selector TV clasico).
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
            // Indicadores visuales de desplazamiento.
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

class AppearanceDialog : public TDialog {
public:
    AppearanceDialog(TRect bounds, ushort fg, ushort bg, char pat, const std::string &patUtf8) :
        TWindowInit(&TDialog::initFrame),
        TDialog(bounds, "Personalizar apariencia") {
        options |= ofCentered;
        eventMask |= evBroadcast;
        palette = dpBlueDialog;
        // Paleta xterm completa (0..255) con scroll.
        fgSel = new MatrixSelectorView(TRect(3, 4, 21, 16), MatrixSelectorView::mkForeground, nullptr, 256, 9, cmFgMatrixChanged);
        bgSel = new MatrixSelectorView(TRect(23, 4, 41, 16), MatrixSelectorView::mkBackground, nullptr, 256, 9, cmBgMatrixChanged);
        symbolOptions = buildSymbolOptions();
        // Simbolos unicode con scroll.
        symSel = new MatrixSelectorView(TRect(43, 4, 55, 16), MatrixSelectorView::mkSymbol, &symbolOptions, ushort(symbolOptions.size()), 4, cmSymbolMatrixChanged);
        preview = new ColorPreviewView(TRect(57, 4, 76, 8), fg, bg, pat);
        fgSel->setSelected(ushort(fg & 0xFF));
        bgSel->setSelected(ushort(bg & 0xFF));
        ushort symIdx = findSymbolIndexUtf8(patUtf8);
        if (symIdx == 0 && !patUtf8.empty() && symbolOptions[0] != patUtf8)
            symIdx = findSymbolIndex(pat);
        symSel->setSelected(symIdx);
        insert(new TStaticText(TRect(3, 3, 21, 4), "Foreground 0..255"));
        insert(new TStaticText(TRect(23, 3, 41, 4), "Background 0..255"));
        insert(new TStaticText(TRect(43, 3, 55, 4), "Simbolos"));
        insert(new TStaticText(TRect(57, 3, 70, 4), "Preview"));
        insert(fgSel);
        insert(bgSel);
        insert(symSel);
        insert(preview);
        insert(new TButton(TRect(52, 15, 64, 17), "Aplicar", cmOK, bfDefault));
        insert(new TButton(TRect(65, 15, 77, 17), "Cancelar", cmCancel, bfNormal));
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
                preview->setPattern(currentSymbol());
                clearEvent(event);
            }
        }
    }

    MatrixSelectorView *fgSel {nullptr};
    MatrixSelectorView *bgSel {nullptr};
    MatrixSelectorView *symSel {nullptr};
    ColorPreviewView *preview {nullptr};
    std::vector<std::string> symbolOptions;

    static std::vector<std::string> buildSymbolOptions() {
        static const char *raw = u8R"(★ ☆ ✡ ✦ ✧ ✩ ✪ ✫ ✬ ✭ ✮ ✯ ✰ ⁂ ⁎ ⁑ ✢ ✣ ✤ ✥ ✱ ✲ ✳ ✴ ✵ ✶ ✷ ✸ ✹ ✺ ✻ ✼ ✽ ✾ ✿ ❀ ❁ ❂ ❃ ❇ ❈ ❉ ❊ ❋ ❄ ❆ ❅ ⋆ ≛ ᕯ ✲ ࿏ ꙰ ۞ ⭒ ⍟
♔ ♕ ♖ ♗ ♘ ♙ ♚ ♛ ♜ ♝ ♞ ♟ ♤ ♠ ♧ ♣ ♡ ♥ ♢ ♦ ♩ ♬ ♫ ♪ ♩ · ‑ ‒ – — ― ‗ ‘ ’ ‚ ‛ “ ” „ ‟ • ‣ ․ ‥ … ‧ ′ ″ ‴ ‵ ‶ ‷ ❛ ❜ ❝ ❞ ʹ ʺ ʻ ʼ ʽ ʾ ʿ ˀ ˁ ˂ ˃ ˄ ˅ ˆ ˇ ˈ ˉ ˊ ˋ ˌ ˍ ˎ ˏ ː ˑ ˒ ˓ ˔ ˕ ˖ ˗ ˘ ˙ ˚ ˛ ˜ ˝ ˠ ⋮ ⋯ ⋰ ⋱ ⁺ ⁻ ⁼ ⁽ ⁾ ⁿ ₊ ₋ ₌ ₍ ₎ ✖ ﹢ ﹣ ＋ － ／ ＝ ÷ ± × ❏ ❐ ❑ ❒ ▀ ▁ ▂ ▃ ▄ ▅ ▆ ▇ ▉ ▊ ▋ █ ▌ ▐ ▍ ▎ ▏ ▕ ░ ▒ ▓ ▔ ▬ ▢ ▣ ▤ ▥ ▦ ▧ ▨ ▩ ▪ ▫ ▭ ▮ ▯ ☰ ☲ ☱ ☴ ☵ ☶ ☳ ☷ ▰ ▱ ◧ ◨ ◩ ◪ ◫ ∎ ■ □ ⊞ ⊟ ⊠ ⊡ ❘ ❙ ❚ 〓 ◊ ◈ ◇ ◆ ⎔ ⎚ ☖ ☗ ◄ ▲ ▼ ► ◀ ◣ ◥ ◤ ◢ ▶ ◂ ▴ ▾ ▸ ◁ △ ▽ ▷ ∆ ∇ ⊳ ⊲ ⊴ ⊵ ◅ ▻ ▵ ▿ ◃ ▹ ◭ ◮ ⫷ ⫸ ⋖ ⋗ ⋪ ⋫ ⋬ ⋭ ⊿ ◬ ≜ ⑅ │ ┃ ╽ ╿ ╏ ║ ╎ ┇ ︱ ┊ ︳ ┋ ┆ ╵ 〡 〢 ╹ ╻ ╷ 〣 ☰ ☱ ☲ ☳ ☴ ☵ ☶ ☷ ≡ ✕ ═ ━ ─ ╍ ┅ ┉ ┄ ┈ ╌ ╴ ╶ ╸ ╺ ╼ ╾ ﹉ ﹍ ﹊ ﹎ ︲ ⑆ ⑇ ⑈ ⑉ ⑊ ⑄ ⑀ ︴ ﹏ ﹌ ﹋ ╳ ╲ ╱ ︶ ︵ 〵 〴 〳 〆 ` ᐟ ‐ ⁃ ⎯ 〄 ◉ ○ ◌ ◍ ◎ ● ◐ ◑ ◒ ◓ ◔ ◕ ◖ ◗ ❂ ☢ ⊗ ⊙ ◘ ◙ ◚ ◛ ◜ ◝ ◞ ◟ ◠ ◡ ◯ 〇 〶 ⚫ ⬤ ◦ ∅ ∘ ⊕ ⊖ ⊘ ⊚ ⊛ ⊜ ⊝ ❍ ⦿ ☁ ☀ ☂ ☃ ☾ ☽ ☼ ❄ ❅ ❆ ✙ ✙ ☯ ☢ ☠ ✞ ☤ ☮ ✢ ♥ ♡ ❥ ❣ ❦ ❧ ও ⚣ ☹ ☺ ☻ ㋡ シ ッ ㋛ ⊞ ⊟ ⊠ ⊡)";

        std::vector<std::string> out;
        std::string s(raw);
        for (size_t i = 0; i < s.size();) {
            unsigned char c = (unsigned char)s[i];
            if (c <= 0x20) { ++i; continue; } // separadores
            size_t len = 1;
            if ((c & 0xE0) == 0xC0) len = 2;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xF8) == 0xF0) len = 4;
            if (i + len > s.size()) break;
            out.push_back(s.substr(i, len));
            i += len;
        }
        // fallback clasico al inicio para compatibilidad segura.
        out.insert(out.begin(), {std::string("\xb0"), std::string("\xb1"), std::string("\xb2"), std::string("\xdb"), std::string(" "), std::string("."), std::string("#")});
        return out;
    }
};

static size_t utf8GlyphByteLen(const char *p, size_t maxRemain) noexcept {
    if (maxRemain == 0 || !p || !p[0]) return 0;
    unsigned char c = (unsigned char)*p;
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return maxRemain >= 2 ? 2 : 1;
    if ((c & 0xF0) == 0xE0) return maxRemain >= 3 ? 3 : 1;
    if ((c & 0xF8) == 0xF0) return maxRemain >= 4 ? 4 : 1;
    return 1;
}

/** Decodifica un codepoint UTF-8 ya segmentado con utf8GlyphByteLen. */
static uint32_t utf8CodepointN(const char *p, size_t step) noexcept {
    if (step == 0 || !p) return 0;
    const unsigned char c0 = (unsigned char)p[0];
    if (step == 1) return c0;
    if (step == 2 && (c0 & 0xE0u) == 0xC0u)
        return (uint32_t(c0 & 0x1Fu) << 6) | (uint32_t((unsigned char)p[1]) & 0x3Fu);
    if (step == 3 && (c0 & 0xF0u) == 0xE0u)
        return (uint32_t(c0 & 0x0Fu) << 12) | ((uint32_t((unsigned char)p[1]) & 0x3Fu) << 6) |
               (uint32_t((unsigned char)p[2]) & 0x3Fu);
    if (step == 4 && (c0 & 0xF8u) == 0xF0u)
        return (uint32_t(c0 & 0x07u) << 18) | ((uint32_t((unsigned char)p[1]) & 0x3Fu) << 12) |
               ((uint32_t((unsigned char)p[2]) & 0x3Fu) << 6) | (uint32_t((unsigned char)p[3]) & 0x3Fu);
    return 0;
}

/** Trazos dobles del bloque Box Drawing (═ ║ ╔ ╗ ╚ ╝ …), no el bloque sólido █. */
static bool isDoubleStrokeBoxDrawing(uint32_t cp) noexcept {
    return cp >= 0x2550u && cp <= 0x256Cu;
}

static std::vector<std::string> welcomeBannerLines() {
    return {
        u8R"BW(██████╗ ███████╗████████╗██████╗  ██████╗     )BW",
        u8R"BW(██╔══██╗██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗    )BW",
        u8R"BW(██████╔╝█████╗     ██║   ██████╔╝██║   ██║    )BW",
        u8R"BW(██╔══██╗██╔══╝     ██║   ██╔══██╗██║   ██║    )BW",
        u8R"BW(██║  ██║███████╗   ██║   ██║  ██║╚██████╔╝    )BW",
        u8R"BW(╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝     )BW",
        u8R"BW(                                              )BW",
        u8R"BW(██╗    ██╗██████╗ ██╗████████╗███████╗██████╗ )BW",
        u8R"BW(██║    ██║██╔══██╗██║╚══██╔══╝██╔════╝██╔══██╗)BW",
        u8R"BW(██║ █╗ ██║██████╔╝██║   ██║   █████╗  ██████╔╝)BW",
        u8R"BW(██║███╗██║██╔══██╗██║   ██║   ██╔══╝  ██╔══██╗)BW",
        u8R"BW(╚███╔███╔╝██║  ██║██║   ██║   ███████╗██║  ██║)BW",
        u8R"BW( ╚══╝╚══╝ ╚═╝  ╚═╝╚═╝   ╚═╝   ╚══════╝╚═╝  ╚═╝)BW",
        u8R"BW(                                              )BW",
    };
}

class RainbowBannerView : public TView {
public:
    RainbowBannerView(const TRect &bounds, std::vector<std::string> rows, const ushort *bgCol,
                      const ushort *fgCol) noexcept :
        TView(bounds), lines(std::move(rows)), bgColor(bgCol), fgColor(fgCol) {
        growMode = gfGrowHiX | gfGrowHiY;
    }

    void draw() override {
        TDrawBuffer b;
        const ushort bgv = bgColor ? (*bgColor & 0xFF) : ushort(1);
        const ushort fgv = fgColor ? (*fgColor & 0xFF) : ushort(15);
        const TColorDesired bgDes(TColorXTerm(uchar((ushort)bgv)));
        const TColorDesired fgLetters(TColorXTerm(uchar((ushort)fgv)));
        const TColorDesired fgLineBlack(TColorRGB(0, 0, 0));
        const short mx = 1;
        const short my = 1;
        const int nLines = (int)lines.size();
        for (short y = 0; y < size.y; ++y) {
            const TColorAttr fill(TColorDesired(TColorXTerm(uchar((ushort)7))), bgDes);
            b.moveChar(0, ' ', fill, size.x);
            if (y >= my && y < my + nLines) {
                const std::string &ln = lines[(size_t)(y - my)];
                short x = mx;
                for (size_t i = 0; i < ln.size() && x < size.x;) {
                    const size_t len = utf8GlyphByteLen(ln.c_str() + i, ln.size() - i);
                    const size_t step = len ? len : 1;
                    const char *gp = ln.c_str() + i;
                    const uint32_t cp = utf8CodepointN(gp, step);
                    TColorAttr cell;
                    if (step == 1 && (unsigned char)*gp == ' ')
                        cell = TColorAttr(bgDes, bgDes);
                    else if (isDoubleStrokeBoxDrawing(cp))
                        cell = TColorAttr(fgLineBlack, bgDes);
                    else
                        cell = TColorAttr(fgLetters, bgDes);
                    b.moveStr(x, gp, cell, 1);
                    i += step;
                    ++x;
                }
            }
            writeLine(0, y, size.x, 1, b);
        }
    }

private:
    std::vector<std::string> lines;
    const ushort *bgColor;
    const ushort *fgColor;
};

class WelcomeDialog : public TDialog {
public:
    WelcomeDialog(const TRect &bounds, TStringView aTitle) noexcept :
        TWindowInit(&TDialog::initFrame),
        TDialog(bounds, aTitle) {
        palette = dpBlueDialog;
    }

    virtual TColorAttr mapColor(uchar index) override {
        // Shadow de TButton en dialogo: negro solido para evitar
        // efecto "transparente" cuando el dialogo pasa sobre otra vista.
        if (index == 0x2E || index == 0x4E || index == 0x6E)
            return TColorAttr(0x00);
        return TDialog::mapColor(index);
    }

    virtual void handleEvent(TEvent &event) override {
        TDialog::handleEvent(event);
        if (event.what == evCommand && event.message.command == cmNewNovel) {
            endModal(cmNewNovel);
            clearEvent(event);
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

class RetroWriterTVApp : public TApplication {
public:
    RetroWriterTVApp(const std::string &projectDir) :
        TProgInit(&RetroWriterTVApp::initStatusLine,
                  &RetroWriterTVApp::initMenuBar,
                  &RetroWriterTVApp::initDeskTop),
        projectDir(projectDir),
        novelsDir(joinPath(projectDir, "novelas")),
        novelsIndexPath(joinPath(projectDir, "novelas.idx")),
        preferencesPath(joinPath(projectDir, "appearance.cfg")),
        workspacePath(joinPath(projectDir, "workspace.cfg")) {
        loadAppearancePreferences();
        fillPaletteExplicit(textColor, backColor, paletteBytes);
        browserDir = absolutePath(this->projectDir);
        lastEditorPath.clear();
        loadNovelsIndex();
        if (novels.empty()) {
            createNovel("Novela 1");
            createChapter("Capitulo 1");
        } else {
            if (!loadWorkspaceSession()) {
                switchNovel(0);
                if (chapters.empty()) createChapter("Capitulo 1");
            }
        }
        createDesktopWidgets();
        if (!lastEditorPath.empty()) {
            std::ifstream chk(lastEditorPath);
            if (chk.good())
                openFileInEditor(lastEditorPath);
            else {
                lastEditorPath.clear();
                openCurrentChapter();
            }
        } else {
            openCurrentChapter();
        }
        applyDesktopPatternChar();
        // La bienvenida se muestra en idle() tras run(): un execView modal
        // desde el constructor deja a veces sin repintar navegador y editor.
    }

    static TMenuBar *initMenuBar(TRect r) {
        r.b.y = r.a.y + 1;
        return new TMenuBar(
            r,
            *new TSubMenu("~F~ile", kbAltF) +
                *new TMenuItem("~N~ueva novela...", cmNewNovel, kbCtrlN, hcNoContext, "Ctrl-N") +
                *new TMenuItem("Nuevo ~c~apitulo...", cmNewChapter, kbCtrlC, hcNoContext, "Ctrl-C") +
                newLine() +
                *new TMenuItem("~B~ienvenida", cmWelcome, kbF2, hcNoContext, "F2") +
                *new TMenuItem("E~x~it", cmQuit, kbAltX, hcNoContext, "Alt-X") +
                *new TMenuItem("Salir rapido", cmQuit, kbCtrlQ, hcNoContext, "Ctrl-Q") +
            *new TSubMenu("~W~indows", kbAltW) +
                *new TMenuItem("File ~M~anager", cmNavigator, kbF4, hcNoContext, "F4") +
                *new TMenuItem("Panel archivos (~E~)", cmToggleFilePanel, kbCtrlE, hcNoContext, "Ctrl-E") +
                *new TMenuItem("Nove~l~a anterior", cmPrevNovel, kbF7, hcNoContext, "F7") +
                *new TMenuItem("Novela ~s~iguiente", cmNextNovel, kbF8, hcNoContext, "F8") +
                *new TMenuItem("Capitulo anterior", cmPrevChapter, kbAltJ, hcNoContext, "Alt-J") +
                *new TMenuItem("Capitulo siguiente", cmNextChapter, kbAltK, hcNoContext, "Alt-K") +
                newLine() +
                *new TMenuItem("Refrescar paneles", cmRefreshWidgets, kbF6, hcNoContext, "F6") +
            *new TSubMenu("~P~referencias", kbAltP) +
                *new TMenuItem("~C~olores y fondo...", cmPreferences, kbF5, hcNoContext, "F5")
        );
    }

    static TStatusLine *initStatusLine(TRect r) {
        r.a.y = r.b.y - 1;
        return new TStatusLine(
            r,
            *new TStatusDef(0, 0xFFFF) +
                *new TStatusItem("~F2~ Welcome", kbF2, cmWelcome) +
                *new TStatusItem("~F4~ File Manager", kbF4, cmNavigator) +
                *new TStatusItem("~F9~ Panel", kbF9, cmToggleFilePanel) +
                *new TStatusItem("~Ctrl-N~ Novela", kbCtrlN, cmNewNovel) +
                *new TStatusItem("~Ctrl-C~ Capitulo", kbCtrlC, cmNewChapter) +
                *new TStatusItem("~F5~ Preferencias", kbF5, cmPreferences) +
                *new TStatusItem("~F7/F8~ Novela", kbF7, cmPrevNovel) +
                *new TStatusItem("~Alt-J/K~ Capitulo", kbAltJ, cmPrevChapter) +
                *new TStatusItem("~Alt-X~ Salir", kbAltX, cmQuit) +
                *new TStatusItem("~Ctrl-Q~ Salir", kbCtrlQ, cmQuit) +
                *new TStatusItem(0, kbF10, cmMenu)
        );
    }

    virtual TPalette &getPalette() const {
        RetroWriterTVApp *self = const_cast<RetroWriterTVApp *>(this);
        if (appPaletteDirty || !cachedAppPalette) {
            self->rebuildCachedPalette();
        }
        return *cachedAppPalette;
    }

    virtual TColorAttr mapColor(uchar index) override {
        // Primero obtenemos el atributo base resuelto por Turbo Vision.
        TColorAttr base = TApplication::mapColor(index);

        // Mantener dialogos clasicos (botones/sombras) en BIOS para no romperlos.
        if (index >= 0x20 && index <= 0x7F)
            return base;

        auto toBios16 = [](ushort c) -> uchar {
            if (c <= 0x0F)
                return uchar(c & 0x0F);
            return uchar(XTerm256toXTerm16(uchar(c & 0xFF)) & 0x0F);
        };

        uchar fg16 = toBios16(textColor);
        uchar bg16 = toBios16(backColor);
        uchar bios = base.toBIOS();
        uchar srcFg = uchar(bios & 0x0F);
        uchar srcBg = uchar((bios >> 4) & 0x0F);

        // Si el atributo base representa inversion (fondo/texto invertidos),
        // respetamos esa inversion en el espacio xterm-256.
        bool swapped = (srcFg == bg16 && srcBg == fg16);
        if (swapped) {
            return TColorAttr(
                TColorDesired(TColorXTerm(uchar(backColor & 0xFF))),
                TColorDesired(TColorXTerm(uchar(textColor & 0xFF)))
            );
        }

        // Caso normal: aplica exactamente los colores elegidos en el selector.
        return TColorAttr(
            TColorDesired(TColorXTerm(uchar(textColor & 0xFF))),
            TColorDesired(TColorXTerm(uchar(backColor & 0xFF)))
        );
    }

    virtual void idle() override {
        TApplication::idle();
        if (pendingStartupWelcome) {
            pendingStartupWelcome = false;
            showWelcomeDialog();
        }
    }

    virtual void shutDown() override {
        saveWorkspaceSession();
        TProgram::shutDown();
    }

    virtual void getEvent(TEvent &event) {
        TProgram::getEvent(event);
        // Salida global: capturamos aquí para que ninguna vista la "coma".
        if (event.what == evKeyDown &&
            (event.keyDown.keyCode == kbAltX ||
             event.keyDown.keyCode == kbCtrlQ ||
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
        // Permite salida inmediata incluso si alguna vista (ej. editor)
        // intenta bloquear cmQuit por validaciones internas.
        if (command == cmQuit)
            return True;
        return TApplication::valid(command);
    }

    virtual void handleEvent(TEvent &event) {
        // Atajo de salida "duro": lo capturamos antes de delegar a views
        // para evitar que controles (como el editor) consuman la tecla.
        if (event.what == evKeyDown &&
            (event.keyDown.keyCode == kbAltX || event.keyDown.keyCode == kbCtrlQ)) {
            requestQuit();
            clearEvent(event);
            return;
        }

        // Si llega el comando de salir desde menu/status, cerramos app directo.
        if (event.what == evCommand && event.message.command == cmQuit) {
            requestQuit();
            clearEvent(event);
            return;
        }

        if (event.what == evBroadcast && event.message.command == cmNavSelect) {
            auto *src = static_cast<NavigatorListView *>(event.message.infoPtr);
            fileManagerActivate(src);
            clearEvent(event);
            return;
        }

        if (event.what == evBroadcast && event.message.command == cmCreateFolder) {
            onCreateFolderInBrowser();
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
            case cmNewNovel:
                onNewNovel();
                clearEvent(event);
                break;
            case cmNewChapter:
                onNewChapter();
                clearEvent(event);
                break;
            case cmWelcome:
                showWelcomeDialog();
                clearEvent(event);
                break;
            case cmNavigator:
                showNavigatorDialog();
                clearEvent(event);
                break;
            case cmPrevNovel:
                stepNovel(-1);
                clearEvent(event);
                break;
            case cmNextNovel:
                stepNovel(+1);
                clearEvent(event);
                break;
            case cmPrevChapter:
                stepChapter(-1);
                clearEvent(event);
                break;
            case cmNextChapter:
                stepChapter(+1);
                clearEvent(event);
                break;
            case cmRefreshWidgets:
                destroyWidgetWindows();
                createDesktopWidgets();
                clearEvent(event);
                break;
            case cmPreferences:
                showPreferencesDialog();
                clearEvent(event);
                break;
            case cmToggleFilePanel:
                onToggleFilePanel();
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
    std::string novelsDir;
    std::string novelsIndexPath;
    std::string preferencesPath;
    std::string workspacePath;
    /** Directorio mostrado en el File Manager (explorador). */
    std::string browserDir;
    /** Ultimo archivo abierto en el editor (sesion y reapertura). */
    std::string lastEditorPath;
    std::string currentNovelDir;
    std::string chaptersDir;
    std::string chaptersIndexPath;

    std::vector<EntryMeta> novels;
    std::vector<EntryMeta> chapters;
    int currentNovelIndex {-1};
    int currentChapterIndex {-1};
    /** Color explicito de texto VGA (0..15). */
    ushort textColor {15};
    /** Color explicito de fondo VGA (0..7 recomendado). */
    ushort backColor {1};
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
    bool filePanelVisible {true};
    UnicodeBackground *unicodeBackground {nullptr};
    /** Primera pasada del bucle principal: mostrar bienvenida fuera del ctor. */
    bool pendingStartupWelcome {true};

    void requestQuit() {
        // Forzamos fin del loop principal y de modales activos.
        if (TProgram::application)
            TProgram::application->endModal(cmQuit);
        if (deskTop)
            deskTop->endModal(cmQuit);
        endState = cmQuit;
        endModal(cmQuit);

        // Inyecta comando quit para drenar cualquier loop intermedio.
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
            }
        }
    }

    void saveAppearancePreferences() {
        std::ofstream out(preferencesPath, std::ios::trunc);
        if (!out.is_open()) return;
        out << "v1\n";
        out << "textColor " << textColor << "\n";
        out << "backColor " << backColor << "\n";
        out << "patternUtf8 " << utf8ToHex(desktopPatternUtf8) << "\n";
    }

    bool loadWorkspaceSession() {
        std::ifstream in(workspacePath);
        if (!in.is_open()) return false;
        std::string ver;
        if (!std::getline(in, ver) || trim(ver) != "v1") return false;
        int novel = -1, chapter = -1;
        std::string cwdVal, editVal;
        std::string line;
        while (std::getline(in, line)) {
            std::string t = trim(line);
            if (t.empty()) continue;
            size_t sp = t.find(' ');
            if (sp == std::string::npos) continue;
            std::string key = trim(t.substr(0, sp));
            std::string val = t.substr(sp + 1);
            while (!val.empty() && val[0] == ' ') val.erase(0, 1);
            try {
                if (key == "novel") novel = std::stoi(trim(val));
                else if (key == "chapter") chapter = std::stoi(trim(val));
            } catch (...) {
            }
            if (key == "cwd") cwdVal = val;
            else if (key == "edit") editVal = val;
            else if (key == "panel") filePanelVisible = (trim(val) != "0");
        }

        bool novelOk = false;
        if (novel >= 0 && novel < (int)novels.size()) {
            switchNovel(novel);
            if (chapters.empty()) createChapter("Capitulo 1");
            if (chapter >= 0 && chapter < (int)chapters.size())
                currentChapterIndex = chapter;
            else
                currentChapterIndex = chapters.empty() ? -1 : 0;
            novelOk = true;
        }

        std::error_code ec;
        if (!cwdVal.empty() && fs::is_directory(fs::path(cwdVal), ec))
            browserDir = absolutePath(cwdVal);

        lastEditorPath.clear();
        if (!editVal.empty())
            lastEditorPath = editVal;

        return novelOk;
    }

    void saveWorkspaceSession() {
        std::ofstream out(workspacePath, std::ios::trunc);
        if (!out.is_open()) return;
        out << "v1\n";
        if (currentNovelIndex >= 0)
            out << "novel " << currentNovelIndex << "\n";
        if (currentChapterIndex >= 0)
            out << "chapter " << currentChapterIndex << "\n";
        out << "cwd " << browserDir << "\n";
        out << "panel " << (filePanelVisible ? 1 : 0) << "\n";
        if (!lastEditorPath.empty())
            out << "edit " << lastEditorPath << "\n";
    }

    void restoreMainWorkspaceAfterModal() {
        if (!deskTop) return;
        destroyWidgetWindows();
        createDesktopWidgets();
        reopenEditorFromSession();
        applyDesktopPatternChar();
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

    bool loadEntriesIndex(const std::string &indexPath, std::vector<EntryMeta> &dst) {
        dst.clear();
        std::ifstream in(indexPath);
        if (!in.is_open()) return true;
        std::string line;
        while (std::getline(in, line)) {
            auto sep = line.find('|');
            if (sep == std::string::npos) continue;
            EntryMeta e;
            e.key = trim(line.substr(0, sep));
            e.title = trim(line.substr(sep + 1));
            if (!e.key.empty() && !e.title.empty()) dst.push_back(e);
        }
        return true;
    }

    bool saveEntriesIndex(const std::string &indexPath, const std::vector<EntryMeta> &src) {
        std::ofstream out(indexPath, std::ios::trunc);
        if (!out.is_open()) return false;
        for (const auto &e : src) out << e.key << "|" << e.title << "\n";
        return true;
    }

    bool loadNovelsIndex() { return loadEntriesIndex(novelsIndexPath, novels); }
    bool saveNovelsIndex() { return saveEntriesIndex(novelsIndexPath, novels); }
    bool loadChaptersIndex() { return loadEntriesIndex(chaptersIndexPath, chapters); }
    bool saveChaptersIndex() { return saveEntriesIndex(chaptersIndexPath, chapters); }

    void switchNovel(int idx) {
        if (idx < 0 || idx >= (int)novels.size()) return;
        currentNovelIndex = idx;
        currentNovelDir = joinPath(novelsDir, novels[idx].key);
        chaptersDir = joinPath(currentNovelDir, "chapters");
        chaptersIndexPath = joinPath(currentNovelDir, "chapters.idx");
        ensureDir(novelsDir);
        ensureDir(currentNovelDir);
        ensureDir(chaptersDir);
        loadChaptersIndex();
        if (!chapters.empty()) currentChapterIndex = 0;
        browserDir = absolutePath(chaptersDir);
    }

    bool createNovel(const std::string &titleRaw) {
        std::string title = trim(titleRaw);
        if (title.empty()) title = "Novela sin titulo";
        if ((int)novels.size() >= 999) return false;

        std::string slug = slugify(title);
        int seq = (int)novels.size() + 1;
        std::string folder;
        while (true) {
            std::ostringstream ss;
            ss << "nv";
            ss.width(3);
            ss.fill('0');
            ss << seq << "_" << slug;
            folder = ss.str();
            bool exists = false;
            for (const auto &n : novels) {
                if (n.key == folder) {
                    exists = true;
                    break;
                }
            }
            if (!exists) break;
            seq++;
        }

        novels.push_back({folder, title});
        saveNovelsIndex();
        switchNovel((int)novels.size() - 1);
        return true;
    }

    bool createChapter(const std::string &titleRaw) {
        if (currentNovelIndex < 0) return false;
        std::string title = trim(titleRaw);
        if (title.empty()) title = "Capitulo sin titulo";
        if ((int)chapters.size() >= 999) return false;

        std::string slug = slugify(title);
        int seq = (int)chapters.size() + 1;
        std::string filename;
        while (true) {
            std::ostringstream ss;
            ss << "ch";
            ss.width(3);
            ss.fill('0');
            ss << seq << "_" << slug << ".txt";
            filename = ss.str();
            bool exists = false;
            for (const auto &c : chapters) {
                if (c.key == filename) {
                    exists = true;
                    break;
                }
            }
            if (!exists) break;
            seq++;
        }

        std::string full = joinPath(chaptersDir, filename);
        std::ofstream f(full, std::ios::app);
        f.close();

        chapters.push_back({filename, title});
        saveChaptersIndex();
        currentChapterIndex = (int)chapters.size() - 1;
        return true;
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

    void closeCurrentEditor() {
        if (editorWindow && editorWindow->owner) {
            editorWindow->close();
        }
        editorWindow = nullptr;
    }

    TRect editorDeskRect() const {
        if (!deskTop) return TRect(34, 2, 80, 24);
        TRect r = deskTop->getExtent();
        r.a.y = 2;
        r.b.x -= 1;
        r.b.y -= 1;
        r.a.x = filePanelVisible ? (short)(kFilePanelRightX + 1) : 1;
        return r;
    }

    void relayoutEditorWindow() {
        if (!editorWindow || !deskTop || !editorWindow->owner) return;
        TRect nr = editorDeskRect();
        // locate() une rect antiguo/nuevo y llama drawUnderRect: evita rastro al encoger.
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

    void openFileInEditor(const std::string &path) {
        if (!deskTop) return;
        const std::string absPath = absolutePath(path);
        closeCurrentEditor();
        TRect r = editorDeskRect();
        TEditWindow *w = new TEditWindow(r, absPath.c_str(), wnNoNumber);
        // Sin ofTopSelect: select() hace setCurrent(escritorio) y no makeFirst(). Con el flag
        // por defecto, makeFirst() -> putInFrontOf -> resetCurrent() y firstMatch() a menudo
        // fija el panel como ventana actual: el editor pierde sfSelected y el raton no enfoca.
        w->options &= ~ofTopSelect;
        TView *v = validView(w);
        if (!v) return;
        editorWindow = w;
        deskTop->insert(v);
        ensureEditorAboveNavigator();
        // ensureEditorAboveNavigator deja el foco en el panel si prev==navWindow; al abrir
        // archivo siempre queremos teclear en el editor.
        if (deskTop && editorWindow)
            deskTop->setCurrent(editorWindow, normalSelect);
        lastEditorPath = absPath;
        saveWorkspaceSession();
    }

    void openCurrentChapter() {
        if (currentNovelIndex < 0 || currentChapterIndex < 0 || currentChapterIndex >= (int)chapters.size()) return;
        std::string path = joinPath(chaptersDir, chapters[currentChapterIndex].key);
        openFileInEditor(path);
    }

    void reopenEditorFromSession() {
        if (!lastEditorPath.empty()) {
            std::ifstream chk(lastEditorPath);
            if (chk.good()) {
                openFileInEditor(lastEditorPath);
                return;
            }
        }
        openCurrentChapter();
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
        if (!promptTitle("Crear carpeta", "Nombre de la carpeta", name)) return;
        name = trim(name);
        if (name.empty() || name == "." || name == "..") return;
        if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return;
        std::string path = joinPath(browserDir, name);
        if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
            messageBox(mfError | mfOKButton, "No se pudo crear la carpeta: %s", strerror(errno));
            return;
        }
        browserDir = absolutePath(path);
        refreshNavigatorWidget();
        saveWorkspaceSession();
    }

    static bool nameEndsWithTxt(const std::string &s) {
        if (s.size() < 4)
            return false;
        return asciiLower(s.substr(s.size() - 4)) == ".txt";
    }

    void onCreateTxtFileInBrowser() {
        std::string name;
        if (!promptTitle("Nuevo archivo", "Nombre del archivo (.txt)", name)) return;
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
        f.close();
        const std::string absPath = absolutePath(path);
        openFileInEditor(absPath);
        refreshNavigatorWidget();
        saveWorkspaceSession();
    }

    void fileManagerActivate(NavigatorListView *src) {
        if (!src) return;
        NavigatorListView::NavItem it;
        if (!src->peekCursorItem(it)) return;

        if (!it.isDirectory && it.label != "..") {
            openFileInEditor(it.fullPath);
            if (src->owner) {
                if (TDialog *dlg = dynamic_cast<TDialog *>(src->owner))
                    dlg->endModal(cmOK);
            }
            return;
        }

        browserDir = absolutePath(it.fullPath);

        if (src == navListView) {
            refreshNavigatorWidget();
        } else {
            src->setItems(buildFileManagerItems());
            if (navListView)
                navListView->setItems(buildFileManagerItems());
        }
        saveWorkspaceSession();
    }

    void stepNovel(int delta) {
        if (novels.empty()) return;
        int n = (int)novels.size();
        if (currentNovelIndex < 0) currentNovelIndex = 0;
        currentNovelIndex = (currentNovelIndex + delta + n) % n;
        switchNovel(currentNovelIndex);
        if (chapters.empty()) createChapter("Capitulo 1");
        currentChapterIndex = std::max(0, currentChapterIndex);
        openCurrentChapter();
        refreshNavigatorWidget();
    }

    void stepChapter(int delta) {
        if (chapters.empty()) return;
        int n = (int)chapters.size();
        if (currentChapterIndex < 0) currentChapterIndex = 0;
        currentChapterIndex = (currentChapterIndex + delta + n) % n;
        openCurrentChapter();
        refreshNavigatorWidget();
    }

    void onNewNovel() {
        std::string title;
        if (!promptTitle("Nueva novela", "Nombre de la novela", title)) return;
        createNovel(title);
        if (chapters.empty()) createChapter("Capitulo 1");
        openCurrentChapter();
        refreshNavigatorWidget();
    }

    void onNewChapter() {
        std::string title;
        if (!promptTitle("Nuevo capitulo", "Nombre del capitulo", title)) return;
        createChapter(title);
        openCurrentChapter();
        refreshNavigatorWidget();
    }

    void showWelcomeDialog() {
        const int dlgL = 6, dlgT = 2, dlgR = 78, dlgB = 26;
        const int dlgW = dlgR - dlgL;
        const int artW = 46;
        const int artLines = 14;
        const int inset = 2;
        const int buttonRow = 21;
        const int innerW = dlgW - 2 * inset;
        const int x0 = inset + (innerW - artW) / 2;
        const int textAreaH = buttonRow - inset;
        const int y0 = inset + (textAreaH - artLines) / 2;
        const int pad = 1;
        const int bx = std::max(1, x0 - pad);
        const int by = std::max(1, y0 - pad);
        const int br = std::min((int)dlgW - inset, x0 + artW + pad);
        const int bb = y0 + artLines;
        TDialog *d = new WelcomeDialog(TRect(dlgL, dlgT, dlgR, dlgB), "RetroWriter Desktop");
        d->options |= ofCentered;
        d->insert(new RainbowBannerView(TRect((short)bx, (short)by, (short)br, (short)bb), welcomeBannerLines(), &backColor,
                                        &textColor));
        // "Nueva novela" (12 chars) en 14 celdas queda al ras del borde; algo mas
        // de ancho evita que se vea cortada con negrita o el glifo \xDC.
        const int bwNovela = 18;
        const int bw = 14;
        const int gap = 3;
        const int btnBarW = bwNovela + 2 * bw + 2 * gap;
        const int bx0 = inset + (innerW - btnBarW) / 2;
        d->insert(new CleanButton(TRect(bx0, buttonRow, bx0 + bwNovela, buttonRow + 2), "Nueva novela", cmNewNovel, false));
        d->insert(new CleanButton(TRect(bx0 + bwNovela + gap, buttonRow, bx0 + bwNovela + gap + bw, buttonRow + 2), "Entrar", cmOK, true));
        d->insert(new CleanButton(TRect(bx0 + bwNovela + gap + bw + gap, buttonRow, bx0 + bwNovela + gap + bw + gap + bw, buttonRow + 2), "Salir", cmCancel, false));
        TColorAttr prevShadowAttr = shadowAttr;
        shadowAttr = TColorAttr(0x00);
        ushort res = deskTop->execView(d);
        shadowAttr = prevShadowAttr;
        destroy(d);
        if (res == cmCancel) {
            endModal(cmQuit);
        } else if (res == cmNewNovel) {
            onNewNovel();
            restoreMainWorkspaceAfterModal();
        } else {
            restoreMainWorkspaceAfterModal();
        }
    }

    void showNavigatorDialog() {
        const int dlgW = 38; // 40 - 2 columnas de marco
        TDialog *d = new TDialog(TRect(2, 2, 40, 23), pathTitleForWidth(browserDir, dlgW - 4).c_str());
        d->palette = dpBlueDialog;
        d->eventMask |= evBroadcast;
        NavigatorListView *navList = new NavigatorListView(TRect(1, 1, 37, 17), &textColor, &backColor);
        navList->setItems(buildFileManagerItems());
        d->insert(navList);
        d->insert(new TButton(TRect(12, 18, 28, 20), "~C~errar", cmCancel, bfDefault));
        d->setCurrent(navList, normalSelect);
        deskTop->execView(d);
        destroy(d);
        if (navListView)
            navListView->setItems(buildFileManagerItems());
    }

    void showPreferencesDialog() {
        AppearanceDialog *d = new AppearanceDialog(TRect(2, 2, 79, 19), textColor, backColor,
            desktopPatternChar, desktopPatternUtf8);

        ushort res = deskTop->execView(d);
        if (res == cmOK) {
            textColor = d->currentFg();
            backColor = d->currentBg();
            desktopPatternChar = d->currentSymbol();
            desktopPatternUtf8 = d->currentSymbolUtf8();
            saveAppearancePreferences();
            fillPaletteExplicit(textColor, backColor, paletteBytes);
            appPaletteDirty = true;
            applyDesktopPatternChar();
            destroyWidgetWindows();
            createDesktopWidgets();
            // Reabre y repinta editor para aplicar paleta a todo el buffer,
            // no solo a la linea activa al teclear.
            reopenEditorFromSession();
            if (deskTop)
                deskTop->drawView();
            redraw();
        }
        destroy(d);
    }

    void refreshNavigatorWidget() {
        if (navWindow && navWindow->owner) navWindow->close();
        navWindow = nullptr;
        navListView = nullptr;

        if (!filePanelVisible)
            return;

        TRect r(1, 2, kFilePanelRightX, 24);
        const int navW = r.b.x - r.a.x;
        TWindow *w = new TWindow(r, pathTitleForWidth(browserDir, navW - 2).c_str(), wnNoNumber);
        w->flags &= ~wfZoom;
        w->flags &= ~wfGrow;
        w->flags &= ~wfClose;
        // TWindow trae ofTopSelect: al enfocar/ciclar ventana, select() -> makeFirst() y el panel
        // pasa delante del editor; la sombra invade el texto. Sin esto, setCurrent basta.
        w->options &= ~ofTopSelect;
        w->eventMask |= evBroadcast;
        const short winRx = short(r.b.x - r.a.x - 1);
        const short winBy = short(r.b.y - r.a.y - 1);
        // Pie: 2 filas (linea + [ Crear carpeta ]); la lista deja esas filas libres.
        navListView = new NavigatorListView(TRect(1, 1, winRx, winBy - 2), &textColor, &backColor);
        navListView->setItems(buildFileManagerItems());
        w->insert(navListView);
        w->insert(new FolderPanelFooterStrip(TRect(1, winBy - 2, winRx, winBy)));
        TView *v = validView(w);
        if (!v) return;
        deskTop->insert(v);
        navWindow = w;
        if (navListView)
            navWindow->setCurrent(navListView, normalSelect);
        ensureEditorAboveNavigator();
    }

    /** El editor debe dibujarse encima del panel para que no se vea sombra/recorte raro. */
    void ensureEditorAboveNavigator() {
        if (!filePanelVisible || !deskTop || !navWindow || !editorWindow) return;
        if (navWindow->owner != deskTop || editorWindow->owner != deskTop) return;
        // putInFrontOf dispara resetCurrent() en el escritorio; firstMatch suele elegir "last"
        // y roba el foco al panel. Restauramos la ventana activa salvo que ya fuera el panel.
        TView *prev = deskTop->current;
        editorWindow->putInFrontOf(navWindow);
        if (prev == navWindow)
            deskTop->setCurrent(navWindow, normalSelect);
        else
            deskTop->setCurrent(editorWindow, normalSelect);
    }

    void createDesktopWidgets() {
        refreshNavigatorWidget();
    }

    void destroyWidgetWindows() {
        if (navWindow && navWindow->owner) navWindow->close();
        navWindow = nullptr;
        navListView = nullptr;
    }
};

} // namespace

int main(int argc, char **argv) {
    std::string projectDir = (argc >= 2) ? argv[1] : ".";
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
