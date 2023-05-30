#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <shell_handler.hh>
#include <array>
#include <vector>
#include <argtable3/argtable3.h>
#include <cstring>
#include <string>
#include <deque>
#include <esp_log.h>
#include <esp_chip_info.h>
#include <esp_ota_ops.h>
#include <i2c.hh>
#include "shell_handler.hh"
#define TAG "CLI"
/*various concepts stolen from https://github.com/antirez/linenoise and https://github.com/JingoC/terminal/*/

void FreeMemoryForOTA();

namespace CLI
{
    int Static_writefn(void *data, const char *buffer, int size);

    constexpr size_t MAX_LINE_LENGTH{160};
    constexpr size_t LINENOISE_DEFAULT_HISTORY_MAX_LEN{16};
    constexpr size_t PROMPT_LEN{6};
    constexpr int EVBIT_CURSOR_POS_RECEIVED{(1 << 0)};
    /*
        constexpr const char *COLOR_RESET = "\x1B[0m";
        constexpr const char *COLOR_BLACK = "\x1B[0;30m";
        constexpr const char *COLOR_RED = "\x1B[0;31m";
        constexpr const char *COLOR_GREEN = "\x1B[0;32m";
        constexpr const char *COLOR_YELLOW = "\x1B[0;33m";
        constexpr const char *COLOR_BLUE = "\x1B[0;34m";
        constexpr const char *COLOR_MAGENTA = "\x1B[0;35m";
        constexpr const char *COLOR_CYAN = "\x1B[0;36m";
        constexpr const char *COLOR_WHITE = "\x1B[0;37m";
        constexpr const char *COLOR_BOLD_ON = "\x1B[1m";
        constexpr const char *COLOR_BOLD_OFF = "\x1B[22m";
        constexpr const char *COLOR_BRIGHT_BLACK = "\x1B[0;90m";
        constexpr const char *COLOR_BRIGHT_RED = "\x1B[0;91m";
        constexpr const char *COLOR_BRIGHT_GREEN = "\x1B[0;92m";
        constexpr const char *COLOR_BRIGHT_YELLOW = "\x1B[0;99m";
        constexpr const char *COLOR_BRIGHT_BLUE = "\x1B[0;94m";
        constexpr const char *COLOR_BRIGHT_MAGENTA = "\x1B[0;95m";
        constexpr const char *COLOR_BRIGHT_CYAN = "\x1B[0;96m";
        constexpr const char *COLOR_BRIGHT_WHITE = "\x1B[0;97m";
        constexpr const char *COLOR_UNDERLINE = "\x1B[4m";
        constexpr const char *COLOR_BRIGHT_RED_BACKGROUND = "\x1B[41;1m";
        */
#define COLOR_RESET "\x1B[0m"
#define COLOR_BLACK "\x1B[0;30m"
#define COLOR_RED "\x1B[0;31m"
#define COLOR_GREEN "\x1B[0;32m"
#define COLOR_YELLOW "\x1B[0;33m"
#define COLOR_BLUE "\x1B[0;34m"
#define COLOR_MAGENTA "\x1B[0;35m"
#define COLOR_CYAN "\x1B[0;36m"
#define COLOR_WHITE "\x1B[0;37m"
#define COLOR_BOLD_ON "\x1B[1m"
#define COLOR_BOLD_OFF "\x1B[22m"
#define COLOR_BRIGHT_BLACK "\x1B[0;90m"
#define COLOR_BRIGHT_RED "\x1B[0;91m"
#define COLOR_BRIGHT_GREEN "\x1B[0;92m"
#define COLOR_BRIGHT_YELLOW "\x1B[0;99m"
#define COLOR_BRIGHT_BLUE "\x1B[0;94m"
#define COLOR_BRIGHT_MAGENTA "\x1B[0;95m"
#define COLOR_BRIGHT_CYAN "\x1B[0;96m"
#define COLOR_BRIGHT_WHITE "\x1B[0;97m"
#define COLOR_UNDERLINE "\x1B[4m"
#define COLOR_BRIGHT_RED_BACKGROUND "\x1B[41;1m"
#define CURSOR_BACKWARD(N) "\033[ND"
#define CURSOR_FORWARD(N) "\033[NC"
    enum class KEY_ACTION
    {
        KEY_NULL = 0,   /* NULL */
        CTRL_A = 1,     /* Ctrl+a */
        CTRL_B = 2,     /* Ctrl-b */
        CTRL_C = 3,     /* Ctrl-c */
        CTRL_D = 4,     /* Ctrl-d */
        CTRL_E = 5,     /* Ctrl-e */
        CTRL_F = 6,     /* Ctrl-f */
        CTRL_H = 8,     /* Ctrl-h */
        TAB = 9,        /* Tab */
        CTRL_K = 11,    /* Ctrl+k */
        CTRL_L = 12,    /* Ctrl+l */
        ENTER = 13,     /* Enter */
        CTRL_N = 14,    /* Ctrl-n */
        CTRL_P = 16,    /* Ctrl-p */
        CTRL_T = 20,    /* Ctrl-t */
        CTRL_U = 21,    /* Ctrl+u */
        CTRL_W = 23,    /* Ctrl+w */
        ESC = 27,       /* Escape */
        BACKSPACE = 127 /* Backspace */
    };

    class AbstractCommand
    {
    public:
        virtual int Execute(IShellCallback *cb, int argc, char *argv[]) = 0;
        virtual const char *GetName() = 0;
    };

    class HistoryQueue : public std::deque<std::string>
    {
    private:
        size_t maxSize;

    public:
        HistoryQueue() : std::deque<std::string>(), maxSize(10)
        {
        }

        HistoryQueue(size_t maxSize) : std::deque<std::string>(), maxSize(maxSize)
        {
        }
        void AddToQueueConsiderSizeLimit(std::string s)
        {
            this->push_front(s);
            if (this->size() > maxSize)
            {
                this->pop_back();
            }
        }
    };


    class HelpCommand : public AbstractCommand
    {
        int Execute(IShellCallback *cb, int argc, char *argv[]) override
        {
            cb->printf("\r\nI am the help command\r\n");
            return 0;
        }
        const char *GetName() override
        {
            return "help";
        }
    };

    class SystemInfoCommand : public AbstractCommand
    {
        const char *GetName() override
        {
            return "systeminfo";
        }

        int Execute(IShellCallback *cb, int argc, char *argv[]) override
        {
            uint32_t res = esp_get_free_heap_size();

            int exitcode = 0;
            ESP_LOGI(TAG, "We have %i arguments", argc);
            for (int i = 0; i < argc; i++)
            {
                ESP_LOGI(TAG, "Argument %i is %s", i, argv[i]);
            }

            arg_lit *help = arg_litn(NULL, "help", 0, 1, "display this help and exit");
            arg_lit *version = arg_litn(NULL, "version", 0, 1, "display version info and exit");
            arg_int *level = arg_intn(NULL, "level", "<n>", 0, 1, "foo value");
            arg_lit *verb = arg_litn("v", "verbose", 0, 1, "verbose output");
            arg_file *o = arg_filen("o", NULL, "myfile", 0, 1, "output file");
            arg_file *file = arg_filen(NULL, NULL, "<file>", 1, 100, "input files");
            auto end_arg = arg_end(20);

            void *argtable[] = {help, version, level, verb, o, file, end_arg};

            FILE *fp = funopen(cb, nullptr, &Static_writefn, nullptr, nullptr);

            int nerrors = arg_parse(argc, argv, argtable);
            if (help->count > 0)
            {
                fprintf(stdout, "Usage: %s", GetName());
                arg_print_syntax(stdout, argtable, "\r\n");
                fprintf(stdout, "Demonstrate command-line parsing in argtable3.\r\n");
                arg_print_glossary(stdout, argtable, "  %-25s %s\r\n");
                exitcode = 0;
                goto exit;
            }

            // If the parser returned any errors then display them and exit
            if (nerrors > 0)
            {
                // Display the error details contained in the arg_end struct.
                arg_print_errors(fp, end_arg, GetName());
                fprintf(fp, "\r\nTry '%s --help' for more information.\n", GetName());
                exitcode = 1;
                goto exit;
            }

            cb->printf("Free Heap is %lu, level value was %i\r\n", res, level->ival[0]);
        exit:
            // deallocate each non-null entry in argtable[]
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            fclose(fp);
            return exitcode;
        }
    };

    class I2CDetectCommand : public CLI::AbstractCommand
    {
        int Execute(IShellCallback *cb, int argc, char *argv[]) override
        {
            arg_int *busindexArg = arg_intn("F", nullptr, "<n>", 0, 2, "bus index");
            auto end_arg = arg_end(2);
            int busindex{0};

            void *argtable[] = {busindexArg, end_arg};

            FILE *fp = funopen(cb, nullptr, &CLI::Static_writefn, nullptr, nullptr);
            int exitcode{0};

            int nerrors = arg_parse(argc, argv, argtable);
            if (nerrors > 0)
            {
                // Display the error details contained in the arg_end struct.
                arg_print_errors(fp, end_arg, GetName());
                exitcode = 1;
                goto exit;
            }
            if (busindexArg->count > 0)
            {
                busindex = busindexArg->ival[0];
            }
            if (I2C::Discover((i2c_port_t)busindex, fp) != ESP_OK)
            {
                cb->printf("Error while i2cdetect!\r\n");
            }
        exit:
            // deallocate each non-null entry in argtable[]
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            fclose(fp);
            return exitcode;
        }

        const char *GetName() override
        {
            return "i2cdetect";
        }
    };

    class RestartCommand : public CLI::AbstractCommand
    {
        int Execute(IShellCallback *cb, int argc, char *argv[]) override
        {
            cb->printf(COLOR_RESET COLOR_RED "Restarting...\r\n" COLOR_RESET);
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
            return 0;
        }

        const char *GetName() override
        {
            return "restart";
        }
    };

    class CLIExecutionOrder
    {
    public:
        ShellId shellId;
        IShellCallback *cb;
        char *buffer;
        CLIExecutionOrder(ShellId shellId, IShellCallback *cb, std::string commandLine) : shellId(shellId), cb(cb)
        {
            buffer = new char[commandLine.length() + 1];
            strcpy(buffer, commandLine.c_str());
        }

        ~CLIExecutionOrder()
        {
            delete buffer;
        }
    };

    class MegaCli : public IShellHandler
    {
    public:
        MegaCli(bool addDefaultInternalCommands, std::vector<CLI::AbstractCommand *> additionalCommands)
        {
            history = HistoryQueue((size_t)40);
            history.AddToQueueConsiderSizeLimit("");

            msg_queue = xQueueCreate(1, sizeof(CLIExecutionOrder *));

            this->commands = additionalCommands;
            if (addDefaultInternalCommands)
            {
                this->commands.push_back(new HelpCommand());
                this->commands.push_back(new SystemInfoCommand());
                this->commands.push_back(new I2CDetectCommand());
                this->commands.push_back(new RestartCommand());
            }
        }

        ShellId beginShell(IShellCallback *cb)
        {
            ShellId shellId = 0;
            cb->printf("\r\n"
                       " __  __                   _____ _      _____ \r\n"
                       "|  \\/  |                 / ____| |    |_   _|\r\n"
                       "| \\  / | ___  __ _  __ _| |    | |      | |  \r\n"
                       "| |\\/| |/ _ \\/ _` |/ _` | |    | |      | |  \r\n"
                       "| |  | |  __/ (_| | (_| | |____| |____ _| |_ \r\n"
                       "|_|  |_|\\___|\\__, |\\__,_|\\_____|______|_____|\r\n"
                       "              __/ |                          \r\n"
                       "             |___/                           \r\n"
                       "\r\n");
            cb->printf("Welcome to MegaCLI v0.0.1. Type 'help' to get help.\r\n");
            const esp_app_desc_t *app = esp_app_get_description();
            cb->printf("Firmware \"%s\" version %s compiled on %s %s with IDF %s.\r\n", app->project_name, app->version, app->date, app->time, app->idf_ver);
            cb->printf("Free Heap %lu bytes.\r\n", esp_get_free_heap_size());

            time_t now;
            struct tm timeinfo;
            char strftime_buf[64];
            time(&now);

            localtime_r(&now, &timeinfo);
            strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            cb->printf("System Time %s \r\n", strftime_buf);

            esp_chip_info_t chip_info;
            esp_chip_info(&chip_info);
            cb->printf("Chip Information: Chip Model %i Revision %i\r\n", chip_info.model, chip_info.revision);
            refreshLine(cb);
            return shellId;
        }

        void endShell(ShellId shellId, IShellCallback *cb)
        {
            return;
        }

        int insertChar(char c, ShellId shellId, IShellCallback *cb)
        {
            int old_history_index = this->history_index;
            copyCurrentHistoryToFrontForEdit(shellId, cb);
            ESP_LOGD(TAG, "insertChar old_history_index=%i historyIndex=%i, char=%c, pos=%u, strlen=%u", old_history_index, history_index, c, pos, history.front().size());
            if (history.front().size() == pos)
            {
                history.front().push_back(c);
                pos++;
                /* Avoid a full update of the line in the
                 * trivial case. */
                cb->printChar(c);
            }
            else
            {
                history.front().insert(pos, 1, c);
                pos++;
                refreshLine(cb);
            }
            return 0;
        }

        /* Move cursor on the left. */
        void moveLeft(ShellId shellId, IShellCallback *cb)
        {
            // not necessary, as there is no editing! copyCurrentHistoryToFrontForEdit();
            if (pos > 0)
            {
                pos--;
                refreshLine(cb);
            }
        }

        void moveInHistory(bool directionNext, ShellId shellId, IShellCallback *cb)
        {
            
            if (directionNext)
            {
                if (history_index - 1 < 0)
                {
                    beep(cb);
                }
                else
                {
                    history_index--;
                    pos = history[history_index].size();
                    ESP_LOGI(TAG, "moveInHistory next history_index=%d, history_size=%u, cursorPos=%u", history_index, history.size(), pos);
                    refreshLine(cb);
                }
            }
            else
            {
                if ((history_index + 1) >= history.size())
                {
                    beep(cb);
                }
                else
                {
                    history_index++;
                    pos = history[history_index].size();
                    ESP_LOGI(TAG, "moveInHistory prev history_index=%d, history_size=%u, cursorPos=%u", history_index, history.size(), pos);
                    refreshLine(cb);
                }
            }
        }

        /* Move cursor on the right. */
        void moveRight(ShellId shellId, IShellCallback *cb)
        {
            if (pos < history[history_index].length())
            {
                pos++;
                refreshLine(cb);
            }
        }

        /* Move cursor to the start of the line. */
        void moveHome(ShellId shellId, IShellCallback *cb)
        {
            if (pos != 0)
            {
                pos = 0;
                refreshLine(cb);
            }
        }

        /* Move cursor to the end of the line. */
        void moveEnd(ShellId shellId, IShellCallback *cb)
        {
            size_t len = history[history_index].length();
            if (pos != len)
            {
                pos = len;
                refreshLine(cb);
            }
        }

        /* Delete the character at the right of the cursor without altering the cursor
         * position. Basically this is what happens with the "Delete" keyboard key. */
        void doDelete(ShellId shellId, IShellCallback *cb)
        {
            copyCurrentHistoryToFrontForEdit(shellId, cb);
            if (pos < history.front().length())
            {
                history.front().replace(pos, 1, "");
                refreshLine(cb);
            }
        }

        /* Backspace implementation. */
        void doBackspace(ShellId shellId, IShellCallback *cb)
        {
            copyCurrentHistoryToFrontForEdit(shellId, cb);
            size_t len = history.front().length();
            ESP_LOGD(TAG, "doBackspace pos=%u len=%u", pos, len);
            if (pos > 0 && len > 0)
            {
                history.front().replace(pos - 1, 1, "");
                pos--;
                refreshLine(cb);
            }
        }

        void copyCurrentHistoryToFrontForEdit(ShellId shellId, IShellCallback *cb)
        {
            if (history_index == 0)
                return;
            ESP_LOGD(TAG, "copyCurrentHistoryToFrontForEdit");
            history.front().clear();
            history.front().append(history[history_index]);
            history_index = 0;
            pos = history.front().size();
            refreshLine(cb);
        }

        void InitAndRunCli(size_t stackSizeForCommandExecution_bytes=4*8192, int priorityForCommandExecution=8)
        {
            xTaskCreate(static_task, "CLI Task", stackSizeForCommandExecution_bytes, this, priorityForCommandExecution, nullptr);
        }

        void task()
        {
            CLIExecutionOrder *order{nullptr};
            while (true)
            {
                // See if there's a message in the queue (do not block)
                auto ret = xQueueReceive(msg_queue, (void *)&order, portMAX_DELAY);
                assert(ret == pdTRUE);
                assert(order);
                assert(order->cb);
                order->cb->printf("\r\n");

                ESP_LOGI(TAG, "executeLine with buf=%s", order->buffer);

                int argc = 0;
                char *p = strtok(order->buffer, " ");
                char *argv[16];
                while (p != NULL)
                {
                    argv[argc++] = p;
                    p = strtok(NULL, " ");
                }
                if (!argv[0])
                {
                    order->cb->printf(COLOR_BRIGHT_RED "No Command found\r\n" COLOR_RESET);
                    refreshLine(order->cb);
                    delete order;
                    continue;
                }
                AbstractCommand *theCmd{nullptr};
                for (auto &cmd : this->commands)
                {
                    if (0 == strcmp(cmd->GetName(), argv[0]))
                        theCmd = cmd;
                }
                if (theCmd == nullptr)
                {
                    order->cb->printf(COLOR_BRIGHT_RED "Unknown Command: %s\r\n" COLOR_RESET, order->buffer);
                    refreshLine(order->cb);
                    delete order;
                    continue;
                }
                int retval = theCmd->Execute(order->cb, argc, argv);
                if (retval != 0)
                {
                    order->cb->printf(COLOR_RESET COLOR_YELLOW "\r\nCommand returned %d\r\n" COLOR_RESET, retval);
                }
                refreshLine(order->cb);
                delete order;
            }
        }

        static void static_task(void *arg)
        {
            MegaCli *myself = static_cast<MegaCli *>(arg);
            myself->task();
        }

        int executeLine(ShellId shellId, IShellCallback *cb)
        {
            this->copyCurrentHistoryToFrontForEdit(shellId, cb);
            CLIExecutionOrder *order = new CLIExecutionOrder(shellId, cb, history.front());
            ESP_LOGD(TAG, "Put buffer into queue: %s", order->buffer);
            history.AddToQueueConsiderSizeLimit("");
            history.front().reserve(MAX_LINE_LENGTH);
            pos = 0;
            xQueueSend(msg_queue, (void *)&order, 10);
            return 0;
        }

        int handleSingleChar(const char ch, ShellId shellId, IShellCallback *cb)
        {
            KEY_ACTION c = (KEY_ACTION)ch;

            switch (c)
            {
            case KEY_ACTION::ENTER: /* enter */
                return this->executeLine(shellId, cb);
            case KEY_ACTION::CTRL_C: /* ctrl-c */
                errno = EAGAIN;
                return -1;
            case KEY_ACTION::BACKSPACE: /* backspace */
            case KEY_ACTION::CTRL_H:    /* ctrl-h */
                doBackspace(shellId, cb);
                break;
            case KEY_ACTION::CTRL_D: /* ctrl-d, remove char at right of cursor, or if the
                            line is empty, act as end-of-file. */
                if (history[history_index].length() > 0)
                {
                    doDelete(shellId, cb);
                }
                else
                {
                    // TODO KL what happens here?
                    // history_len--;
                    // free(history[history_len]);
                    return -1;
                }
                break;
            case KEY_ACTION::CTRL_T: /* ctrl-t, swaps current character with previous. */
                break;
            case KEY_ACTION::CTRL_B: /* ctrl-b */
                moveLeft(shellId, cb);
                break;
            case KEY_ACTION::CTRL_F: /* ctrl-f */
                moveRight(shellId, cb);
                break;
            case KEY_ACTION::CTRL_P: /* ctrl-p */
                moveInHistory(false, shellId, cb);
                break;
            case KEY_ACTION::CTRL_N: /* ctrl-n */
                moveInHistory(true, shellId, cb);
                break;
            default:
                if (insertChar(ch, shellId, cb))
                    return -1;
                break;
            case KEY_ACTION::CTRL_U: /* Ctrl+u, delete the whole line. */
                history.front().clear();
                refreshLine(cb);
                break;
            case KEY_ACTION::CTRL_K: /* Ctrl+k, delete from current to end of line. */
                history.front().resize(pos);
                refreshLine(cb);
                break;
            case KEY_ACTION::CTRL_A: /* Ctrl+a, go to the start of the line */
                moveHome(shellId, cb);
                break;
            case KEY_ACTION::CTRL_E: /* ctrl+e, go to the end of the line */
                moveEnd(shellId, cb);
                break;
            case KEY_ACTION::CTRL_L: /* ctrl+l, clear screen */
                clearScreen(cb);
                refreshLine(cb);
                break;
            case KEY_ACTION::CTRL_W: /* ctrl+w, delete previous word */
                // TODO linenoiseEditDeletePrevWord(&l);
                break;
            }
            return 0;
        }

        // Zentrale Einsprungfunktion - muss schnell zurück kehren
        int handleChars(const char *chars, size_t len, ShellId shellId, IShellCallback *cb)
        {

            if (len > 1 && chars[0] == '\033')
            { // we received an escape sequence
              // we assume: the whole escape sequence comes in one call
                /* ESC [ sequences. */
                if (chars[1] == '[')
                {
                    if (chars[2] >= '0' && chars[2] <= '9')
                    {

                        if (chars[3] == '~')
                        {
                            switch (chars[2])
                            {
                            case '3': /* Delete key. */
                                doDelete(shellId, cb);
                                break;
                            }
                        }
                    }
                    else
                    {
                        switch (chars[2])
                        {
                        case 'A': /* Up */
                            moveInHistory(false, shellId, cb);
                            break;
                        case 'B': /* Down */
                            moveInHistory(true, shellId, cb);
                            break;
                        case 'C': /* Right */
                            moveRight(shellId, cb);
                            break;
                        case 'D': /* Left */
                            moveLeft(shellId, cb);
                            break;
                        case 'H': /* Home */
                            moveHome(shellId, cb);
                            break;
                        case 'F': /* End*/
                            moveEnd(shellId, cb);
                            break;
                        }
                    }
                }
                /* ESC O sequences. */
                else if (chars[1] == 'O')
                {
                    switch (chars[2])
                    {
                    case 'H': /* Home */
                        moveHome(shellId, cb);
                        break;
                    case 'F': /* End*/
                        moveEnd(shellId, cb);
                        break;
                    }
                }
                return 0;
            }
            // wenn es keine EscapeSequence ist, müssen die Zeichen einzeln kommen! -->nein, bei Copy&Paste kommen sie nicht einzeln!
            int retval{0};
            for (size_t i = 0; i < len; i++)
            {

                retval = handleSingleChar(chars[i], shellId, cb);
                if (retval != 0)
                    return retval;
            }
            return retval;
        }

    private:
        std::vector<AbstractCommand *> commands;
        HistoryQueue history;
        QueueHandle_t msg_queue;

        size_t pos{0};        // Current cursor position. "0" ist das Zeichen nach dem Prompt (also nicht bezogen auf den Anfang der Zeile sondern auf nach dem Prompt)
        int history_index{0}; // The history index we are currently showing.

        void clearScreen(IShellCallback *cb)
        {
            cb->printf("\x1b[H\x1b[2J");
        }

        void beep(IShellCallback *cb)
        {
            cb->printf("\x7");
        }

        void refreshLine(IShellCallback *cb)
        {
            std::string buf = std::string("");
            buf.reserve(MAX_LINE_LENGTH + PROMPT_LEN + 10);
            /* Cursor to left edge */
            buf.append("\r");
            /* Write the prompt and the current buffer content */
            assert(cb);
            buf.append(cb->GetUsername(), 4);
            buf.append("> ");

            buf.append(history[history_index]);
            /* Erase to right */
            buf.append("\x1b[0K");
            /* Move cursor to original position. */
            buf.append("\r\x1b[");
            buf.append(std::to_string(pos + PROMPT_LEN));
            buf.append("C");
            cb->printf(buf.c_str());
            ESP_LOGI(TAG, "Refresh sent=%s", history[history_index].c_str());
        }
    };
}
#undef TAG