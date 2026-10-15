/*******************************************************************************
 * cards.c — Card Press
 *
 * Customer-facing designer for a US business card (3.5 x 2 in). Text and
 * images are placed in inches, previewed with trim / bleed / safe guides,
 * and exported to a print-shop PNG and a 10-up letter sheet.
 ******************************************************************************/
#include "raylib.h"

#include "card_doc.h"
#include "card_draw.h"
#include "card_export.h"
#include "card_dialog.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UI_LEFT 220.0f
#define UI_RIGHT 268.0f
#define UI_STATUS 36.0f
#define PALETTE_N 8

typedef enum DragKind {
    DRAG_NONE = 0,
    DRAG_MOVE,
    DRAG_NW,
    DRAG_NE,
    DRAG_SW,
    DRAG_SE
} DragKind;

typedef struct UiLayout {
    Rectangle canvas;
    Rectangle left;
    Rectangle right;
    Rectangle status;
    Rectangle addText;
    Rectangle addImage;
    Rectangle front;
    Rectangle back;
    Rectangle templates[CARD_TEMPLATE_COUNT];
    Rectangle guides;
    Rectangle exportCard;
    Rectangle exportSheet;
    Rectangle save;
    Rectangle open;
    Rectangle sizeSlider;
    Rectangle alignBtn[3];
    Rectangle fontBtn[2];
    Rectangle textSwatch[PALETTE_N];
    Rectangle bgSwatch[PALETTE_N];
    Rectangle deleteBtn;
    Rectangle replaceBtn;
    bool showText;
    bool showImage;
    bool showDelete;
} UiLayout;

typedef struct App {
    CardDoc *doc;
    CardHistory history;
    Font fonts[2];
    bool fontOwned[2];
    float zoom;
    Vector2 pan;
    int fittedW;
    int fittedH;
    bool guides;
    bool editing;
    int caret;
    bool textCoalesce;
    int templateId;
    char projectPath[CARD_PATH_MAX];
    char status[384];
    float statusUntil;

    bool pressActive;
    DragKind pressDrag;
    Vector2 pressMouse;
    Vector2 grabInch;
    Rectangle grabBox;
    bool dragStarted;

    bool panning;
    int panButton;
    Vector2 panGrab;
    Vector2 panOrigin;

    bool sliderDrag;
} App;

static const Color PALETTE[PALETTE_N] = {
    { 248, 244, 236, 255 },
    { 255, 255, 255, 255 },
    { 32, 36, 42, 255 },
    { 20, 44, 66, 255 },
    { 14, 90, 92, 255 },
    { 140, 42, 52, 255 },
    { 196, 154, 78, 255 },
    { 90, 98, 108, 255 }
};

static const char FILTER_IMAGE[] = "Images\0*.png;*.jpg;*.jpeg;*.bmp;*.webp\0\0";
static const char FILTER_CARD[] = "Card Press\0*.card\0\0";
static const char FILTER_PNG[] = "PNG\0*.png\0\0";

static const char *HELP =
    "Drag moves    Corners resize    Type to edit    Wheel zooms    Middle-drag pans    Del removes";

static void Statusf(App *app, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(app->status, sizeof(app->status), fmt, ap);
    va_end(ap);
    app->statusUntil = (float)GetTime() + 5.0f;
}

static bool StatusVisible(const App *app)
{
    return app->status[0] != '\0' && (float)GetTime() < app->statusUntil;
}

static bool InRect(Rectangle r)
{
    if (r.width < 1.0f || r.height < 1.0f) return false;
    return CheckCollisionPointRec(GetMousePosition(), r);
}

static bool SameColor(Color a, Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static bool EndsWithI(const char *s, const char *ext)
{
    size_t n = strlen(s);
    size_t m = strlen(ext);
    if (n < m) return false;
    for (size_t i = 0; i < m; i++) {
        char a = s[n - m + i];
        char b = ext[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

static bool IsImagePath(const char *path)
{
    return EndsWithI(path, ".png") || EndsWithI(path, ".jpg") || EndsWithI(path, ".jpeg")
        || EndsWithI(path, ".bmp") || EndsWithI(path, ".webp");
}

static CardFace *ActiveFace(App *app)
{
    if (app->doc->activeSide < 0 || app->doc->activeSide >= CARD_SIDE_COUNT) {
        app->doc->activeSide = CARD_SIDE_FRONT;
    }
    return &app->doc->faces[app->doc->activeSide];
}

static CardElement *Selected(App *app)
{
    CardFace *face = ActiveFace(app);
    if (app->doc->selected < 0 || app->doc->selected >= face->count) return NULL;
    return &face->items[app->doc->selected];
}

static void Snapshot(App *app)
{
    CardHistoryPush(&app->history, app->doc);
    app->textCoalesce = false;
}

static void SnapshotTyping(App *app)
{
    if (!app->textCoalesce) {
        CardHistoryPush(&app->history, app->doc);
        app->textCoalesce = true;
    }
}

static void AfterHistory(App *app)
{
    app->editing = false;
    app->textCoalesce = false;
    app->pressActive = false;
    app->dragStarted = false;
    app->sliderDrag = false;
    app->templateId = -1;
    CardFace *face = ActiveFace(app);
    if (app->doc->selected < 0 || app->doc->selected >= face->count) app->doc->selected = -1;
    if (app->doc->selected >= 0) {
        int n = (int)strlen(face->items[app->doc->selected].text);
        if (app->caret > n) app->caret = n;
        if (app->caret < 0) app->caret = 0;
    }
}

static void RefreshTitle(const App *app)
{
    if (app->projectPath[0]) {
        char title[320];
        snprintf(title, sizeof(title), "Card Press - %s", GetFileName(app->projectPath));
        SetWindowTitle(title);
    } else {
        SetWindowTitle("Card Press");
    }
}

static Font UiFont(const App *app)
{
    return CardChooseFont(app->fonts, 2, 0);
}

static Vector2 ScreenToInch(const App *app, Vector2 mouse)
{
    return (Vector2){
        (mouse.x - app->pan.x) / app->zoom,
        (mouse.y - app->pan.y) / app->zoom
    };
}

static Rectangle ElementScreen(const App *app, Rectangle box)
{
    return (Rectangle){
        app->pan.x + box.x * app->zoom,
        app->pan.y + box.y * app->zoom,
        box.width * app->zoom,
        box.height * app->zoom
    };
}

static void FitCard(App *app, Rectangle canvas)
{
    float zx = (canvas.width - 72.0f) / CARD_W_IN;
    float zy = (canvas.height - 72.0f) / CARD_H_IN;
    app->zoom = zx < zy ? zx : zy;
    if (app->zoom < 36.0f) app->zoom = 36.0f;
    float w = CARD_W_IN * app->zoom;
    float h = CARD_H_IN * app->zoom;
    app->pan.x = canvas.x + (canvas.width - w) * 0.5f;
    app->pan.y = canvas.y + (canvas.height - h) * 0.5f;
}

static Rectangle Row(float x, float *y, float w, float h, float gap)
{
    Rectangle r = { x, *y, w, h };
    *y += h + gap;
    return r;
}

static void PlaceSwatches(Rectangle *out, float x, float *y)
{
    float size = 22.0f;
    float gap = 6.0f;
    for (int i = 0; i < PALETTE_N; i++) {
        out[i] = (Rectangle){ x + (float)i * (size + gap), *y, size, size };
    }
    *y += size + 12.0f;
}

static UiLayout BuildLayout(const App *app)
{
    UiLayout ui;
    memset(&ui, 0, sizeof(ui));
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    ui.left = (Rectangle){ 0, 0, UI_LEFT, sh - UI_STATUS };
    ui.right = (Rectangle){ sw - UI_RIGHT, 0, UI_RIGHT, sh - UI_STATUS };
    ui.status = (Rectangle){ 0, sh - UI_STATUS, sw, UI_STATUS };
    ui.canvas = (Rectangle){ UI_LEFT, 0, sw - UI_LEFT - UI_RIGHT, sh - UI_STATUS };

    float x = 14.0f;
    float w = UI_LEFT - 28.0f;
    float y = 52.0f;
    float h = 28.0f;
    float gap = 6.0f;
    ui.addText = Row(x, &y, w, h, gap);
    ui.addImage = Row(x, &y, w, h, gap);
    float half = (w - gap) * 0.5f;
    ui.front = (Rectangle){ x, y, half, h };
    ui.back = (Rectangle){ x + half + gap, y, half, h };
    y += h + gap + 22.0f;
    for (int i = 0; i < CARD_TEMPLATE_COUNT; i++) ui.templates[i] = Row(x, &y, w, h, gap);
    y += 10.0f;
    ui.guides = Row(x, &y, w, h, gap);
    y += 16.0f;
    ui.exportCard = Row(x, &y, w, h, gap);
    ui.exportSheet = Row(x, &y, w, h, gap);
    y += 16.0f;
    ui.save = Row(x, &y, w, h, gap);
    ui.open = Row(x, &y, w, h, gap);

    float rx = ui.right.x + 16.0f;
    float rw = ui.right.width - 32.0f;
    float ry = 48.0f;
    const CardFace *face = &app->doc->faces[app->doc->activeSide];
    int sel = app->doc->selected;
    bool has = sel >= 0 && sel < face->count;
    if (has && face->items[sel].kind == CARD_ELEM_TEXT) {
        ui.showText = true;
        ry += 36.0f;
        ui.sizeSlider = (Rectangle){ rx, ry, rw, 16.0f };
        ry += 36.0f;
        float aw = (rw - 8.0f) / 3.0f;
        for (int i = 0; i < 3; i++) {
            ui.alignBtn[i] = (Rectangle){ rx + (float)i * (aw + 4.0f), ry, aw, h };
        }
        ry += h + 16.0f;
        float fw = (rw - gap) * 0.5f;
        ui.fontBtn[0] = (Rectangle){ rx, ry, fw, h };
        ui.fontBtn[1] = (Rectangle){ rx + fw + gap, ry, fw, h };
        ry += h + 22.0f;
        PlaceSwatches(ui.textSwatch, rx, &ry);
        ry += 6.0f;
    } else if (has && face->items[sel].kind == CARD_ELEM_IMAGE) {
        ui.showImage = true;
        ry += 28.0f;
        ui.replaceBtn = Row(rx, &ry, rw, h, gap);
        ry += 8.0f;
    }
    if (has) {
        ui.showDelete = true;
        ui.deleteBtn = Row(rx, &ry, rw, h, gap);
        ry += 10.0f;
    }
    ry += 20.0f;
    PlaceSwatches(ui.bgSwatch, rx, &ry);
    return ui;
}

static void DrawButton(Font font, Rectangle r, const char *label, bool active)
{
    if (r.width < 1.0f) return;
    bool hover = InRect(r);
    Color bg = active ? (Color){ 14, 110, 120, 255 }
                      : (hover ? (Color){ 232, 236, 238, 255 } : (Color){ 244, 241, 236, 255 });
    Color fg = active ? WHITE : (Color){ 32, 36, 42, 255 };
    DrawRectangleRounded(r, 0.18f, 6, bg);
    float size = 15.0f;
    Vector2 ts = MeasureTextEx(font, label, size, 0.4f);
    DrawTextEx(font, label,
               (Vector2){ r.x + (r.width - ts.x) * 0.5f, r.y + (r.height - ts.y) * 0.5f },
               size, 0.4f, fg);
}

static void DrawSwatch(Rectangle r, Color color, bool on)
{
    DrawRectangleRounded(r, 0.22f, 4, color);
    if (on) DrawRectangleLinesEx((Rectangle){ r.x - 2, r.y - 2, r.width + 4, r.height + 4 }, 2.0f,
                                 (Color){ 14, 110, 120, 255 });
    else DrawRectangleLinesEx(r, 1.0f, (Color){ 186, 180, 170, 255 });
}

static void DrawLabel(Font font, const char *text, float x, float y, Color color)
{
    DrawTextEx(font, text, (Vector2){ x, y }, 15.0f, 0.4f, color);
}

static Vector2 CornerPoint(Rectangle box, DragKind kind)
{
    switch (kind) {
    case DRAG_NE: return (Vector2){ box.x + box.width, box.y };
    case DRAG_SW: return (Vector2){ box.x, box.y + box.height };
    case DRAG_SE: return (Vector2){ box.x + box.width, box.y + box.height };
    case DRAG_NW:
    default: return (Vector2){ box.x, box.y };
    }
}

static DragKind HitHandle(Rectangle box, Vector2 mouse)
{
    DragKind kinds[4] = { DRAG_NW, DRAG_NE, DRAG_SW, DRAG_SE };
    DragKind best = DRAG_NONE;
    float bestD = 12.0f * 12.0f;
    for (int i = 0; i < 4; i++) {
        Vector2 p = CornerPoint(box, kinds[i]);
        float dx = mouse.x - p.x;
        float dy = mouse.y - p.y;
        float d = dx * dx + dy * dy;
        if (d <= bestD) {
            bestD = d;
            best = kinds[i];
        }
    }
    return best;
}

static void ResizeBox(Rectangle *box, DragKind mode, Rectangle orig, float dx, float dy)
{
    Rectangle b = orig;
    switch (mode) {
    case DRAG_MOVE:
        b.x = orig.x + dx;
        b.y = orig.y + dy;
        break;
    case DRAG_SE:
        b.width = orig.width + dx;
        b.height = orig.height + dy;
        break;
    case DRAG_NE:
        b.y = orig.y + dy;
        b.width = orig.width + dx;
        b.height = orig.height - dy;
        break;
    case DRAG_SW:
        b.x = orig.x + dx;
        b.width = orig.width - dx;
        b.height = orig.height + dy;
        break;
    case DRAG_NW:
        b.x = orig.x + dx;
        b.y = orig.y + dy;
        b.width = orig.width - dx;
        b.height = orig.height - dy;
        break;
    default:
        break;
    }
    const float minS = 0.18f;
    if (b.width < minS) {
        if (mode == DRAG_NW || mode == DRAG_SW) b.x = orig.x + orig.width - minS;
        b.width = minS;
    }
    if (b.height < minS) {
        if (mode == DRAG_NW || mode == DRAG_NE) b.y = orig.y + orig.height - minS;
        b.height = minS;
    }
    *box = b;
}

static bool KeyFired(int key)
{
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

static void ApplySlider(App *app, Rectangle slider)
{
    CardElement *elem = Selected(app);
    if (!elem || elem->kind != CARD_ELEM_TEXT || slider.width < 1.0f) return;
    float t = (GetMousePosition().x - slider.x) / slider.width;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    int pt = 6 + (int)(t * 42.0f + 0.5f);
    if (pt < 6) pt = 6;
    if (pt > 48) pt = 48;
    elem->fontSize = (float)pt;
}

static void AddTextBlock(App *app)
{
    CardFace *face = ActiveFace(app);
    if (face->count >= CARD_MAX_ELEMENTS) {
        Statusf(app, "This side is full (32 items).");
        return;
    }
    Snapshot(app);
    int index = CardFaceAddText(face, (Rectangle){ 0.45f, 0.78f, 2.6f, 0.36f },
                                "Your text", 16, (Color){ 32, 36, 42, 255 },
                                CARD_ALIGN_LEFT, 0);
    app->doc->selected = index;
    app->editing = true;
    app->caret = (int)strlen(face->items[index].text);
    app->templateId = -1;
}

static void AddImagePath(App *app, const char *path)
{
    CardFace *face = ActiveFace(app);
    if (face->count >= CARD_MAX_ELEMENTS) {
        Statusf(app, "This side is full (32 items).");
        return;
    }
    Snapshot(app);
    int index = CardFaceAddImageFit(face, path);
    if (index < 0) {
        Statusf(app, "Could not open that image.");
        if (CardHistoryUndo(&app->history, app->doc)) AfterHistory(app);
        return;
    }
    app->doc->selected = index;
    app->editing = false;
    app->templateId = -1;
    Statusf(app, "Added %s", GetFileName(path));
}

static void PickImage(App *app)
{
    char path[CARD_PATH_MAX];
    if (!CardDialogOpen(GetWindowHandle(), path, CARD_PATH_MAX, FILTER_IMAGE, "Add an image")) return;
    AddImagePath(app, path);
}

static void ApplyTemplate(App *app, int templateId)
{
    Snapshot(app);
    CardApplyTemplate(app->doc, app->doc->activeSide, templateId);
    app->editing = false;
    app->textCoalesce = false;
    app->templateId = templateId;
    Statusf(app, "Applied %s to the %s.", CardTemplateName(templateId),
            CardSideName(app->doc->activeSide));
}

static bool SaveProject(App *app, bool forceDialog)
{
    char path[CARD_PATH_MAX];
    if (!forceDialog && app->projectPath[0]) {
        snprintf(path, sizeof(path), "%s", app->projectPath);
    } else {
        const char *suggested = app->projectPath[0] ? GetFileName(app->projectPath) : "my-card.card";
        if (!CardDialogSave(GetWindowHandle(), path, CARD_PATH_MAX, FILTER_CARD,
                            "Save project", suggested, "card")) {
            return false;
        }
    }
    if (!CardDocSave(app->doc, path)) {
        Statusf(app, "Could not save the project.");
        return false;
    }
    snprintf(app->projectPath, sizeof(app->projectPath), "%s", path);
    RefreshTitle(app);
    Statusf(app, "Saved %s", GetFileName(path));
    return true;
}

static void OpenProject(App *app, const char *path)
{
    CardDoc *loaded = (CardDoc *)calloc(1, sizeof(CardDoc));
    if (!loaded) return;
    if (!CardDocLoad(loaded, path)) {
        free(loaded);
        Statusf(app, "Could not open that project.");
        return;
    }
    CardDocFree(app->doc);
    *app->doc = *loaded;
    free(loaded);
    CardHistoryFree(&app->history);
    CardHistoryInit(&app->history);
    app->editing = false;
    app->textCoalesce = false;
    app->templateId = -1;
    app->caret = 0;
    snprintf(app->projectPath, sizeof(app->projectPath), "%s", path);
    RefreshTitle(app);
    Statusf(app, "Opened %s", GetFileName(path));
}

static void PickProject(App *app)
{
    char path[CARD_PATH_MAX];
    if (!CardDialogOpen(GetWindowHandle(), path, CARD_PATH_MAX, FILTER_CARD, "Open a project")) return;
    OpenProject(app, path);
}

static void ExportActive(App *app, bool sheet)
{
    const char *side = app->doc->activeSide == CARD_SIDE_BACK ? "back" : "front";
    char suggested[64];
    snprintf(suggested, sizeof(suggested), "%s-%s.png", sheet ? "sheet" : "card", side);
    char path[CARD_PATH_MAX];
    if (!CardDialogSave(GetWindowHandle(), path, CARD_PATH_MAX, FILTER_PNG,
                        sheet ? "Export letter sheet" : "Export card", suggested, "png")) {
        return;
    }
    const CardFace *face = ActiveFace(app);
    bool ok = sheet ? CardExportSheet(face, app->fonts, 2, path)
                    : CardExportShop(face, app->fonts, 2, path);
    if (ok) Statusf(app, "Exported %s", GetFileName(path));
    else Statusf(app, "Export failed.");
}

static void DeleteSelected(App *app)
{
    CardFace *face = ActiveFace(app);
    int index = app->doc->selected;
    if (index < 0 || index >= face->count) return;
    Snapshot(app);
    CardFaceDelete(face, index);
    app->doc->selected = -1;
    app->editing = false;
    app->templateId = -1;
}

static void HandlePanelClick(App *app, const UiLayout *ui)
{
    if (ui->showText && InRect(ui->sizeSlider)) {
        Snapshot(app);
        app->sliderDrag = true;
        ApplySlider(app, ui->sizeSlider);
        return;
    }
    if (InRect(ui->addText)) {
        AddTextBlock(app);
        return;
    }
    if (InRect(ui->addImage)) {
        PickImage(app);
        return;
    }
    if (InRect(ui->front)) {
        app->doc->activeSide = CARD_SIDE_FRONT;
        app->doc->selected = -1;
        app->editing = false;
        app->textCoalesce = false;
        return;
    }
    if (InRect(ui->back)) {
        app->doc->activeSide = CARD_SIDE_BACK;
        app->doc->selected = -1;
        app->editing = false;
        app->textCoalesce = false;
        return;
    }
    for (int i = 0; i < CARD_TEMPLATE_COUNT; i++) {
        if (InRect(ui->templates[i])) {
            ApplyTemplate(app, i);
            return;
        }
    }
    if (InRect(ui->guides)) {
        app->guides = !app->guides;
        return;
    }
    if (InRect(ui->exportCard)) {
        ExportActive(app, false);
        return;
    }
    if (InRect(ui->exportSheet)) {
        ExportActive(app, true);
        return;
    }
    if (InRect(ui->save)) {
        SaveProject(app, app->projectPath[0] == '\0');
        return;
    }
    if (InRect(ui->open)) {
        PickProject(app);
        return;
    }

    CardElement *elem = Selected(app);
    if (ui->showText && elem) {
        for (int i = 0; i < 3; i++) {
            if (InRect(ui->alignBtn[i])) {
                Snapshot(app);
                elem = Selected(app);
                if (elem) elem->align = (CardAlign)i;
                return;
            }
        }
        for (int i = 0; i < 2; i++) {
            if (InRect(ui->fontBtn[i])) {
                Snapshot(app);
                elem = Selected(app);
                if (elem) elem->fontId = i;
                return;
            }
        }
        for (int i = 0; i < PALETTE_N; i++) {
            if (InRect(ui->textSwatch[i])) {
                Snapshot(app);
                elem = Selected(app);
                if (elem) elem->color = PALETTE[i];
                return;
            }
        }
    }
    if (ui->showImage && elem && InRect(ui->replaceBtn)) {
        char path[CARD_PATH_MAX];
        if (!CardDialogOpen(GetWindowHandle(), path, CARD_PATH_MAX, FILTER_IMAGE, "Replace image")) return;
        Snapshot(app);
        elem = Selected(app);
        if (elem && !CardElementReplaceFile(elem, path)) {
            Statusf(app, "Could not open that image.");
            if (CardHistoryUndo(&app->history, app->doc)) AfterHistory(app);
        }
        return;
    }
    if (ui->showDelete && InRect(ui->deleteBtn)) {
        DeleteSelected(app);
        return;
    }
    for (int i = 0; i < PALETTE_N; i++) {
        if (InRect(ui->bgSwatch[i])) {
            Snapshot(app);
            ActiveFace(app)->background = PALETTE[i];
            app->templateId = -1;
            return;
        }
    }
}

static void BeginCanvasPress(App *app)
{
    Vector2 mouse = GetMousePosition();
    bool space = IsKeyDown(KEY_SPACE);
    if (space && !app->editing) {
        app->panning = true;
        app->panButton = MOUSE_BUTTON_LEFT;
        app->panGrab = mouse;
        app->panOrigin = app->pan;
        return;
    }

    CardElement *current = Selected(app);
    if (current) {
        DragKind handle = HitHandle(ElementScreen(app, current->box), mouse);
        if (handle != DRAG_NONE) {
            app->pressActive = true;
            app->pressDrag = handle;
            app->pressMouse = mouse;
            app->grabInch = ScreenToInch(app, mouse);
            app->grabBox = current->box;
            app->dragStarted = false;
            return;
        }
    }

    Vector2 inch = ScreenToInch(app, mouse);
    int hit = CardFaceHitTest(ActiveFace(app), inch.x, inch.y);
    if (hit >= 0) {
        bool same = hit == app->doc->selected;
        if (!same) app->textCoalesce = false;
        app->doc->selected = hit;
        CardElement *elem = &ActiveFace(app)->items[hit];
        if (elem->kind == CARD_ELEM_TEXT) {
            app->editing = true;
            if (!same) app->caret = (int)strlen(elem->text);
        } else {
            app->editing = false;
        }
        app->pressActive = true;
        app->pressDrag = DRAG_MOVE;
        app->pressMouse = mouse;
        app->grabInch = inch;
        app->grabBox = elem->box;
        app->dragStarted = false;
    } else {
        app->doc->selected = -1;
        app->editing = false;
        app->textCoalesce = false;
    }
}

static void UpdateDrag(App *app)
{
    if (!app->pressActive || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) return;
    Vector2 mouse = GetMousePosition();
    float dx = mouse.x - app->pressMouse.x;
    float dy = mouse.y - app->pressMouse.y;
    if (!app->dragStarted) {
        if (dx * dx + dy * dy < 16.0f) return;
        Snapshot(app);
        app->dragStarted = true;
        app->templateId = -1;
    }
    CardElement *elem = Selected(app);
    if (!elem) return;
    Vector2 inch = ScreenToInch(app, mouse);
    ResizeBox(&elem->box, app->pressDrag, app->grabBox,
              inch.x - app->grabInch.x, inch.y - app->grabInch.y);
    CardElementClamp(elem);
}

static void InsertTyping(App *app, const char *text)
{
    CardElement *elem = Selected(app);
    if (!elem || elem->kind != CARD_ELEM_TEXT || !text || !text[0]) return;
    if (strlen(elem->text) + strlen(text) >= CARD_TEXT_MAX) return;
    SnapshotTyping(app);
    elem = Selected(app);
    if (!elem) return;
    CardTextInsert(elem, &app->caret, text);
}

static void UpdateTyping(App *app, bool ctrl)
{
    CardElement *elem = Selected(app);
    if (!app->editing || !elem || elem->kind != CARD_ELEM_TEXT) return;

    if (ctrl && IsKeyPressed(KEY_V)) {
        const char *clip = GetClipboardText();
        if (!clip) return;
        char one[2] = { 0, 0 };
        for (const char *p = clip; *p; p++) {
            if (*p == '\r') continue;
            if (*p == '\n' || (*p >= 32 && *p < 127)) {
                one[0] = *p;
                InsertTyping(app, one);
            }
        }
        return;
    }
    if (ctrl) return;

    int cp = GetCharPressed();
    while (cp > 0) {
        if (cp >= 32 && cp < 127) {
            char one[2] = { (char)cp, 0 };
            InsertTyping(app, one);
        }
        cp = GetCharPressed();
    }
    if (KeyFired(KEY_BACKSPACE) && app->caret > 0) {
        SnapshotTyping(app);
        elem = Selected(app);
        if (elem) CardTextErase(elem, &app->caret, true);
    }
    if (KeyFired(KEY_DELETE)) {
        elem = Selected(app);
        int length = elem ? (int)strlen(elem->text) : 0;
        if (elem && app->caret < length) {
            SnapshotTyping(app);
            elem = Selected(app);
            if (elem) CardTextErase(elem, &app->caret, false);
        }
    }
    if (KeyFired(KEY_ENTER) || KeyFired(KEY_KP_ENTER)) InsertTyping(app, "\n");

    elem = Selected(app);
    int n = elem ? (int)strlen(elem->text) : 0;
    if (KeyFired(KEY_LEFT) && app->caret > 0) app->caret--;
    if (KeyFired(KEY_RIGHT) && app->caret < n) app->caret++;
    if (IsKeyPressed(KEY_HOME)) app->caret = 0;
    if (IsKeyPressed(KEY_END)) app->caret = n;
}

static void UpdateNudge(App *app)
{
    if (app->editing || app->pressActive || app->panning) return;
    CardElement *elem = Selected(app);
    if (!elem) return;
    bool left = KeyFired(KEY_LEFT);
    bool right = KeyFired(KEY_RIGHT);
    bool up = KeyFired(KEY_UP);
    bool down = KeyFired(KEY_DOWN);
    if (!left && !right && !up && !down) return;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) {
        Snapshot(app);
        elem = Selected(app);
        if (!elem) return;
    }
    float step = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 0.05f : 0.01f;
    if (left) elem->box.x -= step;
    if (right) elem->box.x += step;
    if (up) elem->box.y -= step;
    if (down) elem->box.y += step;
    CardElementClamp(elem);
    app->templateId = -1;
}

static void UpdateZoom(App *app, Rectangle canvas)
{
    float wheel = GetMouseWheelMove();
    if (wheel == 0.0f || !InRect(canvas) || app->sliderDrag) return;
    Vector2 mouse = GetMousePosition();
    Vector2 before = ScreenToInch(app, mouse);
    float factor = wheel > 0.0f ? 1.1f : 1.0f / 1.1f;
    app->zoom *= factor;
    if (app->zoom < 36.0f) app->zoom = 36.0f;
    if (app->zoom > 900.0f) app->zoom = 900.0f;
    app->pan.x = mouse.x - before.x * app->zoom;
    app->pan.y = mouse.y - before.y * app->zoom;
}

static void UpdateApp(App *app, const UiLayout *ui)
{
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl) {
        while (GetCharPressed() > 0) { }
        if (IsKeyPressed(KEY_Z)) {
            if (CardHistoryUndo(&app->history, app->doc)) AfterHistory(app);
        } else if (IsKeyPressed(KEY_Y)) {
            if (CardHistoryRedo(&app->history, app->doc)) AfterHistory(app);
        } else if (IsKeyPressed(KEY_S)) {
            SaveProject(app, false);
        } else if (IsKeyPressed(KEY_O)) {
            PickProject(app);
        } else {
            UpdateTyping(app, true);
        }
    } else if (app->editing) {
        UpdateTyping(app, false);
    } else if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        DeleteSelected(app);
    } else {
        UpdateNudge(app);
    }

    if (IsKeyPressed(KEY_ESCAPE)) app->editing = false;

    int cw = (int)ui->canvas.width;
    int ch = (int)ui->canvas.height;
    if (cw != app->fittedW || ch != app->fittedH) {
        FitCard(app, ui->canvas);
        app->fittedW = cw;
        app->fittedH = ch;
    }
    UpdateZoom(app, ui->canvas);

    if (app->sliderDrag && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ApplySlider(app, ui->sizeSlider);
    if (app->panning && IsMouseButtonDown(app->panButton)) {
        Vector2 mouse = GetMousePosition();
        app->pan.x = app->panOrigin.x + (mouse.x - app->panGrab.x);
        app->pan.y = app->panOrigin.y + (mouse.y - app->panGrab.y);
    }
    UpdateDrag(app);

    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) && InRect(ui->canvas)) {
        app->panning = true;
        app->panButton = MOUSE_BUTTON_MIDDLE;
        app->panGrab = GetMousePosition();
        app->panOrigin = app->pan;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (InRect(ui->left) || InRect(ui->right)) HandlePanelClick(app, ui);
        else if (InRect(ui->canvas) && !app->panning) BeginCanvasPress(app);
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        app->sliderDrag = false;
        app->pressActive = false;
        app->dragStarted = false;
        if (app->panButton == MOUSE_BUTTON_LEFT) app->panning = false;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) app->panning = false;

    if (IsFileDropped()) {
        FilePathList files = LoadDroppedFiles();
        for (unsigned int i = 0; i < files.count; i++) {
            const char *path = files.paths[i];
            if (EndsWithI(path, ".card")) OpenProject(app, path);
            else if (IsImagePath(path)) AddImagePath(app, path);
        }
        UnloadDroppedFiles(files);
    }
}

static void DrawSelection(const App *app, Rectangle canvas)
{
    const CardElement *elem = NULL;
    const CardFace *face = &app->doc->faces[app->doc->activeSide];
    if (app->doc->selected >= 0 && app->doc->selected < face->count) {
        elem = &face->items[app->doc->selected];
    }
    if (!elem) return;

    BeginScissorMode((int)canvas.x, (int)canvas.y, (int)canvas.width, (int)canvas.height);
    Rectangle box = ElementScreen(app, elem->box);
    DrawRectangleLinesEx(box, 1.5f, (Color){ 14, 110, 120, 255 });
    DragKind kinds[4] = { DRAG_NW, DRAG_NE, DRAG_SW, DRAG_SE };
    for (int i = 0; i < 4; i++) {
        Vector2 p = CornerPoint(box, kinds[i]);
        Rectangle handle = { p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f };
        DrawRectangleRec(handle, WHITE);
        DrawRectangleLinesEx(handle, 1.25f, (Color){ 14, 110, 120, 255 });
    }

    if (app->editing && elem->kind == CARD_ELEM_TEXT && fmodf((float)GetTime(), 1.0f) < 0.5f) {
        Font font = CardChooseFont(app->fonts, 2, elem->fontId);
        float fontPx = elem->fontSize * (app->zoom / 72.0f);
        if (fontPx < 1.0f) fontPx = 1.0f;
        float cx, cy, ch;
        CardTextCaretPx(font, elem->text, app->caret, fontPx, box.width, elem->align, box, &cx, &cy, &ch);
        DrawLineEx((Vector2){ cx, cy }, (Vector2){ cx, cy + ch }, 1.5f, elem->color);
    }
    EndScissorMode();
}

static void DrawCard(App *app, Rectangle canvas)
{
    float bleed = CARD_BLEED_IN * app->zoom;
    Rectangle trim = { app->pan.x, app->pan.y, CARD_W_IN * app->zoom, CARD_H_IN * app->zoom };
    Rectangle sheet = { trim.x - bleed, trim.y - bleed, trim.width + bleed * 2.0f, trim.height + bleed * 2.0f };
    Rectangle shadow = sheet;
    shadow.x += 7.0f;
    shadow.y += 9.0f;

    BeginScissorMode((int)canvas.x, (int)canvas.y, (int)canvas.width, (int)canvas.height);
    DrawRectangleRounded(shadow, 0.02f, 4, (Color){ 36, 42, 52, 45 });
    EndScissorMode();

    CardDrawFace(ActiveFace(app), trim, canvas, app->fonts, 2, true, app->guides);
    DrawSelection(app, canvas);
}

static void DrawPanels(const App *app, const UiLayout *ui)
{
    Font font = UiFont(app);
    Color ink = { 32, 36, 42, 255 };
    Color mute = { 110, 116, 124, 255 };

    DrawRectangleRec(ui->left, (Color){ 250, 249, 246, 255 });
    DrawRectangleRec(ui->right, (Color){ 250, 249, 246, 255 });
    DrawLine((int)ui->left.width, 0, (int)ui->left.width, (int)ui->left.height, (Color){ 220, 216, 208, 255 });
    DrawLine((int)ui->right.x, 0, (int)ui->right.x, (int)ui->right.height, (Color){ 220, 216, 208, 255 });

    DrawTextEx(font, "Card Press", (Vector2){ 16, 16 }, 22, 0.4f, ink);
    DrawButton(font, ui->addText, "Add text", false);
    DrawButton(font, ui->addImage, "Add image", false);
    DrawButton(font, ui->front, "Front", app->doc->activeSide == CARD_SIDE_FRONT);
    DrawButton(font, ui->back, "Back", app->doc->activeSide == CARD_SIDE_BACK);
    DrawLabel(font, "Templates", ui->templates[0].x, ui->templates[0].y - 20.0f, mute);
    for (int i = 0; i < CARD_TEMPLATE_COUNT; i++) {
        DrawButton(font, ui->templates[i], CardTemplateName(i), app->templateId == i);
    }
    DrawButton(font, ui->guides, app->guides ? "Hide guides" : "Show guides", app->guides);
    DrawLabel(font, "Print", ui->exportCard.x, ui->exportCard.y - 20.0f, mute);
    DrawButton(font, ui->exportCard, "Export card PNG", false);
    DrawButton(font, ui->exportSheet, "Export letter sheet", false);
    DrawLabel(font, "Project", ui->save.x, ui->save.y - 20.0f, mute);
    DrawButton(font, ui->save, "Save project", false);
    DrawButton(font, ui->open, "Open project", false);

    DrawTextEx(font, "Properties", (Vector2){ ui->right.x + 16.0f, 16 }, 22, 0.4f, ink);
    const CardElement *elem = NULL;
    const CardFace *face = &app->doc->faces[app->doc->activeSide];
    if (app->doc->selected >= 0 && app->doc->selected < face->count) elem = &face->items[app->doc->selected];

    if (ui->showText && elem) {
        char info[96];
        snprintf(info, sizeof(info), "Text    %.2f in, %.2f in", elem->box.x, elem->box.y);
        DrawLabel(font, info, ui->right.x + 16.0f, ui->sizeSlider.y - 34.0f, mute);
        char pt[32];
        snprintf(pt, sizeof(pt), "%d pt", (int)(elem->fontSize + 0.5f));
        DrawLabel(font, pt, ui->sizeSlider.x, ui->sizeSlider.y - 18.0f, ink);
        DrawRectangleRounded(ui->sizeSlider, 0.5f, 4, (Color){ 226, 222, 214, 255 });
        float t = (elem->fontSize - 6.0f) / 42.0f;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        Rectangle fill = ui->sizeSlider;
        fill.width = ui->sizeSlider.width * t;
        if (fill.width > 2.0f) DrawRectangleRounded(fill, 0.5f, 4, (Color){ 14, 110, 120, 255 });
        DrawButton(font, ui->alignBtn[0], "Left", elem->align == CARD_ALIGN_LEFT);
        DrawButton(font, ui->alignBtn[1], "Center", elem->align == CARD_ALIGN_CENTER);
        DrawButton(font, ui->alignBtn[2], "Right", elem->align == CARD_ALIGN_RIGHT);
        DrawButton(font, ui->fontBtn[0], "Segoe", elem->fontId == 0);
        DrawButton(font, ui->fontBtn[1], "Georgia", elem->fontId == 1);
        DrawLabel(font, "Text color", ui->textSwatch[0].x, ui->textSwatch[0].y - 18.0f, mute);
        for (int i = 0; i < PALETTE_N; i++) DrawSwatch(ui->textSwatch[i], PALETTE[i], SameColor(elem->color, PALETTE[i]));
    } else if (ui->showImage && elem) {
        const char *name = elem->path[0] ? GetFileName(elem->path) : "Image";
        if (strncmp(elem->path, "solid:#", 7) == 0) name = "Color bar";
        if (strncmp(elem->path, "mark:", 5) == 0) name = "Monogram";
        DrawLabel(font, name, ui->right.x + 16.0f, ui->replaceBtn.y - 22.0f, mute);
        DrawButton(font, ui->replaceBtn, "Replace image", false);
    } else {
        DrawLabel(font, "Click the card to select", ui->right.x + 16.0f, 52.0f, mute);
    }

    if (ui->showDelete) DrawButton(font, ui->deleteBtn, "Delete", false);
    DrawLabel(font, "Card background", ui->bgSwatch[0].x, ui->bgSwatch[0].y - 18.0f, mute);
    for (int i = 0; i < PALETTE_N; i++) {
        DrawSwatch(ui->bgSwatch[i], PALETTE[i], SameColor(face->background, PALETTE[i]));
    }

    DrawRectangleRec(ui->status, (Color){ 32, 36, 42, 255 });
    const char *line = HELP;
    if (StatusVisible(app)) line = app->status;
    else if (app->editing) line = "Typing    Esc leaves the cursor    Ctrl+Z undoes";
    const char *side = CardSideName(app->doc->activeSide);
    Vector2 sideSize = MeasureTextEx(font, side, 15, 0.4f);
    int statusClip = (int)(ui->status.width - sideSize.x - 28.0f);
    if (statusClip < 1) statusClip = 1;
    BeginScissorMode(0, (int)ui->status.y, statusClip, (int)ui->status.height);
    DrawTextEx(font, line, (Vector2){ 14, ui->status.y + 9 }, 15, 0.4f, (Color){ 236, 236, 232, 255 });
    EndScissorMode();
    DrawTextEx(font, side, (Vector2){ ui->status.width - sideSize.x - 16, ui->status.y + 9 },
               15, 0.4f, (Color){ 196, 214, 214, 255 });
}

static Font TryLoadFont(const char *path, bool *owned)
{
    int codepoints[95];
    for (int i = 0; i < 95; i++) codepoints[i] = 32 + i;
    Font font = LoadFontEx(path, 128, codepoints, 95);
    Font fallback = GetFontDefault();
    if (font.texture.id == 0 || font.glyphCount <= 0 || font.texture.id == fallback.texture.id) {
        *owned = false;
        return fallback;
    }
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    *owned = true;
    return font;
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1440, 900, "Card Press");
    SetWindowMinSize(1100, 700);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    Image icon = GenImageColor(32, 32, (Color){ 248, 244, 236, 255 });
    ImageDrawRectangle(&icon, 0, 0, 8, 32, (Color){ 14, 110, 120, 255 });
    SetWindowIcon(icon);
    UnloadImage(icon);

    App app;
    memset(&app, 0, sizeof(app));
    app.doc = (CardDoc *)calloc(1, sizeof(CardDoc));
    if (!app.doc) {
        CloseWindow();
        return 1;
    }
    app.fonts[0] = TryLoadFont("C:\\Windows\\Fonts\\segoeui.ttf", &app.fontOwned[0]);
    app.fonts[1] = TryLoadFont("C:\\Windows\\Fonts\\georgia.ttf", &app.fontOwned[1]);
    app.guides = true;
    app.templateId = CARD_TEMPLATE_CLASSIC;
    app.zoom = 160.0f;
    CardDocInit(app.doc);
    CardHistoryInit(&app.history);

    if (!app.fontOwned[0] && !app.fontOwned[1]) {
        Statusf(&app, "Segoe UI and Georgia were not found. Using a built-in font.");
    } else if (!app.fontOwned[0]) {
        Statusf(&app, "Segoe UI was not found. Using a built-in font.");
    } else if (!app.fontOwned[1]) {
        Statusf(&app, "Georgia was not found. That choice falls back to Segoe UI.");
    }

    while (!WindowShouldClose()) {
        UiLayout ui = BuildLayout(&app);
        UpdateApp(&app, &ui);

        BeginDrawing();
        ClearBackground((Color){ 208, 213, 220, 255 });
        DrawCard(&app, ui.canvas);
        DrawPanels(&app, &ui);
        EndDrawing();
    }

    CardHistoryFree(&app.history);
    CardDocFree(app.doc);
    free(app.doc);
    if (app.fontOwned[0]) UnloadFont(app.fonts[0]);
    if (app.fontOwned[1]) UnloadFont(app.fonts[1]);
    CloseWindow();
    return 0;
}
