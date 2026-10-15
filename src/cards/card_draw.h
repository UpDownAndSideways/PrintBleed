/*******************************************************************************
 * card_draw.h — Draw one card face into a trim rectangle
 *
 * `trimPx` is the finished card in pixels. Element boxes are inches and are
 * scaled from that rectangle, so the preview and the 300 DPI export share
 * this function.
 ******************************************************************************/
#ifndef CARD_DRAW_H
#define CARD_DRAW_H

#include "card_doc.h"

Font CardChooseFont(const Font *fonts, int fontCount, int fontId);

/* `clipPx` limits guides and artwork (the editor canvas, or the export page).
 * `bleed` fills the 0.125 in margin around the trim. `guides` draws trim,
 * bleed, and safe-zone lines for the screen only. */
void CardDrawFace(const CardFace *face, Rectangle trimPx, Rectangle clipPx,
                  const Font *fonts, int fontCount, bool bleed, bool guides);

/* Caret position in pixels for the same wrapping used by CardDrawFace. */
void CardTextCaretPx(Font font, const char *text, int caret, float fontPx,
                     float boxWidth, int align, Rectangle boxPx,
                     float *outX, float *outY, float *outH);

#endif
