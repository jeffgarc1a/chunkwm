#include "config.h"
#include "vspace.h"
#include "node.h"
#include "controller.h"
#include "constants.h"
#include "misc.h"

#include "../../common/config/cvar.h"
#include "../../common/misc/assert.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#define local_persist static

struct token
{
    const char *Text;
    unsigned Length;
};

inline bool
TokenEquals(token Token, const char *Match)
{
    const char *At = Match;
    for(int Index = 0;
        Index < Token.Length;
        ++Index, ++At)
    {
        if((*At == 0) || (Token.Text[Index] != *At))
        {
            return false;
        }
    }

    bool Result = (*At == 0);
    return Result;
}

token GetToken(const char **Data)
{
    token Token;

    Token.Text = *Data;
    while(**Data && **Data != ' ')
    {
        ++(*Data);
    }

    Token.Length = *Data - Token.Text;
    ASSERT(**Data == ' ' || **Data == '\0');

    if(**Data == ' ')
    {
        ++(*Data);
    }
    else
    {
        // NOTE(koekeishiya): Do not go past the null-terminator!
    }

    return Token;
}

inline char *
TokenToString(token Token)
{
    char *Result = (char *) malloc(Token.Length + 1);
    Result[Token.Length] = '\0';
    memcpy(Result, Token.Text, Token.Length);
    return Result;
}

inline float
TokenToFloat(token Token)
{
    float Result = 0;
    char *String = TokenToString(Token);
    sscanf(String, "%f", &Result);
    free(String);
    return Result;
}

inline int
TokenToInt(token Token)
{
    int Result = 0;
    char *String = TokenToString(Token);
    sscanf(String, "%d", &Result);
    free(String);
    return Result;
}

inline char **
BuildArguments(const char *Message, int *Count)
{
    char **Args = (char **) malloc(16 * sizeof(char *));
    *Count = 1;

    while(*Message)
    {
        token ArgToken = GetToken(&Message);
        char *Arg = TokenToString(ArgToken);
        Args[(*Count)++] = Arg;
    }

#if 0
    for(int Index = 1;
        Index < *Count;
        ++Index)
    {
        printf("%d arg '%s'\n", Index, Args[Index]);
    }
#endif

    return Args;
}

inline void
FreeArguments(int Count, char **Args)
{
    for(int Index = 1;
        Index < Count;
        ++Index)
    {
        free(Args[Index]);
    }

    free(Args);
}

struct command
{
    char Flag;
    char *Arg;
    struct command *Next;
};

inline void
FreeCommandChain(command *Chain)
{
    command *Command = Chain->Next;
    while(Command)
    {
        command *Next = Command->Next;
        free(Command->Arg);
        free(Command);
        Command = Next;
    }
}

inline command *
ConstructCommand(char Flag, char *Arg)
{
    command *Command = (command *) malloc(sizeof(command));

    Command->Flag = Flag;
    Command->Arg = strdup(Arg);
    Command->Next = NULL;

    return Command;
}

typedef void (*command_func)(char *);
command_func WindowCommandDispatch[] =
{
    FocusWindow,
    SwapWindow,
    UseInsertionPoint,
    ToggleWindow,
    MoveWindow,
    TemporaryRatio
};

#define WINDOW_FLAG_F 0
#define WINDOW_FLAG_S 1
#define WINDOW_FLAG_I 2
#define WINDOW_FLAG_T 3
#define WINDOW_FLAG_W 4
#define WINDOW_FLAG_R 5

unsigned WindowFuncFromFlag(char Flag)
{
    switch(Flag)
    {
        case 'f': return WINDOW_FLAG_F; break;
        case 's': return WINDOW_FLAG_S; break;
        case 'i': return WINDOW_FLAG_I; break;
        case 't': return WINDOW_FLAG_T; break;
        case 'w': return WINDOW_FLAG_W; break;
        case 'r': return WINDOW_FLAG_R; break;

        // NOTE(koekeishiya): silence compiler warning.
        default: return 0; break;
    }
}

inline bool
ParseWindowCommand(const char *Message, command *Chain)
{
    int Count;
    char **Args = BuildArguments(Message, &Count);

    int Option;
    bool Success = true;
    const char *Short = "f:s:i:t:w:r:";

    command *Command = Chain;
    while((Option = getopt_long(Count, Args, Short, NULL, NULL)) != -1)
    {
        switch(Option)
        {
            // NOTE(koekeishiya): The '-f', '-s', '-i' and '-w' flag support the same arguments.
            case 'f':
            case 's':
            case 'i':
            case 'w':
            {
                if((StringEquals(optarg, "west")) ||
                   (StringEquals(optarg, "east")) ||
                   (StringEquals(optarg, "north")) ||
                   (StringEquals(optarg, "south")))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case 'r':
            {
                float Float;
                if(sscanf(optarg, "%f", &Float))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case 't':
            {
                if((StringEquals(optarg, "float")))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for window flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case '?':
            {
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            } break;
        }
    }

End:
    // NOTE(koekeishiya): Reset getopt.
    optind = 1;

    FreeArguments(Count, Args);
    return Success;
}

command_func SpaceCommandDispatch[] =
{
    RotateWindowTree
};

#define SPACE_FLAG_R 0

unsigned SpaceFuncFromFlag(char Flag)
{
    switch(Flag)
    {
        case 'r': return SPACE_FLAG_R; break;

        // NOTE(koekeishiya): silence compiler warning.
        default: return 0; break;
    }
}

inline bool
ParseSpaceCommand(const char *Message, command *Chain)
{
    int Count;
    char **Args = BuildArguments(Message, &Count);

    int Option;
    bool Success = true;
    const char *Short = "r:";

    command *Command = Chain;
    while((Option = getopt_long(Count, Args, Short, NULL, NULL)) != -1)
    {
        switch(Option)
        {
            case 'r':
            {
                if((StringEquals(optarg, "90")) ||
                   (StringEquals(optarg, "180")) ||
                   (StringEquals(optarg, "270")))
                {
                    command *Entry = ConstructCommand(Option, optarg);
                    Command->Next = Entry;
                    Command = Entry;
                }
                else
                {
                    fprintf(stderr, "    invalid selector '%s' for space flag '%c'\n", optarg, Option);
                    Success = false;
                    FreeCommandChain(Chain);
                    goto End;
                }
            } break;
            case '?':
            {
                Success = false;
                FreeCommandChain(Chain);
                goto End;
            } break;
        }
    }

End:
    // NOTE(koekeishiya): Reset getopt.
    optind = 1;

    FreeArguments(Count, Args);
    return Success;
}

/* NOTE(koekeishiya): Parameters
 *
 * const char *Message
 * int SockFD
 *
 * */
DAEMON_CALLBACK(DaemonCallback)
{
    token Type = GetToken(&Message);

    if(TokenEquals(Type, "config"))
    {
        token Command = GetToken(&Message);
        if(TokenEquals(Command, CVAR_SPACE_MODE))
        {
            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            if(Value.Length > 0)
            {
                printf("        value: '%.*s'\n", Value.Length, Value.Text);
                if(TokenEquals(Value, "bsp"))
                {
                    UpdateCVar(Variable, Virtual_Space_Bsp);
                }
                else if(TokenEquals(Value, "monocle"))
                {
                    UpdateCVar(Variable, Virtual_Space_Monocle);
                }
                else if(TokenEquals(Value, "float"))
                {
                    UpdateCVar(Variable, Virtual_Space_Float);
                }
            }
            else
            {
                fprintf(stderr, "        value: MISSING!!!\n");
            }

            free(Variable);
        }
        else if((TokenEquals(Command, CVAR_SPACE_OFFSET_TOP)) ||
                (TokenEquals(Command, CVAR_SPACE_OFFSET_BOTTOM)) ||
                (TokenEquals(Command, CVAR_SPACE_OFFSET_LEFT)) ||
                (TokenEquals(Command, CVAR_SPACE_OFFSET_RIGHT)) ||
                (TokenEquals(Command, CVAR_SPACE_OFFSET_GAP)))
        {
            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            if(Value.Length > 0)
            {
                float FloatValue = TokenToFloat(Value);
                printf("        value: '%f'\n", FloatValue);
                UpdateCVar(Variable, FloatValue);
            }
            else
            {
                fprintf(stderr, "        value: MISSING!!!\n");
            }

            free(Variable);
        }
        else if(TokenEquals(Command, CVAR_BSP_SPAWN_LEFT))
        {
            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            if(Value.Length > 0)
            {
                int IntValue = TokenToInt(Value);
                printf("        value: '%d'\n", IntValue);
                UpdateCVar(Variable, IntValue);
            }
            else
            {
                fprintf(stderr, "        value: MISSING!!!\n");
            }

            free(Variable);
        }
        else if((TokenEquals(Command, CVAR_BSP_OPTIMAL_RATIO)) ||
                (TokenEquals(Command, CVAR_BSP_SPLIT_RATIO)))
        {
            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            if(Value.Length > 0)
            {
                float FloatValue = TokenToFloat(Value);
                printf("        value: '%f'\n", FloatValue);
                UpdateCVar(Variable, FloatValue);
            }
            else
            {
                fprintf(stderr, "        value: MISSING!!!\n");
            }

            free(Variable);
        }
        else if(TokenEquals(Command, CVAR_BSP_SPLIT_MODE))
        {
            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            if(Value.Length > 0)
            {
                printf("        value: '%.*s'\n", Value.Length, Value.Text);
                if(TokenEquals(Value, "optimal"))
                {
                    UpdateCVar(Variable, Split_Optimal);
                }
                else if(TokenEquals(Value, "vertical"))
                {
                    UpdateCVar(Variable, Split_Vertical);
                }
                else if(TokenEquals(Value, "horizontal"))
                {
                    UpdateCVar(Variable, Split_Horizontal);
                }
            }
            else
            {
                fprintf(stderr, "        value: MISSING!!!\n");
            }

            free(Variable);
        }
        else if((TokenEquals(Command, CVAR_WINDOW_FLOAT_TOPMOST)) ||
                (TokenEquals(Command, CVAR_WINDOW_FLOAT_NEXT)) ||
                (TokenEquals(Command, CVAR_MOUSE_FOLLOWS_FOCUS)))
        {
            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            token Value = GetToken(&Message);
            if(Value.Length > 0)
            {
                int IntValue = TokenToInt(Value);
                printf("        value: '%d'\n", IntValue);
                UpdateCVar(Variable, IntValue);
            }
            else
            {
                fprintf(stderr, "        value: MISSING!!!\n");
            }

            free(Variable);
        }
        else
        {
            // NOTE(koekeishiya): The command we got is not a pre-defined string, but
            // we do allow custom options used for space-specific settings, etc...

            char *Variable = TokenToString(Command);
            printf("        command: '%s'\n", Variable);

            char Buffer[BUFFER_SIZE];
            int First;

            if(sscanf(Variable, "%d_%s", &First, Buffer) == 2)
            {
                if((StringEquals(Buffer, _CVAR_SPACE_OFFSET_TOP)) ||
                   (StringEquals(Buffer, _CVAR_SPACE_OFFSET_BOTTOM)) ||
                   (StringEquals(Buffer, _CVAR_SPACE_OFFSET_LEFT)) ||
                   (StringEquals(Buffer, _CVAR_SPACE_OFFSET_RIGHT)) ||
                   (StringEquals(Buffer, _CVAR_SPACE_OFFSET_GAP)))
                {
                    token Value = GetToken(&Message);
                    if(Value.Length > 0)
                    {
                        float FloatValue = TokenToFloat(Value);
                        printf("        value: '%f'\n", FloatValue);
                        UpdateCVar(Variable, FloatValue);
                    }
                    else
                    {
                        fprintf(stderr, "        value: MISSING!!!\n");
                    }
                }
                else if(StringEquals(Buffer, _CVAR_SPACE_MODE))
                {
                    token Value = GetToken(&Message);
                    if(Value.Length > 0)
                    {
                        printf("        value: '%.*s'\n", Value.Length, Value.Text);
                        if(TokenEquals(Value, "bsp"))
                        {
                            UpdateCVar(Variable, Virtual_Space_Bsp);
                        }
                        else if(TokenEquals(Value, "monocle"))
                        {
                            UpdateCVar(Variable, Virtual_Space_Monocle);
                        }
                        else if(TokenEquals(Value, "float"))
                        {
                            UpdateCVar(Variable, Virtual_Space_Float);
                        }
                    }
                    else
                    {
                        fprintf(stderr, "        value: MISSING!!!\n");
                    }
                }
                else
                {
                    fprintf(stderr, " tiling daemon: '%s' is not a valid config option!\n", Variable);
                }
            }
            else
            {
                fprintf(stderr, " tiling daemon: '%.*s' is not a valid config option!\n", Command.Length, Command.Text);
            }
            free(Variable);
        }
    }
    else if(TokenEquals(Type, "window"))
    {
        command Chain = {};
        bool Success = ParseWindowCommand(Message, &Chain);
        if(Success)
        {
            float Ratio = CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO);
            command *Command = &Chain;
            while((Command = Command->Next))
            {
                printf("    command: '%c', arg: '%s'\n", Command->Flag, Command->Arg);

                /* NOTE(koekeishiya): flags description:
                 * f: focus
                 * s: swap
                 * w: detach and reinsert
                 * i: insertion point (previously 'mark' window)
                 * t: float, fullscreen, (parent ?)
                 * d: move to desktop
                 * m: move to monitor
                 * */

                unsigned Index = WindowFuncFromFlag(Command->Flag);
                WindowCommandDispatch[Index](Command->Arg);
            }

            if(Ratio != CVarFloatingPointValue(CVAR_BSP_SPLIT_RATIO))
            {
                UpdateCVar(CVAR_BSP_SPLIT_RATIO, Ratio);
            }

            FreeCommandChain(&Chain);
        }
    }
    else if(TokenEquals(Type, "space"))
    {
        command Chain = {};
        bool Success = ParseSpaceCommand(Message, &Chain);
        if(Success)
        {
            command *Command = &Chain;
            while((Command = Command->Next))
            {
                printf("    command: '%c', arg: '%s'\n", Command->Flag, Command->Arg);

                /* NOTE(koekeishiya): flags description:
                 * r: rotate 90, 180, 270 degrees
                 * */

                unsigned Index = SpaceFuncFromFlag(Command->Flag);
                SpaceCommandDispatch[Index](Command->Arg);
            }

            FreeCommandChain(&Chain);
        }
    }
    else
    {
        fprintf(stderr, " tiling daemon: no match for '%.*s'\n", Type.Length, Type.Text);
    }
}