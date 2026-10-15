/*******************************************************************************
 * card_dialog.h — Windows open and save dialogs
 *
 * Kept out of the raylib sources so windows.h macros (DrawText, CloseWindow)
 * never reach the renderer.
 ******************************************************************************/
#ifndef CARD_DIALOG_H
#define CARD_DIALOG_H

#include <stdbool.h>

/* `owner` is the raylib window handle, or NULL. */
bool CardDialogOpen(void *owner, char *out, int cap, const char *filter, const char *title);
bool CardDialogSave(void *owner, char *out, int cap, const char *filter, const char *title,
                    const char *defaultName, const char *defExt);

#endif
