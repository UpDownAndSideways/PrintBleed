/*******************************************************************************
 * card_export.c — 300 DPI PNG export
 *
 * The shop file includes bleed and crop marks. The letter sheet does not:
 * ten bled cards will not fit on 8.5 x 11 in, so the sheet uses the Avery
 * 8371 trim grid and puts ticks only in the outer margins.
 ******************************************************************************/
#include "card_export.h"
#include "card_draw.h"

static void DrawCornerMarks(Rectangle trim, float gap, float len)
{
    float x = trim.x;
    float y = trim.y;
    float r = trim.x + trim.width;
    float b = trim.y + trim.height;
    float t = 1.5f;
    Color c = { 20, 20, 20, 255 };

    DrawLineEx((Vector2){ x - gap - len, y }, (Vector2){ x - gap, y }, t, c);
    DrawLineEx((Vector2){ x, y - gap - len }, (Vector2){ x, y - gap }, t, c);

    DrawLineEx((Vector2){ r + gap, y }, (Vector2){ r + gap + len, y }, t, c);
    DrawLineEx((Vector2){ r, y - gap - len }, (Vector2){ r, y - gap }, t, c);

    DrawLineEx((Vector2){ x - gap - len, b }, (Vector2){ x - gap, b }, t, c);
    DrawLineEx((Vector2){ x, b + gap }, (Vector2){ x, b + gap + len }, t, c);

    DrawLineEx((Vector2){ r + gap, b }, (Vector2){ r + gap + len, b }, t, c);
    DrawLineEx((Vector2){ r, b + gap }, (Vector2){ r, b + gap + len }, t, c);
}

static void DrawSheetTicks(int col, int row, Rectangle trim, float dpi)
{
    float gap = 0.045f * dpi;
    float len = 0.16f * dpi;
    float thick = 2.0f;
    Color c = { 20, 20, 20, 255 };
    float x = trim.x;
    float y = trim.y;
    float r = trim.x + trim.width;
    float b = trim.y + trim.height;

    if (col == 0) {
        DrawLineEx((Vector2){ x - gap - len, y }, (Vector2){ x - gap, y }, thick, c);
        DrawLineEx((Vector2){ x - gap - len, b }, (Vector2){ x - gap, b }, thick, c);
    }
    if (col == CARD_AVERY_COLS - 1) {
        DrawLineEx((Vector2){ r + gap, y }, (Vector2){ r + gap + len, y }, thick, c);
        DrawLineEx((Vector2){ r + gap, b }, (Vector2){ r + gap + len, b }, thick, c);
    }
    if (row == 0) {
        DrawLineEx((Vector2){ x, y - gap - len }, (Vector2){ x, y - gap }, thick, c);
        DrawLineEx((Vector2){ r, y - gap - len }, (Vector2){ r, y - gap }, thick, c);
    }
    if (row == CARD_AVERY_ROWS - 1) {
        DrawLineEx((Vector2){ x, b + gap }, (Vector2){ x, b + gap + len }, thick, c);
        DrawLineEx((Vector2){ r, b + gap }, (Vector2){ r, b + gap + len }, thick, c);
    }
}

static bool SaveTarget(RenderTexture2D target, const char *path)
{
    EndTextureMode();
    Image img = LoadImageFromTexture(target.texture);
    bool ok = false;
    if (img.data != NULL) {
        ImageFlipVertical(&img);
        ok = ExportImage(img, path);
        UnloadImage(img);
    }
    UnloadRenderTexture(target);
    return ok;
}

bool CardExportShop(const CardFace *face, const Font *fonts, int fontCount, const char *path)
{
    if (!face || !path) return false;
    const float dpi = (float)CARD_PRINT_DPI;
    int texW = (int)(CARD_SHOP_W_IN * dpi + 0.5f);
    int texH = (int)(CARD_SHOP_H_IN * dpi + 0.5f);
    RenderTexture2D target = LoadRenderTexture(texW, texH);
    if (target.id == 0) return false;

    Rectangle trim = {
        (CARD_MARK_IN + CARD_BLEED_IN) * dpi,
        (CARD_MARK_IN + CARD_BLEED_IN) * dpi,
        CARD_W_IN * dpi,
        CARD_H_IN * dpi
    };
    Rectangle clip = { 0.0f, 0.0f, (float)texW, (float)texH };

    BeginTextureMode(target);
    ClearBackground(WHITE);
    CardDrawFace(face, trim, clip, fonts, fontCount, true, false);
    DrawCornerMarks(trim, (CARD_BLEED_IN + 0.03f) * dpi, 0.18f * dpi);
    return SaveTarget(target, path);
}

bool CardExportSheet(const CardFace *face, const Font *fonts, int fontCount, const char *path)
{
    if (!face || !path) return false;
    const float dpi = (float)CARD_PRINT_DPI;
    int texW = (int)(CARD_SHEET_W_IN * dpi + 0.5f);
    int texH = (int)(CARD_SHEET_H_IN * dpi + 0.5f);
    RenderTexture2D target = LoadRenderTexture(texW, texH);
    if (target.id == 0) return false;

    Rectangle clip = { 0.0f, 0.0f, (float)texW, (float)texH };
    BeginTextureMode(target);
    ClearBackground(WHITE);
    for (int row = 0; row < CARD_AVERY_ROWS; row++) {
        for (int col = 0; col < CARD_AVERY_COLS; col++) {
            Rectangle trim = {
                (CARD_AVERY_LEFT + (float)col * CARD_W_IN) * dpi,
                (CARD_AVERY_TOP + (float)row * CARD_H_IN) * dpi,
                CARD_W_IN * dpi,
                CARD_H_IN * dpi
            };
            CardDrawFace(face, trim, clip, fonts, fontCount, false, false);
            DrawSheetTicks(col, row, trim, dpi);
        }
    }
    return SaveTarget(target, path);
}
