#include <windows.h>
#include <stdio.h>

#define METASTRING(x) x,sizeof(x)-1
#define C2B(x) x-'0'
#define BIT_TEST(var,pos) (((var)>>(pos))&1)

#define COLUMNS   15
#define ROWS      6
#define INDICATOR 0xDC

DWORD lastWrite;
const BYTE bitmasks[] = {
        0b111111,
        0b111111,
        0b111110,
        0b101010,
};
const COORD paddedBeginLoc = { 0, 1 };

COORD waitClick(HANDLE consoleIn) {
    INPUT_RECORD input;
    DWORD evread;
    while (1) {
        ReadConsoleInputA(consoleIn, &input, 1, &evread);
        if ((input.EventType == MOUSE_EVENT) && (input.Event.MouseEvent.dwButtonState == 1) && (input.Event.MouseEvent.dwEventFlags == 0)) {
            return input.Event.MouseEvent.dwMousePosition;
        }
    }
}

void clearBuffer(HANDLE hConsole)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    SMALL_RECT scrollRect;
    COORD scrollTarget;
    CHAR_INFO fill;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;
    scrollRect.Left = 0;
    scrollRect.Top = 0;
    scrollRect.Right = csbi.dwSize.X;
    scrollRect.Bottom = csbi.dwSize.Y;
    scrollTarget.X = 0;
    scrollTarget.Y = (SHORT)(0 - csbi.dwSize.Y);
    fill.Char.UnicodeChar = TEXT(' ');
    fill.Attributes = csbi.wAttributes;
    ScrollConsoleScreenBufferA(hConsole, &scrollRect, NULL, scrollTarget, &fill);
    csbi.dwCursorPosition.X = 0;
    csbi.dwCursorPosition.Y = 0;
    SetConsoleCursorPosition(hConsole, csbi.dwCursorPosition);
}

void writeBitline(BYTE bitline, BYTE bitmask, HANDLE consoleOut) {
    WriteFile(consoleOut, " ", 1, &lastWrite, 0);
    char output[] = {' ', 0};
    for (int i = 0; i < 6; ++i) {
        if (BIT_TEST(bitmask, i)) {
            output[1] = INDICATOR;
            if (BIT_TEST(bitline, i)) {
                SetConsoleTextAttribute(consoleOut, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            } else {
                SetConsoleTextAttribute(consoleOut, FOREGROUND_INTENSITY);
            }
        } else {
            output[1] = ' ';
        }
        WriteFile(consoleOut, output, 2, &lastWrite, 0);
    }
    WriteFile(consoleOut, "\n", 1, &lastWrite, 0);
}

int main() {
    // initialize buffers
    HANDLE inBuffer, outBuffer;
    COORD buffer_size = { COLUMNS, ROWS };
    inBuffer = GetStdHandle(-10);
    outBuffer = GetStdHandle(-11);

    // resize console window
    SMALL_RECT screen = { 0, 0, 1, 1 };
    SetConsoleWindowInfo(outBuffer, TRUE, &screen);
    SetConsoleScreenBufferSize(outBuffer, buffer_size);
    screen.Right = COLUMNS - 1;
    screen.Bottom = ROWS - 1;
    SetConsoleWindowInfo(outBuffer, TRUE, &screen);

    // initialize variables
    HWND consoleWin = GetConsoleWindow();
    int screenX = GetSystemMetrics(SM_CXSCREEN);
    int screenY = GetSystemMetrics(SM_CYSCREEN);

    // set console mode, title, etc.
    SetConsoleOutputCP(437);
    SetConsoleMode(inBuffer, 0x91);
    SetConsoleTitleA("binclock");
    CONSOLE_CURSOR_INFO cci = { 10, FALSE };
    SetConsoleCursorInfo(outBuffer, &cci);
    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof(cfi);
    cfi.nFont = 0;
    cfi.dwFontSize.X = 8;
    cfi.dwFontSize.Y = 12;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
    wcscpy(cfi.FaceName, L"Terminal");
    SetCurrentConsoleFontEx(outBuffer, FALSE, &cfi);

    // center console window
    RECT winRect;
    GetWindowRect(consoleWin, &winRect);
    SetWindowPos(consoleWin, (HWND) -1, screenX / 2 - (winRect.right - winRect.left) / 2, screenY / 2 - (winRect.bottom - winRect.top) / 2, 0, 0, 97);
    SetWindowLongPtrA(consoleWin, -16, WS_VISIBLE|WS_POPUPWINDOW|WS_DLGFRAME);

    char digits[7];
    BYTE bitlines[6], invertedBitline;
    while (1) {
        SYSTEMTIME time;
        GetLocalTime(&time);
        sprintf(digits, "%.2d%.2d%.2d", time.wHour, time.wMinute, time.wSecond);
        for (int i = 0; i < 6; ++i) {
            bitlines[i] = C2B(digits[i]);
        }
        SetConsoleCursorPosition(outBuffer, paddedBeginLoc);
        invertedBitline = 0;
        for (int i = 3; i >= 0; --i) {
            for (int j = 5; j >= 0; --j) {
                invertedBitline <<= 1;
                invertedBitline |= BIT_TEST(bitlines[j], i);
            }
            writeBitline(invertedBitline, bitmasks[i], outBuffer);
        }
        Sleep(1000);
        if (GetNumberOfConsoleInputEvents(inBuffer, &lastWrite) && lastWrite > 0) {
            INPUT_RECORD inputs[lastWrite];
            DWORD reads;
            ReadConsoleInputA(inBuffer, inputs, lastWrite, &reads);
            for (int i = 0; i < reads; ++i) {
                if (inputs[i].EventType == MOUSE_EVENT) {
                    PMOUSE_EVENT_RECORD event = &inputs[i].Event.MouseEvent;
                    if ((event->dwEventFlags == 0) && (event->dwButtonState == 1) && (event->dwMousePosition.X == COLUMNS-1) && (event->dwMousePosition.Y == ROWS-1)) {
                        clearBuffer(outBuffer);
                        Sleep(100);
                        COORD nextPos = {1,1};
                        SetConsoleCursorPosition(outBuffer, nextPos);
                        SetConsoleTextAttribute(outBuffer, FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
                        WriteFile(outBuffer, METASTRING("binclock"), &lastWrite, 0);
                        nextPos.Y = 2;
                        SetConsoleCursorPosition(outBuffer, nextPos);
                        SetConsoleTextAttribute(outBuffer, FOREGROUND_INTENSITY | FOREGROUND_GREEN);
                        WriteFile(outBuffer, METASTRING("by ./lemon.sh"), &lastWrite, 0);
                        nextPos.Y = 4;
                        SetConsoleCursorPosition(outBuffer, nextPos);
                        SetConsoleTextAttribute(outBuffer, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN);
                        WriteFile(outBuffer, METASTRING("(C) 2021"), &lastWrite, 0);
                        SetConsoleTextAttribute(outBuffer, FOREGROUND_INTENSITY | FOREGROUND_RED);
                        WriteFile(outBuffer, METASTRING("  [X]"), &lastWrite, 0);
                        while (1) {
                            COORD click = waitClick(inBuffer);
                            if ((click.X == 12)&&(click.Y == 4)){
                                clearBuffer(outBuffer);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}
