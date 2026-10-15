/*******************************************************************************
 * card_export.h — 300 DPI PNG export
 ******************************************************************************/
#ifndef CARD_EXPORT_H
#define CARD_EXPORT_H

#include "card_doc.h"
#include <stdbool.h>

/* Print-shop card: trim + bleed + crop marks, 4.25 x 2.75 in. */
bool CardExportShop(const CardFace *face, const Font *fonts, int fontCount, const char *path);

/* Letter sheet: 10 cards on the Avery 8371 grid, with ticks in the margins. */
bool CardExportSheet(const CardFace *face, const Font *fonts, int fontCount, const char *path);

#endif
