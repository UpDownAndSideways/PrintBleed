/*******************************************************************************
 * card_doc.c — Business card document, templates, undo, and project files
 ******************************************************************************/
#include "card_doc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TEMPLATE_NAMES[CARD_TEMPLATE_COUNT] = {
    "Classic", "Centered", "Color bar", "Minimal", "Blank"
};

static const char *SIDE_NAMES[CARD_SIDE_COUNT] = { "Front", "Back" };

const char *CardTemplateName(int templateId)
{
    if (templateId < 0 || templateId >= CARD_TEMPLATE_COUNT) return "Template";
    return TEMPLATE_NAMES[templateId];
}

const char *CardSideName(int side)
{
    if (side < 0 || side >= CARD_SIDE_COUNT) return "Card";
    return SIDE_NAMES[side];
}

static int HexNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static Color ParseHex(const char *s)
{
    Color c = { 0, 0, 0, 255 };
    if (!s || strlen(s) < 6) return c;
    c.r = (unsigned char)(HexNibble(s[0]) * 16 + HexNibble(s[1]));
    c.g = (unsigned char)(HexNibble(s[2]) * 16 + HexNibble(s[3]));
    c.b = (unsigned char)(HexNibble(s[4]) * 16 + HexNibble(s[5]));
    return c;
}

static void LimitImageSize(Image *img)
{
    const int maxSide = 4096;
    if (!img || !img->data) return;
    if (img->width <= maxSide && img->height <= maxSide) return;
    int longest = img->width > img->height ? img->width : img->height;
    float scale = (float)maxSide / (float)longest;
    int w = (int)(img->width * scale);
    int h = (int)(img->height * scale);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    ImageResize(img, w, h);
}

static void CardElementRelease(CardElement *elem)
{
    if (elem->texture.id != 0) UnloadTexture(elem->texture);
    elem->texture = (Texture2D){ 0 };
    if (elem->image.data != NULL) UnloadImage(elem->image);
    elem->image = (Image){ 0 };
    elem->hasImage = false;
}

static bool CardElementAssign(CardElement *elem, Image img, const char *path)
{
    if (img.data == NULL) return false;
    LimitImageSize(&img);
    CardElementRelease(elem);
    elem->kind = CARD_ELEM_IMAGE;
    elem->hasImage = true;
    elem->image = img;
    snprintf(elem->path, CARD_PATH_MAX, "%s", path ? path : "");
    elem->texture = LoadTextureFromImage(elem->image);
    if (elem->texture.id != 0) {
        SetTextureFilter(elem->texture, TEXTURE_FILTER_BILINEAR);
    }
    return true;
}

static bool CardLoadMark(CardElement *elem, const char *path)
{
    const char *lettersBegin = path + 5;
    const char *sep1 = strchr(lettersBegin, ':');
    if (!sep1) return false;
    const char *sep2 = strchr(sep1 + 1, ':');
    if (!sep2) return false;

    char letters[4];
    memset(letters, 0, sizeof(letters));
    size_t letterCount = (size_t)(sep1 - lettersBegin);
    if (letterCount > 3) letterCount = 3;
    memcpy(letters, lettersBegin, letterCount);

    char fgHex[8];
    char bgHex[8];
    memset(fgHex, 0, sizeof(fgHex));
    memset(bgHex, 0, sizeof(bgHex));
    size_t fgCount = (size_t)(sep2 - (sep1 + 1));
    if (fgCount > 6) fgCount = 6;
    memcpy(fgHex, sep1 + 1, fgCount);
    snprintf(bgHex, sizeof(bgHex), "%.6s", sep2 + 1);

    Color fg = ParseHex(fgHex);
    Color bg = ParseHex(bgHex);
    Image img = GenImageColor(256, 256, bg);
    if (img.data == NULL) return false;
    ImageDrawCircle(&img, 128, 128, 112, fg);
    ImageDrawCircle(&img, 128, 128, 96, bg);
    int textWidth = MeasureText(letters, 40);
    ImageDrawText(&img, letters, 128 - textWidth / 2, 108, 40, fg);
    return CardElementAssign(elem, img, path);
}

static bool CardLoadAsset(CardElement *elem, const char *path)
{
    if (!path || !path[0]) return false;
    if (strncmp(path, "solid:#", 7) == 0) {
        Image img = GenImageColor(16, 16, ParseHex(path + 7));
        return CardElementAssign(elem, img, path);
    }
    if (strncmp(path, "mark:", 5) == 0) return CardLoadMark(elem, path);
    if (!FileExists(path)) return false;
    Image img = LoadImage(path);
    if (img.data == NULL) return false;
    return CardElementAssign(elem, img, path);
}

static void CardElementClone(CardElement *dst, const CardElement *src, bool upload)
{
    *dst = *src;
    dst->image = (Image){ 0 };
    dst->texture = (Texture2D){ 0 };
    dst->hasImage = false;
    if (src->hasImage && src->image.data != NULL) {
        Image copy = ImageCopy(src->image);
        if (copy.data != NULL) {
            dst->image = copy;
            dst->hasImage = true;
            if (upload) {
                dst->texture = LoadTextureFromImage(dst->image);
                if (dst->texture.id != 0) {
                    SetTextureFilter(dst->texture, TEXTURE_FILTER_BILINEAR);
                }
            }
        }
    }
}

static void CardDocClone(CardDoc *dst, const CardDoc *src, bool upload)
{
    memset(dst, 0, sizeof(*dst));
    dst->activeSide = src->activeSide;
    dst->selected = src->selected;
    for (int side = 0; side < CARD_SIDE_COUNT; side++) {
        dst->faces[side].background = src->faces[side].background;
        dst->faces[side].count = src->faces[side].count;
        if (dst->faces[side].count > CARD_MAX_ELEMENTS) dst->faces[side].count = CARD_MAX_ELEMENTS;
        if (dst->faces[side].count < 0) dst->faces[side].count = 0;
        for (int i = 0; i < dst->faces[side].count; i++) {
            CardElementClone(&dst->faces[side].items[i], &src->faces[side].items[i], upload);
        }
    }
}

static CardDoc *CardDocHeapClone(const CardDoc *src, bool upload)
{
    CardDoc *doc = (CardDoc *)calloc(1, sizeof(CardDoc));
    if (!doc) return NULL;
    CardDocClone(doc, src, upload);
    return doc;
}

static void CardDocDestroy(CardDoc *doc)
{
    if (!doc) return;
    CardDocFree(doc);
    free(doc);
}

void CardDocFree(CardDoc *doc)
{
    if (!doc) return;
    for (int side = 0; side < CARD_SIDE_COUNT; side++) {
        for (int i = 0; i < doc->faces[side].count; i++) {
            CardElementRelease(&doc->faces[side].items[i]);
        }
        doc->faces[side].count = 0;
    }
}

static void CardFaceClear(CardFace *face)
{
    for (int i = 0; i < face->count; i++) CardElementRelease(&face->items[i]);
    Color bg = face->background;
    memset(face, 0, sizeof(*face));
    face->background = bg;
}

int CardFaceAddText(CardFace *face, Rectangle box, const char *text,
                    float pt, Color color, int align, int fontId)
{
    if (!face || face->count >= CARD_MAX_ELEMENTS) return -1;
    CardElement *elem = &face->items[face->count];
    memset(elem, 0, sizeof(*elem));
    elem->kind = CARD_ELEM_TEXT;
    elem->box = box;
    snprintf(elem->text, CARD_TEXT_MAX, "%s", text ? text : "");
    elem->fontSize = pt;
    elem->color = color;
    if (align < CARD_ALIGN_LEFT || align > CARD_ALIGN_RIGHT) align = CARD_ALIGN_LEFT;
    elem->align = (CardAlign)align;
    elem->fontId = fontId == 1 ? 1 : 0;
    return face->count++;
}

int CardFaceAddImageFile(CardFace *face, const char *path, Rectangle box)
{
    if (!face || face->count >= CARD_MAX_ELEMENTS || !path) return -1;
    CardElement *elem = &face->items[face->count];
    memset(elem, 0, sizeof(*elem));
    elem->box = box;
    if (!CardLoadAsset(elem, path)) {
        CardElementRelease(elem);
        memset(elem, 0, sizeof(*elem));
        return -1;
    }
    elem->box = box;
    return face->count++;
}

int CardFaceAddImageFit(CardFace *face, const char *path)
{
    if (!face || face->count >= CARD_MAX_ELEMENTS || !path) return -1;
    CardElement *elem = &face->items[face->count];
    memset(elem, 0, sizeof(*elem));
    if (!CardLoadAsset(elem, path)) {
        CardElementRelease(elem);
        memset(elem, 0, sizeof(*elem));
        return -1;
    }
    float aspect = 1.0f;
    if (elem->image.height > 0) {
        aspect = (float)elem->image.width / (float)elem->image.height;
    }
    float h = 1.15f;
    float w = h * aspect;
    if (w > 1.8f) {
        w = 1.8f;
        h = w / aspect;
    }
    if (h > 1.5f) {
        h = 1.5f;
        w = h * aspect;
    }
    elem->box = (Rectangle){
        (CARD_W_IN - w) * 0.5f,
        (CARD_H_IN - h) * 0.5f,
        w,
        h
    };
    return face->count++;
}

bool CardElementReplaceFile(CardElement *elem, const char *path)
{
    if (!elem || !path) return false;
    Rectangle box = elem->box;
    CardElement loaded;
    memset(&loaded, 0, sizeof(loaded));
    if (!CardLoadAsset(&loaded, path)) return false;
    CardElementRelease(elem);
    *elem = loaded;
    elem->box = box;
    elem->kind = CARD_ELEM_IMAGE;
    return true;
}

void CardFaceDelete(CardFace *face, int index)
{
    if (!face || index < 0 || index >= face->count) return;
    CardElementRelease(&face->items[index]);
    for (int i = index; i < face->count - 1; i++) {
        face->items[i] = face->items[i + 1];
    }
    face->count--;
    memset(&face->items[face->count], 0, sizeof(CardElement));
}

int CardFaceHitTest(const CardFace *face, float x, float y)
{
    if (!face) return -1;
    for (int i = face->count - 1; i >= 0; i--) {
        Rectangle b = face->items[i].box;
        if (x >= b.x && y >= b.y && x <= b.x + b.width && y <= b.y + b.height) return i;
    }
    return -1;
}

void CardElementClamp(CardElement *elem)
{
    if (!elem) return;
    const float minS = 0.18f;
    if (elem->box.width < minS) elem->box.width = minS;
    if (elem->box.height < minS) elem->box.height = minS;

    const float limitR = CARD_W_IN + CARD_BLEED_IN;
    const float limitB = CARD_H_IN + CARD_BLEED_IN;
    if (elem->box.x < -CARD_BLEED_IN) elem->box.x = -CARD_BLEED_IN;
    if (elem->box.y < -CARD_BLEED_IN) elem->box.y = -CARD_BLEED_IN;
    if (elem->box.x + elem->box.width > limitR) {
        elem->box.x = limitR - elem->box.width;
    }
    if (elem->box.y + elem->box.height > limitB) {
        elem->box.y = limitB - elem->box.height;
    }
    if (elem->box.x < -CARD_BLEED_IN) elem->box.x = -CARD_BLEED_IN;
    if (elem->box.y < -CARD_BLEED_IN) elem->box.y = -CARD_BLEED_IN;
}

void CardTextInsert(CardElement *elem, int *caret, const char *utf8)
{
    if (!elem || !caret || !utf8 || !utf8[0]) return;
    size_t n = strlen(elem->text);
    size_t add = strlen(utf8);
    if (*caret < 0) *caret = 0;
    if (*caret > (int)n) *caret = (int)n;
    if (n + add >= CARD_TEXT_MAX) return;
    memmove(elem->text + *caret + add, elem->text + *caret, n - (size_t)(*caret) + 1);
    memcpy(elem->text + *caret, utf8, add);
    *caret += (int)add;
}

void CardTextErase(CardElement *elem, int *caret, bool backspace)
{
    if (!elem || !caret) return;
    size_t n = strlen(elem->text);
    if (*caret < 0) *caret = 0;
    if (*caret > (int)n) *caret = (int)n;
    if (backspace) {
        if (*caret <= 0) return;
        memmove(elem->text + *caret - 1, elem->text + *caret, n - (size_t)(*caret) + 1);
        (*caret)--;
    } else if (*caret < (int)n) {
        memmove(elem->text + *caret, elem->text + *caret + 1, n - (size_t)(*caret));
    }
}

static void AddSolid(CardFace *face, Rectangle box, Color color)
{
    char path[32];
    snprintf(path, sizeof(path), "solid:#%02X%02X%02X", color.r, color.g, color.b);
    CardFaceAddImageFile(face, path, box);
}

static void AddMark(CardFace *face, Rectangle box, const char *letters, Color fg, Color bg)
{
    char path[64];
    snprintf(path, sizeof(path), "mark:%s:%02X%02X%02X:%02X%02X%02X",
             letters, fg.r, fg.g, fg.b, bg.r, bg.g, bg.b);
    CardFaceAddImageFile(face, path, box);
}

void CardApplyTemplate(CardDoc *doc, int side, int templateId)
{
    if (!doc || side < 0 || side >= CARD_SIDE_COUNT) return;
    CardFace *face = &doc->faces[side];
    CardFaceClear(face);
    doc->selected = -1;

    const Color ivory = { 248, 244, 236, 255 };
    const Color ink = { 32, 36, 42, 255 };
    const Color teal = { 14, 90, 92, 255 };
    const Color slate = { 90, 98, 108, 255 };
    const Color navy = { 20, 44, 66, 255 };
    const Color gold = { 196, 154, 78, 255 };
    const Color mist = { 232, 236, 240, 255 };
    const Color white = { 255, 255, 255, 255 };

    switch (templateId) {
    case CARD_TEMPLATE_CLASSIC:
        face->background = ivory;
        AddMark(face, (Rectangle){ 0.22f, 0.64f, 0.72f, 0.72f }, "AM", teal, ivory);
        CardFaceAddText(face, (Rectangle){ 1.12f, 0.36f, 2.15f, 0.36f },
                        "Alex Morgan", 18, ink, CARD_ALIGN_LEFT, 0);
        CardFaceAddText(face, (Rectangle){ 1.12f, 0.74f, 2.15f, 0.26f },
                        "Studio Director", 11, teal, CARD_ALIGN_LEFT, 0);
        CardFaceAddText(face, (Rectangle){ 1.12f, 1.06f, 2.15f, 0.24f },
                        "Northwind Studio", 10, slate, CARD_ALIGN_LEFT, 0);
        CardFaceAddText(face, (Rectangle){ 1.12f, 1.38f, 2.15f, 0.22f },
                        "555 014 2281", 9, ink, CARD_ALIGN_LEFT, 0);
        CardFaceAddText(face, (Rectangle){ 1.12f, 1.60f, 2.15f, 0.22f },
                        "alex@northwind.studio", 9, ink, CARD_ALIGN_LEFT, 0);
        break;
    case CARD_TEMPLATE_CENTERED:
        face->background = navy;
        CardFaceAddText(face, (Rectangle){ 0.2f, 0.46f, 3.1f, 0.42f },
                        "Alex Morgan", 22, white, CARD_ALIGN_CENTER, 1);
        CardFaceAddText(face, (Rectangle){ 0.2f, 0.94f, 3.1f, 0.28f },
                        "Studio Director", 12, gold, CARD_ALIGN_CENTER, 0);
        CardFaceAddText(face, (Rectangle){ 0.15f, 1.42f, 3.2f, 0.26f },
                        "555 014 2281  -  alex@northwind.studio", 9, mist, CARD_ALIGN_CENTER, 0);
        break;
    case CARD_TEMPLATE_COLORBAR:
        face->background = white;
        AddSolid(face, (Rectangle){
            -CARD_BLEED_IN, -CARD_BLEED_IN, 0.48f, CARD_H_IN + 2.0f * CARD_BLEED_IN
        }, teal);
        CardFaceAddText(face, (Rectangle){ 0.62f, 0.40f, 2.65f, 0.38f },
                        "Alex Morgan", 20, ink, CARD_ALIGN_LEFT, 0);
        CardFaceAddText(face, (Rectangle){ 0.62f, 0.84f, 2.65f, 0.26f },
                        "Studio Director", 11, teal, CARD_ALIGN_LEFT, 0);
        CardFaceAddText(face, (Rectangle){ 0.62f, 1.28f, 2.65f, 0.22f },
                        "555 014 2281", 10, ink, CARD_ALIGN_LEFT, 0);
        CardFaceAddText(face, (Rectangle){ 0.62f, 1.52f, 2.65f, 0.22f },
                        "alex@northwind.studio", 10, slate, CARD_ALIGN_LEFT, 0);
        break;
    case CARD_TEMPLATE_MINIMAL:
        face->background = white;
        CardFaceAddText(face, (Rectangle){ 0.28f, 0.58f, 2.94f, 0.46f },
                        "Alex Morgan", 24, ink, CARD_ALIGN_LEFT, 1);
        CardFaceAddText(face, (Rectangle){ 0.28f, 1.16f, 2.94f, 0.28f },
                        "alex@northwind.studio", 12, slate, CARD_ALIGN_LEFT, 0);
        break;
    case CARD_TEMPLATE_BLANK:
    default:
        face->background = white;
        break;
    }
}

void CardDocInit(CardDoc *doc)
{
    memset(doc, 0, sizeof(*doc));
    doc->selected = -1;
    doc->activeSide = CARD_SIDE_FRONT;
    for (int side = 0; side < CARD_SIDE_COUNT; side++) {
        doc->faces[side].background = (Color){ 255, 255, 255, 255 };
    }
    CardApplyTemplate(doc, CARD_SIDE_FRONT, CARD_TEMPLATE_CLASSIC);

    CardFace *back = &doc->faces[CARD_SIDE_BACK];
    back->background = (Color){ 248, 244, 236, 255 };
    CardFaceAddText(back, (Rectangle){ 0.3f, 0.72f, 2.9f, 0.36f },
                    "Northwind Studio", 16, (Color){ 32, 36, 42, 255 }, CARD_ALIGN_CENTER, 1);
    CardFaceAddText(back, (Rectangle){ 0.3f, 1.14f, 2.9f, 0.26f },
                    "northwind.studio", 11, (Color){ 14, 90, 92, 255 }, CARD_ALIGN_CENTER, 0);
    doc->selected = -1;
    doc->activeSide = CARD_SIDE_FRONT;
}

void CardHistoryInit(CardHistory *history)
{
    memset(history, 0, sizeof(*history));
}

static void ClearStack(CardDoc **stack, int *count)
{
    for (int i = 0; i < *count; i++) CardDocDestroy(stack[i]);
    *count = 0;
}

void CardHistoryFree(CardHistory *history)
{
    if (!history) return;
    ClearStack(history->undo, &history->undoCount);
    ClearStack(history->redo, &history->redoCount);
}

void CardHistoryPush(CardHistory *history, const CardDoc *before)
{
    if (!history || !before) return;
    ClearStack(history->redo, &history->redoCount);
    if (history->undoCount == CARD_UNDO_MAX) {
        CardDocDestroy(history->undo[0]);
        memmove(&history->undo[0], &history->undo[1], sizeof(CardDoc *) * (CARD_UNDO_MAX - 1));
        history->undoCount--;
    }
    CardDoc *copy = CardDocHeapClone(before, false);
    if (!copy) return;
    history->undo[history->undoCount++] = copy;
}

bool CardHistoryUndo(CardHistory *history, CardDoc *live)
{
    if (!history || !live || history->undoCount <= 0) return false;
    CardDoc *redoCopy = CardDocHeapClone(live, false);
    if (!redoCopy) return false;
    if (history->redoCount == CARD_UNDO_MAX) {
        CardDocDestroy(history->redo[0]);
        memmove(&history->redo[0], &history->redo[1], sizeof(CardDoc *) * (CARD_UNDO_MAX - 1));
        history->redoCount--;
    }
    history->redo[history->redoCount++] = redoCopy;

    CardDoc *prev = history->undo[--history->undoCount];
    history->undo[history->undoCount] = NULL;
    CardDocFree(live);
    CardDocClone(live, prev, true);
    CardDocDestroy(prev);
    return true;
}

bool CardHistoryRedo(CardHistory *history, CardDoc *live)
{
    if (!history || !live || history->redoCount <= 0) return false;
    CardDoc *undoCopy = CardDocHeapClone(live, false);
    if (!undoCopy) return false;
    if (history->undoCount == CARD_UNDO_MAX) {
        CardDocDestroy(history->undo[0]);
        memmove(&history->undo[0], &history->undo[1], sizeof(CardDoc *) * (CARD_UNDO_MAX - 1));
        history->undoCount--;
    }
    history->undo[history->undoCount++] = undoCopy;

    CardDoc *next = history->redo[--history->redoCount];
    history->redo[history->redoCount] = NULL;
    CardDocFree(live);
    CardDocClone(live, next, true);
    CardDocDestroy(next);
    return true;
}

static void TrimLine(char *line)
{
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ')) {
        line[--n] = '\0';
    }
}

static void WriteEscaped(FILE *f, const char *s)
{
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '\n') fputs("\\n", f);
        else if (*p == '\\') fputs("\\\\", f);
        else if (*p != '\r') fputc(*p, f);
    }
}

static void UnescapeText(const char *src, char *dst, int cap)
{
    int out = 0;
    for (int i = 0; src[i] && out < cap - 1; i++) {
        if (src[i] == '\\' && src[i + 1]) {
            i++;
            dst[out++] = (src[i] == 'n') ? '\n' : src[i];
        } else {
            dst[out++] = src[i];
        }
    }
    dst[out] = '\0';
}

bool CardDocSave(const CardDoc *doc, const char *path)
{
    if (!doc || !path) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fputs("CARDPRESS 1\n", f);
    for (int side = 0; side < CARD_SIDE_COUNT; side++) {
        const CardFace *face = &doc->faces[side];
        fprintf(f, "SIDE %d\n", side);
        fprintf(f, "BG %d %d %d %d\n", face->background.r, face->background.g,
                face->background.b, face->background.a);
        for (int i = 0; i < face->count; i++) {
            const CardElement *elem = &face->items[i];
            if (elem->kind == CARD_ELEM_TEXT) {
                fprintf(f, "TEXT %.4f %.4f %.4f %.4f %.2f %d %d %d %d %d %d |",
                        elem->box.x, elem->box.y, elem->box.width, elem->box.height,
                        elem->fontSize, (int)elem->align, elem->fontId,
                        elem->color.r, elem->color.g, elem->color.b, elem->color.a);
                WriteEscaped(f, elem->text);
                fputc('\n', f);
            } else if (elem->kind == CARD_ELEM_IMAGE) {
                fprintf(f, "IMAGE %.4f %.4f %.4f %.4f %s\n",
                        elem->box.x, elem->box.y, elem->box.width, elem->box.height,
                        elem->path);
            }
        }
    }
    fclose(f);
    return true;
}

bool CardDocLoad(CardDoc *doc, const char *path)
{
    if (!doc) return false;
    memset(doc, 0, sizeof(*doc));
    doc->selected = -1;
    for (int side = 0; side < CARD_SIDE_COUNT; side++) {
        doc->faces[side].background = (Color){ 255, 255, 255, 255 };
    }
    if (!path) return false;

    FILE *f = fopen(path, "rb");
    if (!f) return false;

    char line[1024];
    int side = CARD_SIDE_FRONT;
    bool sawHeader = false;
    bool failed = false;

    while (fgets(line, (int)sizeof(line), f)) {
        TrimLine(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        if (strncmp(line, "CARDPRESS", 9) == 0) {
            sawHeader = true;
            continue;
        }
        if (strncmp(line, "SIDE", 4) == 0) {
            int parsed = side;
            if (sscanf(line, "SIDE %d", &parsed) == 1 && parsed >= 0 && parsed < CARD_SIDE_COUNT) {
                side = parsed;
            }
            continue;
        }
        if (strncmp(line, "BG", 2) == 0) {
            int r = 255, g = 255, b = 255, a = 255;
            if (sscanf(line, "BG %d %d %d %d", &r, &g, &b, &a) >= 3) {
                doc->faces[side].background = (Color){
                    (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a
                };
            }
            continue;
        }
        if (strncmp(line, "TEXT ", 5) == 0) {
            char *bar = strstr(line, " |");
            if (!bar) continue;
            char raw[CARD_TEXT_MAX];
            UnescapeText(bar + 2, raw, CARD_TEXT_MAX);
            *bar = '\0';
            float x, y, w, h, pt;
            int align = 0, fontId = 0, r = 0, g = 0, b = 0, a = 255;
            if (sscanf(line, "TEXT %f %f %f %f %f %d %d %d %d %d %d",
                       &x, &y, &w, &h, &pt, &align, &fontId, &r, &g, &b, &a) < 11) {
                continue;
            }
            int index = CardFaceAddText(&doc->faces[side], (Rectangle){ x, y, w, h }, raw, pt,
                                        (Color){ (unsigned char)r, (unsigned char)g,
                                                 (unsigned char)b, (unsigned char)a },
                                        align, fontId);
            if (index < 0) failed = true;
            continue;
        }
        if (strncmp(line, "IMAGE ", 6) == 0) {
            float x, y, w, h;
            int consumed = 0;
            if (sscanf(line, "IMAGE %f %f %f %f %n", &x, &y, &w, &h, &consumed) < 4 || consumed <= 0) {
                continue;
            }
            const char *asset = line + consumed;
            if (asset[0] == '\0') continue;
            int index = CardFaceAddImageFile(&doc->faces[side], asset, (Rectangle){ x, y, w, h });
            if (index < 0) {
                /* Keep the box so a missing file is still a visible placeholder. */
                if (doc->faces[side].count < CARD_MAX_ELEMENTS) {
                    CardElement *elem = &doc->faces[side].items[doc->faces[side].count];
                    memset(elem, 0, sizeof(*elem));
                    elem->kind = CARD_ELEM_IMAGE;
                    elem->box = (Rectangle){ x, y, w, h };
                    snprintf(elem->path, CARD_PATH_MAX, "%s", asset);
                    doc->faces[side].count++;
                }
            }
        }
    }

    fclose(f);
    if (!sawHeader || failed) {
        CardDocFree(doc);
        memset(doc, 0, sizeof(*doc));
        doc->selected = -1;
        return false;
    }
    return true;
}
