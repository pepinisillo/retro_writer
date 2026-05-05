/*------------------------------------------------------------*/
/* filename - teditor1.cpp                                    */
/*                                                            */
/* function(s)                                                */
/*            TEditor member functions                        */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TKeys
#define Uses_TEditor
#define Uses_TIndicator
#define Uses_TEvent
#define Uses_TScrollBar
#define Uses_TFindDialogRec
#define Uses_TReplaceDialogRec
#define Uses_opstream
#define Uses_ipstream
#define Uses_TText
#define Uses_TClipboard
#include <tvision/tv.h>

#if !defined( __STRING_H )
#include <string.h>
#endif  // __STRING_H

#if !defined( __CTYPE_H )
#include <ctype.h>
#endif  // __CTYPE_H

#if !defined( __DOS_H )
#include <dos.h>
#endif  // __DOS_H

#include <utility>
#include <vector>

#ifndef __BORLANDC__
#define register
#endif

const ushort firstKeys[] =
{
    41,
    kbCtrlA, cmSelectAll,
    kbCtrlC, cmPageDown,
    kbCtrlD, cmCharRight,
    kbCtrlE, cmLineUp,
    kbCtrlF, cmWordRight,
    kbCtrlG, cmDelChar,
    kbCtrlH, cmBackSpace,
    kbCtrlK, 0xFF02,
    kbCtrlL, cmSearchAgain,
    kbCtrlM, cmNewLine,
    kbCtrlO, cmIndentMode,
    kbCtrlP, cmEncoding,
    kbCtrlQ, 0xFF01,
    kbCtrlR, cmPageUp,
    kbCtrlS, cmCharLeft,
    kbCtrlT, cmDelWord,
    kbCtrlU, cmUndo,
    kbCtrlV, cmInsMode,
    kbCtrlX, cmLineDown,
    kbCtrlY, cmDelLine,
    kbLeft, cmCharLeft,
    kbRight, cmCharRight,
    kbAltBack, cmDelWordLeft,
    kbCtrlBack, cmDelWordLeft,
    kbCtrlDel, cmDelWord,
    kbCtrlLeft, cmWordLeft,
    kbCtrlRight, cmWordRight,
    kbHome, cmLineStart,
    kbEnd, cmLineEnd,
    kbUp, cmLineUp,
    kbDown, cmLineDown,
    kbPgUp, cmPageUp,
    kbPgDn, cmPageDown,
    kbCtrlHome, cmTextStart,
    kbCtrlEnd, cmTextEnd,
    kbIns, cmInsMode,
    kbDel, cmDelChar,
    kbShiftIns, cmPaste,
    kbShiftDel, cmCut,
    kbCtrlIns, cmCopy,
    kbCtrlDel, cmClear
};

const ushort quickKeys[] =
{   8,
    'A', cmReplace,
    'C', cmTextEnd,
    'D', cmLineEnd,
    'F', cmFind,
    'H', cmDelStart,
    'R', cmTextStart,
    'S', cmLineStart,
    'Y', cmDelEnd
};

const ushort blockKeys[] =
{   5,
    'B', cmStartSelect,
    'C', cmPaste,
    'H', cmHideSelect,
    'K', cmCopy,
    'Y', cmCut
};

const ushort *keyMap[] = { firstKeys, quickKeys, blockKeys };

ushort defEditorDialog( int, ... );

#pragma warn -asc

ushort scanKeyMap( const void *keyMap, ushort keyCode )
{
#if !defined(__FLAT__)
asm {
    PUSH DS
    LDS SI,keyMap
    MOV DX,keyCode
    CLD
    LODSW
    MOV CX,AX
    }
__1:
asm {
    LODSW
    MOV BX,AX
    LODSW
    CMP BL,DL
    JNE __3
    OR  BH,BH
    JE  __4
    CMP BH,DH
    JE  __4
    }
__3:
asm {
    LOOP    __1
    XOR AX,AX
    }
__4:
asm POP DS
    return _AX;
#else
    register ushort *kM = (ushort *)keyMap;
    uchar codeLow = keyCode & 0xff;
    uchar codeHi  = keyCode >> 8;

    int n;

    for (n = *kM++; n--; kM++)
    {
        uchar  mapLow  = *kM & 0xff;
        uchar  mapHi   = *kM >> 8;
        kM++;
        ushort command = *kM;

        if ((mapLow == codeLow) && ((mapHi == 0) || (mapHi == codeHi)))
            return command;
    };
    return 0;
#endif
}

#pragma warn .asc

#define cpEditor    "\x06\x07"
static const bool kEditorSoftWrap = true;

static int visualWrapWidth(const TEditor &ed)
{
    return max(1, (int)ed.size.x);
}

static inline bool edWrapSpaceChar(TEditor &ed, uint p)
{
    if (p >= ed.bufLen)
        return false;
    char c = ed.bufChar(p);
    return c == ' ' || c == '\t';
}

struct EdWrapHole
{
    uint a;
    uint b;
    int snapVx;
    int snapSubRow;
};

static void edBuildWrapSegments(TEditor &ed, uint ls, uint le, int w,
                                std::vector<std::pair<uint, uint>> &segs,
                                std::vector<EdWrapHole> &holes)
{
    segs.clear();
    holes.clear();
    if (le <= ls)
    {
        segs.emplace_back(ls, ls);
        return;
    }

    uint segStart = ls;
    int col = 0;
    uint p = ls;

    while (p < le)
    {
        uint ps = p;
        while (ps < le && edWrapSpaceChar(ed, ps))
            ps++;
        uint pw = ps;
        while (pw < le && !edWrapSpaceChar(ed, pw))
            pw++;

        const int spaceW = (int) ed.charPos(p, ps);
        const int wordW = (int) ed.charPos(ps, pw);

        if (spaceW > 0)
        {
            if (col + spaceW <= w)
            {
                col += spaceW;
                p = ps;
            }
            else if (col > 0)
            {
                if (segStart < p)
                    segs.emplace_back(segStart, p);
                const int snapSubRow = (int) segs.size() - 1;
                holes.push_back(EdWrapHole{p, ps, min(col, w - 1), snapSubRow});
                segStart = ps;
                col = 0;
                p = ps;
                continue;
            }
            else
            {
                uint q = p;
                while (q < ps)
                {
                    uint nq = ed.nextChar(q);
                    const int cw = (int) ed.charPos(q, nq);
                    if (col + cw > w)
                    {
                        if (col > 0)
                        {
                            if (segStart < q)
                                segs.emplace_back(segStart, q);
                            segStart = q;
                            col = 0;
                        }
                        else if (cw > w)
                        {
                            segs.emplace_back(q, nq);
                            segStart = nq;
                            col = 0;
                            q = nq;
                            continue;
                        }
                    }
                    col += cw;
                    q = nq;
                }
                p = ps;
                continue;
            }
        }

        if (pw == ps)
        {
            p = pw;
            continue;
        }

        if (col + wordW <= w)
        {
            col += wordW;
            p = pw;
            continue;
        }

        if (col > 0)
        {
            if (segStart < ps)
                segs.emplace_back(segStart, ps);
            segStart = ps;
            col = 0;
            p = ps;
            continue;
        }

        uint q = ps;
        while (q < pw)
        {
            uint nq = ed.nextChar(q);
            const int cw = (int) ed.charPos(q, nq);
            if (col + cw > w)
            {
                if (col > 0)
                {
                    if (segStart < q)
                        segs.emplace_back(segStart, q);
                    segStart = q;
                    col = 0;
                    continue;
                }
                if (cw > w)
                {
                    segs.emplace_back(q, nq);
                    segStart = nq;
                    col = 0;
                    q = nq;
                    continue;
                }
            }
            col += cw;
            q = nq;
        }
        p = pw;
    }

    if (segStart < le)
        segs.emplace_back(segStart, le);
    if (segs.empty())
        segs.emplace_back(ls, ls);
}

static int visualRowsForLine(TEditor &ed, uint lineStartPtr)
{
    int w = visualWrapWidth(ed);
    uint le = ed.lineEnd(lineStartPtr);
    std::vector<std::pair<uint, uint>> segs;
    std::vector<EdWrapHole> holes;
    edBuildWrapSegments(ed, lineStartPtr, le, w, segs, holes);
    return max(1, (int) segs.size());
}

static int visualTotalRows(TEditor &ed)
{
    int total = 0;
    for (uint p = 0; p < ed.bufLen; p = ed.nextLine(p))
        total += visualRowsForLine(ed, p);
    return max(total, 1);
}

static void locateVisualRow(TEditor &ed, int targetRow, uint &lineStartPtr, int &subRow)
{
    if (targetRow < 0)
        targetRow = 0;
    int acc = 0;
    uint p = 0;
    while (p < ed.bufLen)
    {
        int rows = visualRowsForLine(ed, p);
        if (targetRow < acc + rows)
        {
            lineStartPtr = p;
            subRow = targetRow - acc;
            return;
        }
        acc += rows;
        p = ed.nextLine(p);
    }
    /* Fila por debajo del texto: no repetir el segmento 0 de la linea del cursor. */
    if( ed.bufLen == 0 )
        {
        lineStartPtr = 0;
        subRow = 0;
        return;
        }
    lineStartPtr = ed.bufLen;
    subRow = 0;
}

static void edWrapPtrToVisual(TEditor &ed, uint ls, uint le, int w, uint ptr,
                              int &vx, int &subRow)
{
    std::vector<std::pair<uint, uint>> segs;
    std::vector<EdWrapHole> holes;
    edBuildWrapSegments(ed, ls, le, w, segs, holes);

    if (ptr >= le && !segs.empty())
    {
        const int last = (int) segs.size() - 1;
        vx = min(ed.charPos(segs[last].first, le), w - 1);
        subRow = last;
        return;
    }

    for (const EdWrapHole &h : holes)
    {
        if (h.a <= ptr && ptr < h.b)
        {
            vx = min(max(0, h.snapVx), w - 1);
            subRow = max(0, h.snapSubRow);
            return;
        }
    }

    for (int i = 0; i < (int) segs.size(); ++i)
    {
        uint a = segs[i].first;
        uint b = segs[i].second;
        if (a <= ptr && ptr < b)
        {
            vx = min(ed.charPos(a, ptr), w - 1);
            subRow = i;
            return;
        }
        if (ptr == b && i + 1 < (int) segs.size() && ptr == segs[i + 1].first)
        {
            vx = 0;
            subRow = i + 1;
            return;
        }
    }

    if (!segs.empty())
    {
        const int last = (int) segs.size() - 1;
        uint a = segs[last].first;
        vx = min(ed.charPos(a, min(ptr, le)), w - 1);
        subRow = last;
        return;
    }
    vx = 0;
    subRow = 0;
}

static void visualCursorPos(TEditor &ed, int &vx, int &vy)
{
    int w = visualWrapWidth(ed);
    uint ls = ed.lineStart(ed.curPtr);
    uint le = ed.lineEnd(ls);
    int subRow = 0;
    edWrapPtrToVisual(ed, ls, le, w, ed.curPtr, vx, subRow);
    vy = 0;
    for (uint p = 0; p < ls; p = ed.nextLine(p))
        vy += visualRowsForLine(ed, p);
    vy += subRow;
}

static void formatLineRange(TDrawBuffer &b, TEditor &ed, uint linePtr, uint endPtr,
                            int hScroll, int width, TAttrPair colors)
{
    hScroll = max(hScroll, 0);
    width = max(width, 0);

    uint P = linePtr;
    int pos = 0;
    int x = 0;
    while (P < endPtr && P < ed.bufLen)
    {
        uint nextP = P;
        int nextPos = pos;
        ed.nextCharAndPos(nextP, nextPos);

        if (x > width || (x == width && pos < nextPos))
            break;

        char buf[maxCharSize];
        uint charLen = nextP - P;
        ed.getText(P, TSpan<char>(buf, charLen));

        if (buf[0] == '\r' || buf[0] == '\n')
            break;

        if (nextPos > hScroll)
        {
            TColorAttr color = (ed.selStart <= P && P < ed.selEnd) ? colors >> 8 : colors;
            int charWidth = nextPos - max(pos, hScroll);
            if (buf[0] == '\t' || pos < hScroll)
                b.moveChar(x, ' ', color, charWidth);
            else
                b.moveStr(x, TStringView(buf, charLen), color);

            x += charWidth;
        }

        P = nextP;
        pos = nextPos;
    }

    if (x < width)
    {
        TColorAttr colorAfter = (ed.selStart <= P && P < ed.selEnd) ? colors >> 8 : colors;
        b.moveChar(x, ' ', colorAfter, width - x);
    }
}

TEditor::TEditor( const TRect& bounds,
                  TScrollBar *aHScrollBar,
                  TScrollBar *aVScrollBar,
                  TIndicator *aIndicator,
                  uint aBufSize ) noexcept :
    TView( bounds ),
    hScrollBar( aHScrollBar ),
    vScrollBar( aVScrollBar ),
    indicator( aIndicator ),
    bufSize( aBufSize ),
    canUndo( True ),
    selecting( False ),
    overwrite( False ),
    autoIndent( True ) ,
    lockCount( 0 ),
    updateFlags( 0 ),
    keyState( 0 ),
    lineEndingType( defaultLineEndingType ),
    encoding( encDefault )
{
    growMode = gfGrowHiX | gfGrowHiY;
    options |= ofSelectable;
    eventMask = evMouseDown | evKeyDown | evCommand | evBroadcast;
    showCursor();
    initBuffer();
    if( buffer != 0 )
        isValid = True;
    else
    {
        editorDialog( edOutOfMemory );
        bufSize = 0;
        isValid = False;
    }
    setBufLen(0);
}

TEditor::~TEditor()
{
}

void TEditor::shutDown()
{
    doneBuffer();
    TView::shutDown();
}

void TEditor::changeBounds( const TRect& bounds )
{
    setBounds(bounds);
    if( kEditorSoftWrap )
        {
        delta.x = 0;
        int maxY = max(0, visualTotalRows(*this) - size.y);
        delta.y = max(0, min(delta.y, maxY));

        int vx, vy;
        visualCursorPos(*this, vx, vy);
        if( vy < delta.y )
            delta.y = vy;
        else if( vy >= delta.y + size.y )
            delta.y = max(0, vy - size.y + 1);
        }
    else
        {
        delta.x = max(0, min(delta.x, limit.x - size.x));
        delta.y = max(0, min(delta.y, limit.y - size.y));
        }
    update(ufView);
}

uint TEditor::getText( uint p, TSpan<char> dest )
{
    if( p < bufLen )
        {
        uint count = min((uint) dest.size(), bufLen - p);
        for (uint i = 0; i < count; ++i)
            dest[i] = bufChar(p + i);
        return count;
        }
    return 0;
}

Boolean TEditor::nextCharAndPos( uint &p, int &pos )
{
    if( p < bufLen )
        {
        if( encoding == TEditor::encSingleByte )
            {
            ++p;
            ++pos;
            }
        else
            {
            char buf[maxCharSize];
            uint count = getText(p, TSpan<char>(buf, maxCharSize));
            if( buf[0] == '\t' )
                {
                ++p;
                pos = (pos | 7) + 1;
                }
            else
                {
                size_t i = 0, w = 0;
                TText::next(TStringView(buf, count), i, w);
                p += i;
                pos += (int) w;
                }
            }
        return True;
        }
    return False;
}

int TEditor::charPos( uint p, uint target )
{
    int pos = 0;
    while( p < target )
        if( !nextCharAndPos(p, pos) )
            break;
    return pos;
}

uint TEditor::charPtr( uint p, int target )
{
    int pos = 0;
    uint prevP = p;
    while( p < bufLen && pos < target )
        {
        char c = bufChar(p);
        if( c == '\r' || c == '\n' )
            break;
        prevP = p;
        if( !nextCharAndPos(p, pos) )
            break;
        }
    if( pos > target )
        p = prevP;
    return p;
}

Boolean TEditor::clipCopy()
{
    Boolean res = False;
    if( clipboard != this )
        {
        if( clipboard != 0 )
            res = clipboard->insertFrom(this);
        else
            {
            TClipboard::setText( TStringView( buffer + bufPtr(selStart),
                                              selEnd - selStart ) );
            res = True;
            }
        selecting = False;
        update(ufUpdate);
        }
    return res;
}

void TEditor::clipCut()
{
    if( clipCopy() == True )
        deleteSelect();
}

void TEditor::clipPaste()
{
    if( clipboard != this )
        {
        if( clipboard != 0 )
            insertFrom(clipboard);
        else
            TClipboard::requestText();
        }
}

void TEditor::convertEvent( TEvent& event )
{
    if( event.what == evKeyDown )
        {
        if( (event.keyDown.controlKeyState & kbShift) != 0 &&
            event.keyDown.charScan.scanCode >= 0x47 &&
            event.keyDown.charScan.scanCode <= 0x51
          )
            event.keyDown.charScan.charCode = 0;

        ushort key = event.keyDown.keyCode;
        if( keyState != 0 )
            {
            if( (key & 0xFF) >= 0x01 && (key & 0xFF) <= 0x1A )
                key += 0x40;
            if( (key & 0xFF) >= 0x61 && (key & 0xFF) <= 0x7A )
                key -= 0x20;
            }
        key = scanKeyMap(keyMap[keyState], key);
        keyState = 0;
        if( key != 0 )
            {
            if( (key & 0xFF00) == 0xFF00 )
                {
                keyState = (key & 0xFF);
                clearEvent(event);
                }
            else
                {
                event.what = evCommand;
                event.message.command = key;
                }
            }
        }
}

Boolean TEditor::cursorVisible()
{
    if (kEditorSoftWrap)
    {
        int vx, vy;
        visualCursorPos(*this, vx, vy);
        return Boolean((vy >= delta.y) && (vy < delta.y + size.y));
    }
    return Boolean((curPos.y >= delta.y) && (curPos.y < delta.y + size.y));
}

void TEditor::deleteRange( uint startPtr,
                           uint endPtr,
                           Boolean delSelect
                         )
{
    if( hasSelection() == True && delSelect == True )
        deleteSelect();
    else
        {
        setSelect(curPtr, endPtr, True);
        deleteSelect();
        setSelect(startPtr, curPtr, False);
        deleteSelect();
        }
}

void TEditor::deleteSelect()
{
    insertText( 0, 0, False );
}

void TEditor::doneBuffer()
{
    delete[] buffer;
}

void TEditor::doSearchReplace()
{
    int i;
    do  {
        i = cmCancel;
        if( search(findStr, editorFlags) == False )
            {
            if( (editorFlags & (efReplaceAll | efDoReplace)) !=
                (efReplaceAll | efDoReplace) )
                    editorDialog( edSearchFailed );
            }
        else
            if( (editorFlags & efDoReplace) != 0 )
                {
                i = cmYes;
                if( (editorFlags & efPromptOnReplace) != 0 )
                    {
                    TPoint c = makeGlobal( cursor );
                    i = editorDialog( edReplacePrompt, &c );
                    }
                if( i == cmYes )
                    {
                    lock();
                    insertText( replaceStr, strlen(replaceStr), False);
                    trackCursor(False);
                    unlock();
                    }
                }
        } while( i != cmCancel && (editorFlags & efReplaceAll) != 0 );
}

void TEditor::doUpdate()
{
    if( updateFlags != 0 )
        {
        if( kEditorSoftWrap )
            {
            int vx, vy;
            visualCursorPos(*this, vx, vy);
            setCursor(vx, vy - delta.y);
            drawView();
            if( hScrollBar != 0 )
                hScrollBar->setParams(0, 0, 0, 1, 1);
            if( vScrollBar != 0 )
                {
                int totalRows = visualTotalRows(*this);
                vScrollBar->setParams(delta.y, 0, max(0, totalRows - size.y), max(1, size.y - 1), 1);
                }
            }
        else
            {
            setCursor(curPos.x - delta.x, curPos.y - delta.y);
            if( (updateFlags & ufView) != 0 )
                drawView();
            else
                if( (updateFlags & ufLine) != 0 )
                    drawLines( curPos.y-delta.y, 1, lineStart(curPtr) );
            if( hScrollBar != 0 )
                hScrollBar->setParams(delta.x, 0, limit.x - size.x, size.x / 2, 1);
            if( vScrollBar != 0 )
                vScrollBar->setParams(delta.y, 0, limit.y - size.y, size.y - 1, 1);
            }
        if( indicator != 0 )
            indicator->setValue(curPos, modified);
        if( (state & sfActive) != 0 )
            updateCommands();
        updateFlags = 0;
        }
}

void TEditor::draw()
{
    if( kEditorSoftWrap )
        {
        drawLines(0, size.y, 0);
        return;
        }
    if( drawLine != delta.y )
        {
        drawPtr = lineMove( drawPtr, delta.y - drawLine );
        drawLine = delta.y;
        }
    drawLines( 0, size.y, drawPtr );
}

void TEditor::drawLines( int y, int count, uint linePtr )
{
    TDrawBuffer b;
    TAttrPair color = getColor(0x0201);
    if( kEditorSoftWrap )
        {
        const int ww = visualWrapWidth(*this);
        while( count-- > 0 )
            {
            uint ls;
            int subRow;
            locateVisualRow(*this, delta.y + y, ls, subRow);
            if( ls >= bufLen && bufLen > 0 )
                {
                formatLineRange(b, *this, bufLen, bufLen, 0, size.x, color);
                writeBuf(0, y, size.x, 1, b);
                y++;
                continue;
                }
            uint le = lineEnd(ls);
            std::vector<std::pair<uint, uint>> segs;
            std::vector<EdWrapHole> holes;
            edBuildWrapSegments(*this, ls, le, ww, segs, holes);
            if( subRow < 0 )
                subRow = 0;
            if( subRow >= (int) segs.size() )
                subRow = (int) segs.size() - 1;
            const uint segA = segs[(size_t) subRow].first;
            const uint segB = segs[(size_t) subRow].second;
            formatLineRange(b, *this, segA, segB, 0, size.x, color);
            writeBuf(0, y, size.x, 1, b);
            y++;
            }
        return;
        }
    while( count-- > 0 )
        {
        formatLine( b, linePtr, delta.x, size.x, color );
        writeBuf( 0, y, size.x, 1, b );
        linePtr = nextLine(linePtr);
        y++;
        }
}

void TEditor::find()
{
    TFindDialogRec findRec( findStr, editorFlags );
    if( editorDialog( edFind, &findRec ) != cmCancel )
        {
        strcpy( findStr, findRec.find );
        editorFlags = findRec.options & ~efDoReplace;
        doSearchReplace();
        }
}

uint TEditor::getMousePtr( TPoint m )
{
    TPoint mouse = makeLocal( m );
    mouse.x = max(0, min(mouse.x, size.x - 1));
    mouse.y = max(0, min(mouse.y, size.y - 1));
    if( kEditorSoftWrap )
        {
        uint ls;
        int subRow;
        locateVisualRow(*this, delta.y + mouse.y, ls, subRow);
        if( ls >= bufLen && bufLen > 0 )
            return bufLen;
        const int ww = visualWrapWidth(*this);
        uint le = lineEnd(ls);
        std::vector<std::pair<uint, uint>> segs;
        std::vector<EdWrapHole> holes;
        edBuildWrapSegments(*this, ls, le, ww, segs, holes);
        if( subRow < 0 )
            subRow = 0;
        if( subRow >= (int) segs.size() )
            subRow = (int) segs.size() - 1;
        const uint segA = segs[(size_t) subRow].first;
        const uint segB = segs[(size_t) subRow].second;
        const int segW = charPos(segA, segB);
        const int mx = min(mouse.x, max(0, segW));
        return charPtr(segA, mx);
        }
    return charPtr(lineMove(drawPtr, mouse.y + delta.y - drawLine),
        mouse.x + delta.x);
}

TPalette& TEditor::getPalette() const
{
    static TPalette palette( cpEditor, sizeof( cpEditor )-1 );
    return palette;
}

void TEditor::checkScrollBar( const TEvent& event,
                              TScrollBar *p,
                              int& d
                            )
{
    if( (event.message.infoPtr == p) && (p->value != d) )
        {
        d = p->value;
        update( ufView );
        }
}

void TEditor::handleEvent( TEvent& event )
{
    TView::handleEvent( event );

    Boolean centerCursor = Boolean(!cursorVisible());
    uchar selectMode = 0;

    if( selecting == True ||
        (event.what & evMouse && (event.mouse.controlKeyState & kbShift) != 0) ||
        (event.what & evKeyboard && (event.keyDown.controlKeyState & kbShift ) != 0)
      )
        selectMode = smExtend;

    convertEvent( event );

    switch( event.what )
        {

        case evMouseDown:
            if( event.mouse.buttons & mbRightButton )
                {
                TMenuItem &menu = initContextMenu( event.mouse.where );
                popupMenu( event.mouse.where, menu, owner );
                break;
                }

            if( event.mouse.buttons & mbMiddleButton )
                {
                TPoint lastMouse = makeLocal( event.mouse.where );
                while( mouseEvent(event, evMouse) )
                    {
                    TPoint mouse = makeLocal( event.mouse.where );
                    TPoint d = delta + (lastMouse - mouse);
                    scrollTo(d.x, d.y);
                    lastMouse = mouse;
                    }
                break;
                }

            if( event.mouse.eventFlags & meDoubleClick )
                selectMode |= smDouble;
            else if( event.mouse.eventFlags & meTripleClick )
                selectMode |= smTriple;

            do  {
                lock();
                if( event.what == evMouseAuto )
                    {
                    TPoint mouse = makeLocal( event.mouse.where );
                    TPoint d = delta;
                    if( mouse.x < 0 )
                        d.x--;
                    if( mouse.x >= size.x )
                        d.x++;
                    if( mouse.y < 0 )
                        d.y--;
                    if( mouse.y >= size.y )
                        d.y++;
                    scrollTo(d.x, d.y);
                    }
                else if( event.what == evMouseWheel )
                    {
                    TEvent ev = event;
                    vScrollBar->handleEvent(ev);
                    hScrollBar->handleEvent(ev);
                    }
                setCurPtr(getMousePtr(event.mouse.where), selectMode);
                selectMode |= smExtend;
                unlock();
                } while( mouseEvent(event, evMouseMove + evMouseAuto + evMouseWheel) );
            break;

        case evKeyDown:
            if( ( encoding != encSingleByte && event.keyDown.textLength > 0 ) ||
                event.keyDown.charScan.charCode == 9 ||
                ( event.keyDown.charScan.charCode >= 32 && event.keyDown.charScan.charCode < 255 )
              )
                {
                lock();
                if( event.keyDown.controlKeyState & kbPaste )
                    {
                    char buf[512];
                    size_t length;
                    while( textEvent( event, TSpan<char>(buf, sizeof(buf)), length ) )
                        insertText( buf, (uint) length, False );
                    }
                else
                    {
                    if( overwrite == True && hasSelection() == False )
                        if( curPtr != lineEnd(curPtr) )
                            selEnd = nextChar(curPtr);

                    if( encoding != encSingleByte && event.keyDown.textLength > 0 )
                        insertText( event.keyDown.text, event.keyDown.textLength, False );
                    else
                        insertText( &event.keyDown.charScan.charCode, 1, False );
                    }

                trackCursor(centerCursor);
                unlock();
                }
            else
                return;
            break;

        case evCommand:
            switch( event.message.command )
                {
                case cmFind:
                    find();
                    break;
                case cmReplace:
                    replace();
                    break;
                case cmSearchAgain:
                    doSearchReplace();
                    break;
                case cmEncoding:
                    toggleEncoding();
                    break;
                default:
                    lock();
                    switch( event.message.command )
                        {
                        case cmCut:
                            clipCut();
                            break;
                        case cmCopy:
                            clipCopy();
                            break;
                        case cmPaste:
                            clipPaste();
                            break;
                        case cmUndo:
                            undo();
                            break;
                        case cmClear:
                            deleteSelect();
                            break;
                        case cmCharLeft:
                            setCurPtr(prevChar(curPtr), selectMode);
                            break;
                        case cmCharRight:
                            setCurPtr(nextChar(curPtr), selectMode);
                            break;
                        case cmWordLeft:
                            setCurPtr(prevWord(curPtr), selectMode);
                            break;
                        case cmWordRight:
                            setCurPtr(nextWord(curPtr), selectMode);
                            break;
                        case cmLineStart:
                            setCurPtr(autoIndent ? indentedLineStart(curPtr) : lineStart(curPtr), selectMode);
                            break;
                        case cmLineEnd:
                            setCurPtr(lineEnd(curPtr), selectMode);
                            break;
                        case cmLineUp:
                            setCurPtr(lineMove(curPtr, -1), selectMode);
                            break;
                        case cmLineDown:
                            setCurPtr(lineMove(curPtr, 1), selectMode);
                            break;
                        case cmPageUp:
                            setCurPtr(lineMove(curPtr, -(size.y-1)), selectMode);
                            break;
                        case cmPageDown:
                            setCurPtr(lineMove(curPtr, size.y-1), selectMode);
                            break;
                        case cmTextStart:
                            setCurPtr(0, selectMode);
                            break;
                        case cmTextEnd:
                            setCurPtr(bufLen, selectMode);
                            break;
                        case cmNewLine:
                            newLine();
                            break;
                        case cmBackSpace:
                            deleteRange(prevChar(curPtr), curPtr, True);
                            break;
                        case cmDelChar:
                            deleteRange(curPtr, nextChar(curPtr), True);
                            break;
                        case cmDelWord:
                            deleteRange(curPtr, nextWord(curPtr), False);
                            break;
                        case cmDelWordLeft:
                            deleteRange(prevWord(curPtr), curPtr, False);
                            break;
                        case cmDelStart:
                            deleteRange(lineStart(curPtr), curPtr, False);
                            break;
                        case cmDelEnd:
                            deleteRange(curPtr, lineEnd(curPtr), False);
                            break;
                        case cmDelLine:
                            deleteRange(lineStart(curPtr), nextLine(curPtr), False);
                            break;
                        case cmInsMode:
                            toggleInsMode();
                            break;
                        case cmStartSelect:
                            startSelect();
                            break;
                        case cmHideSelect:
                            hideSelect();
                            break;
                        case cmIndentMode:
                            autoIndent = Boolean(!autoIndent);
                            break;
                        case cmSelectAll:
                            setCurPtr(0, selectMode);
                            selectMode |= smExtend;
                            setCurPtr(bufLen, selectMode);
                            break;
                        default:
                            unlock();
                            return;
                        }
                    trackCursor(centerCursor);
                    unlock();
                    break;
                }
            break;

        case evBroadcast:
            switch( event.message.command )
                {
                case cmScrollBarChanged:
                    if ((event.message.infoPtr == hScrollBar) ||
                        (event.message.infoPtr == vScrollBar))
                        {
                        checkScrollBar( event, hScrollBar, delta.x );
                        checkScrollBar( event, vScrollBar, delta.y );
                        }
                    else
                        return;
                    break;
                default:
                    return;
                }
            break;
        }
    clearEvent(event);
}

