/**
*
* author one uno dos tres 11112222
* author two quatro cinco seis 33331111
*
*/
//
// Created by syvon on 5/23/17.
//
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <asm/errno.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <memory.h>
#include <fcntl.h>

#define VERSION "1.0.0"

// allows to get the keycode of a ctrl+something key, that macro thing
// is definetly new to me
#define CTRL_KEY(k) ((k) & 0x1f)

enum EditorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PG_UP,
    PG_DOWN,
    MOVE_TAB_LEFT,
    MOVE_TAB_RIGHT
};

void fatal(char *message) {
    write(STDOUT_FILENO, "\x1b[2J", 4); //send an escape sequence to erase the screen
    write(STDOUT_FILENO, "\x1b[H", 3); //resends the cursor at the top of the screen
    perror(message);
    exit(1);
}

/**
 * A text row
 * We use rawSize and rawContent
 * to be able to render some
 * characters differently (like tabs)
 */
struct Row {
    int rawSize;
    char *rawContent;
};

struct Tab {
    char *fileName;
    int numRows;
    int changesCount;
    struct Row *rows;
};

/**
 * A struct that
 * contains the environnement
 * settings like the available rows
 */
struct Environment {
    int screenRows;
    int screenCols;
    int usableTextScreenRows;
    /*** The user's terminal settings ***/
    struct termios orig_termios;

};

struct Environment env;

/**
 * A struct to store
 * the current session
 * of editing
 */
struct Session {
    /*** Cursor positioning ***/
    int colOffset;
    int rowOffset;
    int cursorRow;
    int cursorCol;
    /*** tabs that the user can open ***/
    int currentTabIdx;
    int numTabs;
    struct Tab *tabs;
    /*** message editor row ***/
    int messageLength;
    struct Row messageRow;
    /*** mvmt locked ***/
    int locked;
};

struct Session currentSession;


/**
 *
 * @return the character read from the input
 */
int readKey() {
    int lenRead;
    char cRead;
    while ((lenRead = read(STDIN_FILENO, &cRead, 1) != 1)) {
        if (lenRead == -1 && errno != EAGAIN) {
            fatal("read");
            return -1;
        }
    }

    if (cRead == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) {
            return '\x1b';
        }

        if (read(STDIN_FILENO, &seq[1], 1) != 1) {
            return '\x1b';
        }

        if (seq[0] == '[') {


            if (seq[1] >= '0' && seq[1] <= '9') {

                if (read(STDIN_FILENO, &seq[2], 1) != 1) {
                    return '\x1b';
                }

                char v = seq[2];
                //fatal(&v);

                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1':
                            return HOME_KEY;
                        case '2':
                            return END_KEY;
                        case '3':
                            return DEL_KEY;
                        case '5':
                            return PG_UP;
                        case '6':
                            return PG_DOWN;
                        case '7':
                            return HOME_KEY;
                        case '8':
                            return END_KEY;
                        default:
                            return '\x1b';
                    }
                } else if (seq[2] == ';') {

                    if (read(STDIN_FILENO, &seq[0], 1) != 1) {
                        return '\x1b';
                    }

                    if (read(STDIN_FILENO, &seq[1], 1) != 1) {
                        return '\x1b';
                    }

                    if (seq[0] == '5') {
                        switch (seq[1]) {
                            case 'C':
                                return MOVE_TAB_RIGHT;
                            case 'D':
                                return MOVE_TAB_LEFT;
                            default:
                                return '\x1b';
                        }
                    }
                }

            } else {
                switch (seq[1]) {
                    case 'A':
                        return ARROW_UP;
                    case 'B':
                        return ARROW_DOWN;
                    case 'C':
                        return ARROW_RIGHT;
                    case 'D':
                        return ARROW_LEFT;
                    case 'H':
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                    default:
                        return '\x1b';
                }
            }
        }

        return '\x1b';
    } else if (cRead == '\033') {

    }

    return cRead;
}


/**
 * Get the current tab being edited
 * @return a pointer on the tab or NULL if none
 */
struct Tab *getCurrentTab() {

    struct Tab *tab;

    if ((currentSession.numTabs) > 0 && (currentSession.currentTabIdx < currentSession.numTabs)) {
        tab = &currentSession.tabs[currentSession.currentTabIdx];
    } else {
        tab = NULL;
    }

    return tab;
}

/**
 * Gets the current row being edited
 * @return a pointer on the row or NULL if none
 */
struct Row *getCurrentRow() {
    struct Row *row;

    if (currentSession.locked) {
        return &currentSession.messageRow;
    }

    struct Tab *currentTab = getCurrentTab();
    if (currentTab) {
        int realRowIdx = (currentSession.cursorRow);

        if ((realRowIdx > -1) && (realRowIdx < currentTab->numRows)) {
            row = &currentTab->rows[realRowIdx];
        } else {
            row = NULL;
        }
    } else {
        row = NULL;
    }

    return row;
}

/**
 * Gets the position of the cursor
 * @param rows an int pointer towards the var we want to fill with the number of rows
 * @param cols an int pointer towards the var we want to fill with the number of cols
 * @return -1 on failure, 0 on success
 */
int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }

    while (i < (sizeof(buf) - 1)) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) {
            break;
        }

        if (buf[i] == 'R') {
            break;
        }
        ++i;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') { return -1; }
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) { return -1; }

    return 0;
}

/**
 * Gets the window size of the terminal of the user
 * @param rows an int pointer
 * @param cols an int pointer
 * @return -1 on failure, 0 on success
 */
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    //pass a reference to the ws struct to ioctrl (& is dereferencing)
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {

        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }

        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}


/*** row operations ***/

void rowClearTabs(struct Row *row) {
    int tabs = 0;
    int j;

    for (j = 0; j < row->rawSize; ++j) {
        if (row->rawContent[j] == '\t') {
            ++tabs;
        }
    }

    if (tabs > 0) {
        char *newContent = malloc((size_t) ((row->rawSize + 1) + (tabs * 7) + 1));

        int idx = 0;

        for (j = 0; j < row->rawSize; ++j) {

            if (row->rawContent[j] == '\t') {
                newContent[idx++] = ' ';// add one because the tab takes at least one space
                while (idx % 8 != 0) { // go to a tab stop (each 8 char is a tab col)
                    newContent[idx++] = ' ';
                }
            } else {
                newContent[idx++] = row->rawContent[j];
            }
        }

        free(row->rawContent);
        row->rawContent = newContent;
        row->rawSize = idx;
    }
}

void editorRowInsertTab(struct Row *row, int at) {

    if (!row) {
        fatal("Missing row (editorInsertChar)");
        return;
    }

    if (at < 0 || at > row->rawSize) {
        at = row->rawSize;
    }

    row->rawContent = realloc(row->rawContent, (size_t) row->rawSize + 5);

    memmove(&row->rawContent[at + 4], &row->rawContent[at], (size_t) (row->rawSize - at + 1));
    row->rawSize += 4;

    for (int i = 0; i < 4; ++i) {
        row->rawContent[at + i] = ' ';
    }
}

void editorRowInsertChar(struct Row *row, int at, int c) {

    struct Tab *currentTab = getCurrentTab();
    ++currentTab->changesCount;

    if (!row) {
        fatal("Missing row (editorInsertChar)");
        return;
    }

    if (at < 0 || at > row->rawSize) {
        at = row->rawSize;
    }

    row->rawContent = realloc(row->rawContent, (size_t) row->rawSize + 2);

    memmove(&row->rawContent[at + 1], &row->rawContent[at], (size_t) (row->rawSize - at + 1));
    ++row->rawSize;
    row->rawContent[at] = (char) c;
}

void editorAppendRow(char *s, size_t len) {

    struct Tab *currentTab = getCurrentTab();

    if (NULL == currentTab) {
        fatal("No current tab (append row)");
        return;
    }

    ++currentTab->changesCount;

    //add more room for the rows
    size_t rowSize = sizeof(struct Row);

    if (currentTab->numRows == 0) {
        currentTab->rows = malloc(rowSize);
    } else {
        currentTab->rows = realloc(currentTab->rows, rowSize * (currentTab->numRows + 1));
    }

    int at = currentTab->numRows;

    currentTab->rows[at].rawSize = (int) len;
    currentTab->rows[at].rawContent = malloc(len + 1);
    //fill the content
    memcpy(currentTab->rows[at].rawContent, s, len);
    currentTab->rows[at].rawContent[len] = '\0';

    rowClearTabs(&currentTab->rows[at]);

    currentTab->numRows = currentTab->numRows + 1;
}

/*** Editor operation ***/

void editorInsertChar(int c) {
    struct Tab *currentTab = getCurrentTab();

    if (!currentTab) {
        fatal("Missing tab (editorInsertChar)");
        return;
    }

    ++currentTab->changesCount;

    /*
    if ((currentSession.cursorRow) >= (currentTab->numRows)) {
        editorAppendRow("", 0);
    }
    */

    if (NULL == getCurrentRow()) {
        editorAppendRow("", 0);
    }

    editorRowInsertChar(getCurrentRow(), (currentSession.cursorCol + currentSession.colOffset), c);
    ++currentSession.cursorCol;
}

void onTabKeyPress() {
    struct Tab *currentTab = getCurrentTab();

    if (!currentTab) {
        fatal("Missing tab (editorInsertChar)");
        return;
    }

    if ((currentSession.cursorRow) >= (currentTab->numRows)) {
        editorAppendRow("", 0);
    }

    editorRowInsertTab(getCurrentRow(), (currentSession.cursorCol + currentSession.colOffset));
    currentSession.cursorCol += 4;
}

void editorInsertNewRow() {
    // The idea is to take the remaining of the line
    // Insert it in a new row
    // append the new row in the middle of the tab

    struct Tab *currentTab = getCurrentTab();

    if (NULL == currentTab) {
        fatal("No current tab (append row)");
        return;
    }

    ++currentTab->changesCount;

    int currentRowIdx = currentSession.cursorRow;
    int nextRowIdx = currentRowIdx + 1;

    size_t rowSize = sizeof(struct Row);
    //get room for one more
    currentTab->rows = realloc(currentTab->rows, rowSize * (currentTab->numRows + 1));

    int at;

    if (nextRowIdx > (currentTab->numRows)) {
        at = currentTab->numRows;
    } else {
        at = nextRowIdx;
        // if we are not on a new line, move the data
        memmove(&currentTab->rows[nextRowIdx + 1],
                &currentTab->rows[nextRowIdx],
                rowSize * ((size_t) (currentTab->numRows - nextRowIdx)));
    }

    struct Row *currentRow = getCurrentRow();

    int len;
    char *s;

    if (currentRow) {
        if (currentSession.cursorCol <= currentRow->rawSize) {
            len = currentRow->rawSize - currentSession.cursorCol;
            s = malloc((size_t) len);
            s = memcpy(s, &currentRow->rawContent[currentSession.cursorCol], (size_t) len);

            int newLen = currentSession.cursorCol;

            currentRow->rawContent = realloc(currentRow->rawContent, (size_t) (sizeof(char) * (newLen + 1)));

            if (NULL == currentRow->rawContent) {
                fatal("Failed to shrink the current row (editorInsertRow)");
                return;
            }

            currentRow->rawContent[newLen] = '\0';
            currentRow->rawSize = newLen;
        } else {
            s = NULL;
            len = 0;
        }
    } else {
        s = NULL;
        len = 0;
    }

    currentTab->rows[at].rawSize = len;
    currentTab->rows[at].rawContent = malloc((size_t) (len + 1));
    //fill the content
    memcpy(currentTab->rows[at].rawContent, s, (size_t) len);
    currentTab->rows[at].rawContent[len] = '\0';

    currentTab->numRows = currentTab->numRows + 1;

    free(s);

    ++currentSession.cursorRow;

    currentSession.cursorCol = 0;
    currentSession.colOffset = 0;
}

void editorScroll() {

    //TODO : This scroll thing is confusing

    if (currentSession.cursorRow < currentSession.rowOffset) {
        currentSession.rowOffset = currentSession.cursorRow;
    } else if (currentSession.cursorRow >= (currentSession.rowOffset + env.usableTextScreenRows)) {
        if (currentSession.locked) {
            currentSession.rowOffset =
                    (currentSession.cursorRow - env.screenRows) + (env.screenRows - (env.usableTextScreenRows + 1));
        } else {
            currentSession.rowOffset = (currentSession.cursorRow - env.usableTextScreenRows) + 1;
        }
    }

    /*
    if(currentSession.locked) {
    } else {
        if (currentSession.cursorRow < currentSession.rowOffset) {
            currentSession.rowOffset = currentSession.cursorRow;
        } else if (currentSession.cursorRow >= (currentSession.rowOffset + env.usableTextScreenRows)) {
            currentSession.rowOffset = (currentSession.cursorRow - env.usableTextScreenRows) + 1;
        }
    }
    */


    if (currentSession.cursorCol < currentSession.colOffset) {
        currentSession.colOffset = currentSession.cursorCol;
    } else if (currentSession.cursorCol >= (currentSession.colOffset + env.screenCols)) {
        currentSession.colOffset = (currentSession.cursorCol - env.screenCols) + 1;
    }
}


void deleteRowAtIdx(int idx) {

    struct Tab *currentTab = getCurrentTab();

    if (NULL == currentTab) {
        fatal("No current tab (del row at idx)");
        return;
    }

    int len = currentTab->numRows - idx;

    memmove(&currentTab->rows[idx], &currentTab->rows[idx + 1], sizeof(struct Row) * len);
    currentTab->rows = realloc(currentTab->rows, sizeof(struct Row) * (currentTab->numRows - 1));
    --currentTab->numRows;
    /*
    if (idx > 0) {
        int curCursorRow = currentSession.cursorRow;
        currentSession.cursorRow = idx - 1;
        struct Row *previousRow = getCurrentRow();
        if (previousRow) {
            currentSession.cursorCol = previousRow->rawSize;
            if (previousRow->rawSize < env.screenCols) {
                currentSession.colOffset = 0;
            } else {
                editorScroll();
            }
        }
        currentSession.cursorRow = curCursorRow;
    }
     */
}

//TODO this method does way more than remove a row, we should fix that
void editorRemoveRow() {

    struct Tab *currentTab = getCurrentTab();

    if (NULL == currentTab) {
        fatal("Not tab is currently loaded (editorRemoveRow)");
        return;
    }

    ++currentTab->changesCount;

    struct Row *currentRow = getCurrentRow();
    int currentRowIdx = currentSession.cursorRow;

    if (NULL == currentRow) {
        goto go_back;
    }

    if (currentRow->rawSize > 0 && currentSession.cursorRow > 0) {
        --currentSession.cursorRow;
        struct Row *previousRow = getCurrentRow();
        ++currentSession.cursorRow;

        int previousSize = previousRow->rawSize;

        previousRow->rawContent = realloc(previousRow->rawContent,
                                          (size_t) (previousRow->rawSize +
                                                    currentRow->rawSize + 1));

        memcpy(&previousRow->rawContent[previousSize], currentRow->rawContent, (size_t) currentRow->rawSize);

        previousRow->rawSize += currentRow->rawSize;
        previousRow->rawContent[previousRow->rawSize] = '\0';

        //that line is about to be deleted, so let's clear it up
        free(currentRow->rawContent);
    }

    int len = currentTab->numRows - currentRowIdx;

    memmove(&currentTab->rows[currentRowIdx], &currentTab->rows[currentRowIdx + 1], sizeof(struct Row) * len);
    currentTab->rows = realloc(currentTab->rows, sizeof(struct Row) * (currentTab->numRows - 1));
    --currentTab->numRows;

    go_back:
    if (currentRowIdx > 0) {
        --currentSession.cursorRow;

        struct Row *previousRow = getCurrentRow();

        if (previousRow) {
            currentSession.cursorCol = previousRow->rawSize;

            if (previousRow->rawSize < env.screenCols) {
                currentSession.colOffset = 0;
            } else {
                editorScroll();
            }
        }
    }
}


void editorDelKey() {
    struct Row *row = getCurrentRow();

    if (!row) {
        if (currentSession.cursorRow > 0) {
            --currentSession.cursorRow;
        }
        return;
    }

    struct Tab *tab = getCurrentTab();
    ++tab->changesCount;

    int pos = currentSession.cursorCol;


    if (currentSession.cursorCol < row->rawSize) {
        if (currentSession.locked && ((currentSession.cursorCol - 1) < currentSession.messageLength)) {
            return;
        }

        //We delete the char after
        memmove(&row->rawContent[pos], &row->rawContent[pos + 1], (size_t) (row->rawSize - (pos - 1) - 1));
        row->rawContent = realloc(row->rawContent, (size_t) (row->rawSize));

        --(row->rawSize);

    } else if (!currentSession.locked) {

        ++currentSession.cursorRow;
        struct Row *nextRow = getCurrentRow();
        --currentSession.cursorRow;

        int currentSize = row->rawSize;

        row->rawContent = realloc(row->rawContent,
                                  (size_t) (nextRow->rawSize +
                                            row->rawSize + 1));

        memcpy(&row->rawContent[currentSize], nextRow->rawContent, (size_t) nextRow->rawSize);

        row->rawSize += nextRow->rawSize;
        row->rawContent[row->rawSize] = '\0';

        //that line is about to be deleted, so let's clear it up
        free(nextRow->rawContent);

        deleteRowAtIdx(currentSession.cursorRow + 1);
    }
}

void editorBackspace() {
    struct Row *row = getCurrentRow();

    if (!row) {
        if (currentSession.cursorRow > 0) {
            --currentSession.cursorRow;
        }
        return;
    }

    struct Tab *tab = getCurrentTab();
    ++tab->changesCount;

    int pos = currentSession.cursorCol;

    if (pos < 1 && (!currentSession.locked)) {
        editorRemoveRow();
        return;
    } else if (pos > row->rawSize) {
        return;
    }


    if (currentSession.cursorCol > 0) {
        if (currentSession.locked && ((currentSession.cursorCol - 1) < currentSession.messageLength)) {
            return;
        }

        //We delete the char before
        memmove(&row->rawContent[pos - 1], &row->rawContent[pos], (size_t) (row->rawSize - (pos - 1) - 1));
        row->rawContent = realloc(row->rawContent, (size_t) (row->rawSize));

        --(row->rawSize);

        --(currentSession.cursorCol);
    }
}

/*** file i/o ***/

/**
 * Gets all the rows and render the string representation
 * of those rows
 * @param bufLen the size of the buffer (will be filled in)
 * @return the buffer. Please do not forget to free the buffer!
 * Comment
 * SY
 * Its kinda bad that we expect the caller to close the ressources
 * of the callee. Of course hes gonna forget
 * Maybe we should do it all on the side of the caller. Have a get buffer lenght
 * method, and then he creates it, then we fill it with another method. Its a lot
 * more work, but its also way safer.
 * Ill keep this in because it came from the tutorial, but its not good
 */
char *editorRowsToString(int *bufLen) {

    struct Tab *tab = getCurrentTab();

    if (!tab) {
        fatal("Missing tab (editorRowsToString)");
        return NULL;
    }


    int totalLen = 0;
    int j;
    for (j = 0; j < (tab->numRows); ++j) {
        totalLen += tab->rows[j].rawSize + 1;
    }

    *bufLen = totalLen;

    char *buf = malloc((size_t) totalLen);
    char *p = buf;

    for (j = 0; j < (tab->numRows); ++j) {
        memcpy(p, tab->rows[j].rawContent, (size_t) tab->rows[j].rawSize);
        p += tab->rows[j].rawSize;
        *p = '\n';
        ++p;
    }

    return buf;
}

void editorPrompt(char *msg, int msgLen);

void editorSave() {

    struct Tab *tab = getCurrentTab();

    if (NULL == tab) {
        fatal("No current tab (editorSave)");
        return;
    }

    if (NULL == tab->fileName) {
        const int msgLen = 46;
        editorPrompt("Please enter a file name (or none to cancel): ", msgLen);

        int responseLength = currentSession.messageRow.rawSize - msgLen + 1;

        if (responseLength > 0) {
            tab->fileName = malloc((size_t) sizeof(char) * responseLength);

            if (NULL == tab->fileName) {
                fatal("Failed to malloc the length of the message filename required");
                return;
            }

            tab->fileName = memcpy(
                    tab->fileName,
                    &currentSession.messageRow.rawContent[msgLen],
                    (size_t) sizeof(char) * responseLength);

            if (NULL == tab->fileName) {
                fatal("Failed to copy the content of the filename into the tab struct");
                return;
            }

        } else {
            return;
        }
    }


    int len;
    char *buf = editorRowsToString(&len);

    int fd = open(tab->fileName, O_RDWR | O_CREAT, 0644);

    if (fd != -1) {
        if (-1 != ftruncate(fd, len)) {
            write(fd, buf, (size_t) len);
        }
        close(fd);
    }
    free(buf);
}

static size_t tabSize = sizeof(struct Tab);

void createTab() {
    const int currentTabCount = currentSession.numTabs;

    currentSession.tabs = realloc(currentSession.tabs, tabSize * (currentTabCount + 1));
    if (currentTabCount > 0) {
        const int currentTabIdx = currentSession.currentTabIdx;
        memmove(&currentSession.tabs[currentTabIdx + 2],
                &currentSession.tabs[currentTabIdx + 1],
                sizeof(struct Tab) * (currentTabCount - currentTabIdx - 1));
    }

    ++currentSession.numTabs;
    ++currentSession.currentTabIdx;

    struct Tab *currTab = getCurrentTab();
    currTab->numRows = 0;
    currTab->rows = NULL;
    currTab->fileName = NULL;
    currTab->changesCount = 0;
}

void closeTab() {

    const int currentTabCount = currentSession.numTabs;
    const int currentTabIdx = currentSession.currentTabIdx;

    size_t tabSize = sizeof(struct Tab);

    if (currentTabIdx < (currentTabCount - 1)) {
        memmove(&currentSession.tabs[currentTabIdx], &currentSession.tabs[currentTabIdx + 1],
                tabSize * (currentTabCount - currentTabIdx - 1));
    }

    currentSession.tabs = realloc(currentSession.tabs, tabSize * (currentTabCount - 1));

    if (currentTabIdx > 0) {
        --currentSession.currentTabIdx;
    }

    if (currentSession.numTabs > 0) {
        --currentSession.numTabs;
    }

    if (currentSession.numTabs == 0) {
        currentSession.currentTabIdx = -1;
        createTab();
    }
}

void editorOpen(char *filename, int openInNewTab) {

    if (openInNewTab) {
        createTab();
    }

    struct Tab *currentTab = getCurrentTab();

    if (NULL == currentTab) {
        fatal("No current tab (editor open)");
        return;
    }

    FILE *fp = fopen(filename, "r");

    if (!fp) {
        fatal("fopen");
        return;
    } else {
        currentTab->fileName = filename;
    }

    char *line = NULL;
    ssize_t lineLen;
    size_t lineCap = 0;

    while ((lineLen = getline(&line, &lineCap, fp)) != -1) {

        if (lineLen > -1) {
            while (lineLen > 0 && (line[lineLen - 1] == '\n' ||
                                   line[lineLen - 1] == '\r')) {
                lineLen--;
            }
            editorAppendRow(line, (size_t) lineLen);
        }
    }

    free(line);
    fclose(fp);
}


/*** small string ***/

struct SmallStr {
    char *b;
    int len;
};

#define SMALLSTR_INIT {NULL, 0};

void appendToStr(struct SmallStr *str, const char *s, int len) {

    char *new = realloc(str->b, (size_t) (str->len + len));

    if (new == NULL) return;

    memcpy(&new[str->len], s, (size_t) len);

    str->b = new;
    str->len += len;

}

void clearStr(struct SmallStr *str) {
    free(str->b);
}

void init() {
    currentSession.colOffset = 0;
    currentSession.rowOffset = 0;

    currentSession.cursorRow = 0;
    currentSession.cursorCol = 0;

    currentSession.currentTabIdx = -1;
    currentSession.numTabs = 0;

    currentSession.locked = 0;
    currentSession.messageLength = 0;

    int *cols = &env.screenCols;
    int *rows = &env.screenRows;

    if (getWindowSize(rows, cols) == -1) {
        *cols = 10;
        *rows = 10;
    }

    env.usableTextScreenRows = *rows - 3;
}

void disableRawMode() {
    //takes the origin settings and restores them
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &env.orig_termios) == -1)
        fatal("tcsetattr");
}

void setRawMode() {
    //save the current settings in the static var
    if (tcgetattr(STDIN_FILENO, &env.orig_termios) == -1)
        fatal("tcgetattr");
    //when we exit, we call disable_raw_mode
    atexit(disableRawMode);

    struct termios raw = env.orig_termios;

    tcgetattr(STDIN_FILENO, &raw);
    //disable the signal for ctrl-s and ctrl-q, so stop output
    raw.c_iflag &= ~(IXON);
    //disable the post output
    //That means that without a \r we wont ge the return
    raw.c_oflag &= ~(OPOST);
    //disable the echo
    //disable the canonical mode
    //disable the ctrl-c
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    //minimum byte size before read can return
    raw.c_cc[VMIN] = 0;
    //maximum time to can elapse before read returns
    raw.c_cc[VTIME] = 1;

    //set the terms settings to the new ones
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        fatal("tcsetattr");
}

void setCursorAtStart() {
    write(STDOUT_FILENO, "\x1b[H", 3); //resends the cursor at the top of the screen
}

void clearAllLinesAndGoToStart(struct SmallStr *str) {
    appendToStr(str, "\x1b[2J", 4); //send an escape sequence to erase the screen
    setCursorAtStart();
}

void editorInvertColor(struct SmallStr *str) {
    appendToStr(str, "\x1b[7m", 4);
}

void editorNormalColor(struct SmallStr *str) {
    appendToStr(str, "\x1b[m", 3);
}


void eraseLineFromCursor(struct SmallStr *str) {
    appendToStr(str, "\x1b[K", 3);
}

void editorDrawStatusBar(struct SmallStr *str) {
    editorInvertColor(str);

    char status[env.screenCols];

    int row = currentSession.cursorRow + 1;
    int col = currentSession.cursorCol + 1;

    int currentTab = currentSession.currentTabIdx + 1;
    int totalTab = currentSession.numTabs;

    struct Tab *tab = getCurrentTab();

    char *fileName = tab->fileName;

    int statusLen;

    if (NULL == fileName) {
        statusLen =
                snprintf(status, sizeof(status), "Line %d, Column %d, Tab %d of %d, File never saved / new file",
                         row, col, currentTab, totalTab);

    } else {
        statusLen =
                snprintf(status, sizeof(status), "Line %d, Column %d, Tab %d of %d, File %s",
                         row, col, currentTab, totalTab, fileName);
    }

    if (statusLen > env.screenCols) {
        statusLen = env.screenCols;
    }

    appendToStr(str, status, statusLen);

    //make the rest of the line color inverted
    for (int i = env.screenCols - statusLen; i > 0; --i) {
        appendToStr(str, " ", 1);
    }

    editorNormalColor(str);
}

void editorDrawStatusRow(struct SmallStr *str) {

    struct Row row = currentSession.messageRow;

    if (currentSession.locked) {
        appendToStr(str, (row.rawContent + currentSession.colOffset), row.rawSize);
    }

    eraseLineFromCursor(str);
    appendToStr(str, "\r\n", 2);
}

void editorDrawRows(struct SmallStr *str) {

    struct Tab *tab = getCurrentTab();

    if (!tab) {
        fatal("No tab (editorDrawRow");
        return;
    }

    for (int y = 0; y < env.usableTextScreenRows; ++y) {
        int fileRow = y + currentSession.rowOffset;
        if (fileRow >= tab->numRows) {
            if ((tab->numRows == 0) && (y == (env.usableTextScreenRows / 3) + 1)) {

                char welcome[80];
                int welcomeLen = snprintf(welcome, sizeof(welcome), "-- Mithril -- version %s", VERSION);

                if (welcomeLen > env.screenCols) {
                    welcomeLen = env.screenCols;
                }

                int padding = (env.screenCols - welcomeLen) / 2;

                if (padding) {
                    appendToStr(str, "~", 1);
                    --padding;
                }

                while ((padding--) > 0) {
                    appendToStr(str, " ", 1);
                }

                appendToStr(str, welcome, welcomeLen);
            } else {
                appendToStr(str, "~", 1);
            }
        } else {
            int len = tab->rows[fileRow].rawSize;
            len = len - currentSession.colOffset;
            //make sure the len is never more than what we actually can display
            //if we have a col offset of 5 on a len 10 sentence
            //we want to start at the 5th char
            //but we only want to display 5 chars, not the 10
            if (len < 0) {
                len = 0;
            }

            if (len > env.screenCols) {
                len = env.screenCols;
            }

            appendToStr(str, (tab->rows[fileRow].rawContent + currentSession.colOffset), len);
        }


        eraseLineFromCursor(str);
        appendToStr(str, "\r\n", 2);
    }

    eraseLineFromCursor(str);
    appendToStr(str, "\r\n", 2);
}

void editorRefreshScreen() {

    editorScroll();

    struct SmallStr str = SMALLSTR_INIT;

    appendToStr(&str, "\x1b[?25l", 6);
    appendToStr(&str, "\x1b[H", 3);

    editorDrawRows(&str);
    editorDrawStatusRow(&str);
    editorDrawStatusBar(&str);


    char buf[32];

    if (currentSession.locked) {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
                 (currentSession.cursorRow) + 1,// - currentSession.rowOffset
                 (currentSession.cursorCol) + 1);// - currentSession.colOffset
    } else {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
                 (currentSession.cursorRow - currentSession.rowOffset) + 1,
                 (currentSession.cursorCol - currentSession.colOffset) + 1);
    }


    appendToStr(&str, buf, (int) strlen(buf));

    // Hides the cursor when repainting
    // appendToStr(&str, "\x1b[H", 3);
    appendToStr(&str, "\x1b[?25h", 6);

    write(STDOUT_FILENO, str.b, (size_t) str.len);
    clearStr(&str);
}


/*** input ***/

void moveToBeginningOfLine() {
    currentSession.cursorCol = 0;
    currentSession.colOffset = 0;
}

void moveToEndOfLine() {
    struct Row *row = getCurrentRow();
    if (row) {
        currentSession.cursorCol = row->rawSize;
    }
}


void snapAtEndIfPast() {
    struct Row *row = getCurrentRow();

    if (row && ((currentSession.cursorCol > row->rawSize) || (currentSession.colOffset > row->rawSize))) {
        currentSession.cursorCol = row->rawSize;

        if (row->rawSize < env.screenCols) {
            currentSession.colOffset = 0;
        } else {
            editorScroll();
        }
    } else if (!row) {
        currentSession.cursorCol = 0;
    }

    editorScroll();
}

void editorCursorMove(int code) {

    struct Tab *currentTab = getCurrentTab();

    if (!currentTab) return;

    struct Row *row = getCurrentRow();

    if (currentSession.locked) return;

    switch (code) {
        case ARROW_UP:
            if (currentSession.cursorRow > 0) {
                currentSession.cursorRow--;
                snapAtEndIfPast();
            }
            break;
        case ARROW_DOWN:
            if (currentSession.cursorRow < (currentTab->numRows)) {
                currentSession.cursorRow++;
                snapAtEndIfPast();
            }
            break;
        case ARROW_LEFT:
            if (currentSession.cursorCol > 0) {
                currentSession.cursorCol--;
            } else if (currentSession.cursorRow > 0) {
                --currentSession.cursorRow;
                moveToEndOfLine();
            }
            break;
        case ARROW_RIGHT:
            if (row && currentSession.cursorCol < row->rawSize) {
                currentSession.cursorCol++;
            } else if (currentSession.cursorRow + currentSession.rowOffset < (currentTab->numRows - 1)) {
                ++currentSession.cursorRow;
                moveToBeginningOfLine();
            }
            break;
        default:
            break;
    }
}

void openFile();

void processKeyPress() {
    int c = readKey();

    switch (c) {
        case '\r':
            fatal("Someone pressed enter! :D");
            break;
        case '\n':
            if (currentSession.locked) {
                currentSession.locked = 0;
            } else {
                editorInsertNewRow();
            }
            break;
        case CTRL_KEY('q'): {
            struct SmallStr str = SMALLSTR_INIT;
            clearAllLinesAndGoToStart(&str);
            write(STDOUT_FILENO, str.b, (size_t) str.len);
            clearStr(&str);
            exit(0);
        }
        case '\t' : {
            onTabKeyPress();
        }
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorCursorMove(c);
            break;
        case PG_UP:
        case PG_DOWN: {
            int times = env.usableTextScreenRows;
            while ((times--) > 0) {
                editorCursorMove((c == PG_UP) ? ARROW_UP : ARROW_DOWN);
            }
        }
            break;
        case HOME_KEY: {
            moveToBeginningOfLine();
        }
            break;
        case END_KEY: {
            moveToEndOfLine();
        }
            break;
        case BACKSPACE:
        case CTRL_KEY('h'):
            editorBackspace();
            break;
        case DEL_KEY: {
            editorDelKey();
        }
            break;
        case CTRL_KEY('l'):
        case '\x1b':
            break;

        case CTRL_KEY('s'): {
            editorSave();
        }
            break;
        case CTRL_KEY('o') : {
            openFile();
        }
            break;


        case CTRL_KEY('t') : {
            createTab();
        }
            break;

        case CTRL_KEY('w') : {
            closeTab();
        }
            break;

        case MOVE_TAB_LEFT:
            if (currentSession.currentTabIdx > 0) {
                --currentSession.currentTabIdx;
            }
            break;
        case MOVE_TAB_RIGHT:
            if (currentSession.currentTabIdx + 1 < currentSession.numTabs) {
                ++currentSession.currentTabIdx;
            }
            break;

        default:
            editorInsertChar(c);
            break;
    }
}

void editorPrompt(char *msg, int msgLen) {

    struct Row *messageRow = &currentSession.messageRow;

    int previousRow = currentSession.cursorRow;
    int previousCol = currentSession.cursorCol;

    messageRow->rawContent = realloc(messageRow->rawContent, (size_t) msgLen + 1);
    messageRow->rawSize = msgLen;
    memcpy(messageRow->rawContent, msg, (size_t) msgLen);
    messageRow->rawContent[msgLen] = '\0';

    currentSession.cursorRow = env.screenRows - 2;
    currentSession.cursorCol = msgLen;

    currentSession.messageLength = msgLen;

    currentSession.locked = 1;

    while (currentSession.locked) {
        editorRefreshScreen();
        processKeyPress();
    }

    // after the editor unlock

    currentSession.cursorRow = previousRow;
    currentSession.cursorCol = previousCol;
    //free(msg);
}


void openFile() {
    /*
    editorPrompt("Open in current file (y/n/c): ", 30);
    if ((currentSession.messageRow.rawSize - 30) > 0) {
        char c = currentSession.messageRow.rawContent[30];
        int openInOtherTab;
        if (c == 'y') {
            openInOtherTab = 0;
        } else if (c == 'n') {
            openInOtherTab = 1;
        } else {
            return;
        }*/

    editorPrompt("Please enter a file name (None to exit): ", 41);
    if ((currentSession.messageRow.rawSize - 41) > 0) {
        editorOpen(&currentSession.messageRow.rawContent[41], 0);
    }
    //}

}

int main(int argc, char *argv[]) {

    setRawMode();
    init();

    for (int argPos = 1; argPos < argc; ++argPos) {
        editorOpen(argv[argPos], 1);
    }

    if (currentSession.numTabs < 1) {
        createTab();
    }
    currentSession.currentTabIdx = 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        editorRefreshScreen();
        processKeyPress();
    }
#pragma clang diagnostic pop
}