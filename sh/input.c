/**
 * @file input.c
 *
 * Yori shell command entry from a console
 *
 * Copyright (c) 2017 Malcolm J. Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "yori.h"

/**
 Returns the coordinates in the console if the cursor is moved by a given
 number of cells.  Note the input value is signed, as this routine can move
 forwards (positive values) or backwards (negative values.)

 @param PlacesToMove The number of cells to move relative to the cursor.

 @return The X/Y coordinates of the cell if the cursor was moved the
         specified number of places.
 */
COORD
YoriShDetermineCellLocationIfMoved(
    __in INT PlacesToMove
    )
{
    CONSOLE_SCREEN_BUFFER_INFO ScreenInfo;
    INT PlacesToMoveDown;
    INT PlacesToMoveRight;
    COORD NewPosition;
    HANDLE ConsoleHandle;

    ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    // MSFIX Error handling
    GetConsoleScreenBufferInfo(ConsoleHandle, &ScreenInfo);

    PlacesToMoveDown = PlacesToMove / ScreenInfo.dwSize.X;
    PlacesToMoveRight = PlacesToMove % ScreenInfo.dwSize.X;
    if (PlacesToMoveRight > 0) {
        if (PlacesToMoveRight + ScreenInfo.dwCursorPosition.X >= ScreenInfo.dwSize.X) {
            PlacesToMoveRight -= ScreenInfo.dwSize.X;
            PlacesToMoveDown++;
        }
    } else {

        if (PlacesToMoveRight + ScreenInfo.dwCursorPosition.X < 0) {
            PlacesToMoveRight += ScreenInfo.dwSize.X;
            PlacesToMoveDown--;
        }
    }

    NewPosition.Y = (WORD)(ScreenInfo.dwCursorPosition.Y + PlacesToMoveDown);
    NewPosition.X = (WORD)(ScreenInfo.dwCursorPosition.X + PlacesToMoveRight);

    if (NewPosition.Y >= ScreenInfo.dwSize.Y) {
        SMALL_RECT ContentsToPreserve;
        SMALL_RECT ContentsToErase;
        COORD Origin;
        CHAR_INFO NewChar;
        WORD LinesToMove;

        LinesToMove = (WORD)(NewPosition.Y - ScreenInfo.dwSize.Y + 1);

        ContentsToPreserve.Left = 0;
        ContentsToPreserve.Right = (WORD)(ScreenInfo.dwSize.X - 1);
        ContentsToPreserve.Top = LinesToMove;
        ContentsToPreserve.Bottom = (WORD)(ScreenInfo.dwSize.Y - 1);

        ContentsToErase.Left = 0;
        ContentsToErase.Right = (WORD)(ScreenInfo.dwSize.X - 1);
        ContentsToErase.Top = (WORD)(ScreenInfo.dwSize.Y - LinesToMove);
        ContentsToErase.Bottom = (WORD)(ScreenInfo.dwSize.Y - 1);

        Origin.X = 0;
        Origin.Y = 0;

        NewChar.Char.UnicodeChar = ' ';
        NewChar.Attributes = ScreenInfo.wAttributes;
        ScrollConsoleScreenBuffer(ConsoleHandle, &ContentsToPreserve, NULL, Origin, &NewChar);

        ScreenInfo.dwCursorPosition.Y = (WORD)(ScreenInfo.dwCursorPosition.Y - LinesToMove);
        SetConsoleCursorPosition(ConsoleHandle, ScreenInfo.dwCursorPosition);

        NewPosition.Y = (WORD)(NewPosition.Y - LinesToMove);
    }

    return NewPosition;
}

/**
 Determine the offset within the input buffer of specified X,Y coordinates
 relative to the console screen buffer.

 @param Buffer The current input buffer.

 @param TargetCoordinates The coordinates to check against the input buffer.

 @param StringOffset On successful completion, updated to point to the
        location within the string of the coordinates.  Note this can point
        to one past the length of the string.

 @return TRUE to indicate the specified coordinates are within the string
         range, FALSE to indicate the coordinates are not within the string.
 */
BOOL
YoriShStringOffsetFromCoordinates(
    __in PYORI_SH_INPUT_BUFFER Buffer,
    __in COORD TargetCoordinates,
    __out PDWORD StringOffset
    )
{
    CONSOLE_SCREEN_BUFFER_INFO ScreenInfo;
    HANDLE ConsoleHandle;
    DWORD StartOfString;
    DWORD CursorPosition;
    DWORD TargetPosition;

    UNREFERENCED_PARAMETER(TargetCoordinates);

    ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    if (!GetConsoleScreenBufferInfo(ConsoleHandle, &ScreenInfo)) {
        return FALSE;
    }

    TargetPosition = TargetCoordinates.Y * ScreenInfo.dwSize.X + TargetCoordinates.X;
    CursorPosition = ScreenInfo.dwCursorPosition.Y * ScreenInfo.dwSize.X + ScreenInfo.dwCursorPosition.X;

    if (Buffer->PreviousCurrentOffset > CursorPosition) {
        return FALSE;
    }
    StartOfString = CursorPosition - Buffer->PreviousCurrentOffset;

    if (TargetPosition < StartOfString) {
        return FALSE;
    }

    if (TargetPosition > StartOfString + Buffer->String.LengthInChars) {
        return FALSE;
    }

    *StringOffset = TargetPosition - StartOfString;
    return TRUE;
}

/**
 Move the cursor from its current position.  Note the input value is signed,
 as this routine can move forwards (positive values) or backwards (negative
 values.)

 @param PlacesToMove The number of cells to move the cursor.
 */
VOID
YoriShMoveCursor(
    __in INT PlacesToMove
    )
{
    COORD NewPosition = YoriShDetermineCellLocationIfMoved(PlacesToMove);
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), NewPosition);
}

/**
 After a key has been pressed, capture the current state of the buffer so
 that it is ready to accept transformations as a result of the key
 being pressed.

 @param Buffer Pointer to the input buffer whose state should be captured.
 */
VOID
YoriShPrepareForNextKey(
    __in PYORI_SH_INPUT_BUFFER Buffer
    )
{
    Buffer->PriorTabCount = Buffer->TabContext.TabCount;
}

/**
 Cleanup after processing a key press.

 @param Buffer The current state of the input buffer.
 */
VOID
YoriShPostKeyPress(
    __in PYORI_SH_INPUT_BUFFER Buffer
    )
{
    //
    //  If the number of Tabs hasn't changed, the tab context can be torn
    //  down since the user is not repeatedly pressing Tab.
    //

    if (Buffer->PriorTabCount == Buffer->TabContext.TabCount &&
        Buffer->SuggestionString.LengthInChars == 0) {

        YoriShClearTabCompletionMatches(Buffer);
    }
}

/**
 After a key has been pressed and processed, display the resulting buffer.

 @param Buffer Pointer to the input buffer to display.
 */
VOID
YoriShDisplayAfterKeyPress(
    __in PYORI_SH_INPUT_BUFFER Buffer
    )
{
    DWORD NumberWritten;
    DWORD NumberToWrite = 0;
    DWORD NumberToFill = 0;
    COORD WritePosition;
    COORD FillPosition;
    COORD SuggestionPosition;
    CONSOLE_SCREEN_BUFFER_INFO ScreenInfo;
    WORD FillAttributes;
    HANDLE hConsole;

    YoriLibRedrawSelection(&Buffer->Selection);

    WritePosition.X = 0;
    WritePosition.Y = 0;
    SuggestionPosition.X = 0;
    SuggestionPosition.Y = 0;
    FillPosition.X = 0;
    FillPosition.Y = 0;

    //
    //  Re-render the text if part of the input string has changed,
    //  or if the location of the cursor in the input string has changed
    //

    if (Buffer->DirtyBeginOffset != 0 || Buffer->DirtyLength != 0 ||
        Buffer->SuggestionDirty ||
        Buffer->PreviousCurrentOffset != Buffer->CurrentOffset) {

        // MSFIX Error handling
        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetConsoleScreenBufferInfo(hConsole, &ScreenInfo)) {
            FillAttributes = ScreenInfo.wAttributes;
        } else {
            FillAttributes = YoriLibVtGetDefaultColor();
        }

        //
        //  Calculate the number of characters truncated from the currently
        //  displayed buffer.
        //

        if (Buffer->PreviousCharsDisplayed > Buffer->String.LengthInChars + Buffer->SuggestionString.LengthInChars) {
            NumberToFill = Buffer->PreviousCharsDisplayed - Buffer->String.LengthInChars - Buffer->SuggestionString.LengthInChars;
        }

        //
        //  Calculate the locations to write both the new text as well as where
        //  to erase any previous test.
        //
        //  Calculate where the buffer will end and discard the result; this is
        //  done to ensure the screen buffer is scrolled so the whole output
        //  has somewhere to go.
        //

        if (Buffer->DirtyBeginOffset < Buffer->String.LengthInChars && Buffer->DirtyLength > 0) {
            if (Buffer->DirtyBeginOffset + Buffer->DirtyLength > Buffer->String.LengthInChars) {
                NumberToWrite = Buffer->String.LengthInChars - Buffer->DirtyBeginOffset;
            } else {
                NumberToWrite = Buffer->DirtyLength;
            }
            YoriShDetermineCellLocationIfMoved((-1 * Buffer->PreviousCurrentOffset) + Buffer->DirtyBeginOffset + NumberToWrite);
            WritePosition = YoriShDetermineCellLocationIfMoved((-1 * Buffer->PreviousCurrentOffset) + Buffer->DirtyBeginOffset);
        }

        if (Buffer->SuggestionString.LengthInChars > 0) {
            YoriShDetermineCellLocationIfMoved(-1 * Buffer->PreviousCurrentOffset + Buffer->String.LengthInChars + Buffer->SuggestionString.LengthInChars);
            SuggestionPosition = YoriShDetermineCellLocationIfMoved(-1 * Buffer->PreviousCurrentOffset + Buffer->String.LengthInChars);
        }

        if (NumberToFill) {
            YoriShDetermineCellLocationIfMoved(-1 * Buffer->PreviousCurrentOffset + Buffer->String.LengthInChars + Buffer->SuggestionString.LengthInChars + NumberToFill);
            FillPosition = YoriShDetermineCellLocationIfMoved(-1 * Buffer->PreviousCurrentOffset + Buffer->String.LengthInChars + Buffer->SuggestionString.LengthInChars);
        }

        if (NumberToWrite > 0 ||
            Buffer->SuggestionString.LengthInChars > 0 ||
            Buffer->CurrentOffset != Buffer->PreviousCurrentOffset) {

            //
            //  Now that we know where the text should go, advance the cursor
            //  and render the text.
            //

            YoriShMoveCursor(Buffer->CurrentOffset - Buffer->PreviousCurrentOffset);

            if (NumberToWrite) {
                WriteConsoleOutputCharacter(hConsole, &Buffer->String.StartOfString[Buffer->DirtyBeginOffset], NumberToWrite, WritePosition, &NumberWritten);
                FillConsoleOutputAttribute(hConsole, ScreenInfo.wAttributes, NumberToWrite, WritePosition, &NumberWritten);
            }

            if (Buffer->SuggestionString.LengthInChars > 0) {
                WriteConsoleOutputCharacter(hConsole, Buffer->SuggestionString.StartOfString, Buffer->SuggestionString.LengthInChars, SuggestionPosition, &NumberWritten);
                FillConsoleOutputAttribute(hConsole, (USHORT)((ScreenInfo.wAttributes & 0xF0) | FOREGROUND_INTENSITY), Buffer->SuggestionString.LengthInChars, SuggestionPosition, &NumberWritten);
            }
        }

        //
        //  If there are additional cells to empty due to truncation, display
        //  those now.
        //

        if (NumberToFill) {
            FillConsoleOutputCharacter(hConsole, ' ', NumberToFill, FillPosition, &NumberWritten);
            FillConsoleOutputAttribute(hConsole, ScreenInfo.wAttributes, NumberToFill, FillPosition, &NumberWritten);
        }

        Buffer->PreviousCurrentOffset = Buffer->CurrentOffset;
        Buffer->PreviousCharsDisplayed = Buffer->String.LengthInChars + Buffer->SuggestionString.LengthInChars;
        Buffer->DirtyBeginOffset = 0;
        Buffer->DirtyLength = 0;
        Buffer->SuggestionDirty = FALSE;
    }
}

/**
 Check that the string has enough characters to hold the new number of
 characters including a NULL terminator.  If it doesn't, reallocate a
 new buffer that is large enough to hold the new number of characters.
 Note that since this is an allocation it can fail.

 @param String Pointer to the current string.

 @param CharactersNeeded The number of characters that are needed in
        the buffer.

 @return TRUE to indicate the current buffer is large enough or it was
         successfully reallocated, FALSE to indicate allocation failure.
 */
BOOL
YoriShEnsureStringHasEnoughCharacters(
    __inout PYORI_STRING String,
    __in DWORD CharactersNeeded
    )
{
    while (CharactersNeeded + 1 >= String->LengthAllocated) {
        if (!YoriLibReallocateString(String, String->LengthAllocated * 4)) {
            return FALSE;
        }
    }
    return TRUE;
}

/**
 When YORIQUICKEDIT is set, disable the console's QuickEdit capabilities and
 allow Yori to process mouse input so it can use its internal QuickEdit
 support.

 @param Buffer Pointer to the input buffer which indicates whether this
        behavior is requested.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriShConfigureMouseForPrompt(
    __in PYORI_SH_INPUT_BUFFER Buffer
    )
{
    HANDLE ConsoleHandle;
    DWORD ConsoleMode;
    if (!Buffer->YoriQuickEdit) {
        return TRUE;
    }

    ConsoleHandle = GetStdHandle(STD_INPUT_HANDLE);
    if (!GetConsoleMode(ConsoleHandle, &ConsoleMode)) {
        return FALSE;
    }

    //
    //  Set the same input settings as the base settings, but clear the
    //  extended settings.  We have no way to query these (sigh) but
    //  this has the effect of turning off console's quickedit.
    //

    SetConsoleMode(ConsoleHandle, ConsoleMode | ENABLE_EXTENDED_FLAGS);
    return TRUE;
}

/**
 When YORIQUICKEDIT is set, enable the console's QuickEdit capabilities to
 allow QuickEdit to be used with external programs.

 @param Buffer Pointer to the input buffer which indicates whether this
        behavior is requested.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriShConfigureMouseForPrograms(
    __in PYORI_SH_INPUT_BUFFER Buffer
    )
{
    HANDLE ConsoleHandle;
    DWORD ConsoleMode;
    if (!Buffer->YoriQuickEdit) {
        return TRUE;
    }

    ConsoleHandle = GetStdHandle(STD_INPUT_HANDLE);
    if (!GetConsoleMode(ConsoleHandle, &ConsoleMode)) {
        return FALSE;
    }

    //
    //  Set the same input settings as the base settings, but add in console
    //  QuickEdit.
    //

    SetConsoleMode(ConsoleHandle, ConsoleMode | ENABLE_QUICK_EDIT_MODE | ENABLE_EXTENDED_FLAGS);
    return TRUE;
}

/**
 A handler to invoke when at the input prompt for Ctrl+C and app close
 signals.  Generally, when inputting we want to ignore Ctrl+C etc and just
 read them as keystrokes.  For app close however, we want to be able to save
 state before terminating.  Frustratingly, this handler is invoked and all
 other threads are immediately terminated, so any state must be saved
 right here.

 @param CtrlType Indicates the type of the control message.

 @return FALSE to indicate that this handler performed no processing and the
         signal should be sent to the next handler.  TRUE to indicate this
         handler processed the signal and no other handler should be invoked.
 */
BOOL WINAPI
YoriShAppCloseCtrlHandler(
    __in DWORD CtrlType
    )
{
    if (CtrlType == CTRL_CLOSE_EVENT ||
        CtrlType == CTRL_LOGOFF_EVENT ||
        CtrlType == CTRL_SHUTDOWN_EVENT) {

        YoriShSaveHistoryToFile();
        return FALSE;
    }

    if (CtrlType == CTRL_C_EVENT ||
        CtrlType == CTRL_BREAK_EVENT) {

        return TRUE;
    }

    return TRUE;
}

/**
 NULL terminate the input buffer, and display a carriage return, in preparation
 for parsing and executing the input.

 @param Buffer Pointer to the input buffer.
 */
VOID
YoriShTerminateInput(
    __inout PYORI_SH_INPUT_BUFFER Buffer
    )
{
    if (Buffer->SuggestionString.LengthInChars > 0) {
        Buffer->SuggestionDirty = TRUE;
    }
    YoriLibFreeStringContents(&Buffer->SuggestionString);
    YoriLibFreeStringContents(&Buffer->SearchString);
    SetConsoleCtrlHandler(YoriShAppCloseCtrlHandler, FALSE);
    YoriShDisplayAfterKeyPress(Buffer);
    YoriShPostKeyPress(Buffer);
    YoriShClearTabCompletionMatches(Buffer);
    YoriLibCleanupSelection(&Buffer->Selection);
    Buffer->String.StartOfString[Buffer->String.LengthInChars] = '\0';
    YoriShMoveCursor(Buffer->String.LengthInChars - Buffer->CurrentOffset);
    YoriShConfigureMouseForPrograms(Buffer);
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("\n"));
}


/**
 Empty the current input buffer.

 @param Buffer Pointer to the buffer to empty.
 */
VOID
YoriShClearInput(
    __inout PYORI_SH_INPUT_BUFFER Buffer
    )
{
    YoriLibFreeStringContents(&Buffer->SuggestionString);
    YoriLibFreeStringContents(&Buffer->SearchString);
    YoriShClearTabCompletionMatches(Buffer);
    Buffer->String.LengthInChars = 0;
    Buffer->CurrentOffset = 0;
    Buffer->SearchMode = FALSE;
    YoriLibClearSelection(&Buffer->Selection);
}

/**
 Based on the search text entered so far, find the first match within the
 main string and set the current offset to it.

 @param Buffer Pointer to the input buffer to update.
 */
VOID
YoriShUpdateSelectionWithSearchResult(
    __inout PYORI_SH_INPUT_BUFFER Buffer
    )
{
    DWORD StringOffsetOfMatch;

    //
    //  MSFIX Would like to do something with selection for this, but that
    //  implies having a selection that follows text around lines rather
    //  than a rectangle
    //

    if (YoriLibFindFirstMatchingSubstringInsensitive(&Buffer->String, 1, &Buffer->SearchString, &StringOffsetOfMatch)) {
        Buffer->CurrentOffset = StringOffsetOfMatch + Buffer->SearchString.LengthInChars;
    } else {
        Buffer->CurrentOffset = Buffer->PreSearchOffset;
    }
}


/**
 Perform the necessary buffer transformations to implement backspace.

 @param Buffer Pointer to the buffer to apply backspace to.

 @param Count The number of backspace operations to apply.
 */
VOID
YoriShBackspace(
    __inout PYORI_SH_INPUT_BUFFER Buffer,
    __in DWORD Count
    )
{
    DWORD CountToUse;

    CountToUse = Count;

    //
    //  When updating the search buffer, backspace can only remove from the
    //  end.
    //

    if (Buffer->SearchMode) {
        if (CountToUse > Buffer->SearchString.LengthInChars) {
            CountToUse = Buffer->SearchString.LengthInChars;
        }

        Buffer->SearchString.LengthInChars -= CountToUse;

        YoriShUpdateSelectionWithSearchResult(Buffer);
        return;
    }

    //
    //  On the regular buffer, we may have to shuffle characters around.
    //

    if (Buffer->CurrentOffset < CountToUse) {

        CountToUse = Buffer->CurrentOffset;
    }

    if (Buffer->CurrentOffset != Buffer->String.LengthInChars) {
        memmove(&Buffer->String.StartOfString[Buffer->CurrentOffset - CountToUse],
                &Buffer->String.StartOfString[Buffer->CurrentOffset],
                (Buffer->String.LengthInChars - Buffer->CurrentOffset) * sizeof(TCHAR));
    }

    if (Buffer->DirtyLength == 0) {
        Buffer->DirtyBeginOffset = Buffer->CurrentOffset - CountToUse;
        Buffer->DirtyLength = Buffer->String.LengthInChars - Buffer->DirtyBeginOffset;
    } else {
        if (Buffer->CurrentOffset - CountToUse < Buffer->DirtyBeginOffset) {
            Buffer->DirtyLength += Buffer->DirtyBeginOffset - (Buffer->CurrentOffset - CountToUse);
            Buffer->DirtyBeginOffset = Buffer->CurrentOffset - CountToUse;
        }
        if (Buffer->DirtyBeginOffset + Buffer->DirtyLength < Buffer->String.LengthInChars) {
            Buffer->DirtyLength = Buffer->String.LengthInChars - Buffer->DirtyBeginOffset;
        }
    }

    Buffer->CurrentOffset -= CountToUse;
    Buffer->String.LengthInChars -= CountToUse;

    Buffer->SuggestionDirty = TRUE;
    YoriLibFreeStringContents(&Buffer->SuggestionString);
}

/**
 If a selection region is active and covers the input string, delete the
 selected range of the input string and leave the cursor at the point where
 the selection was to allow for a subsequent insert.

 @param Buffer Pointer to the input buffer describing the selection region.

 @return TRUE if the region was successfully removed; FALSE if no selection
         is active or the selection did not apply to the command input string.
 */
BOOL
YoriShOverwriteSelectionIfInInput(
    __inout PYORI_SH_INPUT_BUFFER Buffer
    )
{

    DWORD StartStringOffset;
    DWORD EndStringOffset;
    DWORD Length;
    COORD StartOfSelection;

    //
    //  No selection, nothing to overwrite
    //

    if (!YoriLibIsSelectionActive(&Buffer->Selection)) {
        return FALSE;
    }

    //
    //  Currently only support operating on one line at a time, to avoid
    //  trying to define the screwy behavior of multiple discontiguous
    //  ranges.
    //

    if (Buffer->Selection.CurrentlySelected.Bottom != Buffer->Selection.CurrentlySelected.Top) {
        return FALSE;
    }

    StartOfSelection.X = Buffer->Selection.CurrentlySelected.Left;
    StartOfSelection.Y = Buffer->Selection.CurrentlySelected.Top;

    if (!YoriShStringOffsetFromCoordinates(Buffer, StartOfSelection, &StartStringOffset)) {
        return FALSE;
    }

    StartOfSelection.X = Buffer->Selection.CurrentlySelected.Right;

    if (!YoriShStringOffsetFromCoordinates(Buffer, StartOfSelection, &EndStringOffset)) {
        return FALSE;
    }

    Length = EndStringOffset - StartStringOffset + 1;

    if (StartStringOffset + Length > Buffer->String.LengthInChars) {
        if (StartStringOffset > Buffer->String.LengthInChars) {
            return FALSE;
        }
        Length = Buffer->String.LengthInChars - StartStringOffset;
    }

    Buffer->CurrentOffset = StartStringOffset + Length;
    YoriShBackspace(Buffer, Length);
    return TRUE;
}

/**
 Apply incoming characters to an input buffer.

 @param Buffer The input buffer to apply new characters to.

 @param String A yori string to enter into the buffer.
 */
VOID
YoriShAddYoriStringToInput(
    __inout PYORI_SH_INPUT_BUFFER Buffer,
    __in PYORI_STRING String
    )
{
    BOOL KeepSuggestions;

    //
    //  Need more allocated than populated due to NULL termination
    //

    KeepSuggestions = FALSE;

    if (!Buffer->SearchMode) {
        YoriShOverwriteSelectionIfInInput(Buffer);
        ASSERT(Buffer->String.LengthAllocated > Buffer->String.LengthInChars);
        ASSERT(Buffer->String.LengthInChars >= Buffer->CurrentOffset);

        //
        //  If the characters are at the end of the string, see if a
        //  current suggestion can be retained.
        //

        if (Buffer->String.LengthInChars == Buffer->CurrentOffset) {
            KeepSuggestions = TRUE;
        }
    }

    if (!KeepSuggestions) {
        YoriLibFreeStringContents(&Buffer->SuggestionString);
        YoriShClearTabCompletionMatches(Buffer);
    } else {
        YoriShTrimSuggestionList(Buffer, String);
    }
    Buffer->SuggestionDirty = TRUE;

    //
    //  If we're inserting, shuffle the data; if we're overwriting, clobber
    //  the data.
    //

    if (Buffer->SearchMode) {
        if (Buffer->SearchString.MemoryToFree == NULL) {
            if (!YoriLibAllocateString(&Buffer->SearchString, String->LengthInChars * 4)) {
                return;
            }
        } else if (!YoriShEnsureStringHasEnoughCharacters(&Buffer->SearchString, Buffer->SearchString.LengthInChars + String->LengthInChars)) {
            return;
        }

        memcpy(&Buffer->SearchString.StartOfString[Buffer->SearchString.LengthInChars], String->StartOfString, String->LengthInChars * sizeof(TCHAR));
        Buffer->SearchString.LengthInChars += String->LengthInChars;

        YoriShUpdateSelectionWithSearchResult(Buffer);

    } else if (Buffer->InsertMode) {
        if (!YoriShEnsureStringHasEnoughCharacters(&Buffer->String, Buffer->String.LengthInChars + String->LengthInChars)) {
            return;
        }

        //
        //  Trim any trailing spaces if we're "inserting" before them.
        //

        while (Buffer->String.LengthInChars > 0 &&
               Buffer->String.LengthInChars != Buffer->CurrentOffset) {
            if (Buffer->String.StartOfString[Buffer->String.LengthInChars - 1] == ' ') {
                Buffer->String.LengthInChars--;
            } else {
                break;
            }
        }
        if (Buffer->String.LengthInChars != Buffer->CurrentOffset) {
            memmove(&Buffer->String.StartOfString[Buffer->CurrentOffset + String->LengthInChars],
                    &Buffer->String.StartOfString[Buffer->CurrentOffset],
                    (Buffer->String.LengthInChars - Buffer->CurrentOffset) * sizeof(TCHAR));
        }
        Buffer->String.LengthInChars += String->LengthInChars;
        memcpy(&Buffer->String.StartOfString[Buffer->CurrentOffset], String->StartOfString, String->LengthInChars * sizeof(TCHAR));

        if (Buffer->DirtyLength == 0) {
            Buffer->DirtyBeginOffset = Buffer->CurrentOffset;
            Buffer->DirtyLength = Buffer->String.LengthInChars - Buffer->CurrentOffset;
        } else {
            if (Buffer->CurrentOffset < Buffer->DirtyBeginOffset) {
                Buffer->DirtyLength += Buffer->DirtyBeginOffset - Buffer->CurrentOffset;
                Buffer->DirtyBeginOffset = Buffer->CurrentOffset;
            }
            if (Buffer->DirtyBeginOffset + Buffer->DirtyLength < Buffer->String.LengthInChars) {
                Buffer->DirtyLength = Buffer->String.LengthInChars - Buffer->DirtyBeginOffset;
            }
        }
        Buffer->CurrentOffset += String->LengthInChars;
    } else {
        if (!YoriShEnsureStringHasEnoughCharacters(&Buffer->String, Buffer->CurrentOffset + String->LengthInChars)) {
            return;
        }
        memcpy(&Buffer->String.StartOfString[Buffer->CurrentOffset], String->StartOfString, String->LengthInChars * sizeof(TCHAR));
        Buffer->CurrentOffset += String->LengthInChars;
        if (Buffer->CurrentOffset > Buffer->String.LengthInChars) {
            Buffer->String.LengthInChars = Buffer->CurrentOffset;
        }
        if (Buffer->DirtyLength == 0) {
            Buffer->DirtyBeginOffset = Buffer->CurrentOffset - String->LengthInChars;
            Buffer->DirtyLength = String->LengthInChars;
        } else {
            if (Buffer->CurrentOffset - String->LengthInChars < Buffer->DirtyBeginOffset) {
                Buffer->DirtyLength += Buffer->DirtyBeginOffset - (Buffer->CurrentOffset - String->LengthInChars);
                Buffer->DirtyBeginOffset = Buffer->CurrentOffset - String->LengthInChars;
            }
            if (Buffer->DirtyBeginOffset + Buffer->DirtyLength < Buffer->CurrentOffset) {
                Buffer->DirtyLength = Buffer->CurrentOffset - Buffer->DirtyBeginOffset;
            }
        }
    }

    ASSERT(Buffer->String.LengthAllocated > Buffer->String.LengthInChars);
    ASSERT(Buffer->String.LengthInChars >= Buffer->CurrentOffset);
}

/**
 Add a NULL terminated string to the input buffer.  This could be an
 append, an insert in the middle, or an overwrite.

 @param Buffer The input buffer context.

 @param String the NULL terminated string to append.
 */
VOID
YoriShAddCStringToInput(
    __inout PYORI_SH_INPUT_BUFFER Buffer,
    __in LPTSTR String
    )
{
    YORI_STRING YoriString;

    YoriLibConstantString(&YoriString, String);
    YoriShAddYoriStringToInput(Buffer, &YoriString);
}


/**
 Move the current cursor offset within the buffer to the argument before the
 one that is selected.  This requires parsing the arguments and moving the
 current offset into the last one.  This is used to implement Ctrl+Left
 functionality.  On error, the offset is not updated.

 @param Buffer Pointer to the current input buffer context.
 */
VOID
YoriShMoveCursorToPriorArgument(
    __in PYORI_SH_INPUT_BUFFER Buffer
    )
{
    YORI_SH_CMD_CONTEXT CmdContext;
    LPTSTR NewString = NULL;
    DWORD BeginCurrentArg = 0;
    DWORD EndCurrentArg = 0;
    DWORD NewStringLen;

    if (!YoriShParseCmdlineToCmdContext(&Buffer->String, Buffer->CurrentOffset, &CmdContext)) {
        return;
    }

    if (CmdContext.ArgC == 0) {
        YoriShFreeCmdContext(&CmdContext);
        return;
    }

    if (CmdContext.CurrentArg > 0) {

        //
        //  If we're on the final argument but not at the first letter, go to
        //  the beginning of the current argument.
        //
        //  MSFIX This is relying on the current string offset being compared
        //  against the new string offset, which is not guaranteed to be
        //  correct.  Doing this properly requires parser support to indicate
        //  "current position within argument."
        //

        if (CmdContext.CurrentArg < CmdContext.ArgC) {
            NewString = YoriShBuildCmdlineFromCmdContext(&CmdContext, FALSE, &BeginCurrentArg, &EndCurrentArg);
            if (Buffer->CurrentOffset <= BeginCurrentArg) {
                YoriLibDereference(NewString);
                NewString = NULL;
                CmdContext.CurrentArg--;
            }
        } else {
            CmdContext.CurrentArg--;
        }
    }

    if (NewString == NULL) {
        NewString = YoriShBuildCmdlineFromCmdContext(&CmdContext, FALSE, &BeginCurrentArg, &EndCurrentArg);
    }

    if (NewString != NULL) {
        NewStringLen = _tcslen(NewString);
        if (!YoriShEnsureStringHasEnoughCharacters(&Buffer->String, NewStringLen)) {
            return;
        }
        YoriLibYPrintf(&Buffer->String, _T("%s"), NewString);
        Buffer->CurrentOffset = BeginCurrentArg;
        if (Buffer->CurrentOffset > Buffer->String.LengthInChars) {
            Buffer->CurrentOffset = Buffer->String.LengthInChars;
        }
        YoriLibDereference(NewString);
    }

    YoriShFreeCmdContext(&CmdContext);
}

/**
 Move the current cursor offset within the buffer to the argument following the
 one that is selected.  This requires parsing the arguments and moving the
 current offset into the next one.  This is used to implement Ctrl+Right
 functionality.  On error, the offset is not updated.

 @param Buffer Pointer to the current input buffer context.
 */
VOID
YoriShMoveCursorToNextArgument(
    __in PYORI_SH_INPUT_BUFFER Buffer
    )
{
    YORI_SH_CMD_CONTEXT CmdContext;
    LPTSTR NewString;
    DWORD BeginCurrentArg;
    DWORD EndCurrentArg;
    BOOL MoveToEnd = FALSE;

    if (!YoriShParseCmdlineToCmdContext(&Buffer->String, Buffer->CurrentOffset, &CmdContext)) {
        return;
    }

    if (CmdContext.ArgC == 0) {
        YoriShFreeCmdContext(&CmdContext);
        return;
    }

    if (CmdContext.CurrentArg + 1 < (DWORD)CmdContext.ArgC) {
        CmdContext.CurrentArg++;
    } else {
        MoveToEnd = TRUE;
    }

    NewString = YoriShBuildCmdlineFromCmdContext(&CmdContext, FALSE, &BeginCurrentArg, &EndCurrentArg);

    if (NewString != NULL) {
        DWORD NewStringLen;
        NewStringLen = _tcslen(NewString);
        if (!YoriShEnsureStringHasEnoughCharacters(&Buffer->String, NewStringLen)) {
            return;
        }
        YoriLibYPrintf(&Buffer->String, _T("%s"), NewString);
        if (MoveToEnd) {
            Buffer->CurrentOffset = Buffer->String.LengthInChars;
        } else {
            Buffer->CurrentOffset = BeginCurrentArg;
        }
        if (Buffer->CurrentOffset > Buffer->String.LengthInChars) {
            Buffer->CurrentOffset = Buffer->String.LengthInChars;
        }
        YoriLibDereference(NewString);
    }

    YoriShFreeCmdContext(&CmdContext);
}

/**
 Delete the current argument from the buffer.

 @param Buffer Pointer to the current input buffer context.
 */
VOID
YoriShDeleteArgument(
    __in PYORI_SH_INPUT_BUFFER Buffer
    )
{
    YORI_SH_CMD_CONTEXT CmdContext;
    YORI_SH_CMD_CONTEXT NewCmdContext;
    DWORD SrcArg;
    DWORD DestArg;
    LPTSTR NewString = NULL;
    DWORD NewStringLen;
    DWORD BeginCurrentArg;
    DWORD EndCurrentArg;

    if (!YoriShParseCmdlineToCmdContext(&Buffer->String, Buffer->CurrentOffset, &CmdContext)) {
        return;
    }

    if (CmdContext.ArgC == 0 || CmdContext.CurrentArg >= CmdContext.ArgC) {
        YoriShFreeCmdContext(&CmdContext);
        return;
    }

    memcpy(&NewCmdContext, &CmdContext, sizeof(CmdContext));
    NewCmdContext.MemoryToFree = YoriLibReferencedMalloc((CmdContext.ArgC - 1) * (sizeof(YORI_STRING) + sizeof(YORI_SH_ARG_CONTEXT)));
    if (NewCmdContext.MemoryToFree == NULL) {
        YoriShFreeCmdContext(&CmdContext);
        return;
    }

    NewCmdContext.ArgC = CmdContext.ArgC - 1;
    NewCmdContext.ArgV = NewCmdContext.MemoryToFree;
    NewCmdContext.ArgContexts = (PYORI_SH_ARG_CONTEXT)YoriLibAddToPointer(NewCmdContext.ArgV, NewCmdContext.ArgC * sizeof(YORI_STRING));


    DestArg = 0;
    for (SrcArg = 0; SrcArg < CmdContext.ArgC; SrcArg++) {
        if (SrcArg != CmdContext.CurrentArg) {
            YoriShCopyArg(&CmdContext, SrcArg, &NewCmdContext, DestArg);
            DestArg++;
        }
    }

    if (CmdContext.CurrentArg > 0) {
        NewCmdContext.CurrentArg--;
    }

    NewString = YoriShBuildCmdlineFromCmdContext(&NewCmdContext, FALSE, &BeginCurrentArg, &EndCurrentArg);
    if (NewString != NULL) {
        NewStringLen = _tcslen(NewString);
        if (!YoriShEnsureStringHasEnoughCharacters(&Buffer->String, NewStringLen)) {
            return;
        }
        YoriLibYPrintf(&Buffer->String, _T("%s"), NewString);
        Buffer->CurrentOffset = EndCurrentArg + 1;
        if (Buffer->CurrentOffset > Buffer->String.LengthInChars) {
            Buffer->CurrentOffset = Buffer->String.LengthInChars;
        }
        YoriLibDereference(NewString);

        Buffer->SuggestionDirty = TRUE;
        YoriLibFreeStringContents(&Buffer->SuggestionString);
        YoriShClearTabCompletionMatches(Buffer);

        Buffer->DirtyBeginOffset = 0;
        Buffer->DirtyLength = Buffer->String.LengthInChars;
    }

    YoriShFreeCmdContext(&CmdContext);
    YoriShFreeCmdContext(&NewCmdContext);
}

/**
 Check if an alias mapping exists for a given hotkey, and if so populate the
 input buffer with the result of that alias and return TRUE indicating that
 it should be executed.

 @param Buffer The input buffer which may be populated with an expression to
        execute.

 @param KeyCode The virtual key code corresponding to the hotkey.

 @param CtrlMask The key modifiers that were held down when the key was
        pressed.

 @return TRUE to indicate the input buffer has been populated with a command
         to execute, FALSE if it was not.
 */
BOOL
YoriShHotkey(
    __in PYORI_SH_INPUT_BUFFER Buffer,
    __in WORD KeyCode,
    __in DWORD CtrlMask
    )
{
    BOOL CtrlPressed = FALSE;
    DWORD FunctionIndex;
    TCHAR NewStringBuffer[32];
    YORI_STRING NewString;
    YORI_SH_CMD_CONTEXT CmdContext;
    LPTSTR CmdLine;

    if (CtrlMask & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
        return FALSE;
    }

    if (CtrlMask & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
        CtrlPressed = TRUE;
    }

    FunctionIndex = KeyCode - VK_F1 + 1;

    YoriLibInitEmptyString(&NewString);
    NewString.StartOfString = NewStringBuffer;
    NewString.LengthAllocated = sizeof(NewStringBuffer)/sizeof(NewStringBuffer[0]);

    NewString.LengthInChars = YoriLibSPrintf(NewString.StartOfString, _T("%sF%i"), CtrlPressed?_T("Ctrl"):_T(""), FunctionIndex);

    if (!YoriShParseCmdlineToCmdContext(&NewString, 0, &CmdContext)) {
        return FALSE;
    }

    if (CmdContext.ArgC == 0) {
        YoriShFreeCmdContext(&CmdContext);
        return FALSE;
    }

    if (!YoriShExpandAlias(&CmdContext)) {
        YoriShFreeCmdContext(&CmdContext);
        return FALSE;
    }

    CmdLine = YoriShBuildCmdlineFromCmdContext(&CmdContext, FALSE, NULL, NULL);
    YoriShFreeCmdContext(&CmdContext);
    if (CmdLine == NULL) {
        return FALSE;
    }

    YoriShClearInput(Buffer);
    YoriShAddCStringToInput(Buffer, CmdLine);
    YoriLibDereference(CmdLine);
    return TRUE;
}

/**
 Check the environment to see if the user wants to customize suggestion
 settings.

 @param Buffer Pointer to the input buffer structure to update in response
        to values found in the environment.
 */
VOID
YoriShConfigureInputSettings(
    __in PYORI_SH_INPUT_BUFFER Buffer
    )
{
    DWORD EnvVarLength;
    YORI_STRING EnvVar;
    LONGLONG llTemp;
    DWORD CharsConsumed;
    TCHAR EnvVarBuffer[10];

    //
    //  Default to suggesting in 400ms after seeing 2 chars in an arg.
    //

    Buffer->DelayBeforeSuggesting = 400;
    Buffer->MinimumCharsInArgBeforeSuggesting = 2;
    Buffer->YoriQuickEdit = FALSE;

    //
    //  Check the environment to see if the user wants to override the
    //  suggestion delay.  Note a value of zero disables the feature.
    //

    YoriLibInitEmptyString(&EnvVar);
    EnvVar.StartOfString = EnvVarBuffer;
    EnvVar.LengthAllocated = sizeof(EnvVarBuffer)/sizeof(EnvVarBuffer[0]);

    EnvVarLength = YoriShGetEnvironmentVariableWithoutSubstitution(_T("YORISUGGESTIONDELAY"), NULL, 0);
    if (EnvVarLength > 0) {
        if (EnvVarLength > EnvVar.LengthAllocated) {
            YoriLibFreeStringContents(&EnvVar);
            YoriLibAllocateString(&EnvVar, EnvVarLength);
        }
        if (EnvVarLength <= EnvVar.LengthAllocated) {
            EnvVar.LengthInChars = YoriShGetEnvironmentVariableWithoutSubstitution(_T("YORISUGGESTIONDELAY"), EnvVar.StartOfString, EnvVar.LengthAllocated);
            if (YoriLibStringToNumber(&EnvVar, TRUE, &llTemp, &CharsConsumed) && CharsConsumed > 0) {
                Buffer->DelayBeforeSuggesting = (ULONG)llTemp;
            }
        }
    }

    //
    //  Check the environment to see if the user wants to override the
    //  minimum number of characters needed in an arg before suggesting.
    //

    EnvVarLength = YoriShGetEnvironmentVariableWithoutSubstitution(_T("YORISUGGESTIONMINCHARS"), NULL, 0);
    if (EnvVarLength > 0) {
        if (EnvVarLength > EnvVar.LengthAllocated) {
            YoriLibFreeStringContents(&EnvVar);
            YoriLibAllocateString(&EnvVar, EnvVarLength);
        }
        if (EnvVarLength <= EnvVar.LengthAllocated) {
            EnvVar.LengthInChars = YoriShGetEnvironmentVariableWithoutSubstitution(_T("YORISUGGESTIONMINCHARS"), EnvVar.StartOfString, EnvVar.LengthAllocated);
            if (YoriLibStringToNumber(&EnvVar, TRUE, &llTemp, &CharsConsumed) && CharsConsumed > 0) {
                Buffer->MinimumCharsInArgBeforeSuggesting = (ULONG)llTemp;
            }
        }
    }

    //
    //  Check the environment to see if the user wants to use Yori's mouse
    //  input support at the prompt and console QuickEdit when running
    //  applications.
    //

    EnvVarLength = YoriShGetEnvironmentVariableWithoutSubstitution(_T("YORIQUICKEDIT"), NULL, 0);
    if (EnvVarLength > 0) {
        if (EnvVarLength > EnvVar.LengthAllocated) {
            YoriLibFreeStringContents(&EnvVar);
            YoriLibAllocateString(&EnvVar, EnvVarLength);
        }
        if (EnvVarLength <= EnvVar.LengthAllocated) {
            EnvVar.LengthInChars = YoriShGetEnvironmentVariableWithoutSubstitution(_T("YORIQUICKEDIT"), EnvVar.StartOfString, EnvVar.LengthAllocated);
            if (YoriLibStringToNumber(&EnvVar, TRUE, &llTemp, &CharsConsumed) && CharsConsumed > 0) {
                if (llTemp == 1) {
                    Buffer->YoriQuickEdit = TRUE;
                }
            }
        }
    }

    YoriLibFreeStringContents(&EnvVar);
}

/**
 Clear the screen, redisplay the prompt and resume editing the current
 input buffer.

 @param Buffer Pointer to the input buffer.
 */
VOID
YoriShClearScreen(
    __in PYORI_SH_INPUT_BUFFER Buffer
    )
{
    HANDLE ConsoleHandle;
    CONSOLE_SCREEN_BUFFER_INFO ScreenInfo;
    DWORD CharsWritten;

    ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    if (!GetConsoleScreenBufferInfo(ConsoleHandle, &ScreenInfo)) {
        return;
    }

    ScreenInfo.dwCursorPosition.X = 0;
    ScreenInfo.dwCursorPosition.Y = 0;

    FillConsoleOutputCharacter(ConsoleHandle, ' ', (DWORD)ScreenInfo.dwSize.X * ScreenInfo.dwSize.Y, ScreenInfo.dwCursorPosition, &CharsWritten);

    FillConsoleOutputAttribute(ConsoleHandle, ScreenInfo.wAttributes, (DWORD)ScreenInfo.dwSize.X * ScreenInfo.dwSize.Y, ScreenInfo.dwCursorPosition, &CharsWritten);

    SetConsoleCursorPosition(ConsoleHandle, ScreenInfo.dwCursorPosition);

    YoriShPreCommand();
    YoriShDisplayPrompt();
    YoriShPreCommand();

    Buffer->PreviousCurrentOffset = 0;
    Buffer->DirtyBeginOffset = 0;
    Buffer->DirtyLength = Buffer->String.LengthInChars;
}

/**
 Create a new selection, and if one already exists, advance the selection
 to the left by one character.

 @param Buffer Pointer to the input buffer whose selection should be updated.
 */
VOID
YoriShSelectLeftChar(
    __inout PYORI_SH_INPUT_BUFFER Buffer
    )
{
    COORD StartCoord;
    COORD NewCoord;
    if (!YoriLibIsSelectionActive(&Buffer->Selection)) {
        StartCoord = YoriShDetermineCellLocationIfMoved(0);
    } else {
        StartCoord.X = Buffer->Selection.CurrentlyDisplayed.Left;
        StartCoord.Y = Buffer->Selection.CurrentlyDisplayed.Top;
    }
    NewCoord = YoriShDetermineCellLocationIfMoved(-1);
    if (NewCoord.Y == StartCoord.Y && Buffer->CurrentOffset > 0) {
        Buffer->CurrentOffset--;
        if (!YoriLibIsSelectionActive(&Buffer->Selection)) {
            YoriLibCreateSelectionFromPoint(&Buffer->Selection, NewCoord.X, NewCoord.Y);
        }
        YoriLibUpdateSelectionToPoint(&Buffer->Selection, NewCoord.X, NewCoord.Y);
    }
}

/**
 Create a new selection, and if one already exists, advance the selection
 to the right by one character.

 @param Buffer Pointer to the input buffer whose selection should be updated.
 */
VOID
YoriShSelectRightChar(
    __inout PYORI_SH_INPUT_BUFFER Buffer
    )
{
    COORD StartCoord;
    COORD NewCoord;
    if (!YoriLibIsSelectionActive(&Buffer->Selection)) {
        StartCoord = YoriShDetermineCellLocationIfMoved(0);
    } else {
        StartCoord.X = Buffer->Selection.CurrentlyDisplayed.Left;
        StartCoord.Y = Buffer->Selection.CurrentlyDisplayed.Top;
    }
    NewCoord = YoriShDetermineCellLocationIfMoved(1);
    if (NewCoord.Y == StartCoord.Y && Buffer->CurrentOffset + 1 < Buffer->String.LengthInChars) {
        Buffer->CurrentOffset++;
        if (!YoriLibIsSelectionActive(&Buffer->Selection)) {
            YoriLibCreateSelectionFromPoint(&Buffer->Selection, StartCoord.X, StartCoord.Y);
        }
        YoriLibUpdateSelectionToPoint(&Buffer->Selection, NewCoord.X, NewCoord.Y);
    }
}

/**
 Perform processing related to when a key is pressed.

 @param Buffer Pointer to the input buffer to update.

 @param InputRecord Pointer to the console input event.

 @param TerminateInput On successful completion, set to TRUE to indicate that
        the input sequence is complete and should be returned to the caller.

 @return TRUE to indicate the input buffer has changed and needs to be
         redisplayed.
 */
BOOL
YoriShProcessKeyDown(
    __inout PYORI_SH_INPUT_BUFFER Buffer,
    __in PINPUT_RECORD InputRecord,
    __out PBOOL TerminateInput
    )
{
    DWORD CtrlMask;
    TCHAR Char;
    DWORD Count;
    WORD KeyCode;
    WORD ScanCode;
    BOOL ClearSelection;

    *TerminateInput = FALSE;
    YoriShPrepareForNextKey(Buffer);

    Char = InputRecord->Event.KeyEvent.uChar.UnicodeChar;
    CtrlMask = InputRecord->Event.KeyEvent.dwControlKeyState & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED | RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED | ENHANCED_KEY | SHIFT_PRESSED);
    KeyCode = InputRecord->Event.KeyEvent.wVirtualKeyCode;
    ScanCode = InputRecord->Event.KeyEvent.wVirtualScanCode;

    if (KeyCode >= VK_F1 && KeyCode <= VK_F12) {
        if (YoriShHotkey(Buffer, KeyCode, CtrlMask)) {
            *TerminateInput = TRUE;
            YoriLibClearSelection(&Buffer->Selection);
            return TRUE;
        }
    }

    ClearSelection = TRUE;

    if (CtrlMask == 0 || CtrlMask == SHIFT_PRESSED) {

        if (Char == '\r') {
            if (Buffer->SearchMode) {
                Buffer->SearchMode = FALSE;
                YoriLibFreeStringContents(&Buffer->SearchString);
            } else {
                if (!YoriLibCopySelectionIfPresent(&Buffer->Selection)) {
                    *TerminateInput = TRUE;
                }
                YoriLibClearSelection(&Buffer->Selection);
                return TRUE;
            }
        } else if (Char == 27) {
            if (Buffer->SearchMode) {
                Buffer->SearchMode = FALSE;
                Buffer->CurrentOffset = Buffer->PreSearchOffset;
                YoriLibFreeStringContents(&Buffer->SearchString);
            } else {
                YoriShClearInput(Buffer);
            }
        } else if (Char == '\t') {
            if ((CtrlMask & SHIFT_PRESSED) == 0) {
                YoriShTabCompletion(Buffer, 0);
            } else {
                YoriShTabCompletion(Buffer, YORI_SH_TAB_COMPLETE_BACKWARDS);
            }
        } else if (Char == '\b') {
            if (!YoriShOverwriteSelectionIfInInput(Buffer)) {
                YoriShBackspace(Buffer, InputRecord->Event.KeyEvent.wRepeatCount);
            }
        } else if (Char == '\0') {
        } else {
            for (Count = 0; Count < InputRecord->Event.KeyEvent.wRepeatCount; Count++) {
                TCHAR StringChar[2];
                StringChar[0] = Char;
                StringChar[1] = '\0';
                YoriShAddCStringToInput(Buffer, StringChar);
            }
        }
    } else if (CtrlMask == RIGHT_CTRL_PRESSED ||
               CtrlMask == LEFT_CTRL_PRESSED ||
               CtrlMask == (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)) {
        if (KeyCode == 'A') {
            Buffer->CurrentOffset = 0;
        } else if (KeyCode == 'C') {
            YoriShClearInput(Buffer);
            YoriLibClearSelection(&Buffer->Selection);
            *TerminateInput = TRUE;
            return TRUE;
        } else if (KeyCode == 'E') {
            Buffer->CurrentOffset = Buffer->String.LengthInChars;
        } else if (KeyCode == 'L') {
            YoriShClearScreen(Buffer);
        } else if (KeyCode == 'V') {
            YORI_STRING ClipboardData;
            YoriLibInitEmptyString(&ClipboardData);
            if (YoriLibPasteText(&ClipboardData)) {
                YoriShAddYoriStringToInput(Buffer, &ClipboardData);
                YoriLibFreeStringContents(&ClipboardData);
            }
        } else if (KeyCode == 0xBF) { // Aka VK_OEM_2, / or ? on US keyboards
            Buffer->SearchMode = TRUE;
            Buffer->PreSearchOffset = Buffer->CurrentOffset;
        } else if (KeyCode == VK_TAB) {
            YoriShTabCompletion(Buffer, YORI_SH_TAB_COMPLETE_FULL_PATH);
        } else if (KeyCode == '\b') {
            if (!YoriShOverwriteSelectionIfInInput(Buffer)) {
                YoriShDeleteArgument(Buffer);
            }
        }
    } else if (CtrlMask == (RIGHT_CTRL_PRESSED | SHIFT_PRESSED) ||
               CtrlMask == (LEFT_CTRL_PRESSED | SHIFT_PRESSED) ||
               CtrlMask == (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED | SHIFT_PRESSED)) {
        if (KeyCode == VK_TAB) {
            YoriShTabCompletion(Buffer, YORI_SH_TAB_COMPLETE_FULL_PATH | YORI_SH_TAB_COMPLETE_BACKWARDS);
        }
    } else if (CtrlMask == ENHANCED_KEY) {
        PYORI_LIST_ENTRY NewEntry = NULL;
        PYORI_SH_HISTORY_ENTRY HistoryEntry;
        if (KeyCode == VK_UP) {
            NewEntry = YoriLibGetPreviousListEntry(&YoriShCommandHistory, Buffer->HistoryEntryToUse);
            if (NewEntry != NULL) {
                Buffer->HistoryEntryToUse = NewEntry;
                HistoryEntry = CONTAINING_RECORD(NewEntry, YORI_SH_HISTORY_ENTRY, ListEntry);
                YoriShClearInput(Buffer);
                YoriShAddYoriStringToInput(Buffer, &HistoryEntry->CmdLine);
            }
        } else if (KeyCode == VK_DOWN) {
            if (Buffer->HistoryEntryToUse != NULL) {
                NewEntry = YoriLibGetNextListEntry(&YoriShCommandHistory, Buffer->HistoryEntryToUse);
            }
            if (NewEntry != NULL) {
                Buffer->HistoryEntryToUse = NewEntry;
                HistoryEntry = CONTAINING_RECORD(NewEntry, YORI_SH_HISTORY_ENTRY, ListEntry);
                YoriShClearInput(Buffer);
                YoriShAddYoriStringToInput(Buffer, &HistoryEntry->CmdLine);
            }
        } else if (KeyCode == VK_LEFT) {
            if (Buffer->CurrentOffset > 0) {
                Buffer->CurrentOffset--;
            }
        } else if (KeyCode == VK_RIGHT) {
            if (Buffer->CurrentOffset < Buffer->String.LengthInChars) {
                Buffer->CurrentOffset++;
            }
        } else if (KeyCode == VK_INSERT) {
            Buffer->CursorInfo.bVisible = TRUE;
            if (Buffer->InsertMode) {
                Buffer->InsertMode = FALSE;
                Buffer->CursorInfo.dwSize = 100;
            } else {
                Buffer->InsertMode = TRUE;
                Buffer->CursorInfo.dwSize = 20;
            }
            SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &Buffer->CursorInfo);
        } else if (KeyCode == VK_HOME) {
            Buffer->CurrentOffset = 0;
        } else if (KeyCode == VK_END) {
            Buffer->CurrentOffset = Buffer->String.LengthInChars;
        } else if (KeyCode == VK_DELETE) {

            if (!YoriShOverwriteSelectionIfInInput(Buffer)) {
                Count = InputRecord->Event.KeyEvent.wRepeatCount;
                if (Count + Buffer->CurrentOffset > Buffer->String.LengthInChars) {
                    Count = Buffer->String.LengthInChars - Buffer->CurrentOffset;
                }

                Buffer->CurrentOffset += Count;

                YoriShBackspace(Buffer, Count);
            }

        } else if (KeyCode == VK_RETURN) {
            if (Buffer->SearchMode) {
                Buffer->SearchMode = FALSE;
                YoriLibFreeStringContents(&Buffer->SearchString);
            } else {
                if (!YoriLibCopySelectionIfPresent(&Buffer->Selection)) {
                    *TerminateInput = TRUE;
                }
                YoriLibClearSelection(&Buffer->Selection);
                return TRUE;
            }
        }
    } else if (CtrlMask == (RIGHT_CTRL_PRESSED | ENHANCED_KEY) ||
               CtrlMask == (LEFT_CTRL_PRESSED | ENHANCED_KEY) ||
               CtrlMask == (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED | ENHANCED_KEY)) {

        if (KeyCode == VK_LEFT) {
            YoriShMoveCursorToPriorArgument(Buffer);
        } else if (KeyCode == VK_RIGHT) {
            YoriShMoveCursorToNextArgument(Buffer);
        } else if (KeyCode == VK_UP) {
            YoriShTabCompletion(Buffer, YORI_SH_TAB_COMPLETE_HISTORY);
        } else if (KeyCode == VK_DOWN) {
            YoriShTabCompletion(Buffer, YORI_SH_TAB_COMPLETE_HISTORY | YORI_SH_TAB_COMPLETE_BACKWARDS);
        } else if (KeyCode == VK_DELETE) {
            if (Buffer->HistoryEntryToUse != NULL) {
                PYORI_LIST_ENTRY NewEntry = NULL;
                PYORI_SH_HISTORY_ENTRY HistoryEntry;

                NewEntry = YoriLibGetPreviousListEntry(&YoriShCommandHistory, Buffer->HistoryEntryToUse);
                HistoryEntry = CONTAINING_RECORD(Buffer->HistoryEntryToUse, YORI_SH_HISTORY_ENTRY, ListEntry);
                YoriShRemoveOneHistoryEntry(HistoryEntry);

                if (NewEntry != NULL) {
                    Buffer->HistoryEntryToUse = NewEntry;
                    HistoryEntry = CONTAINING_RECORD(NewEntry, YORI_SH_HISTORY_ENTRY, ListEntry);
                    YoriShClearInput(Buffer);
                    YoriShAddYoriStringToInput(Buffer, &HistoryEntry->CmdLine);
                } else {
                    YoriShClearInput(Buffer);
                }
            }
        }
    } else if (CtrlMask == LEFT_ALT_PRESSED || CtrlMask == RIGHT_ALT_PRESSED ||
               CtrlMask == (LEFT_ALT_PRESSED | ENHANCED_KEY) ||
               CtrlMask == (RIGHT_ALT_PRESSED | ENHANCED_KEY)) {
        if (KeyCode >= '0' && KeyCode <= '9') {
            if (KeyCode == '0' && Buffer->NumericKeyValue == 0 && !Buffer->NumericKeyAnsiMode) {
                Buffer->NumericKeyAnsiMode = TRUE;
            } else {
                Buffer->NumericKeyValue = Buffer->NumericKeyValue * 10 + KeyCode - '0';
            }
        } else if (KeyCode >= VK_NUMPAD0 && KeyCode <= VK_NUMPAD9) {
            if (KeyCode == VK_NUMPAD0 && Buffer->NumericKeyValue == 0 && !Buffer->NumericKeyAnsiMode) {
                Buffer->NumericKeyAnsiMode = TRUE;
            } else {
                Buffer->NumericKeyValue = Buffer->NumericKeyValue * 10 + KeyCode - VK_NUMPAD0;
            }
        } else if (ScanCode >= 0x47 && ScanCode <= 0x49) {
            Buffer->NumericKeyValue = Buffer->NumericKeyValue * 10 + ScanCode - 0x47 + 7;
        } else if (ScanCode >= 0x4b && ScanCode <= 0x4d) {
            Buffer->NumericKeyValue = Buffer->NumericKeyValue * 10 + ScanCode - 0x4b + 4;
        } else if (ScanCode >= 0x4f && ScanCode <= 0x51) {
            Buffer->NumericKeyValue = Buffer->NumericKeyValue * 10 + ScanCode - 0x4f + 1;
        } else if (ScanCode == 0x52) {
            if (Buffer->NumericKeyValue == 0 && !Buffer->NumericKeyAnsiMode) {
                Buffer->NumericKeyAnsiMode = TRUE;
            } else {
                Buffer->NumericKeyValue = Buffer->NumericKeyValue * 10;
            }
        }
    } else if (CtrlMask == (SHIFT_PRESSED | ENHANCED_KEY)) {
        if (KeyCode == VK_INSERT) {
            YORI_STRING ClipboardData;
            YoriLibInitEmptyString(&ClipboardData);
            if (YoriLibPasteText(&ClipboardData)) {
                YoriShAddYoriStringToInput(Buffer, &ClipboardData);
                YoriLibFreeStringContents(&ClipboardData);
            }
        } else if (KeyCode == VK_LEFT) {
            YoriShSelectLeftChar(Buffer);
            ClearSelection = FALSE;
        } else if (KeyCode == VK_RIGHT) {
            YoriShSelectRightChar(Buffer);
            ClearSelection = FALSE;
        }
    }

    if (KeyCode != VK_SHIFT &&
        KeyCode != VK_CONTROL) {

        if (ClearSelection) {
            YoriLibClearSelection(&Buffer->Selection);
        }

        YoriShPostKeyPress(Buffer);
        return TRUE;
    }

    return FALSE;
}


/**
 Perform processing related to when a key is released.  This is only used for
 Alt+Number numerical key sequences.

 @param Buffer Pointer to the input buffer to update.

 @param InputRecord Pointer to the console input event.

 @param TerminateInput On successful completion, set to TRUE to indicate that
        the input sequence is complete and should be returned to the caller.

 @return TRUE to indicate the input buffer has changed and needs to be
         redisplayed.
 */
BOOL
YoriShProcessKeyUp(
    __inout PYORI_SH_INPUT_BUFFER Buffer,
    __in PINPUT_RECORD InputRecord,
    __out PBOOL TerminateInput
    )
{
    BOOL KeyPressGenerated = FALSE;

    UNREFERENCED_PARAMETER(TerminateInput);

    if ((InputRecord->Event.KeyEvent.dwControlKeyState & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED)) == 0 &&
        Buffer->NumericKeyValue != 0) {

        CHAR SmallKeyValue = (CHAR)Buffer->NumericKeyValue;
        TCHAR HostKeyValue[2];

        HostKeyValue[0] = HostKeyValue[1] = 0;

        MultiByteToWideChar(Buffer->NumericKeyAnsiMode?CP_ACP:CP_OEMCP,
                            0,
                            &SmallKeyValue,
                            1,
                            HostKeyValue,
                            1);

        if (HostKeyValue != 0) {
            YoriShPrepareForNextKey(Buffer);
            YoriShAddCStringToInput(Buffer, HostKeyValue);
            YoriShPostKeyPress(Buffer);
            YoriLibClearSelection(&Buffer->Selection);
            KeyPressGenerated = TRUE;
        }

        Buffer->NumericKeyValue = 0;
        Buffer->NumericKeyAnsiMode = FALSE;
    }

    return KeyPressGenerated;
}

/**
 Perform processing related to when a mouse button is pressed.

 @param Buffer Pointer to the input buffer to update.

 @param InputRecord Pointer to the console input event.

 @param ButtonsPressed A bit mask of buttons that were just pressed.

 @param TerminateInput On successful completion, set to TRUE to indicate that
        the input sequence is complete and should be returned to the caller.

 @return TRUE to indicate the input buffer has changed and needs to be
         redisplayed.
 */
BOOL
YoriShProcessMouseButtonDown(
    __inout PYORI_SH_INPUT_BUFFER Buffer,
    __in PINPUT_RECORD InputRecord,
    __in DWORD ButtonsPressed,
    __out PBOOL TerminateInput
    )
{
    BOOL BufferChanged = FALSE;

    UNREFERENCED_PARAMETER(TerminateInput);

    if (ButtonsPressed & FROM_LEFT_1ST_BUTTON_PRESSED) {
        DWORD StringOffset;

        BufferChanged = YoriLibCreateSelectionFromPoint(&Buffer->Selection,
                                                        InputRecord->Event.MouseEvent.dwMousePosition.X,
                                                        InputRecord->Event.MouseEvent.dwMousePosition.Y);

        if (YoriShStringOffsetFromCoordinates(Buffer, InputRecord->Event.MouseEvent.dwMousePosition, &StringOffset)) {
            Buffer->CurrentOffset = StringOffset;
            BufferChanged = TRUE;
        }
    } else if (ButtonsPressed & RIGHTMOST_BUTTON_PRESSED) {
        if (YoriLibIsSelectionActive(&Buffer->Selection)) {
            BufferChanged = YoriLibCopySelectionIfPresent(&Buffer->Selection);
            if (BufferChanged) {
                YoriLibClearSelection(&Buffer->Selection);
            }
        } else {
            YORI_STRING ClipboardData;
            YoriLibInitEmptyString(&ClipboardData);
            if (YoriLibPasteText(&ClipboardData)) {
                YoriShAddYoriStringToInput(Buffer, &ClipboardData);
                YoriLibFreeStringContents(&ClipboardData);
                BufferChanged = TRUE;
            }
        }
    }

    return BufferChanged;
}

/**
 Perform processing related to when a mouse button is released.

 @param Buffer Pointer to the input buffer to update.

 @param InputRecord Pointer to the console input event.

 @param ButtonsReleased A bit mask of buttons that were just released.

 @param TerminateInput On successful completion, set to TRUE to indicate that
        the input sequence is complete and should be returned to the caller.

 @return TRUE to indicate the input buffer has changed and needs to be
         redisplayed.
 */
BOOL
YoriShProcessMouseButtonUp(
    __inout PYORI_SH_INPUT_BUFFER Buffer,
    __in PINPUT_RECORD InputRecord,
    __in DWORD ButtonsReleased,
    __out PBOOL TerminateInput
    )
{
    UNREFERENCED_PARAMETER(InputRecord);
    UNREFERENCED_PARAMETER(TerminateInput);

    //
    //  If the left mouse button was released and periodic scrolling was in
    //  effect, stop it now.
    //

    if (ButtonsReleased & FROM_LEFT_1ST_BUTTON_PRESSED) {
        YoriLibClearPeriodicScroll(&Buffer->Selection);
    }

    return FALSE;
}


/**
 Perform processing related to when a mouse is double clicked.

 @param Buffer Pointer to the input buffer to update.

 @param InputRecord Pointer to the console input event.

 @param ButtonsPressed A bit mask of buttons that were just pressed.

 @param TerminateInput On successful completion, set to TRUE to indicate that
        the input sequence is complete and should be returned to the caller.

 @return TRUE to indicate the input buffer has changed and needs to be
         redisplayed.
 */
BOOL
YoriShProcessMouseDoubleClick(
    __inout PYORI_SH_INPUT_BUFFER Buffer,
    __in PINPUT_RECORD InputRecord,
    __in DWORD ButtonsPressed,
    __out PBOOL TerminateInput
    )
{
    BOOL BufferChanged = FALSE;
    HANDLE ConsoleHandle;
    COORD ReadPoint;
    TCHAR ReadChar;
    DWORD CharsRead;

    ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    UNREFERENCED_PARAMETER(TerminateInput);

    if (ButtonsPressed & FROM_LEFT_1ST_BUTTON_PRESSED) {
        SHORT StartOffset;
        SHORT EndOffset;
        CONSOLE_SCREEN_BUFFER_INFO ScreenInfo;
        YORI_STRING BreakChars;

        if (!GetConsoleScreenBufferInfo(ConsoleHandle, &ScreenInfo)) {
            return FALSE;
        }
        YoriLibGetSelectionDoubleClickBreakChars(&BreakChars);

        BufferChanged = YoriLibClearSelection(&Buffer->Selection);

        ReadChar = ' ';
        ReadPoint.Y = InputRecord->Event.MouseEvent.dwMousePosition.Y;
        ReadPoint.X = InputRecord->Event.MouseEvent.dwMousePosition.X;

        //
        //  If the user double clicked on a break char, do nothing.
        //

        ReadConsoleOutputCharacter(ConsoleHandle, &ReadChar, 1, ReadPoint, &CharsRead);
        if (YoriLibFindLeftMostCharacter(&BreakChars, ReadChar) != NULL) {
            YoriLibFreeStringContents(&BreakChars);
            return FALSE;
        }

        //
        //  Nagivate left to find beginning of line or next break char.
        //

        for (StartOffset = InputRecord->Event.MouseEvent.dwMousePosition.X; StartOffset > 0; StartOffset--) {
            ReadPoint.X = (SHORT)(StartOffset - 1);
            ReadConsoleOutputCharacter(ConsoleHandle, &ReadChar, 1, ReadPoint, &CharsRead);
            if (YoriLibFindLeftMostCharacter(&BreakChars, ReadChar) != NULL) {
                break;
            }
        }

        //
        //  Navigate right to find end of line or next break char.
        //

        for (EndOffset = InputRecord->Event.MouseEvent.dwMousePosition.X; EndOffset < ScreenInfo.dwSize.X - 1; EndOffset++) {
            ReadPoint.X = (SHORT)(EndOffset + 1);
            ReadConsoleOutputCharacter(ConsoleHandle, &ReadChar, 1, ReadPoint, &CharsRead);
            if (YoriLibFindLeftMostCharacter(&BreakChars, ReadChar) != NULL) {
                break;
            }
        }

        YoriLibCreateSelectionFromRange(&Buffer->Selection, StartOffset, ReadPoint.Y, EndOffset, ReadPoint.Y);

        BufferChanged = TRUE;
        YoriLibFreeStringContents(&BreakChars);
    }

    return BufferChanged;
}

/**
 Perform processing related to a mouse move event.

 @param Buffer Pointer to the input buffer to update.

 @param InputRecord Pointer to the console input event.

 @param TerminateInput On successful completion, set to TRUE to indicate that
        the input sequence is complete and should be returned to the caller.

 @return TRUE to indicate the input buffer has changed and needs to be
         redisplayed.
 */
BOOL
YoriShProcessMouseMove(
    __inout PYORI_SH_INPUT_BUFFER Buffer,
    __in PINPUT_RECORD InputRecord,
    __out PBOOL TerminateInput
    )
{
    UNREFERENCED_PARAMETER(TerminateInput);

    if (InputRecord->Event.MouseEvent.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {

        if (YoriLibIsSelectionActive(&Buffer->Selection)) {

            YoriLibUpdateSelectionToPoint(&Buffer->Selection,
                                          InputRecord->Event.MouseEvent.dwMousePosition.X,
                                          InputRecord->Event.MouseEvent.dwMousePosition.Y);
            //
            //  Do one scroll immediately.  This allows the user to force scrolling
            //  by moving the mouse outside the window.
            //

            YoriLibPeriodicScrollForSelection(&Buffer->Selection);

            return TRUE;
        }
    }

    return FALSE;
}

/**
 Perform processing related to when a mouse wheel is scrolled.

 @param Buffer Pointer to the input buffer to update.

 @param InputRecord Pointer to the console input event.

 @param ButtonsPressed A bit mask of buttons that were just pressed.

 @param TerminateInput On successful completion, set to TRUE to indicate that
        the input sequence is complete and should be returned to the caller.

 @return TRUE to indicate the input buffer has changed and needs to be
         redisplayed.
 */
BOOL
YoriShProcessMouseScroll(
    __inout PYORI_SH_INPUT_BUFFER Buffer,
    __in PINPUT_RECORD InputRecord,
    __in DWORD ButtonsPressed,
    __out PBOOL TerminateInput
    )
{
    HANDLE ConsoleHandle;
    SHORT Direction;
    SHORT LinesToScroll;
    CONSOLE_SCREEN_BUFFER_INFO ScreenInfo;

    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(ButtonsPressed);
    UNREFERENCED_PARAMETER(TerminateInput);

    ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    Direction = HIWORD(InputRecord->Event.MouseEvent.dwButtonState);
    if (!GetConsoleScreenBufferInfo(ConsoleHandle, &ScreenInfo)) {
        return FALSE;
    }

    if (Direction > 0) {
        LinesToScroll = (SHORT)(Direction / 0x20);
        if (ScreenInfo.srWindow.Top > 0) {
            if (ScreenInfo.srWindow.Top > LinesToScroll) {
                ScreenInfo.srWindow.Top = (SHORT)(ScreenInfo.srWindow.Top - LinesToScroll);
                ScreenInfo.srWindow.Bottom = (SHORT)(ScreenInfo.srWindow.Bottom - LinesToScroll);
            } else {
                ScreenInfo.srWindow.Bottom = (SHORT)(ScreenInfo.srWindow.Bottom - ScreenInfo.srWindow.Top);
                ScreenInfo.srWindow.Top = 0;
            }
        }
    } else if (Direction < 0) {
        LinesToScroll = (SHORT)(0 - (Direction / 0x20));
        if (ScreenInfo.srWindow.Bottom < ScreenInfo.dwSize.Y - 1) {
            if (ScreenInfo.srWindow.Bottom < ScreenInfo.dwSize.Y - LinesToScroll - 1) {
                ScreenInfo.srWindow.Top = (SHORT)(ScreenInfo.srWindow.Top + LinesToScroll);
                ScreenInfo.srWindow.Bottom = (SHORT)(ScreenInfo.srWindow.Bottom + LinesToScroll);
            } else {
                ScreenInfo.srWindow.Top = (SHORT)(ScreenInfo.srWindow.Top + (ScreenInfo.dwSize.Y - ScreenInfo.srWindow.Bottom - 1));
                ScreenInfo.srWindow.Bottom = (SHORT)(ScreenInfo.dwSize.Y - 1);
            }
        }
    }

    SetConsoleWindowInfo(ConsoleHandle, TRUE, &ScreenInfo.srWindow);

    return FALSE;
}



/**
 Get a new expression from the user through the console.

 @param Expression On successful completion, updated to point to the
        entered expression.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriShGetExpressionFromConsole(
    __inout PYORI_STRING Expression
    )
{
    YORI_SH_INPUT_BUFFER Buffer;
    DWORD ActuallyRead = 0;
    DWORD CurrentRecordIndex = 0;
    DWORD err;
    INPUT_RECORD InputRecords[20];
    PINPUT_RECORD InputRecord;
    BOOL ReDisplayRequired;
    BOOL TerminateInput;
    BOOL RestartStateSaved = FALSE;
    BOOL SuggestionPopulated = FALSE;
    HANDLE InputHandle;
    HANDLE OutputHandle;

    ZeroMemory(&Buffer, sizeof(Buffer));
    Buffer.InsertMode = TRUE;
    Buffer.CursorInfo.bVisible = TRUE;
    Buffer.CursorInfo.dwSize = 20;

    InputHandle = GetStdHandle(STD_INPUT_HANDLE);
    OutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    SetConsoleCursorInfo(OutputHandle, &Buffer.CursorInfo);
    SetConsoleMode(InputHandle, ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);
    SetConsoleMode(OutputHandle, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    if (!YoriLibAllocateString(&Buffer.String, 256)) {
        return FALSE;
    }

    YoriShConfigureInputSettings(&Buffer);
    YoriShConfigureMouseForPrompt(&Buffer);
    SetConsoleCtrlHandler(YoriShAppCloseCtrlHandler, TRUE);

    while (TRUE) {

        if (!PeekConsoleInput(InputHandle, InputRecords, sizeof(InputRecords)/sizeof(InputRecords[0]), &ActuallyRead)) {
            break;
        }

        ReDisplayRequired = FALSE;

        for (CurrentRecordIndex = 0; CurrentRecordIndex < ActuallyRead; CurrentRecordIndex++) {

            InputRecord = &InputRecords[CurrentRecordIndex];
            TerminateInput = FALSE;

            if (InputRecord->EventType == KEY_EVENT) {

                if (InputRecord->Event.KeyEvent.bKeyDown) {
                    ReDisplayRequired |= YoriShProcessKeyDown(&Buffer, InputRecord, &TerminateInput);
                } else {
                    ReDisplayRequired |= YoriShProcessKeyUp(&Buffer, InputRecord, &TerminateInput);
                }

            } else if (InputRecord->EventType == MOUSE_EVENT) {

                DWORD ButtonsPressed = InputRecord->Event.MouseEvent.dwButtonState - (Buffer.PreviousMouseButtonState & InputRecord->Event.MouseEvent.dwButtonState);
                DWORD ButtonsReleased = Buffer.PreviousMouseButtonState - (Buffer.PreviousMouseButtonState & InputRecord->Event.MouseEvent.dwButtonState);

                if (ButtonsReleased > 0) {
                    ReDisplayRequired |= YoriShProcessMouseButtonUp(&Buffer, InputRecord, ButtonsReleased, &TerminateInput);
                }

                if (ButtonsPressed > 0) {
                    ReDisplayRequired |= YoriShProcessMouseButtonDown(&Buffer, InputRecord, ButtonsPressed, &TerminateInput);
                }

                Buffer.PreviousMouseButtonState = InputRecord->Event.MouseEvent.dwButtonState;
                if (InputRecord->Event.MouseEvent.dwEventFlags & MOUSE_MOVED) {
                    ReDisplayRequired |= YoriShProcessMouseMove(&Buffer, InputRecord, &TerminateInput);
                }

                if (InputRecord->Event.MouseEvent.dwEventFlags & DOUBLE_CLICK) {
                    ReDisplayRequired |= YoriShProcessMouseDoubleClick(&Buffer, InputRecord, ButtonsPressed, &TerminateInput);
                }

                if (InputRecord->Event.MouseEvent.dwEventFlags & MOUSE_WHEELED) {
                    ReDisplayRequired |= YoriShProcessMouseScroll(&Buffer, InputRecord, ButtonsPressed, &TerminateInput);
                }

            } else if (InputRecord->EventType == WINDOW_BUFFER_SIZE_EVENT) {

                ReDisplayRequired |= YoriLibClearSelection(&Buffer.Selection);
            } else if (InputRecord->EventType == FOCUS_EVENT) {
                if (InputRecord->Event.FocusEvent.bSetFocus) {
                    YoriShSetWindowState(YORI_SH_TASK_COMPLETE);
                }
            }

            if (TerminateInput) {
                YoriShTerminateInput(&Buffer);
                ReadConsoleInput(InputHandle, InputRecords, CurrentRecordIndex + 1, &ActuallyRead);
                if (Buffer.String.LengthInChars > 0) {
                    YoriShAddToHistory(&Buffer.String);
                }
                memcpy(Expression, &Buffer.String, sizeof(YORI_STRING));
                return TRUE;
            }
        }

        if (ReDisplayRequired) {
            YoriShDisplayAfterKeyPress(&Buffer);
        }

        //
        //  If we processed any events, remove them from the queue.
        //

        if (ActuallyRead > 0) {
            if (!ReadConsoleInput(InputHandle, InputRecords, ActuallyRead, &ActuallyRead)) {
                break;
            }
        }

        //
        //  Wait to see if any further events arrive.  If we haven't saved
        //  state and the user hasn't done anything for 30 seconds, save
        //  state.  Initialize the variable for compilers that are smart
        //  enough to expect variables to be initialized but too stupid to
        //  understand that they actually are.
        //

        SuggestionPopulated = FALSE;
        if (Buffer.SuggestionString.LengthInChars > 0 ||
            Buffer.DelayBeforeSuggesting == 0 ||
            Buffer.TabContext.TabCount != 0) {

            SuggestionPopulated = TRUE;
        }
        err = WAIT_OBJECT_0;
        while (TRUE) {
            if (YoriLibIsPeriodicScrollActive(&Buffer.Selection)) {

                err = WaitForSingleObject(InputHandle, 250);
                if (err == WAIT_OBJECT_0) {
                    break;
                }
                if (err == WAIT_TIMEOUT) {
                    YoriLibPeriodicScrollForSelection(&Buffer.Selection);
                }
            } else if (!SuggestionPopulated) {
                err = WaitForSingleObject(InputHandle, Buffer.DelayBeforeSuggesting);
                if (err == WAIT_OBJECT_0) {
                    break;
                }
                if (err == WAIT_TIMEOUT) {
                    ASSERT(!SuggestionPopulated);
                    YoriShCompleteSuggestion(&Buffer);
                    SuggestionPopulated = TRUE;
                    Buffer.SuggestionDirty = TRUE;
                    if (Buffer.SuggestionString.LengthInChars > 0) {
                        YoriShDisplayAfterKeyPress(&Buffer);
                    }
                }
            } else if (!RestartStateSaved) {
                err = WaitForSingleObject(InputHandle, 30 * 1000);
                if (err == WAIT_OBJECT_0) {
                    break;
                }
                if (err == WAIT_TIMEOUT) {
                    ASSERT(!RestartStateSaved);
                    YoriShSaveRestartState();
                    RestartStateSaved = TRUE;
                }
            } else {
                err = WaitForSingleObject(InputHandle, INFINITE);
                if (err == WAIT_OBJECT_0) {
                    break;
                }
            }

            if (err != WAIT_TIMEOUT) {
                break;
            }
        }

        if (err != WAIT_OBJECT_0) {
            break;
        }
    }

    err = GetLastError();

    YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("Error reading from console %i handle %08x\n"), err, InputHandle);

    YoriShTerminateInput(&Buffer);
    YoriLibFreeStringContents(&Buffer.String);
    return FALSE;
}

/**
 Context that is preserved across reads from any input file or pipe to
 determine the state of line parsing.
 */
PVOID YoriShGetExpressionLineContext = NULL;

/**
 Get a new expression from the user from whatever input device we have
 configured.

 @param Expression On successful completion, updated to point to the
        entered expression.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOL
YoriShGetExpression(
    __inout PYORI_STRING Expression
    )
{
    HANDLE InputHandle;
    HANDLE OutputHandle;
    DWORD ConsoleMode;

    YoriLibInitEmptyString(Expression);

    InputHandle = GetStdHandle(STD_INPUT_HANDLE);
    OutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(InputHandle, &ConsoleMode) && GetConsoleMode(OutputHandle, &ConsoleMode)) {
        return YoriShGetExpressionFromConsole(Expression);
    }

    SetConsoleMode(InputHandle, ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_ECHO_INPUT);
    SetConsoleMode(OutputHandle, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    if (!YoriLibReadLineToString(Expression, &YoriShGetExpressionLineContext, InputHandle)) {
        return FALSE;
    }

    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%y\n"), Expression);

    return TRUE;
}

/**
 On process termination, cleanup any currently active line parsing context,
 */
VOID
YoriShCleanupInputContext()
{
    if (YoriShGetExpressionLineContext != NULL) {
        YoriLibLineReadClose(YoriShGetExpressionLineContext);
    }
}


// vim:sw=4:ts=4:et:
