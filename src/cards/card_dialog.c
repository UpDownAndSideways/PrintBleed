/*******************************************************************************
 * card_dialog.c — Windows open and save dialogs
 ******************************************************************************/
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

#include "card_dialog.h"

#include <stdio.h>
#include <string.h>

bool CardDialogOpen(void *owner, char *out, int cap, const char *filter, const char *title)
{
    if (!out || cap < 2) return false;
    memset(out, 0, (size_t)cap);

    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = out;
    ofn.nMaxFile = (DWORD)cap;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&ofn) == TRUE;
}

bool CardDialogSave(void *owner, char *out, int cap, const char *filter, const char *title,
                    const char *defaultName, const char *defExt)
{
    if (!out || cap < 2) return false;
    memset(out, 0, (size_t)cap);
    if (defaultName && defaultName[0]) {
        snprintf(out, (size_t)cap, "%s", defaultName);
    }

    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = out;
    ofn.nMaxFile = (DWORD)cap;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = defExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
    return GetSaveFileNameA(&ofn) == TRUE;
}
