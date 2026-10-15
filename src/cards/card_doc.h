/*******************************************************************************
 * card_doc.h — Business card document, templates, undo, and project files
 *
 * Coordinates are inches. The origin is the top-left of the trim (the finished
 * 3.5 x 2 in card). Bleed extends 0.125 in outside that rectangle.
 ******************************************************************************/
#ifndef CARD_DOC_H
#define CARD_DOC_H

#include "raylib.h"
#include <stdbool.h>

#define CARD_W_IN 3.5f
#define CARD_H_IN 2.0f
#define CARD_BLEED_IN 0.125f
#define CARD_SAFE_IN 0.125f
#define CARD_MARK_IN 0.25f
#define CARD_PRINT_DPI 300
#define CARD_SHOP_W_IN (CARD_W_IN + 2.0f * CARD_BLEED_IN + 2.0f * CARD_MARK_IN)
#define CARD_SHOP_H_IN (CARD_H_IN + 2.0f * CARD_BLEED_IN + 2.0f * CARD_MARK_IN)
#define CARD_SHEET_W_IN 8.5f
#define CARD_SHEET_H_IN 11.0f
#define CARD_AVERY_LEFT 0.75f
#define CARD_AVERY_TOP 0.5f
#define CARD_AVERY_COLS 2
#define CARD_AVERY_ROWS 5

#define CARD_MAX_ELEMENTS 32
#define CARD_TEXT_MAX 256
#define CARD_PATH_MAX 512
#define CARD_UNDO_MAX 24
#define CARD_SIDE_COUNT 2
#define CARD_TEMPLATE_COUNT 5

typedef enum CardSide {
    CARD_SIDE_FRONT = 0,
    CARD_SIDE_BACK = 1
} CardSide;

typedef enum CardElemKind {
    CARD_ELEM_TEXT = 0,
    CARD_ELEM_IMAGE = 1
} CardElemKind;

typedef enum CardAlign {
    CARD_ALIGN_LEFT = 0,
    CARD_ALIGN_CENTER = 1,
    CARD_ALIGN_RIGHT = 2
} CardAlign;

typedef enum CardTemplate {
    CARD_TEMPLATE_CLASSIC = 0,
    CARD_TEMPLATE_CENTERED,
    CARD_TEMPLATE_COLORBAR,
    CARD_TEMPLATE_MINIMAL,
    CARD_TEMPLATE_BLANK
} CardTemplate;

typedef struct CardElement {
    CardElemKind kind;
    Rectangle box;                 /* Inches, relative to the trim origin. */
    char text[CARD_TEXT_MAX];
    float fontSize;                /* Points. 1 pt = 1/72 in.              */
    Color color;
    CardAlign align;
    int fontId;                    /* 0 = Segoe UI, 1 = Georgia.           */
    char path[CARD_PATH_MAX];      /* File path, or a generated asset id.  */
    Image image;
    Texture2D texture;
    bool hasImage;
} CardElement;

typedef struct CardFace {
    Color background;
    int count;
    CardElement items[CARD_MAX_ELEMENTS];
} CardFace;

typedef struct CardDoc {
    CardFace faces[CARD_SIDE_COUNT];
    int activeSide;
    int selected;                  /* -1 when nothing is selected.         */
} CardDoc;

typedef struct CardHistory {
    CardDoc *undo[CARD_UNDO_MAX];
    int undoCount;
    CardDoc *redo[CARD_UNDO_MAX];
    int redoCount;
} CardHistory;

void CardDocInit(CardDoc *doc);
void CardDocFree(CardDoc *doc);

const char *CardTemplateName(int templateId);
const char *CardSideName(int side);

/* Replaces one side with a starting layout. Clears the selection. */
void CardApplyTemplate(CardDoc *doc, int side, int templateId);

int CardFaceAddText(CardFace *face, Rectangle box, const char *text,
                    float pt, Color color, int align, int fontId);
int CardFaceAddImageFile(CardFace *face, const char *path, Rectangle box);
int CardFaceAddImageFit(CardFace *face, const char *path);
bool CardElementReplaceFile(CardElement *elem, const char *path);
void CardFaceDelete(CardFace *face, int index);
int CardFaceHitTest(const CardFace *face, float x, float y);
void CardElementClamp(CardElement *elem);

void CardTextInsert(CardElement *elem, int *caret, const char *utf8);
void CardTextErase(CardElement *elem, int *caret, bool backspace);

bool CardDocSave(const CardDoc *doc, const char *path);
/* On failure the document is left empty and safe to free. */
bool CardDocLoad(CardDoc *doc, const char *path);

void CardHistoryInit(CardHistory *history);
void CardHistoryFree(CardHistory *history);
void CardHistoryPush(CardHistory *history, const CardDoc *before);
bool CardHistoryUndo(CardHistory *history, CardDoc *live);
bool CardHistoryRedo(CardHistory *history, CardDoc *live);

#endif
