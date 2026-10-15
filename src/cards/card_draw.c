/*******************************************************************************
 * card_draw.c — Draw one card face into a trim rectangle
 ******************************************************************************/
#include "card_draw.h"

#include <math.h>
#include <string.h>

#define CARD_LINE_FACTOR 1.15f
#define CARD_TEXT_SPACING 1.0f
#define CARD_MAX_LINES 12

typedef struct CardTextLine {
    int start;
    int length;
} CardTextLine;

Font CardChooseFont(const Font *fonts, int fontCount, int fontId)
{
    if (fonts && fontCount > 0) {
        if (fontId < 0 || fontId >= fontCount) fontId = 0;
        if (fonts[fontId].texture.id != 0) return fonts[fontId];
        if (fonts[0].texture.id != 0) return fonts[0];
    }
    return GetFontDefault();
}

static float MeasureRange(Font font, const char *text, int start, int count, float fontPx)
{
    if (count <= 0 || !text) return 0.0f;
    if (start < 0) start = 0;
    char buf[CARD_TEXT_MAX];
    if (count >= CARD_TEXT_MAX) count = CARD_TEXT_MAX - 1;
    memcpy(buf, text + start, (size_t)count);
    buf[count] = '\0';
    return MeasureTextEx(font, buf, fontPx, CARD_TEXT_SPACING).x;
}

static int LayoutText(Font font, const char *text, float fontPx, float maxWidth,
                      CardTextLine *lines, int maxLines)
{
    if (!text || maxLines <= 0) return 0;
    if (maxWidth < 1.0f) maxWidth = 1.0f;
    int len = (int)strlen(text);
    if (len == 0) {
        lines[0].start = 0;
        lines[0].length = 0;
        return 1;
    }

    int count = 0;
    int i = 0;
    while (i < len && count < maxLines) {
        int guard = i;
        if (text[i] == '\n') {
            lines[count].start = i;
            lines[count].length = 0;
            count++;
            i++;
            continue;
        }

        int start = i;
        int lastSpace = -1;
        int j = i;
        while (j < len && text[j] != '\n') {
            if (text[j] == ' ') lastSpace = j;
            float width = MeasureRange(font, text, start, j - start + 1, fontPx);
            if (width > maxWidth && j > start) break;
            j++;
        }

        if (j >= len || text[j] == '\n') {
            lines[count].start = start;
            lines[count].length = j - start;
            count++;
            i = j;
            if (i < len && text[i] == '\n') i++;
        } else if (lastSpace >= start) {
            lines[count].start = start;
            lines[count].length = lastSpace - start;
            count++;
            i = lastSpace + 1;
        } else {
            int take = j - start;
            if (take < 1) take = 1;
            lines[count].start = start;
            lines[count].length = take;
            count++;
            i = start + take;
        }

        if (i <= guard) i = guard + 1;
    }
    return count;
}

void CardTextCaretPx(Font font, const char *text, int caret, float fontPx,
                     float boxWidth, int align, Rectangle boxPx,
                     float *outX, float *outY, float *outH)
{
    CardTextLine lines[CARD_MAX_LINES];
    int n = LayoutText(font, text ? text : "", fontPx, boxWidth, lines, CARD_MAX_LINES);
    float lineH = fontPx * CARD_LINE_FACTOR;
    if (n <= 0) {
        *outX = boxPx.x;
        *outY = boxPx.y;
        *outH = fontPx;
        return;
    }

    int len = text ? (int)strlen(text) : 0;
    if (caret < 0) caret = 0;
    if (caret > len) caret = len;

    int lineIndex = n - 1;
    for (int i = 0; i < n; i++) {
        int a = lines[i].start;
        int b = a + lines[i].length;
        if (caret >= a && caret <= b) {
            lineIndex = i;
            break;
        }
    }

    float lineWidth = MeasureRange(font, text, lines[lineIndex].start, lines[lineIndex].length, fontPx);
    int local = caret - lines[lineIndex].start;
    if (local < 0) local = 0;
    if (local > lines[lineIndex].length) local = lines[lineIndex].length;
    float before = MeasureRange(font, text, lines[lineIndex].start, local, fontPx);

    float lineX = boxPx.x;
    if (align == CARD_ALIGN_CENTER) lineX += (boxPx.width - lineWidth) * 0.5f;
    else if (align == CARD_ALIGN_RIGHT) lineX += boxPx.width - lineWidth;

    *outX = lineX + before;
    *outY = boxPx.y + (float)lineIndex * lineH;
    *outH = fontPx;
}

static Rectangle IntersectRect(Rectangle a, Rectangle b)
{
    float x1 = a.x > b.x ? a.x : b.x;
    float y1 = a.y > b.y ? a.y : b.y;
    float x2 = (a.x + a.width) < (b.x + b.width) ? (a.x + a.width) : (b.x + b.width);
    float y2 = (a.y + a.height) < (b.y + b.height) ? (a.y + a.height) : (b.y + b.height);
    Rectangle r = { x1, y1, x2 - x1, y2 - y1 };
    if (r.width < 0) r.width = 0;
    if (r.height < 0) r.height = 0;
    return r;
}

static void Scissor(Rectangle r)
{
    if (r.width < 1.0f || r.height < 1.0f) {
        BeginScissorMode(0, 0, 1, 1);
        return;
    }
    BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);
}

static void DrawDashedLine(Vector2 a, Vector2 b, Color color)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) return;
    float ux = dx / len;
    float uy = dy / len;
    for (float pos = 0.0f; pos < len; pos += 11.0f) {
        float end = pos + 7.0f;
        if (end > len) end = len;
        DrawLineEx((Vector2){ a.x + ux * pos, a.y + uy * pos },
                   (Vector2){ a.x + ux * end, a.y + uy * end },
                   1.0f, color);
    }
}

static void DrawDashedRect(Rectangle r, Color color)
{
    Vector2 tl = { r.x, r.y };
    Vector2 tr = { r.x + r.width, r.y };
    Vector2 bl = { r.x, r.y + r.height };
    Vector2 br = { r.x + r.width, r.y + r.height };
    DrawDashedLine(tl, tr, color);
    DrawDashedLine(tr, br, color);
    DrawDashedLine(br, bl, color);
    DrawDashedLine(bl, tl, color);
}

static void DrawTextElement(const CardElement *elem, Rectangle screen, Font font, float fontPx, Rectangle clip)
{
    Rectangle local = IntersectRect(screen, clip);
    Scissor(local);

    CardTextLine lines[CARD_MAX_LINES];
    int n = LayoutText(font, elem->text, fontPx, screen.width, lines, CARD_MAX_LINES);
    float lineH = fontPx * CARD_LINE_FACTOR;
    char buf[CARD_TEXT_MAX];

    for (int i = 0; i < n; i++) {
        if (lines[i].length <= 0) continue;
        int count = lines[i].length;
        if (count >= CARD_TEXT_MAX) count = CARD_TEXT_MAX - 1;
        memcpy(buf, elem->text + lines[i].start, (size_t)count);
        buf[count] = '\0';
        float lineWidth = MeasureTextEx(font, buf, fontPx, CARD_TEXT_SPACING).x;
        float x = screen.x;
        if (elem->align == CARD_ALIGN_CENTER) x += (screen.width - lineWidth) * 0.5f;
        else if (elem->align == CARD_ALIGN_RIGHT) x += screen.width - lineWidth;
        DrawTextEx(font, buf, (Vector2){ x, screen.y + (float)i * lineH },
                   fontPx, CARD_TEXT_SPACING, elem->color);
    }
}

static void DrawImageElement(const CardElement *elem, Rectangle screen, Font font, Rectangle clip)
{
    Scissor(clip);
    if (elem->hasImage && elem->texture.id != 0 && elem->texture.width > 0 && elem->texture.height > 0) {
        float iw = (float)elem->texture.width;
        float ih = (float)elem->texture.height;
        float scale = screen.width / iw;
        float scaleY = screen.height / ih;
        if (scaleY < scale) scale = scaleY;
        float dw = iw * scale;
        float dh = ih * scale;
        Rectangle dest = {
            screen.x + (screen.width - dw) * 0.5f,
            screen.y + (screen.height - dh) * 0.5f,
            dw,
            dh
        };
        Rectangle src = { 0.0f, 0.0f, iw, ih };
        DrawTexturePro(elem->texture, src, dest, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
        return;
    }

    DrawRectangleRec(screen, (Color){ 236, 232, 228, 255 });
    DrawRectangleLinesEx(screen, 1.0f, (Color){ 176, 84, 84, 255 });
    const char *label = "Missing image";
    float size = 14.0f;
    if (screen.height < 28.0f) size = screen.height * 0.45f;
    Vector2 ts = MeasureTextEx(font, label, size, 0.5f);
    DrawTextEx(font, label,
               (Vector2){ screen.x + (screen.width - ts.x) * 0.5f,
                          screen.y + (screen.height - ts.y) * 0.5f },
               size, 0.5f, (Color){ 120, 64, 64, 255 });
}

void CardDrawFace(const CardFace *face, Rectangle trimPx, Rectangle clipPx,
                  const Font *fonts, int fontCount, bool bleed, bool guides)
{
    if (!face || trimPx.width < 1.0f || trimPx.height < 1.0f) return;

    float pxPerInch = trimPx.width / CARD_W_IN;
    Rectangle bg = trimPx;
    if (bleed) {
        float pad = CARD_BLEED_IN * pxPerInch;
        bg.x -= pad;
        bg.y -= pad;
        bg.width += pad * 2.0f;
        bg.height += pad * 2.0f;
    }

    Scissor(clipPx);
    DrawRectangleRec(bg, face->background);

    Rectangle contentClip = IntersectRect(clipPx, bg);
    for (int i = 0; i < face->count; i++) {
        const CardElement *elem = &face->items[i];
        Rectangle screen = {
            trimPx.x + elem->box.x * pxPerInch,
            trimPx.y + elem->box.y * pxPerInch,
            elem->box.width * pxPerInch,
            elem->box.height * pxPerInch
        };
        if (screen.width < 0.5f || screen.height < 0.5f) continue;

        if (elem->kind == CARD_ELEM_TEXT) {
            Font font = CardChooseFont(fonts, fontCount, elem->fontId);
            float fontPx = elem->fontSize * (pxPerInch / 72.0f);
            if (fontPx < 1.0f) fontPx = 1.0f;
            DrawTextElement(elem, screen, font, fontPx, contentClip);
        } else {
            Font font = CardChooseFont(fonts, fontCount, 0);
            DrawImageElement(elem, screen, font, contentClip);
        }
    }

    if (guides) {
        Scissor(clipPx);
        if (bleed) DrawDashedRect(bg, (Color){ 186, 72, 72, 220 });
        DrawRectangleLinesEx(trimPx, 1.25f, (Color){ 40, 44, 52, 230 });
        float inset = CARD_SAFE_IN * pxPerInch;
        Rectangle safe = {
            trimPx.x + inset,
            trimPx.y + inset,
            trimPx.width - inset * 2.0f,
            trimPx.height - inset * 2.0f
        };
        if (safe.width > 4.0f && safe.height > 4.0f) {
            DrawDashedRect(safe, (Color){ 48, 108, 168, 220 });
        }
    }

    EndScissorMode();
}
